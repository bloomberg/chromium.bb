// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#import "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper_observer_bridge.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#include "ios/chrome/browser/policy/policy_features.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/search_engines/search_engines_util.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/activity_services/canonical_url_retriever.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_constants.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_navigation_item.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_text_item.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_consumer.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_notification_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_notifier.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#include "ios/web/common/features.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
PopupMenuToolsItem* CreateTableViewItem(int titleID,
                                        PopupMenuAction action,
                                        NSString* imageName,
                                        NSString* accessibilityID) {
  PopupMenuToolsItem* item =
      [[PopupMenuToolsItem alloc] initWithType:kItemTypeEnumZero];
  item.title = l10n_util::GetNSString(titleID);
  item.actionIdentifier = action;
  item.accessibilityIdentifier = accessibilityID;
  if (imageName) {
    item.image = [[UIImage imageNamed:imageName]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }
  return item;
}

PopupMenuTextItem* CreateEnterpriseInfoItem(NSString* imageName,
                                            NSString* message,
                                            PopupMenuAction action,
                                            NSString* accessibilityID) {
  PopupMenuTextItem* item =
      [[PopupMenuTextItem alloc] initWithType:kItemTypeEnumZero];
  item.imageName = imageName;
  item.message = message;
  item.actionIdentifier = action;
  item.accessibilityIdentifier = accessibilityID;

  return item;
}

}  // namespace

@interface PopupMenuMediator () <BookmarkModelBridgeObserver,
                                 CRWWebStateObserver,
                                 IOSLanguageDetectionTabHelperObserving,
                                 OverlayPresenterObserving,
                                 PrefObserverDelegate,
                                 ReadingListMenuNotificationDelegate,
                                 WebStateListObserving> {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _bookmarkModelBridge;
  // Bridge to get notified of the language detection event.
  std::unique_ptr<language::IOSLanguageDetectionTabHelperObserverBridge>
      _iOSLanguageDetectionTabHelperObserverBridge;
  std::unique_ptr<OverlayPresenterObserver> _overlayPresenterObserver;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

// Items to be displayed in the popup menu.
@property(nonatomic, strong)
    NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>* items;

// Type of this mediator.
@property(nonatomic, assign) PopupMenuType type;

// The current web state associated with the toolbar.
@property(nonatomic, assign) web::WebState* webState;

// Whether the popup menu is presented in incognito or not.
@property(nonatomic, assign) BOOL isIncognito;

// Items notifying this items of changes happening to the ReadingList model.
@property(nonatomic, strong) ReadingListMenuNotifier* readingListMenuNotifier;

// Whether the hint for the "New Incognito Tab" item should be triggered.
@property(nonatomic, assign) BOOL triggerNewIncognitoTabTip;

// The current browser policy connector.
@property(nonatomic, assign) BrowserPolicyConnectorIOS* browserPolicyConnector;

// Whether an overlay is currently presented over the web content area.
@property(nonatomic, assign, getter=isWebContentAreaShowingOverlay)
    BOOL webContentAreaShowingOverlay;

// Whether the web content is currently being blocked.
@property(nonatomic, assign) BOOL contentBlocked;

#pragma mark*** Specific Items ***

@property(nonatomic, strong) PopupMenuToolsItem* openNewIncognitoTabItem;
@property(nonatomic, strong) PopupMenuToolsItem* reloadStopItem;
@property(nonatomic, strong) PopupMenuToolsItem* readLaterItem;
@property(nonatomic, strong) PopupMenuToolsItem* bookmarkItem;
@property(nonatomic, strong) PopupMenuToolsItem* translateItem;
@property(nonatomic, strong) PopupMenuToolsItem* findInPageItem;
@property(nonatomic, strong) PopupMenuToolsItem* textZoomItem;
@property(nonatomic, strong) PopupMenuToolsItem* siteInformationItem;
@property(nonatomic, strong) PopupMenuToolsItem* requestDesktopSiteItem;
@property(nonatomic, strong) PopupMenuToolsItem* requestMobileSiteItem;
@property(nonatomic, strong) PopupMenuToolsItem* readingListItem;
// Array containing all the nonnull items/
@property(nonatomic, strong)
    NSArray<TableViewItem<PopupMenuItem>*>* specificItems;

@end

@implementation PopupMenuMediator

#pragma mark - Public

- (instancetype)initWithType:(PopupMenuType)type
                  isIncognito:(BOOL)isIncognito
             readingListModel:(ReadingListModel*)readingListModel
    triggerNewIncognitoTabTip:(BOOL)triggerNewIncognitoTabTip
       browserPolicyConnector:
           (BrowserPolicyConnectorIOS*)browserPolicyConnector {
  self = [super init];
  if (self) {
    _isIncognito = isIncognito;
    _type = type;
    _readingListMenuNotifier =
        [[ReadingListMenuNotifier alloc] initWithReadingList:readingListModel];
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _overlayPresenterObserver =
        std::make_unique<OverlayPresenterObserverBridge>(self);
    _triggerNewIncognitoTabTip = triggerNewIncognitoTabTip;
    _browserPolicyConnector = browserPolicyConnector;
  }
  return self;
}

- (void)disconnect {
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

  if (_engagementTracker) {
    if (_readingListItem.badgeText.length != 0) {
      _engagementTracker->Dismissed(
          feature_engagement::kIPHBadgedReadingListFeature);
    }

    if (_translateItem.badgeText.length != 0) {
      _engagementTracker->Dismissed(
          feature_engagement::kIPHBadgedTranslateManualTriggerFeature);
    }

    _engagementTracker = nullptr;
  }

  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->RemoveObserver(
        _overlayPresenterObserver.get());
    self.webContentAreaShowingOverlay = NO;
    _webContentAreaOverlayPresenter = nullptr;
  }

  _readingListMenuNotifier = nil;
  _bookmarkModelBridge.reset();
  _iOSLanguageDetectionTabHelperObserverBridge.reset();

  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  self.webState = nullptr;
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  DCHECK_EQ(_webStateList, webStateList);
  self.webState = newWebState;
}

