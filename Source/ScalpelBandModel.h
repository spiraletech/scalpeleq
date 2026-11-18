#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace scalpel::bands {

constexpr int count = 7;
enum class Domain : int { stereo = 0, mid = 1, side = 2 };
enum class Anchor : int { left = 0, right = 1 };

struct Link {
    int band = -1;
    Anchor anchor = Anchor::left;
};

struct Band {
    bool present = true;
    double left = 20.0;
    double right = 60.0;
    Domain domain = Domain::stereo;
    std::array<Link, 2> links {};
};

// Fixed-size band layout shared by the editor and the audio processor.
struct Model {
    static constexpr double minimumRatio = 1.001;
    static constexpr std::array<double, 14> factoryBoundaries {
        20.0, 50.0, 65.0, 125.0, 160.0, 315.0, 400.0,
        800.0, 1000.0, 2500.0, 3150.0, 8000.0, 10000.0, 20000.0
    };

    std::array<Band, count> bands {};

    Model() noexcept {
        for (int i = 0; i < count; ++i) {
            bands[(size_t)i].left = factoryBoundaries[(size_t)(i * 2)];
            bands[(size_t)i].right = factoryBoundaries[(size_t)(i * 2 + 1)];
        }
    }

    static double clampHz(double value) noexcept {
        return std::clamp(value, 20.0, 20000.0);
    }

    double& frequency(int band, Anchor anchor) noexcept {
        return anchor == Anchor::left ? bands[(size_t)band].left
                                      : bands[(size_t)band].right;
    }
    double frequency(int band, Anchor anchor) const noexcept {
        return anchor == Anchor::left ? bands[(size_t)band].left
                                      : bands[(size_t)band].right;
    }
    Link& link(int band, Anchor anchor) noexcept {
        return bands[(size_t)band].links[(size_t)anchor];
    }
    Link link(int band, Anchor anchor) const noexcept {
        return bands[(size_t)band].links[(size_t)anchor];
    }

    bool validLink(int band, Anchor anchor) const noexcept {
        const auto connection = link(band, anchor);
        if (connection.band < 0) return true;
        if (connection.band >= count || connection.band == band
            || !bands[(size_t)connection.band].present
            || connection.anchor == anchor
            || bands[(size_t)connection.band].domain != bands[(size_t)band].domain)
            return false;
        const auto reverse = link(connection.band, connection.anchor);
        return reverse.band == band && reverse.anchor == anchor
            && frequency(band, anchor)
                == frequency(connection.band, connection.anchor);
    }

    bool valid() const noexcept {
        for (int i = 0; i < count; ++i) {
            const auto& band = bands[(size_t)i];
            if (!band.present) {
                if (band.links[0].band >= 0 || band.links[1].band >= 0) return false;
                continue;
            }
            if (!std::isfinite(band.left) || !std::isfinite(band.right)
                || band.left < 20.0 || band.right > 20000.0
                || band.right < band.left * minimumRatio
                || !validLink(i, Anchor::left) || !validLink(i, Anchor::right))
                return false;
            for (int j = i + 1; j < count; ++j) {
                const auto& other = bands[(size_t)j];
                if (!other.present || other.domain != band.domain) continue;
                if (std::max(band.left, other.left)
                    < std::min(band.right, other.right))
                    return false;
            }
        }
        return true;
    }

    void unlink(int band, Anchor anchor) noexcept {
        if (band < 0 || band >= count) return;
        const auto old = link(band, anchor);
        link(band, anchor) = {};
        if (old.band >= 0 && old.band < count) {
            const auto reverse = link(old.band, old.anchor);
            if (reverse.band == band && reverse.anchor == anchor)
                link(old.band, old.anchor) = {};
        }
    }

    bool connect(int first, Anchor firstAnchor,
                 int second, Anchor secondAnchor) noexcept {
        if (first < 0 || first >= count || second < 0 || second >= count
            || first == second || firstAnchor == secondAnchor
            || link(first, firstAnchor).band >= 0 || link(second, secondAnchor).band >= 0
            || !bands[(size_t)first].present || !bands[(size_t)second].present
            || bands[(size_t)first].domain != bands[(size_t)second].domain)
            return false;
        auto candidate = *this;
        const double shared = candidate.frequency(second, secondAnchor);
        if (!candidate.moveAnchor(first, firstAnchor, shared)) return false;
        candidate.link(first, firstAnchor) = { second, secondAnchor };
        candidate.link(second, secondAnchor) = { first, firstAnchor };
        if (!candidate.valid()) return false;
        *this = candidate;
        return true;
    }

