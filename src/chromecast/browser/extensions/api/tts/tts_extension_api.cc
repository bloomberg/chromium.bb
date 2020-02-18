// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE NOTE: this is a copy with modifications from
// /chrome/browser/speech/extension_api
// It is temporary until a refactoring to move the chrome TTS implementation up
// into components and extensions/components can be completed.

#include "chromecast/browser/extensions/api/tts/tts_extension_api.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/extensions/api/tts/tts_extension_api_constants.h"
#include "chromecast/browser/tts/tts_controller.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "ui/base/l10n/l10n_util.h"

namespace constants = tts_extension_api_constants;

namespace events {
const char kOnEvent[] = "tts.onEvent";
}  // namespace events

const char* TtsEventTypeToString(TtsEventType event_type) {
  switch (event_type) {
    case TTS_EVENT_START:
      return constants::kEventTypeStart;
    case TTS_EVENT_END:
      return constants::kEventTypeEnd;
    case TTS_EVENT_WORD:
      return constants::kEventTypeWord;
    case TTS_EVENT_SENTENCE:
      return constants::kEventTypeSentence;
    case TTS_EVENT_MARKER:
      return constants::kEventTypeMarker;
    case TTS_EVENT_INTERRUPTED:
      return constants::kEventTypeInterrupted;
    case TTS_EVENT_CANCELLED:
      return constants::kEventTypeCancelled;
    case TTS_EVENT_ERROR:
      return constants::kEventTypeError;
    case TTS_EVENT_PAUSE:
      return constants::kEventTypePause;
    case TTS_EVENT_RESUME:
      return constants::kEventTypeResume;
    default:
      NOTREACHED();
      return constants::kEventTypeError;
  }
}

TtsEventType TtsEventTypeFromString(const std::string& str) {
  if (str == constants::kEventTypeStart)
    return TTS_EVENT_START;
  if (str == constants::kEventTypeEnd)
    return TTS_EVENT_END;
  if (str == constants::kEventTypeWord)
    return TTS_EVENT_WORD;
  if (str == constants::kEventTypeSentence)
    return TTS_EVENT_SENTENCE;
  if (str == constants::kEventTypeMarker)
    return TTS_EVENT_MARKER;
  if (str == constants::kEventTypeInterrupted)
    return TTS_EVENT_INTERRUPTED;
  if (str == constants::kEventTypeCancelled)
    return TTS_EVENT_CANCELLED;
  if (str == constants::kEventTypeError)
    return TTS_EVENT_ERROR;
  if (str == constants::kEventTypePause)
    return TTS_EVENT_PAUSE;
  if (str == constants::kEventTypeResume)
    return TTS_EVENT_RESUME;

  NOTREACHED();
  return TTS_EVENT_ERROR;
}

namespace {
TtsController* GetTtsController() {
  return chromecast::shell::CastBrowserProcess::GetInstance()->tts_controller();
}
}  // namespace

namespace extensions {

// One of these is constructed for each utterance, and deleted
// when the utterance gets any final event.
class TtsExtensionEventHandler : public UtteranceEventDelegate {
 public:
  explicit TtsExtensionEventHandler(const std::string& src_extension_id);

  void OnTtsEvent(Utterance* utterance,
                  TtsEventType event_type,
                  int char_index,
                  const std::string& error_message) override;

