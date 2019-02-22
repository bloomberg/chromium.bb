// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/test/remoting/qunit_browser_test_runner.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif // !defined(OS_MACOSX)

namespace remoting {

// The test is disabled for these reasons:
// - takes longer than 30s to complete in dbg (https://crbug.com/504204),
// - flaky on ASAN (https://crbug.com/685025), and
// - fails with User Activation v2 (https://crbug.com/855655).
IN_PROC_BROWSER_TEST_F(QUnitBrowserTestRunner,
                       DISABLED_Remoting_Webapp_Js_Unittest) {
  base::FilePath base_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &base_dir));

#if defined(OS_MACOSX)
  if (base::mac::AmIBundled()) {
    // If we are inside a mac bundle, navigate up to the output directory
    base_dir = base::mac::GetAppBundlePath(base_dir).DirName();
  }
#endif // !defined(OS_MACOSX)
  RunTest(
      base_dir.Append(FILE_PATH_LITERAL("remoting/unittests/unittests.html")));
}

}  // namespace remoting
