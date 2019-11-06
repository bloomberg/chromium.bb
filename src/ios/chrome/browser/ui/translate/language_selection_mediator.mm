// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/language_selection_mediator.h"

#include <memory>

#include "base/logging.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/language_selection_context.h"
#import "ios/chrome/browser/translate/language_selection_handler.h"
#import "ios/chrome/browser/ui/translate/language_selection_consumer.h"
#import "ios/chrome/browser/ui/translate/language_selection_provider.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LanguageSelectionMediator () <WebStateListObserving,
                                         LanguageSelectionProvider> {
  // WebStateList observers.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserver>>
      _scopedWebStateListObserver;
}

// The language selection handler.
@property(nonatomic, weak) id<LanguageSelectionHandler>
    languageSelectionHandler;

@end

@implementation LanguageSelectionMediator

- (instancetype)initWithLanguageSelectionHandler:
    (id<LanguageSelectionHandler>)handler {
  DCHECK(handler);
  if ((self = [super init])) {
    _languageSelectionHandler = handler;
  }
  return self;
}

#pragma mark - Public methods

- (void)disconnect {
  self.webStateList = nil;
}

- (std::string)languageCodeForLanguageAtIndex:(int)index {
  DCHECK(self.context);
  return self.context.languageData->language_code_at(index);
}

#pragma mark - Properties

- (void)setConsumer:(id<LanguageSelectionConsumer>)consumer {
  _consumer = consumer;
  DCHECK(self.context);
  self.consumer.languageCount = self.context.languageData->num_languages();
  self.consumer.initialLanguageIndex = self.context.initialLanguageIndex;
  self.consumer.disabledLanguageIndex = self.context.unavailableLanguageIndex;
  self.consumer.provider = self;
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList == webStateList)
    return;

  if (_webStateList) {
    [self removeWebStateListObserver];

    // Uninstall delegates for each WebState in WebStateList.
    for (int i = 0; i < self.webStateList->count(); i++) {
      [self uninstallDelegatesForWebState:self.webStateList->GetWebStateAt(i)];
    }
  }

  _webStateList = webStateList;

  if (_webStateList) {
    // Install delegates for each WebState in WebStateList.
    for (int i = 0; i < _webStateList->count(); i++) {
      [self installDelegatesForWebState:_webStateList->GetWebStateAt(i)];
    }

    [self addWebStateListObserver];
  }
}

#pragma mark - Private

// Adds observer for WebStateList.
- (void)addWebStateListObserver {
  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
  _scopedWebStateListObserver =
      std::make_unique<ScopedObserver<WebStateList, WebStateListObserver>>(
          _webStateListObserverBridge.get());
  _scopedWebStateListObserver->Add(self.webStateList);
}

// Removes observer for WebStateList.
- (void)removeWebStateListObserver {
  _scopedWebStateListObserver.reset();
  _webStateListObserverBridge.reset();
}

// Installs delegates for |webState|.
- (void)installDelegatesForWebState:(web::WebState*)webState {
  if (ChromeIOSTranslateClient::FromWebState(webState)) {
    ChromeIOSTranslateClient::FromWebState(webState)
        ->set_language_selection_handler(self.languageSelectionHandler);
  }
}

// Uninstalls delegates for |webState|.
- (void)uninstallDelegatesForWebState:(web::WebState*)webState {
  if (ChromeIOSTranslateClient::FromWebState(webState)) {
    ChromeIOSTranslateClient::FromWebState(webState)
        ->set_language_selection_handler(nil);
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self installDelegatesForWebState:webState];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  [self uninstallDelegatesForWebState:oldWebState];
  [self installDelegatesForWebState:newWebState];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self uninstallDelegatesForWebState:webState];
}

#pragma mark - LanguageSelectionProvider

- (NSString*)languageNameAtIndex:(int)languageIndex {
  DCHECK(self.context);
  if (languageIndex < 0 ||
      languageIndex >=
          static_cast<int>(self.context.languageData->num_languages())) {
    NOTREACHED() << "Language index " << languageIndex
                 << " out of expected range.";
    return nil;
  }
  return base::SysUTF16ToNSString(
      self.context.languageData->language_name_at(languageIndex));
}

@end
