// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_FAKE_ARC_TOS_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_FAKE_ARC_TOS_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace net {
namespace test_server {
struct HttpRequest;
class EmbeddedTestServer;
class HttpResponse;
}  // namespace test_server
}  // namespace net

namespace ash {

using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

// Mixin that serves fake ARC terms of service for OOBE.
class FakeArcTosMixin : public InProcessBrowserTestMixin {
 public:
  FakeArcTosMixin(InProcessBrowserTestMixinHost* host,
                  net::EmbeddedTestServer* test_server);

  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  ~FakeArcTosMixin() override;

  const std::string& GetArcTosContent() { return kArcTosContent; }
  const std::string& GetPrivacyPolicyContent() { return kPrivacyPolicyContent; }
  void set_serve_tos_with_privacy_policy_footer(bool serve_with_footer) {
    serve_tos_with_privacy_policy_footer_ = serve_with_footer;
  }

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);
  std::string TestServerBaseUrl();
  const std::string kArcTosContent = "ARC TOS for test.";
  const std::string kPrivacyPolicyContent = "ARC Privacy Policy for test.";
  net::EmbeddedTestServer* test_server_;
  bool serve_tos_with_privacy_policy_footer_ = true;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_FAKE_ARC_TOS_MIXIN_H_
