// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/accessibility/caption_util.h"
#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/caption_bubble_controller.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/soda/constants.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"

namespace {

const char* const kCaptionStylePrefsToObserve[] = {
    prefs::kAccessibilityCaptionsTextSize,
    prefs::kAccessibilityCaptionsTextFont,
    prefs::kAccessibilityCaptionsTextColor,
    prefs::kAccessibilityCaptionsTextOpacity,
    prefs::kAccessibilityCaptionsBackgroundColor,
    prefs::kAccessibilityCaptionsTextShadow,
    prefs::kAccessibilityCaptionsBackgroundOpacity};

constexpr int kSodaCleanUpDelayInDays = 30;

}  // namespace

namespace captions {

CaptionController::CaptionController(Profile* profile) : profile_(profile) {}

CaptionController::~CaptionController() {
  if (enabled_) {
    enabled_ = false;
    StopLiveCaption();
  }
}

// static
void CaptionController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kLiveCaptionEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Initially default the language to en-US.
  registry->RegisterStringPref(prefs::kLiveCaptionLanguageCode, "en-US");
}

void CaptionController::Init() {
  base::UmaHistogramBoolean("Accessibility.LiveCaption.FeatureEnabled",
                            base::FeatureList::IsEnabled(media::kLiveCaption));

  // Hidden behind a feature flag.
  if (!base::FeatureList::IsEnabled(media::kLiveCaption))
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Return early if current profile is a signin profile (as opposed to a user
  // profile). See crbug.com/1180706
  // TODO(crbug.com/1180706): Remove this check if bug can be resolved through
  // other means.
  if (ash::ProfileHelper::IsSigninProfile(profile_))
    return;
#endif

  base::UmaHistogramBoolean(
      "Accessibility.LiveCaption.UseSodaForLiveCaption",
      base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption));
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line &&
      command_line->HasSwitch(switches::kEnableLiveCaptionPrefForTesting)) {
    profile_->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled, true);
  }

  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&CaptionController::OnLiveCaptionEnabledChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLiveCaptionLanguageCode,
      base::BindRepeating(&CaptionController::OnLiveCaptionLanguageChanged,
                          base::Unretained(this)));

  enabled_ = IsLiveCaptionEnabled();
  if (enabled_) {
    StartLiveCaption();
  } else {
    StopLiveCaption();
  }

  content::BrowserAccessibilityState::GetInstance()
      ->AddUIThreadHistogramCallback(base::BindOnce(
          &CaptionController::UpdateAccessibilityCaptionHistograms,
          base::Unretained(this)));
}

void CaptionController::OnLiveCaptionEnabledChanged() {
  bool enabled = IsLiveCaptionEnabled();
  if (enabled == enabled_)
    return;
  enabled_ = enabled;

  if (enabled) {
    StartLiveCaption();
  } else {
    StopLiveCaption();
    // Schedule SODA to be deleted in 30 days if the feature is not enabled
    // before then.
    g_browser_process->local_state()->SetTime(
        prefs::kSodaScheduledDeletionTime,
        base::Time::Now() + base::TimeDelta::FromDays(kSodaCleanUpDelayInDays));
  }
}

void CaptionController::OnLiveCaptionLanguageChanged() {
  if (enabled_)
    speech::SodaInstaller::GetInstance()->InstallLanguage(profile_->GetPrefs());
}

bool CaptionController::IsLiveCaptionEnabled() {
  PrefService* profile_prefs = profile_->GetPrefs();
  return profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled);
}

void CaptionController::StartLiveCaption() {
  DCHECK(enabled_);
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) ||
      speech::SodaInstaller::GetInstance()->IsSodaInstalled()) {
    CreateUI();
    return;
  }

  // Ask the SodaInstaller to download the speech model and language pack. The
  // SodaInstaller determines whether SODA is already on the device and whether
  // or not to download. Once SODA is on the device and ready, the SodaInstaller
  // calls OnSodaInstalled on its observers. The UI is created at that time.
  g_browser_process->local_state()->SetTime(prefs::kSodaScheduledDeletionTime,
                                            base::Time());
  speech::SodaInstaller::GetInstance()->AddObserver(this);
  speech::SodaInstaller::GetInstance()->InstallSoda(profile_->GetPrefs());
  speech::SodaInstaller::GetInstance()->InstallLanguage(profile_->GetPrefs());
}

