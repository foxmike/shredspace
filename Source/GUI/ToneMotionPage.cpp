#include "ToneMotionPage.h"
#include "CustomLookAndFeel.h"
#include <cmath>

namespace shredspace
{

ToneMotionPage::ToneMotionPage (juce::AudioProcessorValueTreeState& apvtsToUse,
                                shredspace::TelemetryState& telemetryStateToUse)
    : apvts (apvtsToUse),
      telemetryState (telemetryStateToUse)
{
    // Voice Headers
    setupLabel (voiceAToneHeaderLabel, "Voice A");
    voiceAToneHeaderLabel.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    voiceAToneHeaderLabel.setColour (juce::Label::textColourId, CustomLookAndFeel::shredMutedText);

    setupLabel (voiceBToneHeaderLabel, "Voice B");
    voiceBToneHeaderLabel.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    voiceBToneHeaderLabel.setColour (juce::Label::textColourId, CustomLookAndFeel::shredMutedText);

    setupLabel (voiceAMotionHeaderLabel, "Voice A");
    voiceAMotionHeaderLabel.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    voiceAMotionHeaderLabel.setColour (juce::Label::textColourId, CustomLookAndFeel::shredMutedText);

    setupLabel (voiceBMotionHeaderLabel, "Voice B");
    voiceBMotionHeaderLabel.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    voiceBMotionHeaderLabel.setColour (juce::Label::textColourId, CustomLookAndFeel::shredMutedText);

    // Init Voice Sections
    initVoiceSection (voiceA, false);
    initVoiceSection (voiceB, true);

    // Global LF Mono Crossover
    setupSlider (lfCrossoverSlider, paramids::STEREO_LF_MONO_FREQ, apvts, " Hz");
    setupLabel (lfCrossoverLabel, "Low-Frequency Mono Crossover");
    lfCrossoverAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramids::STEREO_LF_MONO_FREQ, lfCrossoverSlider);
}

ToneMotionPage::~ToneMotionPage() = default;

void ToneMotionPage::paint (juce::Graphics& g)
{
    g.fillAll (CustomLookAndFeel::shredDarkBg);

    auto bounds = getLocalBounds().reduced (15);
    auto toneBounds = bounds.removeFromLeft (bounds.getWidth() / 2).reduced (5);
    auto motionBounds = bounds.reduced (5);

    // Draw Tone card
    {
        auto fBounds = toneBounds.toFloat();
        g.setColour (CustomLookAndFeel::shredWidgetBg);
        g.fillRoundedRectangle (fBounds, 6.0f);
        g.setColour (CustomLookAndFeel::shredOutline);
        g.drawRoundedRectangle (fBounds, 6.0f, 1.5f);

        auto header = fBounds.removeFromTop (28.0f);
        g.setColour (CustomLookAndFeel::shredOutline.withAlpha (0.3f));
        g.fillRoundedRectangle (header, 6.0f);

        g.setColour (CustomLookAndFeel::shredHighlightText);
        g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
        g.drawText ("TONE CONTROLS", header.reduced (12.0f, 0.0f), juce::Justification::centredLeft, true);
    }

    // Draw Motion card
    {
        auto fBounds = motionBounds.toFloat();
        g.setColour (CustomLookAndFeel::shredWidgetBg);
        g.fillRoundedRectangle (fBounds, 6.0f);
        g.setColour (CustomLookAndFeel::shredOutline);
        g.drawRoundedRectangle (fBounds, 6.0f, 1.5f);

        auto header = fBounds.removeFromTop (28.0f);
        g.setColour (CustomLookAndFeel::shredOutline.withAlpha (0.3f));
        g.fillRoundedRectangle (header, 6.0f);

        g.setColour (CustomLookAndFeel::shredHighlightText);
        g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
        g.drawText ("MOTION CONTROLS", header.reduced (12.0f, 0.0f), juce::Justification::centredLeft, true);
    }
}

