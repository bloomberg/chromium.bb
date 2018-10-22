// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dom_distiller {

using ::testing::_;
using namespace switches::reader_mode_heuristics;

namespace {

const char kSimpleArticlePath[] = "/dom_distiller/simple_article.html";
const char kSimpleArticleIFramePath[] =
    "/dom_distiller/simple_article_iframe.html";
const char kArticlePath[] = "/dom_distiller/og_article.html";
const char kNonArticlePath[] = "/dom_distiller/non_og_article.html";

const char* kAllPaths[] = {
    kSimpleArticlePath,
    kSimpleArticleIFramePath,
    kArticlePath,
    kNonArticlePath
};

class Holder {
 public:
  virtual ~Holder() {}
  virtual void OnResult(bool, bool, bool) = 0;
};

class MockDelegate : public Holder {
 public:
  MOCK_METHOD3(OnResult, void(bool, bool, bool));

  base::RepeatingCallback<void(bool, bool, bool)> GetDelegate() {
    return base::BindRepeating(&MockDelegate::OnResult, base::Unretained(this));
  }
};

// Wait a bit to make sure there are no extra calls after the last expected
// call. All the expected calls happen within ~1ms on linux release build,
// so 100ms should be pretty safe to catch extra calls.
// If there are no extra calls, changing this doesn't change the test result.
const auto kWaitAfterLastCall = base::TimeDelta::FromMilliseconds(100);

// If there are no expected calls, the test wait for a while to make sure there
// are // no calls in this period of time. When there are expected calls, they
// happen within 100ms after content::WaitForLoadStop() on linux release build,
// and 10X safety margin is used.
// If there are no extra calls, changing this doesn't change the test result.
const auto kWaitNoExpectedCall = base::TimeDelta::FromSeconds(1);

}  // namespace

template<const char Option[]>
class DistillablePageUtilsBrowserTestOption : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
    command_line->AppendSwitchASCII(switches::kReaderModeHeuristics,
        Option);
    command_line->AppendSwitch(switches::kEnableDistillabilityService);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    web_contents_ =
        browser()->tab_strip_model()->GetActiveWebContents();
    setDelegate(web_contents_, holder_.GetDelegate());
  }

  void NavigateAndWait(const char* url, base::TimeDelta test_timeout) {
    run_loop_ = std::make_unique<base::RunLoop>();

    GURL article_url(url);
    if (base::StartsWith(url, "/", base::CompareCase::SENSITIVE)) {
      article_url = embedded_test_server()->GetURL(url);
    }

    // This blocks until the navigation has completely finished.
    ui_test_utils::NavigateToURL(browser(), article_url);
    content::WaitForLoadStop(web_contents_);

    if (!test_timeout.is_zero())
      QuitAfter(test_timeout);

    run_loop_->Run();
    run_loop_.reset();
  }

  void QuitSoon() { QuitAfter(kWaitAfterLastCall); }

  void QuitAfter(base::TimeDelta delta) {
    DCHECK(delta > base::TimeDelta());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop_->QuitClosure(), delta);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  MockDelegate holder_;
  content::WebContents* web_contents_ = nullptr;
};


using DistillablePageUtilsBrowserTestAlways =
    DistillablePageUtilsBrowserTestOption<kAlwaysTrue>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAlways,
                       TestDelegate) {
  for (unsigned i = 0; i < sizeof(kAllPaths) / sizeof(kAllPaths[0]); ++i) {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(true, true, _))
        .WillOnce(testing::InvokeWithoutArgs(
            this, &DistillablePageUtilsBrowserTestOption::QuitSoon));
    NavigateAndWait(kAllPaths[i], base::TimeDelta());
  }
  // Test pages that we don't care about its distillability.
  {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(_, _, _)).Times(0);
    NavigateAndWait("about:blank", kWaitNoExpectedCall);
  }
}


using DistillablePageUtilsBrowserTestNone =
    DistillablePageUtilsBrowserTestOption<kNone>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestNone,
                       TestDelegate) {
  EXPECT_CALL(holder_, OnResult(_, _, _)).Times(0);
  NavigateAndWait(kSimpleArticlePath, kWaitNoExpectedCall);
}


using DistillablePageUtilsBrowserTestOG =
    DistillablePageUtilsBrowserTestOption<kOGArticle>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestOG,
                       TestDelegate) {
  {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(true, true, _))
        .WillOnce(testing::InvokeWithoutArgs(
            this, &DistillablePageUtilsBrowserTestOption::QuitSoon));
    NavigateAndWait(kArticlePath, base::TimeDelta());
  }
  {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(false, true, _))
        .WillOnce(testing::InvokeWithoutArgs(
            this, &DistillablePageUtilsBrowserTestOption::QuitSoon));
    NavigateAndWait(kNonArticlePath, base::TimeDelta());
  }
}


using DistillablePageUtilsBrowserTestAdaboost =
    DistillablePageUtilsBrowserTestOption<kAdaBoost>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAdaboost,
                       TestDelegate) {
  const char* paths[] = {kSimpleArticlePath, kSimpleArticleIFramePath};
  for (unsigned i = 0; i < sizeof(paths)/sizeof(paths[0]); ++i) {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(true, false, false)).Times(1);
    EXPECT_CALL(holder_, OnResult(true, true, false))
        .WillOnce(testing::InvokeWithoutArgs(
            this, &DistillablePageUtilsBrowserTestOption::QuitSoon));
    NavigateAndWait(paths[i], base::TimeDelta());
  }
  {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(false, false, false)).Times(1);
    EXPECT_CALL(holder_, OnResult(false, true, false))
        .WillOnce(testing::InvokeWithoutArgs(
            this, &DistillablePageUtilsBrowserTestOption::QuitSoon));
    NavigateAndWait(kNonArticlePath, base::TimeDelta());
  }
}

using DistillablePageUtilsBrowserTestAllArticles =
    DistillablePageUtilsBrowserTestOption<kAllArticles>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAllArticles,
                       TestDelegate) {
  const char* paths[] = {kSimpleArticlePath, kSimpleArticleIFramePath};
  for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(true, false, false)).Times(1);
    EXPECT_CALL(holder_, OnResult(true, true, false))
        .WillOnce(testing::InvokeWithoutArgs(
            this, &DistillablePageUtilsBrowserTestOption::QuitSoon));
    NavigateAndWait(paths[i], base::TimeDelta());
  }
  {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(false, false, false)).Times(1);
    EXPECT_CALL(holder_, OnResult(false, true, false))
        .WillOnce(testing::InvokeWithoutArgs(
            this, &DistillablePageUtilsBrowserTestOption::QuitSoon));
    NavigateAndWait(kNonArticlePath, base::TimeDelta());
  }
}

}  // namespace dom_distiller
