// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/test_ws/test_window_service_factory.h"

#include <utility>

#include "services/service_manager/public/cpp/service.h"
#include "services/ws/public/cpp/host/gpu_interface_provider.h"
#include "services/ws/test_ws/test_window_service.h"

namespace ws {
namespace test {

std::unique_ptr<service_manager::Service> CreateInProcessWindowService(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private,
    std::unique_ptr<GpuInterfaceProvider> gpu_interface_provider) {
  auto window_service = std::make_unique<TestWindowService>();
  window_service->InitForInProcess(context_factory, context_factory_private,
                                   std::move(gpu_interface_provider));
  return window_service;
}

std::unique_ptr<service_manager::Service> CreateOutOfProcessWindowService() {
  auto window_service = std::make_unique<TestWindowService>();
  return window_service;
}

}  // namespace test
}  // namespace ws
