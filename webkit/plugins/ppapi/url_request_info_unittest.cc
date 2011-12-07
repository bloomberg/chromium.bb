// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/url_request_info_impl.h"
#include "ppapi/thunk/thunk.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/user_agent.h"
#include "webkit/glue/webkit_glue.h"
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
    info_ = new PPB_URLRequestInfo_Impl(instance()->pp_instance(),
                                        ::ppapi::PPB_URLRequestInfo_Data());
  }

  static void SetUpTestCase() {
    webkit_glue::SetUserAgent(webkit_glue::BuildUserAgentFromProduct(
        "TestShell/0.0.0.0"), false);
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
    WebURLRequest web_request;
    if (!info_->ToWebURLRequest(frame_, &web_request))
      return false;
    return web_request.downloadToFile();
  }

  WebCString GetURL() {
    WebURLRequest web_request;
    if (!info_->ToWebURLRequest(frame_, &web_request))
      return WebCString();
    return web_request.url().spec();
  }

  WebString GetMethod() {
    WebURLRequest web_request;
    if (!info_->ToWebURLRequest(frame_, &web_request))
      return WebString();
    return web_request.httpMethod();
  }

  WebString GetHeaderValue(const char* field) {
    WebURLRequest web_request;
    if (!info_->ToWebURLRequest(frame_, &web_request))
      return WebString();
    return web_request.httpHeaderField(WebString::fromUTF8(field));
  }

  bool SetBooleanProperty(PP_URLRequestProperty prop, bool b) {
    return info_->SetBooleanProperty(prop, b);
  }
  bool SetStringProperty(PP_URLRequestProperty prop, const std::string& s) {
    return info_->SetStringProperty(prop, s);
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
  const PPB_URLRequestInfo* request_info =
      ::ppapi::thunk::GetPPB_URLRequestInfo_Thunk();
  EXPECT_TRUE(request_info);
  EXPECT_TRUE(request_info->Create);
  EXPECT_TRUE(request_info->IsURLRequestInfo);
  EXPECT_TRUE(request_info->SetProperty);
  EXPECT_TRUE(request_info->AppendDataToBody);
  EXPECT_TRUE(request_info->AppendFileToBody);
}

TEST_F(URLRequestInfoTest, AsURLRequestInfo) {
  EXPECT_EQ(info_, info_->AsPPB_URLRequestInfo_API());
}

TEST_F(URLRequestInfoTest, StreamToFile) {
  SetStringProperty(PP_URLREQUESTPROPERTY_URL, "http://www.google.com");

  EXPECT_FALSE(GetDownloadToFile());

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_STREAMTOFILE, true));
  EXPECT_TRUE(GetDownloadToFile());

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_STREAMTOFILE, false));
  EXPECT_FALSE(GetDownloadToFile());
}

TEST_F(URLRequestInfoTest, FollowRedirects) {
  EXPECT_TRUE(info_->GetData().follow_redirects);

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, false));
  EXPECT_FALSE(info_->GetData().follow_redirects);

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, true));
  EXPECT_TRUE(info_->GetData().follow_redirects);
}

TEST_F(URLRequestInfoTest, RecordDownloadProgress) {
  EXPECT_FALSE(info_->GetData().record_download_progress);

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS, true));
  EXPECT_TRUE(info_->GetData().record_download_progress);

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS, false));
  EXPECT_FALSE(info_->GetData().record_download_progress);
}

TEST_F(URLRequestInfoTest, RecordUploadProgress) {
  EXPECT_FALSE(info_->GetData().record_upload_progress);

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS, true));
  EXPECT_TRUE(info_->GetData().record_upload_progress);

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS, false));
  EXPECT_FALSE(info_->GetData().record_upload_progress);
}

TEST_F(URLRequestInfoTest, AllowCrossOriginRequests) {
  EXPECT_FALSE(info_->GetData().allow_cross_origin_requests);

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS, true));
  EXPECT_TRUE(info_->GetData().allow_cross_origin_requests);

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS, false));
  EXPECT_FALSE(info_->GetData().allow_cross_origin_requests);
}

TEST_F(URLRequestInfoTest, AllowCredentials) {
  EXPECT_FALSE(info_->GetData().allow_credentials);

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS, true));
  EXPECT_TRUE(info_->GetData().allow_credentials);

  EXPECT_TRUE(SetBooleanProperty(
      PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS, false));
  EXPECT_FALSE(info_->GetData().allow_credentials);
}

TEST_F(URLRequestInfoTest, SetURL) {
  // Test default URL is "about:blank".
  EXPECT_TRUE(IsExpected(GetURL(), "about:blank"));

  const char* url = "http://www.google.com/";
  EXPECT_TRUE(SetStringProperty(
      PP_URLREQUESTPROPERTY_URL, url));
  EXPECT_TRUE(IsExpected(GetURL(), url));
}

TEST_F(URLRequestInfoTest, JavascriptURL) {
  const char* url = "javascript:foo = bar";
  EXPECT_FALSE(info_->RequiresUniversalAccess());
  SetStringProperty(PP_URLREQUESTPROPERTY_URL, url);
  EXPECT_TRUE(info_->RequiresUniversalAccess());
}

TEST_F(URLRequestInfoTest, SetMethod) {
  // Test default method is "GET".
  EXPECT_TRUE(IsExpected(GetMethod(), "GET"));
  EXPECT_TRUE(SetStringProperty(
      PP_URLREQUESTPROPERTY_METHOD, "POST"));
  EXPECT_TRUE(IsExpected(GetMethod(), "POST"));
}

TEST_F(URLRequestInfoTest, SetHeaders) {
  // Test default header field.
  EXPECT_TRUE(IsExpected(
      GetHeaderValue("foo"), ""));
  // Test that we can set a header field.
  EXPECT_TRUE(SetStringProperty(
      PP_URLREQUESTPROPERTY_HEADERS, "foo: bar"));
  EXPECT_TRUE(IsExpected(
      GetHeaderValue("foo"), "bar"));
  // Test that we can set multiple header fields using \n delimiter.
  EXPECT_TRUE(SetStringProperty(
      PP_URLREQUESTPROPERTY_HEADERS, "foo: bar\nbar: baz"));
  EXPECT_TRUE(IsExpected(
      GetHeaderValue("foo"), "bar"));
  EXPECT_TRUE(IsExpected(
      GetHeaderValue("bar"), "baz"));
}

// TODO(bbudge) Unit tests for AppendDataToBody, AppendFileToBody.

}  // namespace ppapi
}  // namespace webkit

