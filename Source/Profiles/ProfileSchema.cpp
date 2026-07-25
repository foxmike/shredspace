#include "ProfileSchema.h"
#include <cmath>
#include <algorithm>

namespace shredspace
{

struct Range
{
    float minVal = 0.0f;
    float maxVal = 0.0f;
};

static Range getParameterRange (const juce::String& paramId)
{
    // Global
    if (paramId == "GLOBAL_INPUT_GAIN")             return { -24.0f, 24.0f };
    if (paramId == "GLOBAL_OUTPUT_GAIN")            return { -24.0f, 24.0f };
    if (paramId == "GLOBAL_MIX")                    return { 0.0f, 100.0f };
    if (paramId == "GLOBAL_BYPASS")                 return { 0.0f, 1.0f };
    if (paramId == "GLOBAL_BYPASS_MODE")            return { 0.0f, 1.0f };
    if (paramId == "GLOBAL_MIX_MODE")               return { 0.0f, 1.0f };
    if (paramId == "GLOBAL_SPILLOVER")              return { 0.0f, 1.0f };
    if (paramId == "GLOBAL_SAFETY_LIMITER")         return { 0.0f, 1.0f };

    // Performance
    if (paramId == "PERF_MACRO_TIME")               return { 20.0f, 2000.0f };
    if (paramId == "PERF_MACRO_SPACE")              return { 0.0f, 100.0f };
    if (paramId == "PERF_MACRO_DEFINITION")         return { 0.0f, 100.0f };
    if (paramId == "PERF_MACRO_BLOOM")              return { 0.0f, 100.0f };
    if (paramId == "PERF_MACRO_TAIL")               return { 0.15f, 12.0f };
    if (paramId == "PERF_MACRO_FOCUS")              return { 0.0f, 100.0f };
    if (paramId == "PERF_MACRO_WIDTH")              return { 0.0f, 100.0f };
    if (paramId == "PERF_MACRO_CHARACTER")          return { 0.0f, 100.0f };
    if (paramId == "PERF_TAP")                      return { 0.0f, 1.0f };
    if (paramId == "PERF_HOLD")                     return { 0.0f, 1.0f };
    if (paramId == "PERF_HOLD_MODE")                return { 0.0f, 1.0f };
    if (paramId == "PERF_ADAPTIVE_MODE")            return { 0.0f, 2.0f };
    if (paramId == "PERF_ROUTING")                  return { 0.0f, 2.0f };
    if (paramId == "PERF_ROUTING_CASCADE")          return { 0.0f, 100.0f };

    // Timing
    if (paramId == "TIME_MODE")                     return { 0.0f, 2.0f };
    if (paramId == "TIME_SYNC_DIVISION")            return { 0.0f, 14.0f };
    if (paramId == "TIME_GLIDE")                    return { 5.0f, 1000.0f };

    // Stereo
    if (paramId == "STEREO_LF_MONO")                return { 0.0f, 1.0f };
    if (paramId == "STEREO_LF_MONO_FREQ")           return { 60.0f, 500.0f };

    // Scene
    if (paramId == "SCENE_SELECT")                  return { 0.0f, 3.0f };
    if (paramId == "SCENE_MORPH")                   return { 0.0f, 100.0f };

    // Voice A
    if (paramId == "VOICE_A_ENABLE")                return { 0.0f, 1.0f };
    if (paramId == "VOICE_A_TIME_RATIO")            return { 0.25f, 4.0f };
    if (paramId == "VOICE_A_FINE_OFFSET")           return { -50.0f, 50.0f };
    if (paramId == "VOICE_A_LEVEL")                 return { -60.0f, 0.0f };
    if (paramId == "VOICE_A_TAIL_SCALE")            return { 0.25f, 4.0f };
    if (paramId == "VOICE_A_ADAPTIVE_ROLE")         return { 0.0f, 100.0f };
    if (paramId == "VOICE_A_PAN")                   return { -100.0f, 100.0f };
    if (paramId == "VOICE_A_WIDTH")                 return { 0.0f, 200.0f };
    if (paramId == "VOICE_A_STEREO_OFFSET")         return { 0.0f, 30.0f };
    if (paramId == "VOICE_A_CROSS_FEEDBACK")        return { 0.0f, 100.0f };
    if (paramId == "VOICE_A_MOD_PHASE")             return { 0.0f, 180.0f };
    if (paramId == "VOICE_A_HPF")                   return { 40.0f, 1500.0f };
    if (paramId == "VOICE_A_LPF")                   return { 2000.0f, 20000.0f };
    if (paramId == "VOICE_A_PRESENCE_FREQ")         return { 1500.0f, 6000.0f };
    if (paramId == "VOICE_A_PRESENCE_GUARD")        return { 0.0f, 12.0f };
    if (paramId == "VOICE_A_ATTACK_SOFTEN")         return { 0.0f, 100.0f };
    if (paramId == "VOICE_A_MOD_RATE")              return { 0.05f, 5.0f };
    if (paramId == "VOICE_A_MOD_DEPTH")             return { 0.0f, 8.0f };
    if (paramId == "VOICE_A_DRIFT")                 return { 0.0f, 100.0f };
    if (paramId == "VOICE_A_DIFFUSION")             return { 0.0f, 40.0f };
    if (paramId == "VOICE_A_SATURATION")            return { 0.0f, 100.0f };

    // Voice B
    if (paramId == "VOICE_B_ENABLE")                return { 0.0f, 1.0f };
    if (paramId == "VOICE_B_TIME_RATIO")            return { 0.25f, 4.0f };
    if (paramId == "VOICE_B_FINE_OFFSET")           return { -50.0f, 50.0f };
    if (paramId == "VOICE_B_LEVEL")                 return { -60.0f, 0.0f };
    if (paramId == "VOICE_B_TAIL_SCALE")            return { 0.25f, 4.0f };
    if (paramId == "VOICE_B_ADAPTIVE_ROLE")         return { 0.0f, 100.0f };
    if (paramId == "VOICE_B_PAN")                   return { -100.0f, 100.0f };
    if (paramId == "VOICE_B_WIDTH")                 return { 0.0f, 200.0f };
    if (paramId == "VOICE_B_STEREO_OFFSET")         return { 0.0f, 30.0f };
    if (paramId == "VOICE_B_CROSS_FEEDBACK")        return { 0.0f, 100.0f };
    if (paramId == "VOICE_B_MOD_PHASE")             return { 0.0f, 180.0f };
    if (paramId == "VOICE_B_HPF")                   return { 40.0f, 1500.0f };
    if (paramId == "VOICE_B_LPF")                   return { 2000.0f, 20000.0f };
    if (paramId == "VOICE_B_PRESENCE_FREQ")         return { 1500.0f, 6000.0f };
    if (paramId == "VOICE_B_PRESENCE_GUARD")        return { 0.0f, 12.0f };
    if (paramId == "VOICE_B_ATTACK_SOFTEN")         return { 0.0f, 100.0f };
    if (paramId == "VOICE_B_MOD_RATE")              return { 0.05f, 5.0f };
    if (paramId == "VOICE_B_MOD_DEPTH")             return { 0.0f, 8.0f };
    if (paramId == "VOICE_B_DRIFT")                 return { 0.0f, 100.0f };
    if (paramId == "VOICE_B_DIFFUSION")             return { 0.0f, 40.0f };
    if (paramId == "VOICE_B_SATURATION")            return { 0.0f, 100.0f };

    // Intelligence
    if (paramId == "INTEL_SENSITIVITY")             return { 0.0f, 100.0f };
    if (paramId == "INTEL_INPUT_GATE")              return { -80.0f, -20.0f };
    if (paramId == "INTEL_DENSITY_WINDOW")          return { 200.0f, 1000.0f };
    if (paramId == "INTEL_DENSITY_START")           return { 1.0f, 8.0f };
    if (paramId == "INTEL_DENSITY_FULL")            return { 6.0f, 20.0f };
    if (paramId == "INTEL_SUSTAIN_START")           return { 50.0f, 800.0f };
    if (paramId == "INTEL_SUSTAIN_FULL")            return { 150.0f, 2000.0f };
    if (paramId == "INTEL_PHRASE_GAP")              return { 20.0f, 500.0f };
    if (paramId == "INTEL_CONTRACTION_ATTACK")      return { 1.0f, 100.0f };
    if (paramId == "INTEL_CONTRACTION_RELEASE")     return { 20.0f, 1000.0f };
    if (paramId == "INTEL_WET_REDUCTION")           return { 0.0f, 18.0f };
    if (paramId == "INTEL_TAIL_REDUCTION")          return { 0.0f, 2.0f };
    if (paramId == "INTEL_TONE_DARKENING")          return { 0.0f, 2.0f };
    if (paramId == "INTEL_WIDTH_REDUCTION")         return { 0.0f, 100.0f };
    if (paramId == "INTEL_ATTACK_SOFTEN_INCREASE")  return { 0.0f, 100.0f };
    if (paramId == "INTEL_BLOOM_RISE")              return { 20.0f, 1500.0f };
    if (paramId == "INTEL_BLOOM_HOLD")              return { 0.0f, 2000.0f };
    if (paramId == "INTEL_BLOOM_RELEASE")           return { 50.0f, 3000.0f };
    if (paramId == "INTEL_WET_LIFT")                return { 0.0f, 12.0f };
    if (paramId == "INTEL_TAIL_EXPANSION")          return { 0.0f, 2.0f };
    if (paramId == "INTEL_TONE_OPENING")            return { 0.0f, 1.0f };
    if (paramId == "INTEL_WIDTH_EXPANSION")         return { 0.0f, 100.0f };
    if (paramId == "INTEL_DIFFUSION_INCREASE")      return { 0.0f, 30.0f };
    if (paramId == "INTEL_TRANSIENT_CROSSOVER")     return { 1000.0f, 6000.0f };

    return { 0.0f, 0.0f };
}

static juce::var getPropertyCaseInsensitiveOrAlternative (const juce::var& obj, const juce::String& snake, const juce::String& camel)
{
    if (obj.hasProperty (snake))
        return obj.getProperty (snake, juce::var());
    if (obj.hasProperty (camel))
        return obj.getProperty (camel, juce::var());
    return juce::var();
}

static bool hasPropertyAlternative (const juce::DynamicObject& obj, const juce::String& snake, const juce::String& camel)
{
    return obj.hasProperty (snake) || obj.hasProperty (camel);
}

static bool parseMixTarget (const juce::var& sceneVar, MixTarget& targetOut, ProfileValidationResult& result, const juce::StringArray& allowedParameterIDs)
{
    auto targetVar = getPropertyCaseInsensitiveOrAlternative (sceneVar, "mix_target", "mixTarget");
    if (targetVar.isVoid())
    {
        auto paramIdVar = getPropertyCaseInsensitiveOrAlternative (sceneVar, "mix_parameter_id", "mixParameterId");
        auto valVar = getPropertyCaseInsensitiveOrAlternative (sceneVar, "mix_value", "mixValue");
        if (paramIdVar.isVoid() || valVar.isVoid())
        {
            result.ok = false;
            result.errors.add ("scene missing mix_target");
            return false;
        }
        targetOut.parameter_id = paramIdVar.toString();
        if (! (valVar.isDouble() || valVar.isInt() || valVar.isInt64()))
        {
            result.ok = false;
            result.errors.add ("mix_value must be numeric");
            return false;
        }
        targetOut.value = (float) valVar;
    }
    else
    {
        if (! targetVar.isObject())
        {
            result.ok = false;
            result.errors.add ("mix_target must be an object");
            return false;
        }

        auto* destObj = targetVar.getDynamicObject();
        if (destObj == nullptr)
        {
            result.ok = false;
            result.errors.add ("mix_target object is null");
            return false;
        }

        auto paramIdVar = getPropertyCaseInsensitiveOrAlternative (targetVar, "parameter_id", "parameterId");
        if (paramIdVar.isVoid())
            paramIdVar = getPropertyCaseInsensitiveOrAlternative (targetVar, "destination_parameter_id", "destinationParameterId");

        auto valVar = targetVar.getProperty ("value", juce::var());

        if (paramIdVar.isVoid() || valVar.isVoid())
        {
            result.ok = false;
            result.errors.add ("mix_target missing parameter_id or value");
            return false;
        }

        targetOut.parameter_id = paramIdVar.toString();
        if (! (valVar.isDouble() || valVar.isInt() || valVar.isInt64()))
        {
            result.ok = false;
            result.errors.add ("mix_target value must be numeric");
            return false;
        }
        targetOut.value = (float) valVar;
    }

    if (! allowedParameterIDs.contains (targetOut.parameter_id))
    {
        result.migration_notes.add ("dropped unknown parameter id: " + targetOut.parameter_id);
        targetOut.parameter_id = "";
        targetOut.value = 0.0f;
    }

    return true;
}

static bool parseRoutingTarget (const juce::var& sceneVar, RoutingTarget& targetOut, ProfileValidationResult& result, const juce::StringArray& allowedParameterIDs)
{
    auto targetVar = getPropertyCaseInsensitiveOrAlternative (sceneVar, "routing_target", "routingTarget");
    if (targetVar.isVoid())
    {
        auto paramIdVar = getPropertyCaseInsensitiveOrAlternative (sceneVar, "routing_parameter_id", "routingParameterId");
        auto valVar = getPropertyCaseInsensitiveOrAlternative (sceneVar, "routing_value", "routingValue");
        if (paramIdVar.isVoid() || valVar.isVoid())
        {
            result.ok = false;
            result.errors.add ("scene missing routing_target");
            return false;
        }
        targetOut.parameter_id = paramIdVar.toString();
        if (! (valVar.isDouble() || valVar.isInt() || valVar.isInt64()))
        {
            result.ok = false;
            result.errors.add ("routing_value must be numeric");
            return false;
        }
        targetOut.value = (float) valVar;
    }
    else
    {
        if (! targetVar.isObject())
        {
            result.ok = false;
            result.errors.add ("routing_target must be an object");
            return false;
        }

        auto* destObj = targetVar.getDynamicObject();
        if (destObj == nullptr)
        {
            result.ok = false;
            result.errors.add ("routing_target object is null");
            return false;
        }

        auto paramIdVar = getPropertyCaseInsensitiveOrAlternative (targetVar, "parameter_id", "parameterId");
        if (paramIdVar.isVoid())
            paramIdVar = getPropertyCaseInsensitiveOrAlternative (targetVar, "destination_parameter_id", "destinationParameterId");

        auto valVar = targetVar.getProperty ("value", juce::var());

        if (paramIdVar.isVoid() || valVar.isVoid())
        {
            result.ok = false;
            result.errors.add ("routing_target missing parameter_id or value");
            return false;
        }

        targetOut.parameter_id = paramIdVar.toString();
        if (! (valVar.isDouble() || valVar.isInt() || valVar.isInt64()))
        {
            result.ok = false;
            result.errors.add ("routing_target value must be numeric");
            return false;
        }
        targetOut.value = (float) valVar;
    }

    if (! allowedParameterIDs.contains (targetOut.parameter_id))
    {
        result.migration_notes.add ("dropped unknown parameter id: " + targetOut.parameter_id);
        targetOut.parameter_id = "";
        targetOut.value = 0.0f;
    }

    return true;
}

std::pair<ProfileDocument, ProfileValidationResult> ProfileSchema::parse (const juce::var& json, const juce::StringArray& allowedParameterIDs)
{
    ProfileValidationResult result;
    result.ok = true;

    if (! json.isObject())
    {
        result.ok = false;
        result.errors.add ("JSON root is not an object");
        return { ProfileDocument(), result };
    }

    ProfileDocument doc;

    // 1. schema_version
    if (! json.hasProperty ("schema_version") && ! json.hasProperty ("schemaVersion"))
    {
        result.ok = false;
        result.errors.add ("Missing schema_version");
        return { ProfileDocument(), result };
    }
    auto schemaVar = getPropertyCaseInsensitiveOrAlternative (json, "schema_version", "schemaVersion");
    if (! (schemaVar.isInt() || schemaVar.isInt64()))
    {
        result.ok = false;
        result.errors.add ("schema_version must be an integer");
        return { ProfileDocument(), result };
    }
    int schemaVersion = (int) schemaVar;
    if (schemaVersion < 1 || schemaVersion > kCurrentSchemaVersion)
    {
        result.ok = false;
        result.errors.add ("schema_version is out of range");
        return { ProfileDocument(), result };
    }
    doc.schema_version = schemaVersion;

    // Metadata strings
    doc.product_version = getPropertyCaseInsensitiveOrAlternative (json, "product_version", "productVersion").toString();
    doc.profile_name = getPropertyCaseInsensitiveOrAlternative (json, "profile_name", "profileName").toString();
    doc.author = getPropertyCaseInsensitiveOrAlternative (json, "author", "author").toString();
    doc.description = getPropertyCaseInsensitiveOrAlternative (json, "description", "description").toString();

    // tags (StringArray)
    auto tagsVar = getPropertyCaseInsensitiveOrAlternative (json, "tags", "tags");
    if (! tagsVar.isVoid())
    {
        if (tagsVar.isArray())
        {
            auto* arr = tagsVar.getArray();
            if (arr != nullptr)
            {
                for (const auto& tag : *arr)
                {
                    doc.tags.add (tag.toString());
                }
            }
        }
        else
        {
            result.ok = false;
            result.errors.add ("tags must be an array");
            return { ProfileDocument(), result };
        }
    }

    // base_parameter_values
    auto baseParamsVar = getPropertyCaseInsensitiveOrAlternative (json, "base_parameter_values", "baseParameterValues");
    if (! baseParamsVar.isVoid())
    {
        if (! baseParamsVar.isObject())
        {
            result.ok = false;
            result.errors.add ("base_parameter_values must be an object");
            return { ProfileDocument(), result };
        }

        auto* obj = baseParamsVar.getDynamicObject();
        if (obj != nullptr)
        {
            for (const auto& prop : obj->getProperties())
            {
                auto paramId = prop.name.toString();
                auto valVar = prop.value;

                if (! (valVar.isDouble() || valVar.isInt() || valVar.isInt64()))
                {
                    result.ok = false;
                    result.errors.add ("base_parameter_values value for " + paramId + " must be numeric");
                    return { ProfileDocument(), result };
                }

                float val = (float) valVar;

                if (! allowedParameterIDs.contains (paramId))
                {
                    result.migration_notes.add ("dropped unknown parameter id: " + paramId);
                    continue;
                }

                Range range = getParameterRange (paramId);
                float clampedVal = std::clamp (val, range.minVal, range.maxVal);
                doc.base_parameter_values[paramId] = clampedVal;
            }
        }
    }

    // macro_mapping
    auto macroMappingVar = getPropertyCaseInsensitiveOrAlternative (json, "macro_mapping", "macroMapping");
    if (! macroMappingVar.isVoid())
    {
        if (! macroMappingVar.isObject())
        {
            result.ok = false;
            result.errors.add ("macro_mapping must be an object");
            return { ProfileDocument(), result };
        }

        auto* obj = macroMappingVar.getDynamicObject();
        if (obj == nullptr)
        {
            result.ok = false;
            result.errors.add ("macro_mapping object is null");
            return { ProfileDocument(), result };
        }

        const auto& properties = obj->getProperties();
        if (properties.size() != 8)
        {
            result.ok = false;
            result.errors.add ("macro_mapping must contain exactly eight macro entries");
            return { ProfileDocument(), result };
        }

        for (const auto& macroName : kMacroNames)
        {
            if (! obj->hasProperty (macroName))
            {
                result.ok = false;
                result.errors.add ("macro_mapping is missing entry for macro: " + juce::String (macroName));
                return { ProfileDocument(), result };
            }
        }

        for (const auto& macroName : kMacroNames)
        {
            auto destsVar = obj->getProperty (macroName);
            if (! destsVar.isArray())
            {
                result.ok = false;
                result.errors.add ("macro_mapping entry for " + juce::String (macroName) + " must be an array");
                return { ProfileDocument(), result };
            }

            auto* destArray = destsVar.getArray();
            std::vector<MacroDestination> destinations;

            if (destArray != nullptr)
            {
                for (const auto& destVar : *destArray)
                {
                    if (! destVar.isObject())
                    {
                        result.ok = false;
                        result.errors.add ("macro destination entry must be an object");
                        return { ProfileDocument(), result };
                    }

                    auto* destObj = destVar.getDynamicObject();
                    if (destObj == nullptr)
                    {
                        result.ok = false;
                        result.errors.add ("macro destination object is null");
                        return { ProfileDocument(), result };
                    }

                    if (! hasPropertyAlternative (*destObj, "destination_parameter_id", "destinationParameterId"))
                    {
                        result.ok = false;
                        result.errors.add ("macro destination missing destination_parameter_id");
                        return { ProfileDocument(), result };
                    }

                    auto destParamId = getPropertyCaseInsensitiveOrAlternative (destVar, "destination_parameter_id", "destinationParameterId").toString();

                    auto minVar = getPropertyCaseInsensitiveOrAlternative (destVar, "delta_min", "deltaMin");
                    auto maxVar = getPropertyCaseInsensitiveOrAlternative (destVar, "delta_max", "deltaMax");

                    if (! (minVar.isDouble() || minVar.isInt() || minVar.isInt64()) ||
                        ! (maxVar.isDouble() || maxVar.isInt() || maxVar.isInt64()))
                    {
                        result.ok = false;
                        result.errors.add ("delta_min and delta_max must be numeric");
                        return { ProfileDocument(), result };
                    }

                    float delta_min = (float) minVar;
                    float delta_max = (float) maxVar;

                    if (! std::isfinite (delta_min) || ! std::isfinite (delta_max))
                    {
                        result.ok = false;
                        result.errors.add ("delta_min and delta_max must be finite");
                        return { ProfileDocument(), result };
                    }

                    float curve = 1.0f;
                    if (hasPropertyAlternative (*destObj, "curve", "curve"))
                    {
                        auto curveVar = getPropertyCaseInsensitiveOrAlternative (destVar, "curve", "curve");
                        if (! (curveVar.isDouble() || curveVar.isInt() || curveVar.isInt64()))
                        {
                            result.ok = false;
                            result.errors.add ("curve must be numeric");
                            return { ProfileDocument(), result };
                        }
                        curve = (float) curveVar;
                        if (curve <= 0.0f)
                        {
                            result.ok = false;
                            result.errors.add ("curve must be greater than zero");
                            return { ProfileDocument(), result };
                        }
                    }

                    if (! allowedParameterIDs.contains (destParamId))
                    {
                        result.migration_notes.add ("dropped unknown parameter id: " + destParamId);
                        continue;
                    }

                    MacroDestination md;
                    md.destination_parameter_id = destParamId;
                    md.delta_min = delta_min;
                    md.delta_max = delta_max;
                    md.curve = curve;
                    destinations.push_back (md);
                }
            }

            doc.macro_mapping[macroName] = destinations;
        }
    }

    // scene_values
    auto sceneValuesVar = getPropertyCaseInsensitiveOrAlternative (json, "scene_values", "sceneValues");
    if (sceneValuesVar.isVoid())
    {
        result.ok = false;
        result.errors.add ("Missing scene_values");
        return { ProfileDocument(), result };
    }

    std::vector<juce::var> sceneVars;

    if (sceneValuesVar.isArray())
    {
        auto* arr = sceneValuesVar.getArray();
        if (arr != nullptr)
        {
            for (const auto& item : *arr)
                sceneVars.push_back (item);
        }
    }
    else if (sceneValuesVar.isObject())
    {
        auto* obj = sceneValuesVar.getDynamicObject();
        if (obj != nullptr)
        {
            for (const auto& prop : obj->getProperties())
                sceneVars.push_back (prop.value);
        }
    }
    else
    {
        result.ok = false;
        result.errors.add ("scene_values must be an array or an object");
        return { ProfileDocument(), result };
    }

    if (sceneVars.size() != 4)
    {
        result.ok = false;
        result.errors.add ("scene_values must contain exactly four scenes");
        return { ProfileDocument(), result };
    }

    for (const auto& sceneVar : sceneVars)
    {
        if (! sceneVar.isObject())
        {
            result.ok = false;
            result.errors.add ("scene entry must be an object");
            return { ProfileDocument(), result };
        }

        SceneValue sv;

        // macro targets
        auto macroTargetsVar = getPropertyCaseInsensitiveOrAlternative (sceneVar, "macro_targets", "macroTargets");
        for (const auto& macroName : kMacroNames)
        {
            juce::var macroValVar;
            if (! macroTargetsVar.isVoid() && macroTargetsVar.isObject())
                macroValVar = macroTargetsVar.getProperty (macroName, juce::var());
            else
                macroValVar = sceneVar.getProperty (macroName, juce::var());

            if (macroValVar.isVoid() || ! (macroValVar.isDouble() || macroValVar.isInt() || macroValVar.isInt64()))
            {
                result.ok = false;
                result.errors.add ("scene macro target for " + juce::String (macroName) + " must be numeric");
                return { ProfileDocument(), result };
            }
            sv.macro_targets[macroName] = (float) macroValVar;
        }

        // Mix target
        if (! parseMixTarget (sceneVar, sv.mix_target, result, allowedParameterIDs))
            return { ProfileDocument(), result };

        // routing target
        if (! parseRoutingTarget (sceneVar, sv.routing_target, result, allowedParameterIDs))
            return { ProfileDocument(), result };

        // Hold behavior
        auto holdVar = getPropertyCaseInsensitiveOrAlternative (sceneVar, "hold_behavior", "holdBehavior");
        if (holdVar.isVoid())
            holdVar = sceneVar.getProperty ("hold", juce::var());

        if (holdVar.isVoid())
        {
            result.ok = false;
            result.errors.add ("scene missing hold_behavior");
            return { ProfileDocument(), result };
        }

        if (holdVar.isInt() || holdVar.isInt64())
        {
            int val = (int) holdVar;
            if (val == 0)
                sv.hold_behavior = HoldBehavior::Momentary;
            else if (val == 1)
                sv.hold_behavior = HoldBehavior::Latch;
            else
            {
                result.ok = false;
                result.errors.add ("invalid hold_behavior value");
                return { ProfileDocument(), result };
            }
        }
        else if (holdVar.isString())
        {
            auto valStr = holdVar.toString();
            if (valStr.equalsIgnoreCase ("Momentary") || valStr == "0")
                sv.hold_behavior = HoldBehavior::Momentary;
            else if (valStr.equalsIgnoreCase ("Latch") || valStr == "1")
                sv.hold_behavior = HoldBehavior::Latch;
            else
            {
                result.ok = false;
                result.errors.add ("invalid hold_behavior string");
                return { ProfileDocument(), result };
            }
        }
        else
        {
            result.ok = false;
            result.errors.add ("hold_behavior must be an integer or string");
            return { ProfileDocument(), result };
        }

        doc.scene_values.push_back (sv);
    }

    // Call validate as a final check to ensure all values conform
    auto valResult = validate (doc, allowedParameterIDs);
    if (! valResult.ok)
    {
        result.ok = false;
        for (const auto& err : valResult.errors)
            result.errors.add (err);
        return { ProfileDocument(), result };
    }

    return { doc, result };
}

ProfileValidationResult ProfileSchema::validate (const ProfileDocument& doc, const juce::StringArray& allowedParameterIDs)
{
    ProfileValidationResult result;
    result.ok = true;

    // 1. schema_version check
    if (doc.schema_version < 1 || doc.schema_version > kCurrentSchemaVersion)
    {
        result.ok = false;
        result.errors.add ("schema_version must be between 1 and " + juce::String (kCurrentSchemaVersion));
    }

    // 2. base_parameter_values existence check
    for (const auto& pair : doc.base_parameter_values)
    {
        const auto& paramId = pair.first;
        if (! allowedParameterIDs.contains (paramId))
        {
            result.ok = false;
            result.errors.add ("unknown parameter ID in base_parameter_values: " + paramId);
        }
    }

    // 3. macro_mapping check
    if (! doc.macro_mapping.empty())
    {
        if (doc.macro_mapping.size() != 8)
        {
            result.ok = false;
            result.errors.add ("macro_mapping must contain exactly eight macro entries");
        }

        for (const auto& macroName : kMacroNames)
        {
            auto it = doc.macro_mapping.find (macroName);
            if (it == doc.macro_mapping.end())
            {
                result.ok = false;
                result.errors.add ("macro_mapping is missing entry for macro: " + juce::String (macroName));
            }
            else
            {
                for (const auto& dest : it->second)
                {
                    if (! allowedParameterIDs.contains (dest.destination_parameter_id))
                    {
                        result.ok = false;
                        result.errors.add ("unknown parameter ID in macro destination: " + dest.destination_parameter_id);
                    }
                    if (! std::isfinite (dest.delta_min))
                    {
                        result.ok = false;
                        result.errors.add ("delta_min must be finite");
                    }
                    if (! std::isfinite (dest.delta_max))
                    {
                        result.ok = false;
                        result.errors.add ("delta_max must be finite");
                    }
                    if (dest.curve <= 0.0f)
                    {
                        result.ok = false;
                        result.errors.add ("curve must be greater than zero");
                    }
                }
            }
        }
    }

    // 4. scene_values check
    if (doc.scene_values.size() != 4)
    {
        result.ok = false;
        result.errors.add ("scene_values must contain exactly four scenes");
    }
    else
    {
        for (size_t i = 0; i < doc.scene_values.size(); ++i)
        {
            const auto& scene = doc.scene_values[i];

            for (const auto& macroName : kMacroNames)
            {
                if (scene.macro_targets.find (macroName) == scene.macro_targets.end())
                {
                    result.ok = false;
                    result.errors.add ("scene " + juce::String ((int)i) + " missing macro target: " + juce::String (macroName));
                }
            }

            if (scene.mix_target.parameter_id.isNotEmpty() && ! allowedParameterIDs.contains (scene.mix_target.parameter_id))
            {
                result.ok = false;
                result.errors.add ("scene " + juce::String ((int)i) + " unknown parameter ID in mix_target: " + scene.mix_target.parameter_id);
            }

            if (scene.routing_target.parameter_id.isNotEmpty() && ! allowedParameterIDs.contains (scene.routing_target.parameter_id))
            {
                result.ok = false;
                result.errors.add ("scene " + juce::String ((int)i) + " unknown parameter ID in routing_target: " + scene.routing_target.parameter_id);
            }
        }
    }

    return result;
}

} // namespace shredspace
