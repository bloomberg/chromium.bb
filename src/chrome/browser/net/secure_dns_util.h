// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_
#define CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_

#include <vector>

#include "base/strings/string_piece.h"

namespace net {
struct DnsConfigOverrides;
}  // namespace net

class PrefRegistrySimple;
class PrefService;

namespace chrome_browser_net {

namespace secure_dns {

// Implements the whitespace-delimited group syntax for DoH templates.
std::vector<base::StringPiece> SplitGroup(base::StringPiece group);

// Returns true if a group of templates are all valid per
// net::dns_util::IsValidDohTemplate().  This should be checked before updating
// stored preferences.
bool IsValidGroup(base::StringPiece group);

// Modifies |overrides| to use the DoH server specified by |server_template|.
void ApplyTemplate(net::DnsConfigOverrides* overrides,
                   base::StringPiece server_template);

// Registers the backup preference required for the DNS probes setting reset.
// TODO(crbug.com/1062698): Remove this once the privacy settings redesign
// is fully launched.
void RegisterProbesSettingBackupPref(PrefRegistrySimple* registry);

// Backs up the unneeded preference controlling DNS and captive portal probes
// once the privacy settings redesign is enabled, or restores the backup
// in case the feature is rolled back.
// TODO(crbug.com/1062698): Remove this once the privacy settings redesign
// is fully launched.
void MigrateProbesSettingToOrFromBackup(PrefService* prefs);

}  // namespace secure_dns

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_SECURE_DNS_UTIL_H_
