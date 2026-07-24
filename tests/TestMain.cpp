/*
  ==============================================================================
  Test runner: a console application that runs every registered juce::UnitTest
  and returns a non-zero exit code if any test failed. Uses ONLY juce::UnitTest;
  no third-party test frameworks are involved.
  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_processors/juce_audio_processors.h>

int main (int argc, char* argv[])
{
    juce::ignoreUnused (argc, argv);

    // Initialises (and later shuts down) JUCE for a console application. This
    // also brings a MessageManager into existence on this thread, which the
    // ShredSpaceAudioProcessor state-handling code relies on.
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::MessageManager::getInstance(); // ensure the message thread is this one

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);

    // Run every registered UnitTest across all categories.
    runner.runAllTests();

    int totalFailures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* result = runner.getResult (i);

        if (result == nullptr)
            continue;

        totalFailures += result->failures;

        if (result->failures > 0)
        {
            juce::String heading = "FAIL: "
                                 + result->unitTestName
                                 + " / "
                                 + result->subcategoryName
                                 + "  (passes=" + juce::String (result->passes)
                                 + " failures=" + juce::String (result->failures) + ")";

            juce::Logger::writeToLog (heading);

            for (const auto& message : result->messages)
                juce::Logger::writeToLog ("    " + message);
        }
    }

    if (totalFailures == 0)
        juce::Logger::writeToLog ("All registered tests passed.");
    else
        juce::Logger::writeToLog ("Total test failures: " + juce::String (totalFailures));

    return totalFailures > 0 ? 1 : 0;
}
