// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"

#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/chrome_url_util.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_consumer.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Constructs a GridItem from a |web_state|.
GridItem* CreateItem(web::WebState* web_state) {
  TabIdTabHelper* tab_helper = TabIdTabHelper::FromWebState(web_state);
  GridItem* item = [[GridItem alloc] initWithIdentifier:tab_helper->tab_id()];
  // chrome://newtab (NTP) tabs have no title.
  if (IsURLNtp(web_state->GetVisibleURL())) {
    item.hidesTitle = YES;
  }
  item.title = tab_util::GetTabTitle(web_state);
  return item;
}

// Constructs an array of GridItems from a |web_state_list|.
NSArray* CreateItems(WebStateList* web_state_list) {
  NSMutableArray* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < web_state_list->count(); i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    [items addObject:CreateItem(web_state)];
  }
  return [items copy];
}

// Returns the ID of the active tab in |web_state_list|.
NSString* GetActiveTabId(WebStateList* web_state_list) {
  if (!web_state_list)
    return nil;

  web::WebState* web_state = web_state_list->GetActiveWebState();
  if (!web_state)
    return nil;
  TabIdTabHelper* tab_helper = TabIdTabHelper::FromWebState(web_state);
  return tab_helper->tab_id();
}

// Returns the WebState with |identifier| in |web_state_list|. Returns |nullptr|
// if not found.
web::WebState* GetWebStateWithId(WebStateList* web_state_list,
                                 NSString* identifier) {
  for (int i = 0; i < web_state_list->count(); i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    TabIdTabHelper* tab_helper = TabIdTabHelper::FromWebState(web_state);
    if ([identifier isEqualToString:tab_helper->tab_id()])
      return web_state;
  }
  return nullptr;
}

}  // namespace

@interface TabStripMediator () <CRWWebStateObserver, WebStateListObserving> {
  // Bridge C++ WebStateListObserver methods to this TabStripController.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // Bridge C++ WebStateObserver methods to this TabStripController.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Forward observer methods for all WebStates in the WebStateList monitored
  // by the TabStripMediator.
  std::unique_ptr<AllWebStateObservationForwarder>
      _allWebStateObservationForwarder;
}

// The consumer for this object.
@property(nonatomic, weak) id<TabStripConsumer> consumer;

@end

@implementation TabStripMediator

- (instancetype)initWithConsumer:(id<TabStripConsumer>)consumer {
  if (self = [super init]) {
    _consumer = consumer;
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _allWebStateObservationForwarder.reset();
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
  }
}

#pragma mark - Public properties

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _allWebStateObservationForwarder.reset();
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;

  if (_webStateList) {
    DCHECK_GE(_webStateList->count(), 0);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    // Observe all webStates of this |_webStateList|.
    _allWebStateObservationForwarder =
        std::make_unique<AllWebStateObservationForwarder>(
            _webStateList, _webStateObserver.get());
  }
  [self populateConsumerItems];
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  [self populateConsumerItems];
}

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self populateConsumerItems];
}

#pragma mark - TabFaviconDataSource

- (void)faviconForIdentifier:(NSString*)identifier
                  completion:(void (^)(UIImage*))completion {
  web::WebState* webState = GetWebStateWithId(_webStateList, identifier);
  if (!webState) {
    return;
  }
  // NTP tabs get no favicon.
  if (IsURLNtp(webState->GetVisibleURL())) {
    return;
  }
  UIImage* defaultFavicon =
      webState->GetBrowserState()->IsOffTheRecord()
          ? [UIImage imageNamed:@"default_world_favicon_incognito"]
          : [UIImage imageNamed:@"default_world_favicon_regular"];
  completion(defaultFavicon);

  favicon::FaviconDriver* faviconDriver =
      favicon::WebFaviconDriver::FromWebState(webState);
  if (faviconDriver) {
    gfx::Image favicon = faviconDriver->GetFavicon();
    if (!favicon.IsEmpty())
      completion(favicon.ToUIImage());
  }
}

#pragma mark - Private

// Calls |-populateItems:selectedItemID:| on the consumer.
- (void)populateConsumerItems {
  if (!self.webStateList)
    return;
  if (self.webStateList->count() > 0) {
    [self.consumer populateItems:CreateItems(self.webStateList)
                  selectedItemID:GetActiveTabId(self.webStateList)];
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  // Assumption: the ID of the webState didn't change as a result of this load.
  TabIdTabHelper* tabHelper = TabIdTabHelper::FromWebState(webState);
  NSString* itemID = tabHelper->tab_id();
  [self.consumer replaceItemID:itemID withItem:CreateItem(webState)];
}

@end