#pragma mark - BookmarkModelBridgeObserver

// If an added or removed bookmark is the same as the current url, update the
// toolbar so the star highlight is kept in sync.
- (void)bookmarkNodeChildrenChanged:
    (const bookmarks::BookmarkNode*)bookmarkNode {
  [self updateBookmarkItem];
}

// If all bookmarks are removed, update the toolbar so the star highlight is
// kept in sync.
- (void)bookmarkModelRemovedAllNodes {
  [self updateBookmarkItem];
}

// In case we are on a bookmarked page before the model is loaded.
- (void)bookmarkModelLoaded {
  [self updateBookmarkItem];
}

- (void)bookmarkNodeChanged:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self updateBookmarkItem];
}
- (void)bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
     movedFromParent:(const bookmarks::BookmarkNode*)oldParent
            toParent:(const bookmarks::BookmarkNode*)newParent {
  // No-op -- required by BookmarkModelBridgeObserver but not used.
}
- (void)bookmarkNodeDeleted:(const bookmarks::BookmarkNode*)node
                 fromFolder:(const bookmarks::BookmarkNode*)folder {
  [self updateBookmarkItem];
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  self.webContentAreaShowingOverlay = YES;
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request {
  self.webContentAreaShowingOverlay = NO;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == bookmarks::prefs::kEditBookmarksEnabled)
    [self updateBookmarkItem];
}

#pragma mark - Properties

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());

    _iOSLanguageDetectionTabHelperObserverBridge.reset();
  }

  _webState = webState;

  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());

    // Observer the language::IOSLanguageDetectionTabHelper for |_webState|.
    _iOSLanguageDetectionTabHelperObserverBridge =
        std::make_unique<language::IOSLanguageDetectionTabHelperObserverBridge>(
            language::IOSLanguageDetectionTabHelper::FromWebState(_webState),
            self);

    if (self.popupMenu) {
      [self updatePopupMenu];
    }
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;
  self.webState = nil;

  if (_webStateList) {
    self.webState = self.webStateList->GetActiveWebState();
    _webStateList->AddObserver(_webStateListObserver.get());
  }
}

- (void)setWebContentAreaOverlayPresenter:
    (OverlayPresenter*)webContentAreaOverlayPresenter {
  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->RemoveObserver(
        _overlayPresenterObserver.get());
    self.webContentAreaShowingOverlay = NO;
  }

  _webContentAreaOverlayPresenter = webContentAreaOverlayPresenter;

  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->AddObserver(
        _overlayPresenterObserver.get());
    self.webContentAreaShowingOverlay =
        _webContentAreaOverlayPresenter->IsShowingOverlayUI();
  }
}

- (void)setPopupMenu:(id<PopupMenuConsumer>)popupMenu {
  _popupMenu = popupMenu;

  [_popupMenu setPopupMenuItems:self.items];
  if (self.triggerNewIncognitoTabTip) {
    _popupMenu.itemToHighlight = self.openNewIncognitoTabItem;
    self.triggerNewIncognitoTabTip = NO;
  }
  if (self.webState) {
    [self updatePopupMenu];
  }
}

- (void)setEngagementTracker:(feature_engagement::Tracker*)engagementTracker {
  _engagementTracker = engagementTracker;

  if (!self.popupMenu || !engagementTracker)
    return;

  if (self.readingListItem &&
      self.engagementTracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHBadgedReadingListFeature)) {
    self.readingListItem.badgeText = l10n_util::GetNSStringWithFixup(
        IDS_IOS_TOOLS_MENU_CELL_NEW_FEATURE_BADGE);
    [self.popupMenu itemsHaveChanged:@[ self.readingListItem ]];
  }

  if (self.translateItem &&
      self.engagementTracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHBadgedTranslateManualTriggerFeature)) {
    self.translateItem.badgeText = l10n_util::GetNSStringWithFixup(
        IDS_IOS_TOOLS_MENU_CELL_NEW_FEATURE_BADGE);
    [self.popupMenu itemsHaveChanged:@[ self.translateItem ]];
  }
}

