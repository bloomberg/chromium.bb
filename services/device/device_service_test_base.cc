// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service_test_base.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/device_service.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"

namespace device {

namespace {

// Simply return a nullptr which means no CustomLocationProvider from embedder.
std::unique_ptr<LocationProvider> GetCustomLocationProviderForTest() {
  return nullptr;
}

std::unique_ptr<DeviceService> CreateTestDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    service_manager::mojom::ServiceRequest request) {
#if defined(OS_ANDROID)
  return CreateDeviceService(
      file_task_runner, io_task_runner, url_loader_factory,
      kTestGeolocationApiKey, false, WakeLockContextCallback(),
      base::BindRepeating(&GetCustomLocationProviderForTest), nullptr,
      std::move(request));
#else
  return CreateDeviceService(
      file_task_runner, io_task_runner, url_loader_factory,
      kTestGeolocationApiKey,
      base::BindRepeating(&GetCustomLocationProviderForTest),
      std::move(request));
#endif
}

}  // namespace

DeviceServiceTestBase::DeviceServiceTestBase()
    : file_thread_("DeviceServiceTestFileThread"),
      io_thread_("DeviceServiceTestIOThread"),
      connector_(test_connector_factory_.CreateConnector()) {
  file_thread_.Start();
  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  service_ = CreateTestDeviceService(
      file_thread_.task_runner(), io_thread_.task_runner(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      test_connector_factory_.RegisterInstance(mojom::kServiceName));
}

DeviceServiceTestBase::~DeviceServiceTestBase() = default;

}  // namespace device
