// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <set>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/language/core/browser/locale_util.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/service/buildflags.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace variations {
namespace {

// Maximum age permitted for a variations seed, in days.
const int kMaxVariationsSeedAgeDays = 30;

// Returns the date that should be used by the VariationsSeedProcessor to do
// expiry and start date checks.
base::Time GetReferenceDateForExpiryChecks(PrefService* local_state) {
  const base::Time seed_date = local_state->GetTime(prefs::kVariationsSeedDate);
  const base::Time build_time = base::GetBuildTime();
  // Use the build time for date checks if either the seed date is invalid or
  // the build time is newer than the seed date.
  base::Time reference_date = seed_date;
  if (seed_date.is_null() || seed_date < build_time)
    reference_date = build_time;
  return reference_date;
}

// Just maps one set of enum values to another. Nothing to see here.
Study::Channel ConvertProductChannelToStudyChannel(
    version_info::Channel product_channel) {
  switch (product_channel) {
    case version_info::Channel::CANARY:
      return Study::CANARY;
    case version_info::Channel::DEV:
      return Study::DEV;
    case version_info::Channel::BETA:
      return Study::BETA;
    case version_info::Channel::STABLE:
      return Study::STABLE;
    case version_info::Channel::UNKNOWN:
      return Study::UNKNOWN;
  }
  NOTREACHED();
  return Study::UNKNOWN;
}

// Records the loaded seed's expiry status.
void RecordSeedExpiry(bool is_safe_seed, VariationsSeedExpiry seed_expiry) {
  const std::string histogram_name =
      is_safe_seed ? "Variations.SafeMode.CreateTrials.SeedExpiry"
                   : "Variations.CreateTrials.SeedExpiry";
  base::UmaHistogramEnumeration(histogram_name, seed_expiry);
}

// Records the loaded seed's age.
void RecordSeedFreshness(base::TimeDelta seed_age) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Variations.SeedFreshness", seed_age.InMinutes(),
                              1, base::Days(30).InMinutes(), 50);
}

// Records details about Chrome's attempt to apply a variations seed.
void RecordVariationsSeedUsage(SeedUsage usage) {
  base::UmaHistogramEnumeration("Variations.SeedUsage", usage);
}

// If an invalid command-line to force field trials was specified, exit the
// browser with a helpful error message, so that the user can correct their
// mistake.
void ExitWithMessage(const std::string& message) {
  puts(message.c_str());
  exit(1);
}

// Retrieves the value of the policy converted to the RestrictionPolicyValues.
RestrictionPolicy GetVariationPolicyRestriction(PrefService* local_state) {
  int value = local_state->GetInteger(prefs::kVariationsRestrictionsByPolicy);

  // If the value form the pref is invalid use the default value.
  if (value < 0 || value > static_cast<int>(RestrictionPolicy::kMaxValue))
    return RestrictionPolicy::NO_RESTRICTIONS;

  return static_cast<RestrictionPolicy>(value);
}

Study::CpuArchitecture GetCurrentCpuArchitecture() {
  std::string process_arch = base::SysInfo::ProcessCPUArchitecture();
  if (process_arch == "ARM_64")
    return Study::ARM64;
  if (process_arch == "ARM")
    return Study::ARM32;
  if (process_arch == "x86")
    return Study::X86_32;
  if (process_arch == "x86_64") {
    std::string os_arch = base::SysInfo::OperatingSystemArchitecture();
    if (base::StartsWith(os_arch, "arm",
                         base::CompareCase::INSENSITIVE_ASCII) ||
        base::EqualsCaseInsensitiveASCII(os_arch, "aarch64")) {
      // x86-64 binary running on an arm64 host via the Rosetta 2 binary
      // translator.
      return Study::TRANSLATED_X86_64;
    }
    return Study::X86_64;
  }
  NOTREACHED();
  return Study::X86_64;
}

