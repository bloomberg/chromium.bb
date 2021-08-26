// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "components/policy/proto/device_management_backend.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace net {
namespace test_server {
class HttpResponse;
struct HttpRequest;
}  // namespace test_server
}  // namespace net

namespace policy {

class ClientStorage;
class PolicyStorage;

extern const char kFakeDeviceToken[];
extern const char kInvalidEnrollmentToken[];

// Runs a fake implementation of the cloud policy server on the local machine.
class EmbeddedPolicyTestServer {
 public:
  class RequestHandler {
   public:
    RequestHandler(ClientStorage* client_storage,
                   PolicyStorage* policy_storage);
    virtual ~RequestHandler();

    // Returns the value associated with the "request_type" query param handled
    // by this request handler.
    virtual std::string RequestType() = 0;

    // Returns a response if this request can be handled by this handler, or
    // nullptr otherwise.
    virtual std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
        const net::test_server::HttpRequest& request) = 0;

    const ClientStorage* client_storage() const { return client_storage_; }
    ClientStorage* client_storage() { return client_storage_; }

    const PolicyStorage* policy_storage() const { return policy_storage_; }
    PolicyStorage* policy_storage() { return policy_storage_; }

   private:
    ClientStorage* client_storage_;
    PolicyStorage* policy_storage_;
  };

  EmbeddedPolicyTestServer();
  EmbeddedPolicyTestServer(const EmbeddedPolicyTestServer&) = delete;
  EmbeddedPolicyTestServer& operator=(const EmbeddedPolicyTestServer&) = delete;
  virtual ~EmbeddedPolicyTestServer();

  // Initializes and waits until the server is ready to accept requests.
  bool Start();

  ClientStorage* client_storage() const { return client_storage_.get(); }

  PolicyStorage* policy_storage() const { return policy_storage_.get(); }

  // Returns the service URL.
  GURL GetServiceURL() const;

  // Public so it can be used by tests.
  void RegisterHandler(std::unique_ptr<EmbeddedPolicyTestServer::RequestHandler>
                           request_handler);

 private:
  // Default request handler.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  net::test_server::EmbeddedTestServer http_server_;
  std::map<std::string, std::unique_ptr<RequestHandler>> request_handlers_;
  std::unique_ptr<ClientStorage> client_storage_;
  std::unique_ptr<PolicyStorage> policy_storage_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_H_
