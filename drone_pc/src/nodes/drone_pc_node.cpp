// ROS 2 supervisor node (C++). Compiles when rclcpp is available.
// Core planning/control/safety logic lives in drone_pc_core (tested standalone).

#include <chrono>
#include <memory>
#include <string>

#include "drone_pc/control/link_loss.hpp"
#include "drone_pc/control/px4_interface.hpp"
#include "drone_pc/params.hpp"
#include "drone_pc/planning/energy_planner.hpp"
#include "drone_pc/planning/escape_planner.hpp"
#include "drone_pc/planning/state_machine.hpp"
#include "drone_pc/safety/alerts.hpp"
#include "drone_pc/safety/event_logger.hpp"
#include "drone_pc/safety/preflight_gate.hpp"

#if __has_include(<rclcpp/rclcpp.hpp>)
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

using namespace std::chrono_literals;

namespace drone_pc {

class DronePcNode : public rclcpp::Node {
 public:
  DronePcNode()
      : Node("drone_pc"),
        params_(Params::defaults()),
        sm_(MissionPhase::SLEEP_CHARGING),
        gate_(params_),
        energy_(params_),
        escape_(params_),
        px4_(params_),
        link_(params_),
        alerts_(params_),
        logger_(params_) {
    declare_parameters();
    status_pub_ = create_publisher<std_msgs::msg::String>("drone_pc/status", 10);
    timer_ = create_wall_timer(std::chrono::duration<double>(px4_.offboard_period_s()),
                               std::bind(&DronePcNode::on_timer, this));
    RCLCPP_INFO(get_logger(),
                "drone_pc online — request-only to PX4; PX4 owns failsafes");
  }

 private:
  void declare_parameters() {
    // Mirror YAML keys so launch overrides apply (subset shown; full set in YAML)
    this->declare_parameter("preflight.max_wind_mps", params_.preflight_max_wind_mps);
    this->declare_parameter("link_loss.rtl_after_s", params_.link_loss_rtl_after_s);
    this->declare_parameter("link_loss.abort_after_s", params_.link_loss_abort_after_s);
    this->declare_parameter("control.offboard_rate_hz", params_.control_offboard_rate_hz);
    params_.preflight_max_wind_mps = this->get_parameter("preflight.max_wind_mps").as_double();
    params_.link_loss_rtl_after_s = this->get_parameter("link_loss.rtl_after_s").as_double();
    params_.link_loss_abort_after_s = this->get_parameter("link_loss.abort_after_s").as_double();
    params_.control_offboard_rate_hz =
        this->get_parameter("control.offboard_rate_hz").as_double();
  }

  void on_timer() {
    px4_.send_heartbeat();
    px4_.tick(px4_.offboard_period_s());
    auto link_res = link_.update(px4_.offboard_period_s(), true);
    if (link_res.action == LinkLossAction::RTL) {
      px4_.request(Px4Request::RTL);
      alerts_.maybe_send("rtl", "link_loss_rtl", now_s());
      logger_.log_verdict(now_s(), "RTL:link_loss");
    } else if (link_res.action == LinkLossAction::ABORT_MISSION) {
      px4_.request(Px4Request::LAND);
      alerts_.maybe_send("abort", "link_loss_abort", now_s());
      logger_.log_verdict(now_s(), "ABORT_MISSION:link_loss");
    }

    std_msgs::msg::String msg;
    msg.data = to_string(sm_.current());
    status_pub_->publish(msg);
  }

  double now_s() const { return this->now().seconds(); }

  Params params_;
  StateMachine sm_;
  PreflightGate gate_;
  EnergyPlanner energy_;
  EscapePlanner escape_;
  Px4Interface px4_;
  LinkLossMonitor link_;
  AlertManager alerts_;
  EventLogger logger_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace drone_pc

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<drone_pc::DronePcNode>());
  rclcpp::shutdown();
  return 0;
}

#else
int main() {
  // Built without ROS — core library + shared tests are the SITL verification path.
  return 0;
}
#endif
