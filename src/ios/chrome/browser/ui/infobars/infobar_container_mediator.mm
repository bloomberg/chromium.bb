// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_mediator.h"

#include "ios/chrome/browser/infobars/infobar_container_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_consumer.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#include "ios/chrome/browser/upgrade/upgrade_center.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarContainerMediator ()<TabModelObserver,
                                       WebStateListObserving> {
  // A single infobar container handles all infobars in all tabs. It keeps
  // track of infobars for current tab (accessed via infobar helper of
  // the current tab).
  std::unique_ptr<InfoBarContainerIOS> _infoBarContainer;
  // Bridge class to deliver webStateList notifications.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

// The mediator's BrowserState.
@property(nonatomic, assign, readonly) ios::ChromeBrowserState* browserState;
// The mediator's TabModel.
@property(nonatomic, weak, readonly) TabModel* tabModel;
// The WebStateList that this mediator listens for any changes on its Webstates.
@property(nonatomic, assign) WebStateList* webStateList;

@end

@implementation InfobarContainerMediator

#pragma mark - Public Interface

- (instancetype)initWithConsumer:(id<InfobarContainerConsumer>)consumer
                  legacyConsumer:(id<InfobarContainerConsumer>)legacyConsumer
                    browserState:(ios::ChromeBrowserState*)browserState
                        tabModel:(TabModel*)tabModel {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _tabModel = tabModel;
    _webStateList = _tabModel.webStateList;

    _infoBarContainer.reset(new InfoBarContainerIOS(consumer, legacyConsumer));
    infobars::InfoBarManager* infoBarManager = nullptr;
    if (_tabModel.currentTab) {
      DCHECK(_tabModel.currentTab.webState);
      infoBarManager =
          InfoBarManagerImpl::FromWebState(_tabModel.currentTab.webState);
    }
    _infoBarContainer->ChangeInfoBarManager(infoBarManager);

    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
    [_tabModel addObserver:self];
  }
  return self;
}

- (void)dealloc {
  [_tabModel removeObserver:self];
  _webStateList->RemoveObserver(_webStateListObserver.get());
  _webStateListObserver.reset();
}

#pragma mark - TabModelObserver

// TODO(crbug.com/892376): Stop observing TabModel and use WebStateList instead.
- (void)tabModel:(TabModel*)model
    newTabWillOpen:(Tab*)tab
      inBackground:(BOOL)background {
  // When adding new tabs, check what kind of reminder infobar should
  // be added to the new tab. Try to add only one of them.
  // This check is done when a new tab is added either through the Tools Menu
  // "New Tab", through a long press on the Tab Switcher button "New Tab", and
  // through creating a New Tab from the Tab Switcher. This method is called
  // after a new tab has added and finished initial navigation. If this is added
  // earlier, the initial navigation may end up clearing the infobar(s) that are
  // just added.
  web::WebState* webState = tab.webState;
  DCHECK(webState);

  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  NSString* tabID = TabIdTabHelper::FromWebState(webState)->tab_id();
  [[UpgradeCenter sharedInstance] addInfoBarToManager:infoBarManager
                                             forTabId:tabID];
  if (!ReSignInInfoBarDelegate::Create(
          self.browserState, tab,
          self.signinPresenter /* id<SigninPresenter> */)) {
    DisplaySyncErrors(self.browserState, tab,
                      self.syncPresenter /* id<SyncPresenter> */);
  }
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  DCHECK_EQ(_webStateList, webStateList);
  if (!newWebState)
    return;
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(newWebState);
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK_EQ(_webStateList, webStateList);
  infobars::InfoBarManager* infoBarManager = nullptr;
  if (newWebState) {
    infoBarManager = InfoBarManagerImpl::FromWebState(newWebState);
  }
  _infoBarContainer->ChangeInfoBarManager(infoBarManager);
}

#pragma mark - UpgradeCenterClient

- (void)showUpgrade:(UpgradeCenter*)center {
  DCHECK(self.tabModel.webStateList);
  // Add an infobar on all the open tabs.
  WebStateList* webStateList = self.tabModel.webStateList;
  for (int index = 0; index < webStateList->count(); ++index) {
    web::WebState* webState = webStateList->GetWebStateAt(index);
    NSString* tabID = TabIdTabHelper::FromWebState(webState)->tab_id();
    infobars::InfoBarManager* infoBarManager =
        InfoBarManagerImpl::FromWebState(webState);
    DCHECK(infoBarManager);
    [center addInfoBarToManager:infoBarManager forTabId:tabID];
  }
}

@end
