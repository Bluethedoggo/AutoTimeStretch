// Shared tempo analysis / tempo-map core for AutoWarp and AutoTimeStretch.
// Standard C++ only: no JUCE dependency.
#pragma once

#include "WarpEngine.h"
#include <cstdint>
#include <string>
#include <vector>

namespace dogside
{
constexpr int PPQ_DEFAULT = 960;

struct BeatAnchor
{
    double seconds = 0.0;
    double beat = 0.0;
    double confidence = 0.0;
};

struct TempoPoint
{
    double beat = 0.0;
    double seconds = 0.0;
    double bpm = 120.0;
};

struct TempoMapSettings
{
    double referenceBpm = 120.0;
    double gridBeats = 0.25;
    double quantize = 1.0;       // 0..1
    double minBpm = 40.0;
    double maxBpm = 240.0;
    double minimumIntervalSeconds = 0.025;
    double smoothing = 0.0;      // optional; keep 0 for exact beat-to-grid timing
};

struct BeatAnalysisSettings
{
    double minBpm = 60.0;
    double maxBpm = 200.0;
    double sensitivity = 0.50;   // 0..1
    double tempoPreferenceBpm = 120.0;
    double preference = 0.20;    // 0..1; bias ambiguous half/double-time choices
    double minimumConfidence = 0.05;
};

struct BeatAnalysis
{
    std::vector<BeatAnchor> anchors;
    double estimatedBpm = 0.0;
    double confidence = 0.0;
    bool valid() const noexcept { return anchors.size() >= 2 && estimatedBpm > 0.0; }
};

struct TempoMap
{
    std::vector<TempoPoint> points;
    bool valid() const noexcept { return points.size() >= 2; }
};

// Estimates a beat grid from a complete music recording. Returns beat anchors
// at quarter-note positions; it does not modify or render the source audio.
BeatAnalysis analyzeBeats (const AudioData& audio, const BeatAnalysisSettings& settings = {});

TempoMap buildTempoMap (const std::vector<BeatAnchor>& anchors,
                        double audioDurationSeconds,
                        const TempoMapSettings& settings);

bool writeTempoMapMidi (const TempoMap& map, const std::string& path,
                        int ticksPerQuarter = PPQ_DEFAULT);
}
