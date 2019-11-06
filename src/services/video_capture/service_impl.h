// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_SERVICE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/video_capture/device_factory_provider_impl.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/public/mojom/testing_controls.mojom.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace base {
class SingleThreadTaskRunner;
}

namespace video_capture {

class ServiceImpl : public service_manager::Service,
                    public service_manager::ServiceKeepalive::Observer {
 public:
  ServiceImpl(service_manager::mojom::ServiceRequest request,
              scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  // Constructs a service instance which overrides the default idle timeout
  // behavior.
  ServiceImpl(service_manager::mojom::ServiceRequest request,
              scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
              base::Optional<base::TimeDelta> idle_timeout);

  ~ServiceImpl() override;

  void SetFactoryProviderClientConnectedObserver(
      base::RepeatingClosure observer_cb);
  void SetFactoryProviderClientDisconnectedObserver(
      base::RepeatingClosure observer_cb);
  void SetShutdownTimeoutCancelledObserver(base::RepeatingClosure observer_cb);
  bool HasNoContextRefs();

  void ShutdownServiceAsap();

  // service_manager::Service implementation.
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  bool OnServiceManagerConnectionLost() override;

  // service_manager::ServiceKeepalive::Observer implementation.
  void OnIdleTimeout() override;
  void OnIdleTimeoutCancelled() override;

 private:
  void OnDeviceFactoryProviderRequest(
      mojom::DeviceFactoryProviderRequest request);
  void OnTestingControlsRequest(mojom::TestingControlsRequest request);
#if defined(OS_CHROMEOS)
  void OnCrosImageCaptureRequest(cros::mojom::CrosImageCaptureRequest request);
#endif  // defined(OS_CHROMEOS)
  void MaybeRequestQuitDelayed();
  void MaybeRequestQuit();
  void LazyInitializeDeviceFactoryProvider();
  void OnProviderClientDisconnected();

  service_manager::ServiceBinding binding_;
  service_manager::ServiceKeepalive keepalive_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

#if defined(OS_WIN)
  // COM must be initialized in order to access the video capture devices.
  base::win::ScopedCOMInitializer com_initializer_;
#endif
  service_manager::BinderRegistry registry_;
  mojo::BindingSet<mojom::DeviceFactoryProvider> factory_provider_bindings_;
  std::unique_ptr<DeviceFactoryProviderImpl> device_factory_provider_;

  // Callbacks that can optionally be set by clients.
  base::RepeatingClosure factory_provider_client_connected_cb_;
  base::RepeatingClosure factory_provider_client_disconnected_cb_;
  base::RepeatingClosure shutdown_timeout_cancelled_cb_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_SERVICE_IMPL_H_
