#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ProfileSchema.h"
#include <vector>
#include <memory>

#if __has_include("BinaryData.h")
 #include "BinaryData.h"
#elif __has_include(<BinaryData.h>)
 #include <BinaryData.h>
#endif

namespace shredspace
{

class ProfileManager
{
public:
    struct BinaryDataAccessor
    {
        int (*namedResourceListSize)() = []() -> int {
            #if __has_include("BinaryData.h") || __has_include(<BinaryData.h>)
             return ::BinaryData::namedResourceListSize;
            #else
             return 0;
            #endif
        };

        const char* (*namedResourceList)(int) = [](int index) -> const char* {
            #if __has_include("BinaryData.h") || __has_include(<BinaryData.h>)
             if (index >= 0 && index < ::BinaryData::namedResourceListSize)
                 return ::BinaryData::namedResourceList[index];
            #endif
            juce::ignoreUnused (index);
            return nullptr;
        };

        const char* (*getNamedResource)(const char*, int&) = [](const char* name, int& size) -> const char* {
            #if __has_include("BinaryData.h") || __has_include(<BinaryData.h>)
             return ::BinaryData::getNamedResource (name, size);
            #else
             size = 0;
             juce::ignoreUnused (name);
             return nullptr;
            #endif
        };
    };

    /**
     * Default constructor. Uses the default BinaryDataAccessor.
     */
    ProfileManager();

    /**
     * Constructor with an injected BinaryDataAccessor.
     */
    ProfileManager (BinaryDataAccessor accessor);

    ~ProfileManager() = default;

    /**
     * Iterates the generated BinaryData profile entries by name prefix "Resources/Profiles".
     * Each entry is parsed through ProfileSchema::parse and validated against the frozen ID list.
     * Invalid profiles are skipped and recorded in the load report.
     */
    void loadBundledProfiles();

    /**
     * Scans *.json in the given directory. Same parse, validate, and skip behavior.
     * Missing or unreadable directory is not an error; it yields an empty result.
     */
    void loadUserProfiles (const juce::File& userDir);

    /**
     * Returns a copy of all validated profiles in the catalog.
     */
    std::vector<ProfileDocument> getProfiles() const;

    /**
     * Returns a pointer to a profile by name, or nullptr if not found.
     * The pointer remains valid as long as this ProfileManager is alive,
     * unless the profile is replaced by a subsequent load.
     */
    const ProfileDocument* getProfile (const juce::String& name) const;

    /**
     * Recalls a profile by name. This is the only code path permitted to replace base APVTS values.
     * Asserts it is running on the message thread (JUCE_ASSERT_MESSAGE_THREAD).
     * Applies base_parameter_values via setValueNotifyingHost after normalized conversion.
     */
    bool recallProfile (const juce::String& name, juce::AudioProcessorValueTreeState& state);

    /**
     * Returns the accumulated load report containing any parse or validation errors.
     */
    juce::StringArray getLoadReport() const;

    /**
     * Clears all entries from the catalog and the load report.
     */
    void clear();

private:
    juce::StringArray getFrozenParameterIDs();
    const ProfileDocument* getProfileInternal (const juce::String& name) const;

    BinaryDataAccessor binaryDataAccessor;
    juce::StringArray allowedParameterIDs;

    mutable juce::CriticalSection catalogMutex;
    std::vector<std::unique_ptr<ProfileDocument>> catalog;
    juce::StringArray loadReport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProfileManager)
};

} // namespace shredspace
