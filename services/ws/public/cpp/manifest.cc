// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/discardable_memory/public/interfaces/discardable_shared_memory_manager.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/event_injector.mojom.h"
#include "services/ws/public/mojom/gpu.mojom.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "services/ws/public/mojom/input_devices/input_device_server.mojom.h"
#include "services/ws/public/mojom/remoting_event_injector.mojom.h"
#include "services/ws/public/mojom/user_activity_monitor.mojom.h"
#include "services/ws/public/mojom/window_server_test.mojom.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/base/mojo/clipboard.mojom.h"

#if defined(OS_CHROMEOS)
#include "services/ws/public/mojom/arc_gpu.mojom.h"
#include "services/ws/public/mojom/input_devices/input_device_controller.mojom.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/interfaces/device_cursor.mojom.h"
#include "ui/ozone/public/interfaces/drm_device.mojom.h"
#endif

namespace ws {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(mojom::kServiceName)
        .WithDisplayName("UI Service")
        .WithOptions(service_manager::ManifestOptionsBuilder()
                         .WithSandboxType("none")
                         .WithInstanceSharingPolicy(
                             service_manager::Manifest::InstanceSharingPolicy::
                                 kSingleton)
                         .Build())
        .ExposeCapability(
            "app",
            service_manager::Manifest::InterfaceList<
                discardable_memory::mojom::DiscardableSharedMemoryManager,
                ui::mojom::ClipboardHost, mojom::Gpu, mojom::IMEDriver,
                mojom::InputDeviceServer, mojom::WindowTreeFactory>())
        .ExposeCapability(
            "discardable_memory",
            service_manager::Manifest::InterfaceList<
                discardable_memory::mojom::DiscardableSharedMemoryManager>())
        .ExposeCapability(
            "gpu_client",
            service_manager::Manifest::InterfaceList<mojom::Gpu>())
#if defined(USE_OZONE)
        // TODO(crbug.com/912221): When viz is run in-process inside window
        // service, window service should provide ozone interfaces; otherwise,
        // viz service provides them itself. Remove this when the in-process viz
        // codepath is removed.
        .ExposeCapability(
            "ozone",
            service_manager::Manifest::InterfaceList<
                ui::ozone::mojom::DeviceCursor, ui::ozone::mojom::DrmDevice>())
#endif
        .ExposeCapability(
            "test",
            service_manager::Manifest::InterfaceList<mojom::EventInjector,
                                                     mojom::WindowServerTest>())
        .ExposeCapability(
            "ime_registrar",
            service_manager::Manifest::InterfaceList<mojom::IMERegistrar>())
        .ExposeCapability(
            "privileged",
            service_manager::Manifest::InterfaceList<
                mojom::EventInjector, mojom::RemotingEventInjector>())
        .ExposeCapability(
            "window_manager",
            service_manager::Manifest::InterfaceList<
#if defined(OS_CHROMEOS)
                mojom::InputDeviceController,
#endif
                discardable_memory::mojom::DiscardableSharedMemoryManager,
                mojom::EventInjector, mojom::Gpu, mojom::IMEDriver,
                mojom::InputDeviceServer, mojom::UserActivityMonitor>())
#if defined(OS_CHROMEOS)
        .ExposeCapability(
            "arc_manager",
            service_manager::Manifest::InterfaceList<mojom::ArcGpu>())
        .ExposeCapability("input_device_controller",
                          service_manager::Manifest::InterfaceList<
                              mojom::InputDeviceController>())
#endif
        .RequireCapability("*", "app")
        .RequireCapability("ui", "ozone")
        .RequireCapability("viz", "viz_host")

        .Build()
  };
  return *manifest;
}

}  // namespace ws
