// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/origin_access_list.h"

namespace network {

namespace cors {

OriginAccessList::OriginAccessList() = default;
OriginAccessList::~OriginAccessList() = default;

void OriginAccessList::SetAllowListForOrigin(
    const url::Origin& source_origin,
    const std::vector<mojom::CorsOriginPatternPtr>& patterns) {
  SetForOrigin(source_origin, patterns, &allow_list_);
}

void OriginAccessList::AddAllowListEntryForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    bool allow_subdomains) {
  AddForOrigin(source_origin, protocol, domain, allow_subdomains, &allow_list_);
}

void OriginAccessList::ClearAllowList() {
  allow_list_.clear();
}

void OriginAccessList::SetBlockListForOrigin(
    const url::Origin& source_origin,
    const std::vector<mojom::CorsOriginPatternPtr>& patterns) {
  SetForOrigin(source_origin, patterns, &block_list_);
}

void OriginAccessList::AddBlockListEntryForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    bool allow_subdomains) {
  AddForOrigin(source_origin, protocol, domain, allow_subdomains, &block_list_);
}

void OriginAccessList::ClearBlockList() {
  block_list_.clear();
}

bool OriginAccessList::IsAllowed(const url::Origin& source_origin,
                                 const GURL& destination) const {
  if (source_origin.unique())
    return false;
  std::string source = source_origin.Serialize();
  url::Origin destination_origin = url::Origin::Create(destination);
  return IsInMapForOrigin(source, destination_origin, allow_list_) &&
         !IsInMapForOrigin(source, destination_origin, block_list_);
}

// static
void OriginAccessList::SetForOrigin(
    const url::Origin& source_origin,
    const std::vector<mojom::CorsOriginPatternPtr>& patterns,
    PatternMap* map) {
  DCHECK(map);
  DCHECK(!source_origin.unique());

  std::string source = source_origin.Serialize();
  map->erase(source);
  if (patterns.empty())
    return;

  Patterns& native_patterns = (*map)[source];
  for (const auto& pattern : patterns) {
    native_patterns.push_back(OriginAccessEntry(
        pattern->protocol, pattern->domain,
        pattern->allow_subdomains ? OriginAccessEntry::kAllowSubdomains
                                  : OriginAccessEntry::kDisallowSubdomains));
  }
}

// static
void OriginAccessList::AddForOrigin(const url::Origin& source_origin,
                                    const std::string& protocol,
                                    const std::string& domain,
                                    bool allow_subdomains,
                                    PatternMap* map) {
  DCHECK(map);
  DCHECK(!source_origin.unique());

  std::string source = source_origin.Serialize();
  (*map)[source].push_back(OriginAccessEntry(
      protocol, domain,
      allow_subdomains ? OriginAccessEntry::kAllowSubdomains
                       : OriginAccessEntry::kDisallowSubdomains));
}

// static
bool OriginAccessList::IsInMapForOrigin(const std::string& source,
                                        const url::Origin& destination_origin,
                                        const PatternMap& map) {
  auto patterns_for_origin_it = map.find(source);
  if (patterns_for_origin_it == map.end())
    return false;
  for (const auto& entry : patterns_for_origin_it->second) {
    if (entry.MatchesOrigin(destination_origin) !=
        OriginAccessEntry::kDoesNotMatchOrigin) {
      return true;
    }
  }
  return false;
}

}  // namespace cors

}  // namespace network
