// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_BROWSER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_BROWSER_H_

#include "components/policy/test_support/embedded_policy_test_server.h"

namespace policy {

// Handler for request type `register_browser`.
class RequestHandlerForRegisterBrowser
    : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  RequestHandlerForRegisterBrowser(ClientStorage* client_storage,
                                   PolicyStorage* policy_storage);
  RequestHandlerForRegisterBrowser(RequestHandlerForRegisterBrowser&& handler) =
      delete;
  RequestHandlerForRegisterBrowser& operator=(
      RequestHandlerForRegisterBrowser&& handler) = delete;
  ~RequestHandlerForRegisterBrowser() override;

  // EmbeddedPolicyTestServer::RequestHandler:
  std::string RequestType() override;
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_REQUEST_HANDLER_FOR_REGISTER_BROWSER_H_
