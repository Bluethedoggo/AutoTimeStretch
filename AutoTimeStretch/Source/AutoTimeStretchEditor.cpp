#include "AutoTimeStretchEditor.h"

AutoTimeStretchAudioProcessorEditor::AutoTimeStretchAudioProcessorEditor (AutoTimeStretchAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (720, 360);
    auto style = [this] (juce::TextButton& b) { addAndMakeVisible (b); b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b1e27)); b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8ecf3)); };
    for (auto* b : { &loadButton, &detectButton, &renderButton, &previewButton, &exportButton }) style (*b);
    loadButton.onClick = [this] { load(); };
    detectButton.onClick = [this] { if (!processor.detectSourceTempo()) info.setText ("Tempo detection failed", juce::dontSendNotification); };
    renderButton.onClick = [this] { render(); };
    previewButton.onClick = [this] { processor.setPreview (!processor.isPreviewing()); };
    exportButton.onClick = [this] { exportFile(); };
    auto setup = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& a)
    {
        l.setText (name, juce::dontSendNotification); l.setColour (juce::Label::textColourId, juce::Colour (0xffaab3c5)); addAndMakeVisible (l);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 20); addAndMakeVisible (s);
        a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, id, s);
    };
    setup (sourceBpm, sourceLabel, "SOURCE BPM", "sourceBpm", sourceAttach);
    setup (targetBpm, targetLabel, "TARGET BPM", "targetBpm", targetAttach);
    setup (gain, gainLabel, "GAIN", "gain", gainAttach);
    preservePitch.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffe8ecf3)); addAndMakeVisible (preservePitch);
    pitchAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "preservePitch", preservePitch);
    info.setColour (juce::Label::textColourId, juce::Colour (0xffaab3c5)); addAndMakeVisible (info);
    startTimerHz (5);
}
AutoTimeStretchAudioProcessorEditor::~AutoTimeStretchAudioProcessorEditor() { stopTimer(); }
bool AutoTimeStretchAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}
void AutoTimeStretchAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg"))
        { if (!processor.loadFile (juce::File (f))) info.setText ("Could not load audio", juce::dontSendNotification); break; }
}
void AutoTimeStretchAudioProcessorEditor::load()
{
    chooser = std::make_unique<juce::FileChooser> ("Load full song", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& c)
    { const auto f = c.getResult(); if (f.existsAsFile() && !processor.loadFile (f)) info.setText ("Could not load audio", juce::dontSendNotification); });
}
void AutoTimeStretchAudioProcessorEditor::render()
{ if (!processor.render()) info.setText ("Stretch failed. Check source and target BPM.", juce::dontSendNotification); }
void AutoTimeStretchAudioProcessorEditor::exportFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Export stretched audio", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (processor.getFileName() + "_stretched.wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& c)
    { const auto f = c.getResult(); if (f != juce::File() && !processor.exportWav (f.withFileExtension ("wav"))) info.setText ("Export failed", juce::dontSendNotification); });
}
void AutoTimeStretchAudioProcessorEditor::timerCallback()
{
    previewButton.setButtonText (processor.isPreviewing() ? "STOP" : "PREVIEW");
    info.setText (processor.hasSource() ? processor.getFileName() + (processor.hasRender() ? "  |  Render ready" : "  |  Original audio loaded") : "Drop/load a full song. No processing until STRETCH.", juce::dontSendNotification);
}
void AutoTimeStretchAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff12141a)); g.setColour (juce::Colour (0xff3fa9ff)); g.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
    g.drawText ("AUTOTIMESTRETCH", 20, 12, 430, 34, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff8a93a6)); g.setFont (13.0f); g.drawText ("DogSide Productions  -  Bluethedoggo", 22, 44, 400, 20, juce::Justification::centredLeft);
}
void AutoTimeStretchAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (18); auto top = r.removeFromTop (44); loadButton.setBounds (top.removeFromLeft (84).reduced (2)); detectButton.setBounds (top.removeFromLeft (112).reduced (2)); renderButton.setBounds (top.removeFromLeft (96).reduced (2)); previewButton.setBounds (top.removeFromLeft (92).reduced (2)); exportButton.setBounds (top.removeFromLeft (104).reduced (2));
    r.removeFromTop (14); auto controls = r.removeFromTop (190); auto w = controls.getWidth() / 3;
    auto a = controls.removeFromLeft (w); sourceLabel.setBounds (a.removeFromTop (20)); sourceBpm.setBounds (a.reduced (8));
    auto b = controls.removeFromLeft (w); targetLabel.setBounds (b.removeFromTop (20)); targetBpm.setBounds (b.reduced (8));
    auto c = controls; gainLabel.setBounds (c.removeFromTop (20)); gain.setBounds (c.reduced (8));
    preservePitch.setBounds (r.removeFromTop (28)); info.setBounds (r.removeFromTop (30));
}
