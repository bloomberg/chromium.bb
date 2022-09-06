// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/test_browser.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestBrowser::TestBrowser(
    ChromeBrowserState* browser_state,
    std::unique_ptr<WebStateListDelegate> web_state_list_delegate)
    : browser_state_(browser_state),
      web_state_list_delegate_(std::move(web_state_list_delegate)),
      command_dispatcher_([[CommandDispatcher alloc] init]) {
  DCHECK(browser_state_);
  DCHECK(web_state_list_delegate_);
  web_state_list_ =
      std::make_unique<WebStateList>(web_state_list_delegate_.get());
}

TestBrowser::TestBrowser(ChromeBrowserState* browser_state)
    : TestBrowser(browser_state, std::make_unique<FakeWebStateListDelegate>()) {
}

TestBrowser::~TestBrowser() {
  for (auto& observer : observers_) {
    observer.BrowserDestroyed(this);
  }
}

#pragma mark - Browser

ChromeBrowserState* TestBrowser::GetBrowserState() {
  return browser_state_;
}

WebStateList* TestBrowser::GetWebStateList() {
  return web_state_list_.get();
}

CommandDispatcher* TestBrowser::GetCommandDispatcher() {
  return command_dispatcher_;
}

void TestBrowser::AddObserver(BrowserObserver* observer) {
  observers_.AddObserver(observer);
}

void TestBrowser::RemoveObserver(BrowserObserver* observer) {
  observers_.RemoveObserver(observer);
}