void CaptionController::StopLiveCaption() {
  DCHECK(!enabled_);
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  DestroyUI();
}

void CaptionController::OnSodaInstalled() {
  // Live Caption should always be enabled when this is called. If Live Caption
  // has been disabled, then this should not be observing the SodaInstaller
  // anymore.
  DCHECK(enabled_);
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  CreateUI();
}

void CaptionController::CreateUI() {
  DCHECK(enabled_);
  if (is_ui_constructed_)
    return;
  DCHECK(!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) ||
         speech::SodaInstaller::GetInstance()->IsSodaInstalled());
  is_ui_constructed_ = true;
  // Create captions UI in each browser view.
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserAdded(browser);
  }

  // Add observers to the BrowserList for new browser views being added.
  BrowserList::GetInstance()->AddObserver(this);

  // Observe caption style prefs.
  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    DCHECK(!pref_change_registrar_->IsObserved(pref_name));
    pref_change_registrar_->Add(
        pref_name, base::BindRepeating(&CaptionController::UpdateCaptionStyle,
                                       base::Unretained(this)));
  }
  UpdateCaptionStyle();
}

void CaptionController::DestroyUI() {
  DCHECK(!enabled_);
  if (!is_ui_constructed_)
    return;
  is_ui_constructed_ = false;
  // Destroy caption bubble controllers.
  caption_bubble_controllers_.clear();

  // Remove observers.
  BrowserList::GetInstance()->RemoveObserver(this);

  // Remove prefs to observe.
  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    DCHECK(pref_change_registrar_->IsObserved(pref_name));
    pref_change_registrar_->Remove(pref_name);
  }
}

void CaptionController::UpdateAccessibilityCaptionHistograms() {
  base::UmaHistogramBoolean("Accessibility.LiveCaption", enabled_);
}

void CaptionController::OnBrowserAdded(Browser* browser) {
  if (browser->profile() != profile_ &&
      browser->profile()->GetOriginalProfile() != profile_) {
    return;
  }

  DCHECK(!caption_bubble_controllers_.count(browser));
  caption_bubble_controllers_[browser] =
      CaptionBubbleController::Create(browser);
  caption_bubble_controllers_[browser]->UpdateCaptionStyle(caption_style_);
}

void CaptionController::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() != profile_ &&
      browser->profile()->GetOriginalProfile() != profile_) {
    return;
  }

  DCHECK(caption_bubble_controllers_.count(browser));
  caption_bubble_controllers_.erase(browser);
}

bool CaptionController::DispatchTranscription(
    content::WebContents* web_contents,
    const chrome::mojom::TranscriptionResultPtr& transcription_result) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser || !caption_bubble_controllers_.count(browser))
    return false;
  return caption_bubble_controllers_[browser]->OnTranscription(
      transcription_result, web_contents);
}

void CaptionController::OnError(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser || !caption_bubble_controllers_.count(browser))
    return;
  return caption_bubble_controllers_[browser]->OnError(web_contents);
}

CaptionBubbleController*
CaptionController::GetCaptionBubbleControllerForBrowser(Browser* browser) {
  if (!browser || !caption_bubble_controllers_.count(browser))
    return nullptr;
  return caption_bubble_controllers_[browser].get();
}

void CaptionController::UpdateCaptionStyle() {
  PrefService* profile_prefs = profile_->GetPrefs();
  // Metrics are recorded when passing the caption prefs to the browser, so do
  // not duplicate them here.
  caption_style_ = GetCaptionStyleFromUserSettings(profile_prefs,
                                                   false /* record_metrics */);

  for (const auto& item : caption_bubble_controllers_) {
    caption_bubble_controllers_[item.first]->UpdateCaptionStyle(caption_style_);
  }
}

}  // namespace captions