- (void)setBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel {
  _bookmarkModel = bookmarkModel;
  _bookmarkModelBridge.reset();
  if (bookmarkModel) {
    _bookmarkModelBridge =
        std::make_unique<bookmarks::BookmarkModelBridge>(self, bookmarkModel);
  }

  if (self.webState && self.popupMenu) {
    [self updateBookmarkItem];
  }
}

- (NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>*)items {
  if (!_items) {
    switch (self.type) {
      case PopupMenuTypeToolsMenu:
        [self createToolsMenuItems];
        break;
      case PopupMenuTypeNavigationForward:
        DCHECK(!ShouldUseUIKitPopupMenu());
        [self createNavigationItemsForType:PopupMenuTypeNavigationForward];
        break;
      case PopupMenuTypeNavigationBackward:
        DCHECK(!ShouldUseUIKitPopupMenu());
        [self createNavigationItemsForType:PopupMenuTypeNavigationBackward];
        break;
      case PopupMenuTypeTabGrid:
        DCHECK(!ShouldUseUIKitPopupMenu());
        [self createTabGridMenuItems];
        break;
      case PopupMenuTypeTabStripTabGrid:
        DCHECK(!ShouldUseUIKitPopupMenu());
        [self createTabGridMenuItems];
        break;
      case PopupMenuTypeNewTab:
        DCHECK(!ShouldUseUIKitPopupMenu());
        [self createSearchMenuItems];
        break;
    }
    NSMutableArray* specificItems = [NSMutableArray array];
    if (self.reloadStopItem)
      [specificItems addObject:self.reloadStopItem];
    if (self.readLaterItem)
      [specificItems addObject:self.readLaterItem];
    if (self.bookmarkItem)
      [specificItems addObject:self.bookmarkItem];
    if (self.translateItem)
      [specificItems addObject:self.translateItem];
    if (self.findInPageItem)
      [specificItems addObject:self.findInPageItem];
    if (self.textZoomItem)
      [specificItems addObject:self.textZoomItem];
    if (self.siteInformationItem)
      [specificItems addObject:self.siteInformationItem];
    if (self.requestDesktopSiteItem)
      [specificItems addObject:self.requestDesktopSiteItem];
    if (self.requestMobileSiteItem)
      [specificItems addObject:self.requestMobileSiteItem];
    if (self.readingListItem)
      [specificItems addObject:self.readingListItem];
    self.specificItems = specificItems;
  }
  return _items;
}

- (void)setWebContentAreaShowingOverlay:(BOOL)webContentAreaShowingOverlay {
  if (_webContentAreaShowingOverlay == webContentAreaShowingOverlay)
    return;
  _webContentAreaShowingOverlay = webContentAreaShowingOverlay;
  [self updatePopupMenu];
}

- (void)setPrefService:(PrefService*)prefService {
  _prefService = prefService;
  _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
  _prefChangeRegistrar->Init(prefService);
  _prefObserverBridge.reset(new PrefObserverBridge(self));
  _prefObserverBridge->ObserveChangesForPreference(
      bookmarks::prefs::kEditBookmarksEnabled, _prefChangeRegistrar.get());
}

#pragma mark - PopupMenuActionHandlerDelegate

- (void)readPageLater {
  if (!self.webState)
    return;
  // The mediator can be destroyed when this callback is executed. So it is not
  // possible to use a weak self.
  __weak id<BrowserCommands> weakDispatcher = self.dispatcher;
  GURL visibleURL = self.webState->GetVisibleURL();
  NSString* title = base::SysUTF16ToNSString(self.webState->GetTitle());
  activity_services::RetrieveCanonicalUrl(self.webState, ^(const GURL& URL) {
    const GURL& pageURL = !URL.is_empty() ? URL : visibleURL;
    if (!pageURL.is_valid() || !pageURL.SchemeIsHTTPOrHTTPS())
      return;

    ReadingListAddCommand* command =
        [[ReadingListAddCommand alloc] initWithURL:pageURL title:title];
    [weakDispatcher addToReadingList:command];
  });
}

- (void)navigateToPageForItem:(TableViewItem<PopupMenuItem>*)item {
  if (!self.webState)
    return;

  web::NavigationItem* navigationItem =
      base::mac::ObjCCastStrict<PopupMenuNavigationItem>(item).navigationItem;
  int index =
      self.webState->GetNavigationManager()->GetIndexOfItem(navigationItem);
  DCHECK_NE(index, -1);
  self.webState->GetNavigationManager()->GoToIndex(index);
}

