// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/service_impl.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/video_capture/device_factory_provider_impl.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"
#include "services/video_capture/testing_controls_impl.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace video_capture {

namespace {

#if defined(OS_ANDROID)
// On Android, we do not use automatic service shutdown, because when shutting
// down the service, we lose caching of the supported formats, and re-querying
// these can take several seconds on certain Android devices.
constexpr base::Optional<base::TimeDelta> kDefaultIdleTimeout;
#else
constexpr base::Optional<base::TimeDelta> kDefaultIdleTimeout =
    base::TimeDelta::FromSeconds(5);
#endif

}  // namespace

ServiceImpl::ServiceImpl(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : ServiceImpl(std::move(request),
                  std::move(ui_task_runner),
                  kDefaultIdleTimeout) {}

ServiceImpl::ServiceImpl(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::Optional<base::TimeDelta> idle_timeout)
    : binding_(this, std::move(request)),
      keepalive_(&binding_, idle_timeout),
      ui_task_runner_(std::move(ui_task_runner)) {
  keepalive_.AddObserver(this);
}

ServiceImpl::~ServiceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  keepalive_.RemoveObserver(this);
}

void ServiceImpl::SetFactoryProviderClientConnectedObserver(
    base::RepeatingClosure observer_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  factory_provider_client_connected_cb_ = std::move(observer_cb);
}

void ServiceImpl::SetFactoryProviderClientDisconnectedObserver(
    base::RepeatingClosure observer_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  factory_provider_client_disconnected_cb_ = std::move(observer_cb);
}

void ServiceImpl::SetShutdownTimeoutCancelledObserver(
    base::RepeatingClosure observer_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  shutdown_timeout_cancelled_cb_ = std::move(observer_cb);
}

bool ServiceImpl::HasNoContextRefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return keepalive_.HasNoRefs();
}

void ServiceImpl::ShutdownServiceAsap() {
  binding_.RequestClose();
}

void ServiceImpl::OnStart() {
  DCHECK(thread_checker_.CalledOnValidThread());

  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::SERVICE_STARTED);

  registry_.AddInterface<mojom::DeviceFactoryProvider>(
      // Unretained |this| is safe because |registry_| is owned by |this|.
      base::BindRepeating(&ServiceImpl::OnDeviceFactoryProviderRequest,
                          base::Unretained(this)));
  registry_.AddInterface<mojom::TestingControls>(
      // Unretained |this| is safe because |registry_| is owned by |this|.
      base::BindRepeating(&ServiceImpl::OnTestingControlsRequest,
                          base::Unretained(this)));

#if defined(OS_CHROMEOS)
  registry_.AddInterface<cros::mojom::CrosImageCapture>(base::BindRepeating(
      &ServiceImpl::OnCrosImageCaptureRequest, base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)

  // Unretained |this| is safe because |factory_provider_bindings_| is owned by
  // |this|.
  factory_provider_bindings_.set_connection_error_handler(base::BindRepeating(
      &ServiceImpl::OnProviderClientDisconnected, base::Unretained(this)));
}

void ServiceImpl::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK(thread_checker_.CalledOnValidThread());
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

bool ServiceImpl::OnServiceManagerConnectionLost() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void ServiceImpl::OnIdleTimeout() {
  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::SERVICE_SHUTTING_DOWN_BECAUSE_NO_CLIENT);
}

void ServiceImpl::OnIdleTimeoutCancelled() {
  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::SERVICE_SHUTDOWN_TIMEOUT_CANCELED);
  if (shutdown_timeout_cancelled_cb_)
    shutdown_timeout_cancelled_cb_.Run();
}

void ServiceImpl::OnDeviceFactoryProviderRequest(
    mojom::DeviceFactoryProviderRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LazyInitializeDeviceFactoryProvider();
  if (factory_provider_bindings_.empty())
    device_factory_provider_->SetServiceRef(keepalive_.CreateRef());
  factory_provider_bindings_.AddBinding(device_factory_provider_.get(),
                                        std::move(request));

  if (!factory_provider_client_connected_cb_.is_null()) {
    factory_provider_client_connected_cb_.Run();
  }
}

void ServiceImpl::OnTestingControlsRequest(
    mojom::TestingControlsRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  mojo::MakeStrongBinding(
      std::make_unique<TestingControlsImpl>(keepalive_.CreateRef()),
      std::move(request));
}

#if defined(OS_CHROMEOS)
void ServiceImpl::OnCrosImageCaptureRequest(
    cros::mojom::CrosImageCaptureRequest request) {
  LazyInitializeDeviceFactoryProvider();
  device_factory_provider_->BindCrosImageCaptureRequest(std::move(request));
}
#endif  // defined(OS_CHROMEOS)

void ServiceImpl::LazyInitializeDeviceFactoryProvider() {
  if (device_factory_provider_)
    return;

  // Use of base::Unretained() is safe because |this| owns, and therefore
  // outlives |device_factory_provider_|
  device_factory_provider_ = std::make_unique<DeviceFactoryProviderImpl>(
      ui_task_runner_, base::BindOnce(&ServiceImpl::ShutdownServiceAsap,
                                      base::Unretained(this)));
}

void ServiceImpl::OnProviderClientDisconnected() {
  // If last client has disconnected, release service ref so that service
  // shutdown timeout starts if no other references are still alive.
  // We keep the |device_factory_provider_| instance alive in order to avoid
  // losing state that would be expensive to reinitialize, e.g. having
  // already enumerated the available devices.
  if (factory_provider_bindings_.empty())
    device_factory_provider_->SetServiceRef(nullptr);

  if (!factory_provider_client_disconnected_cb_.is_null()) {
    factory_provider_client_disconnected_cb_.Run();
  }
}

}  // namespace video_capture
