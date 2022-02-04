// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer_bridge.h"
#include "ios/chrome/browser/policy/policy_features.h"
#import "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/search_engines/search_engines_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_consumer.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#include "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarMediator () <CRWWebStateObserver,
                               OverlayPresenterObserving,
                               WebStateListObserving>

// The current web state associated with the toolbar.
@property(nonatomic, assign) web::WebState* webState;

// Whether an overlay is currently presented over the web content area.
@property(nonatomic, assign, getter=isWebContentAreaShowingOverlay)
    BOOL webContentAreaShowingOverlay;

@end

@implementation ToolbarMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<OverlayPresenterObserverBridge> _overlayObserver;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _overlayObserver = std::make_unique<OverlayPresenterObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)updateConsumerForWebState:(web::WebState*)webState {
  [self updateNavigationBackAndForwardStateForWebState:webState];
  [self updateShareMenuForWebState:webState];
}

- (void)disconnect {
  self.webContentAreaOverlayPresenter = nullptr;

  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }

  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  DCHECK_EQ(_webState, webState);
  [self.consumer setLoadingProgressFraction:progress];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webState = nullptr;
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  DCHECK_EQ(_webStateList, webStateList);
  [self.consumer setTabCount:_webStateList->count()
           addedInBackground:!activating];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  DCHECK_EQ(_webStateList, webStateList);
  [self.consumer setTabCount:_webStateList->count() addedInBackground:NO];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  DCHECK_EQ(_webStateList, webStateList);
  self.webState = newWebState;
}

#pragma mark - AdaptiveToolbarMenusProvider

- (UIMenu*)menuForButtonOfType:(AdaptiveToolbarButtonType)buttonType {
  switch (buttonType) {
    case AdaptiveToolbarButtonTypeBack:
      return [self menuForNavigationItems:self.webState->GetNavigationManager()
                                              ->GetBackwardItems()];

    case AdaptiveToolbarButtonTypeForward:
      return [self menuForNavigationItems:self.webState->GetNavigationManager()
                                              ->GetForwardItems()];

    case AdaptiveToolbarButtonTypeNewTab:
      return [self menuForNewTabButton];

    case AdaptiveToolbarButtonTypeTabGrid:
      return [self menuForTabGridButton];
  }
  return nil;
}

#pragma mark - Setters

- (void)setIncognito:(BOOL)incognito {
  if (incognito == _incognito)
    return;

  _incognito = incognito;
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
  }

  _webState = webState;

  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());

    if (self.consumer) {
      [self updateConsumer];
    }
  }
}

- (void)setConsumer:(id<ToolbarConsumer>)consumer {
  _consumer = consumer;
  [_consumer setVoiceSearchEnabled:ios::provider::IsVoiceSearchEnabled()];
  if (self.webState) {
    [self updateConsumer];
  }
  if (self.webStateList) {
    [self.consumer setTabCount:_webStateList->count() addedInBackground:NO];
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  // TODO(crbug.com/727427):Add support for DCHECK(webStateList).
  _webStateList = webStateList;
  self.webState = nil;

  if (_webStateList) {
    self.webState = self.webStateList->GetActiveWebState();
    _webStateList->AddObserver(_webStateListObserver.get());

    if (self.consumer) {
      [self.consumer setTabCount:_webStateList->count() addedInBackground:NO];
    }
  }
}

- (void)setWebContentAreaOverlayPresenter:
    (OverlayPresenter*)webContentAreaOverlayPresenter {
  if (_webContentAreaOverlayPresenter)
    _webContentAreaOverlayPresenter->RemoveObserver(_overlayObserver.get());

  _webContentAreaOverlayPresenter = webContentAreaOverlayPresenter;

  if (_webContentAreaOverlayPresenter)
    _webContentAreaOverlayPresenter->AddObserver(_overlayObserver.get());
}

- (void)setWebContentAreaShowingOverlay:(BOOL)webContentAreaShowingOverlay {
  if (_webContentAreaShowingOverlay == webContentAreaShowingOverlay)
    return;
  _webContentAreaShowingOverlay = webContentAreaShowingOverlay;
  [self updateShareMenuForWebState:self.webState];
}

#pragma mark - Update helper methods

// Updates the consumer to match the current WebState.
- (void)updateConsumer {
  DCHECK(self.webState);
  DCHECK(self.consumer);
  [self updateConsumerForWebState:self.webState];

  BOOL isNTP = IsVisibleURLNewTabPage(self.webState);
  [self.consumer setIsNTP:isNTP];
  // Never show the loading UI for an NTP.
  BOOL isLoading = self.webState->IsLoading() && !isNTP;
  [self.consumer setLoadingState:isLoading];
  if (isLoading) {
    [self.consumer
        setLoadingProgressFraction:self.webState->GetLoadingProgress()];
  }
  [self updateShareMenuForWebState:self.webState];
}

// Updates the consumer with the new forward and back states.
- (void)updateNavigationBackAndForwardStateForWebState:
    (web::WebState*)webState {
  DCHECK(webState);
  [self.consumer
      setCanGoForward:webState->GetNavigationManager()->CanGoForward()];
  [self.consumer setCanGoBack:webState->GetNavigationManager()->CanGoBack()];
}

// Updates the Share Menu button of the consumer.
- (void)updateShareMenuForWebState:(web::WebState*)webState {
  if (!self.webState)
    return;
  const GURL& URL = webState->GetLastCommittedURL();
  BOOL shareMenuEnabled =
      URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL);
  // Page sharing requires JavaScript execution, which is paused while overlays
  // are displayed over the web content area.
  [self.consumer setShareMenuEnabled:shareMenuEnabled &&
                                     !self.webContentAreaShowingOverlay];
}

