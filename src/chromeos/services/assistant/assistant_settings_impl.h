// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_IMPL_H_

#include <memory>
#include <string>

#include "chromeos/services/assistant/public/cpp/assistant_settings.h"
#include "chromeos/services/libassistant/public/mojom/settings_controller.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
class AssistantController;
class AssistantStateBase;
}  // namespace ash

namespace chromeos {
namespace assistant {

class ServiceContext;

class AssistantSettingsImpl : public AssistantSettings {
 public:
  explicit AssistantSettingsImpl(ServiceContext* context);

  AssistantSettingsImpl(const AssistantSettingsImpl&) = delete;
  AssistantSettingsImpl& operator=(const AssistantSettingsImpl&) = delete;

  ~AssistantSettingsImpl() override;

  void Initialize(
      mojo::PendingRemote<
          chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
          enrollment_controller_remote,
      chromeos::libassistant::mojom::SettingsController* settings_controller);

  // AssistantSettings overrides:
  void GetSettings(const std::string& selector,
                   GetSettingsCallback callback) override;
  void GetSettingsWithHeader(const std::string& selector,
                             GetSettingsCallback callback) override;
  void UpdateSettings(const std::string& update,
                      UpdateSettingsCallback callback) override;
  void StartSpeakerIdEnrollment(
      bool skip_cloud_enrollment,
      base::WeakPtr<SpeakerIdEnrollmentClient> client) override;
  void StopSpeakerIdEnrollment() override;
  void SyncSpeakerIdEnrollmentStatus() override;

  void SyncDeviceAppsStatus(base::OnceCallback<void(bool)> callback);

 private:
  void HandleSpeakerIdEnrollmentStatusSync(
      chromeos::libassistant::mojom::SpeakerIdEnrollmentStatusPtr status);
  void HandleDeviceAppsStatusSync(base::OnceCallback<void(bool)> callback,
                                  const std::string& settings);

  ash::AssistantStateBase* assistant_state();
  ash::AssistantController* assistant_controller();
  chromeos::libassistant::mojom::SettingsController& settings_controller();

  ServiceContext* const context_;
  chromeos::libassistant::mojom::SettingsController* settings_controller_ =
      nullptr;

  mojo::Remote<chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
      speaker_id_enrollment_remote_;

  base::WeakPtrFactory<AssistantSettingsImpl> weak_factory_{this};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_SETTINGS_IMPL_H_
