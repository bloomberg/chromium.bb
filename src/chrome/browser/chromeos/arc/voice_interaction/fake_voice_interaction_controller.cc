// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/fake_voice_interaction_controller.h"

namespace arc {

FakeVoiceInteractionController::FakeVoiceInteractionController()
    : binding_(this) {}

FakeVoiceInteractionController::~FakeVoiceInteractionController() = default;

ash::mojom::VoiceInteractionControllerPtr
FakeVoiceInteractionController::CreateInterfacePtrAndBind() {
  ash::mojom::VoiceInteractionControllerPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void FakeVoiceInteractionController::NotifyStatusChanged(
    ash::mojom::VoiceInteractionState state) {
  voice_interaction_state_ = state;
}

void FakeVoiceInteractionController::NotifySettingsEnabled(bool enabled) {
  voice_interaction_settings_enabled_ = enabled;
}

void FakeVoiceInteractionController::NotifyContextEnabled(bool enabled) {
  voice_interaction_context_enabled_ = enabled;
}

void FakeVoiceInteractionController::NotifyHotwordAlwaysOn(bool enabled) {
  voice_interaction_hotword_always_on_ = enabled;
}

void FakeVoiceInteractionController::NotifyHotwordEnabled(bool enabled) {
  voice_interaction_hotword_enabled_ = enabled;
}

void FakeVoiceInteractionController::NotifyConsentStatus(
    ash::mojom::ConsentStatus consent_status) {
  consent_status_ = consent_status;
}

void FakeVoiceInteractionController::NotifyFeatureAllowed(
    ash::mojom::AssistantAllowedState state) {
  assistant_allowed_state_ = state;
}

void FakeVoiceInteractionController::NotifyNotificationEnabled(bool enabled) {
  voice_interaction_notification_enabled_ = enabled;
}

void FakeVoiceInteractionController::NotifyLocaleChanged(
    const std::string& locale) {
  locale_ = locale;
}

void FakeVoiceInteractionController::NotifyLaunchWithMicOpen(
    bool launch_with_mic_open) {
  launch_with_mic_open_ = launch_with_mic_open;
}

}  // namespace arc
