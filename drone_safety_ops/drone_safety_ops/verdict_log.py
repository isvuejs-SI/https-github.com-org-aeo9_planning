import os


def json_escape(text):
    return "".join(ch for ch in text if ch not in '"\\\n\r')


def format_record(record):
    transition = "true" if record["transition"] else "false"
    return (
        '{"ts":"%s","module":"%s","verdict":"%s","previous":"%s","transition":%s,"px4":"%s","reasons":"%s"}'
        % (record["ts"], record["module"], record["verdict"], record["previous"], transition, record["px4"],
           json_escape(record["reasons"]))
    )


class VerdictLog:
    def __init__(self, directory):
        os.makedirs(directory, exist_ok=True)
        self.path = os.path.join(directory, "verdicts.jsonl")
        self.last = {}

    def append(self, ts, module, verdict, px4, reasons):
        previous = self.last.get(module, "")
        record = {
            "ts": ts,
            "module": module,
            "verdict": verdict,
            "previous": previous,
            "transition": module not in self.last or previous != verdict,
            "px4": px4,
            "reasons": reasons,
        }
        self.last[module] = verdict
        with open(self.path, "a", encoding="utf-8") as handle:
            handle.write(format_record(record) + "\n")
        return record


def directory_writable(directory):
    try:
        os.makedirs(directory, exist_ok=True)
        probe = os.path.join(directory, ".write_probe")
        with open(probe, "a", encoding="utf-8") as handle:
            handle.write("ok\n")
        os.remove(probe)
        return True
    except OSError:
        return False


def choose_log_dir(nvme_dir, fallback_dir):
    if directory_writable(nvme_dir):
        return nvme_dir
    if directory_writable(fallback_dir):
        return fallback_dir
    return fallback_dir
