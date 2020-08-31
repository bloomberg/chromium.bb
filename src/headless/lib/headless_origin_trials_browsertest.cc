// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"

using content::URLLoaderInterceptor;

namespace {
constexpr char kBaseDataDir[] = "headless/test/data/";

}  // namespace

namespace headless {

class HeadlessOriginTrialsBrowserTest : public HeadlessBrowserTest {
 public:
  HeadlessOriginTrialsBrowserTest() = default;
  ~HeadlessOriginTrialsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    HeadlessBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [&](URLLoaderInterceptor::RequestParams* params) -> bool {
              URLLoaderInterceptor::WriteResponse(
                  base::StrCat(
                      {kBaseDataDir, params->url_request.url.path_piece()}),
                  params->client.get());
              return true;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    HeadlessBrowserTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessOriginTrialsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(HeadlessOriginTrialsBrowserTest, TrialsCanBeEnabled) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  // TODO(crbug.com/1050190): Implement a permanent, sample trial so this test
  // doesn't rely on WebComponents V0, which will eventually go away.
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(
              GURL("https://example.test/origin_trial_webcomponentsv0.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  // Ensure we can call createShadowRoot(), which is only available when the
  // WebComponents V0 origin trial is enabled.
  EXPECT_TRUE(EvaluateScript(web_contents,
                             "document.createElement('div').createShadowRoot() "
                             "instanceof ShadowRoot")
                  ->GetResult()
                  ->GetValue()
                  ->GetBool());
}

IN_PROC_BROWSER_TEST_F(HeadlessOriginTrialsBrowserTest,
                       TrialsDisabledByDefault) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("https://example.test/no_origin_trial.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  // Ensures that createShadowRoot() is not defined, as no token is provided to
  // enable the WebComponents V0 origin trial.
  // TODO(crbug.com/1050190): Implement a permanent, sample trial so this test
  // doesn't rely on WebComponents V0, which will eventually go away.
  EXPECT_FALSE(
      EvaluateScript(web_contents,
                     "'createShadowRoot' in document.createElement('div')")
          ->GetResult()
          ->GetValue()
          ->GetBool());
}

IN_PROC_BROWSER_TEST_F(HeadlessOriginTrialsBrowserTest,
                       WebComponentsV0CustomElements) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(
              GURL("https://example.test/origin_trial_webcomponentsv0.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  // Ensure we can call registerElement(), which is only available when the
  // WebComponents V0 origin trial is enabled.
  EXPECT_EQ(
      "function",
      EvaluateScript(web_contents, "typeof document.registerElement('my-tag')")
          ->GetResult()
          ->GetValue()
          ->GetString());
}

}  // namespace headless
