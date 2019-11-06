// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_url_loader.h"

#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

using Result = WebAppUrlLoader::Result;

class WebAppUrlLoaderTest : public InProcessBrowserTest {
 public:
  WebAppUrlLoaderTest() = default;
  ~WebAppUrlLoaderTest() override = default;

  void SetUpOnMainThread() override {
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // The WebContents needs to be destroyed before the profile.
    web_contents_.reset();
  }

  Result LoadUrlAndWait(WebAppUrlLoader* loader, const std::string& path) {
    base::Optional<Result> result;
    base::RunLoop run_loop;
    loader->LoadUrl(embedded_test_server()->GetURL(path), web_contents(),
                    base::BindLambdaForTesting([&](Result r) {
                      result = r;
                      run_loop.Quit();
                    }));
    EXPECT_TRUE(web_contents()->IsLoading());
    run_loop.Run();
    // Currently WebAppUrlLoader uses DidFinishLoad which might get called
    // even if WebContents::IsLoading() is true.
    // TODO(ortuno): Check that the WebContents is no longer loading after
    // the callback is called.

    return result.value();
  }

 protected:
  content::WebContents* web_contents() { return web_contents_.get(); }

  void ResetWebContents() { web_contents_.reset(); }

 private:
  std::unique_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebAppUrlLoaderTest);
};

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, Success) {
  WebAppUrlLoader loader;
  EXPECT_EQ(Result::kUrlLoaded, LoadUrlAndWait(&loader, "/simple.html"));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, 302FoundRedirect) {
  WebAppUrlLoader loader;

  const GURL final_url = embedded_test_server()->GetURL("/simple.html");
  EXPECT_EQ(
      Result::kRedirectedUrlLoaded,
      LoadUrlAndWait(&loader, "/server-redirect-302?" + final_url.spec()));
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, Hung) {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  WebAppUrlLoader loader;
  base::Optional<Result> result;

  loader.LoadUrl(embedded_test_server()->GetURL("/hung"), web_contents(),
                 base::BindLambdaForTesting([&](Result r) { result = r; }));

  // Run all pending tasks. The URL should still be loading.
  EXPECT_TRUE(web_contents()->IsLoading());
  task_runner->RunUntilIdle();
  EXPECT_TRUE(web_contents()->IsLoading());

  // The callback didn't run yet because the site is still loading.
  EXPECT_FALSE(result.has_value());

  // Forward the clock so that |loader| times out.
  task_runner->FastForwardBy(WebAppUrlLoader::kSecondsToWaitForWebContentsLoad);
  EXPECT_FALSE(web_contents()->IsLoading());
  EXPECT_EQ(Result::kFailedPageTookTooLong, result.value());
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, WebContentsDestroyed) {
  WebAppUrlLoader loader;
  base::Optional<Result> result;

  base::RunLoop run_loop;
  loader.LoadUrl(embedded_test_server()->GetURL("/hung"), web_contents(),
                 base::BindLambdaForTesting([&](Result r) {
                   result = r;
                   run_loop.Quit();
                 }));

  // Run all pending tasks. The URL should still be loading.
  EXPECT_TRUE(web_contents()->IsLoading());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_contents()->IsLoading());

  // The callback didn't run yet because the site is still loading.
  EXPECT_FALSE(result.has_value());

  ResetWebContents();
  run_loop.Run();

  EXPECT_EQ(Result::kFailedWebContentsDestroyed, result.value());
}

IN_PROC_BROWSER_TEST_F(WebAppUrlLoaderTest, MultipleLoadUrlCalls) {
  WebAppUrlLoader loader;
  base::Optional<Result> title1_result;
  base::Optional<Result> title2_result;

  std::unique_ptr<content::WebContents> web_contents1 =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  std::unique_ptr<content::WebContents> web_contents2 =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());

  loader.LoadUrl(embedded_test_server()->GetURL("/title1.html"),
                 web_contents1.get(), base::BindLambdaForTesting([&](Result r) {
                   title1_result = r;
                   barrier_closure.Run();
                 }));
  loader.LoadUrl(embedded_test_server()->GetURL("/title2.html"),
                 web_contents2.get(), base::BindLambdaForTesting([&](Result r) {
                   title2_result = r;
                   barrier_closure.Run();
                 }));
  run_loop.Run();
  EXPECT_EQ(Result::kUrlLoaded, title1_result.value());
  EXPECT_EQ(Result::kUrlLoaded, title2_result.value());
}

}  // namespace web_app
