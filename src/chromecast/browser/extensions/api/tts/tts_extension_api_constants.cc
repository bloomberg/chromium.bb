// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PLEASE NOTE: this is a copy with modifications from
// /chrome/browser/speech/extension_api
// It is temporary until a refactoring to move the chrome TTS implementation up
// into components and extensions/components can be completed.

#include "chromecast/browser/extensions/api/tts/tts_extension_api_constants.h"

namespace tts_extension_api_constants {

const char kCharIndexKey[] = "charIndex";
const char kDesiredEventTypesKey[] = "desiredEventTypes";
const char kEnqueueKey[] = "enqueue";
const char kErrorMessageKey[] = "errorMessage";
const char kEventTypeKey[] = "type";
const char kEventTypesKey[] = "eventTypes";
const char kExtensionIdKey[] = "extensionId";
const char kGenderKey[] = "gender";
const char kIsFinalEventKey[] = "isFinalEvent";
const char kLangKey[] = "lang";
const char kOnEventKey[] = "onEvent";
const char kPitchKey[] = "pitch";
const char kRateKey[] = "rate";
const char kRemoteKey[] = "remote";
const char kRequiredEventTypesKey[] = "requiredEventTypes";
const char kSrcIdKey[] = "srcId";
const char kVoiceNameKey[] = "voiceName";
const char kVolumeKey[] = "volume";

const char kGenderFemale[] = "female";
const char kGenderMale[] = "male";

const char kEventTypeCancelled[] = "cancelled";
const char kEventTypeEnd[] = "end";
const char kEventTypeError[] = "error";
const char kEventTypeInterrupted[] = "interrupted";
const char kEventTypeMarker[] = "marker";
const char kEventTypePause[] = "pause";
const char kEventTypeResume[] = "resume";
const char kEventTypeSentence[] = "sentence";
const char kEventTypeStart[] = "start";
const char kEventTypeWord[] = "word";

const char kErrorExtensionIdMismatch[] = "Extension id mismatch.";
const char kErrorInvalidGender[] = "Invalid gender.";
const char kErrorInvalidLang[] = "Invalid lang.";
const char kErrorInvalidPitch[] = "Invalid pitch.";
const char kErrorInvalidRate[] = "Invalid rate.";
const char kErrorInvalidVolume[] = "Invalid volume.";
const char kErrorMissingPauseOrResume[] =
    "A TTS engine extension should either listen for both onPause and onResume "
    "events, or neither.";
const char kErrorUndeclaredEventType[] =
    "Cannot send an event type that is not declared in the extension manifest.";
const char kErrorUtteranceTooLong[] = "Utterance length is too long.";

}  // namespace tts_extension_api_constants.
