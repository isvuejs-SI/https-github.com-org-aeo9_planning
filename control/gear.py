"""Stow before arming. Deploy only after landing. Refuse otherwise."""


class Gear(object):
    def __init__(self):
        self.phase = "DEPLOYED"
        self.landed = 1
        self.air = 0

    def request_stow(self):
        if self.air or not self.landed:
            return "REFUSE airborne"
        self.phase = "STOWING"
        return "OK stowing"

    def complete_stow(self):
        if self.air or not self.landed:
            return "REFUSE airborne"
        if self.phase != "STOWING":
            return "REFUSE bad_phase"
        self.phase = "STOWED"
        return "OK stowed"

    def request_deploy(self):
        if self.air or not self.landed:
            return "REFUSE not_landed"
        self.phase = "DEPLOYING"
        return "OK deploying"

    def complete_deploy(self):
        if self.air or not self.landed:
            return "REFUSE not_landed"
        if self.phase != "DEPLOYING":
            return "REFUSE bad_phase"
        self.phase = "DEPLOYED"
        return "OK deployed"

    def arm(self):
        if self.phase != "STOWED":
            return "REFUSE not_stowed"
        if self.air or not self.landed:
            return "REFUSE airborne"
        return "OK armed"
