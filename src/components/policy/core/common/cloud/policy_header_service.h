// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_HEADER_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_HEADER_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/policy_export.h"
#include "url/gurl.h"

namespace policy {

// Per-profile service used to keep track of policy changes.
// TODO(atwilson): Move to components/policy once CloudPolicyStore is moved.
class POLICY_EXPORT PolicyHeaderService : public CloudPolicyStore::Observer {
 public:
  // |device_policy_store| can be null on platforms that do not support
  // device policy. Both |user_policy_store| and |device_policy_store| must
  // outlive this object.
  PolicyHeaderService(const std::string& server_url,
                      const std::string& verification_key_hash,
                      CloudPolicyStore* user_policy_store);
  ~PolicyHeaderService() override;

  // Update navigation request headers with the policy header if the URL matches
  // |server_url_|.
  // Note: To avoid depending on content/public, a callback is used here.
  // It correspond to NavigationHandle::SetRequestHeader(). It is called
  // synchronously.
  using AddHeaderFunction =
      base::OnceCallback<void(const std::string& /* header_name*/,
                              const std::string& /* header_value */)>;
  void AddPolicyHeaders(const GURL& url, AddHeaderFunction add_header) const;

  // Overridden CloudPolicyStore::Observer methods:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // Test-only routines used to inject the server URL/headers at runtime.
  void SetHeaderForTest(const std::string& new_header);
  void SetServerURLForTest(const std::string& server_url);

 private:
  // Generate a policy header based on the currently loaded policy.
  std::string CreateHeaderValue();

  // The current policy header value.
  std::string policy_header_;

  // URL of the policy server.
  std::string server_url_;

  // Identifier for the verification key this Chrome instance is using.
  std::string verification_key_hash_;

  // Weak pointer to the User-level policy store.
  CloudPolicyStore* user_policy_store_;

  DISALLOW_COPY_AND_ASSIGN(PolicyHeaderService);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_HEADER_SERVICE_H_
