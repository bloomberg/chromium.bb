// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_POLICY_CONTROLLER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_POLICY_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "components/offline_pages/core/offline_page_client_policy.h"

namespace offline_pages {

// This is the class which is a singleton for offline page model
// to get client policies based on namespaces.
class ClientPolicyController {
 public:
  ClientPolicyController();
  ClientPolicyController(const ClientPolicyController&) = delete;
  ~ClientPolicyController();

  ClientPolicyController& operator=(const ClientPolicyController&) = delete;

  // Get the client policy for |name_space|.
  const OfflinePageClientPolicy& GetPolicy(const std::string& name_space) const;

  // Returns a list of all known namespaces.
  std::vector<std::string> GetAllNamespaces() const;

  // Returns whether pages for |name_space| should be removed on cache reset.
  bool IsRemovedOnCacheReset(const std::string& name_space) const;
  const std::vector<std::string>& GetNamespacesRemovedOnCacheReset() const;

  // Returns whether pages for |name_space| are shown in Download UI.
  bool IsSupportedByDownload(const std::string& name_space) const;

  // Returns whether pages for |name_space| are explicitly offlined due to user
  // action.
  bool IsUserRequestedDownload(const std::string& name_space) const;
  const std::vector<std::string>& GetNamespacesForUserRequestedDownload() const;

  // Returns whether pages for |name_space| are shown in recent tabs UI,
  // currently only available on NTP.
  bool IsShownAsRecentlyVisitedSite(const std::string& name_space) const;

  // Returns whether pages for |name_space| should only be opened in a
  // specifically assigned tab.
  // Note: For this restriction to work offline pages saved to this namespace
  // must have the respective tab id set to their ClientId::id field.
  bool IsRestrictedToTabFromClientId(const std::string& name_space) const;

  bool IsDisabledWhenPrefetchDisabled(const std::string& name_space) const;

  // Returns whether pages for |name_space| originate from suggested URLs and
  // are downloaded on behalf of user.
  bool IsSuggested(const std::string& name_space) const;

  // Returns whether we should allow pages for |name_space| to trigger
  // downloads.
  bool ShouldAllowDownloads(const std::string& name_space) const;

 private:
  // The map from name_space to a client policy. Will be generated
  // as pre-defined values for now.
  std::map<std::string, OfflinePageClientPolicy> policies_;

  std::vector<std::string> cache_reset_namespaces_;
  std::vector<std::string> user_requested_download_namespaces_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_POLICY_CONTROLLER_H_
