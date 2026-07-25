#include "PerformancePage.h"
#include "CustomLookAndFeel.h"

namespace shredspace
{

static const char* getMacroParamId (shredspace::MacroId macroId)
{
    switch (macroId)
    {
        case shredspace::MacroId::TIME:       return shredspace::paramids::PERF_MACRO_TIME;
        case shredspace::MacroId::SPACE:      return shredspace::paramids::PERF_MACRO_SPACE;
        case shredspace::MacroId::DEFINITION: return shredspace::paramids::PERF_MACRO_DEFINITION;
        case shredspace::MacroId::BLOOM:      return shredspace::paramids::PERF_MACRO_BLOOM;
        case shredspace::MacroId::TAIL:       return shredspace::paramids::PERF_MACRO_TAIL;
        case shredspace::MacroId::FOCUS:      return shredspace::paramids::PERF_MACRO_FOCUS;
        case shredspace::MacroId::WIDTH:      return shredspace::paramids::PERF_MACRO_WIDTH;
        case shredspace::MacroId::CHARACTER:  return shredspace::paramids::PERF_MACRO_CHARACTER;
    }
    return nullptr;
}

PerformancePage::PerformancePage (juce::AudioProcessorValueTreeState& apvtsToUse,
                                  shredspace::MacroEngine& macroEngineToUse,
                                  shredspace::SceneManager& sceneManagerToUse,
                                  shredspace::ProfileManager& profileManagerToUse,
                                  shredspace::TelemetryState& telemetryStateToUse)
    : apvts (apvtsToUse),
      macroEngine (macroEngineToUse),
      sceneManager (sceneManagerToUse),
      profileManager (profileManagerToUse),
      meterComponent (telemetryStateToUse)
{
    // 1. Profile Browser
    profileLabel.setText ("Profile", juce::dontSendNotification);
    profileLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (profileLabel);

    if (profileManager.getProfiles().empty())
        profileManager.loadBundledProfiles();

    profileComboBox.clear();
    for (const auto& profile : profileManager.getProfiles())
        profileComboBox.addItem (profile.profile_name, profileComboBox.getNumItems() + 1);

    profileComboBox.onChange = [this]() {
        const auto selectedIndex = profileComboBox.getSelectedItemIndex();
        if (selectedIndex >= 0)
        {
            const auto name = profileComboBox.getItemText (selectedIndex);
            if (profileManager.recallProfile (name, apvts))
            {
                if (const auto* doc = profileManager.getProfile (name))
                {
                    // Update MacroEngine tables
                    for (int m = 0; m < 8; ++m)
                    {
                        const auto mId = static_cast<shredspace::MacroId> (m);
                        const auto macroName = shredspace::kMacroNames[m];
                        auto it = doc->macro_mapping.find (macroName);
                        if (it != doc->macro_mapping.end())
                            macroEngine.setDestinationTable (mId, std::span<const shredspace::MacroDestination> (it->second.data(), it->second.size()));
                        else
                            macroEngine.setDestinationTable (mId, {});
                    }

                    // Update SceneManager values
                    std::array<shredspace::SceneValues, 4> allSceneValues;
                    const auto sceneCount = std::min (doc->scene_values.size(), size_t (4));
                    for (size_t i = 0; i < 4; ++i)
                    {
                        shredspace::SceneValues sv;
                        if (i < sceneCount)
                        {
                            const auto& srcScene = doc->scene_values[i];
                            for (int m = 0; m < 8; ++m)
                            {
                                auto it = srcScene.macro_targets.find (shredspace::kMacroNames[m]);
                                if (it != srcScene.macro_targets.end())
                                    sv.macroTargets[m] = it->second;
                            }
                            sv.mixTarget = srcScene.mix_target.value;
                            sv.routingTarget = juce::roundToInt (srcScene.routing_target.value);
                            sv.holdBehaviour = static_cast<int> (srcScene.hold_behavior);
                        }
                        allSceneValues[i] = sv;
                    }
                    sceneManager.setSceneValues (allSceneValues);
                }
            }
        }
    };
    addAndMakeVisible (profileComboBox);

    // 2. Scene Buttons
    sceneLabel.setText ("Scene Select", juce::dontSendNotification);
    sceneLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (sceneLabel);

    for (int i = 0; i < 4; ++i)
    {
        auto& btn = sceneButtons[i];
        btn.setButtonText (juce::String (i + 1));
        btn.setClickingTogglesState (true);
        btn.setRadioGroupId (1001);
        addAndMakeVisible (btn);

        btn.onClick = [this, i]() {
            if (auto* param = apvts.getParameter (shredspace::paramids::SCENE_SELECT))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (static_cast<float> (i)));
                param->endChangeGesture();
            }
        };
    }

    if (auto* param = apvts.getParameter (shredspace::paramids::SCENE_SELECT))
    {
        const int initialScene = juce::roundToInt (param->getNormalisableRange().convertFrom0to1 (param->getValue()));
        if (initialScene >= 0 && initialScene < 4)
            sceneButtons[initialScene].setToggleState (true, juce::dontSendNotification);
    }

    // 3. Macro Controls
    const juce::String macroNames[] = { "Time", "Space", "Definition", "Bloom", "Tail", "Focus", "Width", "Character" };
    for (int i = 0; i < 8; ++i)
    {
        const auto macroId = static_cast<shredspace::MacroId> (i);
        const auto& paramId = getMacroParamId (macroId);

        auto& slider = macroSliders[i];
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 65, 16);
        addAndMakeVisible (slider);

        auto& label = macroLabels[i];
        label.setText (macroNames[i], juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);

        // Attachment
        macroAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);

        // Sync with MacroEngine initially
        if (auto* param = apvts.getParameter (paramId))
            macroEngine.setMacroValue (macroId, param->getValue());

        // Handle value changes (including automated ones)
        slider.onValueChange = [this, macroId, paramId]() {
            if (auto* param = apvts.getParameter (paramId))
                macroEngine.setMacroValue (macroId, param->getValue());
        };
    }

    // 4. Mix Control
    mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    addAndMakeVisible (mixSlider);
    
    mixLabel.setText ("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (mixLabel);
    
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, shredspace::paramids::GLOBAL_MIX, mixSlider);

    // 5. Input Gain
    inputGainSlider.setSliderStyle (juce::Slider::LinearVertical);
    inputGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    addAndMakeVisible (inputGainSlider);
    
    inputGainLabel.setText ("Input", juce::dontSendNotification);
    inputGainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (inputGainLabel);
    
    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, shredspace::paramids::GLOBAL_INPUT_GAIN, inputGainSlider);

    // 6. Output Gain
    outputGainSlider.setSliderStyle (juce::Slider::LinearVertical);
    outputGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    addAndMakeVisible (outputGainSlider);
    
    outputGainLabel.setText ("Output", juce::dontSendNotification);
    outputGainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outputGainLabel);
    
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, shredspace::paramids::GLOBAL_OUTPUT_GAIN, outputGainSlider);

    // 7. Utility buttons
    bypassButton.setButtonText ("Bypass");
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, shredspace::paramids::GLOBAL_BYPASS, bypassButton);

    tapTempoButton.setButtonText ("Tap");
    addAndMakeVisible (tapTempoButton);
    tapTempoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, shredspace::paramids::PERF_TAP, tapTempoButton);

    holdButton.setButtonText ("Hold");
    addAndMakeVisible (holdButton);
    holdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, shredspace::paramids::PERF_HOLD, holdButton);

    // 8. Adaptive Mode
    adaptiveModeComboBox.clear();
    adaptiveModeComboBox.addItemList (juce::StringArray { "Off", "Level", "Articulation" }, 1);
    addAndMakeVisible (adaptiveModeComboBox);
    
    adaptiveModeLabel.setText ("Adaptive", juce::dontSendNotification);
    adaptiveModeLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (adaptiveModeLabel);
    
    adaptiveModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, shredspace::paramids::PERF_ADAPTIVE_MODE, adaptiveModeComboBox);

    // 9. Meter Component
    addAndMakeVisible (meterComponent);

    // 10. Register Parameter Listeners
    apvts.addParameterListener (shredspace::paramids::SCENE_SELECT, this);
    apvts.addParameterListener (shredspace::paramids::SCENE_MORPH, this);

    // Initialize SceneManager morph target
    {
        int sceneIndex = 0;
        if (auto* selectParam = apvts.getParameter (shredspace::paramids::SCENE_SELECT))
            sceneIndex = juce::roundToInt (selectParam->getNormalisableRange().convertFrom0to1 (selectParam->getValue()));
        
        float morphVal = 0.0f;
        if (auto* morphParam = apvts.getParameter (shredspace::paramids::SCENE_MORPH))
            morphVal = morphParam->getNormalisableRange().convertFrom0to1 (morphParam->getValue());
            
        sceneManager.setMorphPosition (sceneIndex, morphVal);
    }
}

