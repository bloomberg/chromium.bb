// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"

#include "base/containers/cxx20_erase.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/unified_consent/pref_names.h"

namespace {

base::TimeDelta GetMinTimeToPrompt() {
  return features::kTrustSafetySentimentSurveyMinTimeToPrompt.Get();
}

base::TimeDelta GetMaxTimeToPrompt() {
  return features::kTrustSafetySentimentSurveyMaxTimeToPrompt.Get();
}

int GetRequiredNtpCount() {
  return base::RandInt(
      features::kTrustSafetySentimentSurveyNtpVisitsMinRange.Get(),
      features::kTrustSafetySentimentSurveyNtpVisitsMaxRange.Get());
}

int GetMaxRequiredNtpCount() {
  return features::kTrustSafetySentimentSurveyNtpVisitsMaxRange.Get();
}

std::string GetHatsTriggerForFeatureArea(
    TrustSafetySentimentService::FeatureArea feature_area) {
  switch (feature_area) {
    case (TrustSafetySentimentService::FeatureArea::kPrivacySettings):
      return kHatsSurveyTriggerTrustSafetyPrivacySettings;
    case (TrustSafetySentimentService::FeatureArea::kTrustedSurface):
      return kHatsSurveyTriggerTrustSafetyTrustedSurface;
    case (TrustSafetySentimentService::FeatureArea::kTransactions):
      return kHatsSurveyTriggerTrustSafetyTransactions;
    default:
      NOTREACHED();
      return "";
  }
}

bool ProbabilityCheck(TrustSafetySentimentService::FeatureArea feature_area) {
  switch (feature_area) {
    case (TrustSafetySentimentService::FeatureArea::kPrivacySettings):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyPrivacySettingsProbability
                 .Get();
    case (TrustSafetySentimentService::FeatureArea::kTrustedSurface):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyTrustedSurfaceProbability
                 .Get();
    case (TrustSafetySentimentService::FeatureArea::kTransactions):
      return base::RandDouble() <
             features::kTrustSafetySentimentSurveyTransactionsProbability.Get();
    default:
      NOTREACHED();
      return false;
  }
}

bool HasNonDefaultPrivacySetting(Profile* profile) {
  auto* prefs = profile->GetPrefs();

  std::vector<std::string> prefs_to_check = {
      prefs::kSafeBrowsingEnabled,
      prefs::kSafeBrowsingEnhanced,
      prefs::kSafeBrowsingScoutReportingEnabled,
      prefs::kEnableDoNotTrack,
      password_manager::prefs::kPasswordLeakDetectionEnabled,
      prefs::kCookieControlsMode,
  };

  bool has_non_default_pref = false;
  for (const auto& pref_name : prefs_to_check) {
    auto* pref = prefs->FindPreference(pref_name);
    if (!pref->IsDefaultValue() && pref->IsUserControlled()) {
      has_non_default_pref = true;
      break;
    }
  }

  // Users consenting to sync automatically enable UKM collection
  auto* ukm_pref = prefs->FindPreference(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled);
  auto* sync_consent_pref =
      prefs->FindPreference(prefs::kGoogleServicesConsentedToSync);

  bool has_non_default_ukm =
      ukm_pref->GetValue()->GetBool() !=
          sync_consent_pref->GetValue()->GetBool() &&
      (ukm_pref->IsUserControlled() || sync_consent_pref->IsUserControlled());

  // Check the default value for each user facing content setting. Note that
  // this will not include content setting exceptions set via permission
  // prompts, as they are site specific.
  bool has_non_default_content_setting = false;
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);

  for (auto content_setting_type :
       site_settings::GetVisiblePermissionCategories()) {
    std::string content_setting_provider;
    auto current_value = map->GetDefaultContentSetting(
        content_setting_type, &content_setting_provider);
    auto content_setting_source =
        HostContentSettingsMap::GetSettingSourceFromProviderName(
            content_setting_provider);

    const bool user_controlled =
        content_setting_source ==
            content_settings::SettingSource::SETTING_SOURCE_NONE ||
        content_setting_source ==
            content_settings::SettingSource::SETTING_SOURCE_USER;

    auto default_value = static_cast<ContentSetting>(
        content_settings::WebsiteSettingsRegistry::GetInstance()
            ->Get(content_setting_type)
            ->initial_default_value()
            ->GetInt());

    if (current_value != default_value && user_controlled) {
      has_non_default_content_setting = true;
      break;
    }
  }

  return has_non_default_pref || has_non_default_ukm ||
         has_non_default_content_setting;
}

// Generates the Product Specific Data which accompanies survey results for the
// Privacy Settings product area. This includes whether the user is receiving
// the survey because they ran safety check, and whether they have any
// non-default core privacy settings.
std::map<std::string, bool> GetPrivacySettingsProductSpecificData(
    Profile* profile,
    bool ran_safety_check) {
  std::map<std::string, bool> product_specific_data;
  product_specific_data["Non default setting"] =
      HasNonDefaultPrivacySetting(profile);
  product_specific_data["Ran safety check"] = ran_safety_check;
  return product_specific_data;
}

}  // namespace

