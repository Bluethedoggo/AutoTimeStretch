#include "TempoEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <numeric>

namespace dogside
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
double clampd (double x, double lo, double hi) { return std::max (lo, std::min (hi, x)); }

void appendU16 (std::vector<unsigned char>& o, unsigned int v)
{ o.push_back ((unsigned char) (v >> 8)); o.push_back ((unsigned char) v); }
void appendU32 (std::vector<unsigned char>& o, std::uint32_t v)
{ o.push_back ((unsigned char) (v >> 24)); o.push_back ((unsigned char) (v >> 16)); o.push_back ((unsigned char) (v >> 8)); o.push_back ((unsigned char) v); }
void appendVlq (std::vector<unsigned char>& o, std::uint32_t v)
{
    unsigned char b[4]; int n = 0; b[n++] = (unsigned char) (v & 0x7f);
    while ((v >>= 7) && n < 4) b[n++] = (unsigned char) ((v & 0x7f) | 0x80);
    while (n) o.push_back (b[--n]);
}

// Positive spectral-like flux from frame RMS/peak energy. A 10 ms hop gives
// enough temporal resolution for beat tracking without expensive FFT work.
std::vector<double> makeOnsetEnvelope (const std::vector<float>& x, double sr, int hop)
{
    const int frame = std::max (hop * 2, (int) std::lround (0.025 * sr));
    const size_t count = x.size() > (size_t) frame ? 1 + (x.size() - (size_t) frame) / (size_t) hop : 0;
    std::vector<double> env (count, 0.0);
    double previous = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t start = i * (size_t) hop;
        double energy = 0.0, peak = 0.0;
        for (int n = 0; n < frame; n += 2)
        {
            const double v = std::abs ((double) x[start + (size_t) n]);
            energy += v * v; peak = std::max (peak, v);
        }
        const double rms = std::sqrt (energy / std::max (1, frame / 2));
        const double current = 0.7 * rms + 0.3 * peak;
        env[i] = std::max (0.0, current - previous);
        previous = current * 0.75 + previous * 0.25;
    }
    // Light smoothing and local-mean subtraction.
    std::vector<double> smooth (env.size(), 0.0);
    for (size_t i = 0; i < env.size(); ++i)
    {
        const double a = env[i > 0 ? i - 1 : i];
        const double b = env[i];
        const double c = env[std::min (env.size() - 1, i + 1)];
        smooth[i] = std::max (0.0, (a + 2.0 * b + c) * 0.25);
    }
    return smooth;
}
}

