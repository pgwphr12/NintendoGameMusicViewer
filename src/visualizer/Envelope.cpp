#include "Envelope.hpp"
#include <algorithm>
#include <cmath>
namespace ngmv {
std::vector<EnvelopePoint> envelope(const std::vector<float> &s, size_t columns, size_t window) {
    std::vector<EnvelopePoint> out(columns);
    if (s.empty() || columns == 0)
        return out;
    window = std::min(window, s.size());
    size_t begin = s.size() - window;
    // Seek a rising zero crossing before the newest display window, keeping silence a stable
    // baseline.
    size_t lo = begin > window / 2 ? begin - window / 2 : 0;
    for (size_t i = begin; i > lo; i--)
        if (s[i - 1] <= 0 && s[i] > 0) {
            begin = i;
            break;
        }
    for (size_t x = 0; x < columns; x++) {
        size_t a = begin + x * window / columns,
               b = std::min(s.size(), begin + (x + 1) * window / columns);
        b = std::max(a + 1, b);
        b = std::min(b, s.size());
        float mn = s[a], mx = mn, sum = 0;
        for (size_t i = a; i < b; i++) {
            mn = std::min(mn, s[i]);
            mx = std::max(mx, s[i]);
            sum += s[i];
        }
        out[x] = {mn, mx, sum / float(b - a)};
    }
    return out;
}
} // namespace ngmv