TrustSafetySentimentService::TrustSafetySentimentService(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
  observed_profiles_.AddObservation(profile);

  // As this service is created lazily, there may already be a primary OTR
  // profile created for the main profile.
  if (auto* primary_otr =
          profile->GetPrimaryOTRProfile(/*create_if_needed=*/false)) {
    observed_profiles_.AddObservation(primary_otr);
  }
}

TrustSafetySentimentService::~TrustSafetySentimentService() = default;

void TrustSafetySentimentService::OpenedNewTabPage() {
  // Explicit early exit for the common path, where the user has not performed
  // any of the trigger actions.
  if (pending_triggers_.size() == 0)
    return;

  // Reduce the NTPs to open count for all the active triggers.
  for (auto& area_trigger : pending_triggers_) {
    auto& trigger = area_trigger.second;
    if (trigger.remaining_ntps_to_open > 0)
      trigger.remaining_ntps_to_open--;
  }

  // Cleanup any triggers which are no longer relevant. This will be every
  // trigger which occurred more than the maximum prompt time ago, or the
  // trigger for the kIneligible area if it is no longer blocking
  // eligibility.
  base::EraseIf(pending_triggers_,
                [](const std::pair<FeatureArea, PendingTrigger>& area_trigger) {
                  return base::Time::Now() - area_trigger.second.occurred_time >
                             GetMaxTimeToPrompt() ||
                         (area_trigger.first == FeatureArea::kIneligible &&
                          !ShouldBlockSurvey(area_trigger.second));
                });

  // This may have emptied the set of pending triggers.
  if (pending_triggers_.size() == 0)
    return;

  // A primary OTR profile (incognito) existing will prevent any surveys from
  // being shown.
  if (profile_->HasPrimaryOTRProfile())
    return;

  // Check if any of the triggers make the user not yet eligible to receive a
  // survey.
  for (const auto& area_trigger : pending_triggers_) {
    if (ShouldBlockSurvey(area_trigger.second))
      return;
  }

  // Choose a trigger at random to avoid any order biasing.
  auto winning_area_iterator = pending_triggers_.begin();
  std::advance(winning_area_iterator,
               base::RandInt(0, pending_triggers_.size() - 1));

  // The winning feature area should never be kIneligible, as this will
  // have either been removed above, or blocked showing any survey.
  DCHECK(winning_area_iterator->first != FeatureArea::kIneligible);

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);

  // A null HaTS service should have prevented this service from being created.
  DCHECK(hats_service);
  hats_service->LaunchSurvey(
      GetHatsTriggerForFeatureArea(winning_area_iterator->first),
      /*success_callback=*/base::DoNothing(),
      /*failure_callback=*/base::DoNothing(),
      winning_area_iterator->second.product_specific_data);
  base::UmaHistogramEnumeration("Feedback.TrustSafetySentiment.SurveyRequested",
                                winning_area_iterator->first);
  pending_triggers_.clear();
}

