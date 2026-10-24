from __future__ import annotations

from dataclasses import dataclass
from enum import Enum

from drone_pc.params import Params


class LinkLossAction(str, Enum):
    NONE = "NONE"
    RTL = "RTL"
    ABORT_MISSION = "ABORT_MISSION"


@dataclass
class LinkLossResult:
    action: LinkLossAction = LinkLossAction.NONE
    disconnected_s: float = 0.0
    reason: str = ""


class LinkLossMonitor:
    def __init__(self, params: Params):
        self.params = params
        self._disconnected_s = 0.0

    @property
    def disconnected_s(self) -> float:
        return self._disconnected_s

    def update(self, dt_s: float, link_ok: bool) -> LinkLossResult:
        if link_ok:
            self._disconnected_s = 0.0
            return LinkLossResult(LinkLossAction.NONE, 0.0, "link_ok")

        self._disconnected_s += dt_s
        if self._disconnected_s > self.params.link_loss_abort_after_s:
            return LinkLossResult(
                LinkLossAction.ABORT_MISSION, self._disconnected_s, "link_loss_abort"
            )
        if self._disconnected_s > self.params.link_loss_rtl_after_s:
            return LinkLossResult(LinkLossAction.RTL, self._disconnected_s, "link_loss_rtl")
        return LinkLossResult(LinkLossAction.NONE, self._disconnected_s, "link_loss_pending")