- (void)recordSettingsMetricsPerProfile {
  profile_metrics::BrowserProfileType type =
      _isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                   : profile_metrics::BrowserProfileType::kRegular;
  base::UmaHistogramEnumeration("Settings.OpenSettingsFromMenu.PerProfileType",
                                type);
}

- (void)recordDownloadsMetricsPerProfile {
  profile_metrics::BrowserProfileType type =
      _isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                   : profile_metrics::BrowserProfileType::kRegular;
  base::UmaHistogramEnumeration("Download.OpenDownloadsFromMenu.PerProfileType",
                                type);
}

- (void)searchCopiedImage {
  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();
  clipboardRecentContent->GetRecentImageFromClipboard(
      base::BindOnce(^(absl::optional<gfx::Image> optionalImage) {
        if (!optionalImage) {
          return;
        }
        UIImage* image = [optionalImage.value().ToUIImage() copy];
        web::NavigationManager::WebLoadParams webParams =
            ImageSearchParamGenerator::LoadParamsForImage(
                image, self.templateURLService);
        UrlLoadParams params = UrlLoadParams::InCurrentTab(webParams);

        self.URLLoadingBrowserAgent->Load(params);
      }));
}

#pragma mark - IOSLanguageDetectionTabHelperObserving

- (void)iOSLanguageDetectionTabHelper:
            (language::IOSLanguageDetectionTabHelper*)tabHelper
                 didDetermineLanguage:
                     (const translate::LanguageDetectionDetails&)details {
  if (!self.translateItem)
    return;
  // Update the translate item state once language details have been determined.
  self.translateItem.enabled = [self isTranslateEnabled];
  [self.popupMenu itemsHaveChanged:@[ self.translateItem ]];
}

#pragma mark - ReadingListMenuNotificationDelegate Implementation

- (void)unreadCountChanged:(NSInteger)unreadCount {
  if (!self.readingListItem)
    return;

  self.readingListItem.badgeNumber = unreadCount;
  [self.popupMenu itemsHaveChanged:@[ self.readingListItem ]];
}

#pragma mark - BrowserContainerConsumer

- (void)setContentBlocked:(BOOL)contentBlocked {
  if (_contentBlocked == contentBlocked) {
    return;
  }
  _contentBlocked = contentBlocked;
  [self updatePopupMenu];
}

#pragma mark - Popup updates (Private)

// Updates the popup menu to have its state in sync with the current page
// status.
- (void)updatePopupMenu {
  [self updateReloadStopItem];
  // The "Add to Reading List" functionality requires JavaScript execution,
  // which is paused while overlays are displayed over the web content area.
  self.readLaterItem.enabled =
      !self.webContentAreaShowingOverlay && [self isCurrentURLWebURL];
  [self updateBookmarkItem];
  self.translateItem.enabled = [self isTranslateEnabled];
  self.findInPageItem.enabled = [self isFindInPageEnabled];
  self.textZoomItem.enabled = [self isTextZoomEnabled];
  self.siteInformationItem.enabled = [self currentWebPageSupportsSiteInfo];
  self.requestDesktopSiteItem.enabled =
      [self userAgentType] == web::UserAgentType::MOBILE;
  self.requestMobileSiteItem.enabled =
      [self userAgentType] == web::UserAgentType::DESKTOP;

  // Reload the items.
  [self.popupMenu itemsHaveChanged:self.specificItems];
}

