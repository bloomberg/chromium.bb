// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
#define IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_

#include "ios/chrome/browser/main/browser.h"

#include "base/macros.h"

class TestBrowser : public Browser {
 public:
  TestBrowser(ios::ChromeBrowserState* browser_state, TabModel* tab_model);
  ~TestBrowser() override;

  // Browser.
  ios::ChromeBrowserState* GetBrowserState() const override;
  TabModel* GetTabModel() const override;
  WebStateList* GetWebStateList() const override;

 private:
  ios::ChromeBrowserState* browser_state_;
  TabModel* tab_model_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowser);
};

#endif  // IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_H_
