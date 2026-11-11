// Stand-alone tests for the tempo-map core (no JUCE required).
// Build: g++ -O2 -std=c++17 -I../Source tempo_engine_test.cpp ../Source/TempoEngine.cpp -o tempo_engine_test
#include "TempoEngine.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

using namespace dogside;
static int failures = 0;
#define CHECK(c, m) do { if (c) std::printf("  ok: %s\n", m); else { std::printf("  FAIL: %s\n", m); ++failures; } } while (0)

int main()
{
    // Eight musical beats performed with small timing deviations.
    std::vector<BeatAnchor> anchors;
    const double offsets[] = { 0.000, 0.515, 0.992, 1.510, 1.996, 2.505, 2.989 };
    for (int i = 0; i < 7; ++i) anchors.push_back ({ offsets[i], (double) i });

    TempoMapSettings settings;
    settings.referenceBpm = 120.0;
    settings.gridBeats = 1.0;
    settings.quantize = 1.0;
    auto map = buildTempoMap (anchors, 4.0, settings);
    CHECK (map.valid(), "tempo map generated from beat anchors");
    CHECK (map.points.size() >= 2, "map contains multiple tempo segments");
    bool ordered = true;
    for (size_t i = 1; i < map.points.size(); ++i)
        ordered = ordered && map.points[i].beat > map.points[i - 1].beat
                          && map.points[i].seconds > map.points[i - 1].seconds;
    CHECK (ordered, "tempo points are strictly monotonic");
    CHECK (std::isfinite (map.points.front().bpm), "tempo values are finite");

    const char* path = "tempo_map_test.mid";
    CHECK (writeTempoMapMidi (map, path), "MIDI tempo map exported");
    std::ifstream f (path, std::ios::binary);
    std::vector<unsigned char> bytes ((std::istreambuf_iterator<char> (f)), {});
    CHECK (bytes.size() > 26 && bytes[0] == 'M' && bytes[1] == 'T' && bytes[2] == 'h' && bytes[3] == 'd',
           "export is a Standard MIDI File");
    std::remove (path);

    settings.quantize = 0.0;
    auto untouched = buildTempoMap (anchors, 4.0, settings);
    CHECK (untouched.valid(), "zero-strength map remains valid");
    CHECK (std::abs (untouched.points[1].beat - anchors[1].beat) < 1.0e-9,
           "zero strength does not snap beat anchors");

    // Synthetic 120 BPM full-song click track with small timing drift.
    AudioData click;
    click.sampleRate = 8000.0;
    click.ch.resize (1);
    click.ch[0].assign (click.sampleRate * 8.0, 0.0f);
    for (int beat = 0; beat < 16; ++beat)
    {
        const double t = beat * 0.5 + (beat % 3 == 0 ? 0.008 : -0.006);
        const int at = (int) std::lround (t * click.sampleRate);
        for (int n = 0; n < 100 && at + n < (int) click.ch[0].size(); ++n)
            click.ch[0][(size_t) (at + n)] = (float) (0.8 * std::exp (-n / 18.0));
    }
    BeatAnalysisSettings beatSettings;
    beatSettings.minBpm = 80.0;
    beatSettings.maxBpm = 160.0;
    const auto detected = analyzeBeats (click, beatSettings);
    CHECK (detected.valid(), "full-song beat tracker detects synthetic click track");
    CHECK (std::abs (detected.estimatedBpm - 120.0) < 4.0, "beat tracker estimates approximately 120 BPM");
    CHECK (detected.anchors.size() >= 10, "beat tracker returns a sequence of musical anchors");

    TempoMapSettings fullGrid;
    fullGrid.referenceBpm = 120.0;
    fullGrid.gridBeats = 0.25;
    fullGrid.quantize = 1.0;
    auto quantized = buildTempoMap (detected.anchors, 8.0, fullGrid);
    CHECK (quantized.valid(), "full-grid tempo automation map generated");
    bool finiteTempo = true;
    for (const auto& point : quantized.points)
        finiteTempo = finiteTempo && std::isfinite (point.bpm) && point.bpm >= fullGrid.minBpm && point.bpm <= fullGrid.maxBpm;
    CHECK (finiteTempo, "tempo automation remains within configured BPM limits");
    bool onGrid = true;
    for (size_t i = 1; i + 1 < quantized.points.size(); ++i)
    {
        const double slots = quantized.points[i].beat / fullGrid.gridBeats;
        onGrid = onGrid && std::abs (slots - std::round (slots)) < 1.0e-6;
    }
    CHECK (onGrid, "100 percent quantization places detected beats exactly on the selected grid");

    std::printf ("\n%s\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED");
    return failures == 0 ? 0 : 1;
}
