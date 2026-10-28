import os
import socket


def sd_notify(message):
    addr = os.environ.get("NOTIFY_SOCKET")
    if not addr:
        return False
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    try:
        if addr.startswith("@"):
            sock.sendto(message.encode(), addr.encode().replace(b"@", b"\0", 1))
        else:
            sock.sendto(message.encode(), addr)
        return True
    except OSError:
        return False
    finally:
        sock.close()


def notify_watchdog():
    return sd_notify("WATCHDOG=1")


def notify_ready():
    return sd_notify("READY=1")


def systemd_watchdog_sec(service_path):
    try:
        with open(service_path, "r", encoding="utf-8") as handle:
            for line in handle:
                if line.startswith("WatchdogSec="):
                    return line.split("=", 1)[1].strip()
    except OSError:
        return ""
    return ""


def watchdog_roundtrip(path):
    try:
        if os.path.exists(path):
            os.remove(path)
    except OSError:
        return "watchdog_socket fail"
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    try:
        srv.bind(path)
        srv.settimeout(1)
        os.environ["NOTIFY_SOCKET"] = path
        ok = notify_watchdog()
        data, _ = srv.recvfrom(64)
        if ok and data == b"WATCHDOG=1":
            return "watchdog_socket ok"
        return "watchdog_socket fail"
    except OSError:
        return "watchdog_socket fail"
    finally:
        srv.close()
        os.environ.pop("NOTIFY_SOCKET", None)
        try:
            os.remove(path)
        except OSError:
            pass