 private:
  // The extension ID of the extension that called speak() and should
  // receive events.
  std::string src_extension_id_;
};

TtsExtensionEventHandler::TtsExtensionEventHandler(
    const std::string& src_extension_id)
    : src_extension_id_(src_extension_id) {}

void TtsExtensionEventHandler::OnTtsEvent(Utterance* utterance,
                                          TtsEventType event_type,
                                          int char_index,
                                          const std::string& error_message) {
  if (utterance->src_id() < 0) {
    if (utterance->finished())
      delete this;
    return;
  }

  const std::set<TtsEventType>& desired_event_types =
      utterance->desired_event_types();
  if (desired_event_types.size() > 0 &&
      desired_event_types.find(event_type) == desired_event_types.end()) {
    if (utterance->finished())
      delete this;
    return;
  }

  const char* event_type_string = TtsEventTypeToString(event_type);
  std::unique_ptr<base::DictionaryValue> details(new base::DictionaryValue());
  if (char_index >= 0)
    details->SetInteger(constants::kCharIndexKey, char_index);
  details->SetString(constants::kEventTypeKey, event_type_string);
  if (event_type == TTS_EVENT_ERROR) {
    details->SetString(constants::kErrorMessageKey, error_message);
  }
  details->SetInteger(constants::kSrcIdKey, utterance->src_id());
  details->SetBoolean(constants::kIsFinalEventKey, utterance->finished());

  std::unique_ptr<base::ListValue> arguments(new base::ListValue());
  arguments->Append(std::move(details));

  auto event = std::make_unique<extensions::Event>(
      ::extensions::events::TTS_ON_EVENT, ::events::kOnEvent,
      std::move(arguments), utterance->browser_context());
  event->event_url = utterance->src_url();
  extensions::EventRouter::Get(utterance->browser_context())
      ->DispatchEventToExtension(src_extension_id_, std::move(event));

  if (utterance->finished())
    delete this;
}

ExtensionFunction::ResponseAction TtsSpeakFunction::Run() {
  std::string text;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &text));
  if (text.size() > 32768) {
    return RespondNow(Error(constants::kErrorUtteranceTooLong));
  }

  std::unique_ptr<base::DictionaryValue> options(new base::DictionaryValue());
  if (args_->GetSize() >= 2) {
    base::DictionaryValue* temp_options = NULL;
    if (args_->GetDictionary(1, &temp_options))
      options.reset(temp_options->DeepCopy());
  }

  std::string voice_name;
  if (options->HasKey(constants::kVoiceNameKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetString(constants::kVoiceNameKey, &voice_name));
  }

  std::string lang;
  if (options->HasKey(constants::kLangKey))
    EXTENSION_FUNCTION_VALIDATE(options->GetString(constants::kLangKey, &lang));
  if (!lang.empty() && !l10n_util::IsValidLocaleSyntax(lang)) {
    return RespondNow(Error(constants::kErrorInvalidLang));
  }

  std::string gender_str;
  TtsGenderType gender;
  if (options->HasKey(constants::kGenderKey))
    EXTENSION_FUNCTION_VALIDATE(
        options->GetString(constants::kGenderKey, &gender_str));
  if (gender_str == constants::kGenderMale) {
    gender = TTS_GENDER_MALE;
  } else if (gender_str == constants::kGenderFemale) {
    gender = TTS_GENDER_FEMALE;
  } else if (gender_str.empty()) {
    gender = TTS_GENDER_NONE;
  } else {
    return RespondNow(Error(constants::kErrorInvalidGender));
  }

  double rate = 1.0;
  if (options->HasKey(constants::kRateKey)) {
    EXTENSION_FUNCTION_VALIDATE(options->GetDouble(constants::kRateKey, &rate));
    if (rate < 0.1 || rate > 10.0) {
      return RespondNow(Error(constants::kErrorInvalidRate));
    }
  }

  double pitch = 1.0;
  if (options->HasKey(constants::kPitchKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetDouble(constants::kPitchKey, &pitch));
    if (pitch < 0.0 || pitch > 2.0) {
      return RespondNow(Error(constants::kErrorInvalidPitch));
    }
  }

  double volume = 1.0;
  if (options->HasKey(constants::kVolumeKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetDouble(constants::kVolumeKey, &volume));
    if (volume < 0.0 || volume > 1.0) {
      return RespondNow(Error(constants::kErrorInvalidVolume));
    }
  }

  bool can_enqueue = false;
  if (options->HasKey(constants::kEnqueueKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetBoolean(constants::kEnqueueKey, &can_enqueue));
  }

  std::set<TtsEventType> required_event_types;
  if (options->HasKey(constants::kRequiredEventTypesKey)) {
    base::ListValue* list;
    EXTENSION_FUNCTION_VALIDATE(
        options->GetList(constants::kRequiredEventTypesKey, &list));
    for (size_t i = 0; i < list->GetSize(); ++i) {
      std::string event_type;
      if (list->GetString(i, &event_type))
        required_event_types.insert(TtsEventTypeFromString(event_type.c_str()));
    }
  }

  std::set<TtsEventType> desired_event_types;
  if (options->HasKey(constants::kDesiredEventTypesKey)) {
    base::ListValue* list;
    EXTENSION_FUNCTION_VALIDATE(
        options->GetList(constants::kDesiredEventTypesKey, &list));
    for (size_t i = 0; i < list->GetSize(); ++i) {
      std::string event_type;
      if (list->GetString(i, &event_type))
        desired_event_types.insert(TtsEventTypeFromString(event_type.c_str()));
    }
  }

  std::string voice_extension_id;
  if (options->HasKey(constants::kExtensionIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetString(constants::kExtensionIdKey, &voice_extension_id));
  }

  int src_id = -1;
  if (options->HasKey(constants::kSrcIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        options->GetInteger(constants::kSrcIdKey, &src_id));
  }

  // If we got this far, the arguments were all in the valid format, so
  // send the success response to the callback now - this ensures that
  // the callback response always arrives before events, which makes
  // the behavior more predictable and easier to write unit tests for too.

  Respond(OneArgument(std::make_unique<base::Value>(true)));

  Utterance* utterance = new Utterance(browser_context());
  utterance->set_text(text);
  utterance->set_voice_name(voice_name);
  utterance->set_src_id(src_id);
  utterance->set_src_url(source_url());
  utterance->set_lang(lang);
  utterance->set_gender(gender);
  utterance->set_continuous_parameters(rate, pitch, volume);
  utterance->set_can_enqueue(can_enqueue);
  utterance->set_required_event_types(required_event_types);
  utterance->set_desired_event_types(desired_event_types);
  utterance->set_extension_id(voice_extension_id);
  utterance->set_options(options.get());
  utterance->set_event_delegate(new TtsExtensionEventHandler(extension_id()));

  GetTtsController()->SpeakOrEnqueue(utterance);
  return did_respond() ? AlreadyResponded() : RespondLater();
}