// Determines whether the field trial testing config defined in
// testing/variations/fieldtrial_testing_config.json should be applied. If the
// "disable_fieldtrial_testing_config" GN flag is set to true, then the testing
// config should never be applied. Otherwise, if the build is a Chrome-branded
// build, then the testing config should only be applied if the
// "--enable-field-trial-config" switch is passed. For non-Chrome branded
// builds, by default, the testing config is applied, unless the
// "--disable-field-trial-config", "--force-fieldtrials", and/or
// "--variations-server-url" switches are passed. It is however possible to
// apply the testing config as well as specify additional field trials (using
// "--force-fieldtrials") by using the "--enable-field-trial-config" switch.
bool ShouldUseFieldTrialTestingConfig(const base::CommandLine* command_line) {
#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return command_line->HasSwitch(switches::kEnableFieldTrialTestingConfig);
#else
  return command_line->HasSwitch(switches::kEnableFieldTrialTestingConfig) ||
         (!command_line->HasSwitch(switches::kDisableFieldTrialTestingConfig) &&
          !command_line->HasSwitch(::switches::kForceFieldTrials) &&
          !command_line->HasSwitch(switches::kVariationsServerURL));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else
  return false;
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
}

}  // namespace

const base::Feature kForceFieldTrialSetupCrashForTesting{
    "ForceFieldTrialSetupCrashForTesting", base::FEATURE_DISABLED_BY_DEFAULT};

VariationsFieldTrialCreator::VariationsFieldTrialCreator(
    VariationsServiceClient* client,
    std::unique_ptr<VariationsSeedStore> seed_store,
    const UIStringOverrider& ui_string_overrider)
    : client_(client),
      ui_string_overrider_(ui_string_overrider),
      seed_store_(std::move(seed_store)),
      create_trials_from_seed_called_(false),
      application_locale_(
          language::GetApplicationLocale(seed_store_->local_state())),
      has_platform_override_(false),
      platform_override_(Study::PLATFORM_WINDOWS) {}

VariationsFieldTrialCreator::~VariationsFieldTrialCreator() {}

std::string VariationsFieldTrialCreator::GetLatestCountry() const {
  const std::string override_country =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kVariationsOverrideCountry);
  return !override_country.empty()
             ? override_country
             : local_state()->GetString(prefs::kVariationsCountry);
}

bool VariationsFieldTrialCreator::SetUpFieldTrials(
    const std::vector<std::string>& variation_ids,
    const std::vector<base::FeatureList::FeatureOverrideInfo>& extra_overrides,
    std::unique_ptr<const base::FieldTrial::EntropyProvider>
        low_entropy_provider,
    std::unique_ptr<base::FeatureList> feature_list,
    metrics::MetricsStateManager* metrics_state_manager,
    PlatformFieldTrials* platform_field_trials,
    SafeSeedManager* safe_seed_manager,
    absl::optional<int> low_entropy_source_value) {
  DCHECK(feature_list);
  DCHECK(metrics_state_manager);
  DCHECK(platform_field_trials);
  DCHECK(safe_seed_manager);

  if (base::FieldTrialList::IsTrialActive(kExtendedSafeModeTrial) &&
      !metrics_state_manager->is_background_session()) {
    // If the session is expected to be a background session, then do not extend
    // Variations Safe Mode. Extending Safe Mode involves monitoring for crashes
    // earlier on in startup; however, this monitoring is not desired in
    // background sessions, whose terminations should never be considered
    // crashes.
    MaybeExtendVariationsSafeMode(metrics_state_manager);
  }

  // TODO(crbug/1257204): Some FieldTrial-setup-related code is here and some is
  // in MetricsStateManager::InstantiateFieldTrialList(). It's not ideal that
  // it's in two places.
  VariationsIdsProvider* http_header_provider =
      VariationsIdsProvider::GetInstance();
  http_header_provider->SetLowEntropySourceValue(low_entropy_source_value);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // Force the variation ids selected in chrome://flags and/or specified using
  // the command-line flag.
  auto result = http_header_provider->ForceVariationIds(
      variation_ids,
      command_line->GetSwitchValueASCII(switches::kForceVariationIds));

  switch (result) {
    case VariationsIdsProvider::ForceIdsResult::INVALID_SWITCH_ENTRY:
      ExitWithMessage(base::StringPrintf("Invalid --%s list specified.",
                                         switches::kForceVariationIds));
      break;
    case VariationsIdsProvider::ForceIdsResult::INVALID_VECTOR_ENTRY:
      // It should not be possible to have invalid variation ids from the
      // vector param (which corresponds to chrome://flags).
      NOTREACHED();
      break;
    case VariationsIdsProvider::ForceIdsResult::SUCCESS:
      break;
  }

  bool success = http_header_provider->ForceDisableVariationIds(
      command_line->GetSwitchValueASCII(switches::kForceDisableVariationIds));
  if (!success) {
    ExitWithMessage(base::StringPrintf("Invalid --%s list specified.",
                                       switches::kForceDisableVariationIds));
  }

  feature_list->InitializeFromCommandLine(
      command_line->GetSwitchValueASCII(::switches::kEnableFeatures),
      command_line->GetSwitchValueASCII(::switches::kDisableFeatures));

  // This needs to happen here: After the InitializeFromCommandLine() call,
  // because the explicit cmdline --disable-features and --enable-features
  // should take precedence over these extra overrides. Before the call to
  // SetInstance(), because overrides cannot be registered after the FeatureList
  // instance is set.
  feature_list->RegisterExtraFeatureOverrides(extra_overrides);

  bool used_testing_config = false;
  if (ShouldUseFieldTrialTestingConfig(command_line)) {
    ApplyFieldTrialTestingConfig(feature_list.get());
    used_testing_config = true;
  }

  bool used_seed = false;
  if (!used_testing_config) {
    used_seed = CreateTrialsFromSeed(low_entropy_provider.get(),
                                     feature_list.get(), safe_seed_manager);
  }

  platform_field_trials->SetUpFeatureControllingFieldTrials(
      used_seed, low_entropy_provider.get(), feature_list.get());

  base::FeatureList::SetInstance(std::move(feature_list));

  // For testing Variations Safe Mode, maybe crash here.
  if (base::FeatureList::IsEnabled(kForceFieldTrialSetupCrashForTesting)) {
    // We log a recognizable token for the crash condition, to allow tests to
    // recognize the crash location in the test output. See:
    // TEST_P(FieldTrialTest, ExtendedSafeModeEndToEnd)
    LOG(FATAL) << "crash_for_testing";
  }

  // This must be called after |local_state_| is initialized.
  platform_field_trials->SetUpFieldTrials();

  return used_seed;
}

