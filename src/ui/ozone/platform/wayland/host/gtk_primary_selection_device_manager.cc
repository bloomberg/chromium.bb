// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device_manager.h"

#include <gtk-primary-selection-client-protocol.h>

#include <memory>

#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"

namespace ui {

GtkPrimarySelectionDeviceManager::GtkPrimarySelectionDeviceManager(
    gtk_primary_selection_device_manager* manager,
    WaylandConnection* connection)
    : device_manager_(manager), connection_(connection) {
  DCHECK(connection_);
  DCHECK(device_manager_);
}

GtkPrimarySelectionDeviceManager::~GtkPrimarySelectionDeviceManager() = default;

GtkPrimarySelectionDevice* GtkPrimarySelectionDeviceManager::GetDevice() {
  DCHECK(connection_->seat());
  if (!device_) {
    device_ = std::make_unique<GtkPrimarySelectionDevice>(
        connection_, gtk_primary_selection_device_manager_get_device(
                         device_manager_.get(), connection_->seat()));
  }
  DCHECK(device_);
  return device_.get();
}

std::unique_ptr<GtkPrimarySelectionSource>
GtkPrimarySelectionDeviceManager::CreateSource(
    GtkPrimarySelectionSource::Delegate* delegate) {
  auto* data_source =
      gtk_primary_selection_device_manager_create_source(device_manager_.get());
  return std::make_unique<GtkPrimarySelectionSource>(data_source, connection_,
                                                     delegate);
}

}  // namespace ui
