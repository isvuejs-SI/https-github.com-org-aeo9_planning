"""Landing upload queue. State is a single canonical JSON document."""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path

from drone_ota.yaml_subset import load_yaml

from drone_logging.events import json_escape
from drone_logging.manifest_log import sha256_hex

CONFIG_PATH = Path(__file__).resolve().parent / "config" / "logging.yaml"


@dataclass
class LogConfig:
    priorities: dict[str, int]
    landing_skip: list[str]
    retained: list[str]
    max_retries: int
    link_stable_s: int
    reserve_free_percent: int
    delete_when_free_below_percent: int
    stop_nonessential_free_percent: int
    nvme_size_gb: int
    os_and_staging_gb: int
    est_gb_per_flight: int
    lidar_mb_s: float
    video_mb_s: float
    thermal_mb_s: float
    other_mb_s: float
    max_bag_size_mb: int
    topics: list[str]


def load_log_config(path: str | None = None) -> LogConfig:
    data = load_yaml(str(path or CONFIG_PATH))
    storage = data["storage"]
    upload = data["upload"]
    return LogConfig(
        priorities={key: int(value) for key, value in data["priorities"].items()},
        landing_skip=list(data["landing_skip"]),
        retained=list(data["retained_categories"]),
        max_retries=int(upload["max_retries"]),
        link_stable_s=int(upload["link_stable_s"]),
        reserve_free_percent=int(storage["reserve_free_percent"]),
        delete_when_free_below_percent=int(storage["delete_when_free_below_percent"]),
        stop_nonessential_free_percent=int(storage["stop_nonessential_free_percent"]),
        nvme_size_gb=int(storage["nvme_size_gb"]),
        os_and_staging_gb=int(storage["os_and_staging_gb"]),
        est_gb_per_flight=int(storage["est_gb_per_flight"]),
        lidar_mb_s=float(storage["lidar_mb_s"]),
        video_mb_s=float(storage["video_mb_s"]),
        thermal_mb_s=float(storage["thermal_mb_s"]),
        other_mb_s=float(storage["other_mb_s"]),
        max_bag_size_mb=int(storage["max_bag_size_mb"]),
        topics=list(data["topics"]),
    )


def classify(name: str) -> str:
    base = name.replace("\\", "/").split("/")[-1].lower()
    if base == "events.jsonl":
        return "events"
    if base == "failsafe.jsonl":
        return "failsafe"
    if base.endswith(".ulg"):
        return "pixhawk"
    if base == "summary.md" or base.startswith("summary."):
        return "summary"
    if "lidar" in base:
        return "lidar"
    if base.endswith(".mp4"):
        return "video"
    if base.endswith(".mcap"):
        return "bag"
    return "other"


def ordered_names(names: list[str], config: LogConfig, landing_pass: bool) -> list[str]:
    skip = set(config.landing_skip) if landing_pass else set()
    kept = [name for name in names if classify(name) not in skip]
    return sorted(kept, key=lambda name: (config.priorities[classify(name)], name))


def format_queue(names: list[str], files: dict[str, bytes]) -> str:
    lines = []
    for index, name in enumerate(names, start=1):
        lines.append(f"Q{index} {name} {sha256_hex(files[name])[:12]}")
    return "\n".join(lines) + ("\n" if lines else "")


@dataclass
class Item:
    name: str
    category: str
    link: str
    sha256: str
    priority: int
    size: int
    bytes_sent: int = 0
    retries: int = 0
    state: str = "QUEUED"


def _quote(text: str) -> str:
    return '"' + json_escape(text) + '"'


def dump_items(items: list[Item]) -> str:
    ordered = sorted(items, key=lambda item: (item.priority, item.name))
    body = []
    for item in ordered:
        body.append(
            "{"
            f'"bytes_sent":{item.bytes_sent},'
            f'"category":{_quote(item.category)},'
            f'"link":{_quote(item.link)},'
            f'"name":{_quote(item.name)},'
            f'"priority":{item.priority},'
            f'"retries":{item.retries},'
            f'"sha256":{_quote(item.sha256)},'
            f'"size":{item.size},'
            f'"state":{_quote(item.state)}'
            "}"
        )
    return '{"items":[' + ",".join(body) + "]}\n"


def _field(obj: str, key: str) -> str:
    token = f'"{key}":'
    start = obj.find(token)
    if start < 0:
        raise ValueError("missing " + key)
    start += len(token)
    if obj[start] == '"':
        end = start + 1
        while obj[end] != '"':
            if obj[end] == "\\":
                end += 2
            else:
                end += 1
        return obj[start + 1 : end]
    end = start
    while end < len(obj) and obj[end] not in ",}":
        end += 1
    return obj[start:end]


def load_items(text: str) -> list[Item]:
    start = text.find("[")
    end = text.rfind("]")
    if start < 0 or end < start:
        raise ValueError("bad queue state")
    body = text[start + 1 : end].strip()
    if not body:
        return []
    chunks = body.split("},{")
    items = []
    for index, chunk in enumerate(chunks):
        piece = chunk
        if index == 0:
            piece = piece[1:] if piece.startswith("{") else piece
        if index == len(chunks) - 1:
            piece = piece[:-1] if piece.endswith("}") else piece
        piece = "{" + piece + "}"
        items.append(
            Item(
                name=_field(piece, "name"),
                category=_field(piece, "category"),
                link=_field(piece, "link"),
                sha256=_field(piece, "sha256"),
                priority=int(_field(piece, "priority")),
                size=int(_field(piece, "size")),
                bytes_sent=int(_field(piece, "bytes_sent")),
                retries=int(_field(piece, "retries")),
                state=_field(piece, "state"),
            )
        )
    return items


@dataclass
class UploadQueue:
    config: LogConfig
    items: list[Item] = field(default_factory=list)

    def add(self, name: str, data: bytes, link: str) -> None:
        category = classify(name)
        self.items.append(
            Item(
                name=name,
                category=category,
                link=link,
                sha256=sha256_hex(data),
                priority=self.config.priorities[category],
                size=len(data),
            )
        )

    def get(self, name: str) -> Item:
        for item in self.items:
            if item.name == name:
                return item
        raise KeyError(name)

    def accept_chunk(self, name: str, offset: int, nbytes: int) -> str:
        item = self.get(name)
        if offset != item.bytes_sent:
            return "OFFSET_MISMATCH"
        if item.bytes_sent + nbytes > item.size:
            return "OVERSHOOT"
        item.bytes_sent += nbytes
        if item.bytes_sent == item.size:
            item.state = "UPLOADED"
            return "COMPLETE"
        item.state = "UPLOADING"
        return "RESUME"

    def finish(self, name: str, received_sha: str) -> str:
        item = self.get(name)
        if item.state == "STUCK":
            return "STUCK"
        if item.state == "VERIFIED":
            return "VERIFIED"
        if received_sha == item.sha256:
            item.state = "VERIFIED"
            item.bytes_sent = item.size
            return "VERIFIED"
        item.retries += 1
        item.bytes_sent = 0
        if item.retries >= self.config.max_retries:
            item.state = "STUCK"
            return "STUCK"
        item.state = "QUEUED"
        return "RETRY"

    def dump(self) -> str:
        return dump_items(self.items)

    def save(self, path: str) -> None:
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(self.dump())

    @classmethod
    def load(cls, path: str, config: LogConfig) -> "UploadQueue":
        with open(path, encoding="utf-8") as handle:
            return cls(config=config, items=load_items(handle.read()))


def link_ready(up_for_s: float, landed: bool, config: LogConfig) -> bool:
    return landed and up_for_s >= config.link_stable_s