// Updates the |bookmark| item to match the bookmarked status of the page.
- (void)updateBookmarkItem {
  if (!self.bookmarkItem)
    return;

  self.bookmarkItem.enabled =
      [self isCurrentURLWebURL] && [self isEditBookmarksEnabled];

  if (self.webState && self.bookmarkModel &&
      self.bookmarkModel->IsBookmarked(self.webState->GetVisibleURL())) {
    self.bookmarkItem.title =
        l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK);
    self.bookmarkItem.accessibilityIdentifier = kToolsMenuEditBookmark;
    self.bookmarkItem.image = [[UIImage imageNamed:@"popup_menu_edit_bookmark"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  } else {
    self.bookmarkItem.title =
        l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS);
    self.bookmarkItem.accessibilityIdentifier = kToolsMenuAddToBookmarks;
    self.bookmarkItem.image = [[UIImage imageNamed:@"popup_menu_add_bookmark"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }

  [self.popupMenu itemsHaveChanged:@[ self.bookmarkItem ]];
}

// Updates the |reloadStopItem| item to match the current behavior.
- (void)updateReloadStopItem {
  if ([self isPageLoading] &&
      self.reloadStopItem.accessibilityIdentifier == kToolsMenuReload) {
    self.reloadStopItem.title = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_STOP);
    self.reloadStopItem.actionIdentifier = PopupMenuActionStop;
    self.reloadStopItem.accessibilityIdentifier = kToolsMenuStop;
    self.reloadStopItem.image = [[UIImage imageNamed:@"popup_menu_stop"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  } else if (![self isPageLoading] &&
             self.reloadStopItem.accessibilityIdentifier == kToolsMenuStop) {
    self.reloadStopItem.title =
        l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_RELOAD);
    self.reloadStopItem.actionIdentifier = PopupMenuActionReload;
    self.reloadStopItem.accessibilityIdentifier = kToolsMenuReload;
    self.reloadStopItem.image = [[UIImage imageNamed:@"popup_menu_reload"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }
}

// Whether the current web page has available site info.
- (BOOL)currentWebPageSupportsSiteInfo {
  if (!self.webState)
    return NO;
  web::NavigationItem* navItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!navItem) {
    return NO;
  }
  const GURL& URL = navItem->GetURL();
  // Show site info for offline pages.
  if (URL.SchemeIs(kChromeUIScheme) && URL.host() == kChromeUIOfflineHost) {
    return YES;
  }
  // Do not show site info for NTP.
  if (URL.spec() == kChromeUIAboutNewTabURL ||
      URL.spec() == kChromeUINewTabURL) {
    return NO;
  }

  if (self.contentBlocked) {
    return NO;
  }

  return navItem->GetVirtualURL().is_valid();
}

// Whether the current page is a web page.
- (BOOL)isCurrentURLWebURL {
  if (!self.webState)
    return NO;
  const GURL& URL = self.webState->GetLastCommittedURL();
  return URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL);
}

// Whether the translate menu item should be enabled.
- (BOOL)isTranslateEnabled {
  if (!self.webState)
    return NO;

  auto* translate_client =
      ChromeIOSTranslateClient::FromWebState(self.webState);
  if (!translate_client)
    return NO;

  translate::TranslateManager* translate_manager =
      translate_client->GetTranslateManager();
  DCHECK(translate_manager);
  return translate_manager->CanManuallyTranslate();
}

// Determines whether or not translate is available on the page and logs the
// result. This method should only be called once per popup menu shown.
- (void)logTranslateAvailability {
  if (!self.webState)
    return;

  auto* translate_client =
      ChromeIOSTranslateClient::FromWebState(self.webState);
  if (!translate_client)
    return;

  translate::TranslateManager* translate_manager =
      translate_client->GetTranslateManager();
  DCHECK(translate_manager);
  translate_manager->CanManuallyTranslate(true);
}

// Whether find in page is enabled.
- (BOOL)isFindInPageEnabled {
  if (!self.webState)
    return NO;
  auto* helper = FindTabHelper::FromWebState(self.webState);
  return (helper && helper->CurrentPageSupportsFindInPage() &&
          !helper->IsFindUIActive());
}

// Whether or not text zoom is enabled for this page.
- (BOOL)isTextZoomEnabled {
  if (self.webContentAreaShowingOverlay) {
    return NO;
  }

  if (!self.webState) {
    return NO;
  }
  FontSizeTabHelper* helper = FontSizeTabHelper::FromWebState(self.webState);
  return helper && helper->CurrentPageSupportsTextZoom() &&
         !helper->IsTextZoomUIActive();
}

// Whether the page is currently loading.
- (BOOL)isPageLoading {
  if (!self.webState)
    return NO;
  return self.webState->IsLoading();
}

#pragma mark - Item creation (Private)

- (void)createNavigationItemsForType:(PopupMenuType)type {
  DCHECK(type == PopupMenuTypeNavigationForward ||
         type == PopupMenuTypeNavigationBackward);
  if (!self.webState)
    return;

  web::NavigationManager* navigationManager =
      self.webState->GetNavigationManager();
  std::vector<web::NavigationItem*> navigationItems;
  if (type == PopupMenuTypeNavigationForward) {
    navigationItems = navigationManager->GetForwardItems();
  } else {
    navigationItems = navigationManager->GetBackwardItems();
  }
  NSMutableArray* items = [NSMutableArray array];
  for (web::NavigationItem* navigationItem : navigationItems) {
    PopupMenuNavigationItem* item =
        [[PopupMenuNavigationItem alloc] initWithType:kItemTypeEnumZero];
    if ([self shouldUseIncognitoNTPResourcesForURL:navigationItem
                                                       ->GetVirtualURL()]) {
      item.title = l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_INCOGNITO_TAB);
      item.favicon = [UIImage imageNamed:@"incognito_badge"];
    } else {
      item.title =
          base::SysUTF16ToNSString(navigationItem->GetTitleForDisplay());
      const gfx::Image& image = navigationItem->GetFaviconStatus().image;
      if (!image.IsEmpty())
        item.favicon = image.ToUIImage();
    }
    item.actionIdentifier = PopupMenuActionNavigate;
    item.navigationItem = navigationItem;
    [items addObject:item];
  }

  self.items = @[ items ];
}

// Creates the menu items for the tab grid menu.
- (void)createTabGridMenuItems {
  PopupMenuToolsItem* closeTab =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_CLOSE_TAB, PopupMenuActionCloseTab,
                          @"popup_menu_close_tab", kToolsMenuCloseTabId);
  closeTab.destructiveAction = YES;
  self.items = @[ [self itemsForNewTab], @[ closeTab ] ];
}

