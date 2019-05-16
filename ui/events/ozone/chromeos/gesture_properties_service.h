// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_CHROMEOS_GESTURE_PROPERTIES_SERVICE_H_
#define UI_EVENTS_OZONE_CHROMEOS_GESTURE_PROPERTIES_SERVICE_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#include "ui/ozone/public/interfaces/gesture_properties_service.mojom.h"

class GesturePropertyProvider;

namespace ui {

class EVENTS_OZONE_EVDEV_EXPORT GesturePropertiesService
    : public ui::ozone::mojom::GesturePropertiesService {
 public:
  GesturePropertiesService(
      GesturePropertyProvider* provider,
      ui::ozone::mojom::GesturePropertiesServiceRequest request);

  void ListDevices(ListDevicesCallback callback) override;
  void ListProperties(int32_t device_id,
                      ListPropertiesCallback callback) override;
  void GetProperty(int32_t device_id,
                   const std::string& name,
                   GetPropertyCallback callback) override;
  void SetProperty(int32_t device_id,
                   const std::string& name,
                   ui::ozone::mojom::GesturePropValuePtr values,
                   SetPropertyCallback callback) override;

 private:
  GesturePropertyProvider* prop_provider_;
  mojo::Binding<ui::ozone::mojom::GesturePropertiesService> binding_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_CHROMEOS_GESTURE_PROPERTIES_SERVICE_H_
