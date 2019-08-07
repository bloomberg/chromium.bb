// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mojo_interface_factory.h"

#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_focus_ring_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_alarm_timer_controller.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_notification_controller.h"
#include "ash/assistant/assistant_screen_context_controller.h"
#include "ash/assistant/assistant_setup_controller.h"
#include "ash/autotest/shelf_integration_test_api.h"
#include "ash/display/cros_display_config.h"
#include "ash/events/event_rewriter_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/keyboard/ui/keyboard_controller.h"
#include "ash/kiosk_next/kiosk_next_shell_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/media/media_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shutdown_controller.h"
#include "ash/system/locale/locale_update_controller.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/network/vpn_list.h"
#include "ash/system/night_light/night_light_controller.h"
#include "ash/tray_action/tray_action.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/constants/chromeos_switches.h"

namespace ash {
namespace mojo_interface_factory {
namespace {

base::LazyInstance<RegisterInterfacesCallback>::Leaky
    g_register_interfaces_callback = LAZY_INSTANCE_INITIALIZER;

void BindAccessibilityControllerRequestOnMainThread(
    mojom::AccessibilityControllerRequest request) {
  Shell::Get()->accessibility_controller()->BindRequest(std::move(request));
}

void BindAccessibilityFocusRingControllerRequestOnMainThread(
    mojom::AccessibilityFocusRingControllerRequest request) {
  Shell::Get()->accessibility_focus_ring_controller()->BindRequest(
      std::move(request));
}

void BindAssistantAlarmTimerControllerRequestOnMainThread(
    mojom::AssistantAlarmTimerControllerRequest request) {
  Shell::Get()->assistant_controller()->alarm_timer_controller()->BindRequest(
      std::move(request));
}

void BindAssistantControllerRequestOnMainThread(
    mojom::AssistantControllerRequest request) {
  Shell::Get()->assistant_controller()->BindRequest(std::move(request));
}

void BindAssistantNotificationControllerRequestOnMainThread(
    mojom::AssistantNotificationControllerRequest request) {
  Shell::Get()->assistant_controller()->notification_controller()->BindRequest(
      std::move(request));
}

void BindAssistantScreenContextControllerRequestOnMainThread(
    mojom::AssistantScreenContextControllerRequest request) {
  Shell::Get()
      ->assistant_controller()
      ->screen_context_controller()
      ->BindRequest(std::move(request));
}

void BindAssistantVolumeControlRequestOnMainThread(
    mojom::AssistantVolumeControlRequest request) {
  Shell::Get()->assistant_controller()->BindRequest(std::move(request));
}

void BindCrosDisplayConfigControllerRequestOnMainThread(
    mojom::CrosDisplayConfigControllerRequest request) {
  Shell::Get()->cros_display_config()->BindRequest(std::move(request));
}

void BindAshMessageCenterControllerRequestOnMainThread(
    mojom::AshMessageCenterControllerRequest request) {
  Shell::Get()->message_center_controller()->BindRequest(std::move(request));
}

void BindEventRewriterControllerRequestOnMainThread(
    mojom::EventRewriterControllerRequest request) {
  Shell::Get()->event_rewriter_controller()->BindRequest(std::move(request));
}

void BindImeControllerRequestOnMainThread(mojom::ImeControllerRequest request) {
  Shell::Get()->ime_controller()->BindRequest(std::move(request));
}

void BindKeyboardControllerRequestOnMainThread(
    mojom::KeyboardControllerRequest request) {
  Shell::Get()->ash_keyboard_controller()->BindRequest(std::move(request));
}

void BindKioskNextShellControllerRequestOnMainThread(
    mojom::KioskNextShellControllerRequest request) {
  Shell::Get()->kiosk_next_shell_controller()->BindRequest(std::move(request));
}

void BindLocaleUpdateControllerOnMainThread(
    mojom::LocaleUpdateControllerRequest request) {
  Shell::Get()->locale_update_controller()->BindRequest(std::move(request));
}

void BindMediaControllerRequestOnMainThread(
    mojom::MediaControllerRequest request) {
  Shell::Get()->media_controller()->BindRequest(std::move(request));
}

void BindNightLightControllerRequestOnMainThread(
    mojom::NightLightControllerRequest request) {
  Shell::Get()->night_light_controller()->BindRequest(std::move(request));
}

void BindShelfIntegrationTestApiRequestOnMainThread(
    mojom::ShelfIntegrationTestApiRequest request) {
  ShelfIntegrationTestApi::BindRequest(std::move(request));
}

void BindShutdownControllerRequestOnMainThread(
    mojom::ShutdownControllerRequest request) {
  Shell::Get()->shutdown_controller()->BindRequest(std::move(request));
}

void BindTrayActionRequestOnMainThread(mojom::TrayActionRequest request) {
  Shell::Get()->tray_action()->BindRequest(std::move(request));
}

void BindVoiceInteractionControllerRequestOnMainThread(
    mojom::VoiceInteractionControllerRequest request) {
  Shell::Get()->voice_interaction_controller()->BindRequest(std::move(request));
}

void BindVpnListRequestOnMainThread(mojom::VpnListRequest request) {
  Shell::Get()->vpn_list()->BindRequest(std::move(request));
}

}  // namespace

void RegisterInterfaces(
    service_manager::BinderRegistry* registry,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner) {
  registry->AddInterface(
      base::BindRepeating(&BindAccessibilityControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(
          &BindAccessibilityFocusRingControllerRequestOnMainThread),
      main_thread_task_runner);
  if (chromeos::switches::IsAssistantEnabled()) {
    registry->AddInterface(
        base::BindRepeating(
            &BindAssistantAlarmTimerControllerRequestOnMainThread),
        main_thread_task_runner);
    registry->AddInterface(
        base::BindRepeating(&BindAssistantControllerRequestOnMainThread),
        main_thread_task_runner);
    registry->AddInterface(
        base::BindRepeating(
            &BindAssistantNotificationControllerRequestOnMainThread),
        main_thread_task_runner);
    registry->AddInterface(
        base::BindRepeating(
            &BindAssistantScreenContextControllerRequestOnMainThread),
        main_thread_task_runner);
    registry->AddInterface(
        base::BindRepeating(&BindAssistantVolumeControlRequestOnMainThread),
        main_thread_task_runner);
  }
  registry->AddInterface(
      base::BindRepeating(&BindCrosDisplayConfigControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindAshMessageCenterControllerRequestOnMainThread),
      main_thread_task_runner);
  if (base::FeatureList::IsEnabled(features::kKioskNextShell)) {
    registry->AddInterface(
        base::BindRepeating(&BindKioskNextShellControllerRequestOnMainThread),
        main_thread_task_runner);
  }
  registry->AddInterface(
      base::BindRepeating(&BindEventRewriterControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindImeControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindKeyboardControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindLocaleUpdateControllerOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindMediaControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindNightLightControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindShutdownControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindTrayActionRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(
      base::BindRepeating(&BindVoiceInteractionControllerRequestOnMainThread),
      main_thread_task_runner);
  registry->AddInterface(base::BindRepeating(&BindVpnListRequestOnMainThread),
                         main_thread_task_runner);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTestInterfaces)) {
    registry->AddInterface(
        base::BindRepeating(&BindShelfIntegrationTestApiRequestOnMainThread),
        main_thread_task_runner);
  }

  // Inject additional optional interfaces.
  if (g_register_interfaces_callback.Get()) {
    std::move(g_register_interfaces_callback.Get())
        .Run(registry, main_thread_task_runner);
  }
}

void SetRegisterInterfacesCallback(RegisterInterfacesCallback callback) {
  g_register_interfaces_callback.Get() = std::move(callback);
}

}  // namespace mojo_interface_factory

}  // namespace ash
