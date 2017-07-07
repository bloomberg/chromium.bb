/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebAssociatedURLLoader.h"

#include <memory>

#include "build/build_config.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/WebLocalFrameBase.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/web/WebAssociatedURLLoaderClient.h"
#include "public/web/WebAssociatedURLLoaderOptions.h"
#include "public/web/WebFrame.h"
#include "public/web/WebView.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::URLTestHelpers::ToKURL;
using blink::testing::RunPendingTasks;

namespace blink {

class WebAssociatedURLLoaderTest : public ::testing::Test,
                                   public WebAssociatedURLLoaderClient {
 public:
  WebAssociatedURLLoaderTest()
      : will_follow_redirect_(false),
        did_send_data_(false),
        did_receive_response_(false),
        did_receive_data_(false),
        did_receive_cached_metadata_(false),
        did_finish_loading_(false),
        did_fail_(false) {
    // Reuse one of the test files from WebFrameTest.
    frame_file_path_ = testing::CoreTestDataPath("iframes_test.html");
  }

  KURL RegisterMockedUrl(const std::string& url_root,
                         const WTF::String& filename) {
    WebURLResponse response;
    response.SetMIMEType("text/html");
    KURL url = ToKURL(url_root + filename.Utf8().data());
    Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
        url, response, testing::CoreTestDataPath(filename.Utf8().data()));
    return url;
  }

  void SetUp() override {
    helper_.Initialize();

    std::string url_root = "http://www.test.com/";
    KURL url = RegisterMockedUrl(url_root, "iframes_test.html");
    const char* iframe_support_files[] = {
        "invisible_iframe.html", "visible_iframe.html",
        "zero_sized_iframe.html",
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(iframe_support_files); ++i) {
      RegisterMockedUrl(url_root, iframe_support_files[i]);
    }

    FrameTestHelpers::LoadFrame(MainFrame(), url.GetString().Utf8().data());

    Platform::Current()->GetURLLoaderMockFactory()->UnregisterURL(url);
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void ServeRequests() {
    Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  }

  std::unique_ptr<WebAssociatedURLLoader> CreateAssociatedURLLoader(
      const WebAssociatedURLLoaderOptions options =
          WebAssociatedURLLoaderOptions()) {
    return WTF::WrapUnique(MainFrame()->CreateAssociatedURLLoader(options));
  }

  // WebAssociatedURLLoaderClient implementation.
  bool WillFollowRedirect(const WebURLRequest& new_request,
                          const WebURLResponse& redirect_response) override {
    will_follow_redirect_ = true;
    EXPECT_EQ(expected_new_request_.Url(), new_request.Url());
    // Check that CORS simple headers are transferred to the new request.
    EXPECT_EQ(expected_new_request_.HttpHeaderField("accept"),
              new_request.HttpHeaderField("accept"));
    EXPECT_EQ(expected_redirect_response_.Url(), redirect_response.Url());
    EXPECT_EQ(expected_redirect_response_.HttpStatusCode(),
              redirect_response.HttpStatusCode());
    EXPECT_EQ(expected_redirect_response_.MimeType(),
              redirect_response.MimeType());
    return true;
  }

  void DidSendData(unsigned long long bytes_sent,
                   unsigned long long total_bytes_to_be_sent) override {
    did_send_data_ = true;
  }

  void DidReceiveResponse(const WebURLResponse& response) override {
    did_receive_response_ = true;
    actual_response_ = WebURLResponse(response);
    EXPECT_EQ(expected_response_.Url(), response.Url());
    EXPECT_EQ(expected_response_.HttpStatusCode(), response.HttpStatusCode());
  }

  void DidDownloadData(int data_length) override { did_download_data_ = true; }

  void DidReceiveData(const char* data, int data_length) override {
    did_receive_data_ = true;
    EXPECT_TRUE(data);
    EXPECT_GT(data_length, 0);
  }

  void DidReceiveCachedMetadata(const char* data, int data_length) override {
    did_receive_cached_metadata_ = true;
  }

  void DidFinishLoading(double finish_time) override {
    did_finish_loading_ = true;
  }

  void DidFail(const WebURLError& error) override { did_fail_ = true; }

  void CheckMethodFails(const char* unsafe_method) {
    WebURLRequest request(ToKURL("http://www.test.com/success.html"));
    request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeSameOrigin);
    request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
    request.SetHTTPMethod(WebString::FromUTF8(unsafe_method));
    WebAssociatedURLLoaderOptions options;
    options.untrusted_http = true;
    CheckFails(request, options);
  }

