// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_CONTROLLER_H_
#define COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_observer.h"

class PrefChangeRegistrar;

namespace ui {
class NativeTheme;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace captions {

class CaptionBubbleController;
class CaptionBubbleContext;

///////////////////////////////////////////////////////////////////////////////
// Live Caption Controller
//
//  The controller of the live caption feature. It enables the captioning
//  service when the preference is enabled. The live caption controller is a
//  KeyedService. There exists one live caption controller per profile and it
//  lasts for the duration of the session. The live caption controller owns the
//  live caption UI, which is a caption bubble controller.
//
class LiveCaptionController : public KeyedService,
                              public speech::SodaInstaller::Observer,
                              public ui::NativeThemeObserver {
 public:
  explicit LiveCaptionController(PrefService* profile_prefs,
                                 PrefService* global_prefs);
  ~LiveCaptionController() override;
  LiveCaptionController(const LiveCaptionController&) = delete;
  LiveCaptionController& operator=(const LiveCaptionController&) = delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void Init();

  // Routes a transcription to the CaptionBubbleController. Returns whether the
  // transcription result was routed successfully. Transcriptions will halt if
  // this returns false.
  bool DispatchTranscription(CaptionBubbleContext* caption_bubble_context,
                             const media::SpeechRecognitionResult& result);

  void OnLanguageIdentificationEvent(
      const media::mojom::LanguageIdentificationEventPtr& event);

  // Alerts the CaptionBubbleController that there is an error in the speech
  // recognition service.
  void OnError(CaptionBubbleContext* caption_bubble_context);

  // Alerts the CaptionBubbleController that the audio stream has ended.
  void OnAudioStreamEnd(CaptionBubbleContext* caption_bubble_context);

  // Mac and ChromeOS move the fullscreened window into a new workspace. When
  // the WebContents associated with the CaptionBubbleContext goes
  // fullscreen, ensure that the Live Caption bubble moves to the new workspace.
#if defined(OS_MAC) || defined(OS_CHROMEOS)
  void OnToggleFullscreen(CaptionBubbleContext* caption_bubble_context);
#endif

 private:
  friend class LiveCaptionControllerFactory;
  friend class LiveCaptionControllerTest;
  friend class LiveCaptionSpeechRecognitionHostTest;

  // SodaInstaller::Observer:
  void OnSodaInstalled() override;
  void OnSodaProgress(int combined_progress) override {}
  void OnSodaError() override {}

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override {}
  void OnCaptionStyleUpdated() override;

  void OnLiveCaptionEnabledChanged();
  void OnLiveCaptionLanguageChanged();
  bool IsLiveCaptionEnabled();
  void StartLiveCaption();
  void StopLiveCaption();
  void CreateUI();
  void DestroyUI();

  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<PrefService> global_prefs_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<CaptionBubbleController> caption_bubble_controller_;
  absl::optional<ui::CaptionStyle> caption_style_;

  // Whether Live Caption is enabled.
  bool enabled_ = false;

  // Whether the UI has been created. The UI is created asynchronously from the
  // feature being enabled--we wait for SODA to download first. This flag
  // ensures that the UI is not constructed or deconstructed twice.
  bool is_ui_constructed_ = false;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_CONTROLLER_H_