std::unique_ptr<ClientFilterableState>
VariationsFieldTrialCreator::GetClientFilterableStateForVersion(
    const base::Version& version) {
  // Note that passing base::Unretained(client_) is safe here because |client_|
  // lives until Chrome exits.
  auto IsEnterpriseCallback = base::BindRepeating(
      &VariationsServiceClient::IsEnterprise, base::Unretained(client_));
  std::unique_ptr<ClientFilterableState> state =
      std::make_unique<ClientFilterableState>(IsEnterpriseCallback);
  state->locale = application_locale_;
  state->reference_date = GetReferenceDateForExpiryChecks(local_state());
  state->version = version;
  state->os_version = ClientFilterableState::GetOSVersion();
  state->channel =
      ConvertProductChannelToStudyChannel(client_->GetChannelForVariations());
  state->form_factor = GetCurrentFormFactor();
  state->cpu_architecture = GetCurrentCpuArchitecture();
  state->platform = GetPlatform();
  // TODO(crbug/1111131): Expand to other platforms.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
  state->hardware_class = base::SysInfo::HardwareModelName();
#endif
#if BUILDFLAG(IS_ANDROID)
  // This is set on Android only currently, because the IsLowEndDevice() API
  // on other platforms has no intrinsic meaning outside of a field trial that
  // controls its value. Since this is before server-side field trials are
  // evaluated, that field trial would not be able to apply for this case.
  state->is_low_end_device = base::SysInfo::IsLowEndDevice();
#endif
  state->session_consistency_country = GetLatestCountry();
  state->permanent_consistency_country = LoadPermanentConsistencyCountry(
      version, state->session_consistency_country);
  state->policy_restriction = GetVariationPolicyRestriction(local_state());
  return state;
}