  void CheckHeaderFails(const char* header_field) {
    CheckHeaderFails(header_field, "foo");
  }

  void CheckHeaderFails(const char* header_field, const char* header_value) {
    WebURLRequest request(ToKURL("http://www.test.com/success.html"));
    request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeSameOrigin);
    request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
    if (EqualIgnoringASCIICase(WebString::FromUTF8(header_field), "referer")) {
      request.SetHTTPReferrer(WebString::FromUTF8(header_value),
                              kWebReferrerPolicyDefault);
    } else {
      request.SetHTTPHeaderField(WebString::FromUTF8(header_field),
                                 WebString::FromUTF8(header_value));
    }

    WebAssociatedURLLoaderOptions options;
    options.untrusted_http = true;
    CheckFails(request, options);
  }

  void CheckFails(
      const WebURLRequest& request,
      WebAssociatedURLLoaderOptions options = WebAssociatedURLLoaderOptions()) {
    expected_loader_ = CreateAssociatedURLLoader(options);
    EXPECT_TRUE(expected_loader_);
    did_fail_ = false;
    expected_loader_->LoadAsynchronously(request, this);
    // Failure should not be reported synchronously.
    EXPECT_FALSE(did_fail_);
    // Allow the loader to return the error.
    RunPendingTasks();
    EXPECT_TRUE(did_fail_);
    EXPECT_FALSE(did_receive_response_);
  }

  bool CheckAccessControlHeaders(const char* header_name, bool exposed) {
    std::string id("http://www.other.com/CheckAccessControlExposeHeaders_");
    id.append(header_name);
    if (exposed)
      id.append("-Exposed");
    id.append(".html");

    KURL url = ToKURL(id);
    WebURLRequest request(url);
    request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeCORS);
    request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

    WebString header_name_string(WebString::FromUTF8(header_name));
    expected_response_ = WebURLResponse();
    expected_response_.SetMIMEType("text/html");
    expected_response_.SetHTTPStatusCode(200);
    expected_response_.AddHTTPHeaderField("Access-Control-Allow-Origin", "*");
    if (exposed) {
      expected_response_.AddHTTPHeaderField("access-control-expose-headers",
                                            header_name_string);
    }
    expected_response_.AddHTTPHeaderField(header_name_string, "foo");
    Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
        url, expected_response_, frame_file_path_);

    WebAssociatedURLLoaderOptions options;
    expected_loader_ = CreateAssociatedURLLoader(options);
    EXPECT_TRUE(expected_loader_);
    expected_loader_->LoadAsynchronously(request, this);
    ServeRequests();
    EXPECT_TRUE(did_receive_response_);
    EXPECT_TRUE(did_receive_data_);
    EXPECT_TRUE(did_finish_loading_);

    return !actual_response_.HttpHeaderField(header_name_string).IsEmpty();
  }

  WebLocalFrameBase* MainFrame() const {
    return helper_.WebView()->MainFrameImpl();
  }

 protected:
  String frame_file_path_;
  FrameTestHelpers::WebViewHelper helper_;

  std::unique_ptr<WebAssociatedURLLoader> expected_loader_;
  WebURLResponse actual_response_;
  WebURLResponse expected_response_;
  WebURLRequest expected_new_request_;
  WebURLResponse expected_redirect_response_;
  bool will_follow_redirect_;
  bool did_send_data_;
  bool did_receive_response_;
  bool did_download_data_;
  bool did_receive_data_;
  bool did_receive_cached_metadata_;
  bool did_finish_loading_;
  bool did_fail_;
};

// Test a successful same-origin URL load.
TEST_F(WebAssociatedURLLoaderTest, SameOriginSuccess) {
  KURL url = ToKURL("http://www.test.com/SameOriginSuccess.html");
  WebURLRequest request(url);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeSameOrigin);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

  expected_response_ = WebURLResponse();
  expected_response_.SetMIMEType("text/html");
  expected_response_.SetHTTPStatusCode(200);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, expected_response_, frame_file_path_);

  expected_loader_ = CreateAssociatedURLLoader();
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);
}

// Test that the same-origin restriction is the default.
TEST_F(WebAssociatedURLLoaderTest, SameOriginRestriction) {
  // This is cross-origin since the frame was loaded from www.test.com.
  KURL url = ToKURL("http://www.other.com/SameOriginRestriction.html");
  WebURLRequest request(url);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeSameOrigin);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
  CheckFails(request);
}

