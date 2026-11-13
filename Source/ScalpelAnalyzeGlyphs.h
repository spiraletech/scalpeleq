#pragma once
#include <JuceHeader.h>
namespace scalpel::analyzeGlyphs {
// The supplied threshold trace merges the flame tip with its inner void.
// Clean its single contour against the approved source: one amber outline,
// no extra inner tongue, filled cap or detached threshold specks.
inline const juce::Path& flame(){static const juce::Path path=[]{
    juce::Path contour,inlay;
    contour.restoreFromString("m 40 9 c 43 26 21 34 24 58 c 18 57 15 53 13 48 c 10 59 4 66 5 78 c 6 96 22 105 40 102 c 59 102 72 89 72 72 c 72 59 63 48 61 39 l 54 52 c 40 45 54 27 40 9 z");
    juce::PathStrokeType(3.4f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded).createStrokedPath(inlay,contour);
    return inlay;
}();return path;}
inline const juce::Path& snowflake(){static const juce::Path path=[]{juce::Path p;p.restoreFromString("m 54 14 l 53 15 l 53 24 l 52 25 l 51 25 l 47 21 l 45 21 l 45 23 l 47 25 l 47 26 l 50 29 l 51 29 l 53 32 l 53 49 l 52 50 l 50 50 l 48 48 l 40 44 l 34 39 l 31 28 l 29 29 l 29 32 l 30 33 l 30 36 l 29 37 l 28 37 l 26 35 l 20 32 l 18 33 l 18 34 l 20 36 l 21 36 l 23 38 l 27 40 l 27 41 l 25 43 l 20 44 l 20 46 l 21 47 l 23 46 l 25 46 l 26 45 l 29 45 l 30 44 l 34 44 l 38 46 l 40 48 l 43 49 l 49 54 l 48 56 l 47 56 l 45 58 l 40 60 l 38 62 l 34 64 l 30 64 l 29 63 l 27 63 l 26 62 l 21 61 l 20 63 l 23 65 l 25 65 l 26 66 l 26 68 l 22 71 l 19 72 l 18 74 l 20 76 l 26 73 l 28 71 l 29 71 l 30 73 l 29 74 l 28 79 l 31 79 l 31 77 l 32 76 l 32 74 l 33 73 l 33 70 l 34 68 l 38 65 l 41 64 l 43 62 l 48 60 l 50 58 l 52 58 l 53 59 l 53 77 l 44 86 l 44 87 l 46 88 l 49 85 l 52 84 l 53 85 l 53 95 l 56 95 l 56 85 l 58 84 l 60 85 l 63 88 l 65 86 l 58 79 l 56 78 l 56 59 l 57 58 l 62 60 l 64 62 l 72 66 l 75 69 l 76 71 l 76 73 l 77 74 l 77 77 l 79 80 l 80 80 l 80 74 l 79 73 l 80 71 l 83 72 l 85 74 l 89 76 l 91 75 l 91 73 l 90 72 l 85 70 l 83 68 l 83 66 l 84 65 l 86 65 l 89 63 l 89 61 l 86 61 l 82 63 l 75 64 l 69 61 l 67 59 l 62 57 l 60 55 l 60 53 l 61 52 l 67 49 l 69 47 l 72 46 l 74 44 l 79 44 l 80 45 l 82 45 l 83 46 l 86 46 l 88 47 l 89 44 l 83 42 l 82 40 l 86 38 l 88 36 l 89 36 l 91 34 l 91 33 l 89 32 l 83 35 l 81 37 l 80 37 l 79 36 l 80 34 l 80 29 l 79 28 l 77 31 l 77 33 l 76 34 l 76 37 l 75 39 l 69 44 l 57 50 l 56 49 l 56 31 l 59 28 l 60 28 l 65 23 l 65 22 l 64 21 l 62 21 l 58 25 l 57 25 l 56 24 l 56 14 z");p.setUsingNonZeroWinding(false);return p;}();return path;}
}
