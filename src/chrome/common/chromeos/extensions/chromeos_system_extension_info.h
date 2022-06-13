// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSION_INFO_H_
#define CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSION_INFO_H_

#include <cstddef>
#include <string>

namespace chromeos {

namespace switches {

extern const char kTelemetryExtensionPwaOriginOverrideForTesting[];

}  // namespace switches

struct ChromeOSSystemExtensionInfo {
  ChromeOSSystemExtensionInfo(const std::string& manufacturer,
                              const std::string& pwa_origin);
  ChromeOSSystemExtensionInfo(const ChromeOSSystemExtensionInfo& other);
  ~ChromeOSSystemExtensionInfo();

  const std::string manufacturer;
  std::string pwa_origin;
};

size_t GetChromeOSSystemExtensionInfosSize();
ChromeOSSystemExtensionInfo GetChromeOSExtensionInfoForId(
    const std::string& id);
bool IsChromeOSSystemExtension(const std::string& id);

}  // namespace chromeos

#endif  // CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSION_INFO_H_
