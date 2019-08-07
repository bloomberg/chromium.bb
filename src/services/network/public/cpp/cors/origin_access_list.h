// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_LIST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_LIST_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "services/network/public/cpp/cors/origin_access_entry.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom-shared.h"
#include "url/origin.h"

namespace network {

namespace mojom {
class CorsOriginPattern;
class CorsOriginAccessPatterns;
}  // namespace mojom

namespace cors {

// A class to manage origin access allow / block lists. If these lists conflict,
// blacklisting is respected. These lists are managed per source-origin basis.
class COMPONENT_EXPORT(NETWORK_CPP) OriginAccessList {
 public:
  using CorsOriginPatternPtr = mojo::StructPtr<mojom::CorsOriginPattern>;

  // Represents if a queried conditions are is allowed, blocked, or not listed
  // in the access list.
  enum class AccessState {
    kAllowed,
    kBlocked,
    kNotListed,
  };

  OriginAccessList();
  ~OriginAccessList();

  // Clears the old allow list for |source_origin|, and set |patterns| to the
  // allow list. When two or more patterns in a list match, the entry with the
  // higher |priority| takes precedence.
  void SetAllowListForOrigin(const url::Origin& source_origin,
                             const std::vector<CorsOriginPatternPtr>& patterns);

  // Adds an access pattern by |protocol|, |domain|, |port|,
  // |domain_match_mode|, |port_match_mode|, and |priority| to the allow list
  // for |source_origin|.
  void AddAllowListEntryForOrigin(
      const url::Origin& source_origin,
      const std::string& protocol,
      const std::string& domain,
      const uint16_t port,
      const mojom::CorsDomainMatchMode domain_match_mode,
      const mojom::CorsPortMatchMode port_match_mode,
      const mojom::CorsOriginAccessMatchPriority priority);

  // Clears the old block list for |source_origin| and set |patterns| to the
  // block list. When two or more patterns in a list match, the entry with the
  // higher |priority| takes precedence.
  void SetBlockListForOrigin(const url::Origin& source_origin,
                             const std::vector<CorsOriginPatternPtr>& patterns);

  // Adds an access pattern by |protocol|, |domain|, |port|,
  // |domain_match_mode|, |port_match_mode|, and |priority| to the block list
  // for |source_origin|.
  void AddBlockListEntryForOrigin(
      const url::Origin& source_origin,
      const std::string& protocol,
      const std::string& domain,
      const uint16_t port,
      const mojom::CorsDomainMatchMode domain_match_mode,
      const mojom::CorsPortMatchMode port_match_mode,
      const mojom::CorsOriginAccessMatchPriority priority);

  // Clears the old allow/block lists for |source_origin|.
  void ClearForOrigin(const url::Origin& source_origin);

  // Clears the old allow/block lists.
  void Clear();

  // Returns |destination|'s AccessState in the list for |source_origin|.
  AccessState CheckAccessState(const url::Origin& source_origin,
                               const GURL& destination) const;

  // Creates mojom::CorsPriginAccessPatterns instance vector that represents
  // |this| OriginAccessList instance.
  std::vector<mojo::StructPtr<mojom::CorsOriginAccessPatterns>>
  CreateCorsOriginAccessPatternsList() const;

 private:
  enum class MapType {
    kAllowPatterns,
    kBlockPatterns,
  };
  using Patterns = std::vector<OriginAccessEntry>;
  using PatternsMap = base::flat_map<MapType, Patterns>;
  using OriginPatternsMap =
      std::map<std::string /* source_origin */, PatternsMap>;

  static void SetForOrigin(const url::Origin& source_origin,
                           const std::vector<CorsOriginPatternPtr>& patterns,
                           OriginPatternsMap* map,
                           MapType type);
  static void AddForOrigin(const url::Origin& source_origin,
                           const CorsOriginPatternPtr& pattern,
                           OriginPatternsMap* map,
                           MapType type);
  static mojom::CorsOriginAccessMatchPriority GetHighestPriorityOfRuleForOrigin(
      const url::Origin& destination_origin,
      const PatternsMap& patterns_map,
      MapType type);

  OriginPatternsMap map_;

  DISALLOW_COPY_AND_ASSIGN(OriginAccessList);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_LIST_H_