void ToneMotionPage::resized()
{
    auto bounds = getLocalBounds().reduced (15);
    auto toneBounds = bounds.removeFromLeft (bounds.getWidth() / 2).reduced (5);
    auto motionBounds = bounds.reduced (5);

    // Tone Panel content area (exclude 28px header)
    auto toneContent = toneBounds.reduced (10);
    toneContent.removeFromTop (28);

    // Global crossover at bottom of Tone card
    auto lfCrossoverArea = toneContent.removeFromBottom (70);
    lfCrossoverLabel.setBounds (lfCrossoverArea.removeFromTop (20));
    lfCrossoverSlider.setBounds (lfCrossoverArea.reduced (20, 0));

    // Split remaining tone area into Voice A and Voice B columns
    auto toneWidth = toneContent.getWidth();
    auto voiceAToneArea = toneContent.removeFromLeft (toneWidth / 2).reduced (4);
    auto voiceBToneArea = toneContent.reduced (4);

    // Set voice labels at the top of Tone columns
    voiceAToneHeaderLabel.setBounds (voiceAToneArea.removeFromTop (20));
    voiceBToneHeaderLabel.setBounds (voiceBToneArea.removeFromTop (20));

    // Motion Panel content area
    auto motionContent = motionBounds.reduced (10);
    motionContent.removeFromTop (28);

    auto motionWidth = motionContent.getWidth();
    auto voiceAMotionArea = motionContent.removeFromLeft (motionWidth / 2).reduced (4);
    auto voiceBMotionArea = motionContent.reduced (4);

    // Set voice labels at the top of Motion columns
    voiceAMotionHeaderLabel.setBounds (voiceAMotionArea.removeFromTop (20));
    voiceBMotionHeaderLabel.setBounds (voiceBMotionArea.removeFromTop (20));

    // Layout lambdas
    auto layoutVoiceTone = [] (VoiceSection& voice, juce::Rectangle<int> col)
    {
        const int numRows = 5;
        const int rowHeight = col.getHeight() / numRows;

        // Row 1: HPF (Freq / Slope)
        auto r1 = col.removeFromTop (rowHeight).reduced (2);
        voice.hpfLabel.setBounds (r1.removeFromTop (16));
        voice.hpfSlider.setBounds (r1.removeFromLeft (r1.getWidth() / 2).reduced (2));
        voice.hpfSlopeSelector.setBounds (r1.reduced (2, 8));

        // Row 2: LPF (Freq / Slope)
        auto r2 = col.removeFromTop (rowHeight).reduced (2);
        voice.lpfLabel.setBounds (r2.removeFromTop (16));
        voice.lpfSlider.setBounds (r2.removeFromLeft (r2.getWidth() / 2).reduced (2));
        voice.lpfSlopeSelector.setBounds (r2.reduced (2, 8));

        // Row 3: Presence Guard (Freq / Threshold / Amount)
        auto r3 = col.removeFromTop (rowHeight).reduced (2);
        voice.presenceFreqLabel.setBounds (r3.removeFromTop (16));
        int w = r3.getWidth() / 3;
        voice.presenceFreqSlider.setBounds (r3.removeFromLeft (w).reduced (1));
        voice.presenceThresholdSlider.setBounds (r3.removeFromLeft (w).reduced (1));
        voice.presenceAmountSlider.setBounds (r3.reduced (1));

        // Row 4: Transient Softener (Amount / Time Constant)
        auto r4 = col.removeFromTop (rowHeight).reduced (2);
        voice.transientAmountLabel.setBounds (r4.removeFromTop (16));
        voice.transientAmountSlider.setBounds (r4.removeFromLeft (r4.getWidth() / 2).reduced (2));
        voice.transientTimeSlider.setBounds (r4.reduced (2));

        // Row 5: Saturation (Drive / Character)
        auto r5 = col.removeFromTop (rowHeight).reduced (2);
        voice.saturationDriveLabel.setBounds (r5.removeFromTop (16));
        voice.saturationDriveSlider.setBounds (r5.removeFromLeft (r5.getWidth() / 2).reduced (2));
        voice.saturationCharacterSlider.setBounds (r5.reduced (2));
    };

    auto layoutVoiceMotion = [] (VoiceSection& voice, juce::Rectangle<int> col)
    {
        const int numRows = 5;
        const int rowHeight = col.getHeight() / numRows;

        // Row 1: Modulation Rate & Depth
        auto r1 = col.removeFromTop (rowHeight).reduced (2);
        voice.modRateLabel.setBounds (r1.removeFromTop (16));
        voice.modRateSlider.setBounds (r1.removeFromLeft (r1.getWidth() / 2).reduced (2));
        voice.modDepthSlider.setBounds (r1.reduced (2));

        // Row 2: Drift
        auto r2 = col.removeFromTop (rowHeight).reduced (2);
        voice.driftLabel.setBounds (r2.removeFromTop (16));
        voice.driftSlider.setBounds (r2.reduced (2));

        // Row 3: Diffusion (Amount / Size)
        auto r3 = col.removeFromTop (rowHeight).reduced (2);
        voice.diffusionAmountLabel.setBounds (r3.removeFromTop (16));
        voice.diffusionAmountSlider.setBounds (r3.removeFromLeft (r3.getWidth() / 2).reduced (2));
        voice.diffusionSizeSlider.setBounds (r3.reduced (2));

        // Row 4: Stereo Width & Spread
        auto r4 = col.removeFromTop (rowHeight).reduced (2);
        voice.stereoWidthLabel.setBounds (r4.removeFromTop (16));
        voice.stereoWidthSlider.setBounds (r4.removeFromLeft (r4.getWidth() / 2).reduced (2));
        voice.stereoSpreadSlider.setBounds (r4.reduced (2));

        // Row 5: Correlation Target
        auto r5 = col.removeFromTop (rowHeight).reduced (2);
        voice.stereoCorrelationLabel.setBounds (r5.removeFromTop (16));
        voice.stereoCorrelationSlider.setBounds (r5.reduced (2));
    };

    layoutVoiceTone (voiceA, voiceAToneArea);
    layoutVoiceTone (voiceB, voiceBToneArea);
    layoutVoiceMotion (voiceA, voiceAMotionArea);
    layoutVoiceMotion (voiceB, voiceBMotionArea);
}