ExtensionFunction::ResponseAction TtsStopSpeakingFunction::Run() {
  GetTtsController()->Stop();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TtsPauseFunction::Run() {
  GetTtsController()->Pause();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TtsResumeFunction::Run() {
  GetTtsController()->Resume();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TtsIsSpeakingFunction::Run() {
  return RespondNow(OneArgument(
      std::make_unique<base::Value>(GetTtsController()->IsSpeaking())));
}

ExtensionFunction::ResponseAction TtsGetVoicesFunction::Run() {
  std::vector<VoiceData> voices;
  GetTtsController()->GetVoices(browser_context(), &voices);

  auto result_voices = std::make_unique<base::ListValue>();
  for (size_t i = 0; i < voices.size(); ++i) {
    const VoiceData& voice = voices[i];
    std::unique_ptr<base::DictionaryValue> result_voice(
        new base::DictionaryValue());
    result_voice->SetString(constants::kVoiceNameKey, voice.name);
    result_voice->SetBoolean(constants::kRemoteKey, voice.remote);
    if (!voice.lang.empty())
      result_voice->SetString(constants::kLangKey, voice.lang);
    if (voice.gender == TTS_GENDER_MALE)
      result_voice->SetString(constants::kGenderKey, constants::kGenderMale);
    else if (voice.gender == TTS_GENDER_FEMALE)
      result_voice->SetString(constants::kGenderKey, constants::kGenderFemale);
    if (!voice.extension_id.empty())
      result_voice->SetString(constants::kExtensionIdKey, voice.extension_id);

    auto event_types = std::make_unique<base::ListValue>();
    for (std::set<TtsEventType>::iterator iter = voice.events.begin();
         iter != voice.events.end(); ++iter) {
      const char* event_name_constant = TtsEventTypeToString(*iter);
      event_types->AppendString(event_name_constant);
    }
    result_voice->Set(constants::kEventTypesKey, std::move(event_types));

    result_voices->Append(std::move(result_voice));
  }

  return RespondNow(OneArgument(std::move(result_voices)));
}

TtsAPI::TtsAPI(content::BrowserContext* context) {
  ExtensionFunctionRegistry& registry =
      ExtensionFunctionRegistry::GetInstance();
  registry.RegisterFunction<TtsGetVoicesFunction>();
  registry.RegisterFunction<TtsIsSpeakingFunction>();
  registry.RegisterFunction<TtsSpeakFunction>();
  registry.RegisterFunction<TtsStopSpeakingFunction>();
  registry.RegisterFunction<TtsPauseFunction>();
  registry.RegisterFunction<TtsResumeFunction>();
}

TtsAPI::~TtsAPI() {}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<TtsAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<TtsAPI>* TtsAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
