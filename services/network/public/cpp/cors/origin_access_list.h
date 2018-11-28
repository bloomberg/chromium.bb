// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_LIST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_LIST_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
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
  using CorsOriginPatternPtr = mojo::InlinedStructPtr<mojom::CorsOriginPattern>;

  OriginAccessList();
  ~OriginAccessList();

  // Clears the old allow list for |source_origin|, and set |patterns| to the
  // allow list. When two or more patterns in a list match, the entry with the
  // higher |priority| takes precedence.
  void SetAllowListForOrigin(const url::Origin& source_origin,
                             const std::vector<CorsOriginPatternPtr>& patterns);

  // Adds an access pattern by |protocol|, |domain|, |mode|, and |priority|,
  // to the allow list for |source_origin|.
  void AddAllowListEntryForOrigin(
      const url::Origin& source_origin,
      const std::string& protocol,
      const std::string& domain,
      const mojom::CorsOriginAccessMatchMode mode,
      const mojom::CorsOriginAccessMatchPriority priority);

  // Clears the old allow list for |source_origin|.
  void ClearAllowListForOrigin(const url::Origin& source_origin);

  // Clears the old allow list.
  void ClearAllowList();

  // Clears the old block list for |source_origin| and set |patterns| to the
  // block list. When two or more patterns in a list match, the entry with the
  // higher |priority| takes precedence.
  void SetBlockListForOrigin(const url::Origin& source_origin,
                             const std::vector<CorsOriginPatternPtr>& patterns);

  // Adds an access pattern by |protocol|, |domain|, |mode|, and |priority|,
  // to the block list for |source_origin|.
  void AddBlockListEntryForOrigin(
      const url::Origin& source_origin,
      const std::string& protocol,
      const std::string& domain,
      const mojom::CorsOriginAccessMatchMode mode,
      const mojom::CorsOriginAccessMatchPriority priority);

  // Clears the old block list for |source_origin|.
  void ClearBlockListForOrigin(const url::Origin& source_origin);

  // Clears the old block list.
  void ClearBlockList();

  // Returns true if |destination| is in the allow list, and not in the block
  // list of the |source_origin|.
  bool IsAllowed(const url::Origin& source_origin,
                 const GURL& destination) const;

  // Creates mojom::CorsPriginAccessPatterns instance vector that represents
  // |this| OriginAccessList instance.
  std::vector<mojo::StructPtr<mojom::CorsOriginAccessPatterns>>
  CreateCorsOriginAccessPatternsList() const;

 private:
  using Patterns = std::vector<OriginAccessEntry>;
  using PatternMap = std::map<std::string, Patterns>;

  static void SetForOrigin(const url::Origin& source_origin,
                           const std::vector<CorsOriginPatternPtr>& patterns,
                           PatternMap* map);
  static void AddForOrigin(const url::Origin& source_origin,
                           const CorsOriginPatternPtr& pattern,
                           PatternMap* map);
  static mojom::CorsOriginAccessMatchPriority GetHighestPriorityOfRuleForOrigin(
      const std::string& source,
      const url::Origin& destination_origin,
      const PatternMap& map);

  // TODO(toyoshim): Redesign to have an unified map to be consistent with
  // mojom::CorsOriginAccessPatterns. See https://crbug.com/908756.
  PatternMap allow_list_;
  PatternMap block_list_;

  DISALLOW_COPY_AND_ASSIGN(OriginAccessList);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_LIST_H_
