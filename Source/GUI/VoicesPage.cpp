#include "VoicesPage.h"
#include "CustomLookAndFeel.h"

namespace shredspace
{

VoicesPage::VoicePanel::VoicePanel (juce::AudioProcessorValueTreeState& apvtsToUse,
                                    const VoiceParamIDs& ids,
                                    const juce::String& title)
    : apvts (apvtsToUse),
      voiceTitle (title),
      enableParamId (ids.enableId)
{
    // Enable button setup
    enableButton.setButtonText ("Enabled");
    addAndMakeVisible (enableButton);
    enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, enableParamId, enableButton);

    // List of labels matching the slider IDs in VoiceParamIDs
    static const std::array<juce::String, NumSliders> sliderLabels = {
        "Time Ratio",
        "Fine Offset",
        "Level",
        "Tail Scale",
        "Pan",
        "Width",
        "Stereo Offset",
        "Cross Feedback",
        "HPF",
        "LPF",
        "Presence Freq",
        "Presence Guard",
        "Attack Soften",
        "Mod Depth",
        "Mod Rate",
        "Mod Phase",
        "Drift",
        "Diffusion",
        "Saturation",
        "Adaptive Role"
    };

    for (size_t i = 0; i < NumSliders; ++i)
    {
        auto& item = sliders[i];
        
        // Setup slider
        item.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        item.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 14);
        addAndMakeVisible (item.slider);
        
        // Setup label
        item.label.setText (sliderLabels[i], juce::dontSendNotification);
        item.label.setJustificationType (juce::Justification::centred);
        item.label.setFont (juce::FontOptions (10.0f));
        addAndMakeVisible (item.label);
        
        // Setup attachment
        item.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ids.sliderIds[i], item.slider);
    }

    // Set initial enablement state
    if (auto* param = apvts.getParameter (enableParamId))
    {
        const bool enabled = (param->getValue() > 0.5f);
        for (auto& s : sliders)
            s.slider.setEnabled (enabled);
    }

    // Register listener
    apvts.addParameterListener (enableParamId, this);
}

VoicesPage::VoicePanel::~VoicePanel()
{
    apvts.removeParameterListener (enableParamId, this);
}

void VoicesPage::VoicePanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw a subtle dark card background
    g.setColour (CustomLookAndFeel::shredWidgetBg);
    g.fillRoundedRectangle (bounds, 6.0f);
    
    // Draw a subtle border outline
    g.setColour (CustomLookAndFeel::shredOutline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.5f);
    
    // Draw a header bar background for the title
    auto headerBar = bounds.removeFromTop (28.0f);
    g.setColour (CustomLookAndFeel::shredOutline.withAlpha (0.3f));
    g.fillRoundedRectangle (headerBar, 6.0f);
    
    // Draw the title text in the top-left
    g.setColour (CustomLookAndFeel::shredHighlightText);
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    g.drawText (voiceTitle, 12, 0, 120, 28, juce::Justification::centredLeft, true);
}

void VoicesPage::VoicePanel::resized()
{
    auto bounds = getLocalBounds();
    
    // We reduce the area inside the group component border
    auto contentArea = bounds.reduced (12);
    
    // Header area for the enable button (aligns with the title bar painted in paint())
    auto headerArea = bounds.removeFromTop (28).reduced (12, 0);
    enableButton.setBounds (headerArea.removeFromRight (80).reduced (0, 2));

    // The remaining contentArea will hold the 5x4 grid of sliders
    // Give some spacing from the header bar
    contentArea.removeFromTop (24);

    const int numRows = 5;
    const int numCols = 4;
    const int rowHeight = contentArea.getHeight() / numRows;
    const int colWidth = contentArea.getWidth() / numCols;

    for (int row = 0; row < numRows; ++row)
    {
        auto rowArea = contentArea.removeFromTop (rowHeight);
        for (int col = 0; col < numCols; ++col)
        {
            auto cell = rowArea.removeFromLeft (colWidth).reduced (4);
            const int idx = row * numCols + col;
            if (idx < NumSliders)
            {
                const size_t uIdx = static_cast<size_t> (idx);
                sliders[uIdx].label.setBounds (cell.removeFromBottom (14));
                sliders[uIdx].slider.setBounds (cell);
            }
        }
    }
}

