// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace url_formatter {

namespace top_domains {

namespace {

// Minimum length for a hostname to be considered for an edit distance
// comparison. Shorter domains are ignored.
const size_t kMinLengthForEditDistance = 5u;

}  // namespace

std::string HostnameWithoutRegistry(const std::string& hostname) {
  DCHECK(!hostname.empty());
  const size_t registry_size =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          hostname.c_str(),
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  return hostname.substr(0, hostname.size() - registry_size);
}

bool IsEditDistanceCandidate(const std::string& hostname) {
  return !hostname.empty() &&
         HostnameWithoutRegistry(hostname).size() >= kMinLengthForEditDistance;
}

}  // namespace top_domains

}  // namespace url_formatter
