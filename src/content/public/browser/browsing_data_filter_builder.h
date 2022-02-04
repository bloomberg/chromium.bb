// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSING_DATA_FILTER_BUILDER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSING_DATA_FILTER_BUILDER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

class GURL;

namespace url {
class Origin;
}

namespace content {

// A class that builds Origin->bool predicates to filter browsing data. These
// filters can be of two modes - a list of items to delete or a list of items to
// preserve, deleting everything else. The filter entries can be origins or
// registrable domains.
//
// This class defines interface to build filters for various kinds of browsing
// data. |BuildOriginFilter()| is useful for most browsing data storage backends,
// but some backends, such as website settings and cookies, use other formats of
// filter.
class CONTENT_EXPORT BrowsingDataFilterBuilder {
 public:
  enum class Mode {
    // Only the origins given will be deleted.
    kDelete,
    // All origins EXCEPT the origins given will be deleted.
    kPreserve
  };

  // Constructs a filter with the given |mode|: delete or preserve.
  static std::unique_ptr<BrowsingDataFilterBuilder> Create(Mode mode);

  virtual ~BrowsingDataFilterBuilder() = default;

  // Adds an origin to the filter. Note that this makes it impossible to
  // create cookie, channel ID, or plugin filters, as those datatypes are
  // scoped more broadly than an origin.
  virtual void AddOrigin(const url::Origin& origin) = 0;

  // Adds a registrable domain (e.g. google.com), an internal hostname
  // (e.g. localhost), or an IP address (e.g. 127.0.0.1). Other domains, such
  // as third and lower level domains (e.g. www.google.com) are not accepted.
  // Formally, it must hold that GetDomainAndRegistry(|registrable_domain|, _)
  // is |registrable_domain| itself or an empty string for this method
  // to accept it.
  virtual void AddRegisterableDomain(const std::string& registrable_domain) = 0;

  // Set the CookiePartitionKeyCollection for a CookieDeletionFilter.
  // Partitioned cookies will be not be deleted if their partition key is not in
  // the keychain. If this method is not invoked, then by default this clears
  // all partitioned cookies that match the other criteria.
  virtual void SetCookiePartitionKeyCollection(
      const net::CookiePartitionKeyCollection&
          cookie_partition_key_collection) = 0;

  // Returns true if this filter is handling a Clear-Site-Data header sent in a
  // cross-site context.
  virtual bool IsCrossSiteClearSiteData() const = 0;

  // Returns true if we're an empty preserve list, where we delete everything.
  virtual bool MatchesAllOriginsAndDomains() = 0;

  // Deprecated: Prefer `BuildOriginFilter()` instead.
  // Builds a filter that matches URLs that are in the list to delete, or aren't
  // in the list to preserve.
  virtual base::RepeatingCallback<bool(const GURL&)> BuildUrlFilter() = 0;

  // Builds a filter that matches origins that are in the list to delete, or
  // aren't in the list to preserve. This is preferred to BuildUrlFilter() as
  // it does not inherently perform GURL->Origin conversions.
  virtual base::RepeatingCallback<bool(const url::Origin&)>
  BuildOriginFilter() = 0;

  // Builds a filter that can be used with the network service. This uses a Mojo
  // struct rather than a predicate function (as used by the rest of the filters
  // built by this class) because we need to be able to pass the filter to the
  // network service via IPC. Returns nullptr if |IsEmptyPreserveList()| is
  // true.
  virtual network::mojom::ClearDataFilterPtr BuildNetworkServiceFilter() = 0;

  // Builds a CookieDeletionInfo object that matches cookies whose sources are
  // in the list to delete, or aren't in the list to preserve.
  virtual network::mojom::CookieDeletionFilterPtr
  BuildCookieDeletionFilter() = 0;

  // Builds a filter that matches the |site| of a plugin.
  virtual base::RepeatingCallback<bool(const std::string& site)>
  BuildPluginFilter() = 0;

  // A convenience method to produce an empty preserve list, a filter that
  // matches everything.
  static base::RepeatingCallback<bool(const GURL&)> BuildNoopFilter();

  // The mode of the filter.
  virtual Mode GetMode() = 0;

  // Create a new filter builder with the same set of origins, set of domains,
  // and mode.
  virtual std::unique_ptr<BrowsingDataFilterBuilder> Copy() = 0;

  // Comparison.
  virtual bool operator==(const BrowsingDataFilterBuilder& other) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSING_DATA_FILTER_BUILDER_H_
