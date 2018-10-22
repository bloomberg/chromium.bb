// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/image_fetch_tab_helper.h"

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
// Timeout for calling on ImageFetchTabHelper::GetImageData.
const NSTimeInterval kWaitForGetImageDataTimeout = 1.0;

const char kImageUrl[] = "http://www.chrooooooooooome.com/";
const char kImageData[] = "abc";
}

// Test fixture for ImageFetchTabHelper class.
class ImageFetchTabHelperTest : public web::WebTestWithWebState {
 protected:
  ImageFetchTabHelperTest()
      : web::WebTestWithWebState(
            web::TestWebThreadBundle::Options::IO_MAINLOOP) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
    ASSERT_TRUE(LoadHtml("<html></html>"));
    ImageFetchTabHelper::CreateForWebState(web_state());
    SetUpTestSharedURLLoaderFactory();
  }

  // Sets up the network::TestURLLoaderFactory to handle download request.
  void SetUpTestSharedURLLoaderFactory() {
    SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));

    network::ResourceResponseHead head;
    std::string raw_header =
        "HTTP/1.1 200 OK\n"
        "Content-type: image/png\n\n";
    head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(raw_header.c_str(),
                                          raw_header.size()));
    head.mime_type = "image/png";
    network::URLLoaderCompletionStatus status;
    status.decoded_body_length = strlen(kImageData);
    test_url_loader_factory_.AddResponse(GURL(kImageUrl), head, kImageData,
                                         status);
  }

  ImageFetchTabHelper* image_fetch_tab_helper() {
    return ImageFetchTabHelper::FromWebState(web_state());
  }

  // Returns the expected image data in NSData*.
  NSData* GetExpectedData() const {
    return [base::SysUTF8ToNSString(kImageData)
        dataUsingEncoding:NSUTF8StringEncoding];
  }

  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetchTabHelperTest);
};

// Tests that ImageFetchTabHelper::GetImageData can get image data from Js.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsSucceed) {
  // Injects fake |__gCrWeb.imageFetch.getImageData| that returns |kImageData|
  // in base64 format.
  id script_result = ExecuteJavaScript([NSString
      stringWithFormat:
          @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
           "function(id, url)"
           "{ __gCrWeb.message.invokeOnHost({'command': "
           "'imageFetch.getImageData', "
           "'id': id, 'data': btoa('%s')}); }; true;",
          kImageData]);
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// Js fails.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsFail) {
  id script_result = ExecuteJavaScript(
      @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
       "function(id, url)"
       "{ __gCrWeb.message.invokeOnHost({'command': 'imageFetch.getImageData',"
       "'id': id}); }; true;");
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// Js does not send a message back.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsTimeout) {
  // Injects fake |__gCrWeb.imageFetch.getImageData| that does not do anything.
  id script_result = ExecuteJavaScript(
      @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
      @"function(id, url) {}; true;");
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// WebState is destroyed.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithWebStateDestroy) {
  // Injects fake |__gCrWeb.imageFetch.getImageData| that does not do anything.
  id script_result = ExecuteJavaScript(
      @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
      @"function(id, url) {}; true;");
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  DestroyWebState();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// WebState navigates to a new web page.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithWebStateNavigate) {
  // Injects fake |__gCrWeb.imageFetch.getImageData| that does not do anything.
  id script_result = ExecuteJavaScript(
      @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
      @"function(id, url) {}; true;");
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  LoadHtml(@"<html>new</html>"), GURL("http://new.webpage.com/");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
}
