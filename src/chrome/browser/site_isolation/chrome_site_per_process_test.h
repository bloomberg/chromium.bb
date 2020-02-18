// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_ISOLATION_CHROME_SITE_PER_PROCESS_TEST_H_
#define CHROME_BROWSER_SITE_ISOLATION_CHROME_SITE_PER_PROCESS_TEST_H_

#include "base/macros.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace base {
class CommandLine;
}  // namespace base

class ChromeSitePerProcessTest : public InProcessBrowserTest {
 public:
  ChromeSitePerProcessTest();
  ~ChromeSitePerProcessTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void SetUpOnMainThread() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSitePerProcessTest);
};

#endif  // CHROME_BROWSER_SITE_ISOLATION_CHROME_SITE_PER_PROCESS_TEST_H_