BeatAnalysis analyzeBeats (const AudioData& audio, const BeatAnalysisSettings& raw)
{
    BeatAnalysis result;
    if (audio.ch.empty() || audio.numSamples() < (size_t) (audio.sampleRate * 2.0) || audio.sampleRate <= 0.0)
        return result;
    const double minBpm = clampd (raw.minBpm, 40.0, 240.0);
    const double maxBpm = clampd (raw.maxBpm, minBpm + 1.0, 300.0);
    const int hop = std::max (1, (int) std::lround (audio.sampleRate * 0.010));
    const auto mono = mixToMono (audio);
    auto env = makeOnsetEnvelope (mono, audio.sampleRate, hop);
    if (env.size() < 100) return result;

    const double mean = std::accumulate (env.begin(), env.end(), 0.0) / env.size();
    double variance = 0.0;
    for (double v : env) variance += (v - mean) * (v - mean);
    const double sd = std::sqrt (variance / env.size());
    if (sd < 1.0e-9) return result;
    for (double& v : env) v = std::max (0.0, v - mean * (0.35 + 0.4 * clampd (raw.sensitivity, 0.0, 1.0)));

    const int minLag = std::max (1, (int) std::floor (60.0 / maxBpm / 0.010));
    const int maxLag = std::min ((int) env.size() / 2, (int) std::ceil (60.0 / minBpm / 0.010));
    if (maxLag <= minLag) return result;

    std::vector<double> corr ((size_t) maxLag + 1, 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0, normA = 1.0e-12, normB = 1.0e-12;
        for (size_t i = (size_t) lag; i < env.size(); i += 2)
        {
            const double a = env[i], b = env[i - (size_t) lag];
            sum += a * b; normA += a * a; normB += b * b;
        }
        const double bpm = 60.0 / (lag * 0.010);
        const double pref = clampd (raw.preference, 0.0, 1.0);
        const double bias = std::exp (-pref * std::abs (std::log (bpm / clampd (raw.tempoPreferenceBpm, minBpm, maxBpm))));
        corr[(size_t) lag] = (sum / std::sqrt (normA * normB)) * (0.65 + 0.35 * bias);
    }
    int bestLag = minLag;
    for (int lag = minLag + 1; lag <= maxLag; ++lag)
        if (corr[(size_t) lag] > corr[(size_t) bestLag]) bestLag = lag;

    // Refine lag parabolically around the best autocorrelation peak.
    double refinedLag = (double) bestLag;
    if (bestLag > minLag && bestLag < maxLag)
    {
        const double l = corr[(size_t) bestLag - 1], c = corr[(size_t) bestLag], r = corr[(size_t) bestLag + 1];
        const double denom = l - 2.0 * c + r;
        if (std::abs (denom) > 1.0e-12) refinedLag += clampd (0.5 * (l - r) / denom, -0.5, 0.5);
    }
    const double period = refinedLag * 0.010;
    result.estimatedBpm = 60.0 / period;
    result.confidence = clampd (corr[(size_t) bestLag] * 4.0, 0.0, 1.0);
    if (result.confidence < raw.minimumConfidence) { result = {}; return result; }

    // Choose the strongest phase in one beat period, then track each subsequent
    // beat near the predicted location. This rejects most off-beat hi-hat peaks.
    const int lag = std::max (1, (int) std::lround (refinedLag));
    int bestPhase = 0; double phaseScore = -1.0;
    const int phaseLimit = std::min (lag, (int) env.size());
    for (int phase = 0; phase < phaseLimit; ++phase)
    {
        double score = 0.0;
        for (size_t i = (size_t) phase; i < env.size(); i += (size_t) lag) score += env[i];
        if (score > phaseScore) { phaseScore = score; bestPhase = phase; }
    }

    const double hopSeconds = 0.010;
    const int search = std::max (1, (int) std::lround (lag * 0.22));
    int frame = bestPhase;
    while (frame < (int) env.size())
    {
        int best = frame;
        double bestScore = -1.0;
        for (int d = -search; d <= search; ++d)
        {
            const int candidate = frame + d;
            if (candidate < 0 || candidate >= (int) env.size()) continue;
            const double score = env[(size_t) candidate] - 0.12 * std::abs (d) * (sd / 4.0);
            if (score > bestScore) { bestScore = score; best = candidate; }
        }
        const double sec = best * hopSeconds;
        if (sec > 0.0 && sec < (double) audio.numSamples() / audio.sampleRate)
            result.anchors.push_back ({ sec, sec * result.estimatedBpm / 60.0, clampd (bestScore / (sd + 1.0e-9), 0.0, 1.0) });
        frame = best + lag;
    }
    if (result.anchors.size() < 2) { result = {}; return result; }
    return result;
}

