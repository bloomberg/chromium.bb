// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_GOOGLE_ASSISTANT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_GOOGLE_ASSISTANT_HANDLER_H_

#include "ash/components/audio/cras_audio_handler.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace settings {

class GoogleAssistantHandler : public ::settings::SettingsPageUIHandler,
                               chromeos::CrasAudioHandler::AudioObserver {
 public:
  GoogleAssistantHandler();

  GoogleAssistantHandler(const GoogleAssistantHandler&) = delete;
  GoogleAssistantHandler& operator=(const GoogleAssistantHandler&) = delete;

  ~GoogleAssistantHandler() override;

  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // chromeos::CrasAudioHandler::AudioObserver overrides
  void OnAudioNodesChanged() override;

 private:
  // WebUI call to launch into the Google Assistant app settings.
  void HandleShowGoogleAssistantSettings(const base::ListValue* args);
  // WebUI call to retrain Assistant voice model.
  void HandleRetrainVoiceModel(const base::ListValue* args);
  // WebUI call to sync Assistant voice model status.
  void HandleSyncVoiceModelStatus(const base::ListValue* args);
  // WebUI call to signal js side is ready.
  void HandleInitialized(const base::ListValue* args);

  bool pending_hotword_update_ = false;

  base::WeakPtrFactory<GoogleAssistantHandler> weak_factory_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_GOOGLE_ASSISTANT_HANDLER_H_
