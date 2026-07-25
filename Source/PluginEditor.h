#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "GUI/CustomLookAndFeel.h"
#include "Parameters/MacroEngine.h"
#include "Profiles/SceneManager.h"
#include "Profiles/ProfileManager.h"
#include "GUI/PerformancePage.h"
#include "GUI/VoicesPage.h"
#include "GUI/ToneMotionPage.h"
#include "GUI/IntelligencePage.h"
#include <memory>

class ShredSpaceAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit ShredSpaceAudioProcessorEditor(ShredSpaceAudioProcessor &);
    ~ShredSpaceAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

private:
    void showPage(juce::Component* pageToShow);

    ShredSpaceAudioProcessor &audioProcessor;

    CustomLookAndFeel lookAndFeel;

    // GUI/Telemetry state managers
    shredspace::MacroEngine macroEngine;
    shredspace::SceneManager sceneManager;
    shredspace::ProfileManager profileManager;

    // Owned page instances
    std::unique_ptr<shredspace::PerformancePage> performancePage;
    std::unique_ptr<shredspace::VoicesPage> voicesPage;
    std::unique_ptr<shredspace::ToneMotionPage> toneMotionPage;
    std::unique_ptr<shredspace::IntelligencePage> intelligencePage;

    // Navigation bar tabs
    juce::TextButton performanceTab { "Performance" };
    juce::TextButton voicesTab { "Voices" };
    juce::TextButton toneMotionTab { "Tone & Motion" };
    juce::TextButton intelligenceTab { "Intelligence" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShredSpaceAudioProcessorEditor)
};
