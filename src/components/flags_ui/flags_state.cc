// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/flags_state.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace flags_ui {

namespace internal {
const char kTrialGroupAboutFlags[] = "AboutFlags";
}  // namespace internal

namespace {

// Separator used for origin list values. The list of origins provided from
// the command line or from the text input in chrome://flags are concatenated
// using this separator. The value is then appended as a command line switch
// and saved in the dictionary pref (kAboutFlagsOriginLists).
// E.g. --isolate_origins=http://example1.net,http://example2.net
const char kOriginListValueSeparator[] = ",";

const struct {
  unsigned bit;
  const char* const name;
} kBitsToOs[] = {
    {kOsMac, "Mac"},         {kOsWin, "Windows"},
    {kOsLinux, "Linux"},     {kOsCrOS, "Chrome OS"},
    {kOsAndroid, "Android"}, {kOsCrOSOwnerOnly, "Chrome OS (owner only)"},
    {kOsIos, "iOS"},         {kOsFuchsia, "Fuchsia"},
};

// Adds a |StringValue| to |list| for each platform where |bitmask| indicates
// whether the entry is available on that platform.
void AddOsStrings(unsigned bitmask, base::Value* list) {
  for (const auto& entry : kBitsToOs) {
    if (bitmask & entry.bit)
      list->Append(entry.name);
  }
}

// Confirms that an entry is valid, used in a DCHECK in
// SanitizeList below.
bool IsValidFeatureEntry(const FeatureEntry& e) {
  switch (e.type) {
    case FeatureEntry::SINGLE_VALUE:
    case FeatureEntry::SINGLE_DISABLE_VALUE:
    case FeatureEntry::ORIGIN_LIST_VALUE:
      return true;
    case FeatureEntry::MULTI_VALUE:
      DCHECK_GT(e.choices.size(), 0u);
      DCHECK(e.ChoiceForOption(0).command_line_switch);
      DCHECK_EQ('\0', e.ChoiceForOption(0).command_line_switch[0]);
      return true;
    case FeatureEntry::ENABLE_DISABLE_VALUE:
      DCHECK(e.switches.command_line_switch);
      DCHECK(e.switches.command_line_value);
      DCHECK(e.switches.disable_command_line_switch);
      DCHECK(e.switches.disable_command_line_value);
      return true;
    case FeatureEntry::FEATURE_VALUE:
      DCHECK(e.feature.feature);
      return true;
    case FeatureEntry::FEATURE_WITH_PARAMS_VALUE:
      DCHECK(e.feature.feature);
      DCHECK(e.feature.feature_variations.size());
      DCHECK(e.feature.feature_trial_name);
      return true;
  }
  NOTREACHED();
  return false;
}

// Returns true if none of this entry's options have been enabled.
bool IsDefaultValue(const FeatureEntry& entry,
                    const std::set<std::string>& enabled_entries) {
  switch (entry.type) {
    case FeatureEntry::SINGLE_VALUE:
    case FeatureEntry::SINGLE_DISABLE_VALUE:
    case FeatureEntry::ORIGIN_LIST_VALUE:
      return enabled_entries.count(entry.internal_name) == 0;
    case FeatureEntry::MULTI_VALUE:
    case FeatureEntry::ENABLE_DISABLE_VALUE:
    case FeatureEntry::FEATURE_VALUE:
    case FeatureEntry::FEATURE_WITH_PARAMS_VALUE:
      for (int i = 0; i < entry.NumOptions(); ++i) {
        if (enabled_entries.count(entry.NameForOption(i)) > 0)
          return false;
      }
      return true;
  }
  NOTREACHED();
  return true;
}

// Returns the Value representing the choice data in the specified entry.
base::Value CreateOptionsData(const FeatureEntry& entry,
                              const std::set<std::string>& enabled_entries) {
  DCHECK(entry.type == FeatureEntry::MULTI_VALUE ||
         entry.type == FeatureEntry::ENABLE_DISABLE_VALUE ||
         entry.type == FeatureEntry::FEATURE_VALUE ||
         entry.type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE);
  base::Value result(base::Value::Type::LIST);
  for (int i = 0; i < entry.NumOptions(); ++i) {
    base::Value value(base::Value::Type::DICTIONARY);
    const std::string name = entry.NameForOption(i);
    value.SetStringKey("internal_name", name);
    value.SetStringKey("description", entry.DescriptionForOption(i));
    value.SetBoolKey("selected", enabled_entries.count(name) > 0);
    result.Append(std::move(value));
  }
  return result;
}

// Registers variation parameters specified by |feature_variation_params| for
// the field trial named |feature_trial_name|, unless a group for this trial has
// already been created (e.g. via command-line switches that take precedence
// over about:flags). In the trial, the function creates a new constant group
// with the given |trail_group| name.
base::FieldTrial* RegisterFeatureVariationParameters(
    const std::string& feature_trial_name,
    const std::map<std::string, std::string>& feature_variation_params,
    const std::string& trial_group) {
  bool success = variations::AssociateVariationParams(
      feature_trial_name, trial_group, feature_variation_params);
  if (!success)
    return nullptr;
  // Successful association also means that no group is created and selected
  // for the trial, yet. Thus, create the trial to select the group. This way,
  // the parameters cannot get overwritten in later phases (such as from the
  // server).
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(feature_trial_name, trial_group);
  if (!trial) {
    DLOG(WARNING) << "Could not create the trial " << feature_trial_name
                  << " with group " << trial_group;
  }
  return trial;
}

// Returns true if |value| is safe to include in a command line string in the
// form of --flag=value.
bool IsSafeValue(const std::string& value) {
  // Punctuation characters at the end ("-", ".", ":", "/") are allowed because
  // origins can contain those (e.g. http://example.test). Comma is allowed
  // because it's used as the separator character.
  static const char kAllowedChars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789"
      "-.:/,";
  return value.find_first_not_of(kAllowedChars) == std::string::npos;
}

// Sanitizes |value| which contains a list of origins separated by whitespace
// and/or comma. The sanitized vector of origins is intended to be added to the
// command line, so this is a security critical operation: The sanitized value
// must have no whitespaces, each individual origin must be separated by a
// comma, and each origin must represent a url::Origin(). The list is not
// reordered.
std::vector<std::string> TokenizeOriginList(const std::string& value) {
  const std::string input = base::CollapseWhitespaceASCII(value, false);
  // Allow both space and comma as separators.
  const std::string delimiters = " ,";
  base::StringTokenizer tokenizer(input, delimiters);
  std::vector<std::string> origin_strings;
  while (tokenizer.GetNext()) {
    base::StringPiece token = tokenizer.token_piece();
    DCHECK(!token.empty());
    const GURL url(token);
    if (!url.is_valid() ||
        (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsWSOrWSS())) {
      continue;
    }
    const std::string origin = url::Origin::Create(url).Serialize();
    if (!IsSafeValue(origin)) {
      continue;
    }
    origin_strings.push_back(origin);
  }
  return origin_strings;
}

// Combines the origin lists contained in |value1| and |value2| separated by
// commas. The lists are concatenated, with invalid or duplicate origins
// removed.
std::string CombineAndSanitizeOriginLists(const std::string& value1,
                                          const std::string& value2) {
  std::set<std::string> seen_origins;
  std::vector<std::string> origin_vector;
  for (const std::string& list : {value1, value2}) {
    for (const std::string& origin : TokenizeOriginList(list)) {
      if (!base::Contains(seen_origins, origin)) {
        origin_vector.push_back(origin);
        seen_origins.insert(origin);
      }
    }
  }
  const std::string result =
      base::JoinString(origin_vector, kOriginListValueSeparator);
  CHECK(IsSafeValue(result));
  return result;
}

// Returns the sanitized combined origin list by concatenating the command line
// and the pref values. Invalid or duplicate origins are dropped.
std::string GetCombinedOriginListValue(const FlagsStorage& flags_storage,
                                       const base::CommandLine& command_line,
                                       const std::string& internal_entry_name,
                                       const std::string& command_line_switch) {
  const std::string existing_value =
      command_line.GetSwitchValueASCII(command_line_switch);
  const std::string new_value =
      flags_storage.GetOriginListFlag(internal_entry_name);
  return CombineAndSanitizeOriginLists(existing_value, new_value);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// ChromeOS does not call ConvertFlagsToSwitches on startup (see
// ChromeFeatureListCreator::ConvertFlagsToSwitches() for details) so the
// command line cannot be updated using pref values. Instead, this method
// modifies it on the fly when the user makes a change.
void DidModifyOriginListFlag(const FlagsStorage& flags_storage,
                             const FeatureEntry& entry) {
  base::CommandLine* current_cl = base::CommandLine::ForCurrentProcess();
  const std::string new_value = GetCombinedOriginListValue(
      flags_storage, *current_cl, entry.internal_name,
      entry.switches.command_line_switch);

  // Remove the switch if it exists.
  base::CommandLine new_cl(current_cl->GetProgram());
  const base::CommandLine::SwitchMap switches = current_cl->GetSwitches();
  for (const auto& it : switches) {
    const auto& switch_name = it.first;
    const auto& switch_value = it.second;
    if (switch_name != entry.switches.command_line_switch) {
      if (switch_value.empty()) {
        new_cl.AppendSwitch(switch_name);
      } else {
        new_cl.AppendSwitchNative(switch_name, switch_value);
      }
    }
  }
  *current_cl = new_cl;

  const std::string sanitized =
      CombineAndSanitizeOriginLists(std::string(), new_value);
  current_cl->AppendSwitchASCII(entry.switches.command_line_switch, sanitized);
}
#endif

}  // namespace

struct FlagsState::SwitchEntry {
  // Corresponding base::Feature to toggle.
  std::string feature_name;

