"""Supervisor failsafe. Latch never steps back toward NOMINAL.

PX4 keeps its own failsafes. This state only chooses the next request.
"""

_RANK = {"NOMINAL": 0, "HOLD": 1, "RTL": 2, "LAND": 3}


def fs_request(state, req):
    if _RANK[req] > _RANK[state]:
        return req
    return state
