// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/ppapi_unittest.h"

using WebKit::WebCString;
using WebKit::WebFrame;
using WebKit::WebFrameClient;
using WebKit::WebString;
using WebKit::WebView;
using WebKit::WebURL;
using WebKit::WebURLRequest;

namespace {

bool IsExpected(const WebCString& web_string, const char* expected) {
  const char* result = web_string.data();
  return strcmp(result, expected) == 0;
}

bool IsExpected(const WebString& web_string, const char* expected) {
  return IsExpected(web_string.utf8(), expected);
}

bool IsNullOrEmpty(const WebString& web_string) {
  return web_string.isNull() || web_string.isEmpty();
}

// The base class destructor is protected, so derive.
class TestWebFrameClient : public WebFrameClient {
};

}  // namespace

namespace webkit {
namespace ppapi {

class URLRequestInfoTest : public PpapiUnittest {
 public:
  URLRequestInfoTest() {
  }

  virtual void SetUp() {
    PpapiUnittest::SetUp();

    // Must be after our base class's SetUp for the instance to be valid.
    info_ = new PPB_URLRequestInfo_Impl(instance()->pp_instance());
  }

  static void SetUpTestCase() {
    web_view_ = WebView::create(NULL);
    web_view_->initializeMainFrame(&web_frame_client_);
    WebURL web_url(GURL(""));
    WebURLRequest url_request;
    url_request.initialize();
    url_request.setURL(web_url);
    frame_ = web_view_->mainFrame();
    frame_->loadRequest(url_request);
  }

  static void TearDownTestCase() {
    web_view_->close();
  }

  bool GetDownloadToFile() {
    WebURLRequest web_request = info_->ToWebURLRequest(frame_);
    return web_request.downloadToFile();
  }

  WebCString GetURL() {
    WebURLRequest web_request = info_->ToWebURLRequest(frame_);
    return web_request.url().spec();
  }

  WebString GetMethod() {
    WebURLRequest web_request = info_->ToWebURLRequest(frame_);
    return web_request.httpMethod();
  }

  WebString GetHeaderValue(const char* field) {
    WebURLRequest web_request = info_->ToWebURLRequest(frame_);
    return web_request.httpHeaderField(WebString::fromUTF8(field));
  }

  scoped_refptr<PPB_URLRequestInfo_Impl> info_;

  static TestWebFrameClient web_frame_client_;
  static WebView* web_view_;
  static WebFrame* frame_;
};

TestWebFrameClient URLRequestInfoTest::web_frame_client_;
WebView* URLRequestInfoTest::web_view_;
WebFrame* URLRequestInfoTest::frame_;

TEST_F(URLRequestInfoTest, GetInterface) {
  const PPB_URLRequestInfo* interface =
      ::ppapi::thunk::GetPPB_URLRequestInfo_Thunk();
  ASSERT_TRUE(interface);
  ASSERT_TRUE(interface->Create);
  ASSERT_TRUE(interface->IsURLRequestInfo);
  ASSERT_TRUE(interface->SetProperty);
  ASSERT_TRUE(interface->AppendDataToBody);
  ASSERT_TRUE(interface->AppendFileToBody);
  ASSERT_TRUE(interface->Create);
  ASSERT_TRUE(interface->Create);
}

TEST_F(URLRequestInfoTest, AsURLRequestInfo) {
  ASSERT_EQ(info_, info_->AsPPB_URLRequestInfo_API());
}

TEST_F(URLRequestInfoTest, StreamToFile) {
  info_->SetStringProperty(PP_URLREQUESTPROPERTY_URL, "http://www.google.com");

  ASSERT_FALSE(GetDownloadToFile());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_STREAMTOFILE, true));
  ASSERT_TRUE(GetDownloadToFile());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_STREAMTOFILE, false));
  ASSERT_FALSE(GetDownloadToFile());
}

TEST_F(URLRequestInfoTest, FollowRedirects) {
  ASSERT_TRUE(info_->follow_redirects());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, false));
  ASSERT_FALSE(info_->follow_redirects());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, true));
  ASSERT_TRUE(info_->follow_redirects());
}

TEST_F(URLRequestInfoTest, RecordDownloadProgress) {
  ASSERT_FALSE(info_->record_download_progress());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS, true));
  ASSERT_TRUE(info_->record_download_progress());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS, false));
  ASSERT_FALSE(info_->record_download_progress());
}

TEST_F(URLRequestInfoTest, RecordUploadProgress) {
  ASSERT_FALSE(info_->record_upload_progress());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS, true));
  ASSERT_TRUE(info_->record_upload_progress());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS, false));
  ASSERT_FALSE(info_->record_upload_progress());
}

