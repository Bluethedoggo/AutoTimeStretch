#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AutoTimeStretchProcessor.h"

class AutoTimeStretchAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit AutoTimeStretchAudioProcessorEditor (AutoTimeStretchAudioProcessor&);
    ~AutoTimeStretchAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
private:
    void timerCallback() override;
    void load(); void render(); void exportFile();
    AutoTimeStretchAudioProcessor& processor;
    juce::TextButton loadButton { "LOAD" }, detectButton { "DETECT BPM" }, renderButton { "STRETCH" }, previewButton { "PREVIEW" }, exportButton { "EXPORT WAV" };
    juce::Slider sourceBpm, targetBpm, gain;
    juce::ToggleButton preservePitch { "Preserve pitch" };
    juce::Label sourceLabel, targetLabel, gainLabel, info;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sourceAttach, targetAttach, gainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pitchAttach;
    std::unique_ptr<juce::FileChooser> chooser;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoTimeStretchAudioProcessorEditor)
};