void ToneMotionPage::initVoiceSection (VoiceSection& section, bool isVoiceB)
{
    const char* hpfId = isVoiceB ? paramids::VOICE_B_HPF : paramids::VOICE_A_HPF;
    const char* lpfId = isVoiceB ? paramids::VOICE_B_LPF : paramids::VOICE_A_LPF;
    const char* presenceFreqId = isVoiceB ? paramids::VOICE_B_PRESENCE_FREQ : paramids::VOICE_A_PRESENCE_FREQ;
    const char* presenceGuardId = isVoiceB ? paramids::VOICE_B_PRESENCE_GUARD : paramids::VOICE_A_PRESENCE_GUARD;
    const char* attackSoftenId = isVoiceB ? paramids::VOICE_B_ATTACK_SOFTEN : paramids::VOICE_A_ATTACK_SOFTEN;
    const char* saturationId = isVoiceB ? paramids::VOICE_B_SATURATION : paramids::VOICE_A_SATURATION;
    const char* modRateId = isVoiceB ? paramids::VOICE_B_MOD_RATE : paramids::VOICE_A_MOD_RATE;
    const char* modDepthId = isVoiceB ? paramids::VOICE_B_MOD_DEPTH : paramids::VOICE_A_MOD_DEPTH;
    const char* driftId = isVoiceB ? paramids::VOICE_B_DRIFT : paramids::VOICE_A_DRIFT;
    const char* diffusionId = isVoiceB ? paramids::VOICE_B_DIFFUSION : paramids::VOICE_A_DIFFUSION;
    const char* widthId = isVoiceB ? paramids::VOICE_B_WIDTH : paramids::VOICE_A_WIDTH;
    const char* stereoOffsetId = isVoiceB ? paramids::VOICE_B_STEREO_OFFSET : paramids::VOICE_A_STEREO_OFFSET;
    const char* crossFeedbackId = isVoiceB ? paramids::VOICE_B_CROSS_FEEDBACK : paramids::VOICE_A_CROSS_FEEDBACK;

    // HPF Freq & Slope
    setupSlider (section.hpfSlider, hpfId, apvts, " Hz");
    setupLabel (section.hpfLabel, "HPF Freq / Slope");
    section.hpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, hpfId, section.hpfSlider);

    section.hpfSlopeSelector.addItem ("12 dB/oct", 1);
    section.hpfSlopeSelector.addItem ("24 dB/oct", 2);
    section.hpfSlopeSelector.setSelectedId (1);
    addAndMakeVisible (section.hpfSlopeSelector);
    setupLabel (section.hpfSlopeLabel, "");

    // LPF Freq & Slope
    setupSlider (section.lpfSlider, lpfId, apvts, " Hz");
    setupLabel (section.lpfLabel, "LPF Freq / Slope");
    section.lpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, lpfId, section.lpfSlider);

    section.lpfSlopeSelector.addItem ("12 dB/oct", 1);
    section.lpfSlopeSelector.addItem ("24 dB/oct", 2);
    section.lpfSlopeSelector.setSelectedId (1);
    addAndMakeVisible (section.lpfSlopeSelector);
    setupLabel (section.lpfSlopeLabel, "");

    // Presence Guard: Freq, Threshold, Amount
    setupSlider (section.presenceFreqSlider, presenceFreqId, apvts, " Hz");
    setupLabel (section.presenceFreqLabel, "Presence Freq / Guard / Amt");
    section.presenceFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, presenceFreqId, section.presenceFreqSlider);

    setupSlider (section.presenceThresholdSlider, presenceGuardId, apvts, " dB");
    setupLabel (section.presenceThresholdLabel, "");
    section.presenceThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, presenceGuardId, section.presenceThresholdSlider);

    section.presenceAmountSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    section.presenceAmountSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    section.presenceAmountSlider.setRange (0.0, 100.0, 1.0);
    section.presenceAmountSlider.setValue (100.0);
    section.presenceAmountSlider.setTextValueSuffix (" %");
    addAndMakeVisible (section.presenceAmountSlider);
    setupLabel (section.presenceAmountLabel, "");

    // Transient Softener: Amount, Time Constant
    setupSlider (section.transientAmountSlider, attackSoftenId, apvts, " %");
    setupLabel (section.transientAmountLabel, "Transient Soften / Time");
    section.transientAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, attackSoftenId, section.transientAmountSlider);

    section.transientTimeSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    section.transientTimeSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    section.transientTimeSlider.setRange (5.0, 200.0, 1.0);
    section.transientTimeSlider.setValue (50.0);
    section.transientTimeSlider.setTextValueSuffix (" ms");
    addAndMakeVisible (section.transientTimeSlider);
    setupLabel (section.transientTimeLabel, "");

    // Saturation: Drive, Character
    setupSlider (section.saturationDriveSlider, saturationId, apvts, " %");
    setupLabel (section.saturationDriveLabel, "Saturation Drive / Char");
    section.saturationDriveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, saturationId, section.saturationDriveSlider);

    section.saturationCharacterSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    section.saturationCharacterSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    section.saturationCharacterSlider.setRange (0.0, 100.0, 1.0);
    section.saturationCharacterSlider.setValue (50.0);
    section.saturationCharacterSlider.setTextValueSuffix (" %");
    addAndMakeVisible (section.saturationCharacterSlider);
    setupLabel (section.saturationCharacterLabel, "");

    // Modulation: Rate, Depth
    setupSlider (section.modRateSlider, modRateId, apvts, " Hz");
    setupLabel (section.modRateLabel, "Mod Rate & Depth");
    section.modRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, modRateId, section.modRateSlider);

    setupSlider (section.modDepthSlider, modDepthId, apvts, " ms");
    setupLabel (section.modDepthLabel, "");
    section.modDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, modDepthId, section.modDepthSlider);

    // Drift
    setupSlider (section.driftSlider, driftId, apvts, " %");
    setupLabel (section.driftLabel, "Drift");
    section.driftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, driftId, section.driftSlider);

    // Diffusion: Amount, Size
    setupSlider (section.diffusionAmountSlider, diffusionId, apvts, " %");
    setupLabel (section.diffusionAmountLabel, "Diffusion Amt / Size");
    section.diffusionAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, diffusionId, section.diffusionAmountSlider);

    section.diffusionSizeSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    section.diffusionSizeSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    section.diffusionSizeSlider.setRange (10.0, 200.0, 1.0);
    section.diffusionSizeSlider.setValue (100.0);
    section.diffusionSizeSlider.setTextValueSuffix (" %");
    addAndMakeVisible (section.diffusionSizeSlider);
    setupLabel (section.diffusionSizeLabel, "");

    // Stereo: Width, Spread, Correlation Target
    setupSlider (section.stereoWidthSlider, widthId, apvts, " %");
    setupLabel (section.stereoWidthLabel, "Stereo Width / Spread");
    section.stereoWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, widthId, section.stereoWidthSlider);

    setupSlider (section.stereoSpreadSlider, stereoOffsetId, apvts, " ms");
    setupLabel (section.stereoSpreadLabel, "");
    section.stereoSpreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, stereoOffsetId, section.stereoSpreadSlider);

    setupSlider (section.stereoCorrelationSlider, crossFeedbackId, apvts, " %");
    setupLabel (section.stereoCorrelationLabel, "Cross Feedback (Correlation)");
    section.stereoCorrelationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, crossFeedbackId, section.stereoCorrelationSlider);
}

