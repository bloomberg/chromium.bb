// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_SERVICE_H_

#include <memory>
#include <string>

#include "ash/public/cpp/assistant/assistant_state_proxy.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

class GoogleServiceAuthError;

namespace base {
class OneShotTimer;
}  // namespace base

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace power_manager {
class PowerSupplyProperties;
}  // namespace power_manager

namespace chromeos {
namespace assistant {

class AssistantManagerService;

class COMPONENT_EXPORT(ASSISTANT_SERVICE) Service
    : public service_manager::Service,
      public chromeos::PowerManagerClient::Observer,
      public ash::mojom::SessionActivationObserver,
      public mojom::AssistantPlatform,
      public ash::DefaultVoiceInteractionObserver {
 public:
  Service(service_manager::mojom::ServiceRequest request,
          network::NetworkConnectionTracker* network_connection_tracker,
          std::unique_ptr<network::SharedURLLoaderFactoryInfo>
              url_loader_factory_info);
  ~Service() override;

  mojom::Client* client() { return client_.get(); }

  mojom::DeviceActions* device_actions() { return device_actions_.get(); }

  ash::mojom::AssistantController* assistant_controller() {
    return assistant_controller_.get();
  }

  ash::mojom::AssistantAlarmTimerController*
  assistant_alarm_timer_controller() {
    return assistant_alarm_timer_controller_.get();
  }

  ash::mojom::AssistantNotificationController*
  assistant_notification_controller() {
    return assistant_notification_controller_.get();
  }

  ash::mojom::AssistantScreenContextController*
  assistant_screen_context_controller() {
    return assistant_screen_context_controller_.get();
  }

  ash::AssistantStateBase* assistant_state() { return &assistant_state_; }

  scoped_refptr<base::SequencedTaskRunner> main_task_runner() {
    return main_task_runner_;
  }

  bool is_signed_out_mode() const { return is_signed_out_mode_; }

  void RequestAccessToken();

  void SetIdentityAccessorForTesting(
      identity::mojom::IdentityAccessorPtr identity_accessor);

  void SetAssistantManagerForTesting(
      std::unique_ptr<AssistantManagerService> assistant_manager_service);

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer);

 private:
  friend class ServiceTest;
  // service_manager::Service overrides
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  void BindAssistantConnection(mojom::AssistantRequest request);
  void BindAssistantPlatformConnection(mojom::AssistantPlatformRequest request);

  // chromeos::PowerManagerClient::Observer overrides:
  void PowerChanged(const power_manager::PowerSupplyProperties& prop) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // ash::mojom::SessionActivationObserver overrides:
  void OnSessionActivated(bool activated) override;
  void OnLockStateChanged(bool locked) override;

  // ash::mojom::VoiceInteractionObserver:
  void OnVoiceInteractionSettingsEnabled(bool enabled) override;
  void OnVoiceInteractionHotwordEnabled(bool enabled) override;
  void OnVoiceInteractionHotwordAlwaysOn(bool always_on) override;
  void OnLocaleChanged(const std::string& locale) override;

  void UpdateAssistantManagerState();
  void BindAssistantSettingsManager(
      mojom::AssistantSettingsManagerRequest request);

  // mojom::AssistantPlatform overrides:
  void Init(mojom::ClientPtr client,
            mojom::DeviceActionsPtr device_actions) override;

  identity::mojom::IdentityAccessor* GetIdentityAccessor();

  void GetPrimaryAccountInfoCallback(
      const base::Optional<AccountInfo>& account_info,
      const identity::AccountState& account_state);

  void GetAccessTokenCallback(const base::Optional<std::string>& token,
                              base::Time expiration_time,
                              const GoogleServiceAuthError& error);
  void RetryRefreshToken();

  void CreateAssistantManagerService();

  void FinalizeAssistantManagerService();

  void StopAssistantManagerService();

  void AddAshSessionObserver();

  void UpdateListeningState();

  bool ShouldEnableHotword();

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry registry_;

  mojo::BindingSet<mojom::Assistant> bindings_;
  mojo::Binding<mojom::AssistantPlatform> platform_binding_;
  mojo::Binding<ash::mojom::SessionActivationObserver>
      session_observer_binding_;
  mojom::ClientPtr client_;
  mojom::DeviceActionsPtr device_actions_;

  identity::mojom::IdentityAccessorPtr identity_accessor_;

  AccountId account_id_;
  std::unique_ptr<AssistantManagerService> assistant_manager_service_;
  std::unique_ptr<base::OneShotTimer> token_refresh_timer_;
  int token_refresh_error_backoff_factor = 1;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_observer_;

  // Whether the current user session is active.
  bool session_active_ = false;
  // Whether the lock screen is on.
  bool locked_ = false;
  // Whether the power source is connected.
  bool power_source_connected_ = false;
  // In the signed-out mode, we are going to run Assistant service without
  // using user's signed in account information.
  bool is_signed_out_mode_ = false;

  base::Optional<std::string> access_token_;

  ash::mojom::AssistantControllerPtr assistant_controller_;
  ash::mojom::AssistantAlarmTimerControllerPtr
      assistant_alarm_timer_controller_;
  ash::mojom::AssistantNotificationControllerPtr
      assistant_notification_controller_;
  ash::mojom::AssistantScreenContextControllerPtr
      assistant_screen_context_controller_;
  ash::AssistantStateProxy assistant_state_;

  network::NetworkConnectionTracker* network_connection_tracker_;
  // non-null until |assistant_manager_service_| is created.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory_info_;

  base::WeakPtrFactory<Service> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_SERVICE_H_
