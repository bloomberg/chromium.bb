// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_recognizer.h"

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/network_speech_recognizer.h"
#include "chrome/browser/speech/on_device_speech_recognizer.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"

namespace {
const char kSpeechRecognitionError[] = "A speech recognition error occurred";
const char kSpeechRecognitionStartError[] =
    "Speech recognition already started";
const char kSpeechRecognitionStopError[] = "Speech recognition already stopped";
}  // namespace

namespace extensions {

SpeechRecognitionPrivateRecognizer::SpeechRecognitionPrivateRecognizer(
    SpeechRecognitionPrivateDelegate* delegate,
    content::BrowserContext* context,
    const std::string& id)
    : delegate_(delegate), context_(context), id_(id) {
  DCHECK(delegate);
}

SpeechRecognitionPrivateRecognizer::~SpeechRecognitionPrivateRecognizer() {}

void SpeechRecognitionPrivateRecognizer::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const absl::optional<media::SpeechRecognitionResult>& full_result) {
  // TODO(crbug.com/1220107): NetworkSpeechRecognizer adds spaces between
  // results, but OnDeviceSpeechRecognizer doesn't. Add behavior in
  // OnDeviceSpeechRecognizer so it's consistent with NetworkSpeechRecognizer.
  if (!interim_results_ && !is_final) {
    // If |interim_results_| is false, then don't handle the result unless this
    // is a final result.
    return;
  }

  delegate_->HandleSpeechRecognitionResult(id_, text, is_final);
}

void SpeechRecognitionPrivateRecognizer::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  SpeechRecognizerStatus next_state = new_state;
  if (new_state == SPEECH_RECOGNIZER_READY) {
    if (current_state_ == SPEECH_RECOGNIZER_OFF && speech_recognizer_) {
      // The SpeechRecognizer is ready to start recognizing speech.
      speech_recognizer_->Start();
    } else {
      // Turn the recognizer off and use |delegate_| to notify listeners of the
      // API that speech recognition has stopped.
      next_state = SPEECH_RECOGNIZER_OFF;
      RecognizerOff();
      delegate_->HandleSpeechRecognitionStopped(id_);
    }
  } else if (new_state == SPEECH_RECOGNIZER_RECOGNIZING) {
    DCHECK(!on_start_callback_.is_null());
    std::move(on_start_callback_)
        .Run(/*type=*/type_, /*error=*/absl::optional<std::string>());
  } else if (new_state == SPEECH_RECOGNIZER_ERROR) {
    // When a speech recognition error occurs, ask the delegate to handle both
    // error and stop events.
    next_state = SPEECH_RECOGNIZER_OFF;
    RecognizerOff();
    delegate_->HandleSpeechRecognitionError(id_, kSpeechRecognitionError);
    delegate_->HandleSpeechRecognitionStopped(id_);
  }
  current_state_ = next_state;
}

void SpeechRecognitionPrivateRecognizer::HandleStart(
    absl::optional<std::string> locale,
    absl::optional<bool> interim_results,
    OnStartCallback callback) {
  if (speech_recognizer_) {
    std::move(callback).Run(
        /*type=*/type_,
        /*error=*/absl::optional<std::string>(kSpeechRecognitionStartError));
    RecognizerOff();
    return;
  }

  MaybeUpdateProperties(locale, interim_results, std::move(callback));

  // Choose which type of speech recognition, either on-device or network.
  Profile* profile = Profile::FromBrowserContext(context_);
  if (OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(locale_)) {
    type_ = speech::SpeechRecognitionType::kOnDevice;
    speech_recognizer_ = std::make_unique<OnDeviceSpeechRecognizer>(
        GetWeakPtr(), profile, locale_,
        /*recognition_mode_ime=*/true, /*enable_formatting=*/false);
  } else {
    type_ = speech::SpeechRecognitionType::kNetwork;
    speech_recognizer_ = std::make_unique<NetworkSpeechRecognizer>(
        GetWeakPtr(),
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcessIOThread(),
        profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages),
        locale_);
  }
}

void SpeechRecognitionPrivateRecognizer::HandleStop(OnStopCallback callback) {
  if (current_state_ == SPEECH_RECOGNIZER_OFF) {
    // If speech recognition is already off, trigger the callback with an error
    // message.
    std::move(callback).Run(
        /*error=*/absl::optional<std::string>(kSpeechRecognitionStopError));
    return;
  }

  RecognizerOff();

  delegate_->HandleSpeechRecognitionStopped(id_);

  DCHECK(!callback.is_null());
  std::move(callback).Run(/*error=*/absl::optional<std::string>());
}

void SpeechRecognitionPrivateRecognizer::RecognizerOff() {
  current_state_ = SPEECH_RECOGNIZER_OFF;
  if (speech_recognizer_)
    speech_recognizer_.reset();
}

void SpeechRecognitionPrivateRecognizer::MaybeUpdateProperties(
    absl::optional<std::string> locale,
    absl::optional<bool> interim_results,
    OnStartCallback callback) {
  if (locale.has_value())
    locale_ = locale.value();
  if (interim_results.has_value())
    interim_results_ = interim_results.value();
  on_start_callback_ = std::move(callback);
  DCHECK(!on_start_callback_.is_null());
}

}  // namespace extensions
