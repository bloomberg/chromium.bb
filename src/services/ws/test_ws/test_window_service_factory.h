// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_TEST_WS_TEST_WINDOW_SERVICE_FACTORY_H_
#define SERVICES_WS_TEST_WS_TEST_WINDOW_SERVICE_FACTORY_H_

#include <memory>

namespace service_manager {
class Service;
}  // namespace service_manager

namespace ui {
class ContextFactory;
class ContextFactoryPrivate;
}  // namespace ui

namespace ws {
class GpuInterfaceProvider;
}  // namespace ws

namespace ws {
namespace test {

std::unique_ptr<service_manager::Service> CreateInProcessWindowService(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private,
    std::unique_ptr<GpuInterfaceProvider> gpu_interface_provider);

std::unique_ptr<service_manager::Service> CreateOutOfProcessWindowService();

}  // namespace test
}  // namespace ws

#endif  // SERVICES_WS_TEST_WS_TEST_WINDOW_SERVICE_FACTORY_H_
