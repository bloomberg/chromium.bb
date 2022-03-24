// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_DEVICE_SETTINGS_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_DEVICE_SETTINGS_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/assistant/internal/action/assistant_action_observer.h"
#include "chromeos/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/services/libassistant/public/mojom/device_settings_delegate.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace assistant {
namespace api {
namespace client_op {
class ModifySettingArgs;
class GetDeviceSettingsArgs;
}  // namespace client_op
}  // namespace api
}  // namespace assistant

namespace chromeos {
namespace assistant {
struct DeviceSetting;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

class AssistantClient;
class Setting;

class DeviceSettingsController
    : public AssistantClientObserver,
      public chromeos::assistant::action::AssistantActionObserver {
 public:
  DeviceSettingsController();
  DeviceSettingsController(DeviceSettingsController&) = delete;
  DeviceSettingsController& operator=(DeviceSettingsController&) = delete;
  ~DeviceSettingsController() override;

  void Bind(mojo::PendingRemote<mojom::DeviceSettingsDelegate> delegate);

  // chromeos::assistant::action::AssistantActionObserver implementation:
  void OnModifyDeviceSetting(
      const ::assistant::api::client_op::ModifySettingArgs& setting) override;
  void OnGetDeviceSettings(
      int interaction_id,
      const ::assistant::api::client_op::GetDeviceSettingsArgs& setting)
      override;

  // AssistantClientObserver implementation:
  void OnAssistantClientCreated(AssistantClient* assistant_client) override;
  void OnDestroyingAssistantClient(AssistantClient* assistant_client) override;

  // Returns which of the given device settings are supported or not.
  std::vector<chromeos::assistant::DeviceSetting> GetSupportedDeviceSettings(
      const ::assistant::api::client_op::GetDeviceSettingsArgs& args) const;

 private:
  bool IsSettingSupported(const std::string& setting_id) const;

  void AddSetting(std::unique_ptr<Setting> setting);

  std::vector<std::unique_ptr<Setting>> settings_;
  AssistantClient* assistant_client_ = nullptr;
  mojo::Remote<mojom::DeviceSettingsDelegate> remote_;
  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  base::WeakPtrFactory<DeviceSettingsController> weak_factory_{this};
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_DEVICE_SETTINGS_CONTROLLER_H_
