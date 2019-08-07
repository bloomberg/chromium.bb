// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/policy_header_service.h"

#include <utility>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace {
const char kUserDMTokenKey[] = "user_dmtoken";
const char kUserPolicyTokenKey[] = "user_policy_token";
const char kVerificationKeyHashKey[] = "verification_key_id";
}

namespace policy {

PolicyHeaderService::PolicyHeaderService(
    const std::string& server_url,
    const std::string& verification_key_hash,
    CloudPolicyStore* user_policy_store)
    : server_url_(server_url),
      verification_key_hash_(verification_key_hash),
      user_policy_store_(user_policy_store) {
  user_policy_store_->AddObserver(this);
  policy_header_ = CreateHeaderValue();
}

PolicyHeaderService::~PolicyHeaderService() {
  user_policy_store_->RemoveObserver(this);
}

void PolicyHeaderService::AddPolicyHeaders(const GURL& url,
                                           AddHeaderFunction add_header) const {
  if (policy_header_.empty())
    return;

  if (url.spec().compare(0, server_url_.size(), server_url_) != 0)
    return;

  std::move(add_header).Run(kChromePolicyHeader, policy_header_);
}

std::string PolicyHeaderService::CreateHeaderValue() {
  // If we have no user policy or no token, return an empty header.
  if (!user_policy_store_->has_policy() ||
      !user_policy_store_->policy()->has_request_token()) {
    return "";
  }

  // Generate a Base64-encoded header of the form:
  // {
  //   user_dmtoken: <dm_token>
  //   user_policy_token: <policy_token>
  //   verification_key_hash: <key_hash>
  // }
  std::string user_dm_token = user_policy_store_->policy()->request_token();
  base::DictionaryValue value;
  value.SetString(kUserDMTokenKey, user_dm_token);
  if (user_policy_store_->policy()->has_policy_token()) {
    value.SetString(kUserPolicyTokenKey,
                    user_policy_store_->policy()->policy_token());
  }
  if (!verification_key_hash_.empty())
    value.SetString(kVerificationKeyHashKey, verification_key_hash_);

  // TODO(atwilson): add user_policy_token once the server starts sending it
  // down (http://crbug.com/326799).
  std::string json;
  base::JSONWriter::Write(value, &json);
  DCHECK(!json.empty());

  // Base64-encode the result so we can include it in a header.
  std::string encoded;
  base::Base64Encode(json, &encoded);
  return encoded;
}

void PolicyHeaderService::OnStoreLoaded(CloudPolicyStore* store) {
  policy_header_ = CreateHeaderValue();
}

void PolicyHeaderService::OnStoreError(CloudPolicyStore* store) {
  // Do nothing on errors.
}

void PolicyHeaderService::SetHeaderForTest(const std::string& new_header) {
  policy_header_ = new_header;
}

void PolicyHeaderService::SetServerURLForTest(const std::string& server_url) {
  server_url_ = server_url;
}

}  // namespace policy
