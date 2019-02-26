// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class RunLoopUntilLoadedAndPainted : public content::WebContentsObserver {
 public:
  explicit RunLoopUntilLoadedAndPainted(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  ~RunLoopUntilLoadedAndPainted() override = default;

  // Runs a RunLoop on the main thread until the first non-empty frame is
  // painted and the load is complete for the WebContents provided to the
  // constructor.
  void Run() {
    if (LoadedAndPainted())
      return;

    run_loop_.Run();
  }

 private:
  bool LoadedAndPainted() {
    return web_contents()->CompletedFirstVisuallyNonEmptyPaint() &&
           !web_contents()->IsLoading();
  }

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override {
    if (LoadedAndPainted())
      run_loop_.Quit();
  }
  void DidStopLoading() override {
    if (LoadedAndPainted())
      run_loop_.Quit();
  }

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RunLoopUntilLoadedAndPainted);
};

class NoBestEffortTasksTest : public InProcessBrowserTest {
 protected:
  NoBestEffortTasksTest() = default;
  ~NoBestEffortTasksTest() override = default;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableBackgroundTasks);
  }

  DISALLOW_COPY_AND_ASSIGN(NoBestEffortTasksTest);
};

}  // namespace

// Verify that it is possible to load and paint the initial about:blank page
// without running BEST_EFFORT tasks.
//
// TODO(fdoray) https://crbug.com/833989: Times out on Win and ChromeOS ASAN.
#if defined(ADDRESS_SANITIZER) && (defined(OS_CHROMEOS) || defined(OS_WIN))
#define MAYBE_LoadAndPaintAboutBlank DISABLED_LoadAndPaintAboutBlank
#else
#define MAYBE_LoadAndPaintAboutBlank LoadAndPaintAboutBlank
#endif
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksTest, MAYBE_LoadAndPaintAboutBlank) {
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(web_contents->GetLastCommittedURL().IsAboutBlank());

  RunLoopUntilLoadedAndPainted run_until_loaded_and_painted(web_contents);
  run_until_loaded_and_painted.Run();
}

// Verify that it is possible to load and paint a page from the network without
// running BEST_EFFORT tasks.
//
// This test has more dependencies than LoadAndPaintAboutBlank, including
// loading cookies.
IN_PROC_BROWSER_TEST_F(NoBestEffortTasksTest, LoadAndPaintFromNetwork) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::OpenURLParams open(
      embedded_test_server()->GetURL("a.com", "/empty.html"),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_TYPED, false);
  content::WebContents* const web_contents = browser()->OpenURL(open);
  EXPECT_TRUE(web_contents->IsLoading());

  RunLoopUntilLoadedAndPainted run_until_loaded_and_painted(web_contents);
  run_until_loaded_and_painted.Run();
}