std::string VariationsFieldTrialCreator::LoadPermanentConsistencyCountry(
    const base::Version& version,
    const std::string& latest_country) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(version.IsValid());

  const std::string override_country =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kVariationsOverrideCountry);
  if (!override_country.empty())
    return override_country;

  const std::string permanent_overridden_country =
      local_state()->GetString(prefs::kVariationsPermanentOverriddenCountry);

  if (!permanent_overridden_country.empty()) {
    base::UmaHistogramEnumeration(
        "Variations.LoadPermanentConsistencyCountryResult",
        LOAD_COUNTRY_HAS_PERMANENT_OVERRIDDEN_COUNTRY, LOAD_COUNTRY_MAX);
    return permanent_overridden_country;
  }

  const base::Value* list_value =
      local_state()->GetList(prefs::kVariationsPermanentConsistencyCountry);
  const std::string* stored_version_string = nullptr;
  const std::string* stored_country = nullptr;

  // Determine if the saved pref value is present and valid.
  const bool is_pref_empty = list_value->GetListDeprecated().empty();
  const bool is_pref_valid =
      list_value->GetListDeprecated().size() == 2 &&
      (stored_version_string =
           list_value->GetListDeprecated()[0].GetIfString()) &&
      (stored_country = list_value->GetListDeprecated()[1].GetIfString()) &&
      base::Version(*stored_version_string).IsValid();

  // Determine if the version from the saved pref matches |version|.
  const bool does_version_match =
      is_pref_valid && version == base::Version(*stored_version_string);

  // Determine if the country in the saved pref matches the country in
  // |latest_country|.
  const bool does_country_match = is_pref_valid && !latest_country.empty() &&
                                  *stored_country == latest_country;

  // Record a histogram for how the saved pref value compares to the current
  // version and the country code in the variations seed.
  LoadPermanentConsistencyCountryResult result;
  if (is_pref_empty) {
    result = !latest_country.empty() ? LOAD_COUNTRY_NO_PREF_HAS_SEED
                                     : LOAD_COUNTRY_NO_PREF_NO_SEED;
  } else if (!is_pref_valid) {
    result = !latest_country.empty() ? LOAD_COUNTRY_INVALID_PREF_HAS_SEED
                                     : LOAD_COUNTRY_INVALID_PREF_NO_SEED;
  } else if (latest_country.empty()) {
    result = does_version_match ? LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_EQ
                                : LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_NEQ;
  } else if (does_version_match) {
    result = does_country_match ? LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_EQ
                                : LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_NEQ;
  } else {
    result = does_country_match ? LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_EQ
                                : LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_NEQ;
  }
  UMA_HISTOGRAM_ENUMERATION("Variations.LoadPermanentConsistencyCountryResult",
                            result, LOAD_COUNTRY_MAX);

  // Use the stored country if one is available and was fetched since the last
  // time Chrome was updated.
  if (does_version_match)
    return *stored_country;

  if (latest_country.empty()) {
    if (!is_pref_valid)
      local_state()->ClearPref(prefs::kVariationsPermanentConsistencyCountry);
    // If we've never received a country code from the server, use an empty
    // country so that it won't pass any filters that specifically include
    // countries, but so that it will pass any filters that specifically exclude
    // countries.
    return std::string();
  }

  // Otherwise, update the pref with the current Chrome version and country.
  StorePermanentCountry(version, latest_country);
  return latest_country;
}

void VariationsFieldTrialCreator::StorePermanentCountry(
    const base::Version& version,
    const std::string& country) {
  base::ListValue new_list_value;
  new_list_value.Append(version.GetString());
  new_list_value.Append(country);
  local_state()->Set(prefs::kVariationsPermanentConsistencyCountry,
                     new_list_value);
}

void VariationsFieldTrialCreator::StoreVariationsOverriddenCountry(
    const std::string& country) {
  local_state()->SetString(prefs::kVariationsPermanentOverriddenCountry,
                           country);
}

void VariationsFieldTrialCreator::OverrideVariationsPlatform(
    Study::Platform platform_override) {
  has_platform_override_ = true;
  platform_override_ = platform_override;
}

void VariationsFieldTrialCreator::OverrideCachedUIStrings() {
  DCHECK(ui::ResourceBundle::HasSharedInstance());

  ui::ResourceBundle* bundle = &ui::ResourceBundle::GetSharedInstance();
  bundle->CheckCanOverrideStringResources();

  for (auto const& it : overridden_strings_map_)
    bundle->OverrideLocaleStringResource(it.first, it.second);

  overridden_strings_map_.clear();
}

bool VariationsFieldTrialCreator::IsOverrideResourceMapEmpty() {
  return overridden_strings_map_.empty();
}