PerformancePage::~PerformancePage()
{
    apvts.removeParameterListener (shredspace::paramids::SCENE_SELECT, this);
    apvts.removeParameterListener (shredspace::paramids::SCENE_MORPH, this);
}

void PerformancePage::paint (juce::Graphics& g)
{
    g.fillAll (CustomLookAndFeel::shredDarkBg);

    // Divider line between header and content
    g.setColour (CustomLookAndFeel::shredOutline);
    g.drawHorizontalLine (55, 0.0f, static_cast<float> (getWidth()));
}

void PerformancePage::resized()
{
    auto bounds = getLocalBounds().reduced (15);

    // 1. Header Row (Proportional layout)
    auto headerArea = bounds.removeFromTop (40);
    const float totalHeaderWidth = static_cast<float> (headerArea.getWidth());

    // Profile Browser (22% of header width)
    auto profileArea = headerArea.removeFromLeft (static_cast<int> (totalHeaderWidth * 0.22f));
    profileLabel.setBounds (profileArea.removeFromLeft (static_cast<int> (profileArea.getWidth() * 0.35f)));
    profileComboBox.setBounds (profileArea.reduced (2));

    // Scene Select (25% of header width)
    auto sceneArea = headerArea.removeFromLeft (static_cast<int> (totalHeaderWidth * 0.25f));
    sceneLabel.setBounds (sceneArea.removeFromLeft (static_cast<int> (sceneArea.getWidth() * 0.40f)));
    const auto sceneBtnGroupWidth = sceneArea.getWidth();
    const auto sceneBtnWidth = sceneBtnGroupWidth / 4;
    for (int i = 0; i < 4; ++i)
        sceneButtons[i].setBounds (sceneArea.removeFromLeft (sceneBtnWidth).reduced (1, 0));

    // Adaptive Mode (23% of header width)
    auto adaptiveArea = headerArea.removeFromLeft (static_cast<int> (totalHeaderWidth * 0.23f));
    adaptiveModeLabel.setBounds (adaptiveArea.removeFromLeft (static_cast<int> (adaptiveArea.getWidth() * 0.40f)));
    adaptiveModeComboBox.setBounds (adaptiveArea.reduced (2));

    // Utility Buttons (Remaining 30% of header width: Tap, Hold, Bypass)
    auto utilsArea = headerArea;
    const auto utilBtnWidth = utilsArea.getWidth() / 3;
    tapTempoButton.setBounds (utilsArea.removeFromLeft (utilBtnWidth).reduced (2));
    holdButton.setBounds (utilsArea.removeFromLeft (utilBtnWidth).reduced (2));
    bypassButton.setBounds (utilsArea.reduced (2));

    bounds.removeFromTop (15); // visual separation spacing

    // 2. Main Content Split (Left/Right)
    const auto totalWidth = bounds.getWidth();
    const auto leftWidth = static_cast<int> (static_cast<float> (totalWidth) * 0.65f);
    auto leftArea = bounds.removeFromLeft (leftWidth);
    auto rightArea = bounds;

    // Macro dials in 2x4 grid
    const auto rowHeight = leftArea.getHeight() / 2;
    auto row1 = leftArea.removeFromTop (rowHeight);
    auto row2 = leftArea;

    const auto colWidth = row1.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto cell = row1.removeFromLeft (colWidth).reduced (4);
        macroLabels[i].setBounds (cell.removeFromBottom (18));
        macroSliders[i].setBounds (cell);
    }
    for (int i = 4; i < 8; ++i)
    {
        auto cell = row2.removeFromLeft (colWidth).reduced (4);
        macroLabels[i].setBounds (cell.removeFromBottom (18));
        macroSliders[i].setBounds (cell);
    }

    // Right Area: Global controls + MeterComponent
    const auto controlsWidth = rightArea.getWidth() / 2;
    auto controlsArea = rightArea.removeFromLeft (controlsWidth);
    auto metersArea = rightArea;

    // Global columns (Input, Mix, Output)
    const auto rightColWidth = controlsArea.getWidth() / 3;
    
    auto inputCell = controlsArea.removeFromLeft (rightColWidth).reduced (2);
    inputGainLabel.setBounds (inputCell.removeFromBottom (18));
    inputGainSlider.setBounds (inputCell);

    auto mixCell = controlsArea.removeFromLeft (rightColWidth).reduced (2);
    mixLabel.setBounds (mixCell.removeFromBottom (18));
    mixSlider.setBounds (mixCell);

    auto outputCell = controlsArea.reduced (2);
    outputGainLabel.setBounds (outputCell.removeFromBottom (18));
    outputGainSlider.setBounds (outputCell);

    // MeterComponent
    meterComponent.setBounds (metersArea.reduced (2));
}

void PerformancePage::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == shredspace::paramids::SCENE_SELECT)
    {
        const int sceneIndex = juce::roundToInt (newValue);
        juce::MessageManager::callAsync ([this, sceneIndex]() {
            for (int i = 0; i < 4; ++i)
                sceneButtons[i].setToggleState (i == sceneIndex, juce::dontSendNotification);
        });

        // Update SceneManager
        float morphVal = 0.0f;
        if (auto* morphParam = apvts.getParameter (shredspace::paramids::SCENE_MORPH))
            morphVal = morphParam->getValue() * 100.0f;
        sceneManager.setMorphPosition (sceneIndex, morphVal);
    }
    else if (parameterID == shredspace::paramids::SCENE_MORPH)
    {
        int sceneIndex = 0;
        if (auto* selectParam = apvts.getParameter (shredspace::paramids::SCENE_SELECT))
            sceneIndex = juce::roundToInt (selectParam->getNormalisableRange().convertFrom0to1 (selectParam->getValue()));
        
        sceneManager.setMorphPosition (sceneIndex, newValue);
    }
}

} // namespace shredspace