void TrustSafetySentimentService::InteractedWithPrivacySettings(
    content::WebContents* web_contents) {
  // Only observe one instance settings at a time. This ignores both multiple
  // instances of settings, and repeated interactions with settings. This
  // reduces the chance that a user is eligible for a survey, but is much
  // simpler. As interactions with settings (visiting password manager and using
  // the privacy card) can occur independently, there is also little risk of
  // starving one interaction.
  if (settings_watcher_)
    return;

  settings_watcher_ = std::make_unique<SettingsWatcher>(
      web_contents,
      features::kTrustSafetySentimentSurveyPrivacySettingsTime.Get(),
      base::BindOnce(&TrustSafetySentimentService::TriggerOccurred,
                     weak_ptr_factory_.GetWeakPtr(),
                     FeatureArea::kPrivacySettings,
                     GetPrivacySettingsProductSpecificData(
                         profile_, /*ran_safety_check=*/false)),
      base::BindOnce(&TrustSafetySentimentService::SettingsWatcherComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrustSafetySentimentService::RanSafetyCheck() {
  TriggerOccurred(FeatureArea::kPrivacySettings,
                  GetPrivacySettingsProductSpecificData(
                      profile_, /*ran_safety_check=*/true));
}

void TrustSafetySentimentService::PageInfoOpened() {
  // Only one Page Info should ever be open.
  DCHECK(!page_info_state_);
  page_info_state_ = std::make_unique<PageInfoState>();
}

void TrustSafetySentimentService::InteractedWithPageInfo() {
  DCHECK(page_info_state_);
  page_info_state_->interacted = true;
}

void TrustSafetySentimentService::PageInfoClosed() {
  DCHECK(page_info_state_);

  // Record a trigger if either the user had page info open for the required
  // time, or if they interacted with it.
  if (base::Time::Now() - page_info_state_->opened_time >=
          features::kTrustSafetySentimentSurveyTrustedSurfaceTime.Get() ||
      page_info_state_->interacted) {
    TriggerOccurred(
        FeatureArea::kTrustedSurface,
        {{"Interacted with Page Info", page_info_state_->interacted}});
  }

  page_info_state_.reset();
}

void TrustSafetySentimentService::SavedPassword() {
  TriggerOccurred(FeatureArea::kTransactions, {{"Saved password", true}});
}

void TrustSafetySentimentService::OpenedPasswordManager(
    content::WebContents* web_contents) {
  if (settings_watcher_)
    return;

  std::map<std::string, bool> product_specific_data = {
      {"Saved password", false}};

  settings_watcher_ = std::make_unique<SettingsWatcher>(
      web_contents,
      features::kTrustSafetySentimentSurveyTransactionsPasswordManagerTime
          .Get(),
      base::BindOnce(&TrustSafetySentimentService::TriggerOccurred,
                     weak_ptr_factory_.GetWeakPtr(), FeatureArea::kTransactions,
                     product_specific_data),
      base::BindOnce(&TrustSafetySentimentService::SettingsWatcherComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrustSafetySentimentService::SavedCard() {
  TriggerOccurred(FeatureArea::kTransactions, {{"Saved password", false}});
}

void TrustSafetySentimentService::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  // Only interested in the primary OTR profile i.e. the one used for incognito
  // browsing. Non-primary OTR profiles are often used as implementation details
  // of other features, and are not inherintly relevant to Trust & Safety.
  if (off_the_record->GetOTRProfileID() == Profile::OTRProfileID::PrimaryID())
    observed_profiles_.AddObservation(off_the_record);
}

void TrustSafetySentimentService::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);

  if (profile->IsOffTheRecord()) {
    // Closing the incognito profile, which is the only OTR profie observed by
    // this class, is an ileligible action.
    PerformedIneligibleAction();
  }
}

TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    const std::map<std::string, bool>& product_specific_data,
    int remaining_ntps_to_open)
    : product_specific_data(product_specific_data),
      remaining_ntps_to_open(remaining_ntps_to_open),
      occurred_time(base::Time::Now()) {}

TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    int remaining_ntps_to_open)
    : remaining_ntps_to_open(remaining_ntps_to_open),
      occurred_time(base::Time::Now()) {}

TrustSafetySentimentService::PendingTrigger::PendingTrigger() = default;
TrustSafetySentimentService::PendingTrigger::~PendingTrigger() = default;
TrustSafetySentimentService::PendingTrigger::PendingTrigger(
    const PendingTrigger& other) = default;

TrustSafetySentimentService::SettingsWatcher::SettingsWatcher(
    content::WebContents* web_contents,
    base::TimeDelta required_open_time,
    base::OnceCallback<void()> success_callback,
    base::OnceCallback<void()> complete_callback)
    : web_contents_(web_contents),
      success_callback_(std::move(success_callback)),
      complete_callback_(std::move(complete_callback)) {
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &TrustSafetySentimentService::SettingsWatcher::TimerComplete,
          weak_ptr_factory_.GetWeakPtr()),
      required_open_time);
  Observe(web_contents);
}

TrustSafetySentimentService::SettingsWatcher::~SettingsWatcher() = default;

void TrustSafetySentimentService::SettingsWatcher::WebContentsDestroyed() {
  std::move(complete_callback_).Run();
}

void TrustSafetySentimentService::SettingsWatcher::TimerComplete() {
  const bool stayed_on_settings =
      web_contents_ &&
      web_contents_->GetVisibility() == content::Visibility::VISIBLE &&
      web_contents_->GetLastCommittedURL().host_piece() ==
          chrome::kChromeUISettingsHost;
  if (stayed_on_settings)
    std::move(success_callback_).Run();

  std::move(complete_callback_).Run();
}

TrustSafetySentimentService::PageInfoState::PageInfoState()
    : opened_time(base::Time::Now()) {}

void TrustSafetySentimentService::SettingsWatcherComplete() {
  settings_watcher_.reset();
}

void TrustSafetySentimentService::TriggerOccurred(
    FeatureArea feature_area,
    const std::map<std::string, bool>& product_specific_data) {
  if (!ProbabilityCheck(feature_area))
    return;

  base::UmaHistogramEnumeration("Feedback.TrustSafetySentiment.TriggerOccurred",
                                feature_area);

  // This will overwrite any previous trigger for this feature area. We are
  // only interested in the most recent trigger, so this is acceptable.
  pending_triggers_[feature_area] =
      PendingTrigger(product_specific_data, GetRequiredNtpCount());
}

void TrustSafetySentimentService::PerformedIneligibleAction() {
  pending_triggers_[FeatureArea::kIneligible] =
      PendingTrigger(GetMaxRequiredNtpCount());

  base::UmaHistogramEnumeration("Feedback.TrustSafetySentiment.TriggerOccurred",
                                FeatureArea::kIneligible);
}

/*static*/ bool TrustSafetySentimentService::ShouldBlockSurvey(
    const PendingTrigger& trigger) {
  return base::Time::Now() - trigger.occurred_time < GetMinTimeToPrompt() ||
         trigger.remaining_ntps_to_open > 0;
}
