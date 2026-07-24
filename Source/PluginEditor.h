#pragma once

#include "PluginProcessor.h"

class ShredSpaceAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  explicit ShredSpaceAudioProcessorEditor(ShredSpaceAudioProcessor &);
  ~ShredSpaceAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  ShredSpaceAudioProcessor &audioProcessor;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShredSpaceAudioProcessorEditor)
};