    void remove(int band) noexcept {
        if (band < 0 || band >= count) return;
        unlink(band, Anchor::left);
        unlink(band, Anchor::right);
        bands[(size_t)band].present = false;
    }

    int addAt(int slot, double center, Domain domain = Domain::stereo) noexcept {
        if (slot < 0 || slot >= count || bands[(size_t)slot].present) return -1;
        center = clampHz(center);
        double low = clampHz(center / std::sqrt(2.0));
        double high = clampHz(center * std::sqrt(2.0));
        for (const auto& band : bands) {
            if (!band.present || band.domain != domain) continue;
            if (center > band.left && center < band.right) return -1;
            if (center <= band.left) high = std::min(high, band.left);
            if (center >= band.right) low = std::max(low, band.right);
        }
        if (high < low * minimumRatio) return -1;
        bands[(size_t)slot] = { true, low, high, domain, {} };
        return slot;
    }

    int add(double center, Domain domain = Domain::stereo) noexcept {
        for (int slot = 0; slot < count; ++slot)
            if (!bands[(size_t)slot].present)
                return addAt(slot, center, domain);
        return -1;
    }

    std::array<bool, count> group(int seed) const noexcept {
        std::array<bool, count> members {};
        if (seed < 0 || seed >= count || !bands[(size_t)seed].present) return members;
        members[(size_t)seed] = true;
        bool changed = true;
        while (changed) {
            changed = false;
            for (int i = 0; i < count; ++i) {
                if (!members[(size_t)i]) continue;
                for (const auto anchor : { Anchor::left, Anchor::right }) {
                    const auto connected = link(i, anchor);
                    if (connected.band >= 0 && !members[(size_t)connected.band]) {
                        members[(size_t)connected.band] = true;
                        changed = true;
                    }
                }
            }
        }
        return members;
    }

    // Returns true when the complete requested movement was legal. A blocked
    // group still moves up to the boundary as one rigid logarithmic unit.
    bool moveGroup(int seed, double octaves) noexcept {
        if (seed < 0 || seed >= count || !bands[(size_t)seed].present
            || !std::isfinite(octaves) || !valid()) return false;
        const auto members = group(seed);
        double minimumShift = -100.0;
        double maximumShift = 100.0;
        for (int i = 0; i < count; ++i) {
            if (!members[(size_t)i]) continue;
            minimumShift = std::max(minimumShift,
                std::log2(20.0 / bands[(size_t)i].left));
            maximumShift = std::min(maximumShift,
                std::log2(20000.0 / bands[(size_t)i].right));
            for (int j = 0; j < count; ++j) {
                if (members[(size_t)j] || !bands[(size_t)j].present
                    || bands[(size_t)i].domain != bands[(size_t)j].domain)
                    continue;
                if (bands[(size_t)i].right <= bands[(size_t)j].left)
                    maximumShift = std::min(maximumShift,
                        std::log2(bands[(size_t)j].left / bands[(size_t)i].right));
                if (bands[(size_t)i].left >= bands[(size_t)j].right)
                    minimumShift = std::max(minimumShift,
                        std::log2(bands[(size_t)j].right / bands[(size_t)i].left));
            }
        }
        const double applied = std::clamp(octaves, minimumShift, maximumShift);
        const auto shifted = [&](double ratio) {
            auto candidate = *this;
            for (int i = 0; i < count; ++i) {
                if (!members[(size_t)i]) continue;
                candidate.bands[(size_t)i].left *= ratio;
                candidate.bands[(size_t)i].right *= ratio;
            }
            return candidate;
        };
        const double ratio = std::exp2(applied);
        auto candidate = shifted(ratio);
        // Rounding at a blocker or spectrum edge must never commit an overlap.
        if (!candidate.valid()) candidate = shifted(std::nextafter(ratio, 1.0));
        if (!candidate.valid()) return false;
        *this = candidate;
        return std::abs(applied - octaves) < 1.0e-9;
    }

    bool moveAnchor(int band, Anchor anchor, double hz) noexcept {
        if (band < 0 || band >= count || !bands[(size_t)band].present) return false;
        auto candidate = *this;
        hz = clampHz(hz);
        candidate.frequency(band, anchor) = hz;
        const auto connected = candidate.link(band, anchor);
        if (connected.band >= 0)
            candidate.frequency(connected.band, connected.anchor) = hz;
        if (!candidate.valid()) return false;
        *this = candidate;
        return true;
    }
};

} // namespace scalpel::bands
