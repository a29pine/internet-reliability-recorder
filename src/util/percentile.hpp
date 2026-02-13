#pragma once
#include <algorithm>
#include <vector>

namespace irr {
inline double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    double rank = (p / 100.0) * (v.size() - 1);
    size_t lo = static_cast<size_t>(rank);
    size_t hi = std::min(lo + 1, v.size() - 1);
    double frac = rank - lo;
    return v[lo] + (v[hi] - v[lo]) * frac;
}
}  // namespace irr