void VoicesPage::VoicePanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == enableParamId)
    {
        const bool enabled = (newValue > 0.5f);
        juce::MessageManager::callAsync ([this, enabled]() {
            for (auto& s : sliders)
                s.slider.setEnabled (enabled);
        });
    }
}

VoicesPage::VoicesPage (juce::AudioProcessorValueTreeState& apvtsToUse,
                        shredspace::TelemetryState& telemetryStateToUse)
    : apvts (apvtsToUse),
      telemetryState (telemetryStateToUse),
      panelA (apvtsToUse,
              VoiceParamIDs {
                  shredspace::paramids::VOICE_A_ENABLE,
                  {
                      shredspace::paramids::VOICE_A_TIME_RATIO,
                      shredspace::paramids::VOICE_A_FINE_OFFSET,
                      shredspace::paramids::VOICE_A_LEVEL,
                      shredspace::paramids::VOICE_A_TAIL_SCALE,
                      shredspace::paramids::VOICE_A_PAN,
                      shredspace::paramids::VOICE_A_WIDTH,
                      shredspace::paramids::VOICE_A_STEREO_OFFSET,
                      shredspace::paramids::VOICE_A_CROSS_FEEDBACK,
                      shredspace::paramids::VOICE_A_HPF,
                      shredspace::paramids::VOICE_A_LPF,
                      shredspace::paramids::VOICE_A_PRESENCE_FREQ,
                      shredspace::paramids::VOICE_A_PRESENCE_GUARD,
                      shredspace::paramids::VOICE_A_ATTACK_SOFTEN,
                      shredspace::paramids::VOICE_A_MOD_DEPTH,
                      shredspace::paramids::VOICE_A_MOD_RATE,
                      shredspace::paramids::VOICE_A_MOD_PHASE,
                      shredspace::paramids::VOICE_A_DRIFT,
                      shredspace::paramids::VOICE_A_DIFFUSION,
                      shredspace::paramids::VOICE_A_SATURATION,
                      shredspace::paramids::VOICE_A_ADAPTIVE_ROLE
                  }
              },
              "Voice A"),
      panelB (apvtsToUse,
              VoiceParamIDs {
                  shredspace::paramids::VOICE_B_ENABLE,
                  {
                      shredspace::paramids::VOICE_B_TIME_RATIO,
                      shredspace::paramids::VOICE_B_FINE_OFFSET,
                      shredspace::paramids::VOICE_B_LEVEL,
                      shredspace::paramids::VOICE_B_TAIL_SCALE,
                      shredspace::paramids::VOICE_B_PAN,
                      shredspace::paramids::VOICE_B_WIDTH,
                      shredspace::paramids::VOICE_B_STEREO_OFFSET,
                      shredspace::paramids::VOICE_B_CROSS_FEEDBACK,
                      shredspace::paramids::VOICE_B_HPF,
                      shredspace::paramids::VOICE_B_LPF,
                      shredspace::paramids::VOICE_B_PRESENCE_FREQ,
                      shredspace::paramids::VOICE_B_PRESENCE_GUARD,
                      shredspace::paramids::VOICE_B_ATTACK_SOFTEN,
                      shredspace::paramids::VOICE_B_MOD_DEPTH,
                      shredspace::paramids::VOICE_B_MOD_RATE,
                      shredspace::paramids::VOICE_B_MOD_PHASE,
                      shredspace::paramids::VOICE_B_DRIFT,
                      shredspace::paramids::VOICE_B_DIFFUSION,
                      shredspace::paramids::VOICE_B_SATURATION,
                      shredspace::paramids::VOICE_B_ADAPTIVE_ROLE
                  }
              },
              "Voice B")
{
    addAndMakeVisible (panelA);
    addAndMakeVisible (panelB);
}

VoicesPage::~VoicesPage() = default;

void VoicesPage::paint (juce::Graphics& g)
{
    g.fillAll (CustomLookAndFeel::shredDarkBg);
}

void VoicesPage::resized()
{
    auto bounds = getLocalBounds().reduced (15);
    
    // We split the area horizontally for Voice A and Voice B with a 15px gap
    const int totalWidth = bounds.getWidth();
    const int panelWidth = (totalWidth - 15) / 2;
    
    panelA.setBounds (bounds.removeFromLeft (panelWidth));
    bounds.removeFromLeft (15); // Gap
    panelB.setBounds (bounds.removeFromLeft (panelWidth));
}

} // namespace shredspace
