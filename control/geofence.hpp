#pragma once
// Distance from home, integer meters. Breach is strictly outside the radius.

inline long long dist2(long long x, long long y) { return x * x + y * y; }

inline int breached(long long x, long long y, long long radius) {
    return dist2(x, y) > radius * radius ? 1 : 0;
}
