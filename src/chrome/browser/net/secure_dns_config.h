// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SECURE_DNS_CONFIG_H_
#define CHROME_BROWSER_NET_SECURE_DNS_CONFIG_H_

#include <vector>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "net/dns/dns_config.h"

namespace net {
struct DnsOverHttpsServerConfig;
}  // namespace net

// Representation of a complete Secure DNS configuration.
class SecureDnsConfig {
 public:
  // Forced management description types. We will check for the override cases
  // in the order they are listed in the enum.
  enum class ManagementMode {
    // Chrome did not override the secure DNS settings.
    kNoOverride,
    // Secure DNS was disabled due to detection of a managed environment.
    kDisabledManaged,
    // Secure DNS was disabled due to detection of OS-level parental controls.
    kDisabledParentalControls,
  };

  // String representations for net::DnsConfig::SecureDnsMode.  Used for both
  // configuration storage and UI state.
  static constexpr char kModeOff[] = "off";
  static constexpr char kModeAutomatic[] = "automatic";
  static constexpr char kModeSecure[] = "secure";

  SecureDnsConfig(net::DnsConfig::SecureDnsMode mode,
                  std::vector<net::DnsOverHttpsServerConfig> servers,
                  ManagementMode management_mode);
  // This class is move-only to avoid any accidental copying.
  SecureDnsConfig(SecureDnsConfig&& other);
  SecureDnsConfig& operator=(SecureDnsConfig&& other);
  ~SecureDnsConfig();

  // Identifies the SecureDnsMode corresponding to one of the above names, or
  // returns nullopt if the name is unrecognized.
  static base::Optional<net::DnsConfig::SecureDnsMode> ParseMode(
      base::StringPiece name);
  // Converts a secure DNS mode to one of the above names.
  static const char* ModeToString(net::DnsConfig::SecureDnsMode mode);

  net::DnsConfig::SecureDnsMode mode() { return mode_; }
  const std::vector<net::DnsOverHttpsServerConfig>& servers() {
    return servers_;
  }
  ManagementMode management_mode() { return management_mode_; }

 private:
  net::DnsConfig::SecureDnsMode mode_;
  std::vector<net::DnsOverHttpsServerConfig> servers_;
  ManagementMode management_mode_;
};

#endif  // CHROME_BROWSER_NET_SECURE_DNS_CONFIG_H_
