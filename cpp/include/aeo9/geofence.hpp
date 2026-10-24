#pragma once

#include <cstdint>
#include <string>

namespace aeo9 {

enum class GeofenceStatus { INSIDE, BREACH };

class Geofence {
 public:
  Geofence(int32_t home_north_cm, int32_t home_east_cm, int32_t radius_cm)
      : home_north_cm_(home_north_cm),
        home_east_cm_(home_east_cm),
        radius_cm_(radius_cm) {}

  int32_t distance_cm(int32_t north_cm, int32_t east_cm) const;
  GeofenceStatus check(int32_t north_cm, int32_t east_cm);
  std::string dump_line(int32_t north_cm, int32_t east_cm);

 private:
  int32_t home_north_cm_;
  int32_t home_east_cm_;
  int32_t radius_cm_;
  bool breached_{false};
};

}  // namespace aeo9
