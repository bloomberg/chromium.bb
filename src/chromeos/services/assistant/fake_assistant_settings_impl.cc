// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/fake_assistant_settings_impl.h"

#include <utility>

#include "base/callback.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"

namespace chromeos {
namespace assistant {

FakeAssistantSettingsImpl::FakeAssistantSettingsImpl() = default;

FakeAssistantSettingsImpl::~FakeAssistantSettingsImpl() = default;

void FakeAssistantSettingsImpl::GetSettings(const std::string& selector,
                                            GetSettingsCallback callback) {
  // Create a fake response
  assistant::SettingsUi settings_ui;
  settings_ui.mutable_consent_flow_ui()->set_consent_status(
      ConsentFlowUi_ConsentStatus_ALREADY_CONSENTED);
  std::move(callback).Run(settings_ui.SerializeAsString());
}

void FakeAssistantSettingsImpl::UpdateSettings(
    const std::string& update,
    UpdateSettingsCallback callback) {
  std::move(callback).Run(std::string());
}

void FakeAssistantSettingsImpl::StartSpeakerIdEnrollment(
    bool skip_cloud_enrollment,
    base::WeakPtr<SpeakerIdEnrollmentClient> client) {
  client->OnSpeakerIdEnrollmentDone();
}

void FakeAssistantSettingsImpl::StopSpeakerIdEnrollment() {}

}  // namespace assistant
}  // namespace chromeos
