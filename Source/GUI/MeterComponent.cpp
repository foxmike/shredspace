#include "MeterComponent.h"
#include "CustomLookAndFeel.h"
#include <cmath>
#include <algorithm>

MeterComponent::MeterComponent (shredspace::TelemetryState& telemetryStateToUse)
    : telemetryState (telemetryStateToUse)
{
    startTimerHz (30);
}

MeterComponent::~MeterComponent()
{
    stopTimer();
}

float MeterComponent::convertLinearToDb (float level) noexcept
{
    return 20.0f * std::log10 (std::max (level, 1.0e-6f));
}

float MeterComponent::mapDbToProportional (float db) noexcept
{
    if (db <= -60.0f)
        return 0.0f;
    if (db >= 0.0f)
        return 1.0f;
    return (db + 60.0f) / 60.0f;
}

void MeterComponent::timerCallback()
{
    // Read raw values from TelemetryState atomics (relaxed memory order)
    float rawLeftPeak  = telemetryState.getLeftPeak();
    float rawRightPeak = telemetryState.getRightPeak();
    float rawLeftRms   = telemetryState.getLeftRms();
    float rawRightRms  = telemetryState.getRightRms();

    // 1. Peak rise / exponential decay
    if (rawLeftPeak > leftPeakLevel)
        leftPeakLevel = rawLeftPeak;
    else
        leftPeakLevel = std::max (rawLeftPeak, leftPeakLevel * decayFactor);

    if (rawRightPeak > rightPeakLevel)
        rightPeakLevel = rawRightPeak;
    else
        rightPeakLevel = std::max (rawRightPeak, rightPeakLevel * decayFactor);

    // 2. RMS rise / decay (smooth display)
    if (rawLeftRms > leftRmsLevel)
        leftRmsLevel = rawLeftRms;
    else
        leftRmsLevel = std::max (rawLeftRms, leftRmsLevel * decayFactor);

    if (rawRightRms > rightRmsLevel)
        rightRmsLevel = rawRightRms;
    else
        rightRmsLevel = std::max (rawRightRms, rightRmsLevel * decayFactor);

    // 3. Peak Hold marker ballistics (resets after 1500 ms / 45 ticks)
    if (rawLeftPeak >= leftPeakHold)
    {
        leftPeakHold = rawLeftPeak;
        leftHoldTicks = 0;
    }
    else
    {
        leftHoldTicks++;
        if (leftHoldTicks >= 45)
        {
            leftPeakHold = leftPeakLevel;
            leftHoldTicks = 0;
        }
    }
    leftPeakHold = std::max (leftPeakHold, leftPeakLevel);

    if (rawRightPeak >= rightPeakHold)
    {
        rightPeakHold = rawRightPeak;
        rightHoldTicks = 0;
    }
    else
    {
        rightHoldTicks++;
        if (rightHoldTicks >= 45)
        {
            rightPeakHold = rightPeakLevel;
            rightHoldTicks = 0;
        }
    }
    rightPeakHold = std::max (rightPeakHold, rightPeakLevel);

    // 4. Read indicators
    activityLevel    = telemetryState.getActivity();
    densityLevel     = telemetryState.getDensity();
    contractionLevel = telemetryState.getContraction();
    bloomLevel       = telemetryState.getBloom();
    safetyEngagedVal = telemetryState.getSafetyEngaged();

    repaint();
}

void MeterComponent::resized()
{
}

