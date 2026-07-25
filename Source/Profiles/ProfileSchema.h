#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>
#include <utility>

namespace shredspace
{

enum class HoldBehavior
{
    Momentary,
    Latch
};

struct MacroDestination
{
    juce::String destination_parameter_id;
    float delta_min = 0.0f;
    float delta_max = 0.0f;
    float curve = 1.0f;
};

struct MixTarget
{
    juce::String parameter_id;
    float value = 0.0f;
};

struct RoutingTarget
{
    juce::String parameter_id;
    float value = 0.0f;
};

struct SceneValue
{
    std::map<juce::String, float> macro_targets;
    MixTarget mix_target;
    RoutingTarget routing_target;
    HoldBehavior hold_behavior = HoldBehavior::Momentary;
};

struct ProfileDocument
{
    int schema_version = 0;
    juce::String product_version;
    juce::String profile_name;
    juce::String author;
    juce::String description;
    std::map<juce::String, float> base_parameter_values;
    std::map<juce::String, std::vector<MacroDestination>> macro_mapping;
    std::vector<SceneValue> scene_values;
    juce::StringArray tags;
};

struct ProfileValidationResult
{
    bool ok = false;
    juce::StringArray errors;
    juce::StringArray migration_notes;
};

inline constexpr int kCurrentSchemaVersion = 1;

inline constexpr const char* kMacroNames[] = {
    "TIME", "SPACE", "DEFINITION", "BLOOM", "TAIL", "FOCUS", "WIDTH", "CHARACTER"
};

namespace ProfileSchema
{
    std::pair<ProfileDocument, ProfileValidationResult> parse (const juce::var& json, const juce::StringArray& allowedParameterIDs);
    ProfileValidationResult validate (const ProfileDocument& doc, const juce::StringArray& allowedParameterIDs);
}

} // namespace shredspace
