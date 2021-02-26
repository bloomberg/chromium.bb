// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCANNING_ZEROCONF_SCANNER_DETECTOR_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_SCANNING_ZEROCONF_SCANNER_DETECTOR_UTILS_H_

#include <string>

#include "base/optional.h"
#include "chromeos/scanning/scanner.h"
#include "net/base/ip_address.h"

namespace chromeos {

// Creates a Scanner with a device name that can be used to interact with a
// scanner via the sane-airscan backend. If errors occur, base::nullopts is
// returned. The device name must be in the format "airscan:escl:name:url",
// where  name is an arbitrary name. The IP address is used instead of the host
// name since sane-airscan may not be able to resolve host names it did not
// discover itself. See mdns_make_escl_endpoint() at
// https://github.com/alexpevzner/sane-airscan/blob/master/airscan-mdns.c for
// more details.
base::Optional<Scanner> CreateSaneAirscanScanner(
    const std::string& name,
    const std::string& service_type,
    const std::string& rs,
    const net::IPAddress& ip_address,
    int port,
    bool usable = true);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCANNING_ZEROCONF_SCANNER_DETECTOR_UTILS_H_
