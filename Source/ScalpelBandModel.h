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
        20.0, 60.0, 60.0, 120.0, 120.0, 250.0, 250.0,
        500.0, 500.0, 2000.0, 2000.0, 6000.0, 6000.0, 20000.0
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

    bool restore(int slot) noexcept {
        if (slot < 0 || slot >= count || bands[(size_t)slot].present) return false;
        const auto previous=bands[(size_t)slot];
        auto candidate=*this;
        candidate.bands[(size_t)slot].present=true;
        if (!candidate.valid()) {
            double bestDistance=100.0;
            int restored=-1;
            // Free intervals start at 20 Hz or at the right edge of a present band.
            for (int i=-1; i<count; ++i) {
                double low=20.0;
                if (i>=0) {
                    const auto& band=bands[(size_t)i];
                    if (!band.present || band.domain!=previous.domain) continue;
                    low=band.right;
                }
                double high=20000.0;
                for (const auto& band:bands)
                    if (band.present && band.domain==previous.domain && band.left>=low)
                        high=std::min(high,band.left);
                if (high<low*minimumRatio) continue;
                const double width=std::min(previous.right/previous.left, high/low);
                const double left=std::clamp(previous.left,low,std::max(low,high/width));
                const double right=std::min(high,left*width);
                const double distance=std::abs(std::log(left/previous.left));
                if (distance>=bestDistance) continue;
                candidate.bands[(size_t)slot]={true,left,right,previous.domain,{}};
                bestDistance=distance;
                restored=slot;
            }
            if (restored<0) {
                // The vacancy may have been covered by a later drag. Make
                // space at the nearest editable seam by shortening one band.
                for (int i=0; i<count; ++i) {
                    const auto& band=bands[(size_t)i];
                    if (!band.present || band.domain!=previous.domain) continue;
                    const double available=std::sqrt(band.right/band.left);
                    if (available<minimumRatio+0.001) continue;
                    const double ratio=std::min(previous.right/previous.left,available);
                    for (const auto anchor:{Anchor::left,Anchor::right}) {
                        if (link(i,anchor).band>=0) continue;
                        const double edge=frequency(i,anchor);
                        const double distance=std::abs(std::log(
                            edge/std::sqrt(previous.left*previous.right)));
                        if (distance>=bestDistance) continue;
                        const double seam=anchor==Anchor::left?edge*ratio:edge/ratio;
                        candidate=*this;
                        candidate.frequency(i,anchor)=seam;
                        candidate.bands[(size_t)slot]={true,std::min(edge,seam),
                            std::max(edge,seam),previous.domain,{}};
                        bestDistance=distance;
                        restored=slot;
                    }
                }
            }
            if (restored<0 || !candidate.valid()) return false;
        }
        *this=candidate;
        return true;
    }

    Link touchingNeighbor(int band, Anchor anchor) const noexcept {
        const auto& source=bands[(size_t)band];
        const auto opposite=anchor==Anchor::left?Anchor::right:Anchor::left;
        for (int i=0; i<count; ++i)
            if (i!=band && bands[(size_t)i].present && bands[(size_t)i].domain==source.domain
                && frequency(i,opposite)==frequency(band,anchor))
                return {i,opposite};
        return {};
    }

    bool moveSeam(int band, Anchor anchor, double hz) noexcept {
        const auto neighbor=touchingNeighbor(band,anchor);
        if (neighbor.band<0 || link(band,anchor).band>=0) return moveAnchor(band,anchor,hz);
        auto candidate=*this;
        candidate.frequency(band,anchor)=candidate.frequency(neighbor.band,neighbor.anchor)=clampHz(hz);
        if (!candidate.valid()) return false;
        *this=candidate;
        return true;
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

    // The selected group translates at fixed width. A touching band behind it
    // stretches to keep the seam closed. Bands ahead yield only as needed.
    bool moveGroup(int seed, double octaves) noexcept {
        if (seed<0 || seed>=count || !bands[(size_t)seed].present
            || !std::isfinite(octaves) || !valid()) return false;
        if (octaves==0.0) return true;
        const auto members=group(seed);
        const bool rightward=octaves>0.0;
        const auto entry=rightward?Anchor::left:Anchor::right;
        const auto exit=rightward?Anchor::right:Anchor::left;
        const auto ahead=[rightward](double a,double b){return rightward?a>b:a<b;};
        std::array<int,count> order {};
        int size=0;
        int first=seed,last=seed;
        for (int i=0; i<count; ++i) {
            if (!bands[(size_t)i].present || bands[(size_t)i].domain!=bands[(size_t)seed].domain)
                continue;
            if (members[(size_t)i]) {
                if (ahead(frequency(first,entry),frequency(i,entry))) first=i;
                if (ahead(frequency(i,exit),frequency(last,exit))) last=i;
            } else {
                order[(size_t)size++]=i;
            }
        }
        std::sort(order.begin(),order.begin()+size,[&](int a,int b){
            return ahead(frequency(b,entry),frequency(a,entry));
        });
        const auto behind=touchingNeighbor(first,entry);
        const auto translated=[&](double fraction) {
            auto candidate=*this;
            const double shift=std::exp2(octaves*fraction);
            for (int i=0; i<count; ++i) {
                if (!members[(size_t)i]) continue;
                candidate.bands[(size_t)i].left*=shift;
                candidate.bands[(size_t)i].right*=shift;
            }
            if (behind.band>=0)
                candidate.frequency(behind.band,behind.anchor)=candidate.frequency(first,entry);

            double seam=candidate.frequency(last,exit);
            int previous=last;
            for (int index=0; index<size; ++index) {
                const int i=order[(size_t)index];
                if (ahead(frequency(last,exit),frequency(i,entry))) continue;
                if (!ahead(seam,frequency(i,entry))
                    && link(previous,exit).band!=i) break;
                candidate.frequency(i,entry)=seam;
                // Leave a usable strip when a drag runs against the graph edge.
                // Respect a narrower territory that was already user-defined.
                const auto& originalBand=bands[(size_t)i];
                const double dragRatio=std::clamp(
                    originalBand.right/originalBand.left,minimumRatio+0.001,1.12);
                const double minExit=rightward?seam*dragRatio:seam/dragRatio;
                if (ahead(minExit,frequency(i,exit))) candidate.frequency(i,exit)=minExit;
                seam=candidate.frequency(i,exit);
                previous=i;
            }
            return candidate;
        };
        auto candidate=translated(1.0);
        if (candidate.valid()) {
            *this=candidate;
            return true;
        }
        double low=0.0,high=1.0;
        for (int i=0; i<36; ++i) {
            const double middle=(low+high)*0.5;
            candidate=translated(middle);
            if (candidate.valid()) low=middle;
            else high=middle;
        }
        if (low>0.0) *this=translated(low);
        return false;
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
