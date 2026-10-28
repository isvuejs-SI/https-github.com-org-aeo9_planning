#include "drone_safety_ops/aggregate.hpp"
#include "drone_safety_ops/alert.hpp"
#include "drone_safety_ops/day3.hpp"
#include "drone_safety_ops/params.hpp"
#include "drone_safety_ops/px4_request.hpp"
#include "drone_safety_ops/verdict_log.hpp"
#include "drone_safety_ops/watchdog.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <vector>
#include <ctime>
#include <memory>
#include <sstream>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

#ifndef DSO_ROLE
#define DSO_ROLE "safety_supervisor"
#endif

namespace {

double mono() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string iso_now() {
  std::time_t now = std::time(nullptr);
  std::tm tm {};
  gmtime_r(&now, &tm);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

}  // namespace

class SafetyNode : public rclcpp::Node {
 public:
  explicit SafetyNode(const std::string& role) : Node(role + "_node"), role_(role), start_(mono()) {
    const std::string from_param = declare_parameter<std::string>("params_file", "");
    const char* from_env = std::getenv("DRONE_SAFETY_PARAMS");
    std::string path = !from_param.empty() ? from_param : (from_env ? from_env : "config/params.yaml");
    params_ = drone_safety::load_params(path);
    send_alerts_ = declare_parameter<bool>("send_alerts", true);
    const char* log_env = std::getenv("DRONE_SAFETY_LOG_DIR");
    std::string log_dir = log_env ? log_env
                                  : drone_safety::choose_log_dir(params_.logging.nvme_dir, params_.logging.fallback_dir);
    log_ = std::make_unique<drone_safety::VerdictLog>(log_dir);
    alerter_ = std::make_unique<drone_safety::Alerter>(params_.alert.duplicate_holdback_s, [] { return mono(); });
    alerter_->set_sender([this](const std::string& verdict, const std::string& msg, const std::string&) {
      const char* topic = std::getenv("NTFY_TOPIC");
      if (!topic || !*topic) {
        RCLCPP_WARN(get_logger(), "NTFY_TOPIC unset; alert logged only");
        return false;
      }
      return drone_safety::Alerter::send_ntfy_curl(params_.alert.ntfy_host, topic, verdict, msg);
    });

    auto qos = rclcpp::QoS(10);
    auto latch = rclcpp::QoS(rclcpp::KeepLast(1)).transient_local().reliable();
    using Msg = std_msgs::msg::String;
    pub_supervisor_ = create_publisher<Msg>("/drone_safety_ops/supervisor/verdict", qos);
    pub_deploy_ = create_publisher<Msg>("/drone_safety_ops/supervisor/deploy", qos);
    pub_gate_ = create_publisher<Msg>("/drone_safety_ops/mission_gate/verdict", qos);
    pub_day3_ = create_publisher<Msg>("/drone_safety_ops/day3/verdict", qos);
    pub_day3_replay_ = create_publisher<Msg>("/drone_safety_ops/day3/replay", latch);
    pub_guard_ = create_publisher<Msg>("/drone_safety_ops/guard/verdict", qos);
    pub_landing_ = create_publisher<Msg>("/drone_safety_ops/landing/verdict", qos);
    pub_route_ = create_publisher<Msg>("/drone_safety_ops/route/verdict", qos);
    pub_worst_ = create_publisher<Msg>("/drone_safety_ops/worst_verdict", latch);
    pub_px4_ = create_publisher<Msg>(params_.px4.request_topic, qos);
    pub_latch_ = create_publisher<Msg>(params_.px4.no_arm_latch_topic, latch);
    pub_heartbeat_ = create_publisher<Msg>("/drone_safety_ops/heartbeat", qos);

    auto bind = [this](const std::string& topic, const std::string& source) {
      return create_subscription<Msg>(topic, qos, [this, source](Msg::SharedPtr msg) { on_sensor(source, msg->data); });
    };
    subs_.push_back(bind("/drone_safety_ops/battery", "battery"));
    subs_.push_back(bind("/drone_safety_ops/gps_geofence", "gps"));
    subs_.push_back(bind("/drone_safety_ops/link_heartbeat", "link"));
    subs_.push_back(bind("/drone_safety_ops/lidar", "lidar"));
    subs_.push_back(bind("/drone_safety_ops/thermal", "thermal"));
    subs_.push_back(create_subscription<Msg>("/drone_safety_ops/guard/verdict", qos,
                                             [this](Msg::SharedPtr msg) { on_guard_verdict(msg->data); }));

    if (role_ == "day3" || role_ == "safety_supervisor") {
      std::string replay = drone_safety::render_day3(params_);
      if (!replay.empty() && replay.back() == '\n') replay.pop_back();
      publish(pub_day3_replay_, replay);
    }
    const auto period = std::chrono::duration<double>(std::max(0.2, params_.watchdog.period_s));
    timer_ = create_wall_timer(std::chrono::duration_cast<std::chrono::milliseconds>(period), [this] { on_timer(); });
    if (role_ != "alert") recompute();
    drone_safety::notify_ready();
    drone_safety::notify_watchdog();
    RCLCPP_INFO(get_logger(),
                "drone_safety_ops %s ready; PX4 owns failsafes; requests are hold, rtl, land, no_arm", role_.c_str());
  }

