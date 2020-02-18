// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/libvda_service_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/video/gpu_arc_video_service_host.h"
#include "chrome/browser/profiles/profile.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

LibvdaServiceProvider::LibvdaServiceProvider() {}

LibvdaServiceProvider::~LibvdaServiceProvider() = default;

void LibvdaServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      libvda::kLibvdaServiceInterface, libvda::kProvideMojoConnectionMethod,
      base::BindRepeating(&LibvdaServiceProvider::ProvideMojoConnection,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&LibvdaServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
}

void LibvdaServiceProvider::OnExported(const std::string& interface_name,
                                       const std::string& method_name,
                                       bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

void LibvdaServiceProvider::ProvideMojoConnection(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (!arc_session_manager) {
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_FAILED,
            "Could not find ARC session manager"));
    return;
  }
  // LibvdaService will return the GpuArcVideoServiceHost instance that
  // corresponds to the same profile used by ARC++. This might have to be
  // changed if callers other than ARCVM use this service.
  arc::GpuArcVideoServiceHost* gpu_arc_video_service_host =
      arc::GpuArcVideoServiceHost::GetForBrowserContext(
          arc_session_manager->profile());
  gpu_arc_video_service_host->OnBootstrapVideoAcceleratorFactory(base::BindOnce(
      &LibvdaServiceProvider::OnBootstrapVideoAcceleratorFactoryCallback,
      weak_ptr_factory_.GetWeakPtr(), method_call, std::move(response_sender)));
}

void LibvdaServiceProvider::OnBootstrapVideoAcceleratorFactoryCallback(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    mojo::ScopedHandle handle,
    const std::string& pipe_name) {
  base::ScopedFD fd = mojo::UnwrapPlatformHandle(std::move(handle)).TakeFD();

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendFileDescriptor(fd.get());
  writer.AppendString(pipe_name);
  std::move(response_sender).Run(std::move(response));
}

}  // namespace chromeos