#pragma mark - OverlayPresesenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  self.webContentAreaShowingOverlay = YES;
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request {
  self.webContentAreaShowingOverlay = NO;
}

#pragma mark - Private

// Returns a menu for the |navigationItems|.
- (UIMenu*)menuForNavigationItems:
    (const std::vector<web::NavigationItem*>)navigationItems {
  NSMutableArray<UIMenuElement*>* actions = [NSMutableArray array];
  for (web::NavigationItem* navigationItem : navigationItems) {
    NSString* title;
    UIImage* image;
    if ([self shouldUseIncognitoNTPResourcesForURL:navigationItem
                                                       ->GetVirtualURL()]) {
      title = l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_INCOGNITO_TAB);
      image = [UIImage imageNamed:@"incognito_badge"];
    } else {
      title = base::SysUTF16ToNSString(navigationItem->GetTitleForDisplay());
      const gfx::Image& gfxImage = navigationItem->GetFaviconStatus().image;
      if (!gfxImage.IsEmpty()) {
        image = gfxImage.ToUIImage();
      } else {
        image = [UIImage imageNamed:@"default_favicon"];
      }
    }

    __weak __typeof(self) weakSelf = self;
    UIAction* action =
        [UIAction actionWithTitle:title
                            image:image
                       identifier:nil
                          handler:^(UIAction* action) {
                            [weakSelf navigateToPageForItem:navigationItem];
                          }];
    [actions addObject:action];
  }
  return [UIMenu menuWithTitle:@"" children:actions];
}

// Returns YES if incognito NTP title and image should be used for back/forward
// item associated with |URL|.
- (BOOL)shouldUseIncognitoNTPResourcesForURL:(const GURL&)URL {
  return URL.DeprecatedGetOriginAsURL() == kChromeUINewTabURL &&
         self.isIncognito &&
         base::FeatureList::IsEnabled(kUpdateHistoryEntryPointsInIncognito);
}

// Returns the menu for the new tab button.
- (UIMenu*)menuForNewTabButton {
  UIAction* QRCodeSearch = [self.actionFactory actionToShowQRScanner];
  UIAction* voiceSearch = [self.actionFactory actionToStartVoiceSearch];
  UIAction* newSearch = [self.actionFactory actionToStartNewSearch];
  UIAction* newIncognitoSearch =
      [self.actionFactory actionToStartNewIncognitoSearch];

  NSArray* staticActions =
      @[ newSearch, newIncognitoSearch, voiceSearch, QRCodeSearch ];

  UIMenuElement* clipboardAction = [self menuElementForPasteboard];

  if (clipboardAction) {
    UIMenu* staticMenu = [UIMenu menuWithTitle:@""
                                         image:nil
                                    identifier:nil
                                       options:UIMenuOptionsDisplayInline
                                      children:staticActions];

    return [UIMenu menuWithTitle:@"" children:@[ staticMenu, clipboardAction ]];
  }
  return [UIMenu menuWithTitle:@"" children:staticActions];
}

// Returns the menu for the TabGrid button.
- (UIMenu*)menuForTabGridButton {
  UIAction* openNewTab = [self.actionFactory actionToOpenNewTab];

  UIAction* openNewIncognitoTab =
      [self.actionFactory actionToOpenNewIncognitoTab];

  UIMenu* newTabActions =
      [UIMenu menuWithTitle:@""
                      image:nil
                 identifier:nil
                    options:UIMenuOptionsDisplayInline
                   children:@[ openNewTab, openNewIncognitoTab ]];

  UIAction* closeTab = [self.actionFactory actionToCloseCurrentTab];

  return [UIMenu menuWithTitle:@"" children:@[ newTabActions, closeTab ]];
}

// Returns the UIMenuElement for the content of the pasteboard. Can return nil.
- (UIMenuElement*)menuElementForPasteboard {
  absl::optional<std::set<ClipboardContentType>> clipboardContentType =
      ClipboardRecentContent::GetInstance()->GetCachedClipboardContentTypes();

  if (clipboardContentType.has_value()) {
    std::set<ClipboardContentType> clipboardContentTypeValues =
        clipboardContentType.value();

    if (search_engines::SupportsSearchByImage(self.templateURLService) &&
        clipboardContentTypeValues.find(ClipboardContentType::Image) !=
            clipboardContentTypeValues.end()) {
      return [self.actionFactory actionToSearchCopiedImage];
    } else if (clipboardContentTypeValues.find(ClipboardContentType::URL) !=
               clipboardContentTypeValues.end()) {
      return [self.actionFactory actionToSearchCopiedURL];
    } else if (clipboardContentTypeValues.find(ClipboardContentType::Text) !=
               clipboardContentTypeValues.end()) {
      return [self.actionFactory actionToSearchCopiedText];
    }
  }
  return nil;
}

// Navigates to the page associated with |item|.
- (void)navigateToPageForItem:(web::NavigationItem*)item {
  if (!self.webState)
    return;

  int index = self.webState->GetNavigationManager()->GetIndexOfItem(item);
  DCHECK_NE(index, -1);
  self.webState->GetNavigationManager()->GoToIndex(index);
}

@end
