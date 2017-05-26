// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_SERVICE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_SERVICE_IMPL_H_

#include <memory>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/public/interfaces/device_factory_provider.mojom.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace video_capture {

class ServiceImpl : public service_manager::Service {
 public:
  ServiceImpl();
  ~ServiceImpl() override;

  // service_manager::Service implementation.
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  bool OnServiceManagerConnectionLost() override;

 private:
  void OnDeviceFactoryProviderRequest(
      const service_manager::BindSourceInfo& source_info,
      mojom::DeviceFactoryProviderRequest request);
  void SetShutdownDelayInSeconds(float seconds);
  void MaybeRequestQuitDelayed();
  void MaybeRequestQuit();

#if defined(OS_WIN)
  // COM must be initialized in order to access the video capture devices.
  base::win::ScopedCOMInitializer com_initializer_;
#endif
  float shutdown_delay_in_seconds_;
  service_manager::BinderRegistry registry_;
  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<ServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_SERVICE_IMPL_H_
