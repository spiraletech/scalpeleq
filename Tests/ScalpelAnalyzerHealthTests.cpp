#include "../Source/ScalpelAnalyzerHealth.h"
#include <cassert>
#include <iostream>

int main() {
    scalpel::analysis::AnalyzerHealth stats;
    auto initial = stats.snapshot();
    assert(initial.acceptedSamples == 0 && initial.droppedSamples == 0);
    assert(initial.completedFftFrames == 0);

    stats.recordAudioBlock(512, 512);
    stats.recordAudioBlock(512, 384); // Full analyzer queue: lose 128 samples.
    stats.recordAudioBlock(128, 0);
    stats.recordAudioBlock(0, 0);
    stats.recordFftFrame();
    stats.recordFftFrame();

    auto result = stats.snapshot();
    assert(result.acceptedSamples == 896);
    assert(result.droppedSamples == 256);
    assert(result.completedFftFrames == 2);

    stats.reset();
    result = stats.snapshot();
    assert(result.acceptedSamples == 0 && result.droppedSamples == 0);
    assert(result.completedFftFrames == 0);
    std::cout << "Scalpel analyzer transport counters PASS\n";
}
