// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/embedded_policy_test_server.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#if !BUILDFLAG(IS_ANDROID)
#include "components/policy/proto/chrome_extension_policy.pb.h"
#endif  // !BUILDFLAG(IS_ANDROID)
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/failing_request_handler.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/request_handler_for_api_authorization.h"
#include "components/policy/test_support/request_handler_for_auto_enrollment.h"
#include "components/policy/test_support/request_handler_for_check_android_management.h"
#include "components/policy/test_support/request_handler_for_chrome_desktop_report.h"
#include "components/policy/test_support/request_handler_for_device_attribute_update.h"
#include "components/policy/test_support/request_handler_for_device_attribute_update_permission.h"
#include "components/policy/test_support/request_handler_for_device_initial_enrollment_state.h"
#include "components/policy/test_support/request_handler_for_device_state_retrieval.h"
#include "components/policy/test_support/request_handler_for_policy.h"
#include "components/policy/test_support/request_handler_for_psm_auto_enrollment.h"
#include "components/policy/test_support/request_handler_for_register_browser.h"
#include "components/policy/test_support/request_handler_for_register_cert_based.h"
#include "components/policy/test_support/request_handler_for_register_device_and_user.h"
#include "components/policy/test_support/request_handler_for_remote_commands.h"
#include "components/policy/test_support/request_handler_for_status_upload.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "crypto/sha2.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using ::net::test_server::EmbeddedTestServer;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

namespace policy {

namespace {

const char kExternalPolicyDataPath[] = "/externalpolicydata";
const char kExternalPolicyTypeParam[] = "policy_type";
const char kExternalEntityIdParam[] = "entity_id";

std::unique_ptr<HttpResponse> LogStatusAndReturn(
    GURL url,
    std::unique_ptr<HttpResponse> response) {
  if (!response)
    return nullptr;

  CustomHttpResponse* basic_response =
      static_cast<CustomHttpResponse*>(response.get());
  if (basic_response->code() == net::HTTP_OK) {
    DLOG(INFO) << "Request succeeded: " << url;
  } else {
    DLOG(INFO) << "Request failed with error code " << basic_response->code()
               << " (" << basic_response->content() << "): " << url;
  }
  return response;
}

}  // namespace

const char kFakeDeviceToken[] = "fake_device_management_token";
const char kInvalidEnrollmentToken[] = "invalid_enrollment_token";

EmbeddedPolicyTestServer::RequestHandler::RequestHandler(
    ClientStorage* client_storage,
    PolicyStorage* policy_storage)
    : client_storage_(client_storage), policy_storage_(policy_storage) {}

EmbeddedPolicyTestServer::RequestHandler::~RequestHandler() = default;

EmbeddedPolicyTestServer::EmbeddedPolicyTestServer()
    : http_server_(EmbeddedTestServer::TYPE_HTTP),
      client_storage_(std::make_unique<ClientStorage>()),
      policy_storage_(std::make_unique<PolicyStorage>()) {
  RegisterHandler(std::make_unique<RequestHandlerForApiAuthorization>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForAutoEnrollment>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForCheckAndroidManagement>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForChromeDesktopReport>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForDeviceAttributeUpdate>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(
      std::make_unique<RequestHandlerForDeviceAttributeUpdatePermission>(
          client_storage_.get(), policy_storage_.get()));
  RegisterHandler(
      std::make_unique<RequestHandlerForDeviceInitialEnrollmentState>(
          client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForDeviceStateRetrieval>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForPolicy>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForPsmAutoEnrollment>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForRegisterBrowser>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForRegisterCertBased>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForRegisterDeviceAndUser>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForRemoteCommands>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForStatusUpload>(
      client_storage_.get(), policy_storage_.get()));

  http_server_.RegisterDefaultHandler(base::BindRepeating(
      &EmbeddedPolicyTestServer::HandleRequest, base::Unretained(this)));
}

EmbeddedPolicyTestServer::~EmbeddedPolicyTestServer() = default;

bool EmbeddedPolicyTestServer::Start() {
  return http_server_.Start();
}

GURL EmbeddedPolicyTestServer::GetServiceURL() const {
  return http_server_.GetURL("/device_management");
}

void EmbeddedPolicyTestServer::RegisterHandler(
    std::unique_ptr<EmbeddedPolicyTestServer::RequestHandler> request_handler) {
  request_handlers_[request_handler->RequestType()] =
      std::move(request_handler);
}

void EmbeddedPolicyTestServer::ConfigureRequestError(
    const std::string& request_type,
    net::HttpStatusCode error_code) {
  RegisterHandler(std::make_unique<FailingRequestHandler>(
      client_storage_.get(), policy_storage_.get(), request_type, error_code));
}

#if !BUILDFLAG(IS_ANDROID)
void EmbeddedPolicyTestServer::UpdateExternalPolicy(
    const std::string& type,
    const std::string& entity_id,
    const std::string& raw_policy) {
  // Register raw policy to be served by external endpoint.
  policy_storage()->SetExternalPolicyPayload(type, entity_id, raw_policy);

  // Register proto policy with details on how to fetch the raw policy.
  GURL external_policy_url = http_server_.GetURL(kExternalPolicyDataPath);
  external_policy_url = net::AppendOrReplaceQueryParameter(
      external_policy_url, kExternalPolicyTypeParam, type);
  external_policy_url = net::AppendOrReplaceQueryParameter(
      external_policy_url, kExternalEntityIdParam, entity_id);

  enterprise_management::ExternalPolicyData external_policy_data;
  external_policy_data.set_download_url(external_policy_url.spec());
  external_policy_data.set_secure_hash(crypto::SHA256HashString(raw_policy));
  policy_storage()->SetPolicyPayload(type, entity_id,
                                     external_policy_data.SerializeAsString());
}
#endif  // !BUILDFLAG(IS_ANDROID)

std::unique_ptr<HttpResponse> EmbeddedPolicyTestServer::HandleRequest(
    const HttpRequest& request) {
  GURL url = request.GetURL();
  DLOG(INFO) << "Request URL: " << url;

  if (url.path() == kExternalPolicyDataPath)
    return HandleExternalPolicyDataRequest(url);

  std::string request_type = KeyValueFromUrl(url, dm_protocol::kParamRequest);
  auto it = request_handlers_.find(request_type);
  if (it == request_handlers_.end()) {
    LOG(ERROR) << "No request handler for: " << url;
    return nullptr;
  }

  if (!MeetsServerSideRequirements(url)) {
    return LogStatusAndReturn(
        url, CreateHttpResponse(
                 net::HTTP_BAD_REQUEST,
                 "URL must define device type, app type, and device id."));
  }

  return LogStatusAndReturn(url, it->second->HandleRequest(request));
}

std::unique_ptr<HttpResponse>
EmbeddedPolicyTestServer::HandleExternalPolicyDataRequest(const GURL& url) {
  DCHECK_EQ(url.path(), kExternalPolicyDataPath);
  std::string policy_type = KeyValueFromUrl(url, kExternalPolicyTypeParam);
  std::string entity_id = KeyValueFromUrl(url, kExternalEntityIdParam);
  std::string policy_payload =
      policy_storage_->GetExternalPolicyPayload(policy_type, entity_id);
  std::unique_ptr<HttpResponse> response;
  if (policy_payload.empty()) {
    response = CreateHttpResponse(
        net::HTTP_NOT_FOUND,
        "No external policy payload for specified policy type and entity ID");
  } else {
    response = CreateHttpResponse(net::HTTP_OK, policy_payload);
  }
  return LogStatusAndReturn(url, std::move(response));
}

}  // namespace policy