void VariationsFieldTrialCreator::MaybeExtendVariationsSafeMode(
    metrics::MetricsStateManager* metrics_state_manager) {
  const std::string group_name =
      base::FieldTrialList::FindFullName(kExtendedSafeModeTrial);
  DCHECK(!group_name.empty());

  if (group_name == kDefaultGroup)
    return;

  if (group_name == kControlGroup) {
    // Populate the histogram for the control group to more easily compare it
    // with the groups that introduce new behavior.
    SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
        "Variations.ExtendedSafeMode.WritePrefsTime");
    return;
  }

  DCHECK_EQ(group_name, kSignalAndWriteViaFileUtilGroup);
  metrics_state_manager->LogHasSessionShutdownCleanly(
      /*has_session_shutdown_cleanly=*/false,
      /*is_extended_safe_mode=*/true);
}

Study::Platform VariationsFieldTrialCreator::GetPlatform() {
  if (has_platform_override_)
    return platform_override_;
  return ClientFilterableState::GetCurrentPlatform();
}

void VariationsFieldTrialCreator::OverrideUIString(uint32_t resource_hash,
                                                   const std::u16string& str) {
  int resource_id = ui_string_overrider_.GetResourceIndex(resource_hash);
  if (resource_id == -1)
    return;

  // This function may be called before the resource bundle is initialized. So
  // we cache the UI strings and override them after the full browser starts.
  if (!ui::ResourceBundle::HasSharedInstance()) {
    overridden_strings_map_[resource_id] = str;
    return;
  }

  ui::ResourceBundle::GetSharedInstance().OverrideLocaleStringResource(
      resource_id, str);
}

Study::FormFactor VariationsFieldTrialCreator::GetCurrentFormFactor() {
  return client_->GetCurrentFormFactor();
}

void VariationsFieldTrialCreator::ApplyFieldTrialTestingConfig(
    base::FeatureList* feature_list) {
  // Note that passing base::Unretained(this) below is safe because the callback
  // is executed synchronously.
  AssociateDefaultFieldTrialConfig(
      base::BindRepeating(&VariationsFieldTrialCreator::OverrideUIString,
                          base::Unretained(this)),
      GetPlatform(), GetCurrentFormFactor(), feature_list);
}

bool VariationsFieldTrialCreator::HasSeedExpired(bool is_safe_seed) {
  const base::Time fetch_time = is_safe_seed
                                    ? GetSeedStore()->GetSafeSeedFetchTime()
                                    : GetSeedStore()->GetLastFetchTime();

  // If the fetch time is null, skip the expiry check. If the seed is a regular
  // seed (i.e. not a safe seed) and the fetch time is missing, then this must
  // be the first run of Chrome. If the seed is a safe seed, the fetch time may
  // be missing because the pref was added about a milestone later than most of
  // the other safe seed prefs.
  if (fetch_time.is_null()) {
    RecordSeedExpiry(is_safe_seed, VariationsSeedExpiry::kFetchTimeMissing);
    if (!is_safe_seed) {
      // Store the current time as the last fetch time for Chrome's first run.
      GetSeedStore()->RecordLastFetchTime(base::Time::Now());
      // Record freshness of "0", since we expect a first run seed to be fresh.
      RecordSeedFreshness(base::TimeDelta());
    }
    return false;
  }

  const base::TimeDelta seed_age = base::Time::Now() - fetch_time;
  bool has_seed_expired = seed_age.InDays() > kMaxVariationsSeedAgeDays &&
                          GetBuildTime() > fetch_time;
  if (!has_seed_expired)
    RecordSeedFreshness(seed_age);
  RecordSeedExpiry(is_safe_seed, has_seed_expired
                                     ? VariationsSeedExpiry::kExpired
                                     : VariationsSeedExpiry::kNotExpired);
  return has_seed_expired;
}

bool VariationsFieldTrialCreator::IsSeedForFutureMilestone(bool is_safe_seed) {
  const std::string milestone_pref = is_safe_seed
                                         ? prefs::kVariationsSafeSeedMilestone
                                         : prefs::kVariationsSeedMilestone;

  // The regular and safe seed milestone prefs were added in M97, so the prefs
  // are not populated for seeds stored before then.
  int seed_milestone = local_state()->GetInteger(milestone_pref);
  if (!seed_milestone)
    return false;

  int client_milestone = version_info::GetMajorVersionNumberAsInt();
  return seed_milestone > client_milestone;
}

