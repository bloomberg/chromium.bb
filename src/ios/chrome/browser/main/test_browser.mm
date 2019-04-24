// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/test_browser.h"

#import "ios/chrome/browser/tabs/tab_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestBrowser::TestBrowser(ios::ChromeBrowserState* browser_state,
                         TabModel* tab_model)
    : browser_state_(browser_state), tab_model_(tab_model) {}

TestBrowser::~TestBrowser() {}

ios::ChromeBrowserState* TestBrowser::GetBrowserState() const {
  return browser_state_;
}

TabModel* TestBrowser::GetTabModel() const {
  return tab_model_;
}

WebStateList* TestBrowser::GetWebStateList() const {
  return tab_model_.webStateList;
}
