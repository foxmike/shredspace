#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace shredspace
{

/**
 * Creates the complete AudioProcessorValueTreeState parameter layout.
 * Calls each helper function in sequence to build the layout.
 */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Individual parameter group helpers
void addGlobalParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
void addPerformanceParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
void addTimingParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
void addStereoParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
void addSceneParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

// Note: The voice and intelligence helpers are declared here but will be
// fully implemented in subsequent tasks (b.3 and b.4).
void addVoiceParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
void addIntelligenceParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

} // namespace shredspace
