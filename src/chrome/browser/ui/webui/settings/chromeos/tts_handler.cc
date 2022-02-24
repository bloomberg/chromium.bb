// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/tts_handler.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {
TtsHandler::TtsHandler() {}

TtsHandler::~TtsHandler() {
  RemoveTtsControllerDelegates();
}

void TtsHandler::HandleGetAllTtsVoiceData(base::Value::ConstListView args) {
  OnVoicesChanged();
}

void TtsHandler::HandleGetTtsExtensions(base::Value::ConstListView args) {
  // Ensure the built in tts engine is loaded to be able to respond to messages.
  WakeTtsEngine(base::Value::ConstListView());

  base::ListValue responses;
  Profile* profile = Profile::FromWebUI(web_ui());
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);

  const std::set<std::string> extensions =
      TtsEngineExtensionObserverChromeOS::GetInstance(profile)
          ->engine_extension_ids();
  std::set<std::string>::const_iterator iter;
  for (iter = extensions.begin(); iter != extensions.end(); ++iter) {
    const std::string extension_id = *iter;
    const extensions::Extension* extension =
        registry->GetInstalledExtension(extension_id);
    if (!extension) {
      // The extension is still loading from OnVoicesChange call to
      // TtsController::GetVoices(). Don't do any work, voices will
      // be updated again after extension load.
      continue;
    }
    base::DictionaryValue response;
    response.SetStringKey("name", extension->name());
    response.SetStringKey("extensionId", extension_id);
    if (extensions::OptionsPageInfo::HasOptionsPage(extension)) {
      response.SetStringKey(
          "optionsPage",
          extensions::OptionsPageInfo::GetOptionsPage(extension).spec());
    }
    responses.Append(std::move(response));
  }

  FireWebUIListener("tts-extensions-updated", responses);
}

void TtsHandler::OnVoicesChanged() {
  content::TtsController* tts_controller =
      content::TtsController::GetInstance();
  std::vector<content::VoiceData> voices;
  tts_controller->GetVoices(Profile::FromWebUI(web_ui()), GURL(), &voices);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  base::ListValue responses;
  for (const auto& voice : voices) {
    base::DictionaryValue response;
    int language_score = GetVoiceLangMatchScore(&voice, app_locale);
    std::string language_code;
    if (voice.lang.empty()) {
      language_code = "noLanguageCode";
      response.SetStringKey(
          "displayLanguage",
          l10n_util::GetStringUTF8(IDS_TEXT_TO_SPEECH_SETTINGS_NO_LANGUAGE));
    } else {
      language_code = l10n_util::GetLanguage(voice.lang);
      response.SetStringKey(
          "displayLanguage",
          l10n_util::GetDisplayNameForLocale(
              language_code, g_browser_process->GetApplicationLocale(), true));
    }
    response.SetStringKey("name", voice.name);
    response.SetStringKey("languageCode", language_code);
    response.SetStringKey("fullLanguageCode", voice.lang);
    response.SetIntKey("languageScore", language_score);
    response.SetStringKey("extensionId", voice.engine_id);
    responses.Append(std::move(response));
  }
  AllowJavascript();
  FireWebUIListener("all-voice-data-updated", responses);

  // Also refresh the TTS extensions in case they have changed.
  HandleGetTtsExtensions(base::Value::ConstListView());
}

void TtsHandler::OnTtsEvent(content::TtsUtterance* utterance,
                            content::TtsEventType event_type,
                            int char_index,
                            int length,
                            const std::string& error_message) {
  if (event_type == content::TTS_EVENT_END ||
      event_type == content::TTS_EVENT_INTERRUPTED ||
      event_type == content::TTS_EVENT_ERROR) {
    base::Value result(false /* preview stopped */);
    FireWebUIListener("tts-preview-state-changed", result);
  }
}

void TtsHandler::HandlePreviewTtsVoice(base::Value::ConstListView args) {
  DCHECK_EQ(2U, args.size());
  const std::string& text = args[0].GetString();
  const std::string& voice_id = args[1].GetString();

  if (text.empty() || voice_id.empty())
    return;

  std::unique_ptr<base::DictionaryValue> json =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(voice_id));
  std::string name;
  std::string extension_id;
  if (const std::string* ptr = json->FindStringKey("name"))
    name = *ptr;
  if (const std::string* ptr = json->FindStringKey("extension"))
    extension_id = *ptr;

  std::unique_ptr<content::TtsUtterance> utterance =
      content::TtsUtterance::Create(web_ui()->GetWebContents());
  utterance->SetText(text);
  utterance->SetVoiceName(name);
  utterance->SetEngineId(extension_id);
  utterance->SetSrcUrl(
      GURL(chrome::GetOSSettingsUrl("manageAccessibility/tts")));
  utterance->SetEventDelegate(this);
  content::TtsController::GetInstance()->Stop();

  base::Value result(true /* preview started */);
  FireWebUIListener("tts-preview-state-changed", result);
  content::TtsController::GetInstance()->SpeakOrEnqueue(std::move(utterance));
}

void TtsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAllTtsVoiceData",
      base::BindRepeating(&TtsHandler::HandleGetAllTtsVoiceData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTtsExtensions",
      base::BindRepeating(&TtsHandler::HandleGetTtsExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "previewTtsVoice", base::BindRepeating(&TtsHandler::HandlePreviewTtsVoice,
                                             base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "wakeTtsEngine",
      base::BindRepeating(&TtsHandler::WakeTtsEngine, base::Unretained(this)));
}

void TtsHandler::OnJavascriptAllowed() {
  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);
}

void TtsHandler::OnJavascriptDisallowed() {
  RemoveTtsControllerDelegates();
}

int TtsHandler::GetVoiceLangMatchScore(const content::VoiceData* voice,
                                       const std::string& app_locale) {
  if (voice->lang.empty() || app_locale.empty())
    return 0;
  if (voice->lang == app_locale)
    return 2;
  return l10n_util::GetLanguage(voice->lang) ==
                 l10n_util::GetLanguage(app_locale)
             ? 1
             : 0;
}

void TtsHandler::WakeTtsEngine(base::Value::ConstListView args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  TtsExtensionEngine::GetInstance()->LoadBuiltInTtsEngine(profile);
  extensions::ProcessManager::Get(profile)->WakeEventPage(
      extension_misc::kGoogleSpeechSynthesisExtensionId,
      base::BindOnce(&TtsHandler::OnTtsEngineAwake,
                     weak_factory_.GetWeakPtr()));
}

void TtsHandler::OnTtsEngineAwake(bool success) {
  OnVoicesChanged();
}

void TtsHandler::RemoveTtsControllerDelegates() {
  content::TtsController::GetInstance()->RemoveVoicesChangedDelegate(this);
  content::TtsController::GetInstance()->RemoveUtteranceEventDelegate(this);
}

}  // namespace settings
