// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBEndpoint.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMException.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "modules/webusb/USBAlternateInterface.h"

using device::usb::blink::EndpointType;
using device::usb::blink::TransferDirection;

namespace blink {

namespace {

String ConvertDirectionToEnum(const TransferDirection& direction) {
  switch (direction) {
    case TransferDirection::INBOUND:
      return "in";
    case TransferDirection::OUTBOUND:
      return "out";
    default:
      ASSERT_NOT_REACHED();
      return "";
  }
}

String ConvertTypeToEnum(const EndpointType& type) {
  switch (type) {
    case EndpointType::BULK:
      return "bulk";
    case EndpointType::INTERRUPT:
      return "interrupt";
    case EndpointType::ISOCHRONOUS:
      return "isochronous";
    default:
      ASSERT_NOT_REACHED();
      return "";
  }
}

}  // namespace

USBEndpoint* USBEndpoint::Create(const USBAlternateInterface* alternate,
                                 size_t endpoint_index) {
  return new USBEndpoint(alternate, endpoint_index);
}

USBEndpoint* USBEndpoint::Create(const USBAlternateInterface* alternate,
                                 size_t endpoint_number,
                                 const String& direction,
                                 ExceptionState& exception_state) {
  TransferDirection mojo_direction = direction == "in"
                                         ? TransferDirection::INBOUND
                                         : TransferDirection::OUTBOUND;
  const auto& endpoints = alternate->Info().endpoints;
  for (size_t i = 0; i < endpoints.size(); ++i) {
    const auto& endpoint = endpoints[i];
    if (endpoint->endpoint_number == endpoint_number &&
        endpoint->direction == mojo_direction)
      return USBEndpoint::Create(alternate, i);
  }
  exception_state.ThrowRangeError(
      "No such endpoint exists in the given alternate interface.");
  return nullptr;
}

USBEndpoint::USBEndpoint(const USBAlternateInterface* alternate,
                         size_t endpoint_index)
    : alternate_(alternate), endpoint_index_(endpoint_index) {
  ASSERT(alternate_);
  ASSERT(endpoint_index_ < alternate_->Info().endpoints.size());
}

const device::usb::blink::EndpointInfo& USBEndpoint::Info() const {
  const device::usb::blink::AlternateInterfaceInfo& alternate_info =
      alternate_->Info();
  ASSERT(endpoint_index_ < alternate_info.endpoints.size());
  return *alternate_info.endpoints[endpoint_index_];
}

String USBEndpoint::direction() const {
  return ConvertDirectionToEnum(Info().direction);
}

String USBEndpoint::type() const {
  return ConvertTypeToEnum(Info().type);
}

DEFINE_TRACE(USBEndpoint) {
  visitor->Trace(alternate_);
}

}  // namespace blink
