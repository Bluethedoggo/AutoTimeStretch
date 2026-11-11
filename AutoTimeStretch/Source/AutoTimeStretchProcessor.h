#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "WarpEngine.h"
#include "TempoEngine.h"

class AutoTimeStretchAudioProcessor final : public juce::AudioProcessor
{
public:
    AutoTimeStretchAudioProcessor();
    ~AutoTimeStretchAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "AutoTimeStretch"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int size) override;

    bool loadFile (const juce::File& file);
    bool render();
    bool exportWav (const juce::File& file) const;
    bool detectSourceTempo();
    void setPreview (bool enabled) noexcept { preview.store (enabled); if (enabled) previewStart.store (true); }
    bool isPreviewing() const noexcept { return preview.load(); }
    juce::String getFileName() const { return fileName; }
    double getDetectedBpm() const noexcept { return detectedBpm.load(); }
    bool hasSource() const noexcept { return source != nullptr; }
    bool hasRender() const noexcept { return std::atomic_load (&rendered) != nullptr; }
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioFormatManager formats;
    std::shared_ptr<dogside::AudioData> source;
    std::shared_ptr<dogside::RenderResult> rendered;
    juce::String fileName;
    std::atomic<double> detectedBpm { 0.0 };
    std::atomic<bool> preview { false }, previewStart { false };
    double sampleRate = 44100.0;
    double playbackSeconds = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoTimeStretchAudioProcessor)
};
