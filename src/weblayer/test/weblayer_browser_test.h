// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_H_
#define WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_H_

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"

namespace weblayer {
class ProfileImpl;
class Shell;

class WebLayerBrowserTest : public content::BrowserTestBase {
 public:
  WebLayerBrowserTest();
  ~WebLayerBrowserTest() override;

  // content::BrowserTestBase implementation.
  void SetUp() override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

  // Returns the window for the test.
  Shell* shell() const { return shell_; }

  ProfileImpl* GetProfile();

 private:
  Shell* shell_ = nullptr;

  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerBrowserTest);
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_H_
