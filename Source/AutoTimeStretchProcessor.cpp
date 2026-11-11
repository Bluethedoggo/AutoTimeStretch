
#include "AutoTimeStretchProcessor.h"
#include "AutoTimeStretchEditor.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace
{
    constexpr juce::int32 kTimeStretchAudioMagic = 0x31535441;
}

juce::AudioProcessorValueTreeState::ParameterLayout AutoTimeStretchAudioProcessor::createLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout p;

    p.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "sourceBpm", 1 }, "Source BPM",
        NormalisableRange<float> (20.0f, 400.0f, 0.01f), 120.0f));

    p.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "targetBpm", 1 }, "Target BPM",
        NormalisableRange<float> (20.0f, 400.0f, 0.01f), 120.0f));

    p.add (std::make_unique<AudioParameterBool> (
        ParameterID { "preservePitch", 1 }, "Preserve Pitch", true));

    p.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "gain", 1 }, "Output Gain",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    return p;
}

AutoTimeStretchAudioProcessor::AutoTimeStretchAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AUTOTIMESTRETCH", createLayout())
{
    formats.registerBasicFormats();
}

bool AutoTimeStretchAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AutoTimeStretchAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    playbackSeconds = 0.0;
}

juce::AudioProcessorEditor* AutoTimeStretchAudioProcessor::createEditor()
{
    return new AutoTimeStretchAudioProcessorEditor (*this);
}

bool AutoTimeStretchAudioProcessor::loadFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (!reader
        || reader->lengthInSamples < 64
        || reader->sampleRate <= 0.0
        || reader->lengthInSamples > reader->sampleRate * 600.0)
        return false;

    const int channels = (int) juce::jmin (2u, reader->numChannels);
    const int count = (int) reader->lengthInSamples;

    juce::AudioBuffer<float> temp (channels, count);

    if (!reader->read (&temp, 0, count, 0, true, channels > 1))
        return false;

    auto data = std::make_shared<dogside::AudioData>();

    data->sampleRate = reader->sampleRate;
    data->ch.resize ((size_t) channels);

    for (int c = 0; c < channels; ++c)
    {
        data->ch[(size_t) c].assign (
            temp.getReadPointer (c),
            temp.getReadPointer (c) + count);
    }

    source = std::move (data);

    std::atomic_store (&rendered, std::shared_ptr<dogside::RenderResult>());

    fileName = file.getFileNameWithoutExtension();

    preview.store (false);
    previewStart.store (true);

    return true;
}

bool AutoTimeStretchAudioProcessor::detectSourceTempo()
{
    if (!source)
        return false;

    dogside::BeatAnalysisSettings s;
    const auto a = dogside::analyzeBeats (*source, s);

    if (!a.valid())
        return false;

    detectedBpm.store (a.estimatedBpm);

    if (auto* p = apvts.getParameter ("sourceBpm"))
    {
        p->setValueNotifyingHost (
            p->convertTo0to1 ((float) a.estimatedBpm));
    }

    return true;
}

bool AutoTimeStretchAudioProcessor::render()
{
    if (!source)
        return false;

    const double sourceBpm =
        apvts.getRawParameterValue ("sourceBpm")->load();

    const double targetBpm =
        apvts.getRawParameterValue ("targetBpm")->load();

    if (sourceBpm < 1.0 || targetBpm < 1.0)
        return false;

    const double duration =
        (double) source->numSamples() / source->sampleRate;

    const double beats = duration * sourceBpm / 60.0;

    if (beats < 1.0 || beats > 100000.0)
        return false;

    dogside::WarpPlan plan;

    plan.loopBeats = beats;
    plan.points.push_back ({ 0.0, 0.0, 0.0, false, -1 });
    plan.points.push_back ({ (double) source->numSamples(), beats, beats, false, -1 });

    auto result = std::make_shared<dogside::RenderResult>();

    const auto mode =
        apvts.getRawParameterValue ("preservePitch")->load() > 0.5f
            ? dogside::StretchMode::Stretch
            : dogside::StretchMode::RePitch;

    if (!dogside::renderWarp (*source, plan, sampleRate, targetBpm, mode, *result))
        return false;

    std::atomic_store (&rendered, std::move (result));

    preview.store (false);
    previewStart.store (true);

    return true;
}

