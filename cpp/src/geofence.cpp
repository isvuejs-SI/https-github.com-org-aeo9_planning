#include "aeo9/geofence.hpp"

#include <cstdlib>

namespace aeo9 {

int32_t Geofence::distance_cm(int32_t north_cm, int32_t east_cm) const {
  const int32_t dn = std::abs(north_cm - home_north_cm_);
  const int32_t de = std::abs(east_cm - home_east_cm_);
  if (dn > de) return dn + de / 2;
  return de + dn / 2;
}

GeofenceStatus Geofence::check(int32_t north_cm, int32_t east_cm) {
  const int32_t dist = distance_cm(north_cm, east_cm);
  if (dist > radius_cm_) {
    breached_ = true;
    return GeofenceStatus::BREACH;
  }
  return breached_ ? GeofenceStatus::BREACH : GeofenceStatus::INSIDE;
}

std::string Geofence::dump_line(int32_t north_cm, int32_t east_cm) {
  const int32_t dist = distance_cm(north_cm, east_cm);
  const GeofenceStatus status = check(north_cm, east_cm);
  const char* name = status == GeofenceStatus::BREACH ? "BREACH" : "INSIDE";
  return std::string("geofence status=") + name +
         " dist_cm=" + std::to_string(dist) +
         " radius_cm=" + std::to_string(radius_cm_);
}

}  // namespace aeo9
