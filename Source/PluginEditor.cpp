#include "PluginEditor.h"

ShredSpaceAudioProcessorEditor::ShredSpaceAudioProcessorEditor (ShredSpaceAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    // Instantiate owned pages using processor reference accessors
    performancePage = std::make_unique<shredspace::PerformancePage> (
        audioProcessor.getValueTreeState(),
        macroEngine,
        sceneManager,
        profileManager,
        audioProcessor.getTelemetryState()
    );

    voicesPage = std::make_unique<shredspace::VoicesPage> (
        audioProcessor.getValueTreeState(),
        audioProcessor.getTelemetryState()
    );

    toneMotionPage = std::make_unique<shredspace::ToneMotionPage> (
        audioProcessor.getValueTreeState(),
        audioProcessor.getTelemetryState()
    );

    intelligencePage = std::make_unique<shredspace::IntelligencePage> (
        audioProcessor.getValueTreeState(),
        audioProcessor.getTelemetryState()
    );

    // Make tabs and pages child components
    addAndMakeVisible (performanceTab);
    addAndMakeVisible (voicesTab);
    addAndMakeVisible (toneMotionTab);
    addAndMakeVisible (intelligenceTab);

    addAndMakeVisible (performancePage.get());
    addAndMakeVisible (voicesPage.get());
    addAndMakeVisible (toneMotionPage.get());
    addAndMakeVisible (intelligencePage.get());

    // Configure tab button onClick handlers to switch pages
    performanceTab.onClick  = [this]() { showPage (performancePage.get()); };
    voicesTab.onClick       = [this]() { showPage (voicesPage.get()); };
    toneMotionTab.onClick   = [this]() { showPage (toneMotionPage.get()); };
    intelligenceTab.onClick = [this]() { showPage (intelligencePage.get()); };

    // Set buttons to not toggle automatically on click; we toggle them programmatically in showPage
    performanceTab.setClickingTogglesState (false);
    voicesTab.setClickingTogglesState (false);
    toneMotionTab.setClickingTogglesState (false);
    intelligenceTab.setClickingTogglesState (false);

    // Initial page selection
    showPage (performancePage.get());

    setResizable (true, true);
    setResizeLimits (600, 400, 2400, 1600);
    setSize (900, 600);
}

ShredSpaceAudioProcessorEditor::~ShredSpaceAudioProcessorEditor()
{
    // Clear look and feel explicitly to avoid dangling pointer references
    setLookAndFeel (nullptr);
}

void ShredSpaceAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void ShredSpaceAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto navArea = bounds.removeFromTop (40);

    auto buttonWidth = navArea.getWidth() / 4;
    performanceTab.setBounds (navArea.removeFromLeft (buttonWidth).reduced (2));
    voicesTab.setBounds (navArea.removeFromLeft (buttonWidth).reduced (2));
    toneMotionTab.setBounds (navArea.removeFromLeft (buttonWidth).reduced (2));
    intelligenceTab.setBounds (navArea.reduced (2));

    if (performancePage != nullptr && performancePage->isVisible())
        performancePage->setBounds (bounds);
    if (voicesPage != nullptr && voicesPage->isVisible())
        voicesPage->setBounds (bounds);
    if (toneMotionPage != nullptr && toneMotionPage->isVisible())
        toneMotionPage->setBounds (bounds);
    if (intelligencePage != nullptr && intelligencePage->isVisible())
        intelligencePage->setBounds (bounds);
}

void ShredSpaceAudioProcessorEditor::showPage (juce::Component* pageToShow)
{
    performancePage->setVisible (performancePage.get() == pageToShow);
    voicesPage->setVisible (voicesPage.get() == pageToShow);
    toneMotionPage->setVisible (toneMotionPage.get() == pageToShow);
    intelligencePage->setVisible (intelligencePage.get() == pageToShow);

    performanceTab.setToggleState (performancePage.get() == pageToShow, juce::dontSendNotification);
    voicesTab.setToggleState (voicesPage.get() == pageToShow, juce::dontSendNotification);
    toneMotionTab.setToggleState (toneMotionPage.get() == pageToShow, juce::dontSendNotification);
    intelligenceTab.setToggleState (intelligencePage.get() == pageToShow, juce::dontSendNotification);

    resized();
}