bool AutoTimeStretchAudioProcessor::exportWav (const juce::File& file) const
{
    const auto result = std::atomic_load (&rendered);

    if (!result || result->numSamples() == 0 || result->ch.empty())
        return false;

    if (result->numSamples() > (size_t) std::numeric_limits<int>::max())
        return false;

    file.deleteFile();

    std::unique_ptr<juce::OutputStream> stream =
        std::make_unique<juce::FileOutputStream> (file);

    if (!stream || !stream->openedOk())
        return false;

    juce::WavAudioFormat wav;

    const auto options =
        juce::AudioFormatWriterOptions()
            .withSampleRate (result->sampleRate)
            .withNumChannels ((int) result->ch.size())
            .withBitsPerSample (24);

    auto writer = wav.createWriterFor (stream, options);

    if (!writer)
        return false;

    std::vector<const float*> channels;

    channels.reserve (result->ch.size());

    for (const auto& c : result->ch)
        channels.push_back (c.data());

    return writer->writeFromFloatArrays (
        channels.data(),
        (int) channels.size(),
        (int) result->numSamples());
}

void AutoTimeStretchAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    const auto result = std::atomic_load (&rendered);

    if (!result || !preview.load() || result->numSamples() == 0 || result->ch.empty())
        return;

    if (previewStart.exchange (false))
        playbackSeconds = 0.0;

    const double gain = juce::Decibels::decibelsToGain (
        apvts.getRawParameterValue ("gain")->load());

    const size_t total = result->numSamples();
    const size_t channelCount = result->ch.size();

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const double samplePosition = playbackSeconds * result->sampleRate;

        if (samplePosition < 0.0 || samplePosition >= (double) total)
        {
            preview.store (false);
            break;
        }

        const size_t pos = (size_t) samplePosition;

        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            const size_t sourceChannel =
                juce::jmin ((size_t) c, channelCount - 1);

            const auto& src = result->ch[sourceChannel];

            buffer.setSample (c, i, src[pos] * (float) gain);
        }

        playbackSeconds += 1.0 / sampleRate;
    }
}

void AutoTimeStretchAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("AUTOTIMESTRETCH_STATE");

    root.setProperty ("name", fileName, nullptr);
    root.appendChild (apvts.copyState(), nullptr);

    if (source && !source->ch.empty())
    {
        const size_t samples = source->numSamples();
        const size_t bytes = samples * source->ch.size() * sizeof (float);

        if (bytes <= 200u * 1024u * 1024u)
        {
            juce::MemoryBlock blob;
            juce::MemoryOutputStream stream (blob, false);

            stream.writeInt (kTimeStretchAudioMagic);
            stream.writeInt ((int) source->ch.size());
            stream.writeDouble (source->sampleRate);
            stream.writeInt64 ((juce::int64) samples);

            for (const auto& channel : source->ch)
                stream.write (channel.data(), samples * sizeof (float));

            root.setProperty ("audio", juce::var (blob), nullptr);
        }
    }

    juce::MemoryOutputStream out (dest, false);
    root.writeToStream (out);
}

void AutoTimeStretchAudioProcessor::setStateInformation (
    const void* data,
    int size)
{
    const auto root =
        juce::ValueTree::readFromData (data, (size_t) size);

    if (!root.isValid() || !root.hasType ("AUTOTIMESTRETCH_STATE"))
        return;

    const auto params = root.getChildWithName (apvts.state.getType());

    if (params.isValid())
        apvts.replaceState (params);

    if (auto* blob = root.getProperty ("audio").getBinaryData())
    {
        juce::MemoryInputStream in (*blob, false);

        if (in.readInt() == kTimeStretchAudioMagic)
        {
            const int channels = in.readInt();
            const double sr = in.readDouble();
            const auto count = (size_t) in.readInt64();

            if (channels >= 1
                && channels <= 2
                && sr > 0.0
                && count > 0
                && count <= (size_t) (sr * 600.0)
                && count <= blob->getSize() / sizeof (float))
            {
                auto dataAudio = std::make_shared<dogside::AudioData>();

                dataAudio->sampleRate = sr;
                dataAudio->ch.resize ((size_t) channels);

                bool valid = true;

                for (int c = 0; c < channels; ++c)
                {
                    dataAudio->ch[(size_t) c].resize (count);

                    const auto bytesRead = in.read (
                        dataAudio->ch[(size_t) c].data(),
                        count * sizeof (float));

                    if (bytesRead != (int) (count * sizeof (float)))
                    {
                        valid = false;
                        break;
                    }
                }

                if (valid)
                    source = std::move (dataAudio);
            }
        }
    }

    fileName = root.getProperty ("name").toString();

    std::atomic_store (&rendered, std::shared_ptr<dogside::RenderResult>());

    preview.store (false);
    previewStart.store (true);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoTimeStretchAudioProcessor();
}
