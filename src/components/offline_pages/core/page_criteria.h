// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PAGE_CRITERIA_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PAGE_CRITERIA_H_

#include <cstdint>

#include <string>
#include <vector>

#include "base/optional.h"
#include "components/offline_pages/core/client_id.h"
#include "url/gurl.h"

namespace offline_pages {
class ClientPolicyController;
struct OfflinePageItem;

// Criteria for matching an offline page. The default |PageCriteria| matches
// all pages.
struct PageCriteria {
  PageCriteria();
  ~PageCriteria();
  PageCriteria(const PageCriteria&);
  PageCriteria(PageCriteria&&);

  // If non-empty, the page must match this URL. The provided URL
  // is matched both against the original and the actual URL fields (they
  // sometimes differ because of possible redirects).
  GURL url;
  // Whether to exclude pages that may only be opened in a specific tab.
  bool exclude_tab_bound_pages = false;
  // If specified, accepts pages that can be displayed in the specified tab.
  // That is, tab-bound pages are filtered out unless the tab ID matches this
  // field and non-tab-bound pages are always included.
  base::Optional<int> pages_for_tab_id;
  // Whether to restrict pages to those in namespaces supported by the
  // downloads UI.
  bool supported_by_downloads = false;
  // Whether to restrict pages to those removed on cache reset.
  bool removed_on_cache_reset = false;
  // If set, the page's file_size must match.
  base::Optional<int64_t> file_size;
  // If non-empty, the page's digest must match.
  std::string digest;
  // If non-empty, the page's namespace must match.
  std::vector<std::string> client_namespaces;
  // If non-empty, the page's client_id must match one of these.
  std::vector<ClientId> client_ids;
  // If non-empty, the page's client_id.id must match this.
  std::string guid;
  // If > 0, returns at most this many pages.
  size_t maximum_matches = 0;
  // If non-empty, the page's request_origin must match.
  std::string request_origin;
  // If set, the page's offline_id must match.
  base::Optional<int64_t> offline_id;
};

// Returns true if an offline page with |client_id| could potentially match
// |criteria|.
bool MeetsCriteria(const ClientPolicyController& policy_controller,
                   const PageCriteria& criteria,
                   const ClientId& client_id);

// Returns whether |item| matches |criteria|.
bool MeetsCriteria(const ClientPolicyController& policy_controller,
                   const PageCriteria& criteria,
                   const OfflinePageItem& item);

// Returns the list of offline page namespaces that could potentially match
// Criteria. Returns an empty list if any namespace could match.
std::vector<std::string> PotentiallyMatchingNamespaces(
    const ClientPolicyController& policy_controller,
    const PageCriteria& criteria);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PAGE_CRITERIA_H_