TEST_F(URLRequestInfoTest, AllowCrossOriginRequests) {
  ASSERT_FALSE(info_->allow_cross_origin_requests());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS, true));
  ASSERT_TRUE(info_->allow_cross_origin_requests());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS, false));
  ASSERT_FALSE(info_->allow_cross_origin_requests());
}

TEST_F(URLRequestInfoTest, AllowCredentials) {
  ASSERT_FALSE(info_->allow_credentials());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS, true));
  ASSERT_TRUE(info_->allow_credentials());

  ASSERT_TRUE(info_->SetBooleanProperty(
      PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS, false));
  ASSERT_FALSE(info_->allow_credentials());
}

TEST_F(URLRequestInfoTest, SetURL) {
  // Test default URL is "about:blank".
  ASSERT_TRUE(IsExpected(GetURL(), "about:blank"));

  const char* url = "http://www.google.com/";
  ASSERT_TRUE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_URL, url));
  ASSERT_TRUE(IsExpected(GetURL(), url));
}

TEST_F(URLRequestInfoTest, JavascriptURL) {
  const char* url = "javascript:foo = bar";
  ASSERT_FALSE(info_->RequiresUniversalAccess());
  info_->SetStringProperty(PP_URLREQUESTPROPERTY_URL, url);
  ASSERT_TRUE(info_->RequiresUniversalAccess());
}

TEST_F(URLRequestInfoTest, SetMethod) {
  // Test default method is "GET".
  ASSERT_TRUE(IsExpected(GetMethod(), "GET"));
  ASSERT_TRUE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "POST"));
  ASSERT_TRUE(IsExpected(GetMethod(), "POST"));

  // Test that method names are converted to upper case.
  ASSERT_TRUE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "get"));
  ASSERT_TRUE(IsExpected(GetMethod(), "GET"));
  ASSERT_TRUE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "post"));
  ASSERT_TRUE(IsExpected(GetMethod(), "POST"));
}

TEST_F(URLRequestInfoTest, SetInvalidMethod) {
  ASSERT_FALSE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "CONNECT"));
  ASSERT_FALSE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "connect"));
  ASSERT_FALSE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "TRACE"));
  ASSERT_FALSE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "trace"));
  ASSERT_FALSE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "TRACK"));
  ASSERT_FALSE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "track"));

  ASSERT_FALSE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "POST\x0d\x0ax-csrf-token:\x20test1234"));
}

TEST_F(URLRequestInfoTest, SetValidHeaders) {
  // Test default header field.
  ASSERT_TRUE(IsExpected(
      GetHeaderValue("foo"), ""));
  // Test that we can set a header field.
  ASSERT_TRUE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_HEADERS, "foo: bar"));
  ASSERT_TRUE(IsExpected(
      GetHeaderValue("foo"), "bar"));
  // Test that we can set multiple header fields using \n delimiter.
  ASSERT_TRUE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_HEADERS, "foo: bar\nbar: baz"));
  ASSERT_TRUE(IsExpected(
      GetHeaderValue("foo"), "bar"));
  ASSERT_TRUE(IsExpected(
      GetHeaderValue("bar"), "baz"));
}

TEST_F(URLRequestInfoTest, SetInvalidHeaders) {
  const char* const kForbiddenHeaderFields[] = {
    "accept-charset",
    "accept-encoding",
    "connection",
    "content-length",
    "cookie",
    "cookie2",
    "content-transfer-encoding",
    "date",
    "expect",
    "host",
    "keep-alive",
    "origin",
    "referer",
    "te",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "user-agent",
    "via",

    "proxy-foo",  // Test for any header starting with proxy- or sec-.
    "sec-foo",
  };

  // Test that no forbidden header fields can be set.
  for (size_t i = 0; i < arraysize(kForbiddenHeaderFields); ++i) {
    std::string headers(kForbiddenHeaderFields[i]);
    headers.append(": foo");
    ASSERT_FALSE(info_->SetStringProperty(
        PP_URLREQUESTPROPERTY_HEADERS, headers.c_str()));
    ASSERT_TRUE(IsNullOrEmpty(GetHeaderValue(kForbiddenHeaderFields[i])));
  }

  // Test that forbidden header can't be set in various ways.
  ASSERT_FALSE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_HEADERS, "cookie : foo"));
  ASSERT_TRUE(IsNullOrEmpty(GetHeaderValue("cookie")));

  // Test that forbidden header can't be set with an allowed one.
  ASSERT_FALSE(info_->SetStringProperty(
      PP_URLREQUESTPROPERTY_HEADERS, "foo: bar\ncookie: foo"));
  ASSERT_TRUE(IsNullOrEmpty(GetHeaderValue("cookie")));
}

// TODO(bbudge) Unit tests for AppendDataToBody, AppendFileToBody.

}  // namespace ppapi
}  // namespace webkit

