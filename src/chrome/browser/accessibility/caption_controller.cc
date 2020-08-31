// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_controller.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/accessibility/caption_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/caption_bubble_controller.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
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

}  // namespace

namespace captions {

CaptionController::CaptionController(Profile* profile) : profile_(profile) {}

CaptionController::~CaptionController() = default;

// static
void CaptionController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kLiveCaptionEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterFilePathPref(prefs::kSODAPath, base::FilePath());
}

// static
void CaptionController::InitOffTheRecordPrefs(Profile* off_the_record_profile) {
  DCHECK(off_the_record_profile->IsOffTheRecord());
  off_the_record_profile->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                                 false);
  off_the_record_profile->GetPrefs()->SetFilePath(prefs::kSODAPath,
                                                  base::FilePath());
}

void CaptionController::Init() {
  // Hidden behind a feature flag.
  if (!base::FeatureList::IsEnabled(media::kLiveCaption))
    return;

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&CaptionController::OnLiveCaptionEnabledChanged,
                          base::Unretained(this)));

  enabled_ = IsLiveCaptionEnabled();
  if (enabled_)
    UpdateUIEnabled();

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

  UpdateSpeechRecognitionServiceEnabled();
  UpdateUIEnabled();
}

bool CaptionController::IsLiveCaptionEnabled() {
  PrefService* profile_prefs = profile_->GetPrefs();
  return profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled);
}

void CaptionController::UpdateSpeechRecognitionServiceEnabled() {
  if (enabled_) {
    // Register SODA component and download speech model.
    component_updater::RegisterSODAComponent(
        g_browser_process->component_updater(), profile_->GetPrefs(),
        base::BindOnce(&component_updater::SODAComponentInstallerPolicy::
                           UpdateSODAComponentOnDemand));
  } else {
    // TODO(evliu): Unregister SODA component.
  }
}

void CaptionController::UpdateUIEnabled() {
  if (enabled_) {
    // Create captions UI in each browser view.
    for (Browser* browser : *BrowserList::GetInstance()) {
      OnBrowserAdded(browser);
    }

    // Add observers to the BrowserList for new browser views being added.
    BrowserList::GetInstance()->AddObserver(this);

    // Observe caption style prefs.
    for (const char* const pref_name : kCaptionStylePrefsToObserve) {
      pref_change_registrar_->Add(
          pref_name, base::BindRepeating(&CaptionController::UpdateCaptionStyle,
                                         base::Unretained(this)));
    }
    UpdateCaptionStyle();
  } else {
    // Destroy caption bubble controllers.
    caption_bubble_controllers_.clear();

    // Remove observers.
    BrowserList::GetInstance()->RemoveObserver(this);

    // Remove prefs to observe.
    for (const char* const pref_name : kCaptionStylePrefsToObserve) {
      pref_change_registrar_->Remove(pref_name);
    }
  }
}

void CaptionController::UpdateAccessibilityCaptionHistograms() {
  base::UmaHistogramBoolean("Accessibility.LiveCaptions", enabled_);
}

void CaptionController::OnBrowserAdded(Browser* browser) {
  if (browser->profile() != profile_)
    return;

  caption_bubble_controllers_[browser] =
      CaptionBubbleController::Create(browser);
  caption_bubble_controllers_[browser]->UpdateCaptionStyle(caption_style_);
}

void CaptionController::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() != profile_)
    return;

  DCHECK(caption_bubble_controllers_.count(browser));
  caption_bubble_controllers_.erase(browser);
}

void CaptionController::DispatchTranscription(
    content::WebContents* web_contents,
    const chrome::mojom::TranscriptionResultPtr& transcription_result) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;
  if (!caption_bubble_controllers_.count(browser))
    return;
  caption_bubble_controllers_[browser]->OnTranscription(transcription_result,
                                                        web_contents);
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
