// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_policy.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/signature_provider.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

RequestHandlerForPolicy::RequestHandlerForPolicy(ClientStorage* client_storage,
                                                 PolicyStorage* policy_storage)
    : EmbeddedPolicyTestServer::RequestHandler(client_storage, policy_storage) {
}

RequestHandlerForPolicy::~RequestHandlerForPolicy() = default;

std::string RequestHandlerForPolicy::RequestType() {
  return dm_protocol::kValueRequestPolicy;
}

std::unique_ptr<HttpResponse> RequestHandlerForPolicy::HandleRequest(
    const HttpRequest& request) {
  const std::set<std::string> kCloudPolicyTypes{
      dm_protocol::kChromeDevicePolicyType,
      dm_protocol::kChromeExtensionPolicyType,
      dm_protocol::kChromeMachineLevelUserCloudPolicyType,
      dm_protocol::kChromeMachineLevelUserCloudPolicyAndroidType,
      dm_protocol::kChromeMachineLevelExtensionCloudPolicyType,
      dm_protocol::kChromePublicAccountPolicyType,
      dm_protocol::kChromeSigninExtensionPolicyType,
      dm_protocol::kChromeUserPolicyType,
  };

  std::string request_device_token;
  if (!GetDeviceTokenFromRequest(request, &request_device_token))
    return CreateHttpResponse(net::HTTP_UNAUTHORIZED, "Invalid device token.");

  const ClientStorage::ClientInfo* client_info =
      client_storage()->GetClientOrNull(
          KeyValueFromUrl(request.GetURL(), dm_protocol::kParamDeviceID));
  if (!client_info || client_info->device_token != request_device_token)
    return CreateHttpResponse(net::HTTP_GONE, "Invalid device token.");

  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);

  em::DeviceManagementResponse device_management_response;
  for (const auto& fetch_request :
       device_management_request.policy_request().requests()) {
    const std::string& policy_type = fetch_request.policy_type();
    // TODO(crbug.com/1221328): Add other policy types as needed.
    if (kCloudPolicyTypes.find(policy_type) == kCloudPolicyTypes.end()) {
      return CreateHttpResponse(
          net::HTTP_BAD_REQUEST,
          base::StringPrintf("Invalid policy_type: %s", policy_type.c_str()));
    }

    std::string error_msg;
    if (!ProcessCloudPolicy(
            fetch_request, *client_info,
            device_management_response.mutable_policy_response()
                ->add_responses(),
            &error_msg)) {
      return CreateHttpResponse(net::HTTP_BAD_REQUEST, error_msg);
    }
  }

  return CreateHttpResponse(net::HTTP_OK,
                            device_management_response.SerializeAsString());
}

bool RequestHandlerForPolicy::ProcessCloudPolicy(
    const em::PolicyFetchRequest& fetch_request,
    const ClientStorage::ClientInfo& client_info,
    em::PolicyFetchResponse* fetch_response,
    std::string* error_msg) {
  const std::string& policy_type = fetch_request.policy_type();
  if (client_info.allowed_policy_types.find(policy_type) ==
      client_info.allowed_policy_types.end()) {
    error_msg->assign("Policy type not allowed for token: ")
        .append(policy_type);
    return false;
  }

  // Determine the current key on the client.
  const SignatureProvider::SigningKey* client_key = nullptr;
  if (fetch_request.has_public_key_version()) {
    int public_key_version = fetch_request.public_key_version();
    client_key = policy_storage()->signature_provider()->GetKeyByVersion(
        public_key_version);
    if (!client_key) {
      error_msg->assign(base::StringPrintf("Invalid public key version: %d",
                                           public_key_version));
      return false;
    }
  }

  // Choose the key for signing the policy.
  const SignatureProvider::SigningKey* signing_key =
      policy_storage()->signature_provider()->GetCurrentKey();
  if (!signing_key) {
    error_msg->assign(base::StringPrintf(
        "Can't find signin key for version: %d",
        policy_storage()->signature_provider()->current_key_version()));
    return false;
  }

  em::PolicyData policy_data;
  policy_data.set_policy_type(policy_type);
  policy_data.set_timestamp(policy_storage()->timestamp().is_null()
                                ? base::Time::Now().ToJavaTime()
                                : policy_storage()->timestamp().ToJavaTime());
  policy_data.set_request_token(client_info.device_token);
  policy_data.set_policy_value(policy_storage()->GetPolicyPayload(policy_type));
  policy_data.set_machine_name(client_info.machine_name);
  policy_data.set_service_account_identity(
      policy_storage()->service_account_identity().empty()
          ? "policy_testserver.py-service_account_identity@gmail.com"
          : policy_storage()->service_account_identity());
  policy_data.set_device_id(client_info.device_id);
  policy_data.set_username(
      client_info.username.value_or(policy_storage()->policy_user().empty()
                                        ? "username@example.com"
                                        : policy_storage()->policy_user()));
  policy_data.set_policy_invalidation_topic(
      policy_storage()->policy_invalidation_topic());

  if (fetch_request.signature_type() != em::PolicyFetchRequest::NONE) {
    policy_data.set_public_key_version(
        policy_storage()->signature_provider()->current_key_version());
  }

  policy_data.SerializeToString(fetch_response->mutable_policy_data());

  if (fetch_request.signature_type() == em::PolicyFetchRequest::SHA1_RSA) {
    // Sign the serialized policy data.
    if (!signing_key->Sign(fetch_response->policy_data(),
                           fetch_response->mutable_policy_data_signature())) {
      error_msg->assign("Error signing policy_data");
      return false;
    }

    if (fetch_request.public_key_version() !=
        policy_storage()->signature_provider()->current_key_version()) {
      fetch_response->set_new_public_key(signing_key->public_key());
    }

    // Set the verification signature appropriate for the policy domain.
    // TODO(http://crbug.com/328038): Use the enrollment domain for public
    // accounts when we add key validation for ChromeOS.
    std::string domain = gaia::ExtractDomainName(policy_data.username());
    if (!signing_key->GetSignatureForDomain(
            domain,
            fetch_response
                ->mutable_new_public_key_verification_signature_deprecated())) {
      error_msg->assign(
          base::StringPrintf("No signature for domain: %s", domain.c_str()));
      return false;
    }

    if (client_key &&
        !client_key->Sign(fetch_response->new_public_key(),
                          fetch_response->mutable_new_public_key_signature())) {
      error_msg->assign("Error signing new_public_key");
      return false;
    }
  }

  return true;
}

}  // namespace policy