// Creates the menu items for the search menu.
- (void)createSearchMenuItems {
  self.items = @[ [self searchMenuStaticItems] ];

  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();

  std::set<ClipboardContentType> clipboard_types;
  clipboard_types.insert(ClipboardContentType::URL);
  clipboard_types.insert(ClipboardContentType::Text);
  clipboard_types.insert(ClipboardContentType::Image);

  clipboardRecentContent->HasRecentContentFromClipboard(
      clipboard_types,
      base::BindOnce(^(std::set<ClipboardContentType> matched_types) {
        __block NSMutableArray* items = [NSMutableArray array];
        // The consumer is expecting an array of arrays of items. Each sub array
        // represent a section in the popup menu. Having one sub array means
        // having all the items in the same section.
        PopupMenuToolsItem* copiedContentItem = nil;
        if (search_engines::SupportsSearchByImage(self.templateURLService) &&
            matched_types.find(ClipboardContentType::Image) !=
                matched_types.end()) {
          copiedContentItem = CreateTableViewItem(
              IDS_IOS_TOOLS_MENU_SEARCH_COPIED_IMAGE,
              PopupMenuActionSearchCopiedImage, @"popup_menu_paste_and_go",
              kToolsMenuCopiedImageSearch);
        } else if (matched_types.find(ClipboardContentType::URL) !=
                   matched_types.end()) {
          copiedContentItem = CreateTableViewItem(
              IDS_IOS_TOOLS_MENU_VISIT_COPIED_LINK,
              PopupMenuActionVisitCopiedLink, @"popup_menu_paste_and_go",
              kToolsMenuPasteAndGo);
        } else if (matched_types.find(ClipboardContentType::Text) !=
                   matched_types.end()) {
          copiedContentItem = CreateTableViewItem(
              IDS_IOS_TOOLS_MENU_SEARCH_COPIED_TEXT,
              PopupMenuActionSearchCopiedText, @"popup_menu_paste_and_go",
              kToolsMenuPasteAndGo);
        }
        if (copiedContentItem) {
          [items addObject:@[ copiedContentItem ]];
        }

        [items addObject:[self searchMenuStaticItems]];
        self.items = items;

        dispatch_async(dispatch_get_main_queue(), ^{
          [self.popupMenu setPopupMenuItems:self.items];
        });
      }));
}

// Creates and returns the search menu items that are static (i.e. always in
// the search menu).
- (NSArray<TableViewItem<PopupMenuItem>*>*)searchMenuStaticItems {
  PopupMenuToolsItem* QRCodeSearch = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_QR_SCANNER, PopupMenuActionQRCodeSearch,
      @"popup_menu_qr_scanner", kToolsMenuQRCodeSearch);
  PopupMenuToolsItem* voiceSearch = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_VOICE_SEARCH, PopupMenuActionVoiceSearch,
      @"popup_menu_voice_search", kToolsMenuVoiceSearch);

  PopupMenuToolsItem* newSearch =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_NEW_SEARCH, PopupMenuActionSearch,
                          @"popup_menu_search", kToolsMenuSearch);
  // Disable the new search if the incognito mode is forced by enterprise
  // policy.
  newSearch.enabled = !IsIncognitoModeForced(self.prefService);

  PopupMenuToolsItem* newIncognitoSearch = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_SEARCH, PopupMenuActionIncognitoSearch,
      @"popup_menu_new_incognito_tab", kToolsMenuIncognitoSearch);
  // Disable the new incognito search if the incognito mode is disabled by
  // enterprise policy.
  newIncognitoSearch.enabled = !IsIncognitoModeDisabled(self.prefService);

  return @[ newSearch, newIncognitoSearch, voiceSearch, QRCodeSearch ];
}

// Creates the menu items for the tools menu.
- (void)createToolsMenuItems {
  // Reload or stop page action, created as reload.
  self.reloadStopItem =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_RELOAD, PopupMenuActionReload,
                          @"popup_menu_reload", kToolsMenuReload);

  NSArray* tabActions = [@[ self.reloadStopItem ]
      arrayByAddingObjectsFromArray:[self itemsForNewTab]];

  if (base::ios::IsMultipleScenesSupported()) {
    tabActions =
        [tabActions arrayByAddingObjectsFromArray:[self itemsForNewWindow]];
  }

  NSArray* browserActions = [self actionItems];

  NSArray* collectionActions = [self collectionItems];

  if (_browserPolicyConnector &&
      _browserPolicyConnector->HasMachineLevelPolicies()) {
    // Show enterprise infomation when chrome is managed by policy and the
    // settings UI flag is enabled.
    NSArray* textActions = [self enterpriseInfoSection];
    self.items =
        @[ tabActions, collectionActions, browserActions, textActions ];
  } else {
    self.items = @[ tabActions, collectionActions, browserActions ];
  }
}

