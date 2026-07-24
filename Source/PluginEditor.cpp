#include "PluginEditor.h"
#include "PluginProcessor.h"

ShredSpaceAudioProcessorEditor::ShredSpaceAudioProcessorEditor(
    ShredSpaceAudioProcessor &p)
    : juce::AudioProcessorEditor(&p), audioProcessor(p) {
  setResizable(true, true);
  setResizeLimits(600, 400, 2400, 1600);
  setSize(900, 600);
}

ShredSpaceAudioProcessorEditor::~ShredSpaceAudioProcessorEditor() = default;

void ShredSpaceAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  g.setColour(juce::Colours::white);
  g.setFont(24.0f);
  g.drawText("ShredSpace", getLocalBounds(), juce::Justification::centred,
             true);
}

void ShredSpaceAudioProcessorEditor::resized() {}