 private:
  void publish(const rclcpp::Publisher<std_msgs::msg::String>::SharedPtr& pub, const std::string& text) {
    std_msgs::msg::String msg;
    msg.data = text;
    pub->publish(msg);
  }

  void on_sensor(const std::string& source, const std::string& data) {
    drone_safety::Json payload;
    try {
      payload = drone_safety::parse_json(data);
    } catch (const std::exception& ex) {
      RCLCPP_ERROR(get_logger(), "bad %s json: %s", source.c_str(), ex.what());
      return;
    }
    const double now = mono();
    if (source == "battery") drone_safety::apply_battery(cache_, payload);
    else if (source == "gps") drone_safety::apply_gps(cache_, payload);
    else if (source == "link") {
      last_link_ = now;
      explicit_gap_ = payload.has("link_gap_s") ? payload.num("link_gap_s") : -1.0;
      drone_safety::apply_link(cache_, payload);
    } else if (source == "lidar") drone_safety::apply_lidar(cache_, payload);
    else if (source == "thermal") {
      last_thermal_ = now;
      drone_safety::apply_thermal(cache_, payload);
    }
    if (role_ != "alert") recompute();
  }

  void on_guard_verdict(const std::string& verdict) {
    if (role_ != "alert") return;
    std::ostringstream text;
    text << verdict << ": lidar " << cache_.lidar_kind << " " << cache_.lidar_dist << " m";
    std::string line;
    if (alerter_->alert(verdict, text.str(), false, &line)) RCLCPP_INFO(get_logger(), "%s", line.c_str());
  }

  void refresh_ages() {
    const double now = mono();
    const double measured = now - (last_link_ < 0 ? start_ : last_link_);
    cache_.link_gap_s = explicit_gap_ >= 0 ? std::max(measured, explicit_gap_) : measured;
    cache_.detector_ok =
        last_thermal_ >= 0 && (now - last_thermal_) <= params_.supervisor.detector_timeout_s;
  }

  void on_timer() {
    drone_safety::notify_watchdog();
    publish(pub_heartbeat_, "ok");
    if (role_ != "alert") recompute();
  }

