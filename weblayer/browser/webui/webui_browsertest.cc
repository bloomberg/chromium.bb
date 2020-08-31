// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

using WebLayerWebUIBrowserTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(WebLayerWebUIBrowserTest, WebUI) {
  NavigateAndWaitForCompletion(GURL("chrome://weblayer"), shell());
  base::RunLoop run_loop;
  bool result =
      ExecuteScript(shell(),
                    "document.getElementById('remote-debug-label').hidden",
                    true /* use_separate_isolate */)
          .GetBool();
  // The remote debug checkbox should only be visible on Android.
#if defined(OS_ANDROID)
  EXPECT_FALSE(result);
#else
  EXPECT_TRUE(result);
#endif
}

}  // namespace weblayer
