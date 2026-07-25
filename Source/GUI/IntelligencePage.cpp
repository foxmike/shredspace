#include "IntelligencePage.h"
#include "CustomLookAndFeel.h"
#include <cmath>

namespace shredspace
{

IntelligencePage::IntelligencePage (juce::AudioProcessorValueTreeState& apvtsToUse,
                                    shredspace::TelemetryState& telemetryStateToUse)
    : apvts (apvtsToUse),
      telemetryState (telemetryStateToUse),
      detectorDisplay (telemetryStateToUse)
{
    // Add and make visible the DetectorDisplay
    addAndMakeVisible (detectorDisplay);

    // --- Detector Settings ---
    setupControl (sensitivityControl, paramids::INTEL_SENSITIVITY, "Sensitivity", " %");
    setupControl (inputGateControl, paramids::INTEL_INPUT_GATE, "Input Gate", " dB");
    setupControl (densityWindowControl, paramids::INTEL_DENSITY_WINDOW, "Density Window", " ms");
    setupControl (densityStartControl, paramids::INTEL_DENSITY_START, "Density Start", " ev/s");
    setupControl (densityFullControl, paramids::INTEL_DENSITY_FULL, "Density Full", " ev/s");
    setupControl (sustainStartControl, paramids::INTEL_SUSTAIN_START, "Sustain Start", " ms");
    setupControl (sustainFullControl, paramids::INTEL_SUSTAIN_FULL, "Sustain Full", " ms");
    setupControl (phraseGapControl, paramids::INTEL_PHRASE_GAP, "Phrase Gap", " ms");
    setupControl (transientCrossoverControl, paramids::INTEL_TRANSIENT_CROSSOVER, "Crossover", " Hz");

    // --- Contraction Settings ---
    setupControl (contractionAttackControl, paramids::INTEL_CONTRACTION_ATTACK, "Attack", " ms");
    setupControl (contractionReleaseControl, paramids::INTEL_CONTRACTION_RELEASE, "Release", " ms");
    setupControl (wetReductionControl, paramids::INTEL_WET_REDUCTION, "Wet Reduction", " dB");
    setupControl (tailReductionControl, paramids::INTEL_TAIL_REDUCTION, "Tail Scale Red.", " oct");
    setupControl (toneDarkeningControl, paramids::INTEL_TONE_DARKENING, "Tone Darkening", " oct");
    setupControl (widthReductionControl, paramids::INTEL_WIDTH_REDUCTION, "Width Red.", " %");
    setupControl (attackSoftenIncreaseControl, paramids::INTEL_ATTACK_SOFTEN_INCREASE, "Att. Soften Inc.", " %");

    // --- Bloom Settings ---
    setupControl (bloomRiseControl, paramids::INTEL_BLOOM_RISE, "Bloom Rise", " ms");
    setupControl (bloomHoldControl, paramids::INTEL_BLOOM_HOLD, "Bloom Hold", " ms");
    setupControl (bloomReleaseControl, paramids::INTEL_BLOOM_RELEASE, "Bloom Release", " ms");
    setupControl (wetLiftControl, paramids::INTEL_WET_LIFT, "Wet Lift", " dB");
    setupControl (tailExpansionControl, paramids::INTEL_TAIL_EXPANSION, "Tail Scale Exp.", " oct");
    setupControl (toneOpeningControl, paramids::INTEL_TONE_OPENING, "Tone Opening", " oct");
    setupControl (widthExpansionControl, paramids::INTEL_WIDTH_EXPANSION, "Width Exp.", " %");
    setupControl (diffusionIncreaseControl, paramids::INTEL_DIFFUSION_INCREASE, "Diff. Increase", " %");
}

IntelligencePage::~IntelligencePage() = default;

void IntelligencePage::paint (juce::Graphics& g)
{
    g.fillAll (CustomLookAndFeel::shredDarkBg);

    // Calculate background cards
    auto bounds = getLocalBounds().reduced (15);
    bounds.removeFromTop (180); // Detector display height
    bounds.removeFromTop (15);  // Gap

    const int totalWidth = bounds.getWidth();
    const int colWidth = (totalWidth - 30) / 3;

    auto drawCard = [&g] (juce::Rectangle<int> cardBounds, const juce::String& title) {
        auto fBounds = cardBounds.toFloat();
        g.setColour (CustomLookAndFeel::shredWidgetBg);
        g.fillRoundedRectangle (fBounds, 6.0f);
        g.setColour (CustomLookAndFeel::shredOutline);
        g.drawRoundedRectangle (fBounds, 6.0f, 1.5f);

        auto header = fBounds.removeFromTop (28.0f);
        g.setColour (CustomLookAndFeel::shredOutline.withAlpha (0.3f));
        g.fillRoundedRectangle (header, 6.0f);

        g.setColour (CustomLookAndFeel::shredHighlightText);
        g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
        g.drawText (title, header.reduced (12.0f, 0.0f), juce::Justification::centredLeft, true);
    };

    auto detectorCardBounds = bounds.removeFromLeft (colWidth);
    bounds.removeFromLeft (15);
    auto contractionCardBounds = bounds.removeFromLeft (colWidth);
    bounds.removeFromLeft (15);
    auto bloomCardBounds = bounds;

    drawCard (detectorCardBounds, "DETECTOR CONFIGURATION");
    drawCard (contractionCardBounds, "CONTRACTION RESPONSE (DENSE INPUT)");
    drawCard (bloomCardBounds, "BLOOM RESPONSE (SPARSE INPUT)");
}

void IntelligencePage::resized()
{
    auto bounds = getLocalBounds().reduced (15);
    
    // Position detector display
    auto displayBounds = bounds.removeFromTop (180);
    detectorDisplay.setBounds (displayBounds);
    
    bounds.removeFromTop (15); // Gap

    const int totalWidth = bounds.getWidth();
    const int colWidth = (totalWidth - 30) / 3;

    auto detectorCardBounds = bounds.removeFromLeft (colWidth);
    bounds.removeFromLeft (15);
    auto contractionCardBounds = bounds.removeFromLeft (colWidth);
    bounds.removeFromLeft (15);
    auto bloomCardBounds = bounds;

    // --- Lay out Detector Settings ---
    {
        auto content = detectorCardBounds.reduced (12);
        content.removeFromTop (28); // Header height

        const int numRows = 3;
        const int numCols = 3;
        const int rowHeight = content.getHeight() / numRows;
        const int cellWidth = content.getWidth() / numCols;

        std::array<ParameterControl*, 9> detectorControls = {
            &sensitivityControl, &inputGateControl, &densityWindowControl,
            &densityStartControl, &densityFullControl, &phraseGapControl,
            &sustainStartControl, &sustainFullControl, &transientCrossoverControl
        };

        for (int row = 0; row < numRows; ++row)
        {
            auto rowArea = content.removeFromTop (rowHeight);
            for (int col = 0; col < numCols; ++col)
            {
                auto cell = rowArea.removeFromLeft (cellWidth).reduced (4);
                detectorControls[static_cast<size_t> (row * numCols + col)]->label.setBounds (cell.removeFromBottom (14));
                detectorControls[static_cast<size_t> (row * numCols + col)]->slider.setBounds (cell);
            }
        }
    }

    // --- Lay out Contraction Settings ---
    {
        auto content = contractionCardBounds.reduced (12);
        content.removeFromTop (28); // Header height

        const int numRows = 3;
        const int rowHeight = content.getHeight() / numRows;

        // Row 1 (2 columns): Contraction Attack & Release
        auto row1 = content.removeFromTop (rowHeight);
        const int colW2 = row1.getWidth() / 2;
        
        auto cell = row1.removeFromLeft (colW2).reduced (4);
        contractionAttackControl.label.setBounds (cell.removeFromBottom (14));
        contractionAttackControl.slider.setBounds (cell);

        cell = row1.removeFromLeft (colW2).reduced (4);
        contractionReleaseControl.label.setBounds (cell.removeFromBottom (14));
        contractionReleaseControl.slider.setBounds (cell);

        // Row 2 (3 columns): Wet Reduction, Tail Reduction, Tone Darkening
        auto row2 = content.removeFromTop (rowHeight);
        const int colW3 = row2.getWidth() / 3;

        cell = row2.removeFromLeft (colW3).reduced (4);
        wetReductionControl.label.setBounds (cell.removeFromBottom (14));
        wetReductionControl.slider.setBounds (cell);

        cell = row2.removeFromLeft (colW3).reduced (4);
        tailReductionControl.label.setBounds (cell.removeFromBottom (14));
        tailReductionControl.slider.setBounds (cell);

        cell = row2.removeFromLeft (colW3).reduced (4);
        toneDarkeningControl.label.setBounds (cell.removeFromBottom (14));
        toneDarkeningControl.slider.setBounds (cell);

        // Row 3 (2 columns): Width Reduction, Attack Soften Increase
        auto row3 = content.removeFromTop (rowHeight);

        cell = row3.removeFromLeft (colW2).reduced (4);
        widthReductionControl.label.setBounds (cell.removeFromBottom (14));
        widthReductionControl.slider.setBounds (cell);

        cell = row3.removeFromLeft (colW2).reduced (4);
        attackSoftenIncreaseControl.label.setBounds (cell.removeFromBottom (14));
        attackSoftenIncreaseControl.slider.setBounds (cell);
    }

    // --- Lay out Bloom Settings ---
    {
        auto content = bloomCardBounds.reduced (12);
        content.removeFromTop (28); // Header height

        const int numRows = 3;
        const int rowHeight = content.getHeight() / numRows;

        // Row 1 (3 columns): Bloom Rise, Hold, Release
        auto row1 = content.removeFromTop (rowHeight);
        const int colW3 = row1.getWidth() / 3;

        auto cell = row1.removeFromLeft (colW3).reduced (4);
        bloomRiseControl.label.setBounds (cell.removeFromBottom (14));
        bloomRiseControl.slider.setBounds (cell);

        cell = row1.removeFromLeft (colW3).reduced (4);
        bloomHoldControl.label.setBounds (cell.removeFromBottom (14));
        bloomHoldControl.slider.setBounds (cell);

        cell = row1.removeFromLeft (colW3).reduced (4);
        bloomReleaseControl.label.setBounds (cell.removeFromBottom (14));
        bloomReleaseControl.slider.setBounds (cell);

        // Row 2 (3 columns): Wet Lift, Tail Expansion, Tone Opening
        auto row2 = content.removeFromTop (rowHeight);

        cell = row2.removeFromLeft (colW3).reduced (4);
        wetLiftControl.label.setBounds (cell.removeFromBottom (14));
        wetLiftControl.slider.setBounds (cell);

        cell = row2.removeFromLeft (colW3).reduced (4);
        tailExpansionControl.label.setBounds (cell.removeFromBottom (14));
        tailExpansionControl.slider.setBounds (cell);

        cell = row2.removeFromLeft (colW3).reduced (4);
        toneOpeningControl.label.setBounds (cell.removeFromBottom (14));
        toneOpeningControl.slider.setBounds (cell);

        // Row 3 (2 columns): Width Expansion, Diffusion Increase
        auto row3 = content.removeFromTop (rowHeight);
        const int colW2 = row3.getWidth() / 2;

        cell = row3.removeFromLeft (colW2).reduced (4);
        widthExpansionControl.label.setBounds (cell.removeFromBottom (14));
        widthExpansionControl.slider.setBounds (cell);

        cell = row3.removeFromLeft (colW2).reduced (4);
        diffusionIncreaseControl.label.setBounds (cell.removeFromBottom (14));
        diffusionIncreaseControl.slider.setBounds (cell);
    }
}

void IntelligencePage::setupControl (ParameterControl& control,
                                     const juce::String& paramId,
                                     const juce::String& labelText,
                                     const juce::String& suffix)
{
    // 1. Setup Slider
    control.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible (control.slider);

    if (auto* param = apvts.getParameter (paramId))
    {
        auto rangeFloat = param->getNormalisableRange();
        juce::NormalisableRange<double> rangeDouble {
            static_cast<double> (rangeFloat.start),
            static_cast<double> (rangeFloat.end)
        };
        rangeDouble.interval = static_cast<double> (rangeFloat.interval);
        rangeDouble.skew = static_cast<double> (rangeFloat.skew);
        rangeDouble.symmetricSkew = rangeFloat.symmetricSkew;
        control.slider.setNormalisableRange (rangeDouble);
    }

    if (suffix.isNotEmpty())
    {
        if (suffix == " Hz")
        {
            control.slider.textFromValueFunction = [] (double v) {
                return juce::String (std::round (v)) + " Hz";
            };
        }
        else if (suffix == " dB")
        {
            control.slider.textFromValueFunction = [] (double v) {
                return juce::String (v, 1) + " dB";
            };
        }
        else if (suffix == " %")
        {
            control.slider.textFromValueFunction = [] (double v) {
                return juce::String (juce::roundToInt (v)) + " %";
            };
        }
        else if (suffix == " ms")
        {
            control.slider.textFromValueFunction = [] (double v) {
                return juce::String (v, 1) + " ms";
            };
        }
        else if (suffix == " oct")
        {
            control.slider.textFromValueFunction = [] (double v) {
                return juce::String (v, 2) + " oct";
            };
        }
        else if (suffix == " ev/s")
        {
            control.slider.textFromValueFunction = [] (double v) {
                return juce::String (v, 1) + " ev/s";
            };
        }

        control.slider.valueFromTextFunction = [] (const juce::String& text) {
            return text.initialSectionContainingOnly ("0123456789.-").getDoubleValue();
        };
    }

    // 2. Setup Label
    control.label.setText (labelText, juce::dontSendNotification);
    control.label.setJustificationType (juce::Justification::centred);
    control.label.setFont (juce::FontOptions (10.0f));
    control.label.setColour (juce::Label::textColourId, CustomLookAndFeel::shredText);
    addAndMakeVisible (control.label);

    // 3. Setup Attachment
    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, control.slider);
}

} // namespace shredspace
