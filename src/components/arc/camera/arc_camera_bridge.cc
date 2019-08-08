// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/camera/arc_camera_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/dbus/arc_camera_client.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "crypto/random.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for ArcCameraBridge.
class ArcCameraBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcCameraBridge,
          ArcCameraBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcCameraBridgeFactory";

  static ArcCameraBridgeFactory* GetInstance() {
    return base::Singleton<ArcCameraBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcCameraBridgeFactory>;
  ArcCameraBridgeFactory() = default;
  ~ArcCameraBridgeFactory() override = default;
};

}  // namespace

// static
ArcCameraBridge* ArcCameraBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcCameraBridgeFactory::GetForBrowserContext(context);
}

ArcCameraBridge::ArcCameraBridge(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->camera()->SetHost(this);
}

ArcCameraBridge::~ArcCameraBridge() {
  arc_bridge_service_->camera()->SetHost(nullptr);
}

void ArcCameraBridge::StartCameraService(StartCameraServiceCallback callback) {
  char random_bytes[16];
  crypto::RandBytes(random_bytes, 16);
  std::string token = base::HexEncode(random_bytes, 16);

  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe =
      invitation.AttachMessagePipe(token);
  // Run the callback now.
  // When an error occurs, it'll be communicated via the pipe.
  mojom::CameraServicePtr service;
  service.Bind(mojom::CameraServicePtrInfo(std::move(server_pipe), 0u));
  std::move(callback).Run(std::move(service));

  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());
  base::ScopedFD fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();

  chromeos::ArcCameraClient::Get()->StartService(
      fd.get(), token, base::BindOnce([](bool success) {}));
}

}  // namespace arc
