// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_BROWSER_DELEGATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_BROWSER_DELEGATE_H_

#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "base/component_export.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/services/libassistant/public/mojom/service.mojom-forward.h"
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

namespace chromeos {
namespace assistant {

// Main interface implemented in browser to provide dependencies to
// |chromeos::assistant::Service|.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) AssistantBrowserDelegate {
 public:
  AssistantBrowserDelegate();
  AssistantBrowserDelegate(const AssistantBrowserDelegate&) = delete;
  AssistantBrowserDelegate& operator=(const AssistantBrowserDelegate&) = delete;
  virtual ~AssistantBrowserDelegate();

  static AssistantBrowserDelegate* Get();

  using RequestAssistantStructureCallback =
      base::OnceCallback<void(ax::mojom::AssistantExtraPtr,
                              std::unique_ptr<ui::AssistantTree>)>;
  // Requests Assistant structure for the active browser or ARC++ app window.
  virtual void RequestAssistantStructure(
      RequestAssistantStructureCallback callback) = 0;

  // Notifies assistant client that assistant running status has changed.
  virtual void OnAssistantStatusChanged(AssistantStatus new_status) = 0;

  // Requests Ash's AssistantVolumeControl interface from the browser.
  virtual void RequestAssistantVolumeControl(
      mojo::PendingReceiver<ash::mojom::AssistantVolumeControl> receiver) = 0;

  // Requests a BatteryMonitor from the browser.
  virtual void RequestBatteryMonitor(
      mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) = 0;

  // Requests a connection to the Device service's WakeLockProvider interface
  // from the browser.
  virtual void RequestWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) = 0;

  // Requests an Audio Service AudioStreamFactory from the browser.
  virtual void RequestAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) = 0;

  // Requests an audio decoder interface from the Assistant Audio Decoder
  // service, via the browser.
  virtual void RequestAudioDecoderFactory(
      mojo::PendingReceiver<mojom::AssistantAudioDecoderFactory> receiver) = 0;

  // Requests a connection to the Media Session service's AudioFocusManager from
  // the browser.
  virtual void RequestAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager>
          receiver) = 0;

  // Requests a connection to the Media Session service's MediaControllerManager
  // interface from the browser.
  virtual void RequestMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) = 0;

  // Requests a connection to the CrosNetworkConfig service interface via the
  // browser.
  virtual void RequestNetworkConfig(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver) = 0;

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  // Requests a connection to Libassistant service interface via the browser.
  virtual void RequestLibassistantService(
      mojo::PendingReceiver<chromeos::libassistant::mojom::LibassistantService>
          receiver) = 0;
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_BROWSER_DELEGATE_H_