// Test a successful cross-origin load.
TEST_F(WebAssociatedURLLoaderTest, CrossOriginSuccess) {
  // This is cross-origin since the frame was loaded from www.test.com.
  KURL url = ToKURL("http://www.other.com/CrossOriginSuccess");
  WebURLRequest request(url);
  // No-CORS requests (CrossOriginRequestPolicyAllow) aren't allowed for the
  // default context. So we set the context as Script here.
  request.SetRequestContext(WebURLRequest::kRequestContextScript);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

  expected_response_ = WebURLResponse();
  expected_response_.SetMIMEType("text/html");
  expected_response_.SetHTTPStatusCode(200);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, expected_response_, frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);
}

// Test a successful cross-origin load using CORS.
TEST_F(WebAssociatedURLLoaderTest, CrossOriginWithAccessControlSuccess) {
  // This is cross-origin since the frame was loaded from www.test.com.
  KURL url =
      ToKURL("http://www.other.com/CrossOriginWithAccessControlSuccess.html");
  WebURLRequest request(url);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeCORS);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

  expected_response_ = WebURLResponse();
  expected_response_.SetMIMEType("text/html");
  expected_response_.SetHTTPStatusCode(200);
  expected_response_.AddHTTPHeaderField("access-control-allow-origin", "*");
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, expected_response_, frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);
}

// Test an unsuccessful cross-origin load using CORS.
TEST_F(WebAssociatedURLLoaderTest, CrossOriginWithAccessControlFailure) {
  // This is cross-origin since the frame was loaded from www.test.com.
  KURL url =
      ToKURL("http://www.other.com/CrossOriginWithAccessControlFailure.html");
  // Send credentials. This will cause the CORS checks to fail, because
  // credentials can't be sent to a server which returns the header
  // "access-control-allow-origin" with "*" as its value.
  WebURLRequest request(url);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeCORS);

  expected_response_ = WebURLResponse();
  expected_response_.SetMIMEType("text/html");
  expected_response_.SetHTTPStatusCode(200);
  expected_response_.AddHTTPHeaderField("access-control-allow-origin", "*");
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, expected_response_, frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);

  // Failure should not be reported synchronously.
  EXPECT_FALSE(did_fail_);
  // The loader needs to receive the response, before doing the CORS check.
  ServeRequests();
  EXPECT_TRUE(did_fail_);
  EXPECT_FALSE(did_receive_response_);
}

// Test an unsuccessful cross-origin load using CORS.
TEST_F(WebAssociatedURLLoaderTest,
       CrossOriginWithAccessControlFailureBadStatusCode) {
  // This is cross-origin since the frame was loaded from www.test.com.
  KURL url =
      ToKURL("http://www.other.com/CrossOriginWithAccessControlFailure.html");
  WebURLRequest request(url);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeCORS);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

  expected_response_ = WebURLResponse();
  expected_response_.SetMIMEType("text/html");
  expected_response_.SetHTTPStatusCode(0);
  expected_response_.AddHTTPHeaderField("access-control-allow-origin", "*");
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, expected_response_, frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);

  // Failure should not be reported synchronously.
  EXPECT_FALSE(did_fail_);
  // The loader needs to receive the response, before doing the CORS check.
  ServeRequests();
  EXPECT_TRUE(did_fail_);
  EXPECT_FALSE(did_receive_response_);
}

// Test a same-origin URL redirect and load.
TEST_F(WebAssociatedURLLoaderTest, RedirectSuccess) {
  KURL url = ToKURL("http://www.test.com/RedirectSuccess.html");
  char redirect[] = "http://www.test.com/RedirectSuccess2.html";  // Same-origin
  KURL redirect_url = ToKURL(redirect);

  WebURLRequest request(url);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeSameOrigin);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

  expected_redirect_response_ = WebURLResponse();
  expected_redirect_response_.SetMIMEType("text/html");
  expected_redirect_response_.SetHTTPStatusCode(301);
  expected_redirect_response_.SetHTTPHeaderField("Location", redirect);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, expected_redirect_response_, frame_file_path_);

  expected_new_request_ = WebURLRequest(redirect_url);

  expected_response_ = WebURLResponse();
  expected_response_.SetMIMEType("text/html");
  expected_response_.SetHTTPStatusCode(200);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      redirect_url, expected_response_, frame_file_path_);

  expected_loader_ = CreateAssociatedURLLoader();
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  EXPECT_TRUE(will_follow_redirect_);
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);
}

