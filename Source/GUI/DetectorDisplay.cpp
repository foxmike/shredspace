#include "DetectorDisplay.h"
#include "CustomLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace shredspace
{

DetectorDisplay::DetectorDisplay (shredspace::TelemetryState& telemetryStateToUse)
    : telemetryState (telemetryStateToUse)
{
    startTimerHz (30);
}

DetectorDisplay::~DetectorDisplay()
{
    stopTimer();
}

void DetectorDisplay::timerCallback()
{
    // Drain history FIFO into ring buffer
    std::array<HistorySample, TelemetryState::historyCapacity> tempBuffer;
    int numRead = telemetryState.readHistory (tempBuffer.data(), static_cast<int> (tempBuffer.size()));
    for (int i = 0; i < numRead; ++i)
    {
        ringBuffer[static_cast<size_t> (writeIndex)] = tempBuffer[static_cast<size_t> (i)];
        writeIndex = (writeIndex + 1) % ringBufferSize;
    }

    // Read raw values from TelemetryState atomics (relaxed memory order)
    currentActivity = telemetryState.getActivity();
    currentDensity  = telemetryState.getDensity();
    currentSustain  = telemetryState.getSustain();

    repaint();
}

void DetectorDisplay::paint (juce::Graphics& g)
{
    auto totalBounds = getLocalBounds();
    auto boundsF = totalBounds.toFloat();

    // Fill background of the main component bounds
    g.setColour (CustomLookAndFeel::shredWidgetBg);
    g.fillRoundedRectangle (boundsF, 6.0f);
    
    g.setColour (CustomLookAndFeel::shredOutline);
    g.drawRoundedRectangle (boundsF, 6.0f, 1.5f);

    // Title / Header bar
    auto headerBar = boundsF.removeFromTop (24.0f);
    g.setColour (CustomLookAndFeel::shredOutline.withAlpha (0.3f));
    g.fillRoundedRectangle (headerBar, 6.0f);

    g.setColour (CustomLookAndFeel::shredHighlightText);
    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    g.drawText ("DETECTOR SIGNAL VISUALIZER", headerBar.reduced (10.0f, 0.0f), juce::Justification::centredLeft, true);

    // Subtract reduced padding for content
    auto contentBounds = totalBounds.reduced (8);
    contentBounds.removeFromTop (24); // Exclude header height
    
    auto plotBounds = contentBounds;
    auto sidebarBounds = plotBounds.removeFromRight (120);
    plotBounds.removeFromRight (10); // Gap between plot and sidebar

    auto plotBoundsF = plotBounds.toFloat();
    auto sidebarBoundsF = sidebarBounds.toFloat();

    // Draw plot background
    g.setColour (CustomLookAndFeel::shredDarkBg);
    g.fillRoundedRectangle (plotBoundsF, 4.0f);
    g.setColour (CustomLookAndFeel::shredOutline.withAlpha (0.5f));
    g.drawRoundedRectangle (plotBoundsF, 4.0f, 1.0f);

    // Draw horizontal grid lines inside the plot area
    g.setColour (CustomLookAndFeel::shredOutline.withAlpha (0.25f));
    for (float ratio : { 0.25f, 0.50f, 0.75f })
    {
        float y = plotBoundsF.getY() + ratio * plotBoundsF.getHeight();
        g.drawHorizontalLine (juce::roundToInt (y), plotBoundsF.getX(), plotBoundsF.getRight());
    }

    // Paint scrolling traces
    juce::Path onsetPath;
    juce::Path densityPath;
    juce::Path sustainPath;
    juce::Path gapPath;

    const int numPoints = juce::roundToInt (plotBoundsF.getWidth());
    if (numPoints > 1)
    {
        const float h = plotBoundsF.getHeight();
        const float yOffset = plotBoundsF.getY();
        const float xOffset = plotBoundsF.getX();
        const float padding = 2.0f;
        const float plotH = h - 2.0f * padding;

        for (int x = 0; x < numPoints; ++x)
        {
            float progress = static_cast<float> (x) / static_cast<float> (numPoints - 1);
            int ringIdx = (writeIndex + static_cast<int> (progress * (ringBufferSize - 1))) % ringBufferSize;
            const auto& sample = ringBuffer[static_cast<size_t> (ringIdx)];

            float yOnset   = yOffset + padding + (1.0f - juce::jlimit (0.0f, 1.0f, sample.onset)) * plotH;
            float yDensity = yOffset + padding + (1.0f - juce::jlimit (0.0f, 1.0f, sample.density)) * plotH;
            float ySustain = yOffset + padding + (1.0f - juce::jlimit (0.0f, 1.0f, sample.sustain)) * plotH;
            float yGap     = yOffset + padding + (1.0f - juce::jlimit (0.0f, 1.0f, sample.gap)) * plotH;

            float px = xOffset + static_cast<float> (x);

            if (x == 0)
            {
                onsetPath.startNewSubPath (px, yOnset);
                densityPath.startNewSubPath (px, yDensity);
                sustainPath.startNewSubPath (px, ySustain);
                gapPath.startNewSubPath (px, yGap);
            }
            else
            {
                onsetPath.lineTo (px, yOnset);
                densityPath.lineTo (px, yDensity);
                sustainPath.lineTo (px, ySustain);
                gapPath.lineTo (px, yGap);
            }
        }

        // Draw traces with beautiful distinct colors
        g.setColour (juce::Colour (0xfff1c40f)); // Gold for Onset
        g.strokePath (onsetPath, juce::PathStrokeType (1.5f));

        g.setColour (juce::Colour (0xff3498db)); // Blue for Density
        g.strokePath (densityPath, juce::PathStrokeType (1.5f));

        g.setColour (juce::Colour (0xff2ecc71)); // Green for Sustain
        g.strokePath (sustainPath, juce::PathStrokeType (1.5f));

        g.setColour (juce::Colour (0xff9b59b6)); // Purple for Phrase Gap
        g.strokePath (gapPath, juce::PathStrokeType (1.5f));
    }

    // Paint sidebar with current scalar values from TelemetryState atomics (real-time, not FIFO-based)
    const float rowH = sidebarBoundsF.getHeight() / 3.0f;
    
    auto drawSidebarRow = [&g] (juce::Rectangle<float> area, const juce::String& label, float value, juce::Colour col) {
        // Label
        g.setColour (CustomLookAndFeel::shredMutedText);
        g.setFont (juce::FontOptions (9.0f).withStyle ("Bold"));
        g.drawText (label, area.removeFromTop (12.0f), juce::Justification::topLeft, true);

        // Numeric percentage string
        g.setColour (CustomLookAndFeel::shredHighlightText);
        g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
        juce::String valStr = juce::String (juce::roundToInt (value * 100.0f)) + "%";
        g.drawText (valStr, area.removeFromTop (14.0f), juce::Justification::topLeft, true);

        area.removeFromTop (2.0f); // spacer

        // Progress bar representing the value
        auto barArea = area.removeFromTop (6.0f);
        g.setColour (CustomLookAndFeel::shredOutline);
        g.fillRoundedRectangle (barArea, 1.5f);

        auto fillArea = barArea;
        fillArea.setWidth (fillArea.getWidth() * juce::jlimit (0.0f, 1.0f, value));
        g.setColour (col);
        g.fillRoundedRectangle (fillArea, 1.5f);
    };

    drawSidebarRow (sidebarBoundsF.removeFromTop (rowH).reduced (0.0f, 4.0f), "ONSET ACTIVITY", currentActivity, juce::Colour (0xfff1c40f));
    drawSidebarRow (sidebarBoundsF.removeFromTop (rowH).reduced (0.0f, 4.0f), "DENSITY", currentDensity, juce::Colour (0xff3498db));
    drawSidebarRow (sidebarBoundsF.removeFromTop (rowH).reduced (0.0f, 4.0f), "SUSTAIN", currentSustain, juce::Colour (0xff2ecc71));
}

void DetectorDisplay::resized()
{
}

} // namespace shredspace
