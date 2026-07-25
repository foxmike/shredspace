#include "ProfileManager.h"
#include "../Parameters/ParameterLayout.h"

namespace shredspace
{

namespace
{
    class DummyAudioProcessor : public juce::AudioProcessor
    {
    public:
        DummyAudioProcessor()
            : juce::AudioProcessor (BusesProperties())
        {}
        ~DummyAudioProcessor() override = default;

        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        const juce::String getName() const override { return "Dummy"; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 0; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return ""; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };
} // namespace

ProfileManager::ProfileManager()
    : ProfileManager (BinaryDataAccessor())
{
}

ProfileManager::ProfileManager (BinaryDataAccessor accessor)
    : binaryDataAccessor (accessor)
{
    allowedParameterIDs = getFrozenParameterIDs();
}

juce::StringArray ProfileManager::getFrozenParameterIDs()
{
    DummyAudioProcessor dummyProcessor;
    juce::AudioProcessorValueTreeState dummyState (dummyProcessor, nullptr, "PARAMS", createParameterLayout());
    
    juce::StringArray ids;
    for (auto* param : dummyProcessor.getParameters())
    {
        if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            ids.add (rangedParam->getParameterID());
        }
    }
    return ids;
}

void ProfileManager::clear()
{
    const juce::ScopedLock lock (catalogMutex);
    catalog.clear();
    loadReport.clear();
}

void ProfileManager::loadBundledProfiles()
{
    const juce::ScopedLock lock (catalogMutex);

    if (binaryDataAccessor.namedResourceListSize == nullptr ||
        binaryDataAccessor.namedResourceList == nullptr ||
        binaryDataAccessor.getNamedResource == nullptr)
    {
        loadReport.add ("BinaryDataAccessor functions are null");
        return;
    }

    const int numResources = binaryDataAccessor.namedResourceListSize();
    for (int i = 0; i < numResources; ++i)
    {
        const char* resourceName = binaryDataAccessor.namedResourceList (i);
        if (resourceName == nullptr)
            continue;

        juce::String resourceNameStr (resourceName);
        if (! resourceNameStr.startsWith ("Resources/Profiles"))
            continue;

        int dataSize = 0;
        const char* data = binaryDataAccessor.getNamedResource (resourceName, dataSize);
        if (data == nullptr || dataSize <= 0)
        {
            loadReport.add ("Resource empty or not found: " + resourceNameStr);
            continue;
        }

        juce::String jsonText = juce::String::fromUTF8 (data, dataSize);
        juce::var jsonVar;
        juce::Result jsonParseResult = juce::JSON::parse (jsonText, jsonVar);
        if (jsonParseResult.failed())
        {
            loadReport.add ("JSON parse error in " + resourceNameStr + ": " + jsonParseResult.getErrorMessage());
            continue;
        }

        auto parseResult = ProfileSchema::parse (jsonVar, allowedParameterIDs);
        if (! parseResult.second.ok)
        {
            for (const auto& err : parseResult.second.errors)
            {
                loadReport.add ("Validation error in " + resourceNameStr + ": " + err);
            }
            continue;
        }

        const auto& newDoc = parseResult.first;

        // Insert or replace in catalog
        auto it = std::find_if (catalog.begin(), catalog.end(), [&newDoc](const std::unique_ptr<ProfileDocument>& d) {
            return d->profile_name == newDoc.profile_name;
        });

        if (it != catalog.end())
            *it = std::make_unique<ProfileDocument> (newDoc);
        else
            catalog.push_back (std::make_unique<ProfileDocument> (newDoc));
    }
}

void ProfileManager::loadUserProfiles (const juce::File& userDir)
{
    const juce::ScopedLock lock (catalogMutex);

    if (! userDir.isDirectory())
        return; // Missing/unreadable directory is not an error; yields empty result

    juce::Array<juce::File> files;
    userDir.findChildFiles (files, juce::File::findFiles, false, "*.json");

    for (const auto& file : files)
    {
        juce::String jsonText = file.loadFileAsString();
        if (jsonText.isEmpty())
        {
            loadReport.add ("Failed to read user profile file or file is empty: " + file.getFileName());
            continue;
        }

        juce::var jsonVar;
        juce::Result jsonParseResult = juce::JSON::parse (jsonText, jsonVar);
        if (jsonParseResult.failed())
        {
            loadReport.add ("JSON parse error in " + file.getFileName() + ": " + jsonParseResult.getErrorMessage());
            continue;
        }

        auto parseResult = ProfileSchema::parse (jsonVar, allowedParameterIDs);
        if (! parseResult.second.ok)
        {
            for (const auto& err : parseResult.second.errors)
            {
                loadReport.add ("Validation error in " + file.getFileName() + ": " + err);
            }
            continue;
        }

        const auto& newDoc = parseResult.first;

        // Insert or replace in catalog
        auto it = std::find_if (catalog.begin(), catalog.end(), [&newDoc](const std::unique_ptr<ProfileDocument>& d) {
            return d->profile_name == newDoc.profile_name;
        });

        if (it != catalog.end())
            *it = std::make_unique<ProfileDocument> (newDoc);
        else
            catalog.push_back (std::make_unique<ProfileDocument> (newDoc));
    }
}

std::vector<ProfileDocument> ProfileManager::getProfiles() const
{
    const juce::ScopedLock lock (catalogMutex);
    std::vector<ProfileDocument> result;
    result.reserve (catalog.size());
    for (const auto& docPtr : catalog)
    {
        if (docPtr != nullptr)
            result.push_back (*docPtr);
    }
    return result;
}

const ProfileDocument* ProfileManager::getProfile (const juce::String& name) const
{
    const juce::ScopedLock lock (catalogMutex);
    return getProfileInternal (name);
}

const ProfileDocument* ProfileManager::getProfileInternal (const juce::String& name) const
{
    for (const auto& docPtr : catalog)
    {
        if (docPtr != nullptr && docPtr->profile_name == name)
            return docPtr.get();
    }
    return nullptr;
}

bool ProfileManager::recallProfile (const juce::String& name, juce::AudioProcessorValueTreeState& state)
{
    JUCE_ASSERT_MESSAGE_THREAD;

    std::map<juce::String, float> paramsToSet;
    {
        const juce::ScopedLock lock (catalogMutex);
        const auto* profile = getProfileInternal (name);
        if (profile == nullptr)
            return false;
        paramsToSet = profile->base_parameter_values;
    }

    for (const auto& pair : paramsToSet)
    {
        const auto& paramId = pair.first;
        const auto value = pair.second;

        if (auto* param = state.getParameter (paramId))
        {
            auto normalisedVal = param->getNormalisableRange().convertTo0to1 (value);
            param->setValueNotifyingHost (normalisedVal);
        }
    }

    return true;
}

juce::StringArray ProfileManager::getLoadReport() const
{
    const juce::ScopedLock lock (catalogMutex);
    return loadReport;
}

} // namespace shredspace