void ToneMotionPage::setupSlider (juce::Slider& slider, const juce::String& paramId, juce::AudioProcessorValueTreeState& state, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible (slider);
    
    if (auto* param = state.getParameter (paramId))
    {
        auto rangeFloat = param->getNormalisableRange();
        juce::NormalisableRange<double> rangeDouble {
            static_cast<double> (rangeFloat.start),
            static_cast<double> (rangeFloat.end)
        };
        rangeDouble.interval = static_cast<double> (rangeFloat.interval);
        rangeDouble.skew = static_cast<double> (rangeFloat.skew);
        rangeDouble.symmetricSkew = rangeFloat.symmetricSkew;
        slider.setNormalisableRange (rangeDouble);
    }

    if (suffix.isNotEmpty())
    {
        if (suffix == " Hz")
        {
            slider.textFromValueFunction = [] (double v) {
                return juce::String (std::round (v)) + " Hz";
            };
        }
        else if (suffix == " dB")
        {
            slider.textFromValueFunction = [] (double v) {
                return juce::String (v, 1) + " dB";
            };
        }
        else if (suffix == " %")
        {
            slider.textFromValueFunction = [] (double v) {
                return juce::String (std::round (v)) + " %";
            };
        }
        else if (suffix == " ms")
        {
            slider.textFromValueFunction = [] (double v) {
                return juce::String (v, 1) + " ms";
            };
        }
        
        slider.valueFromTextFunction = [] (const juce::String& text) {
            return text.initialSectionContainingOnly ("0123456789.-").getDoubleValue();
        };
    }
}

void ToneMotionPage::setupLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (10.0f));
    label.setColour (juce::Label::textColourId, CustomLookAndFeel::shredText);
    addAndMakeVisible (label);
}

} // namespace shredspace
