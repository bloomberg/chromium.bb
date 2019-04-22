// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/page_criteria.h"

#include "base/strings/string_number_conversions.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_item_utils.h"

namespace offline_pages {

PageCriteria::PageCriteria() = default;
PageCriteria::~PageCriteria() = default;
PageCriteria::PageCriteria(const PageCriteria&) = default;
PageCriteria::PageCriteria(PageCriteria&&) = default;

bool MeetsCriteria(const ClientPolicyController& policy_controller,
                   const PageCriteria& criteria,
                   const ClientId& client_id) {
  if (criteria.exclude_tab_bound_pages) {
    const OfflinePageClientPolicy& policy =
        policy_controller.GetPolicy(client_id.name_space);
    if (policy.feature_policy.is_restricted_to_tab_from_client_id)
      return false;
  }
  if (criteria.pages_for_tab_id &&
      policy_controller.IsRestrictedToTabFromClientId(client_id.name_space)) {
    std::string tab_id_str =
        base::NumberToString(criteria.pages_for_tab_id.value());
    if (client_id.id != tab_id_str)
      return false;
  }
  if (!criteria.client_ids.empty()) {
    if (std::find(criteria.client_ids.begin(), criteria.client_ids.end(),
                  client_id) == criteria.client_ids.end()) {
      return false;
    }
  }
  if (!criteria.client_namespaces.empty()) {
    if (std::find(criteria.client_namespaces.begin(),
                  criteria.client_namespaces.end(),
                  client_id.name_space) == criteria.client_namespaces.end()) {
      return false;
    }
  }
  if (criteria.supported_by_downloads &&
      !policy_controller.IsSupportedByDownload(client_id.name_space)) {
    return false;
  }
  if (criteria.removed_on_cache_reset &&
      !policy_controller.IsRemovedOnCacheReset(client_id.name_space)) {
    return false;
  }
  if (!criteria.guid.empty() && client_id.id != criteria.guid)
    return false;

  return true;
}

bool MeetsCriteria(const ClientPolicyController& policy_controller,
                   const PageCriteria& criteria,
                   const OfflinePageItem& item) {
  if (!MeetsCriteria(policy_controller, criteria, item.client_id))
    return false;

  if (criteria.file_size && item.file_size != criteria.file_size.value())
    return false;
  if (!criteria.digest.empty() && item.digest != criteria.digest)
    return false;

  if (!criteria.request_origin.empty() &&
      item.request_origin != criteria.request_origin)
    return false;

  if (!criteria.url.is_empty() &&
      !EqualsIgnoringFragment(criteria.url, item.url) &&
      (item.original_url_if_different.is_empty() ||
       !EqualsIgnoringFragment(criteria.url, item.original_url_if_different))) {
    return false;
  }

  if (criteria.offline_id && criteria.offline_id.value() != item.offline_id)
    return false;

  return true;
}

std::vector<std::string> PotentiallyMatchingNamespaces(
    const ClientPolicyController& policy_controller,
    const PageCriteria& criteria) {
  std::vector<std::string> matching_namespaces;
  if (criteria.supported_by_downloads || criteria.removed_on_cache_reset) {
    std::vector<std::string> allowed_namespaces;
    if (criteria.client_namespaces.empty()) {
      allowed_namespaces = policy_controller.GetAllNamespaces();
    } else {
      allowed_namespaces = criteria.client_namespaces;
    }
    std::vector<std::string> filtered;
    for (const std::string& name_space : allowed_namespaces) {
      if (criteria.supported_by_downloads &&
          !policy_controller.IsSupportedByDownload(name_space)) {
        continue;
      }
      if (criteria.removed_on_cache_reset &&
          !policy_controller.IsRemovedOnCacheReset(name_space)) {
        continue;
      }
      matching_namespaces.push_back(name_space);
    }
  } else if (!criteria.client_namespaces.empty()) {
    matching_namespaces = criteria.client_namespaces;
  }
  // no filter otherwise.

  return matching_namespaces;
}

}  // namespace offline_pages
