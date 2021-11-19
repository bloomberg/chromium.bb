// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_features.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"

namespace chromecast {
namespace {
// A constant used to always activate a FieldTrial.
const base::FieldTrial::Probability k100PercentProbability = 100;

// The name of the default group to use for Cast DCS features.
const char kDefaultDCSFeaturesGroup[] = "default_dcs_features_group";

std::unordered_set<int32_t>& GetExperimentIds() {
  static base::NoDestructor<std::unordered_set<int32_t>> g_experiment_ids;
  return *g_experiment_ids;
}

bool g_experiment_ids_initialized = false;

// The collection of features that have been registered by unit tests
std::vector<const base::Feature*>& GetTestFeatures() {
  static base::NoDestructor<std::vector<const base::Feature*>>
      features_for_test;
  return *features_for_test;
}

void SetExperimentIds(const base::Value& list) {
  DCHECK(!g_experiment_ids_initialized);
  DCHECK(list.is_list());
  std::unordered_set<int32_t> ids;
  for (const auto& it : list.GetList()) {
    if (it.is_int()) {
      ids.insert(it.GetInt());
    } else {
      LOG(ERROR) << "Non-integer value found in experiment id list!";
    }
  }
  GetExperimentIds().swap(ids);
  g_experiment_ids_initialized = true;
}

}  // namespace

// PLEASE READ!
// Cast Platform Features are listed below. These features may be
// toggled via configs fetched from DCS for devices in the field, or via
// command-line flags set by the developer. For the end-to-end details of the
// system design, please see go/dcs-experiments.
//
// Below are useful steps on how to use these features in your code.
//
// 1) Declaring and defining the feature.
//    All Cast Platform Features should be declared in a common file with other
//    Cast Platform Features (ex. chromecast/base/cast_features.h). When
//    defining your feature, you will need to assign a default value. This is
//    the value that the feature will hold until overriden by the server or the
//    command line. Here's an exmaple:
//
//      const base::Feature kSuperSecretSauce{
//          "enable_super_secret_sauce", base::FEATURE_DISABLED_BY_DEFAULT};
//
//    IMPORTANT NOTE:
//    The first parameter that you pass in the definition is the feature's name.
//    This MUST match the DCS experiment key for this feature.
//
//    While Features elsewhere in Chromium alternatively use dashed-case or
//    PascalCase for their names, Chromecast features should use snake_case
//    (lowercase letters separated by underscores). This will ensure that DCS
//    configs, which are passed around as JSON, remain conformant and readable.
//
// 2) Using the feature in client code.
//    Using these features in your code is easy. Here's an example:
//
//      #include “base/feature_list.h”
//      #include “chromecast/base/chromecast_switches.h”
//
//      std::unique_ptr<Foo> CreateFoo() {
//        if (base::FeatureList::IsEnabled(kSuperSecretSauce))
//          return std::make_unique<SuperSecretFoo>();
//        return std::make_unique<BoringOldFoo>();
//      }
//
//    base::FeatureList can be called from any thread, in any process, at any
//    time after PreCreateThreads(). It will return whether the feature is
//    enabled.
//
// 3) Overriding the default value from the server.
//    For devices in the field, DCS will issue different configs to different
//    groups of devices, allowing us to run experiments on features. These
//    feature settings will manifest on the next boot of cast_shell. In the
//    example, if the latest config for a particular device set the value of
//    kSuperSecretSauce to true, the appropriate code path would be taken.
//    Otherwise, the default value, false, would be used. For more details on
//    setting up experiments, see go/dcs-launch.
//
// 4) Overriding the default and server values from the command-line.
//    While the server value trumps the default values, the command line trumps
//    both. Enable features by passing this command line arg to cast_shell:
//
//      --enable-features=enable_foo,enable_super_secret_sauce
//
//    Features are separated by commas. Disable features by passing:
//
//      --disable-features=enable_foo,enable_bar
//
// 5) If you add a new feature to the system you must include it in kFeatures
//    This is because the system relies on knowing all of the features so
//    it can properly iterate over all features to detect changes.
//

// Begin Chromecast Feature definitions.

// Allows applications to access media capture devices (webcams/microphones)
// through getUserMedia API.
const base::Feature kAllowUserMediaAccess{"allow_user_media_access",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
// Enables the use of QUIC in Cast-specific NetworkContexts. See
// chromecast/browser/cast_network_contexts.cc for usage.
const base::Feature kEnableQuic{"enable_quic",
                                base::FEATURE_DISABLED_BY_DEFAULT};
// Enables triple-buffer 720p graphics (overriding default graphics buffer
// settings for a platform).
const base::Feature kTripleBuffer720{"enable_triple_buffer_720",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
// Enables single-buffered graphics (overriding default graphics buffer
// settings and takes precedence over triple-buffer feature).
const base::Feature kSingleBuffer{"enable_single_buffer",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
// Disable idle sockets closing on memory pressure. See
// chromecast/browser/cast_network_contexts.cc for usage.
const base::Feature kDisableIdleSocketsCloseOnMemoryPressure{
    "disable_idle_sockets_close_on_memory_pressure",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableGeneralAudienceBrowsing{
    "enable_general_audience_browsing", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableSideGesturePassThrough{
    "enable_side_gesture_pass_through", base::FEATURE_DISABLED_BY_DEFAULT};

// Uses AudioManagerAndroid, instead of CastAudioManagerAndroid. This will
// disable lots of Cast features, so it should only be used for development and
// testing.
const base::Feature kEnableChromeAudioManagerAndroid{
    "enable_chrome_audio_manager_android", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables CastAudioOutputDevice for audio output on Android. When disabled,
// CastAudioManagerAndroid will be used.
const base::Feature kEnableCastAudioOutputDevice{
    "enable_cast_audio_output_device", base::FEATURE_DISABLED_BY_DEFAULT};

// End Chromecast Feature definitions.
const base::Feature* kFeatures[] = {
    &kAllowUserMediaAccess,
    &kEnableQuic,
    &kTripleBuffer720,
    &kSingleBuffer,
    &kDisableIdleSocketsCloseOnMemoryPressure,
    &kEnableGeneralAudienceBrowsing,
    &kEnableSideGesturePassThrough,
    &kEnableChromeAudioManagerAndroid,
    &kEnableCastAudioOutputDevice,
};

std::vector<const base::Feature*> GetInternalFeatures();

const std::vector<const base::Feature*>& GetFeatures() {
  static const base::NoDestructor<std::vector<const base::Feature*>> features(
      [] {
        std::vector<const base::Feature*> features(std::begin(kFeatures),
                                                   std::end(kFeatures));
        auto internal_features = GetInternalFeatures();
        features.insert(features.end(), internal_features.begin(),
                        internal_features.end());
        return features;
      }());
  if (GetTestFeatures().size() > 0)
    return GetTestFeatures();
  return *features;
}

void InitializeFeatureList(const base::Value& dcs_features,
                           const base::Value& dcs_experiment_ids,
                           const std::string& cmd_line_enable_features,
                           const std::string& cmd_line_disable_features,
                           const std::string& extra_enable_features,
                           const std::string& extra_disable_features) {
  DCHECK(!base::FeatureList::GetInstance());
  DCHECK(dcs_features.is_dict());
  DCHECK(dcs_experiment_ids.is_list());

  // Set the experiments.
  SetExperimentIds(dcs_experiment_ids);

  std::string all_enable_features =
      cmd_line_enable_features + "," + extra_enable_features;
  std::string all_disable_features =
      cmd_line_disable_features + "," + extra_disable_features;

  // Initialize the FeatureList from the command line.
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->InitializeFromCommandLine(all_enable_features,
                                          all_disable_features);

  // Override defaults from the DCS config.
  for (const auto kv : dcs_features.DictItems()) {
    // Each feature must have its own FieldTrial object. Since experiments are
    // controlled server-side for Chromecast, and this class is designed with a
    // client-side experimentation framework in mind, these parameters are
    // carefully chosen:
    //   - The field trial name is unused for our purposes. However, we need to
    //     maintain a 1:1 mapping with Features in order to properly store and
    //     access parameters associated with each Feature. Therefore, use the
    //     Feature's name as the FieldTrial name to ensure uniqueness.
    //   - The probability is hard-coded to 100% so that the FeatureList always
    //     respects the value from DCS.
    //   - The default group is unused; it will be the same for every feature.
    //   - SESSION_RANDOMIZED is used to prevent the need for an
    //     entropy_provider. However, this value doesn't matter.
    //   - We don't care about the group_id.
    //
    const std::string& feature_name = kv.first;
    auto* field_trial = base::FieldTrialList::FactoryGetFieldTrial(
        feature_name, k100PercentProbability, kDefaultDCSFeaturesGroup,
        base::FieldTrial::SESSION_RANDOMIZED, nullptr);

    if (kv.second.is_bool()) {
      // A boolean entry simply either enables or disables a feature.
      feature_list->RegisterFieldTrialOverride(
          feature_name,
          kv.second.GetBool() ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                              : base::FeatureList::OVERRIDE_DISABLE_FEATURE,
          field_trial);
      continue;
    }

    if (kv.second.is_dict()) {
      // A dictionary entry implies that the feature is enabled.
      feature_list->RegisterFieldTrialOverride(
          feature_name, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
          field_trial);

      // If the feature has not been overriden from the command line, set its
      // parameters accordingly.
      if (!feature_list->IsFeatureOverriddenFromCommandLine(
              feature_name, base::FeatureList::OVERRIDE_DISABLE_FEATURE)) {
        // Build a map of the FieldTrial parameters and associate it to the
        // FieldTrial.
        base::FieldTrialParams params;
        for (const auto params_kv : kv.second.DictItems()) {
          if (params_kv.second.is_string()) {
            params[params_kv.first] = params_kv.second.GetString();
          } else {
            LOG(ERROR) << "Entry in params dict for \"" << feature_name << "\""
                       << " feature is not a string. Skipping.";
          }
        }

        // Register the params, so that they can be queried by client code.
        bool success = base::AssociateFieldTrialParams(
            feature_name, kDefaultDCSFeaturesGroup, params);
        DCHECK(success);
      }
      continue;
    }

    // Other base::Value types are not supported.
    LOG(ERROR) << "A DCS feature mapped to an unsupported value. key: "
               << feature_name << " type: " << kv.second.type();
  }

  base::FeatureList::SetInstance(std::move(feature_list));
}

bool IsFeatureEnabled(const base::Feature& feature) {
  DCHECK(base::Contains(GetFeatures(), &feature)) << feature.name;
  return base::FeatureList::IsEnabled(feature);
}

base::Value GetOverriddenFeaturesForStorage(const base::Value& features) {
  base::Value persistent_dict(base::Value::Type::DICTIONARY);

  // |features| maps feature names to either a boolean or a dict of params.
  for (const auto feature : features.DictItems()) {
    if (feature.second.is_bool()) {
      persistent_dict.SetBoolKey(feature.first, feature.second.GetBool());
      continue;
    }

    if (feature.second.is_dict()) {
      const base::Value* params_dict = &feature.second;
      base::Value params(base::Value::Type::DICTIONARY);

      for (const auto params_kv : params_dict->DictItems()) {
        const auto& param_key = params_kv.first;
        const auto& param_val = params_kv.second;
        if (param_val.is_bool()) {
          params.SetStringKey(param_key,
                              param_val.GetBool() ? "true" : "false");
        } else if (param_val.is_int()) {
          params.SetStringKey(param_key,
                              base::NumberToString(param_val.GetInt()));
        } else if (param_val.is_double()) {
          params.SetStringKey(param_key,
                              base::NumberToString(param_val.GetDouble()));
        } else if (param_val.is_string()) {
          params.SetStringKey(param_key, param_val.GetString());
        } else {
          LOG(ERROR) << "Entry in params dict for \"" << feature.first << "\""
                     << " is not of a supported type (key: " << param_key
                     << ", type: " << param_val.type();
        }
      }
      persistent_dict.SetPath(feature.first, std::move(params));
      continue;
    }

    // Other base::Value types are not supported.
    LOG(ERROR) << "A DCS feature mapped to an unsupported value. key: "
               << feature.first << " type: " << feature.second.type();
  }

  return persistent_dict;
}

const std::unordered_set<int32_t>& GetDCSExperimentIds() {
  DCHECK(g_experiment_ids_initialized);
  return GetExperimentIds();
}

void ResetCastFeaturesForTesting() {
  g_experiment_ids_initialized = false;
  base::FeatureList::ClearInstanceForTesting();
  GetTestFeatures().clear();
}

void SetFeaturesForTest(std::vector<const base::Feature*> features) {
  GetTestFeatures() = std::move(features);
}

}  // namespace chromecast
