#pragma once

#include <atomic>
#include <cstdint>

namespace scalpel::analysis {

// Diagnostics only. All updates are bounded, lock-free atomic operations;
// none of these counters participates in audio processing or EQ decisions.
struct AnalyzerHealthSnapshot {
    std::uint64_t acceptedSamples = 0;
    std::uint64_t droppedSamples = 0;
    std::uint64_t completedFftFrames = 0;
};

class AnalyzerHealth final {
public:
    void reset() noexcept {
        accepted_.store(0, std::memory_order_relaxed);
        dropped_.store(0, std::memory_order_relaxed);
        frames_.store(0, std::memory_order_relaxed);
    }

    void recordAudioBlock(int requested, int accepted) noexcept {
        if (requested <= 0) return;
        const auto valid = accepted < 0 ? 0 : (accepted > requested ? requested : accepted);
        accepted_.fetch_add(static_cast<std::uint64_t>(valid), std::memory_order_relaxed);
        dropped_.fetch_add(static_cast<std::uint64_t>(requested - valid), std::memory_order_relaxed);
    }

    void recordFftFrame() noexcept {
        frames_.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] AnalyzerHealthSnapshot snapshot() const noexcept {
        return {accepted_.load(std::memory_order_relaxed),
                dropped_.load(std::memory_order_relaxed),
                frames_.load(std::memory_order_relaxed)};
    }

private:
    std::atomic<std::uint64_t> accepted_{0}, dropped_{0}, frames_{0};
};

} // namespace scalpel::analysis
