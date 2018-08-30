// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/network_interface.h"

#include <cstring>

namespace openscreen {
namespace platform {

void InterfaceInfo::CopyHardwareAddressTo(uint8_t x[6]) const {
  std::memcpy(x, hardware_address, sizeof(hardware_address));
}

}  // namespace platform
}  // namespace openscreen
