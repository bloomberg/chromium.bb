// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/origin_access_list.h"

#include "services/network/public/mojom/cors_origin_pattern.mojom.h"

namespace network {

namespace cors {

OriginAccessList::OriginAccessList() = default;
OriginAccessList::~OriginAccessList() = default;

void OriginAccessList::SetAllowListForOrigin(
    const url::Origin& source_origin,
    const std::vector<CorsOriginPatternPtr>& patterns) {
  SetForOrigin(source_origin, patterns, &allow_list_);
}

void OriginAccessList::AddAllowListEntryForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    const mojom::CorsOriginAccessMatchMode mode,
    const mojom::CorsOriginAccessMatchPriority priority) {
  AddForOrigin(source_origin,
               mojom::CorsOriginPattern::New(protocol, domain, mode, priority),
               &allow_list_);
}

void OriginAccessList::ClearAllowListForOrigin(
    const url::Origin& source_origin) {
  SetForOrigin(source_origin, std::vector<mojom::CorsOriginPatternPtr>(),
               &allow_list_);
}

void OriginAccessList::ClearAllowList() {
  allow_list_.clear();
}

void OriginAccessList::SetBlockListForOrigin(
    const url::Origin& source_origin,
    const std::vector<CorsOriginPatternPtr>& patterns) {
  SetForOrigin(source_origin, patterns, &block_list_);
}

void OriginAccessList::AddBlockListEntryForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    const mojom::CorsOriginAccessMatchMode mode,
    const mojom::CorsOriginAccessMatchPriority priority) {
  AddForOrigin(source_origin,
               mojom::CorsOriginPattern::New(protocol, domain, mode, priority),
               &block_list_);
}

void OriginAccessList::ClearBlockListForOrigin(
    const url::Origin& source_origin) {
  SetForOrigin(source_origin, std::vector<mojom::CorsOriginPatternPtr>(),
               &block_list_);
}

void OriginAccessList::ClearBlockList() {
  block_list_.clear();
}

bool OriginAccessList::IsAllowed(const url::Origin& source_origin,
                                 const GURL& destination) const {
  if (source_origin.opaque())
    return false;
  std::string source = source_origin.Serialize();
  url::Origin destination_origin = url::Origin::Create(destination);
  network::mojom::CorsOriginAccessMatchPriority allow_list_priority =
      GetHighestPriorityOfRuleForOrigin(source, destination_origin,
                                        allow_list_);
  if (allow_list_priority ==
      network::mojom::CorsOriginAccessMatchPriority::kNoMatchingOrigin)
    return false;
  network::mojom::CorsOriginAccessMatchPriority block_list_priority =
      GetHighestPriorityOfRuleForOrigin(source, destination_origin,
                                        block_list_);
  if (block_list_priority ==
      network::mojom::CorsOriginAccessMatchPriority::kNoMatchingOrigin)
    return true;
  return allow_list_priority > block_list_priority;
}

std::vector<mojo::StructPtr<mojom::CorsOriginAccessPatterns>>
OriginAccessList::CreateCorsOriginAccessPatternsList() const {
  std::set<std::string> origins;
  for (const auto& allow_map : allow_list_)
    origins.insert(allow_map.first);
  for (const auto& block_map : block_list_)
    origins.insert(block_map.first);

  std::vector<mojom::CorsOriginAccessPatternsPtr> access_patterns;
  for (const auto& origin : origins) {
    std::vector<mojom::CorsOriginPatternPtr> allow_patterns;
    const auto& allow_entries = allow_list_.find(origin);
    if (allow_entries != allow_list_.end()) {
      for (const auto& pattern : allow_entries->second)
        allow_patterns.push_back(pattern.CreateCorsOriginPattern());
    }
    std::vector<mojom::CorsOriginPatternPtr> block_patterns;
    const auto& block_entries = block_list_.find(origin);
    if (block_entries != block_list_.end()) {
      for (const auto& pattern : block_entries->second)
        block_patterns.push_back(pattern.CreateCorsOriginPattern());
    }
    access_patterns.push_back(mojom::CorsOriginAccessPatterns::New(
        origin, std::move(allow_patterns), std::move(block_patterns)));
  }
  return access_patterns;
}

// static
void OriginAccessList::SetForOrigin(
    const url::Origin& source_origin,
    const std::vector<CorsOriginPatternPtr>& patterns,
    PatternMap* map) {
  DCHECK(map);
  DCHECK(!source_origin.opaque());

  std::string source = source_origin.Serialize();
  map->erase(source);
  if (patterns.empty())
    return;

  Patterns& native_patterns = (*map)[source];
  for (const auto& pattern : patterns) {
    native_patterns.push_back(OriginAccessEntry(
        pattern->protocol, pattern->domain, pattern->mode, pattern->priority));
  }
}

// static
void OriginAccessList::AddForOrigin(const url::Origin& source_origin,
                                    const CorsOriginPatternPtr& pattern,
                                    PatternMap* map) {
  DCHECK(map);
  DCHECK(!source_origin.opaque());

  std::string source = source_origin.Serialize();
  (*map)[source].push_back(OriginAccessEntry(pattern->protocol, pattern->domain,
                                             pattern->mode, pattern->priority));
}

// static
// TODO(nrpeter): Sort OriginAccessEntry entries on edit then we can return the
// first match which will be the top priority.
network::mojom::CorsOriginAccessMatchPriority
OriginAccessList::GetHighestPriorityOfRuleForOrigin(
    const std::string& source,
    const url::Origin& destination_origin,
    const PatternMap& map) {
  network::mojom::CorsOriginAccessMatchPriority highest_priority =
      network::mojom::CorsOriginAccessMatchPriority::kNoMatchingOrigin;
  auto patterns_for_origin_it = map.find(source);
  if (patterns_for_origin_it == map.end())
    return highest_priority;
  for (const auto& entry : patterns_for_origin_it->second) {
    if (entry.MatchesOrigin(destination_origin) !=
        OriginAccessEntry::kDoesNotMatchOrigin) {
      highest_priority = std::max(highest_priority, entry.priority());
    }
  }
  return highest_priority;
}

}  // namespace cors

}  // namespace network
