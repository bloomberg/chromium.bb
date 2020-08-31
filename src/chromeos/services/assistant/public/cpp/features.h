// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace chromeos {
namespace assistant {
namespace features {

// Enable Assistant Feedback UI.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantAudioEraser;

// Enable Assistant Feedback UI.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantFeedbackUi;

// Enables Assistant warmer welcome.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantWarmerWelcomeFeature;

// Enables Assistant app support.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantAppSupport;

// Enables Assistant launcher chip integration.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantLauncherChipIntegration;

// Enables Assistant proactive suggestions.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantProactiveSuggestions;

// A comma-delimited list of experiment IDs to trigger on the proactive
// suggestions server.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::FeatureParam<std::string>
    kAssistantProactiveSuggestionsServerExperimentIds;

// Enables suppression of Assistant proactive suggestions that have already been
// shown to the user.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::FeatureParam<bool>
    kAssistantProactiveSuggestionsSuppressDuplicates;

// The timeout threshold (in milliseconds) for the proactive suggestions chip.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::FeatureParam<int>
    kAssistantProactiveSuggestionsTimeoutThresholdMillis;

// When enabled, Assistant will use response processing V2. This is a set of
// synced client and server changes which will turn on default parallel client
// op processing and eager (streaming) UI element rendering.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantResponseProcessingV2;

// Enables Assistant routines.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantRoutines;

// When enabled, we support the second version of timers UX which includes new
// UI treatments for timers in Assistant and System UI.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantTimersV2;

// Enables server-driven wait scheduling. This allows the server to inject
// pauses into the interaction response to give the user time to digest one leg
// of a routine before proceeding to the next, for example, or to provide
// comedic timing for jokes.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kAssistantWaitScheduling;

// Enables clear cut logging.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableClearCutLog;

// Enables DSP for hotword detection.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableDspHotword;

// Enables MediaSession Integration.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableMediaSessionIntegration;

// Enables stereo audio input.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableStereoAudioInput;

// Enables power management features i.e. Wake locks and wake up alarms.
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnablePowerManager;

// Enables on-device-assistant to handle the most common queries on device.
// See go/marble
COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
extern const base::Feature kEnableOnDeviceAssistant;

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
int GetProactiveSuggestionsMaxWidth();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
int GetProactiveSuggestionsRichEntryPointBackgroundBlurRadius();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
int GetProactiveSuggestionsRichEntryPointCornerRadius();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
std::string GetProactiveSuggestionsServerExperimentIds();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
base::TimeDelta GetProactiveSuggestionsTimeoutThreshold();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsAppSupportEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsAudioEraserEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsClearCutLogEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsConversationStartersV2Enabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsDspHotwordEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsFeedbackUiEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsLauncherChipIntegrationEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsMediaSessionIntegrationEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsPowerManagerEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsOnDeviceAssistantEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsProactiveSuggestionsEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsProactiveSuggestionsShowOnScrollEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsProactiveSuggestionsShowRichEntryPointEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsProactiveSuggestionsSuppressDuplicatesEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC)
bool IsResponseProcessingV2Enabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsRoutinesEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsStereoAudioInputEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsTimersV2Enabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsVoiceMatchDisabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsWaitSchedulingEnabled();

COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) bool IsWarmerWelcomeEnabled();

}  // namespace features
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_FEATURES_H_
