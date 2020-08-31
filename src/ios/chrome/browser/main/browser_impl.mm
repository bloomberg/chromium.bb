// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_impl.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_agent_util.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserImpl::BrowserImpl(ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      command_dispatcher_([[CommandDispatcher alloc] init]) {
  DCHECK(browser_state_);

  web_state_list_delegate_ = std::make_unique<BrowserWebStateListDelegate>();
  web_state_list_ =
      std::make_unique<WebStateList>(web_state_list_delegate_.get());
}

void BrowserImpl::CreateTabModel() {
  tab_model_ = [[TabModel alloc] initWithBrowser:this];
}

BrowserImpl::BrowserImpl(ChromeBrowserState* browser_state,
                         TabModel* tab_model,
                         std::unique_ptr<WebStateList> web_state_list)
    : browser_state_(browser_state),
      tab_model_(tab_model),
      web_state_list_(std::move(web_state_list)) {
  DCHECK(browser_state_);
  DCHECK(tab_model.webStateList == web_state_list_.get());
}

BrowserImpl::~BrowserImpl() {
  for (auto& observer : observers_) {
    observer.BrowserDestroyed(this);
  }
}

ChromeBrowserState* BrowserImpl::GetBrowserState() const {
  return browser_state_;
}

TabModel* BrowserImpl::GetTabModel() const {
  return tab_model_;
}

WebStateList* BrowserImpl::GetWebStateList() const {
  return web_state_list_.get();
}

CommandDispatcher* BrowserImpl::GetCommandDispatcher() const {
  return command_dispatcher_;
}

void BrowserImpl::AddObserver(BrowserObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserImpl::RemoveObserver(BrowserObserver* observer) {
  observers_.RemoveObserver(observer);
}

// static
std::unique_ptr<Browser> Browser::Create(ChromeBrowserState* browser_state) {
  std::unique_ptr<BrowserImpl> browser =
      std::make_unique<BrowserImpl>(browser_state);
  AttachBrowserAgents(browser.get());
  browser->CreateTabModel();
  return browser;
}
