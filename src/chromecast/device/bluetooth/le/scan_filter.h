// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_SCAN_FILTER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_SCAN_FILTER_H_

#include <string>

#include "base/optional.h"
#include "chromecast/device/bluetooth/le/le_scan_result.h"
#include "chromecast/public/bluetooth/bluetooth_types.h"

namespace chromecast {
namespace bluetooth {

struct ScanFilter {
  // Helper function to get ScanFilter which matches 16 bit |service_uuid|.
  static ScanFilter From16bitUuid(uint16_t service_uuid);

  ScanFilter();
  ScanFilter(const ScanFilter& other);
  ScanFilter(ScanFilter&& other);
  ~ScanFilter();

  bool Matches(const LeScanResult& scan_result) const;

  // Exact name.
  base::Optional<std::string> name;

  // RE2 partial match on name. This is ignored if |name| is specified.
  // https://github.com/google/re2
  base::Optional<std::string> regex_name;

  base::Optional<bluetooth_v2_shlib::Uuid> service_uuid;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_SCAN_FILTER_H_
