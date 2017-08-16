// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_INPUT_DEVICES_TOUCH_DEVICE_SERVER_H_
#define SERVICES_UI_INPUT_DEVICES_TOUCH_DEVICE_SERVER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/ui/public/interfaces/input_devices/touch_device_server.mojom.h"

namespace display {
class DefaultTouchTransformSetter;
}

namespace ui {

// Implementation of mojom::TouchDeviceServer. Uses an instance of
// DefaultTouchTransformSetter to apply the actual updates.
class TouchDeviceServer : public mojom::TouchDeviceServer {
 public:
  TouchDeviceServer();
  ~TouchDeviceServer() override;

  // Adds interface with the connection registry so remote observers can
  // connect.
  void AddInterface(service_manager::BinderRegistryWithArgs<
                    const service_manager::BindSourceInfo&>* registry);

  // mojom::TouchDeviceServer:
  void ConfigureTouchDevices(
      const std::vector<ui::TouchDeviceTransform>& transforms) override;

 private:
  void BindTouchDeviceServerRequest(
      mojom::TouchDeviceServerRequest request,
      const service_manager::BindSourceInfo& source_info);

  mojo::BindingSet<mojom::TouchDeviceServer> bindings_;
  std::unique_ptr<display::DefaultTouchTransformSetter> touch_transform_setter_;

  DISALLOW_COPY_AND_ASSIGN(TouchDeviceServer);
};

}  // namespace ui

#endif  // SERVICES_UI_INPUT_DEVICES_TOUCH_DEVICE_SERVER_H_
