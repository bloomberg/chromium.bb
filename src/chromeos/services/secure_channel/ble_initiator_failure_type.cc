// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_initiator_failure_type.h"

namespace chromeos {

namespace secure_channel {

std::ostream& operator<<(std::ostream& stream,
                         const BleInitiatorFailureType& failure_type) {
  switch (failure_type) {
    case BleInitiatorFailureType::kAuthenticationError:
      stream << "[authentication error]";
      break;
    case BleInitiatorFailureType::kGattConnectionError:
      stream << "[GATT connection error]";
      break;
    case BleInitiatorFailureType::kInterruptedByHigherPriorityConnectionAttempt:
      stream << "[interrupted by higher priority attempt]";
      break;
    case BleInitiatorFailureType::kTimeoutContactingRemoteDevice:
      stream << "[timeout contacting remote device]";
      break;
    case BleInitiatorFailureType::kCouldNotGenerateAdvertisement:
      stream << "[could not generate advertisement]";
      break;
  }
  return stream;
}

}  // namespace secure_channel

}  // namespace chromeos