- (NSArray<TableViewItem*>*)itemsForNewTab {
  // Open New Tab.
  PopupMenuToolsItem* openNewTabItem =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_NEW_TAB, PopupMenuActionOpenNewTab,
                          @"popup_menu_new_tab", kToolsMenuNewTabId);

  // Disable the new tab menu item if the incognito mode is forced by enterprise
  // policy.
  openNewTabItem.enabled = !IsIncognitoModeForced(self.prefService);

  // Open New Incognito Tab.
  self.openNewIncognitoTabItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, PopupMenuActionOpenNewIncognitoTab,
      @"popup_menu_new_incognito_tab", kToolsMenuNewIncognitoTabId);

  // Disable the new incognito tab menu item if the incognito mode is disabled
  // by enterprise policy.
  self.openNewIncognitoTabItem.enabled =
      !IsIncognitoModeDisabled(self.prefService);

  return @[ openNewTabItem, self.openNewIncognitoTabItem ];
}

- (NSArray<TableViewItem*>*)itemsForNewWindow {
  if (!base::ios::IsMultipleScenesSupported())
    return @[];

  // Create the menu item -- hardcoded string and no accessibility ID.
  PopupMenuToolsItem* openNewWindowItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_NEW_WINDOW, PopupMenuActionOpenNewWindow,
      @"popup_menu_new_window", kToolsMenuNewWindowId);

  return @[ openNewWindowItem ];
}

- (NSArray<TableViewItem*>*)actionItems {
  NSMutableArray* actionsArray = [NSMutableArray array];
  // Read Later.
  self.readLaterItem = CreateTableViewItem(
      IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST, PopupMenuActionReadLater,
      @"popup_menu_read_later", kToolsMenuReadLater);
  [actionsArray addObject:self.readLaterItem];

  // Add to bookmark.
  self.bookmarkItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS, PopupMenuActionPageBookmark,
      @"popup_menu_add_bookmark", kToolsMenuAddToBookmarks);
  [actionsArray addObject:self.bookmarkItem];

  // Translate.
  [self logTranslateAvailability];
  self.translateItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_TRANSLATE, PopupMenuActionTranslate,
      @"popup_menu_translate", kToolsMenuTranslateId);
  if (self.engagementTracker &&
      self.engagementTracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHBadgedTranslateManualTriggerFeature) &&
      self.isTranslateEnabled) {
    self.translateItem.badgeText = l10n_util::GetNSStringWithFixup(
        IDS_IOS_TOOLS_MENU_CELL_NEW_FEATURE_BADGE);
  }
  [actionsArray addObject:self.translateItem];

  // Find in Page.
  self.findInPageItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_FIND_IN_PAGE, PopupMenuActionFindInPage,
      @"popup_menu_find_in_page", kToolsMenuFindInPageId);
  [actionsArray addObject:self.findInPageItem];

  // Text Zoom
  if (ios::provider::IsTextZoomEnabled()) {
    self.textZoomItem = CreateTableViewItem(
        IDS_IOS_TOOLS_MENU_TEXT_ZOOM, PopupMenuActionTextZoom,
        @"popup_menu_text_zoom", kToolsMenuTextZoom);
    [actionsArray addObject:self.textZoomItem];
  }

  if ([self userAgentType] != web::UserAgentType::DESKTOP) {
    // Request Desktop Site.
    self.requestDesktopSiteItem = CreateTableViewItem(
        IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE, PopupMenuActionRequestDesktop,
        @"popup_menu_request_desktop_site", kToolsMenuRequestDesktopId);
    // Disable the action if the user agent is not mobile.
    self.requestDesktopSiteItem.enabled =
        [self userAgentType] == web::UserAgentType::MOBILE;
    [actionsArray addObject:self.requestDesktopSiteItem];
  } else {
    // Request Mobile Site.
    self.requestMobileSiteItem = CreateTableViewItem(
        IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE, PopupMenuActionRequestMobile,
        @"popup_menu_request_mobile_site", kToolsMenuRequestMobileId);
    [actionsArray addObject:self.requestMobileSiteItem];
  }

  // Site Information.
  self.siteInformationItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_SITE_INFORMATION, PopupMenuActionSiteInformation,
      @"popup_menu_site_information", kToolsMenuSiteInformation);
  [actionsArray addObject:self.siteInformationItem];

  // Report an Issue.
  if (ios::GetChromeBrowserProvider()
          .GetUserFeedbackProvider()
          ->IsUserFeedbackEnabled()) {
    TableViewItem* reportIssue = CreateTableViewItem(
        IDS_IOS_OPTIONS_REPORT_AN_ISSUE, PopupMenuActionReportIssue,
        @"popup_menu_report_an_issue", kToolsMenuReportAnIssueId);
    [actionsArray addObject:reportIssue];
  }

  // Help.
  TableViewItem* help =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_HELP_MOBILE, PopupMenuActionHelp,
                          @"popup_menu_help", kToolsMenuHelpId);
  [actionsArray addObject:help];

