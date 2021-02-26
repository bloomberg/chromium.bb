// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_INSTALL_PREF_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_INSTALL_PREF_H_

#include <vector>

#include "base/optional.h"
#include "extensions/common/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {

struct RulesetInstallPref {
  // ID of the ruleset.
  RulesetID ruleset_id;
  // Checksum of the indexed ruleset if specified.
  base::Optional<int> checksum;

  // If set to true, then the ruleset was ignored and not indexed.
  bool ignored;

  RulesetInstallPref(RulesetID ruleset_id,
                     base::Optional<int> checksum,
                     bool ignored);
};

using RulesetInstallPrefs = std::vector<RulesetInstallPref>;

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_INSTALL_PREF_H_