  // If |feature_name| is not empty, the state (enable/disabled) to set.
  bool feature_state;

  // The name of the switch to add.
  std::string switch_name;

  // If |switch_name| is not empty, the value of the switch to set.
  std::string switch_value;

  SwitchEntry() : feature_state(false) {}
};

bool FlagsState::Delegate::ShouldExcludeFlag(const FlagsStorage* state,
                                             const FeatureEntry& entry) {
  return false;
}

FlagsState::Delegate::Delegate() = default;
FlagsState::Delegate::~Delegate() = default;

FlagsState::FlagsState(base::span<const FeatureEntry> feature_entries,
                       FlagsState::Delegate* delegate)
    : feature_entries_(feature_entries),
      needs_restart_(false),
      delegate_(delegate) {}

FlagsState::~FlagsState() {}

void FlagsState::ConvertFlagsToSwitches(
    FlagsStorage* flags_storage,
    base::CommandLine* command_line,
    SentinelsMode sentinels,
    const char* enable_features_flag_name,
    const char* disable_features_flag_name) {
  std::set<std::string> enabled_entries;
  std::map<std::string, SwitchEntry> name_to_switch_map;
  GenerateFlagsToSwitchesMapping(flags_storage, *command_line, &enabled_entries,
                                 &name_to_switch_map);
  AddSwitchesToCommandLine(enabled_entries, name_to_switch_map, sentinels,
                           command_line, enable_features_flag_name,
                           disable_features_flag_name);
}

void FlagsState::GetSwitchesAndFeaturesFromFlags(
    FlagsStorage* flags_storage,
    std::set<std::string>* switches,
    std::set<std::string>* features) const {
  std::set<std::string> enabled_entries;
  std::map<std::string, SwitchEntry> name_to_switch_map;
  GenerateFlagsToSwitchesMapping(flags_storage,
                                 *base::CommandLine::ForCurrentProcess(),
                                 &enabled_entries, &name_to_switch_map);

  for (const std::string& entry_name : enabled_entries) {
    const auto& entry_it = name_to_switch_map.find(entry_name);
    DCHECK(entry_it != name_to_switch_map.end());

    const SwitchEntry& entry = entry_it->second;
    if (!entry.switch_name.empty())
      switches->insert("--" + entry.switch_name);

    if (!entry.feature_name.empty()) {
      if (entry.feature_state)
        features->insert(entry.feature_name + ":enabled");
      else
        features->insert(entry.feature_name + ":disabled");
    }
  }
}

bool FlagsState::IsRestartNeededToCommitChanges() {
  return needs_restart_;
}

void FlagsState::SetFeatureEntryEnabled(FlagsStorage* flags_storage,
                                        const std::string& internal_name,
                                        bool enable) {
  size_t at_index = internal_name.find(testing::kMultiSeparator);
  if (at_index != std::string::npos) {
    DCHECK(enable);
    // We're being asked to enable a multi-choice entry. Disable the
    // currently selected choice.
    DCHECK_NE(at_index, 0u);
    const std::string entry_name = internal_name.substr(0, at_index);
    SetFeatureEntryEnabled(flags_storage, entry_name, false);

    // And enable the new choice, if it is not the default first choice.
    if (internal_name != entry_name + "@0") {
      std::set<std::string> enabled_entries;
      GetSanitizedEnabledFlags(flags_storage, &enabled_entries);
      needs_restart_ |= enabled_entries.insert(internal_name).second;
      flags_storage->SetFlags(enabled_entries);
    }
    return;
  }

  std::set<std::string> enabled_entries;
  GetSanitizedEnabledFlags(flags_storage, &enabled_entries);

  const FeatureEntry* e = FindFeatureEntryByName(internal_name);
  DCHECK(e);

  if (e->type == FeatureEntry::SINGLE_VALUE ||
      e->type == FeatureEntry::ORIGIN_LIST_VALUE) {
    if (enable)
      needs_restart_ |= enabled_entries.insert(internal_name).second;
    else
      needs_restart_ |= (enabled_entries.erase(internal_name) > 0);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // If an origin list was enabled or disabled, update the command line flag.
    if (e->type == FeatureEntry::ORIGIN_LIST_VALUE && enable)
      DidModifyOriginListFlag(*flags_storage, *e);
#endif

  } else if (e->type == FeatureEntry::SINGLE_DISABLE_VALUE) {
    if (!enable)
      needs_restart_ |= enabled_entries.insert(internal_name).second;
    else
      needs_restart_ |= (enabled_entries.erase(internal_name) > 0);
  } else {
    if (enable) {
      // Enable the first choice.
      needs_restart_ |= enabled_entries.insert(e->NameForOption(0)).second;
    } else {
      // Find the currently enabled choice and disable it.
      for (int i = 0; i < e->NumOptions(); ++i) {
        std::string choice_name = e->NameForOption(i);
        if (enabled_entries.find(choice_name) != enabled_entries.end()) {
          needs_restart_ = true;
          enabled_entries.erase(choice_name);
          // Continue on just in case there's a bug and more than one
          // entry for this choice was enabled.
        }
      }
    }
  }

  flags_storage->SetFlags(enabled_entries);
}

void FlagsState::SetOriginListFlag(const std::string& internal_name,
                                   const std::string& value,
                                   FlagsStorage* flags_storage) {
  const std::string new_value =
      CombineAndSanitizeOriginLists(std::string(), value);
  flags_storage->SetOriginListFlag(internal_name, new_value);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const FeatureEntry* entry = FindFeatureEntryByName(internal_name);
  DCHECK(entry);

  std::set<std::string> enabled_entries;
  GetSanitizedEnabledFlags(flags_storage, &enabled_entries);
  const bool enabled = base::Contains(enabled_entries, entry->internal_name);
  if (enabled)
    DidModifyOriginListFlag(*flags_storage, *entry);
#endif
}

void FlagsState::RemoveFlagsSwitches(
    base::CommandLine::SwitchMap* switch_list) {
  for (const auto& entry : flags_switches_)
    switch_list->erase(entry.first);

  // If feature entries were added to --enable-features= or --disable-features=
  // lists, remove them here while preserving existing values.
  for (const auto& entry : appended_switches_) {
    const auto& switch_name = entry.first;
    const auto& switch_added_values = entry.second;

    // The below is either a std::string or a std::u16string based on platform.
    const auto& existing_value = (*switch_list)[switch_name];
#if defined(OS_WIN)
    const std::string existing_value_utf8 = base::WideToUTF8(existing_value);
#else
    const std::string& existing_value_utf8 = existing_value;
#endif

    std::vector<base::StringPiece> features =
        base::FeatureList::SplitFeatureListString(existing_value_utf8);
    std::vector<base::StringPiece> remaining_features;
    // For any featrue name in |features| that is not in |switch_added_values| -
    // i.e. it wasn't added by about_flags code, add it to |remaining_features|.
    for (const auto& feature : features) {
      if (!base::Contains(switch_added_values, std::string(feature)))
        remaining_features.push_back(feature);
    }

    // Either remove the flag entirely if |remaining_features| is empty, or set
    // the new list.
    if (remaining_features.empty()) {
      switch_list->erase(switch_name);
    } else {
      std::string switch_value = base::JoinString(remaining_features, ",");
#if defined(OS_WIN)
      (*switch_list)[switch_name] = base::UTF8ToWide(switch_value);
#else
      (*switch_list)[switch_name] = switch_value;
#endif
    }
  }
}

void FlagsState::ResetAllFlags(FlagsStorage* flags_storage) {
  needs_restart_ = true;

  std::set<std::string> no_entries;
  flags_storage->SetFlags(no_entries);
}

void FlagsState::Reset() {
  needs_restart_ = false;
  flags_switches_.clear();
  appended_switches_.clear();
}

std::vector<std::string> FlagsState::RegisterAllFeatureVariationParameters(
    FlagsStorage* flags_storage,
    base::FeatureList* feature_list) {
  std::set<std::string> enabled_entries;
  GetSanitizedEnabledFlagsForCurrentPlatform(flags_storage, &enabled_entries);
  return RegisterEnabledFeatureVariationParameters(
      feature_entries_, enabled_entries, internal::kTrialGroupAboutFlags,
      feature_list);
}

// static
std::vector<std::string> FlagsState::RegisterEnabledFeatureVariationParameters(
    const base::span<const FeatureEntry>& feature_entries,
    const std::set<std::string>& enabled_entries,
    const std::string& trial_group,
    base::FeatureList* feature_list) {
  std::vector<std::string> variation_ids;
  std::map<std::string, std::set<std::string>> enabled_features_by_trial_name;
  std::map<std::string, std::map<std::string, std::string>>
      params_by_trial_name;

  // First collect all the data for each trial.
  for (const FeatureEntry& entry : feature_entries) {
    if (entry.type == FeatureEntry::FEATURE_WITH_PARAMS_VALUE) {
      for (int j = 0; j < entry.NumOptions(); ++j) {
        if (entry.StateForOption(j) == FeatureEntry::FeatureState::ENABLED &&
            enabled_entries.count(entry.NameForOption(j))) {
          std::string trial_name = entry.feature.feature_trial_name;
          // The user has chosen to enable the feature by this option.
          enabled_features_by_trial_name[trial_name].insert(
              entry.feature.feature->name);

          const FeatureEntry::FeatureVariation* variation =
              entry.VariationForOption(j);
          if (!variation)
            continue;

          // The selected variation is non-default, collect its params & id.

          for (int i = 0; i < variation->num_params; ++i) {
            auto insert_result = params_by_trial_name[trial_name].insert(
                std::make_pair(variation->params[i].param_name,
                               variation->params[i].param_value));
            DCHECK(insert_result.second)
                << "Multiple values for the same parameter '"
                << variation->params[i].param_name
                << "' are specified in chrome://flags!";
          }
          if (variation->variation_id)
            variation_ids.push_back(variation->variation_id);
        }
      }
    }
  }

  // Now create the trials and associate the features to them.
  for (const auto& kv : enabled_features_by_trial_name) {
    const std::string& trial_name = kv.first;
    const std::set<std::string>& trial_features = kv.second;

    base::FieldTrial* field_trial = RegisterFeatureVariationParameters(
        trial_name, params_by_trial_name[trial_name], trial_group);
    if (!field_trial)
      continue;

    for (const std::string& feature_name : trial_features) {
      feature_list->RegisterFieldTrialOverride(
          feature_name,
          base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
          field_trial);
    }
  }

  return variation_ids;
}

void FlagsState::GetFlagFeatureEntries(
    FlagsStorage* flags_storage,
    FlagAccess access,
    base::Value::ListStorage& supported_entries,
    base::Value::ListStorage& unsupported_entries,
    base::RepeatingCallback<bool(const FeatureEntry&)> skip_feature_entry) {
  DCHECK(flags_storage);
  std::set<std::string> enabled_entries;
  GetSanitizedEnabledFlags(flags_storage, &enabled_entries);

  int current_platform = GetCurrentPlatform();

  for (const FeatureEntry& entry : feature_entries_) {
    if (skip_feature_entry.Run(entry))
      continue;

    base::Value data(base::Value::Type::DICTIONARY);
    data.SetStringKey("internal_name", entry.internal_name);
    data.SetStringKey("name", base::StringPiece(entry.visible_name));
    data.SetStringKey("description",
                      base::StringPiece(entry.visible_description));

    base::Value supported_platforms(base::Value::Type::LIST);
    AddOsStrings(entry.supported_platforms, &supported_platforms);
    data.SetKey("supported_platforms", std::move(supported_platforms));
    // True if the switch is not currently passed.
    bool is_default_value = IsDefaultValue(entry, enabled_entries);
    data.SetBoolKey("is_default", is_default_value);

    switch (entry.type) {
      case FeatureEntry::SINGLE_VALUE:
      case FeatureEntry::SINGLE_DISABLE_VALUE:
        data.SetBoolKey(
            "enabled",
            (!is_default_value && entry.type == FeatureEntry::SINGLE_VALUE) ||
                (is_default_value &&
                 entry.type == FeatureEntry::SINGLE_DISABLE_VALUE));
        break;
      case FeatureEntry::ORIGIN_LIST_VALUE:
        data.SetBoolKey("enabled", !is_default_value);
        data.SetStringKey(
            "origin_list_value",
            GetCombinedOriginListValue(
                *flags_storage, *base::CommandLine::ForCurrentProcess(),
                entry.internal_name, entry.switches.command_line_switch));
        break;
      case FeatureEntry::MULTI_VALUE:
      case FeatureEntry::ENABLE_DISABLE_VALUE:
      case FeatureEntry::FEATURE_VALUE:
      case FeatureEntry::FEATURE_WITH_PARAMS_VALUE:
        data.SetKey("options", CreateOptionsData(entry, enabled_entries));
        break;
    }

    bool supported = (entry.supported_platforms & current_platform) != 0;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (access == kOwnerAccessToFlags &&
        (entry.supported_platforms & kOsCrOSOwnerOnly) != 0) {
      supported = true;
    }
#endif

    if (supported)
      supported_entries.push_back(std::move(data));
    else
      unsupported_entries.push_back(std::move(data));
  }
}

// static
unsigned short FlagsState::GetCurrentPlatform() {
#if defined(OS_IOS)
  return kOsIos;
#elif defined(OS_MAC)
  return kOsMac;
#elif defined(OS_WIN)
  return kOsWin;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return kOsCrOS;
#elif (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    defined(OS_OPENBSD)
  return kOsLinux;
#elif defined(OS_ANDROID)
  return kOsAndroid;
#elif defined(OS_FUCHSIA)
  return kOsFuchsia;
#else
#error Unknown platform
#endif
}

void FlagsState::AddSwitchMapping(
    const std::string& key,
    const std::string& switch_name,
    const std::string& switch_value,
    std::map<std::string, SwitchEntry>* name_to_switch_map) const {
  DCHECK(!base::Contains(*name_to_switch_map, key));

  SwitchEntry* entry = &(*name_to_switch_map)[key];
  entry->switch_name = switch_name;
  entry->switch_value = switch_value;
}

void FlagsState::AddFeatureMapping(
    const std::string& key,
    const std::string& feature_name,
    bool feature_state,
    std::map<std::string, SwitchEntry>* name_to_switch_map) const {
  DCHECK(!base::Contains(*name_to_switch_map, key));

  SwitchEntry* entry = &(*name_to_switch_map)[key];
  entry->feature_name = feature_name;
  entry->feature_state = feature_state;
}

void FlagsState::AddSwitchesToCommandLine(
    const std::set<std::string>& enabled_entries,
    const std::map<std::string, SwitchEntry>& name_to_switch_map,
    SentinelsMode sentinels,
    base::CommandLine* command_line,
    const char* enable_features_flag_name,
    const char* disable_features_flag_name) {
  std::map<std::string, bool> feature_switches;
  if (sentinels == kAddSentinels) {
    command_line->AppendSwitch(switches::kFlagSwitchesBegin);
    flags_switches_[switches::kFlagSwitchesBegin] = std::string();
  }

  for (const std::string& entry_name : enabled_entries) {
    const auto& entry_it = name_to_switch_map.find(entry_name);
    if (entry_it == name_to_switch_map.end()) {
      NOTREACHED();
      continue;
    }

    const SwitchEntry& entry = entry_it->second;
    if (!entry.feature_name.empty()) {
      feature_switches[entry.feature_name] = entry.feature_state;
    } else if (!entry.switch_name.empty()) {
      command_line->AppendSwitchASCII(entry.switch_name, entry.switch_value);
      flags_switches_[entry.switch_name] = entry.switch_value;
    }
    // If an entry doesn't match either of the above, then it is likely the
    // default entry for a FEATURE_VALUE entry. Safe to ignore.
  }

  if (!feature_switches.empty()) {
    MergeFeatureCommandLineSwitch(feature_switches, enable_features_flag_name,
                                  true, command_line);
    MergeFeatureCommandLineSwitch(feature_switches, disable_features_flag_name,
                                  false, command_line);
  }

  if (sentinels == kAddSentinels) {
    command_line->AppendSwitch(switches::kFlagSwitchesEnd);
    flags_switches_[switches::kFlagSwitchesEnd] = std::string();
  }
}

void FlagsState::MergeFeatureCommandLineSwitch(
    const std::map<std::string, bool>& feature_switches,
    const char* switch_name,
    bool feature_state,
    base::CommandLine* command_line) {
  std::string original_switch_value =
      command_line->GetSwitchValueASCII(switch_name);
  std::vector<base::StringPiece> features =
      base::FeatureList::SplitFeatureListString(original_switch_value);
  // Only add features that don't already exist in the lists.
  // Note: The base::Contains() call results in O(n^2) performance, but in
  // practice n should be very small.
  for (const auto& entry : feature_switches) {
    if (entry.second == feature_state &&
        !base::Contains(features, entry.first)) {
      features.push_back(entry.first);
      appended_switches_[switch_name].insert(entry.first);
    }
  }
  // Update the switch value only if it didn't change. This avoids setting an
  // empty list or duplicating the same list (since AppendSwitch() adds the
  // switch to the end but doesn't remove previous ones).
  std::string switch_value = base::JoinString(features, ",");
  if (switch_value != original_switch_value)
    command_line->AppendSwitchASCII(switch_name, switch_value);
}

std::set<std::string> FlagsState::SanitizeList(
    const FlagsStorage* storage,
    const std::set<std::string>& enabled_entries,
    int platform_mask) const {
  std::set<std::string> new_enabled_entries;

  // For each entry in |enabled_entries|, check whether it exists in the list
  // of supported features. Remove those that don't. Note: Even though this is
  // an O(n^2) search, this is more efficient than creating a set from
  // |feature_entries_| first because |feature_entries_| is large and
  // |enabled_entries| should generally be small/empty.
  for (const std::string& entry_name : enabled_entries) {
    if (IsSupportedFeature(storage, entry_name, platform_mask))
      new_enabled_entries.insert(entry_name);
  }

  return new_enabled_entries;
}

void FlagsState::GetSanitizedEnabledFlags(FlagsStorage* flags_storage,
                                          std::set<std::string>* result) const {
  std::set<std::string> enabled_entries = flags_storage->GetFlags();
  std::set<std::string> new_enabled_entries =
      SanitizeList(flags_storage, enabled_entries, -1);
  if (new_enabled_entries.size() != enabled_entries.size())
    flags_storage->SetFlags(new_enabled_entries);
  result->swap(new_enabled_entries);
}

void FlagsState::GetSanitizedEnabledFlagsForCurrentPlatform(
    FlagsStorage* flags_storage,
    std::set<std::string>* result) const {
  // TODO(asvitkine): Consider making GetSanitizedEnabledFlags() do the platform
  // filtering by default so that we don't need two calls to SanitizeList().
  GetSanitizedEnabledFlags(flags_storage, result);

  int platform_mask = GetCurrentPlatform();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  platform_mask |= kOsCrOSOwnerOnly;
#endif
  std::set<std::string> platform_entries =
      SanitizeList(flags_storage, *result, platform_mask);
  result->swap(platform_entries);
}

void FlagsState::GenerateFlagsToSwitchesMapping(
    FlagsStorage* flags_storage,
    const base::CommandLine& command_line,
    std::set<std::string>* enabled_entries,
    std::map<std::string, SwitchEntry>* name_to_switch_map) const {
  GetSanitizedEnabledFlagsForCurrentPlatform(flags_storage, enabled_entries);

  if (enabled_entries->empty())
    return;

  for (const FeatureEntry& entry : feature_entries_) {
    switch (entry.type) {
      case FeatureEntry::SINGLE_VALUE:
      case FeatureEntry::SINGLE_DISABLE_VALUE:
        AddSwitchMapping(entry.internal_name,
                         entry.switches.command_line_switch,
                         entry.switches.command_line_value, name_to_switch_map);
        break;

      case FeatureEntry::ORIGIN_LIST_VALUE: {
        // Combine the existing command line value with the user provided list.
        // This is done to retain the existing list from the command line when
        // the browser is restarted. Otherwise, the user provided list would
        // overwrite the list provided from the command line.
        const std::string origin_list_value = GetCombinedOriginListValue(
            *flags_storage, command_line, entry.internal_name,
            entry.switches.command_line_switch);
        AddSwitchMapping(entry.internal_name,
                         entry.switches.command_line_switch, origin_list_value,
                         name_to_switch_map);
        break;
      }

      case FeatureEntry::MULTI_VALUE:
        for (int j = 0; j < entry.NumOptions(); ++j) {
          AddSwitchMapping(entry.NameForOption(j),
                           entry.ChoiceForOption(j).command_line_switch,
                           entry.ChoiceForOption(j).command_line_value,
                           name_to_switch_map);
        }
        break;

      case FeatureEntry::ENABLE_DISABLE_VALUE:
        AddSwitchMapping(entry.NameForOption(0), std::string(), std::string(),
                         name_to_switch_map);
        AddSwitchMapping(entry.NameForOption(1),
                         entry.switches.command_line_switch,
                         entry.switches.command_line_value, name_to_switch_map);
        AddSwitchMapping(
            entry.NameForOption(2), entry.switches.disable_command_line_switch,
            entry.switches.disable_command_line_value, name_to_switch_map);
        break;

      case FeatureEntry::FEATURE_VALUE:
      case FeatureEntry::FEATURE_WITH_PARAMS_VALUE:
        for (int j = 0; j < entry.NumOptions(); ++j) {
          FeatureEntry::FeatureState state = entry.StateForOption(j);
          if (state == FeatureEntry::FeatureState::DEFAULT) {
            AddFeatureMapping(entry.NameForOption(j), std::string(), false,
                              name_to_switch_map);
          } else {
            const FeatureEntry::FeatureVariation* variation =
                entry.VariationForOption(j);
            std::string feature_name(entry.feature.feature->name);
            std::vector<std::string> params_value;

            if (variation) {
              feature_name.append(":");
              for (int i = 0; i < variation->num_params; ++i) {
                std::string param_name =
                    variations::EscapeValue(variation->params[i].param_name);
                std::string param_value =
                    variations::EscapeValue(variation->params[i].param_value);
                params_value.push_back(
                    param_name.append("/").append(param_value));
              }
            }
            AddFeatureMapping(
                entry.NameForOption(j),
                feature_name.append(base::JoinString(params_value, "/")),
                state == FeatureEntry::FeatureState::ENABLED,
                name_to_switch_map);
          }
        }
        break;
    }
  }
}

const FeatureEntry* FlagsState::FindFeatureEntryByName(
    const std::string& internal_name) const {
  for (const FeatureEntry& entry : feature_entries_) {
    if (entry.internal_name == internal_name)
      return &entry;
  }
  return nullptr;
}

bool FlagsState::IsSupportedFeature(const FlagsStorage* storage,
                                    const std::string& name,
                                    int platform_mask) const {
  for (const auto& entry : feature_entries_) {
    DCHECK(IsValidFeatureEntry(entry));
    if (!(entry.supported_platforms & platform_mask))
      continue;
    if (!entry.InternalNameMatches(name))
      continue;
    if (delegate_ && delegate_->ShouldExcludeFlag(storage, entry))
      continue;
    return true;
  }
  return false;
}

}  // namespace flags_ui
