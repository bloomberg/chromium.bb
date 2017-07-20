// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/service_impl.h"

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/video_capture/device_factory_provider_impl.h"
#include "services/video_capture/public/interfaces/constants.mojom.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"
#include "services/video_capture/testing_controls_impl.h"

namespace video_capture {

ServiceImpl::ServiceImpl()
    : shutdown_delay_in_seconds_(mojom::kDefaultShutdownDelayInSeconds),
      weak_factory_(this) {}

ServiceImpl::~ServiceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ServiceImpl::OnStart() {
  DCHECK(thread_checker_.CalledOnValidThread());

  video_capture::uma::LogVideoCaptureServiceEvent(
      video_capture::uma::SERVICE_STARTED);

  ref_factory_ =
      base::MakeUnique<service_manager::ServiceContextRefFactory>(base::Bind(
          &ServiceImpl::MaybeRequestQuitDelayed, base::Unretained(this)));
  registry_.AddInterface<mojom::DeviceFactoryProvider>(
      // Unretained |this| is safe because |registry_| is owned by |this|.
      base::Bind(&ServiceImpl::OnDeviceFactoryProviderRequest,
                 base::Unretained(this)));
  registry_.AddInterface<mojom::TestingControls>(
      // Unretained |this| is safe because |registry_| is owned by |this|.
      base::Bind(&ServiceImpl::OnTestingControlsRequest,
                 base::Unretained(this)));
}

void ServiceImpl::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(thread_checker_.CalledOnValidThread());
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

bool ServiceImpl::OnServiceManagerConnectionLost() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return true;
}

void ServiceImpl::OnDeviceFactoryProviderRequest(
    mojom::DeviceFactoryProviderRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  mojo::MakeStrongBinding(
      base::MakeUnique<DeviceFactoryProviderImpl>(
          ref_factory_->CreateRef(),
          // Use of unretained |this| is safe, because the
          // VideoCaptureServiceImpl has shared ownership of |this| via the
          // reference created by ref_factory->CreateRef().
          base::Bind(&ServiceImpl::SetShutdownDelayInSeconds,
                     base::Unretained(this))),
      std::move(request));
}

void ServiceImpl::OnTestingControlsRequest(
    mojom::TestingControlsRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  mojo::MakeStrongBinding(
      base::MakeUnique<TestingControlsImpl>(ref_factory_->CreateRef()),
      std::move(request));
}

void ServiceImpl::SetShutdownDelayInSeconds(float seconds) {
  DCHECK(thread_checker_.CalledOnValidThread());
  shutdown_delay_in_seconds_ = seconds;
}

void ServiceImpl::MaybeRequestQuitDelayed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ServiceImpl::MaybeRequestQuit, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(shutdown_delay_in_seconds_));
}

void ServiceImpl::MaybeRequestQuit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ref_factory_);
  if (ref_factory_->HasNoRefs()) {
    video_capture::uma::LogVideoCaptureServiceEvent(
        video_capture::uma::SERVICE_SHUTTING_DOWN_BECAUSE_NO_CLIENT);
    context()->RequestQuit();
  } else {
    video_capture::uma::LogVideoCaptureServiceEvent(
        video_capture::uma::SERVICE_SHUTDOWN_TIMEOUT_CANCELED);
  }
}

}  // namespace video_capture