#if !defined(NDEBUG)
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  if ([standardDefaults boolForKey:@"DevViewSource"]) {
    PopupMenuToolsItem* item =
        [[PopupMenuToolsItem alloc] initWithType:kItemTypeEnumZero];
    item.title = @"View Source";
    item.actionIdentifier = PopupMenuActionViewSource;
    item.accessibilityIdentifier = @"View Source";

    // Debug menu, not localized, only visible if turned on by a default.
    [actionsArray addObject:item];
  }
#endif  // !defined(NDEBUG)

  return actionsArray;
}

- (NSArray<TableViewItem*>*)collectionItems {
  // Bookmarks.
  TableViewItem* bookmarks = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_BOOKMARKS, PopupMenuActionBookmarks,
      @"popup_menu_bookmarks", kToolsMenuBookmarksId);

  // Reading List.
  self.readingListItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_READING_LIST, PopupMenuActionReadingList,
      @"popup_menu_reading_list", kToolsMenuReadingListId);
  NSInteger numberOfUnreadArticles =
      [self.readingListMenuNotifier readingListUnreadCount];
  self.readingListItem.badgeNumber = numberOfUnreadArticles;
  if (numberOfUnreadArticles) {
    self.readingListItem.additionalAccessibilityLabel =
        AccessibilityLabelForReadingListCellWithCount(numberOfUnreadArticles);
  }
  if (self.engagementTracker &&
      self.engagementTracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHBadgedReadingListFeature)) {
    self.readingListItem.badgeText = l10n_util::GetNSStringWithFixup(
        IDS_IOS_TOOLS_MENU_CELL_NEW_FEATURE_BADGE);
  }

  // Recent Tabs.
  PopupMenuToolsItem* recentTabs = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_RECENT_TABS, PopupMenuActionRecentTabs,
      @"popup_menu_recent_tabs", kToolsMenuOtherDevicesId);
  recentTabs.enabled = !self.isIncognito;

  // History.
  PopupMenuToolsItem* history =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_HISTORY, PopupMenuActionHistory,
                          @"popup_menu_history", kToolsMenuHistoryId);
  history.enabled = !self.isIncognito;

  // Open Downloads folder.
  TableViewItem* downloadsFolder = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_DOWNLOADS, PopupMenuActionOpenDownloads,
      @"popup_menu_downloads", kToolsMenuDownloadsId);

  // Settings.
  TableViewItem* settings =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_SETTINGS, PopupMenuActionSettings,
                          @"popup_menu_settings", kToolsMenuSettingsId);

  if (self.isIncognito &&
      base::FeatureList::IsEnabled(kUpdateHistoryEntryPointsInIncognito)) {
    return @[ bookmarks, self.readingListItem, downloadsFolder, settings ];
  }

  return @[
    bookmarks, self.readingListItem, recentTabs, history, downloadsFolder,
    settings
  ];
}

// Creates the section for enterprise info.
- (NSArray<TableViewItem*>*)enterpriseInfoSection {
  NSString* message = l10n_util::GetNSString(IDS_IOS_ENTERPRISE_MANAGED_INFO);
  TableViewItem* enterpriseInfoItem = CreateEnterpriseInfoItem(
      @"popup_menu_enterprise_icon", message,
      PopupMenuActionEnterpriseInfoMessage, kTextMenuEnterpriseInfo);
  return @[ enterpriseInfoItem ];
}

// Returns the UserAgentType currently in use.
- (web::UserAgentType)userAgentType {
  if (!self.webState)
    return web::UserAgentType::NONE;
  web::NavigationItem* visibleItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!visibleItem)
    return web::UserAgentType::NONE;

  return visibleItem->GetUserAgentType();
}

#pragma mark - Other private methods

// Returns YES if user is allowed to edit any bookmarks.
- (BOOL)isEditBookmarksEnabled {
    return self.prefService->GetBoolean(
        bookmarks::prefs::kEditBookmarksEnabled);
}

// Returns YES if incognito NTP title and image should be used for back/forward
// item associated with |URL|.
- (BOOL)shouldUseIncognitoNTPResourcesForURL:(const GURL&)URL {
  return URL.DeprecatedGetOriginAsURL() == kChromeUINewTabURL &&
         self.isIncognito &&
         base::FeatureList::IsEnabled(kUpdateHistoryEntryPointsInIncognito);
}

@end