  void recompute() {
    refresh_ages();
    auto snap = drone_safety::evaluate(cache_, params_);
    if (role_ == "supervisor" || role_ == "safety_supervisor") {
      publish(pub_supervisor_, snap.supervisor);
      publish(pub_deploy_, snap.deploy);
    }
    if (role_ == "mission_gate" || role_ == "safety_supervisor") publish(pub_gate_, snap.gate + ":" + snap.gate_why);
    if (role_ == "guard" || role_ == "safety_supervisor") publish(pub_guard_, snap.guard);
    if (role_ == "landing" || role_ == "safety_supervisor") publish(pub_landing_, snap.landing);
    if (role_ == "route" || role_ == "safety_supervisor") publish(pub_route_, snap.route);
    if (role_ == "day3" || role_ == "safety_supervisor") publish(pub_day3_, snap.gate + ":" + snap.gate_why);
    if (role_ != "safety_supervisor") {
      const std::string shown = role_ == "mission_gate" || role_ == "day3" ? snap.gate
                                : role_ == "guard"                         ? snap.guard
                                : role_ == "landing"                       ? snap.landing
                                : role_ == "route"                         ? snap.route
                                                                           : snap.supervisor;
      RCLCPP_INFO(get_logger(), "verdict %s %s", role_.c_str(), shown.c_str());
      return;
    }
    publish(pub_worst_, snap.worst);
    publish(pub_latch_, snap.px4.latch_no_arm ? "true" : "false");
    const std::string signature = snap.px4.wire + (snap.px4.latch_no_arm ? ":1" : ":0");
    if (signature != last_px4_ && (snap.px4.wire != "none" || snap.px4.latch_no_arm)) {
      std::ostringstream body;
      body << "{\"wire\":\"" << snap.px4.wire << "\",\"command\":" << snap.px4.command << ",\"latch_no_arm\":"
           << (snap.px4.latch_no_arm ? "true" : "false") << ",\"uxrce_topic\":\"" << params_.px4.uxrce_command_topic
           << "\",\"note\":\"request only; PX4 owns the failsafe\",\"mavsdk\":\""
           << drone_safety::mavsdk_action(snap.px4.wire) << "\"";
      if (snap.px4.has_param1) body << ",\"param1\":" << snap.px4.param1;
      if (snap.px4.has_param2) body << ",\"param2\":" << snap.px4.param2;
      if (snap.px4.has_param3) body << ",\"param3\":" << snap.px4.param3;
      body << "}";
      publish(pub_px4_, body.str());
      RCLCPP_INFO(get_logger(), "px4 request %s uxrce mavsdk=%s", snap.px4.wire.c_str(),
                  drone_safety::mavsdk_action(snap.px4.wire));
    }
    last_px4_ = signature;
    const std::string ts = iso_now();
    log_->append(ts, "supervisor", snap.supervisor, snap.px4.wire, "");
    log_->append(ts, "mission_gate", snap.gate, snap.px4.wire, snap.gate_why);
    log_->append(ts, "guard", snap.guard, snap.px4.wire, "");
    log_->append(ts, "landing", snap.landing, snap.px4.wire, "");
    log_->append(ts, "route", snap.route, snap.px4.wire, "");
    log_->append(ts, "worst", snap.worst, snap.px4.wire, snap.worst_module);
    if (send_alerts_) {
      std::string line;
      if (alerter_->alert(snap.guard, snap.guard + ": guard verdict", false, &line))
        RCLCPP_INFO(get_logger(), "%s", line.c_str());
    }
    RCLCPP_INFO(get_logger(), "worst %s px4 %s", snap.worst.c_str(), snap.px4.wire.c_str());
  }

  std::string role_;
  double start_{0};
  drone_safety::Params params_;
  drone_safety::Cache cache_;
  bool send_alerts_{true};
  double last_link_{-1};
  double last_thermal_{-1};
  double explicit_gap_{-1};
  std::string last_px4_;
  std::unique_ptr<drone_safety::VerdictLog> log_;
  std::unique_ptr<drone_safety::Alerter> alerter_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_supervisor_, pub_deploy_, pub_gate_, pub_day3_,
      pub_day3_replay_, pub_guard_, pub_landing_, pub_route_, pub_worst_, pub_px4_, pub_latch_, pub_heartbeat_;
  std::vector<rclcpp::Subscription<std_msgs::msg::String>::SharedPtr> subs_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SafetyNode>(DSO_ROLE));
  rclcpp::shutdown();
  return 0;
}