TempoMap buildTempoMap (const std::vector<BeatAnchor>& input, double duration, const TempoMapSettings& raw)
{
    TempoMap result;
    if (!(duration > 0.0) || input.size() < 2) return result;
    const double grid = raw.gridBeats > 1.0e-6 ? raw.gridBeats : 0.25;
    const double strength = clampd (raw.quantize, 0.0, 1.0);
    const double minBpm = clampd (raw.minBpm, 20.0, 400.0);
    const double maxBpm = clampd (raw.maxBpm, minBpm + 1.0, 600.0);
    const double minInterval = std::max (0.005, raw.minimumIntervalSeconds);

    std::vector<BeatAnchor> a;
    for (const auto& p : input)
        if (std::isfinite (p.seconds) && std::isfinite (p.beat) && p.seconds >= 0.0 && p.seconds < duration)
            a.push_back (p);
    std::stable_sort (a.begin(), a.end(), [] (const BeatAnchor& x, const BeatAnchor& y) { return x.seconds < y.seconds; });
    std::vector<BeatAnchor> clean;
    for (const auto& p : a)
    {
        if (!clean.empty() && p.seconds - clean.back().seconds < minInterval) continue;
        clean.push_back (p);
    }
    if (clean.size() < 2) return result;

    // The first anchor is the timeline origin. Quantization blends detected beat
    // positions toward the selected musical grid; at 100%, each beat is exact.
    for (auto& p : clean)
    {
        const double snapped = std::round (p.beat / grid) * grid;
        p.beat += (snapped - p.beat) * strength;
    }
    if (clean.front().seconds > 1.0e-6)
        clean.insert (clean.begin(), { 0.0, 0.0, 0.0 });
    if (clean.back().seconds < duration - 1.0e-6)
    {
        const size_t n = clean.size();
        const double lastDt = clean[n - 1].seconds - clean[n - 2].seconds;
        const double lastDb = clean[n - 1].beat - clean[n - 2].beat;
        const double endBeat = clean.back().beat + std::max (0.01, lastDb) * (duration - clean.back().seconds) / std::max (0.01, lastDt);
        clean.push_back ({ duration, endBeat, 0.0 });
    }

    const double minBeatGap = minInterval * minBpm / 60.0;
    for (size_t i = 1; i < clean.size(); ++i)
        clean[i].beat = std::max (clean[i].beat, clean[i - 1].beat + minBeatGap);
    for (size_t i = clean.size() - 1; i > 0; --i)
        clean[i - 1].beat = std::min (clean[i - 1].beat, clean[i].beat - minBeatGap);

    std::vector<double> bpms;
    for (size_t i = 0; i + 1 < clean.size(); ++i)
    {
        const double dt = clean[i + 1].seconds - clean[i].seconds;
        const double db = clean[i + 1].beat - clean[i].beat;
        if (dt <= 0.0 || db <= 0.0) continue;
        bpms.push_back (clampd (60.0 * db / dt, minBpm, maxBpm));
    }
    const double smooth = clampd (raw.smoothing, 0.0, 1.0);
    for (size_t i = 0; i < bpms.size(); ++i)
    {
        const double prev = bpms[i > 0 ? i - 1 : i];
        const double next = bpms[std::min (bpms.size() - 1, i + 1)];
        const double local = (prev + 2.0 * bpms[i] + next) * 0.25;
        bpms[i] = bpms[i] * (1.0 - smooth) + local * smooth;
    }
    for (size_t i = 0; i < bpms.size(); ++i)
        result.points.push_back ({ clean[i].beat, clean[i].seconds, bpms[i] });
    result.points.push_back ({ clean.back().beat, clean.back().seconds, bpms.back() });
    return result;
}

bool writeTempoMapMidi (const TempoMap& map, const std::string& path, int ppq)
{
    if (!map.valid() || ppq <= 0 || ppq > 32767) return false;
    struct Event { std::uint64_t tick; std::vector<unsigned char> bytes; };
    std::vector<Event> events;
    for (const auto& p : map.points)
    {
        if (!std::isfinite (p.beat) || !std::isfinite (p.bpm) || p.bpm <= 0.0) continue;
        const auto tick = (std::uint64_t) std::llround (std::max (0.0, p.beat) * ppq);
        const auto us = (std::uint32_t) std::llround (60000000.0 / clampd (p.bpm, 1.0, 1000.0));
        Event e { tick, { 0xff, 0x51, 0x03, (unsigned char) (us >> 16), (unsigned char) (us >> 8), (unsigned char) us } };
        events.push_back (std::move (e));
    }
    if (events.empty()) return false;
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.tick < b.tick; });
    std::vector<unsigned char> track;
    std::uint64_t previous = 0;
    for (const auto& e : events)
    {
        appendVlq (track, (std::uint32_t) std::min<std::uint64_t> (e.tick - previous, 0x0fffffff));
        track.insert (track.end(), e.bytes.begin(), e.bytes.end()); previous = e.tick;
    }
    track.insert (track.end(), { 0x00, 0xff, 0x2f, 0x00 });
    std::vector<unsigned char> file;
    file.insert (file.end(), { 'M', 'T', 'h', 'd' }); appendU32 (file, 6); appendU16 (file, 0); appendU16 (file, 1); appendU16 (file, (unsigned) ppq);
    file.insert (file.end(), { 'M', 'T', 'r', 'k' }); appendU32 (file, (std::uint32_t) track.size()); file.insert (file.end(), track.begin(), track.end());
    FILE* f = std::fopen (path.c_str(), "wb"); if (!f) return false;
    const bool ok = std::fwrite (file.data(), 1, file.size(), f) == file.size();
    return std::fclose (f) == 0 && ok;
}
}
