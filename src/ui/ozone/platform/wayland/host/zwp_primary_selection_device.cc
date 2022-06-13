// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_primary_selection_device.h"

#include <primary-selection-unstable-v1-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/zwp_primary_selection_offer.h"

namespace ui {

// static
ZwpPrimarySelectionDevice::ZwpPrimarySelectionDevice(
    WaylandConnection* connection,
    zwp_primary_selection_device_v1* data_device)
    : WaylandDataDeviceBase(connection), data_device_(data_device) {
  static constexpr zwp_primary_selection_device_v1_listener kListener = {
      &OnDataOffer, &OnSelection};
  zwp_primary_selection_device_v1_add_listener(data_device_.get(), &kListener,
                                               this);
}

ZwpPrimarySelectionDevice::~ZwpPrimarySelectionDevice() = default;

void ZwpPrimarySelectionDevice::SetSelectionSource(
    ZwpPrimarySelectionSource* source) {
  auto serial = GetSerialForSelection();
  if (!serial.has_value()) {
    LOG(ERROR) << "Failed to set selection. No serial found.";
    return;
  }
  auto* data_source = source ? source->data_source() : nullptr;
  zwp_primary_selection_device_v1_set_selection(data_device_.get(), data_source,
                                                serial->value);
  connection()->ScheduleFlush();
}

// static
void ZwpPrimarySelectionDevice::OnDataOffer(
    void* data,
    zwp_primary_selection_device_v1* data_device,
    zwp_primary_selection_offer_v1* offer) {
  auto* self = static_cast<ZwpPrimarySelectionDevice*>(data);
  DCHECK(self);
  self->set_data_offer(std::make_unique<ZwpPrimarySelectionOffer>(offer));
}

// static
void ZwpPrimarySelectionDevice::OnSelection(
    void* data,
    zwp_primary_selection_device_v1* data_device,
    zwp_primary_selection_offer_v1* offer) {
  auto* self = static_cast<ZwpPrimarySelectionDevice*>(data);
  DCHECK(self);

  // 'offer' will be null to indicate that the selection is no longer valid,
  // i.e. there is no longer selection data available to be fetched.
  if (!offer) {
    self->ResetDataOffer();
  } else {
    DCHECK(self->data_offer());
    self->data_offer()->EnsureTextMimeTypeIfNeeded();
  }

  self->NotifySelectionOffer(self->data_offer());
}

}  // namespace ui