void MeterComponent::paint (juce::Graphics& g)
{
    // Draw outer panel background
    g.fillAll (CustomLookAndFeel::shredWidgetBg);

    // Draw outer panel border
    g.setColour (CustomLookAndFeel::shredOutline);
    g.drawRect (getLocalBounds(), 1);

    auto bounds = getLocalBounds().reduced (8);

    // Layout configuration:
    // Left side: Stereo Meters (approx 35% width)
    // Right side: Indicator Cluster (approx 65% width)
    auto meterWidth = static_cast<int> (static_cast<float> (bounds.getWidth()) * 0.35f);
    auto meterArea = bounds.removeFromLeft (meterWidth);

    // Vertical divider
    g.setColour (CustomLookAndFeel::shredOutline);
    g.drawRect (meterArea.getRight() + 4, bounds.getY(), 1, bounds.getHeight());

    // Indicator area with horizontal padding
    auto indicatorArea = bounds.withTrimmedLeft (12);

    // --- 1. Draw Stereo Meters ---
    auto labelHeight = 15;
    auto meterBarsArea = meterArea.withTrimmedBottom (labelHeight);
    auto labelsArea = meterArea.withTrimmedTop (meterBarsArea.getHeight());

    auto leftMeterRect  = meterBarsArea.removeFromLeft (meterBarsArea.getWidth() / 2).reduced (4, 0);
    auto rightMeterRect = meterBarsArea.reduced (4, 0);

    // Background tracks for meters
    g.setColour (CustomLookAndFeel::shredDarkBg);
    g.fillRoundedRectangle (leftMeterRect.toFloat(), 2.0f);
    g.fillRoundedRectangle (rightMeterRect.toFloat(), 2.0f);

    g.setColour (CustomLookAndFeel::shredOutline);
    g.drawRoundedRectangle (leftMeterRect.toFloat(), 2.0f, 1.0f);
    g.drawRoundedRectangle (rightMeterRect.toFloat(), 2.0f, 1.0f);

    // Left Channel drawings
    {
        float dbPeak = convertLinearToDb (leftPeakLevel);
        float dbRms  = convertLinearToDb (leftRmsLevel);
        float dbHold = convertLinearToDb (leftPeakHold);

        float pPeak = mapDbToProportional (dbPeak);
        float pRms  = mapDbToProportional (dbRms);
        float pHold = mapDbToProportional (dbHold);

        float leftPeakHeight = leftMeterRect.getHeight() * pPeak;
        float leftRmsHeight  = leftMeterRect.getHeight() * pRms;

        // Draw peak block underlay (semi-transparent)
        if (leftPeakHeight > 0.0f)
        {
            auto r = leftMeterRect.toFloat().withTop (leftMeterRect.getBottom() - leftPeakHeight);
            g.setColour (CustomLookAndFeel::shredAccent.withAlpha (0.3f));
            g.fillRoundedRectangle (r, 1.0f);
        }

        // Draw RMS block (solid overlay)
        if (leftRmsHeight > 0.0f)
        {
            auto r = leftMeterRect.toFloat().withTop (leftMeterRect.getBottom() - leftRmsHeight);
            g.setColour (CustomLookAndFeel::shredAccent.withAlpha (0.7f));
            g.fillRoundedRectangle (r, 1.0f);
        }

        // Draw peak-hold tick
        if (pHold > 0.0f)
        {
            float y = leftMeterRect.getBottom() - leftMeterRect.getHeight() * pHold;
            g.setColour (CustomLookAndFeel::shredHighlightText);
            g.fillRect (leftMeterRect.getX(), static_cast<int> (y) - 1, leftMeterRect.getWidth(), 2);
        }
    }

    // Right Channel drawings
    {
        float dbPeak = convertLinearToDb (rightPeakLevel);
        float dbRms  = convertLinearToDb (rightRmsLevel);
        float dbHold = convertLinearToDb (rightPeakHold);

        float pPeak = mapDbToProportional (dbPeak);
        float pRms  = mapDbToProportional (dbRms);
        float pHold = mapDbToProportional (dbHold);

        float rightPeakHeight = rightMeterRect.getHeight() * pPeak;
        float rightRmsHeight  = rightMeterRect.getHeight() * pRms;

        // Draw peak block underlay (semi-transparent)
        if (rightPeakHeight > 0.0f)
        {
            auto r = rightMeterRect.toFloat().withTop (rightMeterRect.getBottom() - rightPeakHeight);
            g.setColour (CustomLookAndFeel::shredAccent.withAlpha (0.3f));
            g.fillRoundedRectangle (r, 1.0f);
        }

        // Draw RMS block (solid overlay)
        if (rightRmsHeight > 0.0f)
        {
            auto r = rightMeterRect.toFloat().withTop (rightMeterRect.getBottom() - rightRmsHeight);
            g.setColour (CustomLookAndFeel::shredAccent.withAlpha (0.7f));
            g.fillRoundedRectangle (r, 1.0f);
        }

        // Draw peak-hold tick
        if (pHold > 0.0f)
        {
            float y = rightMeterRect.getBottom() - rightMeterRect.getHeight() * pHold;
            g.setColour (CustomLookAndFeel::shredHighlightText);
            g.fillRect (rightMeterRect.getX(), static_cast<int> (y) - 1, rightMeterRect.getWidth(), 2);
        }
    }

    // Draw L/R labels
    g.setColour (CustomLookAndFeel::shredMutedText);
    g.setFont (10.0f);
    auto halfWidth = labelsArea.getWidth() / 2;
    g.drawText ("L", labelsArea.removeFromLeft (halfWidth), juce::Justification::centred);
    g.drawText ("R", labelsArea, juce::Justification::centred);

    // --- 2. Draw Indicator Cluster ---
    auto numRows = 5;
    auto rowHeight = indicatorArea.getHeight() / numRows;

    drawLabelledBar (g, "Activity", activityLevel, indicatorArea.removeFromTop (rowHeight));
    drawLabelledBar (g, "Density", densityLevel, indicatorArea.removeFromTop (rowHeight));
    drawLabelledBar (g, "Contraction", contractionLevel, indicatorArea.removeFromTop (rowHeight));
    drawLabelledBar (g, "Bloom", bloomLevel, indicatorArea.removeFromTop (rowHeight));
    drawSafetyLamp (g, safetyEngagedVal, indicatorArea.removeFromTop (rowHeight));
}

