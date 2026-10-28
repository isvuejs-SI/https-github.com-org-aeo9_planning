#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace drone_safety {

// systemd Type=notify / WatchdogSec. NOTIFY_SOCKET datagram, no libsystemd required.
inline bool sd_notify(const std::string& message) {
  const char* addr = std::getenv("NOTIFY_SOCKET");
  if (!addr || !*addr) return false;
  int fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
  if (fd < 0) return false;
  sockaddr_un un {};
  un.sun_family = AF_UNIX;
  std::string path(addr);
  if (!path.empty() && path[0] == '@') path[0] = '\0';
  if (path.size() >= sizeof(un.sun_path)) {
    ::close(fd);
    return false;
  }
  std::memcpy(un.sun_path, path.data(), path.size());
  socklen_t len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size());
  if (path[0] != '\0') len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + std::strlen(un.sun_path) + 1);
  const ssize_t n = ::sendto(fd, message.data(), message.size(), 0, reinterpret_cast<sockaddr*>(&un), len);
  ::close(fd);
  return n == static_cast<ssize_t>(message.size());
}

inline bool notify_watchdog() { return sd_notify("WATCHDOG=1"); }
inline bool notify_ready() { return sd_notify("READY=1"); }

inline std::string systemd_watchdog_sec(const std::string& service_path) {
  std::ifstream in(service_path);
  std::string line;
  while (std::getline(in, line)) {
    const std::string prefix = "WatchdogSec=";
    if (line.compare(0, prefix.size(), prefix) == 0) return line.substr(prefix.size());
  }
  return "";
}

}  // namespace drone_safety
