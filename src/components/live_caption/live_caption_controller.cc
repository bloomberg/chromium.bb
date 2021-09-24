// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_caption_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "media/base/media_switches.h"
#include "ui/native_theme/native_theme.h"

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

LiveCaptionController::LiveCaptionController(PrefService* profile_prefs,
                                             PrefService* global_prefs)
    : profile_prefs_(profile_prefs), global_prefs_(global_prefs) {}

LiveCaptionController::~LiveCaptionController() {
  if (enabled_) {
    enabled_ = false;
    StopLiveCaption();
  }
}

// static
void LiveCaptionController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kLiveCaptionEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Initially default the language to en-US.
  registry->RegisterStringPref(prefs::kLiveCaptionLanguageCode, "en-US",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void LiveCaptionController::Init() {
  base::UmaHistogramBoolean("Accessibility.LiveCaption.FeatureEnabled",
                            media::IsLiveCaptionFeatureEnabled());

  // Hidden behind a feature flag.
  if (!media::IsLiveCaptionFeatureEnabled())
    return;

  base::UmaHistogramBoolean(
      "Accessibility.LiveCaption.UseSodaForLiveCaption",
      base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption));
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line &&
      command_line->HasSwitch(switches::kEnableLiveCaptionPrefForTesting)) {
    profile_prefs_->SetBoolean(prefs::kLiveCaptionEnabled, true);
  }

  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&LiveCaptionController::OnLiveCaptionEnabledChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLiveCaptionLanguageCode,
      base::BindRepeating(&LiveCaptionController::OnLiveCaptionLanguageChanged,
                          base::Unretained(this)));

  enabled_ = IsLiveCaptionEnabled();
  base::UmaHistogramBoolean("Accessibility.LiveCaption", enabled_);
  if (enabled_) {
    StartLiveCaption();
  } else {
    StopLiveCaption();
  }
}

void LiveCaptionController::OnLiveCaptionEnabledChanged() {
  bool enabled = IsLiveCaptionEnabled();
  if (enabled == enabled_)
    return;
  enabled_ = enabled;

  if (enabled) {
    StartLiveCaption();
  } else {
    StopLiveCaption();
    speech::SodaInstaller::GetInstance()->SetUninstallTimer(profile_prefs_,
                                                            global_prefs_);
  }
}

void LiveCaptionController::OnLiveCaptionLanguageChanged() {
  if (enabled_)
    speech::SodaInstaller::GetInstance()->InstallLanguage(
        profile_prefs_->GetString(prefs::kLiveCaptionLanguageCode),
        global_prefs_);
}

bool LiveCaptionController::IsLiveCaptionEnabled() {
  return profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled);
}

void LiveCaptionController::StartLiveCaption() {
  DCHECK(enabled_);
  if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {
    CreateUI();
    return;
  }

  // The SodaInstaller determines whether SODA is already on the device and
  // whether or not to download. Once SODA is on the device and ready, the
  // SODAInstaller calls OnSodaInstalled on its observers. The UI is created at
  // that time.
  const std::string locale =
      profile_prefs_->GetString(prefs::kLiveCaptionLanguageCode);
  if (speech::SodaInstaller::GetInstance()->IsSodaInstalled(
          speech::GetLanguageCode(locale))) {
    CreateUI();
  } else {
    speech::SodaInstaller::GetInstance()->AddObserver(this);
    speech::SodaInstaller::GetInstance()->Init(profile_prefs_, global_prefs_);
  }
}

void LiveCaptionController::StopLiveCaption() {
  DCHECK(!enabled_);
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  DestroyUI();
}

void LiveCaptionController::OnSodaInstalled() {
  // Live Caption should always be enabled when this is called. If Live Caption
  // has been disabled, then this should not be observing the SodaInstaller
  // anymore.
  DCHECK(enabled_);
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  CreateUI();
}

void LiveCaptionController::CreateUI() {
  if (is_ui_constructed_)
    return;

  is_ui_constructed_ = true;

  caption_bubble_controller_ = CaptionBubbleController::Create();
  caption_bubble_controller_->UpdateCaptionStyle(caption_style_);

  // Observe native theme changes for caption style updates.
  ui::NativeTheme::GetInstanceForWeb()->AddObserver(this);

  // Observe caption style prefs.
  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    DCHECK(!pref_change_registrar_->IsObserved(pref_name));
    pref_change_registrar_->Add(
        pref_name,
        base::BindRepeating(&LiveCaptionController::OnCaptionStyleUpdated,
                            base::Unretained(this)));
  }
  OnCaptionStyleUpdated();
}

void LiveCaptionController::DestroyUI() {
  if (!is_ui_constructed_)
    return;
  is_ui_constructed_ = false;
  caption_bubble_controller_.reset(nullptr);

  // Remove native theme observer.
  ui::NativeTheme::GetInstanceForWeb()->RemoveObserver(this);

  // Remove prefs to observe.
  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    DCHECK(pref_change_registrar_->IsObserved(pref_name));
    pref_change_registrar_->Remove(pref_name);
  }
}

bool LiveCaptionController::DispatchTranscription(
    CaptionBubbleContext* caption_bubble_context,
    const media::SpeechRecognitionResult& result) {
  if (!caption_bubble_controller_)
    return false;
  return caption_bubble_controller_->OnTranscription(caption_bubble_context,
                                                     result);
}

void LiveCaptionController::OnError(
    CaptionBubbleContext* caption_bubble_context) {
  if (!caption_bubble_controller_)
    return;
  caption_bubble_controller_->OnError(caption_bubble_context);
}

void LiveCaptionController::OnAudioStreamEnd(
    CaptionBubbleContext* caption_bubble_context) {
  if (!caption_bubble_controller_)
    return;
  caption_bubble_controller_->OnAudioStreamEnd(caption_bubble_context);
}

void LiveCaptionController::OnLanguageIdentificationEvent(
    const media::mojom::LanguageIdentificationEventPtr& event) {
  // TODO(crbug.com/1175357): Implement the UI for language identification.
}

#if defined(OS_MAC) || defined(OS_CHROMEOS)
void LiveCaptionController::OnToggleFullscreen(
    CaptionBubbleContext* caption_bubble_context) {
  if (!enabled_)
    return;
  // The easiest way to move the Live Caption UI to the right workspace is to
  // simply destroy and recreate the UI. The UI will automatically be created
  // in the workspace of the browser window that is transmitting captions.
  DestroyUI();
  CreateUI();
}
#endif

void LiveCaptionController::OnCaptionStyleUpdated() {
  // Metrics are recorded when passing the caption prefs to the browser, so do
  // not duplicate them here.
  caption_style_ = GetCaptionStyleFromUserSettings(profile_prefs_,
                                                   false /* record_metrics */);
  caption_bubble_controller_->UpdateCaptionStyle(caption_style_);
}

}  // namespace captions