bool VariationsFieldTrialCreator::CreateTrialsFromSeed(
    const base::FieldTrial::EntropyProvider* low_entropy_provider,
    base::FeatureList* feature_list,
    SafeSeedManager* safe_seed_manager) {
  TRACE_EVENT0("startup", "VariationsFieldTrialCreator::CreateTrialsFromSeed");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!create_trials_from_seed_called_);
  create_trials_from_seed_called_ = true;

  base::TimeTicks start_time = base::TimeTicks::Now();

  const base::Version& current_version = version_info::GetVersion();
  if (!current_version.IsValid())
    return false;

  std::unique_ptr<ClientFilterableState> client_filterable_state =
      GetClientFilterableStateForVersion(current_version);
  base::UmaHistogramSparse("Variations.UserChannel",
                           client_filterable_state->channel);
  base::UmaHistogramEnumeration("Variations.PolicyRestriction",
                                client_filterable_state->policy_restriction);

  VariationsSeed seed;
  bool run_in_safe_mode = safe_seed_manager->ShouldRunInSafeMode();
  if (run_in_safe_mode) {
    if (GetSeedStore()->LoadSafeSeed(&seed, client_filterable_state.get())) {
      // TODO(crbug/1261685): The expiry and milestone checks are repeated below
      // for regular seeds. Refactor this.
      if (HasSeedExpired(/*is_safe_seed=*/true)) {
        RecordVariationsSeedUsage(SeedUsage::kExpiredSafeSeedNotUsed);
        return false;
      }
      if (IsSeedForFutureMilestone(/*is_safe_seed=*/true)) {
        RecordVariationsSeedUsage(
            SeedUsage::kSafeSeedForFutureMilestoneNotUsed);
        return false;
      }
      RecordVariationsSeedUsage(SeedUsage::kSafeSeedUsed);
    } else {
      // If Chrome should run in safe mode but the safe seed was not
      // successfully loaded, then do not apply a seed. Fall back to client-side
      // defaults.
      RecordVariationsSeedUsage(SeedUsage::kUnloadableSafeSeedNotUsed);
      return false;
    }
  }

  std::string seed_data;
  std::string base64_seed_signature;
  if (!run_in_safe_mode) {
    if (GetSeedStore()->LoadSeed(&seed, &seed_data, &base64_seed_signature)) {
      if (HasSeedExpired(/*is_safe_seed=*/false)) {
        RecordVariationsSeedUsage(SeedUsage::kExpiredRegularSeedNotUsed);
        return false;
      }
      if (IsSeedForFutureMilestone(/*is_safe_seed=*/false)) {
        RecordVariationsSeedUsage(
            SeedUsage::kRegularSeedForFutureMilestoneNotUsed);
        return false;
      }
      RecordVariationsSeedUsage(SeedUsage::kRegularSeedUsed);
    } else {
      // The regular seed was not successfully loaded, so do not apply a seed.
      // Fall back to client-side defaults.
      RecordVariationsSeedUsage(SeedUsage::kUnloadableRegularSeedNotUsed);
      return false;
    }
  }

  // Note that passing base::Unretained(this) below is safe because the callback
  // is executed synchronously. It is not possible to pass UIStringOverrider
  // directly to VariationsSeedProcessor (which is in components/variations and
  // not components/variations/service) as the variations component should not
  // depend on //ui/base.
  VariationsSeedProcessor().CreateTrialsFromSeed(
      seed, *client_filterable_state,
      base::BindRepeating(&VariationsFieldTrialCreator::OverrideUIString,
                          base::Unretained(this)),
      low_entropy_provider, feature_list);

  // Store into the |safe_seed_manager| the combined server and client data used
  // to create the field trials. But, as an optimization, skip this step when
  // running in safe mode – once running in safe mode, there can never be a need
  // to save the active state to the safe seed prefs.
  if (!run_in_safe_mode) {
    safe_seed_manager->SetActiveSeedState(
        seed_data, base64_seed_signature,
        local_state()->GetInteger(prefs::kVariationsSeedMilestone),
        std::move(client_filterable_state), seed_store_->GetLastFetchTime());
  }

  base::UmaHistogramCounts1M("Variations.AppliedSeed.Size", seed_data.size());
  base::UmaHistogramTimes("Variations.SeedProcessingTime",
                          base::TimeTicks::Now() - start_time);
  return true;
}

VariationsSeedStore* VariationsFieldTrialCreator::GetSeedStore() {
  return seed_store_.get();
}

base::Time VariationsFieldTrialCreator::GetBuildTime() const {
  return base::GetBuildTime();
}

}  // namespace variations
