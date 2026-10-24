#include "drone_pc/planning/global_planner.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>

namespace drone_pc {
namespace {

struct NodeKey {
  int x, y, z;
  bool operator==(const NodeKey& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct NodeKeyHash {
  std::size_t operator()(const NodeKey& k) const {
    return (static_cast<std::size_t>(k.x) * 73856093u) ^
           (static_cast<std::size_t>(k.y) * 19349663u) ^
           (static_cast<std::size_t>(k.z) * 83492791u);
  }
};

struct OpenItem {
  double f;
  NodeKey key;
  bool operator>(const OpenItem& o) const {
    if (f != o.f) return f > o.f;
    if (key.x != o.key.x) return key.x > o.key.x;
    if (key.y != o.key.y) return key.y > o.key.y;
    return key.z > o.key.z;
  }
};

double heuristic(const NodeKey& a, const NodeKey& b, double weight) {
  const double dx = static_cast<double>(a.x - b.x);
  const double dy = static_cast<double>(a.y - b.y);
  const double dz = static_cast<double>(a.z - b.z);
  return weight * std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

GlobalPlanner::GlobalPlanner(const Params& params) : params_(params) {}

GlobalPlanResult GlobalPlanner::plan(const OccupancyMap& map, const Point3& start,
                                     const Point3& goal) const {
  GlobalPlanResult result;
  int sx, sy, sz, gx, gy, gz;
  if (!map.world_to_index(start, sx, sy, sz) || !map.world_to_index(goal, gx, gy, gz)) {
    result.reason = "start_or_goal_out_of_bounds";
    return result;
  }
  if (!map.is_traversable(sx, sy, sz)) {
    result.reason = "start_not_traversable";
    return result;
  }
  if (!map.is_traversable(gx, gy, gz)) {
    result.reason = "goal_not_traversable";
    return result;
  }

  const NodeKey start_k{sx, sy, sz};
  const NodeKey goal_k{gx, gy, gz};

  std::priority_queue<OpenItem, std::vector<OpenItem>, std::greater<OpenItem>> open;
  std::unordered_map<NodeKey, double, NodeKeyHash> g_score;
  std::unordered_map<NodeKey, NodeKey, NodeKeyHash> came_from;

  g_score[start_k] = 0.0;
  open.push(OpenItem{heuristic(start_k, goal_k, params_.global_heuristic_weight), start_k});

  // Neighbor order must match Python (dx, dy, dz nested loops) for identical paths.
  std::vector<std::array<int, 3>> dirs;
  dirs.reserve(26);
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dz = -1; dz <= 1; ++dz) {
        if (dx == 0 && dy == 0 && dz == 0) continue;
        dirs.push_back({dx, dy, dz});
      }
    }
  }

  int expansions = 0;
  while (!open.empty() && expansions < params_.global_max_expansions) {
    const NodeKey current = open.top().key;
    open.pop();
    ++expansions;

    if (current == goal_k) {
      result.success = true;
      result.expansions = expansions;
      result.reason = "ok";
      std::vector<Point3> rev;
      NodeKey cur = goal_k;
      rev.push_back(map.index_to_world(cur.x, cur.y, cur.z));
      while (!(cur == start_k)) {
        cur = came_from[cur];
        rev.push_back(map.index_to_world(cur.x, cur.y, cur.z));
      }
      result.path.assign(rev.rbegin(), rev.rend());
      return result;
    }

    const double g_cur = g_score[current];
    for (const auto& d : dirs) {
      NodeKey nxt{current.x + d[0], current.y + d[1], current.z + d[2]};
      if (!map.is_traversable(nxt.x, nxt.y, nxt.z)) continue;
      const double step = std::sqrt(static_cast<double>(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
      const double tentative = g_cur + step;
      auto it = g_score.find(nxt);
      if (it == g_score.end() || tentative < it->second) {
        came_from[nxt] = current;
        g_score[nxt] = tentative;
        open.push(OpenItem{tentative + heuristic(nxt, goal_k, params_.global_heuristic_weight), nxt});
      }
    }
  }

  result.expansions = expansions;
  result.reason = expansions >= params_.global_max_expansions ? "max_expansions" : "no_path";
  return result;
}

}  // namespace drone_pc