void MeterComponent::drawLabelledBar (juce::Graphics& g, const juce::String& label, float value, juce::Rectangle<int> area)
{
    auto labelWidth = static_cast<int> (static_cast<float> (area.getWidth()) * 0.4f);
    auto barRect = area.withTrimmedLeft (labelWidth).reduced (0, 4);

    // Draw row label text
    g.setColour (CustomLookAndFeel::shredText);
    g.setFont (12.0f);
    g.drawText (label, area.getX(), area.getY(), labelWidth, area.getHeight(), juce::Justification::centredLeft);

    // Draw track background
    g.setColour (CustomLookAndFeel::shredDarkBg);
    g.fillRoundedRectangle (barRect.toFloat(), 2.0f);

    g.setColour (CustomLookAndFeel::shredOutline);
    g.drawRoundedRectangle (barRect.toFloat(), 2.0f, 1.0f);

    // Fill level
    float fillWidth = static_cast<float> (barRect.getWidth()) * std::clamp (value, 0.0f, 1.0f);
    if (fillWidth > 0.5f)
    {
        auto fillRect = barRect.toFloat().withWidth (fillWidth);
        g.setColour (CustomLookAndFeel::shredAccent);
        g.fillRoundedRectangle (fillRect, 2.0f);
    }
}

void MeterComponent::drawSafetyLamp (juce::Graphics& g, bool isEngaged, juce::Rectangle<int> area)
{
    auto labelWidth = static_cast<int> (static_cast<float> (area.getWidth()) * 0.4f);

    // Draw safety row label text
    g.setColour (CustomLookAndFeel::shredText);
    g.setFont (12.0f);
    g.drawText ("Safety", area.getX(), area.getY(), labelWidth, area.getHeight(), juce::Justification::centredLeft);

    auto lampArea = area.withTrimmedLeft (labelWidth);
    auto lampSize = juce::jmin (12, lampArea.getHeight() - 8);
    auto lampRect = juce::Rectangle<int> (lampArea.getX() + 4, lampArea.getCentreY() - lampSize / 2, lampSize, lampSize);

    if (isEngaged)
    {
        // Engaged: Glow bright red/orange (shredAccent)
        g.setColour (CustomLookAndFeel::shredAccent);
        g.fillEllipse (lampRect.toFloat());

        g.setColour (CustomLookAndFeel::shredHighlightText);
        g.fillEllipse (lampRect.toFloat().reduced (3.0f));
    }
    else
    {
        // Disengaged: Muted track background and outline
        g.setColour (CustomLookAndFeel::shredDarkBg);
        g.fillEllipse (lampRect.toFloat());

        g.setColour (CustomLookAndFeel::shredOutline);
        g.drawEllipse (lampRect.toFloat(), 1.0f);
    }

    // Safety status text
    g.setColour (isEngaged ? CustomLookAndFeel::shredAccent : CustomLookAndFeel::shredMutedText);
    g.setFont (10.0f);
    auto statusTextRect = lampArea.withTrimmedLeft (lampSize + 10);
    g.drawText (isEngaged ? "LIMITING" : "OK", statusTextRect, juce::Justification::centredLeft);
}