// Test a cross-origin URL redirect without Access Control set.
TEST_F(WebAssociatedURLLoaderTest, RedirectCrossOriginFailure) {
  KURL url = ToKURL("http://www.test.com/RedirectCrossOriginFailure.html");
  char redirect[] =
      "http://www.other.com/RedirectCrossOriginFailure.html";  // Cross-origin
  KURL redirect_url = ToKURL(redirect);

  WebURLRequest request(url);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeSameOrigin);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

  expected_redirect_response_ = WebURLResponse();
  expected_redirect_response_.SetMIMEType("text/html");
  expected_redirect_response_.SetHTTPStatusCode(301);
  expected_redirect_response_.SetHTTPHeaderField("Location", redirect);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, expected_redirect_response_, frame_file_path_);

  expected_new_request_ = WebURLRequest(redirect_url);

  expected_response_ = WebURLResponse();
  expected_response_.SetMIMEType("text/html");
  expected_response_.SetHTTPStatusCode(200);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      redirect_url, expected_response_, frame_file_path_);

  expected_loader_ = CreateAssociatedURLLoader();
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);

  ServeRequests();
  EXPECT_FALSE(will_follow_redirect_);
  EXPECT_FALSE(did_receive_response_);
  EXPECT_FALSE(did_receive_data_);
  EXPECT_FALSE(did_finish_loading_);
}

// Test that a cross origin redirect response without CORS headers fails.
TEST_F(WebAssociatedURLLoaderTest,
       RedirectCrossOriginWithAccessControlFailure) {
  KURL url = ToKURL(
      "http://www.test.com/RedirectCrossOriginWithAccessControlFailure.html");
  char redirect[] =
      "http://www.other.com/"
      "RedirectCrossOriginWithAccessControlFailure.html";  // Cross-origin
  KURL redirect_url = ToKURL(redirect);

  WebURLRequest request(url);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeCORS);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

  expected_redirect_response_ = WebURLResponse();
  expected_redirect_response_.SetMIMEType("text/html");
  expected_redirect_response_.SetHTTPStatusCode(301);
  expected_redirect_response_.SetHTTPHeaderField("Location", redirect);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, expected_redirect_response_, frame_file_path_);

  expected_new_request_ = WebURLRequest(redirect_url);

  expected_response_ = WebURLResponse();
  expected_response_.SetMIMEType("text/html");
  expected_response_.SetHTTPStatusCode(200);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      redirect_url, expected_response_, frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);

  ServeRequests();
  // We should get a notification about access control check failure.
  EXPECT_FALSE(will_follow_redirect_);
  EXPECT_FALSE(did_receive_response_);
  EXPECT_FALSE(did_receive_data_);
  EXPECT_TRUE(did_fail_);
}

// Test that a cross origin redirect response with CORS headers that allow the
// requesting origin succeeds.
TEST_F(WebAssociatedURLLoaderTest,
       RedirectCrossOriginWithAccessControlSuccess) {
  KURL url = ToKURL(
      "http://www.test.com/RedirectCrossOriginWithAccessControlSuccess.html");
  char redirect[] =
      "http://www.other.com/"
      "RedirectCrossOriginWithAccessControlSuccess.html";  // Cross-origin
  KURL redirect_url = ToKURL(redirect);

  WebURLRequest request(url);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeCORS);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);
  // Add a CORS simple header.
  request.SetHTTPHeaderField("accept", "application/json");

  // Create a redirect response that allows the redirect to pass the access
  // control checks.
  expected_redirect_response_ = WebURLResponse();
  expected_redirect_response_.SetMIMEType("text/html");
  expected_redirect_response_.SetHTTPStatusCode(301);
  expected_redirect_response_.SetHTTPHeaderField("Location", redirect);
  expected_redirect_response_.AddHTTPHeaderField("access-control-allow-origin",
                                                 "*");
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, expected_redirect_response_, frame_file_path_);

  expected_new_request_ = WebURLRequest(redirect_url);
  expected_new_request_.SetHTTPHeaderField("accept", "application/json");

  expected_response_ = WebURLResponse();
  expected_response_.SetMIMEType("text/html");
  expected_response_.SetHTTPStatusCode(200);
  expected_response_.AddHTTPHeaderField("access-control-allow-origin", "*");
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      redirect_url, expected_response_, frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  // We should not receive a notification for the redirect.
  EXPECT_FALSE(will_follow_redirect_);
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);
}

// Test that untrusted loads can't use a forbidden method.
TEST_F(WebAssociatedURLLoaderTest, UntrustedCheckMethods) {
  // Check non-token method fails.
  CheckMethodFails("GET()");
  CheckMethodFails("POST\x0d\x0ax-csrf-token:\x20test1234");

  // Forbidden methods should fail regardless of casing.
  CheckMethodFails("CoNneCt");
  CheckMethodFails("TrAcK");
  CheckMethodFails("TrAcE");
}

