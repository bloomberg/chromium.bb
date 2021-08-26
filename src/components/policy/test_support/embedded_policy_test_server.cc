// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/embedded_policy_test_server.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/request_handler_for_api_authorization.h"
#include "components/policy/test_support/request_handler_for_chrome_desktop_report.h"
#include "components/policy/test_support/request_handler_for_policy.h"
#include "components/policy/test_support/request_handler_for_register_browser.h"
#include "components/policy/test_support/request_handler_for_register_device_and_user.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::EmbeddedTestServer;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

namespace policy {

namespace {

std::unique_ptr<HttpResponse> LogStatusAndReturn(
    GURL url,
    std::unique_ptr<HttpResponse> response) {
  if (!response)
    return nullptr;

  BasicHttpResponse* basic_response =
      static_cast<BasicHttpResponse*>(response.get());
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
  RegisterHandler(std::make_unique<RequestHandlerForChromeDesktopReport>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForPolicy>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForRegisterBrowser>(
      client_storage_.get(), policy_storage_.get()));
  RegisterHandler(std::make_unique<RequestHandlerForRegisterDeviceAndUser>(
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

std::unique_ptr<HttpResponse> EmbeddedPolicyTestServer::HandleRequest(
    const HttpRequest& request) {
  GURL url = request.GetURL();
  DLOG(INFO) << "Request URL: " << url;

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

}  // namespace policy
