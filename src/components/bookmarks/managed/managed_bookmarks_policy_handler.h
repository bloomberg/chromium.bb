// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_POLICY_HANDLER_H_
#define COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_POLICY_HANDLER_H_

#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

namespace bookmarks {

// Handles the ManagedBookmarks policy.
class ManagedBookmarksPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit ManagedBookmarksPolicyHandler(policy::Schema chrome_schema);

  ManagedBookmarksPolicyHandler(const ManagedBookmarksPolicyHandler&) = delete;
  ManagedBookmarksPolicyHandler& operator=(
      const ManagedBookmarksPolicyHandler&) = delete;

  ~ManagedBookmarksPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  std::string GetFolderName(const base::Value& list);
  base::Value::ListStorage FilterBookmarks(base::Value::ListStorage bookmarks);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_POLICY_HANDLER_H_