// This test is flaky on Windows and Android. See <http://crbug.com/471645>.
#if defined(OS_WIN) || defined(OS_ANDROID)
#define MAYBE_UntrustedCheckHeaders DISABLED_UntrustedCheckHeaders
#else
#define MAYBE_UntrustedCheckHeaders UntrustedCheckHeaders
#endif

// Test that untrusted loads can't use a forbidden header field.
TEST_F(WebAssociatedURLLoaderTest, MAYBE_UntrustedCheckHeaders) {
  // Check non-token header fails.
  CheckHeaderFails("foo()");

  // Check forbidden headers fail.
  CheckHeaderFails("accept-charset");
  CheckHeaderFails("accept-encoding");
  CheckHeaderFails("connection");
  CheckHeaderFails("content-length");
  CheckHeaderFails("cookie");
  CheckHeaderFails("cookie2");
  CheckHeaderFails("date");
  CheckHeaderFails("dnt");
  CheckHeaderFails("expect");
  CheckHeaderFails("host");
  CheckHeaderFails("keep-alive");
  CheckHeaderFails("origin");
  CheckHeaderFails("referer", "http://example.com/");
  CheckHeaderFails("te");
  CheckHeaderFails("trailer");
  CheckHeaderFails("transfer-encoding");
  CheckHeaderFails("upgrade");
  CheckHeaderFails("user-agent");
  CheckHeaderFails("via");

  CheckHeaderFails("proxy-");
  CheckHeaderFails("proxy-foo");
  CheckHeaderFails("sec-");
  CheckHeaderFails("sec-foo");

  // Check that validation is case-insensitive.
  CheckHeaderFails("AcCePt-ChArSeT");
  CheckHeaderFails("ProXy-FoO");

  // Check invalid header values.
  CheckHeaderFails("foo", "bar\x0d\x0ax-csrf-token:\x20test1234");
}

// Test that the loader filters response headers according to the CORS standard.
TEST_F(WebAssociatedURLLoaderTest, CrossOriginHeaderWhitelisting) {
  // Test that whitelisted headers are returned without exposing them.
  EXPECT_TRUE(CheckAccessControlHeaders("cache-control", false));
  EXPECT_TRUE(CheckAccessControlHeaders("content-language", false));
  EXPECT_TRUE(CheckAccessControlHeaders("content-type", false));
  EXPECT_TRUE(CheckAccessControlHeaders("expires", false));
  EXPECT_TRUE(CheckAccessControlHeaders("last-modified", false));
  EXPECT_TRUE(CheckAccessControlHeaders("pragma", false));

  // Test that non-whitelisted headers aren't returned.
  EXPECT_FALSE(CheckAccessControlHeaders("non-whitelisted", false));

  // Test that Set-Cookie headers aren't returned.
  EXPECT_FALSE(CheckAccessControlHeaders("Set-Cookie", false));
  EXPECT_FALSE(CheckAccessControlHeaders("Set-Cookie2", false));

  // Test that exposed headers that aren't whitelisted are returned.
  EXPECT_TRUE(CheckAccessControlHeaders("non-whitelisted", true));

  // Test that Set-Cookie headers aren't returned, even if exposed.
  EXPECT_FALSE(CheckAccessControlHeaders("Set-Cookie", true));
}

// Test that the loader can allow non-whitelisted response headers for trusted
// CORS loads.
TEST_F(WebAssociatedURLLoaderTest, CrossOriginHeaderAllowResponseHeaders) {
  KURL url =
      ToKURL("http://www.other.com/CrossOriginHeaderAllowResponseHeaders.html");
  WebURLRequest request(url);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeCORS);
  request.SetFetchCredentialsMode(WebURLRequest::kFetchCredentialsModeOmit);

  WebString header_name_string(WebString::FromUTF8("non-whitelisted"));
  expected_response_ = WebURLResponse();
  expected_response_.SetMIMEType("text/html");
  expected_response_.SetHTTPStatusCode(200);
  expected_response_.AddHTTPHeaderField("Access-Control-Allow-Origin", "*");
  expected_response_.AddHTTPHeaderField(header_name_string, "foo");
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, expected_response_, frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  options.expose_all_response_headers =
      true;  // This turns off response whitelisting.
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);

  EXPECT_FALSE(actual_response_.HttpHeaderField(header_name_string).IsEmpty());
}

}  // namespace blink
