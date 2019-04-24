// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller+private.h"

#import <MessageUI/MessageUI.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service_helper.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_metrics.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#include "components/signin/ios/browser/active_state_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#include "ios/chrome/browser/feature_engagement/tracker_util.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#include "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/language/url_language_histogram_factory.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/metrics/size_class_recorder.h"
#include "ios/chrome/browser/metrics/tab_usage_recorder.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/passwords/password_controller.h"
#include "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/prerender/preload_controller_delegate.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#include "ios/chrome/browser/reading_list/features.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/search_engines_util.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/signin/account_consistency_service_factory.h"
#include "ios/chrome/browser/signin/account_reconcilor_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"
#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper_delegate.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_dialog_delegate.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/activity_services/activity_service_legacy_coordinator.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_interaction_controller.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/browser_view_controller_helper.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter_delegate.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"
#import "ios/chrome/browser/ui/dialogs/dialog_presenter.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_presenter_impl.h"
#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/ui/external_file_controller.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_controller_ios.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/image_util/image_copier.h"
#import "ios/chrome/browser/ui/image_util/image_saver.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"
#import "ios/chrome/browser/ui/key_commands_provider.h"
#include "ios/chrome/browser/ui/location_bar/location_bar_model_delegate_ios.h"
#import "ios/chrome/browser/ui/location_bar_notification_names.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui_broadcasting_util.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui_state.h"
#import "ios/chrome/browser/ui/main_content/web_scroll_view_main_content_ui_forwarder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_owning.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/page_not_available_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_manager.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/presenters/vertical_animation_container.h"
#import "ios/chrome/browser/ui/reading_list/offline_page_native_content.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_coordinator.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"
#import "ios/chrome/browser/ui/side_swipe/swipe_view.h"
#import "ios/chrome/browser/ui/static_content/static_html_native_content.h"
#import "ios/chrome/browser/ui/tabs/background_tab_animation_view.h"
#import "ios/chrome/browser/ui/tabs/foreground_tab_animation_view.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_presentation.h"
#import "ios/chrome/browser/ui/tabs/switch_to_tab_animation_view.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_legacy_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/legacy_toolbar_ui_updater.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui_broadcasting_util.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_adaptor.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_coordinator.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_features.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ios/chrome/browser/ui/util/page_animation_util.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller_factory.h"
#include "ios/chrome/browser/upgrade/upgrade_center.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_factory.h"
#import "ios/chrome/browser/url_loading/url_loading_observer_bridge.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"
#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"
#import "ios/chrome/browser/web/blocked_popup_tab_helper.h"
#import "ios/chrome/browser/web/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web/load_timing_tab_helper.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#include "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_factory.h"
#import "ios/chrome/browser/webui/net_export_tab_helper.h"
#import "ios/chrome/browser/webui/net_export_tab_helper_delegate.h"
#import "ios/chrome/browser/webui/show_mail_composer_context.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/fullscreen_provider.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_controller.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ios/web/public/features.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer_util.h"
#include "ios/web/public/url_scheme_util.h"
#include "ios/web/public/user_agent.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/navigation_context.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_delegate_bridge.h"
#include "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/page_transition_types.h"
#import "ui/gfx/image/image_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;
using bookmarks::BookmarkNode;

namespace {

const size_t kMaxURLDisplayChars = 32 * 1024;

typedef NS_ENUM(NSInteger, ContextMenuHistogram) {
  // Note: these values must match the ContextMenuOption enum in histograms.xml.
  ACTION_OPEN_IN_NEW_TAB = 0,
  ACTION_OPEN_IN_INCOGNITO_TAB = 1,
  ACTION_COPY_LINK_ADDRESS = 2,
  ACTION_SAVE_IMAGE = 6,
  ACTION_OPEN_IMAGE = 7,
  ACTION_OPEN_IMAGE_IN_NEW_TAB = 8,
  ACTION_COPY_IMAGE = 9,
  ACTION_SEARCH_BY_IMAGE = 11,
  ACTION_OPEN_JAVASCRIPT = 21,
  ACTION_READ_LATER = 22,
  NUM_ACTIONS = 23,
};

void Record(ContextMenuHistogram action, bool is_image, bool is_link) {
  if (is_image) {
    if (is_link) {
      UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOption.ImageLink", action,
                                NUM_ACTIONS);
    } else {
      UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOption.Image", action,
                                NUM_ACTIONS);
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOption.Link", action,
                              NUM_ACTIONS);
  }
}

bool IsHelpURL(GURL& URL) {
  GURL helpUrl(l10n_util::GetStringUTF16(IDS_IOS_TOOLS_MENU_HELP_URL));
  return helpUrl == URL;
}

// Histogram that tracks user actions related to the WKWebView 3D touch link
// preview API. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class WKWebViewLinkPreviewAction {
  kPreviewAttempted = 0,
  kMaxValue = kPreviewAttempted,
};

// Records 3D touch link preview action histograms.
void Record(WKWebViewLinkPreviewAction action) {
  UMA_HISTOGRAM_ENUMERATION("IOS.WKWebViewLinkPreview", action);
}

// Returns the status bar background color.
UIColor* StatusBarBackgroundColor() {
  return [UIColor colorWithRed:0.11 green:0.11 blue:0.11 alpha:1.0];
}

// Duration of the toolbar animation.
const NSTimeInterval kLegacyFullscreenControllerToolbarAnimationDuration = 0.3;

// When the tab strip moves beyond this origin offset, switch the status bar
// appearance from light to dark.
const CGFloat kTabStripAppearanceOffset = -29;

enum HeaderBehaviour {
  // The header moves completely out of the screen.
  Hideable = 0,
  // This header stay on screen and covers part of the content.
  Overlap
};

// Snackbar category for browser view controller.
NSString* const kBrowserViewControllerSnackbarCategory =
    @"BrowserViewControllerSnackbarCategory";

}  // namespace

#pragma mark - ToolbarContainerView

// TODO(crbug.com/880672): This is a temporary solution.  This logic should be
// handled by ToolbarContainerViewController.
@interface LegacyToolbarContainerView : UIView
@end

@implementation LegacyToolbarContainerView

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  // Don't receive events that don't occur within a subview.  This is necessary
  // because the container view overlaps with web content and the default
  // behavior will intercept touches meant for the web page when the toolbars
  // are collapsed.
  for (UIView* subview in self.subviews) {
    if (CGRectContainsPoint(subview.frame, point))
      return [super hitTest:point withEvent:event];
  }
  return nil;
}

@end

#pragma mark - HeaderDefinition helper

// Class used to define a header, an object displayed at the top of the browser.
@interface HeaderDefinition : NSObject

// The header view.
@property(nonatomic, strong) UIView* view;
// How to place the view, and its behaviour when the headers move.
@property(nonatomic, assign) HeaderBehaviour behaviour;

- (instancetype)initWithView:(UIView*)view
             headerBehaviour:(HeaderBehaviour)behaviour;

+ (instancetype)definitionWithView:(UIView*)view
                   headerBehaviour:(HeaderBehaviour)behaviour;

@end

@implementation HeaderDefinition
@synthesize view = _view;
@synthesize behaviour = _behaviour;

+ (instancetype)definitionWithView:(UIView*)view
                   headerBehaviour:(HeaderBehaviour)behaviour {
  return [[self alloc] initWithView:view headerBehaviour:behaviour];
}

- (instancetype)initWithView:(UIView*)view
             headerBehaviour:(HeaderBehaviour)behaviour {
  self = [super init];
  if (self) {
    _view = view;
    _behaviour = behaviour;
  }
  return self;
}

@end

#pragma mark - BVC

@interface BrowserViewController () <ActivityServicePresentation,
                                     BubblePresenterDelegate,
                                     CaptivePortalDetectorTabHelperDelegate,
                                     CRWNativeContentProvider,
                                     CRWWebStateDelegate,
                                     CRWWebStateObserver,
                                     DialogPresenterDelegate,
                                     FullscreenUIElement,
                                     InfobarPositioner,
                                     KeyCommandsPlumbing,
                                     MainContentUI,
                                     ManageAccountsDelegate,
                                     MFMailComposeViewControllerDelegate,
                                     NetExportTabHelperDelegate,
                                     NewTabPageTabHelperDelegate,
                                     OmniboxPopupPresenterDelegate,
                                     OverscrollActionsControllerDelegate,
                                     PasswordControllerDelegate,
                                     PreloadControllerDelegate,
                                     SideSwipeControllerDelegate,
                                     SnapshotGeneratorDelegate,
                                     TabDialogDelegate,
                                     TabModelObserver,
                                     TabStripPresentation,
                                     ToolbarHeightProviderForFullscreen,
                                     UIGestureRecognizerDelegate,
                                     URLLoadingObserver> {
  // The dependency factory passed on initialization.  Used to vend objects used
  // by the BVC.
  BrowserViewControllerDependencyFactory* _dependencyFactory;

  // Backing ivar for the public property, strong even though the property is
  // weak, because things explode otherwise.
  // Do not directly access this ivar outside of object initialization; use the
  // -tabModel property.
  TabModel* _strongTabModel;

  // Facade objects used by |_toolbarCoordinator|.
  // Must outlive |_toolbarCoordinator|.
  std::unique_ptr<LocationBarModelDelegateIOS> _locationBarModelDelegate;
  std::unique_ptr<LocationBarModel> _locationBarModel;

  // Controller for edge swipe gestures for page and tab navigation.
  SideSwipeController* _sideSwipeController;

  // Handles displaying the context menu for all form factors.
  ContextMenuCoordinator* _contextMenuCoordinator;

  // Backing object for property of the same name.
  DialogPresenter* _dialogPresenter;

  // Handles presentation of JavaScript dialogs.
  std::unique_ptr<JavaScriptDialogPresenterImpl> _javaScriptDialogPresenter;

  // Keyboard commands provider.  It offloads most of the keyboard commands
  // management off of the BVC.
  KeyCommandsProvider* _keyCommandsProvider;

  // Used to inject Javascript implementing the PaymentRequest API and to
  // display the UI.
  PaymentRequestManager* _paymentRequestManager;

  // Used to display the Voice Search UI.  Nil if not visible.
  scoped_refptr<VoiceSearchController> _voiceSearchController;

  // Used to display the Find In Page UI. Nil if not visible.
  FindBarControllerIOS* _findBarController;

  // Adapter to let BVC be the delegate for WebState.
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegate;

  // YES if new tab is animating in.
  BOOL _inNewTabAnimation;

  // YES if Voice Search should be started when the new tab animation is
  // finished.
  BOOL _startVoiceSearchAfterNewTabAnimation;
  // YES if a load was cancelled due to typing in the location bar.
  BOOL _locationBarEditCancelledLoad;
  // YES if waiting for a foreground tab due to expectNewForegroundTab.
  BOOL _expectingForegroundTab;

  // Whether or not -shutdown has been called.
  BOOL _isShutdown;

  // The ChromeBrowserState associated with this BVC.
  ios::ChromeBrowserState* _browserState;  // weak

  // Whether or not Incognito* is enabled.
  BOOL _isOffTheRecord;

  // The last point within |contentArea| that's received a touch.
  CGPoint _lastTapPoint;

  // The time at which |_lastTapPoint| was most recently set.
  CFTimeInterval _lastTapTime;

  // The controller that shows the bookmarking UI after the user taps the star
  // button.
  BookmarkInteractionController* _bookmarkInteractionController;

  // Native controller vended to tab before Tab is added to the tab model.
  __weak id _temporaryNativeController;

  // Coordinator for the share menu (Activity Services).
  ActivityServiceLegacyCoordinator* _activityServiceCoordinator;

  // Coordinator for displaying alerts.
  AlertCoordinator* _alertCoordinator;

  // Coordinator for displaying Sad Tab.
  SadTabCoordinator* _sadTabCoordinator;

  ToolbarCoordinatorAdaptor* _toolbarCoordinatorAdaptor;

  // The toolbar UI updater for the toolbar managed by |_toolbarCoordinator|.
  LegacyToolbarUIUpdater* _toolbarUIUpdater;

  // The main content UI updater for the content displayed by this BVC.
  MainContentUIStateUpdater* _mainContentUIUpdater;

  // The forwarder for web scroll view interation events.
  WebScrollViewMainContentUIForwarder* _webMainContentUIForwarder;

  // The updater that adjusts the toolbar's layout for fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;

  // Coordinator for the Download Manager UI.
  DownloadManagerCoordinator* _downloadManagerCoordinator;

  // A map associating webStates with their NTP coordinators.
  std::map<web::WebState*, NewTabPageCoordinator*> _ntpCoordinatorsForWebStates;

  // Fake status bar view used to blend the toolbar into the status bar.
  UIView* _fakeStatusBarView;

  // Forwards observer methods for all WebStates in the WebStateList to this
  // BrowserViewController object.
  std::unique_ptr<AllWebStateObservationForwarder>
      _allWebStateObservationForwarder;

  // Bridges C++ WebStateObserver methods to this BrowserViewController.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  std::unique_ptr<UrlLoadingObserverBridge> _URLLoadingObserverBridge;
}

// Activates/deactivates the object. This will enable/disable the ability for
// this object to browse, and to have live UIWebViews associated with it. While
// not active, the UI will not react to changes in the tab model, so generally
// an inactive BVC should not be visible.
@property(nonatomic, assign, getter=isActive) BOOL active;
// Browser container view controller.
@property(nonatomic, strong)
    BrowserContainerViewController* browserContainerViewController;
// Command dispatcher.
@property(nonatomic, weak) CommandDispatcher* commandDispatcher;
// The browser's side swipe controller.  Lazily instantiated on the first call.
@property(nonatomic, strong, readonly) SideSwipeController* sideSwipeController;
// The dialog presenter for this BVC's tab model.
@property(nonatomic, strong, readonly) DialogPresenter* dialogPresenter;
// The object that manages keyboard commands on behalf of the BVC.
@property(nonatomic, strong, readonly) KeyCommandsProvider* keyCommandsProvider;
// Helper method to check web controller canShowFindBar method.
@property(nonatomic, assign, readonly) BOOL canShowFindBar;
// Whether the controller's view is currently available.
// YES from viewWillAppear to viewWillDisappear.
@property(nonatomic, assign, getter=isVisible) BOOL visible;
// Whether the controller's view is currently visible.
// YES from viewDidAppear to viewWillDisappear.
@property(nonatomic, assign) BOOL viewVisible;
// Whether the controller should broadcast its UI.
@property(nonatomic, assign, getter=isBroadcasting) BOOL broadcasting;
// Whether the controller is currently dismissing a presented view controller.
@property(nonatomic, assign, getter=isDismissingModal) BOOL dismissingModal;
// Whether web usage is enabled for the WebStates in |self.tabModel|.
@property(nonatomic, assign, getter=isWebUsageEnabled) BOOL webUsageEnabled;
// Returns YES if the toolbar has not been scrolled out by fullscreen.
@property(nonatomic, assign, readonly, getter=isToolbarOnScreen)
    BOOL toolbarOnScreen;
// Whether a new tab animation is occurring.
@property(nonatomic, assign, getter=isInNewTabAnimation) BOOL inNewTabAnimation;
// Whether BVC prefers to hide the status bar. This value is used to determine
// the response from the |prefersStatusBarHidden| method.
@property(nonatomic, assign) BOOL hideStatusBar;
// Coordinator for displaying a modal overlay with activity indicator to prevent
// the user from interacting with the browser view.
@property(nonatomic, strong)
    ActivityOverlayCoordinator* activityOverlayCoordinator;
// A block to be run when the |tabWasAdded:| method completes the animation
// for the presentation of a new tab. Can be used to record performance metrics.
@property(nonatomic, strong, nullable)
    ProceduralBlock foregroundTabWasAddedCompletionBlock;
// Coordinator for tablet tab strip.
@property(nonatomic, strong) TabStripLegacyCoordinator* tabStripCoordinator;
// Coordinator for Infobars.
@property(nonatomic, strong)
    InfobarContainerCoordinator* infobarContainerCoordinator;
// A weak reference to the view of the tab strip on tablet.
@property(nonatomic, weak) UIView* tabStripView;
// Helper for saving images.
@property(nonatomic, strong) ImageSaver* imageSaver;
// Helper for copying images.
@property(nonatomic, strong) ImageCopier* imageCopier;
// Helper for the bvc.
@property(nonatomic, strong) BrowserViewControllerHelper* helper;

// The user agent type used to load the currently visible page. User agent
// type is NONE if there is no visible page or visible page is a native
// page.
@property(nonatomic, assign, readonly) web::UserAgentType userAgentType;

// Returns the header views, all the chrome on top of the page, including the
// ones that cannot be scrolled off screen by full screen.
@property(nonatomic, strong, readonly) NSArray<HeaderDefinition*>* headerViews;

// Coordinator for the popup menus.
@property(nonatomic, strong) PopupMenuCoordinator* popupMenuCoordinator;

@property(nonatomic, strong) BubblePresenter* bubblePresenter;

// Primary toolbar.
@property(nonatomic, strong)
    PrimaryToolbarCoordinator* primaryToolbarCoordinator;
// Secondary toolbar.
@property(nonatomic, strong)
    AdaptiveToolbarCoordinator* secondaryToolbarCoordinator;
// The container view for the secondary toolbar.
// TODO(crbug.com/880656): Convert to a container coordinator.
@property(nonatomic, strong) UIView* secondaryToolbarContainerView;
// Coordinator used to manage the secondary toolbar view.
@property(nonatomic, strong)
    ToolbarContainerCoordinator* secondaryToolbarContainerCoordinator;
// Interface object with the toolbars.
@property(nonatomic, strong) id<ToolbarCoordinating> toolbarInterface;

// Vertical offset for the primary toolbar, used for fullscreen.
@property(nonatomic, strong) NSLayoutConstraint* primaryToolbarOffsetConstraint;
// Height constraint for the primary toolbar.
@property(nonatomic, strong) NSLayoutConstraint* primaryToolbarHeightConstraint;
// Height constraint for the secondary toolbar.
@property(nonatomic, strong)
    NSLayoutConstraint* secondaryToolbarHeightConstraint;
// Height constraint for the frame the secondary toolbar would have if
// fullscreen was disabled.
@property(nonatomic, strong)
    NSLayoutConstraint* secondaryToolbarNoFullscreenHeightConstraint;
// Current Fullscreen progress for the footers.
@property(nonatomic, assign) CGFloat footerFullscreenProgress;
// Y-dimension offset for placement of the header.
@property(nonatomic, readonly) CGFloat headerOffset;
// Height of the header view.
@property(nonatomic, readonly) CGFloat headerHeight;

// The webState of the active tab.
@property(nonatomic, readonly) web::WebState* currentWebState;

// Whether the browser container should be the full bounds of self.view.
@property(nonatomic, readonly) BOOL usesFullscreenContainer;
// Whether the safe area insets should be used to adjust the viewport.
@property(nonatomic, readonly) BOOL usesSafeInsetsForViewportAdjustments;

// BVC initialization
// ------------------
// If the BVC is initialized with a valid browser state & tab model immediately,
// the path is straightforward: functionality is enabled, and the UI is built
// when -viewDidLoad is called.
// If the BVC is initialized without a browser state or tab model, the tab model
// and browser state may or may not be provided before -viewDidLoad is called.
// In most cases, they will not, to improve startup performance.
// In order to handle this, initialization of various aspects of BVC have been
// broken out into the following functions, which have expectations (enforced
// with DCHECKs) regarding |_browserState|, |self.tabModel|, and [self
// isViewLoaded].

// Updates non-view-related functionality with the given browser state and tab
// model.
// Does not matter whether or not the view has been loaded.
- (void)updateWithTabModel:(TabModel*)model
              browserState:(ios::ChromeBrowserState*)browserState;
// On iOS7, iPad should match iOS6 status bar.  Install a simple black bar under
// the status bar to mimic this layout.
- (void)installFakeStatusBar;
// Builds the UI parts of tab strip and the toolbar.  Does not matter whether
// or not browser state and tab model are valid.
- (void)buildToolbarAndTabStrip;
// Sets up the constraints on the toolbar.
- (void)addConstraintsToToolbar;
// Updates view-related functionality with the given tab model and browser
// state. The view must have been loaded.  Uses |_browserState| and
// |self.tabModel|.
- (void)addUIFunctionalityForModelAndBrowserState;
// Sets the correct frame and hierarchy for subviews and helper views.  Only
// insert views on |initialLayout|.
- (void)setUpViewLayout:(BOOL)initialLayout;
// Makes |tab| the currently visible tab, displaying its view.
- (void)displayTab:(Tab*)tab;
// Initializes the bookmark interaction controller if not already initialized.
- (void)initializeBookmarkInteractionController;
// Installs the BVC as overscroll actions controller of |nativeContent| if
// needed. Sets the style of the overscroll actions toolbar.
- (void)setOverScrollActionControllerToStaticNativeContent:
    (StaticHtmlNativeContent*)nativeContent;

// UI Configuration, update and Layout
// -----------------------------------
// Updates the toolbar display based on the current tab.
- (void)updateToolbar;
// Starts or stops broadcasting the toolbar UI and main content UI depending on
// whether the BVC is visible and active.
- (void)updateBroadcastState;
// Updates |dialogPresenter|'s |active| property to account for the BVC's
// |active|, |visible|, and |inNewTabAnimation| properties.
- (void)updateDialogPresenterActiveState;
// Dismisses popups and modal dialogs that are displayed above the BVC upon size
// changes (e.g. rotation, resizing,…) or when the accessibility escape gesture
// is performed.
// TODO(crbug.com/522721): Support size changes for all popups and modal
// dialogs.
- (void)dismissPopups;
// Returns the footer view if one exists (e.g. the voice search bar).
- (UIView*)footerView;
// Returns the appropriate frame for the NTP.
// TODO(crbug.com/826369): Most of this method's implementation details can be
// unwound when BVC fullscreen and NTP experiments are enabled by default.
- (CGRect)ntpFrameForWebState:(web::WebState*)webState;
// Returns web contents frame without including primary toolbar.
- (CGRect)visibleFrameForTab:(Tab*)tab;
// Sets the frame for the headers.
- (void)setFramesForHeaders:(NSArray<HeaderDefinition*>*)headers
                   atOffset:(CGFloat)headerOffset;

// Find Bar UI
// -----------
// Update find bar with model data. If |shouldFocus| is set to YES, the text
// field will become first responder.
- (void)updateFindBar:(BOOL)initialUpdate shouldFocus:(BOOL)shouldFocus;
// Hide find bar.
- (void)hideFindBarWithAnimation:(BOOL)animate;
// Shows find bar. If |selectText| is YES, all text inside the Find Bar
// textfield will be selected. If |shouldFocus| is set to YES, the textfield is
// set to be first responder.
- (void)showFindBarWithAnimation:(BOOL)animate
                      selectText:(BOOL)selectText
                     shouldFocus:(BOOL)shouldFocus;

// Alerts
// ------
// Shows a self-dismissing snackbar displaying |message|.
- (void)showSnackbar:(NSString*)message;
// Shows an alert dialog with |title| and |message|.
- (void)showErrorAlertWithStringTitle:(NSString*)title
                              message:(NSString*)message;

// Tap Handling
// ------------
// Record the last tap point based on the |originPoint| (if any) passed in
// command.
- (void)setLastTapPointFromCommand:(CGPoint)originPoint;
// Returns the last stored |_lastTapPoint| if it's been set within the past
// second.
- (CGPoint)lastTapPoint;
// Store the tap CGPoint in |_lastTapPoint| and the current timestamp.
- (void)saveContentAreaTapLocation:(UIGestureRecognizer*)gestureRecognizer;

// Tab creation and selection
// --------------------------
// Whether the given tab's URL is an application specific URL.
- (BOOL)isTabNativePage:(Tab*)tab;
// Add all delegates to the provided |tab|.
- (void)installDelegatesForTab:(Tab*)tab;
// Remove delegates from the provided |tab|.
- (void)uninstallDelegatesForTab:(Tab*)tab;
// Called when a tab is selected in the model. Make any required view changes.
// The notification will not be sent when the tab is already the selected tab.
// |notifyToolbar| indicates whether the toolbar is notified that the tab has
// changed.
- (void)tabSelected:(Tab*)tab notifyToolbar:(BOOL)notifyToolbar;
// Returns the native controller being used by |tab|'s web controller.
- (id)nativeControllerForTab:(Tab*)tab;

// Voice Search
// ------------
// Lazily instantiates |_voiceSearchController|.
- (void)ensureVoiceSearchControllerCreated;

// Reading List
// ------------
// Adds the given url to the reading list.
- (void)addToReadingListURL:(const GURL&)URL title:(NSString*)title;

@end

@implementation BrowserViewController

#pragma mark - Object lifecycle

- (instancetype)initWithTabModel:(TabModel*)model
                      browserState:(ios::ChromeBrowserState*)browserState
                 dependencyFactory:
                     (BrowserViewControllerDependencyFactory*)factory
        applicationCommandEndpoint:
            (id<ApplicationCommands>)applicationCommandEndpoint
                 commandDispatcher:(CommandDispatcher*)commandDispatcher
    browserContainerViewController:
        (BrowserContainerViewController*)browserContainerViewController {
  self = [super initWithNibName:nil bundle:base::mac::FrameworkBundle()];
  if (self) {
    DCHECK(factory);

    _browserContainerViewController = browserContainerViewController;
    _dependencyFactory = factory;
    _dialogPresenter = [[DialogPresenter alloc] initWithDelegate:self
                                        presentingViewController:self];
    self.commandDispatcher = commandDispatcher;
    [self.commandDispatcher
        startDispatchingToTarget:self
                     forProtocol:@protocol(BrowserCommands)];
    [self.commandDispatcher
        startDispatchingToTarget:applicationCommandEndpoint
                     forProtocol:@protocol(ApplicationCommands)];
    // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
    // passed protocol conforms to, so ApplicationSettingsCommands is explicitly
    // dispatched to the endpoint as well. Since this is potentially
    // fragile, DCHECK that it should still work (if the endpoint is nonnull).
    DCHECK(!applicationCommandEndpoint ||
           [applicationCommandEndpoint
               conformsToProtocol:@protocol(ApplicationSettingsCommands)]);
    [self.commandDispatcher
        startDispatchingToTarget:applicationCommandEndpoint
                     forProtocol:@protocol(ApplicationSettingsCommands)];
    // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
    // passed protocol conforms to, so BrowsingDataCommands is explicitly
    // dispatched to the endpoint as well. Since this is potentially
    // fragile, DCHECK that it should still work (if the endpoint is nonnull).
    DCHECK(!applicationCommandEndpoint ||
           [applicationCommandEndpoint
               conformsToProtocol:@protocol(BrowsingDataCommands)]);
    [self.commandDispatcher
        startDispatchingToTarget:applicationCommandEndpoint
                     forProtocol:@protocol(BrowsingDataCommands)];
    _toolbarCoordinatorAdaptor =
        [[ToolbarCoordinatorAdaptor alloc] initWithDispatcher:self.dispatcher];
    self.toolbarInterface = _toolbarCoordinatorAdaptor;

    _downloadManagerCoordinator = [[DownloadManagerCoordinator alloc]
        initWithBaseViewController:_browserContainerViewController];
    _downloadManagerCoordinator.presenter =
        [[VerticalAnimationContainer alloc] init];

    _javaScriptDialogPresenter.reset(
        new JavaScriptDialogPresenterImpl(_dialogPresenter));
    _webStateDelegate.reset(new web::WebStateDelegateBridge(self));
    _inNewTabAnimation = NO;

    _footerFullscreenProgress = 1.0;

    if (model && browserState)
      [self updateWithTabModel:model browserState:browserState];
  }
  return self;
}

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  DCHECK(_isShutdown) << "-shutdown must be called before dealloc.";
}

#pragma mark - Public Properties

- (id<ApplicationCommands,
      BrowserCommands,
      OmniboxFocuser,
      PopupMenuCommands,
      FakeboxFocuser,
      SnackbarCommands,
      ToolbarCommands>)dispatcher {
  return static_cast<
      id<ApplicationCommands, BrowserCommands, OmniboxFocuser,
         PopupMenuCommands, FakeboxFocuser, SnackbarCommands, ToolbarCommands>>(
      self.commandDispatcher);
}

- (UIView*)contentArea {
  return self.browserContainerViewController.view;
}

- (BOOL)isPlayingTTS {
  return _voiceSearchController && _voiceSearchController->IsPlayingAudio();
}

- (TabModel*)tabModel {
  return _strongTabModel;
}

- (ios::ChromeBrowserState*)browserState {
  return _browserState;
}

#pragma mark - Private Properties

- (SideSwipeController*)sideSwipeController {
  if (!_sideSwipeController) {
    _sideSwipeController =
        [[SideSwipeController alloc] initWithTabModel:self.tabModel
                                         browserState:_browserState];
    [_sideSwipeController setSnapshotDelegate:self];
    _sideSwipeController.toolbarInteractionHandler = self.toolbarInterface;
    _sideSwipeController.primaryToolbarSnapshotProvider =
        self.primaryToolbarCoordinator;
    _sideSwipeController.secondaryToolbarSnapshotProvider =
        self.secondaryToolbarCoordinator;
    [_sideSwipeController setSwipeDelegate:self];
    [_sideSwipeController setTabStripDelegate:self.tabStripCoordinator];
  }
  return _sideSwipeController;
}

- (DialogPresenter*)dialogPresenter {
  return _dialogPresenter;
}

- (KeyCommandsProvider*)keyCommandsProvider {
  if (!_keyCommandsProvider) {
    _keyCommandsProvider = [_dependencyFactory newKeyCommandsProvider];
  }
  return _keyCommandsProvider;
}

- (BOOL)canShowFindBar {
  // Make sure web controller can handle find in page.
  Tab* tab = [self.tabModel currentTab];
  if (!tab) {
    return NO;
  }

  auto* helper = FindTabHelper::FromWebState(tab.webState);
  return (helper && helper->CurrentPageSupportsFindInPage() &&
          !helper->IsFindUIActive());
}

- (BOOL)canShowTabStrip {
  return IsRegularXRegularSizeClass(self);
}

- (web::UserAgentType)userAgentType {
  web::WebState* webState = [self.tabModel currentTab].webState;
  if (!webState)
    return web::UserAgentType::NONE;
  web::NavigationItem* visibleItem =
      webState->GetNavigationManager()->GetVisibleItem();
  if (!visibleItem)
    return web::UserAgentType::NONE;

  return visibleItem->GetUserAgentType();
}

- (void)setVisible:(BOOL)visible {
  if (_visible == visible)
    return;

  _visible = visible;
}

- (void)setViewVisible:(BOOL)viewVisible {
  if (_viewVisible == viewVisible)
    return;
  _viewVisible = viewVisible;
  self.visible = viewVisible;
  [self updateDialogPresenterActiveState];
  [self updateBroadcastState];
}

- (void)setBroadcasting:(BOOL)broadcasting {
  if (_broadcasting == broadcasting)
    return;
  _broadcasting = broadcasting;
  // TODO(crbug.com/790886): Use the Browser's broadcaster once Browsers are
  // supported.
  FullscreenController* fullscreenController =
      FullscreenControllerFactory::GetInstance()->GetForBrowserState(
          _browserState);
  ChromeBroadcaster* broadcaster = fullscreenController->broadcaster();
  if (_broadcasting) {
    fullscreenController->SetWebStateList(self.tabModel.webStateList);

    _toolbarUIUpdater = [[LegacyToolbarUIUpdater alloc]
        initWithToolbarUI:[[ToolbarUIState alloc] init]
             toolbarOwner:self
             webStateList:self.tabModel.webStateList];
    [_toolbarUIUpdater startUpdating];
    StartBroadcastingToolbarUI(_toolbarUIUpdater.toolbarUI, broadcaster);

    _mainContentUIUpdater = [[MainContentUIStateUpdater alloc]
        initWithState:[[MainContentUIState alloc] init]];
    _webMainContentUIForwarder = [[WebScrollViewMainContentUIForwarder alloc]
        initWithUpdater:_mainContentUIUpdater
           webStateList:self.tabModel.webStateList];
    StartBroadcastingMainContentUI(self, broadcaster);

    _fullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(self);
    fullscreenController->AddObserver(_fullscreenUIUpdater.get());
    [self updateForFullscreenProgress:fullscreenController->GetProgress()];
  } else {
    StopBroadcastingToolbarUI(broadcaster);
    StopBroadcastingMainContentUI(broadcaster);
    [_toolbarUIUpdater stopUpdating];
    _toolbarUIUpdater = nil;
    _mainContentUIUpdater = nil;
    [_webMainContentUIForwarder disconnect];
    _webMainContentUIForwarder = nil;

    fullscreenController->RemoveObserver(_fullscreenUIUpdater.get());
    _fullscreenUIUpdater = nullptr;

    fullscreenController->SetWebStateList(nullptr);
  }
}

- (BOOL)isWebUsageEnabled {
  return _browserState && !_isShutdown &&
         WebStateListWebUsageEnablerFactory::GetInstance()
             ->GetForBrowserState(_browserState)
             ->IsWebUsageEnabled();
}

- (void)setWebUsageEnabled:(BOOL)webUsageEnabled {
  if (!_browserState || _isShutdown)
    return;
  WebStateListWebUsageEnablerFactory::GetInstance()
      ->GetForBrowserState(_browserState)
      ->SetWebUsageEnabled(webUsageEnabled);
}

- (BOOL)isToolbarOnScreen {
  return [self nonFullscreenToolbarHeight] - [self currentHeaderOffset] > 0;
}

- (void)setInNewTabAnimation:(BOOL)inNewTabAnimation {
  if (_inNewTabAnimation == inNewTabAnimation)
    return;
  _inNewTabAnimation = inNewTabAnimation;
  [self updateDialogPresenterActiveState];
  [self updateBroadcastState];
}

- (BOOL)isInNewTabAnimation {
  return _inNewTabAnimation;
}

- (void)setHideStatusBar:(BOOL)hideStatusBar {
  if (_hideStatusBar == hideStatusBar)
    return;
  _hideStatusBar = hideStatusBar;
  [self setNeedsStatusBarAppearanceUpdate];
}

- (NSArray<HeaderDefinition*>*)headerViews {
  NSMutableArray<HeaderDefinition*>* results = [[NSMutableArray alloc] init];
  if (![self isViewLoaded])
    return results;

  if (![self canShowTabStrip]) {
    if (self.primaryToolbarCoordinator.viewController.view) {
      [results addObject:[HeaderDefinition
                             definitionWithView:self.primaryToolbarCoordinator
                                                    .viewController.view
                                headerBehaviour:Hideable]];
    }
  } else {
    if (self.tabStripView) {
      [results addObject:[HeaderDefinition definitionWithView:self.tabStripView
                                              headerBehaviour:Hideable]];
    }
    if (self.primaryToolbarCoordinator.viewController.view) {
      [results addObject:[HeaderDefinition
                             definitionWithView:self.primaryToolbarCoordinator
                                                    .viewController.view
                                headerBehaviour:Hideable]];
    }
    if ([_findBarController view]) {
      [results addObject:[HeaderDefinition
                             definitionWithView:[_findBarController view]
                                headerBehaviour:Overlap]];
    }
  }
  return [results copy];
}

- (CGFloat)headerOffset {
  CGFloat headerOffset = 0;
  headerOffset = self.view.safeAreaInsets.top;
  return [self canShowTabStrip] ? headerOffset : 0.0;
}

- (CGFloat)headerHeight {
  NSArray<HeaderDefinition*>* views = [self headerViews];

  CGFloat height = self.headerOffset;
  for (HeaderDefinition* header in views) {
    if (header.view && header.behaviour == Hideable) {
      height += CGRectGetHeight([header.view frame]);
    }
  }

  CGFloat statusBarOffset = 0;
  if (!self.usesFullscreenContainer) {
    statusBarOffset = StatusBarHeight();
  }
  return height - statusBarOffset;
}

- (web::WebState*)currentWebState {
  return self.tabModel.currentTab.webState;
}

- (BOOL)usesFullscreenContainer {
  return base::FeatureList::IsEnabled(
             web::features::kBrowserContainerFullscreen) ||
         self.usesSafeInsetsForViewportAdjustments;
}

- (BOOL)usesSafeInsetsForViewportAdjustments {
  fullscreen::features::ViewportAdjustmentExperiment viewportExperiment =
      fullscreen::features::GetActiveViewportExperiment();
  return viewportExperiment ==
             fullscreen::features::ViewportAdjustmentExperiment::SAFE_AREA ||
         viewportExperiment ==
             fullscreen::features::ViewportAdjustmentExperiment::HYBRID;
}

- (BubblePresenter*)bubblePresenter {
  if (!_bubblePresenter) {
    _bubblePresenter =
        [[BubblePresenter alloc] initWithBrowserState:self.browserState
                                             delegate:self
                                   rootViewController:self];
    _bubblePresenter.dispatcher = self.dispatcher;
    self.popupMenuCoordinator.bubblePresenter = _bubblePresenter;
  }
  return _bubblePresenter;
}

#pragma mark - Public methods

- (void)setPrimary:(BOOL)primary {
  [self.tabModel setPrimary:primary];
  if (primary) {
    [self updateDialogPresenterActiveState];
    [self updateBroadcastState];
  } else {
    self.dialogPresenter.active = false;
  }
}

- (void)shieldWasTapped:(id)sender {
  [self.dispatcher cancelOmniboxEdit];
}

- (void)userEnteredTabSwitcher {
  [self.bubblePresenter userEnteredTabSwitcher];
}

- (void)presentBubblesIfEligible {
  [self.bubblePresenter presentBubblesIfEligible];
}

- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox {
  NSTimeInterval startTime = [NSDate timeIntervalSinceReferenceDate];
  BOOL offTheRecord = self.isOffTheRecord;
  ProceduralBlock oldForegroundTabWasAddedCompletionBlock =
      self.foregroundTabWasAddedCompletionBlock;
  __weak BrowserViewController* weakSelf = self;
  self.foregroundTabWasAddedCompletionBlock = ^{
    if (oldForegroundTabWasAddedCompletionBlock) {
      oldForegroundTabWasAddedCompletionBlock();
    }
    double duration = [NSDate timeIntervalSinceReferenceDate] - startTime;
    base::TimeDelta timeDelta = base::TimeDelta::FromSecondsD(duration);
    if (offTheRecord) {
      UMA_HISTOGRAM_TIMES("Toolbar.Menu.NewIncognitoTabPresentationDuration",
                          timeDelta);
    } else {
      UMA_HISTOGRAM_TIMES("Toolbar.Menu.NewTabPresentationDuration", timeDelta);
    }
    if (focusOmnibox) {
      [weakSelf.dispatcher focusOmnibox];
    }
  };

  [self setLastTapPointFromCommand:originPoint];
  // The new tab can be opened before BVC has been made visible onscreen.  Test
  // for this case by checking if the parent container VC is currently in the
  // process of being presented.
  DCHECK(self.visible || self.dismissingModal ||
         self.parentViewController.isBeingPresented);

  // In most cases, we want to take a snapshot of the current tab before opening
  // a new tab. However, if the current tab is not fully visible (did not finish
  // |-viewDidAppear:|, then we must not take an empty snapshot, replacing an
  // existing snapshot for the tab. This can happen when a new regular tab is
  // opened from an incognito tab. A different BVC is displayed, which may not
  // have enough time to finish appearing before a snapshot is requested.
  if (self.currentWebState && self.viewVisible) {
    SnapshotTabHelper::FromWebState(self.currentWebState)
        ->UpdateSnapshotWithCallback(nil);
  }

  [self.tabModel
      insertTabWithLoadParams:web_navigation_util::CreateWebLoadParams(
                                  GURL(kChromeUINewTabURL),
                                  ui::PAGE_TRANSITION_TYPED, nullptr)
                       opener:nil
                  openedByDOM:NO
                      atIndex:self.tabModel.count
                 inBackground:NO];
}

- (void)appendTabAddedCompletion:(ProceduralBlock)tabAddedCompletion {
  if (tabAddedCompletion) {
    if (self.foregroundTabWasAddedCompletionBlock) {
      ProceduralBlock oldForegroundTabWasAddedCompletionBlock =
          self.foregroundTabWasAddedCompletionBlock;
      self.foregroundTabWasAddedCompletionBlock = ^{
        oldForegroundTabWasAddedCompletionBlock();
        tabAddedCompletion();
      };
    } else {
      self.foregroundTabWasAddedCompletionBlock = tabAddedCompletion;
    }
  }
}

- (void)expectNewForegroundTab {
  _expectingForegroundTab = YES;
}

- (void)startVoiceSearch {
  // Delay Voice Search until new tab animations have finished.
  if (self.inNewTabAnimation) {
    _startVoiceSearchAfterNewTabAnimation = YES;
    return;
  }

  // Keyboard shouldn't overlay the ecoutez window, so dismiss find in page and
  // dismiss the keyboard.
  [self closeFindInPage];
  [[self viewForTab:self.tabModel.currentTab] endEditing:NO];

  // Ensure that voice search objects are created.
  [self ensureVoiceSearchControllerCreated];

  // Present voice search.
  _voiceSearchController->StartRecognition(self, self.tabModel.currentTab);
  [self.dispatcher cancelOmniboxEdit];
}

#pragma mark - browser_view_controller+private.h

- (void)setActive:(BOOL)active {
  if (_active == active) {
    return;
  }
  _active = active;

  // If not active, display an activity indicator overlay over the view to
  // prevent interaction with the web page.
  // TODO(crbug.com/637093): This coordinator should be managed by the
  // coordinator used to present BrowserViewController, when implemented.
  if (active) {
    [self.activityOverlayCoordinator stop];
    self.activityOverlayCoordinator = nil;
  } else if (!self.activityOverlayCoordinator) {
    self.activityOverlayCoordinator =
        [[ActivityOverlayCoordinator alloc] initWithBaseViewController:self];
    [self.activityOverlayCoordinator start];
  }

  if (_browserState) {
    ActiveStateManager* active_state_manager =
        ActiveStateManager::FromBrowserState(_browserState);
    active_state_manager->SetActive(active);

    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForBrowserState(_browserState)
        ->SetEnabled(_active);
  }

  self.webUsageEnabled = active;
  [self updateDialogPresenterActiveState];
  [self updateBroadcastState];

  // Stop the NTP on web usage toggle. This happens when clearing browser
  // data, and forces the NTP to be recreated in -displayTab below.
  // TODO(crbug.com/906199): Move this to the NewTabPageTabHelper when
  // WebStateObserver has a webUsage callback.
  if (!active) {
    for (const auto& element : _ntpCoordinatorsForWebStates)
      [element.second stop];
  }

  if (active) {
    // Make sure the tab (if any; it's possible to get here without a current
    // tab if the caller is about to create one) ends up on screen completely.
    // Force loading the view in case it was not loaded yet.
    [self loadViewIfNeeded];
    if (self.currentWebState && _expectingForegroundTab) {
      PagePlaceholderTabHelper::FromWebState(self.currentWebState)
          ->AddPlaceholderForNextNavigation();
    }
    Tab* currentTab = self.tabModel.currentTab;
    if (currentTab)
      [self displayTab:currentTab];
  } else {
    [_dialogPresenter cancelAllDialogs];
  }
  [_paymentRequestManager enablePaymentRequest:active];

  [self setNeedsStatusBarAppearanceUpdate];
}

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [_activityServiceCoordinator cancelShare];
  [_bookmarkInteractionController dismissBookmarkModalControllerAnimated:NO];
  [_bookmarkInteractionController dismissSnackbar];
  if (dismissOmnibox) {
    [self.dispatcher cancelOmniboxEdit];
  }
  [_dialogPresenter cancelAllDialogs];
  [self.dispatcher hidePageInfo];
  [self.bubblePresenter dismissBubbles];
  if (_voiceSearchController)
    _voiceSearchController->DismissMicPermissionsHelp();

  web::WebState* webState = self.currentWebState;
  [self.tabModel.currentTab dismissModals];

  if (webState) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(webState);
    if (NTPHelper && NTPHelper->IsActive()) {
      [_ntpCoordinatorsForWebStates[webState] dismissModals];
    }
    auto* findHelper = FindTabHelper::FromWebState(webState);
    if (findHelper) {
      findHelper->StopFinding(^{
        [self updateFindBar:NO shouldFocus:NO];
      });
    }
  }

  [_paymentRequestManager cancelRequest];
  [self.dispatcher dismissPopupMenuAnimated:NO];
  [_contextMenuCoordinator stop];

  if (self.presentedViewController) {
    // Dismisses any other modal controllers that may be present, e.g. Recent
    // Tabs.
    //
    // Note that currently, some controllers like the bookmark ones were already
    // dismissed (in this example in -dismissBookmarkModalControllerAnimated:),
    // but are still reported as the presentedViewController.  Calling
    // |dismissViewControllerAnimated:completion:| again would dismiss the BVC
    // itself, so instead check the value of |self.dismissingModal| and only
    // call dismiss if one of the above calls has not already triggered a
    // dismissal.
    //
    // To ensure the completion is called, nil is passed to the call to dismiss,
    // and the completion is called explicitly below.
    if (!self.dismissingModal) {
      [self dismissViewControllerAnimated:NO completion:nil];
    }
    // Dismissed controllers will be so after a delay. Queue the completion
    // callback after that.
    if (completion) {
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.4 * NSEC_PER_SEC)),
          dispatch_get_main_queue(), ^{
            completion();
          });
    }
  } else if (completion) {
    // If no view controllers are presented, we should be ok with dispatching
    // the completion block directly.
    dispatch_async(dispatch_get_main_queue(), completion);
  }
}

- (void)animateOpenBackgroundTabFromOriginPoint:(CGPoint)originPoint
                                     completion:(void (^)())completion {
  if ([self canShowTabStrip] || CGPointEqualToPoint(originPoint, CGPointZero)) {
    completion();
  } else {
    self.inNewTabAnimation = YES;
    // Exit fullscreen if needed.
    FullscreenControllerFactory::GetInstance()
        ->GetForBrowserState(_browserState)
        ->ExitFullscreen();
    const CGFloat kAnimatedViewSize = 50;
    BackgroundTabAnimationView* animatedView =
        [[BackgroundTabAnimationView alloc]
            initWithFrame:CGRectMake(0, 0, kAnimatedViewSize,
                                     kAnimatedViewSize)];
    __weak UIView* weakAnimatedView = animatedView;
    auto completionBlock = ^() {
      self.inNewTabAnimation = NO;
      [weakAnimatedView removeFromSuperview];
      completion();
    };
    [self.view addSubview:animatedView];
    [animatedView animateFrom:originPoint
        toTabGridButtonWithCompletion:completionBlock];
  }
}

- (void)shutdown {
  DCHECK(!_isShutdown);
  _isShutdown = YES;

  [self setActive:NO];
  [_paymentRequestManager close];
  _paymentRequestManager = nil;

  if (_browserState) {
    TextToSpeechPlaybackController* controller =
        TextToSpeechPlaybackControllerFactory::GetInstance()
            ->GetForBrowserState(_browserState);
    if (controller)
      controller->SetWebStateList(nullptr);

    WebStateListWebUsageEnabler* webUsageEnabler =
        WebStateListWebUsageEnablerFactory::GetInstance()->GetForBrowserState(
            _browserState);
    if (webUsageEnabler)
      webUsageEnabler->SetWebStateList(nullptr);

    UrlLoadingNotifier* urlLoadingNotifier =
        UrlLoadingNotifierFactory::GetForBrowserState(_browserState);
    if (urlLoadingNotifier)
      urlLoadingNotifier->RemoveObserver(_URLLoadingObserverBridge.get());
  }

  // Disconnect child coordinators.
  [_activityServiceCoordinator disconnect];
  [self.popupMenuCoordinator stop];
  [self.tabStripCoordinator stop];
  self.tabStripCoordinator = nil;
  self.tabStripView = nil;

  _browserState = nullptr;
  [self.commandDispatcher stopDispatchingToTarget:self];
  self.commandDispatcher = nil;

  [self.tabStripCoordinator stop];
  self.tabStripCoordinator = nil;
  [self.primaryToolbarCoordinator stop];
  self.primaryToolbarCoordinator = nil;
  [self.secondaryToolbarContainerCoordinator stop];
  self.secondaryToolbarContainerCoordinator = nil;
  [self.secondaryToolbarCoordinator stop];
  self.secondaryToolbarCoordinator = nil;
  [_downloadManagerCoordinator stop];
  _downloadManagerCoordinator = nil;
  self.toolbarInterface = nil;
  self.tabStripView = nil;
  [self.infobarContainerCoordinator stop];
  self.infobarContainerCoordinator = nil;
  // SideSwipeController is a tab model observer, so it needs to stop observing
  // before self.tabModel is released.
  _sideSwipeController = nil;
  [self.tabModel removeObserver:self];
  _allWebStateObservationForwarder = nullptr;
  if (_voiceSearchController)
    _voiceSearchController->SetDispatcher(nil);
  [_paymentRequestManager setActiveWebState:nullptr];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - NSObject

- (BOOL)accessibilityPerformEscape {
  [self dismissPopups];
  return YES;
}

#pragma mark - UIResponder

- (NSArray*)keyCommands {
  if (![self shouldRegisterKeyboardCommands]) {
    return nil;
  }
  return [self.keyCommandsProvider
      keyCommandsForConsumer:self
          baseViewController:self
                  dispatcher:self.dispatcher
                 editingText:![self isFirstResponder]];
}

#pragma mark - UIResponder helpers

// Whether the BVC should declare keyboard commands.
- (BOOL)shouldRegisterKeyboardCommands {
  if ([self presentedViewController])
    return NO;

  if (_voiceSearchController && _voiceSearchController->IsVisible())
    return NO;

  // If there is no first responder, try to make the webview the first
  // responder.
  if (!GetFirstResponder()) {
    if (self.currentWebState)
      [self.currentWebState->GetWebViewProxy() becomeFirstResponder];
  }

  return YES;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  CGRect initialViewsRect = self.view.bounds;
  if (!self.usesFullscreenContainer) {
    initialViewsRect.origin.y += StatusBarHeight();
    initialViewsRect.size.height -= StatusBarHeight();
  }
  UIViewAutoresizing initialViewAutoresizing =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  // Clip the content to the bounds of the view. This prevents the WebView to
  // overflow outside of the BVC, which is particularly visible during rotation.
  // The WebView is overflowing its bounds to be displayed below the toolbars.
  self.view.clipsToBounds = YES;

  self.contentArea.frame = initialViewsRect;
  self.typingShield = [[UIButton alloc] initWithFrame:initialViewsRect];
  self.typingShield.autoresizingMask = initialViewAutoresizing;
  self.typingShield.accessibilityIdentifier = @"Typing Shield";
  self.typingShield.accessibilityLabel = l10n_util::GetNSString(IDS_CANCEL);

  [self.typingShield addTarget:self
                        action:@selector(shieldWasTapped:)
              forControlEvents:UIControlEventTouchUpInside];
  self.view.autoresizingMask = initialViewAutoresizing;
  self.view.backgroundColor = [UIColor colorWithWhite:0.75 alpha:1.0];

  [self addChildViewController:self.browserContainerViewController];
  [self.view addSubview:self.contentArea];
  [self.browserContainerViewController didMoveToParentViewController:self];
  [self.view addSubview:self.typingShield];
  [super viewDidLoad];

  // Install fake status bar for iPad iOS7
  [self installFakeStatusBar];
  [self buildToolbarAndTabStrip];
  [self setUpViewLayout:YES];
  [self addConstraintsToToolbar];

  // If the tab model and browser state are valid, finish initialization.
  if (self.tabModel && _browserState)
    [self addUIFunctionalityForModelAndBrowserState];

  // Add a tap gesture recognizer to save the last tap location for the source
  // location of the new tab animation.
  UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(saveContentAreaTapLocation:)];
  [tapRecognizer setDelegate:self];
  [tapRecognizer setCancelsTouchesInView:NO];
  [self.contentArea addGestureRecognizer:tapRecognizer];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self setUpViewLayout:NO];
  // Update the heights of the toolbars to account for the new insets.
  self.primaryToolbarHeightConstraint.constant =
      [self primaryToolbarHeightWithInset];
  self.secondaryToolbarHeightConstraint.constant =
      [self secondaryToolbarHeightWithInset];
  self.secondaryToolbarNoFullscreenHeightConstraint.constant =
      [self secondaryToolbarHeightWithInset];

  // Native content pages depend on |self.view|'s safeArea.  If the BVC is
  // presented underneath another view (such as the first time welcome view),
  // the BVC has no safe area set during webController's layout initial, and
  // won't automatically get another layout without forcing it here.
  Tab* currentTab = self.tabModel.currentTab;
  if ([self isTabNativePage:currentTab]) {
    [currentTab.webController.view setNeedsLayout];
  }

  // Update the tab strip placement.
  if (self.tabStripView) {
    [self showTabStripView:self.tabStripView];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Update the toolbar height to account for |topLayoutGuide| changes.
  self.primaryToolbarHeightConstraint.constant =
      [self primaryToolbarHeightWithInset];

  if (self.currentWebState && self.webUsageEnabled) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(self.currentWebState);
    if (NTPHelper && NTPHelper->IsActive()) {
      _ntpCoordinatorsForWebStates[self.currentWebState]
          .viewController.view.frame =
          [self ntpFrameForWebState:self.currentWebState];
    }
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.viewVisible = YES;
  [self updateDialogPresenterActiveState];
  [self updateBroadcastState];
  [_toolbarUIUpdater updateState];

  // |viewDidAppear| can be called after |browserState| is destroyed. Since
  // |presentBubblesIfEligible| requires that |self.browserState| is not NULL,
  // check for |self.browserState| before calling the presenting the bubbles.
  if (self.browserState) {
    [self presentBubblesIfEligible];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  self.visible = YES;

  if (self.transitionCoordinator.animated) {
    // The transition coordinator is animated only when presented from the
    // TabGrid (when presented at the app start up, it is not animated). In that
    // case, display the Long Press InProductHelp if needed.
    auto completion =
        ^(id<UIViewControllerTransitionCoordinatorContext> context) {
          [self.bubblePresenter presentLongPressBubbleIfEligible];
        };

    [self.transitionCoordinator animateAlongsideTransition:nil
                                                completion:completion];
  }

  // If the controller is suspended, or has been paged out due to low memory,
  // updating the view will be handled when it's displayed again.
  if (!self.webUsageEnabled || !self.contentArea)
    return;
  // Update the displayed tab (if any; the switcher may not have created one
  // yet) in case it changed while showing the switcher.
  Tab* currentTab = self.tabModel.currentTab;
  if (currentTab)
    [self displayTab:currentTab];
}

- (void)viewWillDisappear:(BOOL)animated {
  self.viewVisible = NO;
  [self updateDialogPresenterActiveState];
  [self updateBroadcastState];
  web::WebState* activeWebState =
      self.tabModel.webStateList->GetActiveWebState();
  if (activeWebState)
    activeWebState->WasHidden();
  [_bookmarkInteractionController dismissSnackbar];
  [super viewWillDisappear:animated];
}

- (BOOL)prefersStatusBarHidden {
  return self.hideStatusBar || [super prefersStatusBarHidden];
}

// Called when in the foreground and the OS needs more memory. Release as much
// as possible.
- (void)didReceiveMemoryWarning {
  // Releases the view if it doesn't have a superview.
  [super didReceiveMemoryWarning];

  if (![self isViewLoaded]) {
    // Do not release |_infobarContainerCoordinator|, as this must have the same
    // lifecycle as the BrowserViewController.
    self.typingShield = nil;
    if (_voiceSearchController)
      _voiceSearchController->SetDispatcher(nil);
    self.primaryToolbarCoordinator = nil;
    self.secondaryToolbarContainerCoordinator = nil;
    self.secondaryToolbarCoordinator = nil;
    self.toolbarInterface = nil;
    [_toolbarUIUpdater stopUpdating];
    _toolbarUIUpdater = nil;
    _locationBarModelDelegate = nil;
    _locationBarModel = nil;
    self.helper = nil;
    [self.tabStripCoordinator stop];
    self.tabStripCoordinator = nil;
    self.tabStripView = nil;
    _sideSwipeController = nil;
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  FullscreenControllerFactory::GetInstance()
      ->GetForBrowserState(_browserState)
      ->BrowserTraitCollectionChangedBegin();

  // TODO(crbug.com/527092): - traitCollectionDidChange: is not always forwarded
  // because in some cases the presented view controller isn't a child of the
  // BVC in the view controller hierarchy (some intervening object isn't a
  // view controller).
  [self.presentedViewController
      traitCollectionDidChange:previousTraitCollection];
  // Change the height of the secondary toolbar to show/hide it.
  self.secondaryToolbarHeightConstraint.constant =
      [self secondaryToolbarHeightWithInset];
  self.secondaryToolbarNoFullscreenHeightConstraint.constant =
      [self secondaryToolbarHeightWithInset];
  [self updateFootersForFullscreenProgress:self.footerFullscreenProgress];
  if (!self.usesSafeInsetsForViewportAdjustments && self.currentWebState) {
    UIEdgeInsets contentPadding =
        self.currentWebState->GetWebViewProxy().contentInset;
    contentPadding.bottom = AlignValueToPixel(
        self.footerFullscreenProgress * [self secondaryToolbarHeightWithInset]);
    self.currentWebState->GetWebViewProxy().contentInset = contentPadding;
  }

  [_toolbarUIUpdater updateState];

  // If the device's size class has changed from RegularXRegular to another and
  // vice-versa, the find bar should switch between regular mode and compact
  // mode accordingly. Hide the findbar here and it will be reshown in [self
  // updateToobar];
  if (ShouldShowCompactToolbar(previousTraitCollection) !=
      ShouldShowCompactToolbar()) {
    [self hideFindBarWithAnimation:NO];
  }

  // Update the toolbar visibility.
  [self updateToolbar];

  // Update the tab strip visibility.
  if (self.tabStripView) {
    [self showTabStripView:self.tabStripView];
    [self.tabStripCoordinator hideTabStrip:![self canShowTabStrip]];
    _fakeStatusBarView.hidden = ![self canShowTabStrip];
    [self addConstraintsToPrimaryToolbar];
  }

  [self setNeedsStatusBarAppearanceUpdate];

  FullscreenControllerFactory::GetInstance()
      ->GetForBrowserState(_browserState)
      ->BrowserTraitCollectionChangedEnd();
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self dismissPopups];

  [coordinator animateAlongsideTransition:^(
                   id<UIViewControllerTransitionCoordinatorContext> context) {
    // Force updates of the toolbar updater as the toolbar height might
    // change on rotation.
    [_toolbarUIUpdater updateState];
  }
                               completion:nil];
}

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)())completion {
  if (!self.presentedViewController) {
    // TODO(crbug.com/801165): On iOS10, UIDocumentMenuViewController and
    // WKFileUploadPanel somehow combine to call dismiss twice instead of once.
    // The second call would dismiss the BVC itself, so look for that case and
    // return early.
    //
    // TODO(crbug.com/811671): A similar bug exists on all iOS versions with
    // WKFileUploadPanel and UIDocumentPickerViewController.
    //
    // To make M65 as safe as possible, return early whenever this method is
    // invoked but no VC appears to be presented.  These cases will always end
    // up dismissing the BVC itself, which would put the app into an
    // unresponsive state.
    return;
  }

  // Some calling code invokes |dismissViewControllerAnimated:completion:|
  // multiple times. Because the BVC is presented, subsequent calls end up
  // dismissing the BVC itself. This is never what should happen, so check for
  // this case and return early.  It is not enough to check
  // |self.dismissingModal| because some dismissals do not go through
  // -[BrowserViewController dismissViewControllerAnimated:completion:|.
  // TODO(crbug.com/782338): Fix callers and remove this early return.
  if (self.dismissingModal || self.presentedViewController.isBeingDismissed) {
    return;
  }

  self.dismissingModal = YES;
  __weak BrowserViewController* weakSelf = self;
  [super dismissViewControllerAnimated:flag
                            completion:^{
                              BrowserViewController* strongSelf = weakSelf;
                              strongSelf.dismissingModal = NO;
                              if (completion)
                                completion();
                              [strongSelf.dialogPresenter tryToPresent];
                            }];
}

- (void)presentViewController:(UIViewController*)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^)())completion {
  ProceduralBlock finalCompletionHandler = [completion copy];
  // TODO(crbug.com/580098) This is an interim fix for the flicker between the
  // launch screen and the FRE Animation. The fix is, if the FRE is about to be
  // presented, to show a temporary view of the launch screen and then remove it
  // when the controller for the FRE has been presented. This fix should be
  // removed when the FRE startup code is rewritten.
  BOOL firstRunLaunch = (FirstRun::IsChromeFirstRun() ||
                         experimental_flags::AlwaysDisplayFirstRun()) &&
                        !tests_hook::DisableFirstRun();
  // These if statements check that |presentViewController| is being called for
  // the FRE case.
  if (firstRunLaunch &&
      [viewControllerToPresent isKindOfClass:[UINavigationController class]]) {
    UINavigationController* navController =
        base::mac::ObjCCastStrict<UINavigationController>(
            viewControllerToPresent);
    if ([navController.topViewController
            isMemberOfClass:[WelcomeToChromeViewController class]]) {
      self.hideStatusBar = YES;

      // Load view from Launch Screen and add it to window.
      NSBundle* mainBundle = base::mac::FrameworkBundle();
      NSArray* topObjects =
          [mainBundle loadNibNamed:@"LaunchScreen" owner:self options:nil];
      UIViewController* launchScreenController =
          base::mac::ObjCCastStrict<UIViewController>([topObjects lastObject]);
      // |launchScreenView| is loaded as an autoreleased object, and is retained
      // by the |completion| block below.
      UIView* launchScreenView = launchScreenController.view;
      launchScreenView.userInteractionEnabled = NO;
      UIWindow* window = UIApplication.sharedApplication.keyWindow;
      launchScreenView.frame = window.bounds;
      [window addSubview:launchScreenView];

      // Replace the completion handler sent to the superclass with one which
      // removes |launchScreenView| and resets the status bar. If |completion|
      // exists, it is called from within the new completion handler.
      __weak BrowserViewController* weakSelf = self;
      finalCompletionHandler = ^{
        [launchScreenView removeFromSuperview];
        weakSelf.hideStatusBar = NO;
        if (completion)
          completion();
      };
    }
  }

  if ([self.sideSwipeController inSwipe]) {
    [self.sideSwipeController resetContentView];
  }

  [super presentViewController:viewControllerToPresent
                      animated:flag
                    completion:finalCompletionHandler];
}

- (BOOL)shouldAutorotate {
  if (self.presentedViewController.beingPresented ||
      self.presentedViewController.beingDismissed) {
    // Don't rotate while a presentation or dismissal animation is occurring.
    return NO;
  } else if (_sideSwipeController &&
             ![self.sideSwipeController shouldAutorotate]) {
    // Don't auto rotate if side swipe controller view says not to.
    return NO;
  } else {
    return [super shouldAutorotate];
  }
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  if ([self canShowTabStrip] && !_isOffTheRecord) {
    return self.tabStripView.frame.origin.y < kTabStripAppearanceOffset
               ? UIStatusBarStyleDefault
               : UIStatusBarStyleLightContent;
  }
  return _isOffTheRecord ? UIStatusBarStyleLightContent
                         : UIStatusBarStyleDefault;
}

#pragma mark - ** Private BVC Methods **

#pragma mark - Private Methods: BVC Initialization

- (void)updateWithTabModel:(TabModel*)model
              browserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(model);
  DCHECK(browserState);
  DCHECK(!self.tabModel);
  DCHECK(!_browserState);
  _browserState = browserState;
  _isOffTheRecord = browserState->IsOffTheRecord() ? YES : NO;
  _strongTabModel = model;
  WebStateListWebUsageEnablerFactory::GetInstance()
      ->GetForBrowserState(_browserState)
      ->SetWebStateList(self.tabModel.webStateList);

  [self.tabModel addObserver:self];
  _webStateObserverBridge = std::make_unique<web::WebStateObserverBridge>(self);
  _allWebStateObservationForwarder =
      std::make_unique<AllWebStateObservationForwarder>(
          self.tabModel.webStateList, _webStateObserverBridge.get());

  _URLLoadingObserverBridge = std::make_unique<UrlLoadingObserverBridge>(self);
  UrlLoadingNotifier* urlLoadingNotifier =
      UrlLoadingNotifierFactory::GetForBrowserState(_browserState);
  urlLoadingNotifier->AddObserver(_URLLoadingObserverBridge.get());

  NSUInteger count = self.tabModel.count;
  for (NSUInteger index = 0; index < count; ++index)
    [self installDelegatesForTab:[self.tabModel tabAtIndex:index]];

  self.imageSaver = [[ImageSaver alloc] initWithBaseViewController:self];
  self.imageCopier = [[ImageCopier alloc] initWithBaseViewController:self];

  // Set the TTS playback controller's WebStateList.
  TextToSpeechPlaybackControllerFactory::GetInstance()
      ->GetForBrowserState(_browserState)
      ->SetWebStateList(self.tabModel.webStateList);

  // When starting the browser with an open tab, it is necessary to reset the
  // clipsToBounds property of the WKWebView so the page can bleed behind the
  // toolbar.
  if (self.currentWebState) {
    self.currentWebState->GetWebViewProxy().scrollViewProxy.clipsToBounds = NO;
  }
}

- (void)installFakeStatusBar {
  CGRect statusBarFrame = CGRectMake(0, 0, CGRectGetWidth(self.view.bounds), 0);
  _fakeStatusBarView = [[UIView alloc] initWithFrame:statusBarFrame];
  [_fakeStatusBarView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  if (IsIPadIdiom()) {
    [_fakeStatusBarView setBackgroundColor:StatusBarBackgroundColor()];
    [_fakeStatusBarView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    [[self view] addSubview:_fakeStatusBarView];
  } else {
    // Add a white bar on phone so that the status bar on the NTP is white.
    [_fakeStatusBarView setBackgroundColor:ntp_home::kNTPBackgroundColor()];
    [self.view insertSubview:_fakeStatusBarView atIndex:0];
  }
}

// Create the UI elements.  May or may not have valid browser state & tab model.
- (void)buildToolbarAndTabStrip {
  DCHECK([self isViewLoaded]);
  DCHECK(!_locationBarModelDelegate);

  // Initialize the prerender service before creating the toolbar controller.
  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(self.browserState);
  if (prerenderService) {
    prerenderService->SetDelegate(self);
  }

  // Create the location bar model and controller.
  _locationBarModelDelegate.reset(
      new LocationBarModelDelegateIOS(self.tabModel.webStateList));
  _locationBarModel = std::make_unique<LocationBarModelImpl>(
      _locationBarModelDelegate.get(), kMaxURLDisplayChars);
  self.helper = [_dependencyFactory newBrowserViewControllerHelper];

  PrimaryToolbarCoordinator* topToolbarCoordinator =
      [[PrimaryToolbarCoordinator alloc] initWithBrowserState:_browserState];
  self.primaryToolbarCoordinator = topToolbarCoordinator;
  topToolbarCoordinator.delegate = self;
  topToolbarCoordinator.popupPresenterDelegate = self;
  topToolbarCoordinator.webStateList = self.tabModel.webStateList;
  topToolbarCoordinator.dispatcher = self.dispatcher;
  topToolbarCoordinator.commandDispatcher = self.commandDispatcher;
  topToolbarCoordinator.longPressDelegate = self.popupMenuCoordinator;
  [topToolbarCoordinator start];

  SecondaryToolbarCoordinator* bottomToolbarCoordinator =
      [[SecondaryToolbarCoordinator alloc] initWithBrowserState:_browserState];
  self.secondaryToolbarCoordinator = bottomToolbarCoordinator;
  bottomToolbarCoordinator.webStateList = self.tabModel.webStateList;
  bottomToolbarCoordinator.dispatcher = self.dispatcher;
  bottomToolbarCoordinator.longPressDelegate = self.popupMenuCoordinator;

  if (base::FeatureList::IsEnabled(
          toolbar_container::kToolbarContainerEnabled)) {
    self.secondaryToolbarContainerCoordinator =
        [[ToolbarContainerCoordinator alloc]
            initWithBrowserState:self.browserState
                            type:ToolbarContainerType::kSecondary];
    self.secondaryToolbarContainerCoordinator.toolbarCoordinators =
        @[ bottomToolbarCoordinator ];
    [self.secondaryToolbarContainerCoordinator start];
  } else {
    [bottomToolbarCoordinator start];
  }

  [_toolbarCoordinatorAdaptor addToolbarCoordinator:topToolbarCoordinator];
  [_toolbarCoordinatorAdaptor addToolbarCoordinator:bottomToolbarCoordinator];

  self.sideSwipeController.toolbarInteractionHandler = self.toolbarInterface;
  self.sideSwipeController.primaryToolbarSnapshotProvider =
      self.primaryToolbarCoordinator;
  self.sideSwipeController.secondaryToolbarSnapshotProvider =
      self.secondaryToolbarCoordinator;

  [self updateBroadcastState];
  if (_voiceSearchController)
    _voiceSearchController->SetDispatcher(
        static_cast<id<LoadQueryCommands>>(self.commandDispatcher));

  if (IsIPadIdiom()) {
    self.tabStripCoordinator =
        [[TabStripLegacyCoordinator alloc] initWithBaseViewController:self];
    self.tabStripCoordinator.browserState = _browserState;
    self.tabStripCoordinator.dispatcher = self.commandDispatcher;
    self.tabStripCoordinator.tabModel = self.tabModel;
    self.tabStripCoordinator.presentationProvider = self;
    self.tabStripCoordinator.animationWaitDuration =
        kLegacyFullscreenControllerToolbarAnimationDuration;
    self.tabStripCoordinator.longPressDelegate = self.popupMenuCoordinator;

    UILayoutGuide* guide =
        [[NamedGuide alloc] initWithName:kTabStripTabSwitcherGuide];
    [self.view addLayoutGuide:guide];
    [self.tabStripCoordinator start];
  }

  // Create the Infobar Container Coordinator.
  self.infobarContainerCoordinator = [[InfobarContainerCoordinator alloc]
      initWithBaseViewController:self
                    browserState:_browserState
                        tabModel:self.tabModel];
  self.infobarContainerCoordinator.commandDispatcher = self.dispatcher;
  self.infobarContainerCoordinator.positioner = self;
  self.infobarContainerCoordinator.syncPresenter = self;
  [self.infobarContainerCoordinator start];
}

// Called by NSNotificationCenter when the view's window becomes key to account
// for topLayoutGuide length updates.
- (void)updateToolbarHeightForKeyWindow {
  // Update the toolbar height to account for |topLayoutGuide| changes.
  self.primaryToolbarHeightConstraint.constant =
      [self primaryToolbarHeightWithInset];
  // Stop listening for the key window notification.
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIWindowDidBecomeKeyNotification
              object:self.view.window];
}

// The height of the primary toolbar with the top safe area inset included.
- (CGFloat)primaryToolbarHeightWithInset {
  UIView* primaryToolbar = self.primaryToolbarCoordinator.viewController.view;
  CGFloat intrinsicHeight = primaryToolbar.intrinsicContentSize.height;
  if (!IsSplitToolbarMode()) {
    // When the adaptive toolbar is unsplit, add a margin.
    intrinsicHeight += kTopToolbarUnsplitMargin;
  }
  // If the primary toolbar is not the topmost header, it does not overlap with
  // the unsafe area.
  // TODO(crbug.com/806437): Update implementation such that this calculates the
  // topmost header's height.
  UIView* topmostHeader = [self.headerViews firstObject].view;
  if (primaryToolbar != topmostHeader)
    return intrinsicHeight;
  // If the primary toolbar is topmost, subtract the height of the portion of
  // the unsafe area.
  CGFloat unsafeHeight = self.view.safeAreaInsets.top;

  // The topmost header is laid out |headerOffset| from the top of |view|, so
  // subtract that from the unsafe height.
  unsafeHeight -= self.headerOffset;
  return intrinsicHeight + unsafeHeight;
}

// The height of the secondary toolbar with the bottom safe area inset included.
// Returns 0 if the toolbar should be hidden.
- (CGFloat)secondaryToolbarHeightWithInset {
  if (!IsSplitToolbarMode(self))
    return 0;

  UIView* secondaryToolbar =
      self.secondaryToolbarCoordinator.viewController.view;
  // Add the safe area inset to the toolbar height.
  CGFloat unsafeHeight = self.view.safeAreaInsets.bottom;
  return secondaryToolbar.intrinsicContentSize.height + unsafeHeight;
}

- (void)addConstraintsToPrimaryToolbar {
  NSLayoutYAxisAnchor* topAnchor;
  // On iPad, the toolbar is underneath the tab strip.
  // On iPhone, it is underneath the top of the screen.
  if ([self canShowTabStrip]) {
    topAnchor = self.tabStripView.bottomAnchor;
  } else {
    topAnchor = [self view].topAnchor;
  }

  // Only add leading and trailing constraints once as they are never updated.
  // This uses the existence of |primaryToolbarOffsetConstraint| as a proxy for
  // whether we've already added the leading and trailing constraints.
  if (!self.primaryToolbarOffsetConstraint) {
    [NSLayoutConstraint activateConstraints:@[
      [self.primaryToolbarCoordinator.viewController.view.leadingAnchor
          constraintEqualToAnchor:[self view].leadingAnchor],
      [self.primaryToolbarCoordinator.viewController.view.trailingAnchor
          constraintEqualToAnchor:[self view].trailingAnchor],
    ]];
  }

  // Offset and Height can be updated, so reset first.
  self.primaryToolbarOffsetConstraint.active = NO;
  self.primaryToolbarHeightConstraint.active = NO;

  // Create a constraint for the vertical positioning of the toolbar.
  UIView* primaryView = self.primaryToolbarCoordinator.viewController.view;
  self.primaryToolbarOffsetConstraint =
      [primaryView.topAnchor constraintEqualToAnchor:topAnchor];

  // Create a constraint for the height of the toolbar to include the unsafe
  // area height.
  self.primaryToolbarHeightConstraint = [primaryView.heightAnchor
      constraintEqualToConstant:[self primaryToolbarHeightWithInset]];

  self.primaryToolbarOffsetConstraint.active = YES;
  self.primaryToolbarHeightConstraint.active = YES;
}

- (void)addConstraintsToSecondaryToolbar {
  if (self.secondaryToolbarCoordinator) {
    // Create a constraint for the height of the toolbar to include the unsafe
    // area height.
    UIView* toolbarView = self.secondaryToolbarCoordinator.viewController.view;
    self.secondaryToolbarHeightConstraint = [toolbarView.heightAnchor
        constraintEqualToConstant:[self secondaryToolbarHeightWithInset]];
    self.secondaryToolbarHeightConstraint.active = YES;
    AddSameConstraintsToSides(
        self.secondaryToolbarContainerView, toolbarView,
        LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);

    // Constrain the container view to the bottom of self.view, and add a
    // constant height constraint such that the container's frame is equal to
    // that of the secondary toolbar at a fullscreen progress of 1.0.
    UIView* containerView = self.secondaryToolbarContainerView;
    self.secondaryToolbarNoFullscreenHeightConstraint =
        [containerView.heightAnchor
            constraintEqualToConstant:[self secondaryToolbarHeightWithInset]];
    self.secondaryToolbarNoFullscreenHeightConstraint.active = YES;
    AddSameConstraintsToSides(
        self.view, containerView,
        LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);

    NamedGuide* guide =
        [[NamedGuide alloc] initWithName:kSecondaryToolbarNoFullscreenGuide];
    [self.view addLayoutGuide:guide];
    guide.constrainedView = containerView;
  }
}

// Adds constraints to the secondary toolbar container anchoring it to the
// bottom of the browser view.
- (void)addConstraintsToSecondaryToolbarContainer {
  if (!self.secondaryToolbarContainerCoordinator)
    return;

  // Constrain the container to the bottom of the view.
  UIView* containerView =
      self.secondaryToolbarContainerCoordinator.viewController.view;
  AddSameConstraintsToSides(
      self.view, containerView,
      LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);

  NamedGuide* guide =
      [[NamedGuide alloc] initWithName:kSecondaryToolbarNoFullscreenGuide];
  [self.view addLayoutGuide:guide];
  guide.constrainedView = containerView;
}

// Adds constraints to the primary and secondary toolbars, anchoring them to the
// top and bottom of the browser view.
- (void)addConstraintsToToolbar {
  [self addConstraintsToPrimaryToolbar];
  if (base::FeatureList::IsEnabled(
          toolbar_container::kToolbarContainerEnabled)) {
    [self addConstraintsToSecondaryToolbarContainer];
  } else {
    [self addConstraintsToSecondaryToolbar];
  }
  [[self view] layoutIfNeeded];
}

// Enable functionality that only makes sense if the views are loaded and
// both browser state and tab model are valid.
- (void)addUIFunctionalityForModelAndBrowserState {
  DCHECK(_browserState);
  DCHECK(_locationBarModel);
  DCHECK(self.tabModel);
  DCHECK([self isViewLoaded]);

  [self.sideSwipeController addHorizontalGesturesToView:self.view];

  // Create child coordinators.
  _activityServiceCoordinator = [[ActivityServiceLegacyCoordinator alloc]
      initWithBaseViewController:self];
  _activityServiceCoordinator.dispatcher = self.commandDispatcher;
  _activityServiceCoordinator.tabModel = self.tabModel;
  _activityServiceCoordinator.browserState = _browserState;
  _activityServiceCoordinator.positionProvider =
      [self.primaryToolbarCoordinator activityServicePositioner];
  _activityServiceCoordinator.presentationProvider = self;

  // DownloadManagerCoordinator is already created.
  DCHECK(_downloadManagerCoordinator);
  _downloadManagerCoordinator.webStateList = self.tabModel.webStateList;
  _downloadManagerCoordinator.bottomMarginHeightAnchor =
      [NamedGuide guideWithName:kSecondaryToolbarGuide view:self.contentArea]
          .heightAnchor;

  self.popupMenuCoordinator = [[PopupMenuCoordinator alloc]
      initWithBaseViewController:self
                    browserState:self.browserState];
  self.popupMenuCoordinator.bubblePresenter = self.bubblePresenter;
  self.popupMenuCoordinator.dispatcher = self.commandDispatcher;
  self.popupMenuCoordinator.webStateList = self.tabModel.webStateList;
  self.popupMenuCoordinator.UIUpdater = _toolbarCoordinatorAdaptor;
  [self.popupMenuCoordinator start];

  self.primaryToolbarCoordinator.longPressDelegate = self.popupMenuCoordinator;
  self.secondaryToolbarCoordinator.longPressDelegate =
      self.popupMenuCoordinator;
  self.tabStripCoordinator.longPressDelegate = self.popupMenuCoordinator;

  _sadTabCoordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:self.browserContainerViewController
                    browserState:_browserState];
  _sadTabCoordinator.dispatcher = self.dispatcher;
  _sadTabCoordinator.overscrollDelegate = self;

  // If there are any existing SadTabHelpers in |self.tabModel|, update the
  // helpers delegate with the new |_sadTabCoordinator|.
  DCHECK(_sadTabCoordinator);
  for (NSUInteger i = 0; i < self.tabModel.count; i++) {
    SadTabTabHelper* sadTabHelper =
        SadTabTabHelper::FromWebState([self.tabModel tabAtIndex:i].webState);
    sadTabHelper->SetDelegate(_sadTabCoordinator);
  }

  _paymentRequestManager = [[PaymentRequestManager alloc]
      initWithBaseViewController:self
                    browserState:_browserState
                      dispatcher:self.dispatcher];
  [_paymentRequestManager setLocationBarModel:_locationBarModel.get()];
  [_paymentRequestManager setActiveWebState:self.currentWebState];
}

// Set the frame for the various views. View must be loaded.
- (void)setUpViewLayout:(BOOL)initialLayout {
  DCHECK([self isViewLoaded]);

  CGFloat topInset = self.view.safeAreaInsets.top;

  // Update the fake toolbar background height.
  CGRect fakeStatusBarFrame = _fakeStatusBarView.frame;
  fakeStatusBarFrame.size.height = topInset;
  _fakeStatusBarView.frame = fakeStatusBarFrame;


  // Position the toolbar next, either at the top of the browser view or
  // directly under the tabstrip.
  if (initialLayout) {
    [self addChildViewController:self.primaryToolbarCoordinator.viewController];
    if (self.secondaryToolbarCoordinator) {
      if (base::FeatureList::IsEnabled(
              toolbar_container::kToolbarContainerEnabled)) {
        [self addChildViewController:self.secondaryToolbarContainerCoordinator
                                         .viewController];
      } else {
        [self addChildViewController:self.secondaryToolbarCoordinator
                                         .viewController];
      }
    }
  }

  // Place the toolbar controller above the infobar container and adds the
  // layout guides.
  if (initialLayout) {
    UIView* bottomView =
        IsIPadIdiom() ? _fakeStatusBarView
                      : [self.infobarContainerCoordinator legacyContainerView];
    [[self view]
        insertSubview:self.primaryToolbarCoordinator.viewController.view
         aboveSubview:bottomView];
    if (self.secondaryToolbarCoordinator) {
      if (base::FeatureList::IsEnabled(
              toolbar_container::kToolbarContainerEnabled)) {
        // Add the container view to the hierarchy.
        UIView* containerView =
            self.secondaryToolbarContainerCoordinator.viewController.view;
        [self.view
            insertSubview:containerView
             aboveSubview:self.primaryToolbarCoordinator.viewController.view];
      } else {
        // Create the container view for the secondary toolbar and add it to the
        // hierarchy
        UIView* container = [[LegacyToolbarContainerView alloc] init];
        container.translatesAutoresizingMaskIntoConstraints = NO;
        [container
            addSubview:self.secondaryToolbarCoordinator.viewController.view];
        [self.view
            insertSubview:container
             aboveSubview:self.primaryToolbarCoordinator.viewController.view];
        self.secondaryToolbarContainerView = container;
      }
    }
    NSArray<GuideName*>* guideNames = @[
      kContentAreaGuide,
      kOmniboxGuide,
      kBackButtonGuide,
      kForwardButtonGuide,
      kToolsMenuGuide,
      kTabSwitcherGuide,
      kTranslateInfobarOptionsGuide,
      kSearchButtonGuide,
      kSecondaryToolbarGuide,
      kVoiceSearchButtonGuide,
    ];
    AddNamedGuidesToView(guideNames, self.view);

    // Configure the content area guide.
    NamedGuide* contentAreaGuide =
        [NamedGuide guideWithName:kContentAreaGuide view:self.view];

    // Constrain top to bottom of top toolbar.
    UIView* primaryToolbarView =
        self.primaryToolbarCoordinator.viewController.view;
    [contentAreaGuide.topAnchor
        constraintEqualToAnchor:primaryToolbarView.bottomAnchor]
        .active = YES;

    LayoutSides contentSides = LayoutSides::kLeading | LayoutSides::kTrailing;
    if (self.secondaryToolbarCoordinator) {
      // If there's a bottom toolbar, the content area guide is constrained to
      // its top.
      UIView* secondaryToolbarView =
          self.secondaryToolbarCoordinator.viewController.view;
      [contentAreaGuide.bottomAnchor
          constraintEqualToAnchor:secondaryToolbarView.topAnchor]
          .active = YES;
    } else {
      // Otherwise, the content area guide is constrained to self.view's bootom
      // along with its sides;
      contentSides = contentSides | LayoutSides::kBottom;
    }
    AddSameConstraintsToSides(self.view, contentAreaGuide, contentSides);
  }
  if (initialLayout) {
    [self.primaryToolbarCoordinator.viewController
        didMoveToParentViewController:self];
    if (self.secondaryToolbarCoordinator) {
      if (base::FeatureList::IsEnabled(
              toolbar_container::kToolbarContainerEnabled)) {
        [self.secondaryToolbarContainerCoordinator.viewController
            didMoveToParentViewController:self];
      } else {
        [self.secondaryToolbarCoordinator.viewController
            didMoveToParentViewController:self];
      }
    }
  }

  // Adjust the content area to be under the toolbar, for fullscreen or below
  // the toolbar is not fullscreen.
  CGRect contentFrame = self.contentArea.frame;
  if (!self.usesFullscreenContainer) {
    CGFloat marginWithHeader = StatusBarHeight();
    contentFrame.size.height = CGRectGetMaxY(contentFrame) - marginWithHeader;
    contentFrame.origin.y = marginWithHeader;
  }
  self.contentArea.frame = contentFrame;

  // Attach the typing shield to the content area but have it hidden.
  self.typingShield.frame = self.contentArea.frame;
  if (initialLayout) {
    [self.view insertSubview:self.typingShield aboveSubview:self.contentArea];
    [self.typingShield setHidden:YES];
  }
}

- (void)displayTab:(Tab*)tab {
  DCHECK(tab);
  [self loadViewIfNeeded];

  if (!self.inNewTabAnimation) {
    // Hide findbar.  |updateToolbar| will restore the findbar later.
    [self hideFindBarWithAnimation:NO];

    // Make new content visible, resizing it first as the orientation may
    // have changed from the last time it was displayed.
    [self viewForTab:tab].frame = self.contentArea.bounds;
    if (base::FeatureList::IsEnabled(
            web::features::kBrowserContainerFullscreen)) {
      [_toolbarUIUpdater updateState];
    }
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(tab.webState);
    if (NTPHelper && NTPHelper->IsActive()) {
      UIViewController* viewController =
          _ntpCoordinatorsForWebStates[tab.webState].viewController;
      viewController.view.frame = [self ntpFrameForWebState:tab.webState];
      // TODO(crbug.com/873729): For a newly created WebState, the session will
      // not be restored until LoadIfNecessary call. Remove when fixed.
      tab.webState->GetNavigationManager()->LoadIfNecessary();

      // Always show the webState view under the NTP, to work around
      // crbug.com/848789
      if (base::FeatureList::IsEnabled(kBrowserContainerKeepsContentView)) {
        if (self.browserContainerViewController.contentView !=
            tab.webState->GetView()) {
          self.browserContainerViewController.contentView =
              tab.webState->GetView();
        }
        self.browserContainerViewController.contentView.frame =
            self.contentArea.bounds;
      } else {
        self.browserContainerViewController.contentView = nil;
      }
      self.browserContainerViewController.contentViewController =
          viewController;

    } else {
      self.browserContainerViewController.contentView = [self viewForTab:tab];
    }
  }
  [self updateToolbar];

  // Notify the WebState that it was displayed.
  DCHECK(tab.webState);
  tab.webState->WasShown();
}

- (void)initializeBookmarkInteractionController {
  if (_bookmarkInteractionController)
    return;
  _bookmarkInteractionController = [[BookmarkInteractionController alloc]
      initWithBrowserState:_browserState
          parentController:self
                dispatcher:self.dispatcher
              webStateList:self.tabModel.webStateList];
}

- (void)setOverScrollActionControllerToStaticNativeContent:
    (StaticHtmlNativeContent*)nativeContent {
  if (!IsIPadIdiom()) {
    OverscrollActionsController* controller =
        [[OverscrollActionsController alloc]
            initWithScrollView:[nativeContent scrollView]];
    [controller setDelegate:self];
    OverscrollStyle style = _isOffTheRecord
                                ? OverscrollStyle::REGULAR_PAGE_INCOGNITO
                                : OverscrollStyle::REGULAR_PAGE_NON_INCOGNITO;
    controller.style = style;
    nativeContent.overscrollActionsController = controller;
  }
}

#pragma mark - Private Methods: UI Configuration, update and Layout

// Update the state of back and forward buttons, hiding the forward button if
// there is nowhere to go. Assumes the model's current tab is up to date.
- (void)updateToolbar {
  // If the BVC has been partially torn down for low memory, wait for the
  // view rebuild to handle toolbar updates.
  if (!(self.helper && _browserState))
    return;

  web::WebState* webState = self.currentWebState;
  if (!webState)
    return;

  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(self.browserState);
  BOOL isPrerendered =
      (prerenderService && prerenderService->IsLoadingPrerender());
  if (isPrerendered && ![self.helper isToolbarLoading:self.currentWebState])
    [self.primaryToolbarCoordinator showPrerenderingAnimation];

  auto* findHelper = FindTabHelper::FromWebState(webState);
  if (findHelper && findHelper->IsFindUIActive()) {
    [self showFindBarWithAnimation:NO
                        selectText:YES
                       shouldFocus:[_findBarController isFocused]];
  }

  BOOL hideToolbar = NO;
  if (webState) {
    // There are times when the NTP can be hidden but before the visibleURL
    // changes.  This can leave the BVC in a blank state where only the bottom
    // toolbar is visible. Instead, if possible, use the NewTabPageTabHelper
    // IsActive() value rather than checking -IsVisibleURLNewTabPage.
    BOOL isNTP = false;
    if (base::FeatureList::IsEnabled(kBrowserContainerContainsNTP)) {
      NewTabPageTabHelper* NTPHelper =
          NewTabPageTabHelper::FromWebState(webState);
      isNTP = NTPHelper && NTPHelper->IsActive();
    } else {
      isNTP = IsVisibleURLNewTabPage(webState);
    }
    // Hide the toolbar when displaying content suggestions without the tab
    // strip, without the focused omnibox, and for UI Refresh, only when in
    // split toolbar mode.
    hideToolbar = isNTP && !_isOffTheRecord &&
                  ![self.primaryToolbarCoordinator isOmniboxFirstResponder] &&
                  ![self.primaryToolbarCoordinator showingOmniboxPopup] &&
                  ![self canShowTabStrip] && IsSplitToolbarMode(self);
  }
  [self.primaryToolbarCoordinator.viewController.view setHidden:hideToolbar];
}

- (void)updateBroadcastState {
  self.broadcasting = self.active && self.viewVisible;
}

- (void)updateDialogPresenterActiveState {
  self.dialogPresenter.active =
      self.active && self.viewVisible && !self.inNewTabAnimation;
}

- (void)dismissPopups {
  // The dispatcher may not be fully connected during shutdown, so selectors may
  // be unrecognized.
  if (_isShutdown)
    return;
  [self.dispatcher hidePageInfo];
  [self.dispatcher dismissPopupMenuAnimated:NO];
  [self.bubblePresenter dismissBubbles];
}

- (UIView*)footerView {
  return self.secondaryToolbarCoordinator.viewController.view;
}

- (CGRect)ntpFrameForWebState:(web::WebState*)webState {
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (!NTPHelper || !NTPHelper->IsActive())
    return CGRectZero;
  if (base::FeatureList::IsEnabled(
          web::features::kBrowserContainerFullscreen) &&
      !IsRegularXRegularSizeClass())
    return self.contentArea.bounds;
  // NTP expects to be laid out behind the bottom toolbar.  It uses
  // |contentInset| to push content above the toolbar.
  UIEdgeInsets viewportInsets = [self viewportInsetsForView:self.contentArea];
  viewportInsets.bottom = 0.0;
  return UIEdgeInsetsInsetRect(self.contentArea.bounds, viewportInsets);
}

- (CGRect)visibleFrameForTab:(Tab*)tab {
  UIView* tabView = [self viewForTab:tab];
  return UIEdgeInsetsInsetRect(tabView.bounds,
                               [self viewportInsetsForView:tabView]);
}

- (void)setFramesForHeaders:(NSArray<HeaderDefinition*>*)headers
                   atOffset:(CGFloat)headerOffset {
  CGFloat height = self.headerOffset;
  for (HeaderDefinition* header in headers) {
    CGFloat yOrigin = height - headerOffset;
    BOOL isPrimaryToolbar =
        header.view == self.primaryToolbarCoordinator.viewController.view;
    // Make sure the toolbarView's constraints are also updated.  Leaving the
    // -setFrame call to minimize changes in this CL -- otherwise the way
    // toolbar_view manages it's alpha changes would also need to be updated.
    // TODO(crbug.com/778822): This can be cleaned up when the new fullscreen
    // is enabled.
    if (isPrimaryToolbar && ![self canShowTabStrip]) {
      self.primaryToolbarOffsetConstraint.constant = yOrigin;
    }
    CGRect frame = [header.view frame];
    frame.origin.y = yOrigin;
    [header.view setFrame:frame];
    if (header.behaviour != Overlap)
      height += CGRectGetHeight(frame);

    if (header.view == self.tabStripView)
      [self setNeedsStatusBarAppearanceUpdate];
  }
}

- (UIView*)viewForTab:(Tab*)tab {
  DCHECK(tab);
  if (!tab.webState)
    return nil;
  web::WebState* webState = tab.webState;
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive()) {
    return _ntpCoordinatorsForWebStates[webState].viewController.view;
  }
  DCHECK([self.tabModel indexOfTab:tab] != NSNotFound);
  // TODO(crbug.com/904588): Move |RecordPageLoadStart| to TabUsageRecorder.
  if (webState->IsEvicted() && [self.tabModel tabUsageRecorder]) {
    [self.tabModel tabUsageRecorder] -> RecordPageLoadStart(webState);
  }
  if (!webState->IsCrashed()) {
    // Load the page if it was evicted by browsing data clearing logic.
    webState->GetNavigationManager()->LoadIfNecessary();
  }
  return webState->GetView();
}

#pragma mark - Private Methods: Find Bar UI

- (void)hideFindBarWithAnimation:(BOOL)animate {
  [_findBarController hideFindBarView:animate];
}

- (void)showFindBarWithAnimation:(BOOL)animate
                      selectText:(BOOL)selectText
                     shouldFocus:(BOOL)shouldFocus {
  DCHECK(_findBarController);
  [_findBarController
      addFindBarViewToParentView:self.view
                usingToolbarView:_primaryToolbarCoordinator.viewController.view
                      selectText:selectText
                        animated:animate];
  [self updateFindBar:YES shouldFocus:shouldFocus];
}

- (void)updateFindBar:(BOOL)initialUpdate shouldFocus:(BOOL)shouldFocus {
  // TODO(crbug.com/731045): This early return temporarily replaces a DCHECK.
  // For unknown reasons, this DCHECK sometimes was hit in the wild, resulting
  // in a crash.
  if (!self.currentWebState) {
    return;
  }
  auto* helper = FindTabHelper::FromWebState(self.currentWebState);
  if (helper && helper->IsFindUIActive()) {
    if (initialUpdate && !_isOffTheRecord) {
      helper->RestoreSearchTerm();
    }

    [self setFramesForHeaders:[self headerViews]
                     atOffset:[self currentHeaderOffset]];
    [_findBarController updateView:helper->GetFindResult()
                     initialUpdate:initialUpdate
                    focusTextfield:shouldFocus];
  } else {
    [self hideFindBarWithAnimation:YES];
  }
}

#pragma mark - Private Methods: Alerts

- (void)showErrorAlertWithStringTitle:(NSString*)title
                              message:(NSString*)message {
  // Dismiss current alert.
  [_alertCoordinator stop];

  _alertCoordinator = [_dependencyFactory alertCoordinatorWithTitle:title
                                                            message:message
                                                     viewController:self];
  [_alertCoordinator start];
}

- (void)showSnackbar:(NSString*)text {
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.accessibilityLabel = text;
  message.duration = 2.0;
  message.category = kBrowserViewControllerSnackbarCategory;
  [self.dispatcher showSnackbarMessage:message];
}

#pragma mark - Private Methods: Tap handling

- (void)setLastTapPointFromCommand:(CGPoint)originPoint {
  if (CGPointEqualToPoint(originPoint, CGPointZero)) {
    _lastTapPoint = CGPointZero;
  } else {
    _lastTapPoint =
        [self.view.window convertPoint:originPoint toView:self.view];
  }
  _lastTapTime = CACurrentMediaTime();
}

- (CGPoint)lastTapPoint {
  if (CACurrentMediaTime() - _lastTapTime < 1) {
    return _lastTapPoint;
  }
  return CGPointZero;
}

- (void)saveContentAreaTapLocation:(UIGestureRecognizer*)gestureRecognizer {
  UIView* view = gestureRecognizer.view;
  CGPoint viewCoordinate = [gestureRecognizer locationInView:view];
  _lastTapPoint =
      [[view superview] convertPoint:viewCoordinate toView:self.view];
  _lastTapTime = CACurrentMediaTime();
}

#pragma mark - Private Methods: Tab creation and selection

- (BOOL)isTabNativePage:(Tab*)tab {
  web::WebState* webState = tab.webState;
  if (!webState)
    return NO;
  web::NavigationItem* visibleItem =
      webState->GetNavigationManager()->GetVisibleItem();
  if (!visibleItem)
    return NO;
  return web::GetWebClient()->IsAppSpecificURL(visibleItem->GetURL());
}

- (void)installDelegatesForTab:(Tab*)tab {
  // Unregistration happens when the Tab is removed from the TabModel.
  DCHECK_NE(tab.webState->GetDelegate(), _webStateDelegate.get());

  // There should be no pre-rendered Tabs in TabModel.
  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(_browserState);
  DCHECK(!prerenderService ||
         !prerenderService->IsWebStatePrerendered(tab.webState));

  SnapshotTabHelper::FromWebState(tab.webState)->SetDelegate(self);

  // TODO(crbug.com/777557): do not pass the dispatcher to PasswordTabHelper.
  if (PasswordTabHelper* passwordTabHelper =
          PasswordTabHelper::FromWebState(tab.webState)) {
    passwordTabHelper->SetBaseViewController(self);
    passwordTabHelper->SetDispatcher(self.dispatcher);
    passwordTabHelper->SetPasswordControllerDelegate(self);
  }

  tab.dialogDelegate = self;
  if (!IsIPadIdiom()) {
    tab.overscrollActionsControllerDelegate = self;
  }
  // Install the proper CRWWebController delegates.
  tab.webController.nativeProvider = self;
  tab.webController.swipeRecognizerProvider = self.sideSwipeController;
  tab.webState->SetDelegate(_webStateDelegate.get());
  SadTabTabHelper::FromWebState(tab.webState)->SetDelegate(_sadTabCoordinator);
  NetExportTabHelper::CreateForWebState(tab.webState, self);
  CaptivePortalDetectorTabHelper::CreateForWebState(tab.webState, self);

  if (reading_list::IsOfflinePageWithoutNativeContentEnabled()) {
    OfflinePageTabHelper::CreateForWebState(
        tab.webState,
        ReadingListModelFactory::GetForBrowserState(_browserState));
  }

  // DownloadManagerTabHelper cannot function without delegate.
  DCHECK(_downloadManagerCoordinator);
  DownloadManagerTabHelper::CreateForWebState(tab.webState,
                                              _downloadManagerCoordinator);
  if (base::FeatureList::IsEnabled(kBrowserContainerContainsNTP)) {
    NewTabPageTabHelper::CreateForWebState(tab.webState, self);
  }

  // The language detection helper accepts a callback from the translate
  // client, so must be created after it.
  // This will explode if the webState doesn't have a JS injection manager
  // (this only comes up in unit tests), so check for that and bypass the
  // init of the translation helpers if needed.
  // TODO(crbug.com/785238): Remove the need for this check.
  if (tab.webState->GetJSInjectionReceiver()) {
    ChromeIOSTranslateClient::CreateForWebState(tab.webState);
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        tab.webState,
        ChromeIOSTranslateClient::FromWebState(tab.webState)
            ->GetTranslateDriver()
            ->CreateLanguageDetectionCallback(),
        UrlLanguageHistogramFactory::GetForBrowserState(self.browserState));
  }

  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              self.browserState)) {
    accountConsistencyService->SetWebStateHandler(tab.webState, self);
  }
}

- (void)uninstallDelegatesForTab:(Tab*)tab {
  DCHECK_EQ(tab.webState->GetDelegate(), _webStateDelegate.get());

  // TODO(crbug.com/777557): do not pass the dispatcher to PasswordTabHelper.
  if (PasswordTabHelper* passwordTabHelper =
          PasswordTabHelper::FromWebState(tab.webState)) {
    passwordTabHelper->SetDispatcher(nil);
  }

  tab.dialogDelegate = nil;
  if (!IsIPadIdiom()) {
    tab.overscrollActionsControllerDelegate = nil;
  }
  tab.webController.nativeProvider = nil;
  tab.webController.swipeRecognizerProvider = nil;
  tab.webState->SetDelegate(nullptr);
  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              self.browserState)) {
    accountConsistencyService->RemoveWebStateHandler(tab.webState);
  }

  SnapshotTabHelper::FromWebState(tab.webState)->SetDelegate(nil);
}

- (void)tabSelected:(Tab*)tab notifyToolbar:(BOOL)notifyToolbar {
  DCHECK(tab);

  // Ignore changes while the tab stack view is visible (or while suspended).
  // The display will be refreshed when this view becomes active again.
  if (!self.visible || !self.webUsageEnabled)
    return;

  [self displayTab:tab];

  if (_expectingForegroundTab && !self.inNewTabAnimation) {
    // Now that the new tab has been displayed, return to normal. Rather than
    // keep a reference to the previous tab, just turn off preview mode for all
    // tabs (since doing so is a no-op for the tabs that don't have it set).
    _expectingForegroundTab = NO;

    WebStateList* webStateList = self.tabModel.webStateList;
    for (int index = 0; index < webStateList->count(); ++index) {
      web::WebState* webState = webStateList->GetWebStateAt(index);
      PagePlaceholderTabHelper::FromWebState(webState)
          ->CancelPlaceholderForNextNavigation();
    }
  }
}

- (id)nativeControllerForTab:(Tab*)tab {
  id nativeController = tab.webController.nativeController;
  if (tab.webState) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(tab.webState);
    if (NTPHelper && NTPHelper->IsActive())
      nativeController = _ntpCoordinatorsForWebStates[tab.webState];
  }
  return nativeController ? nativeController : _temporaryNativeController;
}

#pragma mark - Private Methods: Voice Search

- (void)ensureVoiceSearchControllerCreated {
  if (!_voiceSearchController) {
    VoiceSearchProvider* provider =
        ios::GetChromeBrowserProvider()->GetVoiceSearchProvider();
    if (provider) {
      _voiceSearchController =
          provider->CreateVoiceSearchController(_browserState);
      if (self.primaryToolbarCoordinator) {
        _voiceSearchController->SetDispatcher(
            static_cast<id<LoadQueryCommands>>(self.commandDispatcher));
      }
    }
  }
}

#pragma mark - Private Methods: Reading List

- (void)addToReadingListURL:(const GURL&)URL title:(NSString*)title {
  base::RecordAction(UserMetricsAction("MobileReadingListAdd"));

  ReadingListModel* readingModel =
      ReadingListModelFactory::GetForBrowserState(_browserState);
  readingModel->AddEntry(URL, base::SysNSStringToUTF8(title),
                         reading_list::ADDED_VIA_CURRENT_APP);

  [self.dispatcher triggerToolsMenuButtonAnimation];

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  [self showSnackbar:l10n_util::GetNSString(
                         IDS_IOS_READING_LIST_SNACKBAR_MESSAGE)];
}

#pragma mark - ** Protocol Implementations and Helpers **

#pragma mark - BubblePresenterDelegate

- (web::WebState*)currentWebStateForBubblePresenter:
    (BubblePresenter*)bubblePresenter {
  DCHECK(bubblePresenter == self.bubblePresenter);
  return self.currentWebState;
}

- (BOOL)rootViewVisibleForBubblePresenter:(BubblePresenter*)bubblePresenter {
  DCHECK(bubblePresenter == self.bubblePresenter);
  return self.viewVisible;
}

- (BOOL)isTabScrolledToTopForBubblePresenter:(BubblePresenter*)bubblePresenter {
  DCHECK(bubblePresenter == self.bubblePresenter);

  // If there is a native controller, use the native controller's scroll offset.
  id nativeController =
      [self nativeControllerForTab:[self.tabModel currentTab]];
  if ([nativeController conformsToProtocol:@protocol(NewTabPageOwning)] &&
      [nativeController respondsToSelector:@selector(contentOffset)]) {
    CGFloat scrolledToTopOffset =
        [nativeController respondsToSelector:@selector(contentInset)]
            ? [nativeController contentInset].top
            : 0.0;
    return [nativeController contentOffset].y == scrolledToTopOffset;
  }

  CRWWebViewScrollViewProxy* scrollProxy =
      self.currentWebState->GetWebViewProxy().scrollViewProxy;
  CGPoint scrollOffset = scrollProxy.contentOffset;
  UIEdgeInsets contentInset = scrollProxy.contentInset;
  return AreCGFloatsEqual(scrollOffset.y, -contentInset.top);
}

#pragma mark - SnapshotGeneratorDelegate methods

- (BOOL)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    canTakeSnapshotForWebState:(web::WebState*)webState {
  DCHECK(webState);
  PagePlaceholderTabHelper* pagePlaceholderTabHelper =
      PagePlaceholderTabHelper::FromWebState(webState);
  return !pagePlaceholderTabHelper->displaying_placeholder() &&
         !pagePlaceholderTabHelper->will_add_placeholder_for_next_navigation();
}

- (UIEdgeInsets)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    snapshotEdgeInsetsForWebState:(web::WebState*)webState {
  DCHECK(webState);
  // The NTP's snapshot should be inset |headerHeight| from the top to remove
  // the fake NTP toolbar from the snapshot.
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  BOOL isNTPActive = NTPHelper && NTPHelper->IsActive();
  // When using frame-based viewport adjustment calculations out of web//, the
  // WebState view's superview is used as the base snapshot view.  The content
  // area is |headerHeight| from the top of its superview, so snapshots should
  // be inset from this amount.
  bool outOfWeb =
      base::FeatureList::IsEnabled(web::features::kOutOfWebFullscreen);
  bool usesContentInset = ios::GetChromeBrowserProvider()
                              ->GetFullscreenProvider()
                              ->IsInitialized() ||
                          webState->GetWebViewProxy().shouldUseViewContentInset;
  // When the tab strip is present, there is no inset for the content area of
  // the NTP.
  if ([self canShowTabStrip] && isNTPActive) {
    return UIEdgeInsetsZero;
  }
  if (isNTPActive || (outOfWeb && !usesContentInset)) {
    return UIEdgeInsetsMake(self.headerHeight, 0.0, self.bottomToolbarHeight,
                            0.0);
  }

  // For all other scenarios, the content area is inset from the snapshot base
  // view by the web view proxy's contentInset.
  return webState->GetWebViewProxy().contentInset;
}

- (NSArray<UIView*>*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
           snapshotOverlaysForWebState:(web::WebState*)webState {
  DCHECK(webState);
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState);
  DCHECK([self.tabModel indexOfTab:tab] != NSNotFound);
  if (!self.webUsageEnabled)
    return @[];

  NSMutableArray<UIView*>* overlays = [NSMutableArray array];
  UIView* infoBarView = [self infoBarOverlayViewForTab:tab];
  if (infoBarView) {
    [overlays addObject:infoBarView];
  }

  UIView* downloadManagerView = _downloadManagerCoordinator.viewController.view;
  if (downloadManagerView) {
    [overlays addObject:downloadManagerView];
  }

  UIView* sadTabView = _sadTabCoordinator.viewController.view;
  if (sadTabView) {
    [overlays addObject:sadTabView];
  }

  return overlays;
}

- (void)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    willUpdateSnapshotForWebState:(web::WebState*)webState {
  DCHECK(webState);
  Tab* tab = LegacyTabHelper::GetTabForWebState(webState);
  DCHECK([self.tabModel indexOfTab:tab] != NSNotFound);
  id nativeController = [self nativeControllerForTab:tab];
  if ([nativeController respondsToSelector:@selector(willUpdateSnapshot)]) {
    [nativeController willUpdateSnapshot];
  }
  [tab willUpdateSnapshot];
}

- (UIView*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
         baseViewForWebState:(web::WebState*)webState {
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive()) {
    return _ntpCoordinatorsForWebStates[webState].viewController.view;
  } else if (base::FeatureList::IsEnabled(web::features::kOutOfWebFullscreen)) {
    // The webstate view is getting resized because of fullscreen. Using its
    // superview ensure that we have a view with a with a consistent size.
    if (webState->GetView().superview)
      return webState->GetView().superview;
  }
  DCHECK(webState->GetView());
  return webState->GetView();
}

#pragma mark - SnapshotGeneratorDelegate helpers

// Provides a view that encompasses currently displayed infobar(s) or nil
// if no infobar is presented.
- (UIView*)infoBarOverlayViewForTab:(Tab*)tab {
  if (tab && self.tabModel.currentTab == tab) {
    DCHECK(self.currentWebState);
    DCHECK(self.infobarContainerCoordinator);
    if ([self.infobarContainerCoordinator
            isInfobarPresentingForWebState:self.currentWebState]) {
      return [self.infobarContainerCoordinator legacyContainerView];
    }
  }
  return nil;
}

#pragma mark - PasswordControllerDelegate methods

- (BOOL)displaySignInNotification:(UIViewController*)viewController
                        fromTabId:(NSString*)tabId {
  // Check if the call comes from currently visible tab.
  NSString* visibleTabId =
      TabIdTabHelper::FromWebState(self.tabModel.currentTab.webState)->tab_id();
  if ([tabId isEqual:visibleTabId]) {
    [self addChildViewController:viewController];
    [self.view addSubview:viewController.view];
    [viewController didMoveToParentViewController:self];
    return YES;
  } else {
    return NO;
  }
}

- (void)displaySavedPasswordList {
  [self.dispatcher showSavedPasswordsSettingsFromViewController:self];
}

#pragma mark - CRWWebStateDelegate methods.

- (web::WebState*)webState:(web::WebState*)webState
    createNewWebStateForURL:(const GURL&)URL
                  openerURL:(const GURL&)openerURL
            initiatedByUser:(BOOL)initiatedByUser {
  // Check if requested web state is a popup and block it if necessary.
  if (!initiatedByUser) {
    auto* helper = BlockedPopupTabHelper::FromWebState(webState);
    if (helper->ShouldBlockPopup(openerURL)) {
      // It's possible for a page to inject a popup into a window created via
      // window.open before its initial load is committed.  Rather than relying
      // on the last committed or pending NavigationItem's referrer policy, just
      // use ReferrerPolicyDefault.
      // TODO(crbug.com/719993): Update this to a more appropriate referrer
      // policy once referrer policies are correctly recorded in
      // NavigationItems.
      web::Referrer referrer(openerURL, web::ReferrerPolicyDefault);
      helper->HandlePopup(URL, referrer);
      return nil;
    }
  }

  // Requested web state should not be blocked from opening.
  Tab* currentTab = LegacyTabHelper::GetTabForWebState(webState);
  SnapshotTabHelper::FromWebState(currentTab.webState)
      ->UpdateSnapshotWithCallback(nil);

  Tab* childTab = [[self tabModel] insertOpenByDOMTabWithOpener:currentTab];

  return childTab.webState;
}

- (void)closeWebState:(web::WebState*)webState {
  // Only allow a web page to close itself if it was opened by DOM, or if there
  // are no navigation items.
  DCHECK(webState->HasOpener() ||
         !webState->GetNavigationManager()->GetItemCount());
  if (![self tabModel])
    return;
  WebStateList* webStateList = self.tabModel.webStateList;
  int index = webStateList->GetIndexOfWebState(webState);
  if (index != WebStateList::kInvalidIndex)
    webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

- (web::WebState*)webState:(web::WebState*)webState
         openURLWithParams:(const web::WebState::OpenURLParams&)params {
  switch (params.disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB: {
      Tab* tab = [[self tabModel]
          insertTabWithURL:params.url
                  referrer:params.referrer
                transition:params.transition
                    opener:LegacyTabHelper::GetTabForWebState(webState)
               openedByDOM:NO
                   atIndex:TabModelConstants::kTabPositionAutomatically
              inBackground:(params.disposition ==
                            WindowOpenDisposition::NEW_BACKGROUND_TAB)];
      return tab.webState;
    }
    case WindowOpenDisposition::CURRENT_TAB: {
      web::NavigationManager::WebLoadParams loadParams(params.url);
      loadParams.referrer = params.referrer;
      loadParams.transition_type = params.transition;
      loadParams.is_renderer_initiated = params.is_renderer_initiated;
      webState->GetNavigationManager()->LoadURLWithParams(loadParams);
      return webState;
    }
    case WindowOpenDisposition::NEW_POPUP: {
      Tab* tab = [[self tabModel]
          insertTabWithURL:params.url
                  referrer:params.referrer
                transition:params.transition
                    opener:LegacyTabHelper::GetTabForWebState(webState)
               openedByDOM:YES
                   atIndex:TabModelConstants::kTabPositionAutomatically
              inBackground:NO];
      return tab.webState;
    }
    default:
      NOTIMPLEMENTED();
      return nullptr;
  };
}

- (void)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params {
  // Prevent context menu from displaying for a tab which is no longer the
  // current one.
  if (webState != self.currentWebState) {
    return;
  }

  // No custom context menu if no valid url is available in |params|.
  if (!params.link_url.is_valid() && !params.src_url.is_valid()) {
    return;
  }

  DCHECK(_browserState);

  _contextMenuCoordinator =
      [[ContextMenuCoordinator alloc] initWithBaseViewController:self
                                                          params:params];

  NSString* title = nil;
  ProceduralBlock action = nil;

  __weak BrowserViewController* weakSelf = self;
  GURL link = params.link_url;
  bool isLink = link.is_valid();
  GURL imageUrl = params.src_url;
  bool isImage = imageUrl.is_valid();
  const GURL& lastCommittedURL = webState->GetLastCommittedURL();
  CGPoint originPoint = [params.view convertPoint:params.location toView:nil];

  if (isLink) {
    if (link.SchemeIs(url::kJavaScriptScheme)) {
      // Open
      title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_OPEN);
      action = ^{
        Record(ACTION_OPEN_JAVASCRIPT, isImage, isLink);
        [weakSelf openJavascript:base::SysUTF8ToNSString(link.GetContent())];
      };
      [_contextMenuCoordinator addItemWithTitle:title action:action];
    }

    if (web::UrlHasWebScheme(link)) {
      web::Referrer referrer(lastCommittedURL, params.referrer_policy);

      // Open in New Tab.
      title = l10n_util::GetNSStringWithFixup(
          IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
      action = ^{
        Record(ACTION_OPEN_IN_NEW_TAB, isImage, isLink);
        // The "New Tab" item in the context menu opens a new tab in the current
        // browser state. |isOffTheRecord| indicates whether or not the current
        // browser state is incognito.
        BrowserViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;

        OpenNewTabCommand* command =
            [[OpenNewTabCommand alloc] initWithURL:link
                                          referrer:referrer
                                       inIncognito:strongSelf.isOffTheRecord
                                      inBackground:YES
                                          appendTo:kCurrentTab];
        command.originPoint = originPoint;
        UrlLoadingServiceFactory::GetForBrowserState(strongSelf.browserState)
            ->OpenUrlInNewTab(command);
      };
      [_contextMenuCoordinator addItemWithTitle:title action:action];
      if (!_isOffTheRecord) {
        // Open in Incognito Tab.
        title = l10n_util::GetNSStringWithFixup(
            IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
        action = ^{
          BrowserViewController* strongSelf = weakSelf;
          if (!strongSelf)
            return;

          Record(ACTION_OPEN_IN_INCOGNITO_TAB, isImage, isLink);

          OpenNewTabCommand* command =
              [[OpenNewTabCommand alloc] initWithURL:link
                                            referrer:referrer
                                         inIncognito:YES
                                        inBackground:NO
                                            appendTo:kCurrentTab];
          UrlLoadingServiceFactory::GetForBrowserState(strongSelf.browserState)
              ->OpenUrlInNewTab(command);
        };
        [_contextMenuCoordinator addItemWithTitle:title action:action];
      }
    }
    if (link.SchemeIsHTTPOrHTTPS()) {
      NSString* innerText = params.link_text;
      if ([innerText length] > 0) {
        // Add to reading list.
        title = l10n_util::GetNSStringWithFixup(
            IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST);
        action = ^{
          Record(ACTION_READ_LATER, isImage, isLink);
          [weakSelf addToReadingListURL:link title:innerText];
        };
        [_contextMenuCoordinator addItemWithTitle:title action:action];
      }
    }
    // Copy Link.
    title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_COPY);
    action = ^{
      Record(ACTION_COPY_LINK_ADDRESS, isImage, isLink);
      StoreURLInPasteboard(link);
    };
    [_contextMenuCoordinator addItemWithTitle:title action:action];
  }
  if (isImage) {
    web::Referrer referrer(lastCommittedURL, params.referrer_policy);
    // Save Image.
    title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_SAVEIMAGE);
    action = ^{
      Record(ACTION_SAVE_IMAGE, isImage, isLink);
        [weakSelf.imageSaver saveImageAtURL:imageUrl
                                   referrer:referrer
                                   webState:weakSelf.currentWebState];
    };
    [_contextMenuCoordinator addItemWithTitle:title action:action];
    // Copy Image.
      title =
          l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_COPYIMAGE);
      action = ^{
        Record(ACTION_COPY_IMAGE, isImage, isLink);
        DCHECK(imageUrl.is_valid());
        [weakSelf.imageCopier copyImageAtURL:imageUrl
                                    referrer:referrer
                                    webState:weakSelf.currentWebState];
      };
      [_contextMenuCoordinator addItemWithTitle:title action:action];
    // Open Image.
    title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_OPENIMAGE);
    action = ^{
      BrowserViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;

      Record(ACTION_OPEN_IMAGE, isImage, isLink);
      ChromeLoadParams params(imageUrl);
      UrlLoadingServiceFactory::GetForBrowserState(strongSelf.browserState)
          ->LoadUrlInCurrentTab(params);
    };
    [_contextMenuCoordinator addItemWithTitle:title action:action];
    // Open Image In New Tab.
    title = l10n_util::GetNSStringWithFixup(
        IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
    action = ^{
      Record(ACTION_OPEN_IMAGE_IN_NEW_TAB, isImage, isLink);
      BrowserViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;

      OpenNewTabCommand* command =
          [[OpenNewTabCommand alloc] initWithURL:imageUrl
                                        referrer:referrer
                                     inIncognito:strongSelf.isOffTheRecord
                                    inBackground:YES

                                        appendTo:kCurrentTab];
      command.originPoint = originPoint;
      UrlLoadingServiceFactory::GetForBrowserState(strongSelf.browserState)
          ->OpenUrlInNewTab(command);
    };
    [_contextMenuCoordinator addItemWithTitle:title action:action];

    TemplateURLService* service =
        ios::TemplateURLServiceFactory::GetForBrowserState(_browserState);
    if (search_engines::SupportsSearchByImage(service)) {
      const TemplateURL* defaultURL = service->GetDefaultSearchProvider();
      title = l10n_util::GetNSStringF(IDS_IOS_CONTEXT_MENU_SEARCHWEBFORIMAGE,
                                      defaultURL->short_name());
      action = ^{
        Record(ACTION_SEARCH_BY_IMAGE, isImage, isLink);
          ImageFetchTabHelper* image_fetcher =
              ImageFetchTabHelper::FromWebState(self.currentWebState);
          DCHECK(image_fetcher);
          image_fetcher->GetImageData(imageUrl, referrer, ^(NSData* data) {
            [weakSelf searchByImageData:data atURL:imageUrl];
          });
      };
      [_contextMenuCoordinator addItemWithTitle:title action:action];
    }
  }

  [_contextMenuCoordinator start];
}

- (void)webState:(web::WebState*)webState
    runRepostFormDialogWithCompletionHandler:(void (^)(BOOL))handler {
  // Display the action sheet with the arrow pointing at the top center of the
  // web contents.
  CGRect bounds = self.view.bounds;
  CGPoint dialogLocation = CGPointMake(
      CGRectGetMidX(bounds), CGRectGetMinY(bounds) + self.headerHeight);
  auto* helper = RepostFormTabHelper::FromWebState(webState);
  helper->PresentDialog(dialogLocation, base::BindOnce(^(bool shouldContinue) {
                          handler(shouldContinue);
                        }));
}

- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState {
  return _javaScriptDialogPresenter.get();
}

- (void)webState:(web::WebState*)webState
    didRequestHTTPAuthForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                      proposedCredential:(NSURLCredential*)proposedCredential
                       completionHandler:(void (^)(NSString* username,
                                                   NSString* password))handler {
  [self.dialogPresenter runAuthDialogForProtectionSpace:protectionSpace
                                     proposedCredential:proposedCredential
                                               webState:webState
                                      completionHandler:handler];
}

- (BOOL)webState:(web::WebState*)webState
    shouldPreviewLinkWithURL:(const GURL&)linkURL {
  // Link previews are not currently supported.  The kPreviewAttempted histogram
  // is used to gauge user interest in implementing this feature.
  Record(WKWebViewLinkPreviewAction::kPreviewAttempted);
  return NO;
}

#pragma mark - CRWWebStateDelegate helpers

// Evaluates Javascript asynchronously using the current page context.
- (void)openJavascript:(NSString*)javascript {
  DCHECK(javascript);
  javascript = [javascript stringByRemovingPercentEncoding];
  if (self.currentWebState) {
    self.currentWebState->ExecuteJavaScript(
        base::SysNSStringToUTF16(javascript));
  }
}

// Performs a search using |data| and |imageURL| as inputs. Opens the results in
// a new tab based on |inNewTab|.
- (void)searchByImageData:(NSData*)data atURL:(const GURL&)imageURL {
  NSData* imageData = data;
  UIImage* image = [UIImage imageWithData:data];
  gfx::Image gfxImage(image);
  // Converting to gfx::Image creates an empty image if UIImage is nil. However,
  // we still want to do the image search with nil data because that gives
  // the user the best error experience.
  if (gfxImage.IsEmpty()) {
    [self searchByResizedImageData:imageData atURL:&imageURL inNewTab:YES];
    return;
  }
  UIImage* resizedImage =
      gfx::ResizedImageForSearchByImage(gfxImage).ToUIImage();
  if (![image isEqual:resizedImage]) {
    imageData = UIImageJPEGRepresentation(resizedImage, 1.0);
  }
  [self searchByResizedImageData:imageData atURL:&imageURL inNewTab:YES];
}

// Performs a search with the given image data. The data should alread have
// been scaled down in |ResizedImageForSearchByImage|.
- (void)searchByResizedImageData:(NSData*)data
                           atURL:(const GURL*)imageURL
                        inNewTab:(BOOL)inNewTab {
  char const* bytes = reinterpret_cast<const char*>([data bytes]);
  std::string byteString(bytes, [data length]);

  TemplateURLService* templateUrlService =
      ios::TemplateURLServiceFactory::GetForBrowserState(_browserState);
  const TemplateURL* defaultURL =
      templateUrlService->GetDefaultSearchProvider();
  DCHECK(!defaultURL->image_url().empty());
  DCHECK(defaultURL->image_url_ref().IsValid(
      templateUrlService->search_terms_data()));
  TemplateURLRef::SearchTermsArgs search_args(base::ASCIIToUTF16(""));
  if (imageURL) {
    search_args.image_url = *imageURL;
  }
  search_args.image_thumbnail_content = byteString;

  // Generate the URL and populate |post_content| with the content type and
  // HTTP body for the request.
  TemplateURLRef::PostContent postContent;
  GURL result(defaultURL->image_url_ref().ReplaceSearchTerms(
      search_args, templateUrlService->search_terms_data(), &postContent));
  web::NavigationManager::WebLoadParams loadParams =
      web_navigation_util::CreateWebLoadParams(
          result, ui::PAGE_TRANSITION_TYPED, &postContent);
  if (inNewTab) {
    [self.tabModel insertTabWithLoadParams:loadParams
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:self.tabModel.count
                              inBackground:NO];
  } else {
    UrlLoadingServiceFactory::GetForBrowserState(self.browserState)
        ->LoadUrlInCurrentTab(ChromeLoadParams(loadParams));
  }
}

#pragma mark - URLLoadingObserver

// TODO(crbug.com/907527): consider moving these separate functional blurbs
// closer to their main component (using localized observers)

- (void)tabWillOpenURL:(GURL)URL
        transitionType:(ui::PageTransition)transitionType {
  [_bookmarkInteractionController dismissBookmarkModalControllerAnimated:YES];

  WebStateList* webStateList = self.tabModel.webStateList;
  web::WebState* current_web_state = webStateList->GetActiveWebState();
  if (current_web_state &&
      (transitionType & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    bool isExpectingVoiceSearch =
        VoiceSearchNavigationTabHelper::FromWebState(current_web_state)
            ->IsExpectingVoiceSearch();
    new_tab_page_uma::RecordActionFromOmnibox(
        self.browserState, URL, transitionType, isExpectingVoiceSearch);
  }
}

- (void)tabDidOpenURL:(GURL)URL
       transitionType:(ui::PageTransition)transitionType {
  // Deactivate the NTP immediately on a load to hide the NTP quickly, but
  // after calling UrlLoadingService::LoadUrlInCurrentTab.  Otherwise, if the
  // webState has never been visible (such as during startup with an NTP), it's
  // possible the webView can trigger a unnecessary load for chrome://newtab.
  if (URL.GetOrigin() != kChromeUINewTabURL) {
    WebStateList* webStateList = self.tabModel.webStateList;
    web::WebState* current_web_state = webStateList->GetActiveWebState();

    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(current_web_state);
    if (NTPHelper && NTPHelper->IsActive()) {
      NTPHelper->Deactivate();
    }
  }
}

- (void)newTabWillOpenURL:(GURL)URL inIncognito:(BOOL)inIncognito {
  if (!IsHelpURL(URL)) {
    // Send either the "New Tab Opened" or "New Incognito Tab" opened to the
    // feature_engagement::Tracker based on |inIncognito|.
    feature_engagement::NotifyNewTabEvent(_browserState, inIncognito);
  }
}

- (void)willSwitchToTabWithURL:(GURL)URL
              newWebStateIndex:(NSInteger)newWebStateIndex {
  if ([self canShowTabStrip])
    return;

  WebStateList* webStateList = self.tabModel.webStateList;
  web::WebState* webStateBeingActivated =
      webStateList->GetWebStateAt(newWebStateIndex);

  // Add animations only if the tab strip isn't shown.
  UIView* snapshotView = [self.view snapshotViewAfterScreenUpdates:NO];

  // TODO(crbug.com/904992): Do not repurpose SnapshotGeneratorDelegate.
  SwipeView* swipeView = [[SwipeView alloc]
      initWithFrame:self.contentArea.frame
          topMargin:[self snapshotGenerator:nil
                        snapshotEdgeInsetsForWebState:webStateBeingActivated]
                        .top];

  [swipeView setTopToolbarImage:[self.primaryToolbarCoordinator
                                    toolbarSideSwipeSnapshotForWebState:
                                        webStateBeingActivated]];
  [swipeView setBottomToolbarImage:[self.secondaryToolbarCoordinator
                                       toolbarSideSwipeSnapshotForWebState:
                                           webStateBeingActivated]];

  SnapshotTabHelper::FromWebState(webStateBeingActivated)
      ->RetrieveColorSnapshot(^(UIImage* image) {
        if (PagePlaceholderTabHelper::FromWebState(webStateBeingActivated)
                ->will_add_placeholder_for_next_navigation()) {
          [swipeView setImage:nil];
        } else {
          [swipeView setImage:image];
        }
      });

  SwitchToTabAnimationView* animationView =
      [[SwitchToTabAnimationView alloc] initWithFrame:self.view.bounds];

  [self.view addSubview:animationView];

  SwitchToTabAnimationPosition position =
      newWebStateIndex > webStateList->active_index()
          ? SwitchToTabAnimationPositionAfter
          : SwitchToTabAnimationPositionBefore;
  [animationView animateFromCurrentView:snapshotView
                              toNewView:swipeView
                             inPosition:position];
}

#pragma mark - CRWWebStateObserver methods.

// TODO(crbug.com/918934): This call to closeFindInPage incorrectly triggers for
// all navigations, not just navigations in the active WebState.
- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  // Stop any Find in Page searches and close the find bar when navigating to a
  // new page.
  [self closeFindInPage];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [_toolbarUIUpdater updateState];
  [self.tabModel saveSessionImmediately:NO];
  if ([self canShowTabStrip]) {
    UIUserInterfaceSizeClass sizeClass =
        self.view.window.traitCollection.horizontalSizeClass;
    [SizeClassRecorder pageLoadedWithHorizontalSizeClass:sizeClass];
  }
}

#pragma mark - OmniboxPopupPresenterDelegate methods.

- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter {
  return self.view;
}

- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter {
  return self;
}

- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter {
  self.contentArea.accessibilityElementsHidden = YES;
  self.secondaryToolbarContainerView.accessibilityElementsHidden = YES;
  self.infobarContainerCoordinator.legacyContainerView
      .accessibilityElementsHidden = YES;
}

- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter {
  self.contentArea.accessibilityElementsHidden = NO;
  self.secondaryToolbarContainerView.accessibilityElementsHidden = NO;
  self.infobarContainerCoordinator.legacyContainerView
      .accessibilityElementsHidden = NO;
}

#pragma mark - OverscrollActionsControllerDelegate methods.

- (void)overscrollActionsController:(OverscrollActionsController*)controller
                   didTriggerAction:(OverscrollAction)action {
  switch (action) {
    case OverscrollAction::NEW_TAB:
      [self.dispatcher
          openURLInNewTab:[OpenNewTabCommand
                              commandWithIncognito:self.isOffTheRecord]];
      break;
    case OverscrollAction::CLOSE_TAB:
      [self.dispatcher closeCurrentTab];
      break;
    case OverscrollAction::REFRESH:
      // Instruct the SnapshotTabHelper to ignore the next load event.
      // Attempting to snapshot while the overscroll "bounce back" animation is
      // occurring will cut the animation short.
      DCHECK(self.currentWebState);
      SnapshotTabHelper::FromWebState(self.currentWebState)->IgnoreNextLoad();
      [self reload];
      break;
    case OverscrollAction::NONE:
      NOTREACHED();
      break;
  }
}

- (BOOL)shouldAllowOverscrollActions {
  return YES;
}

- (UIView*)headerView {
  return self.primaryToolbarCoordinator.viewController.view;
}

- (UIView*)toolbarSnapshotView {
  return [self.primaryToolbarCoordinator.viewController.view
      snapshotViewAfterScreenUpdates:NO];
}

- (CGFloat)overscrollActionsControllerHeaderInset:
    (OverscrollActionsController*)controller {
  if (controller ==
      [[[self tabModel] currentTab] overscrollActionsController]) {
    if (!base::ios::IsRunningOnIOS12OrLater() &&
        self.currentWebState->GetContentsMimeType() == "application/pdf") {
      return self.headerHeight - self.view.safeAreaInsets.top;
    } else {
      return self.headerHeight;
    }
  } else
    return 0;
}

- (CGFloat)overscrollHeaderHeight {
  return self.headerHeight +
         (self.usesFullscreenContainer ? 0 : StatusBarHeight());
}

#pragma mark - CRWNativeContentProvider methods

- (BOOL)hasControllerForURL:(const GURL&)url {
  base::StringPiece host = url.host_piece();
  if (host == kChromeUIOfflineHost &&
      !reading_list::IsOfflinePageWithoutNativeContentEnabled()) {
    // Only allow offline URL that are fully specified.
    return reading_list::IsOfflineURLValid(
        url, ReadingListModelFactory::GetForBrowserState(_browserState));
  }
  if (host == kChromeUINewTabHost) {
    return !base::FeatureList::IsEnabled(kBrowserContainerContainsNTP);
  }
  if (host == kChromeUIExternalFileHost) {
    id<CRWNativeContent> controller =
        [self controllerForURL:url webState:self.currentWebState];
    return controller ? YES : NO;
  }

  return NO;
}

- (id<CRWNativeContent>)controllerForURL:(const GURL&)url
                                webState:(web::WebState*)webState {
  DCHECK(url.SchemeIs(kChromeUIScheme));

  id<CRWNativeContent> nativeController = nil;
  base::StringPiece url_host = url.host_piece();
  if (url_host == kChromeUINewTabHost) {
    if (base::FeatureList::IsEnabled(kBrowserContainerContainsNTP))
      return nil;

    CGFloat fakeStatusBarHeight = _fakeStatusBarView.frame.size.height;
    UIEdgeInsets safeAreaInset = self.view.safeAreaInsets;
    safeAreaInset.top = MAX(safeAreaInset.top - fakeStatusBarHeight, 0);

    NewTabPageController* pageController = [[NewTabPageController alloc]
                 initWithUrl:url
                     focuser:self.dispatcher
                browserState:_browserState
             toolbarDelegate:self.toolbarInterface
                    tabModel:self.tabModel
        parentViewController:self.browserContainerViewController
                  dispatcher:self.dispatcher
               safeAreaInset:safeAreaInset];
    pageController.swipeRecognizerProvider = self.sideSwipeController;
    nativeController = pageController;
  } else if (!reading_list::IsOfflinePageWithoutNativeContentEnabled() &&
             url_host == kChromeUIOfflineHost &&
             [self hasControllerForURL:url]) {
    StaticHtmlNativeContent* staticNativeController =
        [[OfflinePageNativeContent alloc] initWithBrowserState:_browserState
                                                      webState:webState
                                                           URL:url];
    [self setOverScrollActionControllerToStaticNativeContent:
              staticNativeController];
    nativeController = staticNativeController;
  } else if (url_host == kChromeUIExternalFileHost) {
    if (!base::FeatureList::IsEnabled(
            experimental_flags::kExternalFilesLoadedInWebState)) {
      // Return an instance of the |ExternalFileController| only if the file is
      // still in the sandbox.
      NSString* filePath = [ExternalFileController pathForExternalFileURL:url];
      if ([[NSFileManager defaultManager] fileExistsAtPath:filePath]) {
        nativeController =
            [[ExternalFileController alloc] initWithURL:url
                                           browserState:_browserState];
      }
    }
  } else if (url_host == kChromeUICrashHost) {
    // There is no native controller for kChromeUICrashHost, it is instead
    // handled as any other renderer crash by the SadTabTabHelper.
    // nativeController must be set to nil to prevent defaulting to a
    // PageNotAvailableController.
    nativeController = nil;
  } else {
    DCHECK(![self hasControllerForURL:url]);
    // In any other case the PageNotAvailableController is returned.
    nativeController = [[PageNotAvailableController alloc] initWithUrl:url];
  }
  // If a native controller is vended before its tab is added to the tab model,
  // use the temporary key and add it under the new tab's tabId in the
  // TabModelObserver callback.  This happens:
  // - when there is no current tab (occurs when vending the NTP controller for
  //   the first tab that is opened),
  // - when the current tab's url doesn't match |url| (occurs when a native
  //   controller is opened in a new tab)
  // - when the current tab's url matches |url| and there is already a native
  //   controller of the appropriate type vended to it (occurs when a native
  //   controller is opened in a new tab from a tab with a matching URL, e.g.
  //   opening an NTP when an NTP is already displayed in the current tab).
  // For normal page loads, history navigations, tab restorations, and crash
  // recoveries, the tab will already exist in the tab model and the tabId can
  // be used as the native controller key.
  // TODO(crbug.com/498568): To reduce complexity here, refactor the flow so
  // that native controllers vended here always correspond to the current tab.
  Tab* currentTab = self.tabModel.currentTab;
  if (!self.currentWebState ||
      self.currentWebState->GetLastCommittedURL() != url ||
      [currentTab.webController.nativeController
          isKindOfClass:[nativeController class]]) {
    _temporaryNativeController = nativeController;
  }
  return nativeController;
}

- (UIEdgeInsets)nativeContentInsetForWebState:(web::WebState*)webState {
  return [self viewportInsetsForView:webState->GetView()];
}

#pragma mark - DialogPresenterDelegate methods

- (void)dialogPresenter:(DialogPresenter*)presenter
    willShowDialogForWebState:(web::WebState*)webState {
  WebStateList* webStateList = self.tabModel.webStateList;
  int indexOfWebState = webStateList->GetIndexOfWebState(webState);
  if (indexOfWebState != WebStateList::kInvalidIndex) {
    webStateList->ActivateWebStateAt(indexOfWebState);
    DCHECK([webState->GetView() isDescendantOfView:self.contentArea]);
  }
}

- (BOOL)shouldDialogPresenterPresentDialog:(DialogPresenter*)presenter {
  if (self.presentedViewController)
    return NO;
  for (UIViewController* childViewController in self.childViewControllers) {
    if (childViewController.presentedViewController)
      return NO;
  }
  return YES;
}

#pragma mark - ToolbarHeightProviderForFullscreen

- (CGFloat)collapsedTopToolbarHeight {
  if (base::FeatureList::IsEnabled(web::features::kOutOfWebFullscreen) &&
      IsVisibleURLNewTabPage(self.currentWebState) && ![self canShowTabStrip]) {
    // When the NTP is displayed in a horizontally compact environment, the top
    // toolbars are hidden.
    return 0;
  }
  CGFloat collapsedToolbarHeight =
      ToolbarCollapsedHeight(self.traitCollection.preferredContentSizeCategory);
  // |collapsedToolbarHeight| describes the distance past the top safe area.  If
  // the browser container view is laid out using the full screen, it extends
  // past the status bar, so that additional overlap is added here.
  if (self.usesFullscreenContainer) {
    collapsedToolbarHeight += self.view.safeAreaInsets.top;
  }
  return collapsedToolbarHeight;
}

- (CGFloat)expandedTopToolbarHeight {
  if (base::FeatureList::IsEnabled(web::features::kOutOfWebFullscreen) &&
      IsVisibleURLNewTabPage(self.currentWebState) && ![self canShowTabStrip]) {
    // When the NTP is displayed in a horizontally compact environment, the top
    // toolbars are hidden.
    return 0;
  }
  return [self primaryToolbarHeightWithInset] +
         ([self canShowTabStrip] ? self.tabStripView.frame.size.height : 0.0) +
         (self.usesFullscreenContainer ? self.headerOffset : 0.0);
}

- (CGFloat)bottomToolbarHeight {
  return [self secondaryToolbarHeightWithInset];
}

#pragma mark - Toolbar height helpers

- (CGFloat)nonFullscreenToolbarHeight {
  return MAX(
      0, [self expandedTopToolbarHeight] - [self collapsedTopToolbarHeight]);
}

#pragma mark - FullscreenUIElement methods

- (void)updateForFullscreenProgress:(CGFloat)progress {
  [self updateHeadersForFullscreenProgress:progress];
  [self updateFootersForFullscreenProgress:progress];
  [self updateBrowserViewportForFullscreenProgress:progress];
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled)
    [self updateForFullscreenProgress:1.0];
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  // If the headers are being hidden, it's possible that this will reveal a
  // portion of the webview beyond the top of the page's rendered content.  In
  // order to prevent that, update the top padding and content before the
  // animation begins.
  CGFloat finalProgress = animator.finalProgress;
  BOOL hidingHeaders = animator.finalProgress < animator.startProgress;
  if (hidingHeaders) {
    id<CRWWebViewProxy> webProxy = self.currentWebState->GetWebViewProxy();
    CRWWebViewScrollViewProxy* scrollProxy = webProxy.scrollViewProxy;
    CGPoint contentOffset = scrollProxy.contentOffset;
    if (contentOffset.y - scrollProxy.contentInset.top <
        webProxy.contentInset.top) {
      [self updateBrowserViewportForFullscreenProgress:finalProgress];
      contentOffset.y = -scrollProxy.contentInset.top;
      scrollProxy.contentOffset = contentOffset;
    }
  }

  // Add animations to update the headers and footers.
  __weak BrowserViewController* weakSelf = self;
  [animator addAnimations:^{
    [weakSelf updateHeadersForFullscreenProgress:finalProgress];
    [weakSelf updateFootersForFullscreenProgress:finalProgress];
  }];

  // Animating layout changes of the rendered content in the WKWebView is not
  // supported, so update the content padding in the completion block of the
  // animator to trigger a rerender in the page's new viewport.
  __weak FullscreenAnimator* weakAnimator = animator;
  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    [weakSelf updateBrowserViewportForFullscreenProgress:
                  [weakAnimator progressForAnimatingPosition:finalPosition]];
  }];
}

#pragma mark - FullscreenUIElement helpers

// Translates the header views up and down according to |progress|, where a
// progress of 1.0 fully shows the headers and a progress of 0.0 fully hides
// them.
- (void)updateHeadersForFullscreenProgress:(CGFloat)progress {
  CGFloat offset =
      AlignValueToPixel((1.0 - progress) * [self nonFullscreenToolbarHeight]);
  [self setFramesForHeaders:[self headerViews] atOffset:offset];
}

// Translates the footer view up and down according to |progress|, where a
// progress of 1.0 fully shows the footer and a progress of 0.0 fully hides it.
- (void)updateFootersForFullscreenProgress:(CGFloat)progress {
  self.footerFullscreenProgress = progress;

  CGFloat height = 0.0;
  if (base::FeatureList::IsEnabled(
          toolbar_container::kToolbarContainerEnabled)) {
    height = [self.secondaryToolbarContainerCoordinator
        toolbarStackHeightForFullscreenProgress:progress];
  } else {
    // Update the height constraint and force a layout on the container view
    // so that the update is animatable.
    height = [self secondaryToolbarHeightWithInset] * progress;
    self.secondaryToolbarHeightConstraint.constant = height;
    [self.secondaryToolbarContainerView setNeedsLayout];
    [self.secondaryToolbarContainerView layoutIfNeeded];
  }

  // Resize the InfobarContainer to take into account the changes in the
  // toolbar.
  [self.infobarContainerCoordinator updateInfobarContainer];

  // Resize the NTP's contentInset.bottom to be above the secondary toolbar.
  id nativeController = [self nativeControllerForTab:self.tabModel.currentTab];
  if ([nativeController conformsToProtocol:@protocol(NewTabPageOwning)]) {
    id<NewTabPageOwning> newTabPageController = nativeController;
    UIEdgeInsets contentInset = newTabPageController.contentInset;
    contentInset.bottom = height;
    newTabPageController.contentInset = contentInset;
  }
}

// Updates the browser container view such that its viewport is the space
// between the primary and secondary toolbars.
- (void)updateBrowserViewportForFullscreenProgress:(CGFloat)progress {
  if (!self.currentWebState)
    return;

  // Calculate the heights of the toolbars for |progress|.  |-toolbarHeight|
  // returns the height of the toolbar extending below this view controller's
  // safe area, so the unsafe top height must be added.
  CGFloat top = AlignValueToPixel(
      self.headerHeight + (progress - 1.0) * [self nonFullscreenToolbarHeight]);
  CGFloat bottom =
      AlignValueToPixel(progress * [self secondaryToolbarHeightWithInset]);

  if (self.usesSafeInsetsForViewportAdjustments) {
    if (fullscreen::features::GetActiveViewportExperiment() ==
        fullscreen::features::ViewportAdjustmentExperiment::HYBRID) {
      [self updateWebViewFrameForBottomOffset:bottom];
    }

    [self updateBrowserSafeAreaForTopToolbarHeight:top
                               bottomToolbarHeight:bottom];
  } else {
    [self updateContentPaddingForTopToolbarHeight:top
                              bottomToolbarHeight:bottom];
  }
}

// Updates the frame of the web view so that it's |offset| from the bottom of
// the container view.
- (void)updateWebViewFrameForBottomOffset:(CGFloat)offset {
  if (!self.currentWebState)
    return;

  // Move the frame of the container view such that the bottom is aligned with
  // the top of the bottom toolbar.
  id<CRWWebViewProxy> webViewProxy = self.currentWebState->GetWebViewProxy();
  CGRect webViewFrame = webViewProxy.frame;
  CGFloat oldOriginY = CGRectGetMinY(webViewFrame);
  webViewProxy.contentOffset = CGPointMake(0.0, -offset);
  // Update the contentOffset so that the scroll position is maintained
  // relative to the screen.
  CRWWebViewScrollViewProxy* scrollViewProxy = webViewProxy.scrollViewProxy;
  CGFloat originDelta = CGRectGetMinY(webViewProxy.frame) - oldOriginY;
  CGPoint contentOffset = scrollViewProxy.contentOffset;
  contentOffset.y += originDelta;
  scrollViewProxy.contentOffset = contentOffset;
}

// Updates the web view's viewport by changing the safe area insets.
- (void)updateBrowserSafeAreaForTopToolbarHeight:(CGFloat)topToolbarHeight
                             bottomToolbarHeight:(CGFloat)bottomToolbarHeight {
  UIViewController* containerViewController =
      self.browserContainerViewController;
  containerViewController.additionalSafeAreaInsets = UIEdgeInsetsMake(
      topToolbarHeight - self.view.safeAreaInsets.top -
          self.currentWebState->GetWebViewProxy().contentOffset.y,
      0, 0, 0);
}

// Updates the padding of the web view proxy. This either resets the frame of
// the WKWebView or the contentInsets of the WKWebView's UIScrollView, depending
// on the the proxy's |shouldUseViewContentInset| property.
- (void)updateContentPaddingForTopToolbarHeight:(CGFloat)topToolbarHeight
                            bottomToolbarHeight:(CGFloat)bottomToolbarHeight {
  if (!self.currentWebState)
    return;

  id<CRWWebViewProxy> webViewProxy = self.currentWebState->GetWebViewProxy();
  UIEdgeInsets contentPadding = webViewProxy.contentInset;
  contentPadding.top = topToolbarHeight;
  contentPadding.bottom = bottomToolbarHeight;
  webViewProxy.contentInset = contentPadding;
}

- (CGFloat)currentHeaderOffset {
  NSArray<HeaderDefinition*>* headers = [self headerViews];
  if (!headers.count)
    return 0.0;

  // Prerender tab does not have a toolbar, return |headerHeight| as promised by
  // API documentation.
  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(self.browserState);
  if (prerenderService && prerenderService->IsLoadingPrerender())
    return self.headerHeight;

  UIView* topHeader = headers[0].view;
  return -(topHeader.frame.origin.y - self.headerOffset);
}

// Returns the insets into |view| that result in the visible viewport.
- (UIEdgeInsets)viewportInsetsForView:(UIView*)view {
  DCHECK(view);
  UIEdgeInsets viewportInsets = FullscreenControllerFactory::GetInstance()
                                    ->GetForBrowserState(_browserState)
                                    ->GetCurrentViewportInsets();
  // TODO(crbug.com/917548): Use BVC for viewport inset coordinate space rather
  // than the content area.
  CGRect viewportFrame = [view
      convertRect:UIEdgeInsetsInsetRect(self.contentArea.bounds, viewportInsets)
         fromView:self.contentArea];
  return UIEdgeInsetsMake(
      CGRectGetMinY(viewportFrame), CGRectGetMinX(viewportFrame),
      CGRectGetMaxY(view.bounds) - CGRectGetMaxY(viewportFrame),
      CGRectGetMaxX(view.bounds) - CGRectGetMaxX(viewportFrame));
}

#pragma mark - KeyCommandsPlumbing

- (BOOL)isOffTheRecord {
  return _isOffTheRecord;
}

- (BOOL)isFindInPageAvailable {
  if (!self.currentWebState) {
    return NO;
  }

  auto* helper = FindTabHelper::FromWebState(self.currentWebState);
  return (helper && helper->CurrentPageSupportsFindInPage());
}

- (NSUInteger)tabsCount {
  return self.tabModel.count;
}

- (BOOL)canGoBack {
  return self.currentWebState->GetNavigationManager()->CanGoBack();
}

- (BOOL)canGoForward {
  return self.currentWebState->GetNavigationManager()->CanGoForward();
}

- (void)focusTabAtIndex:(NSUInteger)index {
  if (self.tabModel.count > index) {
    [self.tabModel setCurrentTab:[self.tabModel tabAtIndex:index]];
  }
}

- (void)focusNextTab {
  NSInteger currentTabIndex =
      [self.tabModel indexOfTab:self.tabModel.currentTab];
  NSInteger modelCount = self.tabModel.count;
  if (currentTabIndex < modelCount - 1) {
    Tab* nextTab = [self.tabModel tabAtIndex:currentTabIndex + 1];
    [self.tabModel setCurrentTab:nextTab];
  } else {
    [self.tabModel setCurrentTab:[self.tabModel tabAtIndex:0]];
  }
}

- (void)focusPreviousTab {
  NSInteger currentTabIndex =
      [self.tabModel indexOfTab:self.tabModel.currentTab];
  if (currentTabIndex > 0) {
    Tab* previousTab = [self.tabModel tabAtIndex:currentTabIndex - 1];
    [self.tabModel setCurrentTab:previousTab];
  } else {
    Tab* lastTab = [self.tabModel tabAtIndex:self.tabModel.count - 1];
    [self.tabModel setCurrentTab:lastTab];
  }
}

- (void)reopenClosedTab {
  sessions::TabRestoreService* const tabRestoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(_browserState);
  if (!tabRestoreService || tabRestoreService->entries().empty())
    return;

  const std::unique_ptr<sessions::TabRestoreService::Entry>& entry =
      tabRestoreService->entries().front();
  // Only handle the TAB type.
  if (entry->type != sessions::TabRestoreService::TAB)
    return;

  [self.dispatcher openURLInNewTab:[OpenNewTabCommand command]];
  RestoreTab(entry->id, WindowOpenDisposition::CURRENT_TAB, self.browserState);
}

#pragma mark - MainContentUI

- (MainContentUIState*)mainContentUIState {
  return _mainContentUIUpdater.state;
}

#pragma mark - ToolbarCoordinatorDelegate (Public)

- (void)locationBarDidBecomeFirstResponder {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kLocationBarBecomesFirstResponderNotification
                    object:nil];
  [self.sideSwipeController setEnabled:NO];

  if (!IsVisibleURLNewTabPage(self.currentWebState)) {
    // Tapping on web content area should dismiss the keyboard. Tapping on NTP
    // gesture should propagate to NTP view.
    [self.view insertSubview:self.typingShield aboveSubview:self.contentArea];
    [self.typingShield setAlpha:0.0];
    [self.typingShield setHidden:NO];
    [UIView animateWithDuration:0.3
                     animations:^{
                       [self.typingShield setAlpha:1.0];
                     }];
  }
  [[OmniboxGeolocationController sharedInstance]
      locationBarDidBecomeFirstResponder:_browserState];

  [self.primaryToolbarCoordinator transitionToLocationBarFocusedState:YES];
}

- (void)locationBarDidResignFirstResponder {
  [self.sideSwipeController setEnabled:YES];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kLocationBarResignsFirstResponderNotification
                    object:nil];
  [UIView animateWithDuration:0.3
      animations:^{
        [self.typingShield setAlpha:0.0];
      }
      completion:^(BOOL finished) {
        // This can happen if one quickly resigns the omnibox and then taps
        // on the omnibox again during this animation. If the animation is
        // interrupted and the toolbar controller is first responder, it's safe
        // to assume |self.typingShield| shouldn't be hidden here.
        if (!finished &&
            [self.primaryToolbarCoordinator isOmniboxFirstResponder])
          return;
        [self.typingShield setHidden:YES];
      }];
  [[OmniboxGeolocationController sharedInstance]
      locationBarDidResignFirstResponder:_browserState];

  // If a load was cancelled by an omnibox edit, but nothing is loading when
  // editing ends (i.e., editing was cancelled), restart the cancelled load.
  if (_locationBarEditCancelledLoad) {
    _locationBarEditCancelledLoad = NO;

    web::WebState* webState = self.currentWebState;
    if (!_isShutdown && webState && ![self.helper isToolbarLoading:webState])
      webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                               false /* check_for_repost */);
  }
  [self.primaryToolbarCoordinator transitionToLocationBarFocusedState:NO];
}

- (void)locationBarBeganEdit {
  // On handsets, if a page is currently loading it should be stopped.
  if (![self canShowTabStrip] &&
      [self.helper isToolbarLoading:self.currentWebState]) {
    [self.dispatcher stopLoading];
    _locationBarEditCancelledLoad = YES;
  }
}

- (LocationBarModel*)locationBarModel {
  return _locationBarModel.get();
}

#pragma mark - BrowserCommands

- (void)goBack {
  web_navigation_util::GoBack(self.currentWebState);
}

- (void)goForward {
  web_navigation_util::GoForward(self.currentWebState);
}

- (void)stopLoading {
  self.currentWebState->Stop();
}

- (void)reload {
  if (self.currentWebState) {
    // |check_for_repost| is true because the reload is explicitly initiated
    // by the user.
    self.currentWebState->GetNavigationManager()->Reload(
        web::ReloadType::NORMAL, true /* check_for_repost */);
  }
}

- (void)bookmarkPage {
  [self initializeBookmarkInteractionController];
  [_bookmarkInteractionController
      presentBookmarkEditorForTab:self.tabModel.currentTab
              currentlyBookmarked:[self.helper isWebStateBookmarkedByUser:
                                                   self.currentWebState]];
}

- (void)addToReadingList:(ReadingListAddCommand*)command {
  [self addToReadingListURL:[command URL] title:[command title]];
}

- (void)preloadVoiceSearch {
  // Preload VoiceSearchController and views and view controllers needed
  // for voice search.
  [self ensureVoiceSearchControllerCreated];
  _voiceSearchController->PrepareToAppear();
}

#if !defined(NDEBUG)
- (void)viewSource {
  Tab* tab = self.tabModel.currentTab;
  DCHECK(tab);
  NSString* script = @"document.documentElement.outerHTML;";
  __weak Tab* weakTab = tab;
  __weak BrowserViewController* weakSelf = self;
  web::JavaScriptResultBlock completionHandlerBlock = ^(id result, NSError*) {
    Tab* strongTab = weakTab;
    if (!strongTab)
      return;
    if (![result isKindOfClass:[NSString class]])
      result = @"Not an HTML page";
    std::string base64HTML;
    base::Base64Encode(base::SysNSStringToUTF8(result), &base64HTML);
    GURL URL(std::string("data:text/plain;charset=utf-8;base64,") + base64HTML);
    web::Referrer referrer(strongTab.webState->GetLastCommittedURL(),
                           web::ReferrerPolicyDefault);

    [[weakSelf tabModel]
        insertTabWithURL:URL
                referrer:referrer
              transition:ui::PAGE_TRANSITION_LINK
                  opener:strongTab
             openedByDOM:YES
                 atIndex:TabModelConstants::kTabPositionAutomatically
            inBackground:NO];
  };
  [tab.webState->GetJSInjectionReceiver()
      executeJavaScript:script
      completionHandler:completionHandlerBlock];
}
#endif  // !defined(NDEBUG)

- (void)showFindInPage {
  if (!self.canShowFindBar)
    return;

  if (!_findBarController) {
    _findBarController =
        [[FindBarControllerIOS alloc] initWithIncognito:_isOffTheRecord];
    _findBarController.dispatcher = self.dispatcher;
  }

  Tab* tab = self.tabModel.currentTab;
  DCHECK(tab);
  auto* helper = FindTabHelper::FromWebState(tab.webState);
  DCHECK(!helper->IsFindUIActive());
  helper->SetFindUIActive(true);
  [self showFindBarWithAnimation:YES selectText:YES shouldFocus:YES];
}

- (void)closeFindInPage {
  __weak BrowserViewController* weakSelf = self;
  if (self.currentWebState) {
    FindTabHelper::FromWebState(self.currentWebState)->StopFinding(^{
      [weakSelf updateFindBar:NO shouldFocus:NO];
    });
  }
}

- (void)searchFindInPage {
  DCHECK(self.currentWebState);
  auto* helper = FindTabHelper::FromWebState(self.currentWebState);
  __weak BrowserViewController* weakSelf = self;
  helper->StartFinding(
      [_findBarController searchTerm], ^(FindInPageModel* model) {
        BrowserViewController* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf->_findBarController updateResultsCount:model];
      });

  if (!_isOffTheRecord)
    helper->PersistSearchTerm();
}

- (void)findNextStringInPage {
  DCHECK(self.currentWebState);
  // TODO(crbug.com/603524): Reshow find bar if necessary.
  FindTabHelper::FromWebState(self.currentWebState)
      ->ContinueFinding(FindTabHelper::FORWARD, ^(FindInPageModel* model) {
        [_findBarController updateResultsCount:model];
      });
}

- (void)findPreviousStringInPage {
  DCHECK(self.currentWebState);
  // TODO(crbug.com/603524): Reshow find bar if necessary.
  FindTabHelper::FromWebState(self.currentWebState)
      ->ContinueFinding(FindTabHelper::REVERSE, ^(FindInPageModel* model) {
        [_findBarController updateResultsCount:model];
      });
}

- (void)showHelpPage {
  GURL helpUrl(l10n_util::GetStringUTF16(IDS_IOS_TOOLS_MENU_HELP_URL));
  OpenNewTabCommand* command =
      [[OpenNewTabCommand alloc] initWithURL:helpUrl
                                    referrer:web::Referrer()
                                 inIncognito:NO
                                inBackground:NO
                                    appendTo:kCurrentTab];
  UrlLoadingServiceFactory::GetForBrowserState(self.browserState)
      ->OpenUrlInNewTab(command);
}

- (void)showBookmarksManager {
  [self initializeBookmarkInteractionController];
  [_bookmarkInteractionController presentBookmarks];
}

- (void)requestDesktopSite {
  [self reloadWithUserAgentType:web::UserAgentType::DESKTOP];
}

- (void)requestMobileSite {
  [self reloadWithUserAgentType:web::UserAgentType::MOBILE];
}

- (void)closeCurrentTab {
  Tab* currentTab = self.tabModel.currentTab;
  NSUInteger tabIndex = [self.tabModel indexOfTab:currentTab];
  if (tabIndex == NSNotFound)
    return;

  UIView* snapshotView = [self.contentArea snapshotViewAfterScreenUpdates:NO];
  snapshotView.frame = self.contentArea.frame;

  [self.tabModel closeTabAtIndex:tabIndex];

  if (![self canShowTabStrip]) {
    [self.contentArea addSubview:snapshotView];
    page_animation_util::AnimateOutWithCompletion(snapshotView, ^{
      [snapshotView removeFromSuperview];
    });
  }
}

- (void)navigateToMemexTabSwitcher {
  // TODO(crbug.com/799601): Delete this once its not needed.
  const GURL memexURL("https://chrome-memex.appspot.com");
  ChromeLoadParams params(memexURL);
  UrlLoadingServiceFactory::GetForBrowserState(self.browserState)
      ->LoadUrlInCurrentTab(params);
}

- (void)prepareForPopupMenuPresentation:(PopupMenuCommandType)type {
  DCHECK(_browserState);
  DCHECK(self.visible || self.dismissingModal);

  // Dismiss the omnibox (if open).
  [self.dispatcher cancelOmniboxEdit];
  // Dismiss the soft keyboard (if open).
  [[self viewForTab:self.tabModel.currentTab] endEditing:NO];
  // Dismiss Find in Page focus.
  [self updateFindBar:NO shouldFocus:NO];

  if (type == PopupMenuCommandTypeToolsMenu) {
    [self.bubblePresenter toolsMenuDisplayed];
  }
}

- (void)focusFakebox {
  id nativeController = [self nativeControllerForTab:self.tabModel.currentTab];
  DCHECK([nativeController conformsToProtocol:@protocol(NewTabPageOwning)]);
  [nativeController focusFakebox];
}

- (void)searchByImage:(UIImage*)image {
  gfx::Image gfxImage(image);
  UIImage* resizedImage =
      gfx::ResizedImageForSearchByImage(gfxImage).ToUIImage();
  NSData* data = UIImageJPEGRepresentation(resizedImage, 1.0);
  [self searchByResizedImageData:data atURL:nil inNewTab:NO];
}

#pragma mark - BrowserCommands helpers

// Reloads the original url of the last non-redirect item (including non-history
// items) with |userAgentType|.
- (void)reloadWithUserAgentType:(web::UserAgentType)userAgentType {
  if (self.userAgentType == userAgentType)
    return;
  web::WebState* webState = self.currentWebState;
  web::NavigationManager* navigationManager = webState->GetNavigationManager();
  navigationManager->ReloadWithUserAgentType(userAgentType);
}

#pragma mark - TabModelObserver methods

// Observer method, tab inserted.
- (void)tabModel:(TabModel*)model
    didInsertTab:(Tab*)tab
         atIndex:(NSUInteger)modelIndex
    inForeground:(BOOL)fg {
  DCHECK(tab);
  [self installDelegatesForTab:tab];

  if (fg) {
    [_paymentRequestManager setActiveWebState:tab.webState];
  }
}

// Observer method, active tab changed.
- (void)tabModel:(TabModel*)model
    didChangeActiveTab:(Tab*)newTab
           previousTab:(Tab*)previousTab
               atIndex:(NSUInteger)index {
  if (previousTab) {
    previousTab.webState->WasHidden();
    [self dismissPopups];
  }

  // TODO(rohitrao): tabSelected expects to always be called with a non-nil tab.
  // Currently this observer method is always called with a non-nil |newTab|,
  // but that may change in the future.  Remove this DCHECK when it does.
  DCHECK(newTab);

  self.currentWebState->GetWebViewProxy().scrollViewProxy.clipsToBounds = NO;

  [_paymentRequestManager setActiveWebState:newTab.webState];

  [self tabSelected:newTab notifyToolbar:YES];
}

- (void)tabModel:(TabModel*)model didChangeTab:(Tab*)tab {
  DCHECK(tab && ([self.tabModel indexOfTab:tab] != NSNotFound));
  if (tab == self.tabModel.currentTab) {
    [self updateToolbar];
  }
}

- (void)tabModel:(TabModel*)model
    newTabWillOpen:(Tab*)tab
      inBackground:(BOOL)background {
  DCHECK(tab);
  _temporaryNativeController = nil;

  // The rest of this function initiates the new tab animation, which is
  // phone-specific.  Call the foreground tab added completion block; for
  // iPhones, this will get executed after the animation has finished.
  if ([self canShowTabStrip]) {
    if (self.foregroundTabWasAddedCompletionBlock) {
      // This callback is called before webState is activated (on
      // kTabModelNewTabWillOpenNotification notification). Dispatch the
      // callback asynchronously to be sure the activation is complete.
      dispatch_async(dispatch_get_main_queue(), ^{
        // Test existence again as the block may have been deleted.
        if (self.foregroundTabWasAddedCompletionBlock) {
          self.foregroundTabWasAddedCompletionBlock();
          self.foregroundTabWasAddedCompletionBlock = nil;
        }
      });
    }
    return;
  }

  // Do nothing if browsing is currently suspended.  The BVC will set everything
  // up correctly when browsing resumes.
  if (!self.visible || !self.webUsageEnabled)
    return;

  // Block that starts voice search at the end of new Tab animation if
  // necessary.
  ProceduralBlock startVoiceSearchIfNecessary = ^void() {
    if (_startVoiceSearchAfterNewTabAnimation) {
      _startVoiceSearchAfterNewTabAnimation = NO;
      [self startVoiceSearch];
    }
  };

  if (background) {
    self.inNewTabAnimation = NO;
  } else {
    self.inNewTabAnimation = YES;
    [self animateNewTab:tab
        inForegroundWithCompletion:startVoiceSearchIfNecessary];
  }
}

// Observer method, tab replaced.
- (void)tabModel:(TabModel*)model
    didReplaceTab:(Tab*)oldTab
          withTab:(Tab*)newTab
          atIndex:(NSUInteger)index {
  [self uninstallDelegatesForTab:oldTab];
  [self installDelegatesForTab:newTab];

  // Add |newTab|'s view to the hierarchy if it's the current Tab.
  if (self.active && model.currentTab == newTab)
    [self displayTab:newTab];

  if (newTab)
    [_paymentRequestManager setActiveWebState:newTab.webState];
}

// A tab has been removed, remove its views from display if necessary.
- (void)tabModel:(TabModel*)model
    didRemoveTab:(Tab*)tab
         atIndex:(NSUInteger)index {
  tab.webState->WasHidden();

  [self uninstallDelegatesForTab:tab];

  // Cancel dialogs for |tab|'s WebState.
  [self.dialogPresenter cancelDialogForWebState:tab.webState];

  // Ignore changes while the tab stack view is visible (or while suspended).
  // The display will be refreshed when this view becomes active again.
  if (!self.visible || !self.webUsageEnabled)
    return;

  // Remove the find bar for now.
  [self hideFindBarWithAnimation:NO];
}

- (void)tabModel:(TabModel*)model willRemoveTab:(Tab*)tab {
  if (tab == [model currentTab]) {
    self.browserContainerViewController.contentView = nil;
  }

  [_paymentRequestManager stopTrackingWebState:tab.webState];

  [[UpgradeCenter sharedInstance]
      tabWillClose:TabIdTabHelper::FromWebState(tab.webState)->tab_id()];
  if ([model count] == 1) {  // About to remove the last tab.
    [_paymentRequestManager setActiveWebState:nullptr];
  }
}

#pragma mark - TabModelObserver helpers (new tab animations)

- (void)animateNewTab:(Tab*)tab
    inForegroundWithCompletion:(ProceduralBlock)completion {
  // Create the new page image, and load with the new tab snapshot except if
  // it is the NTP.
  UIView* newPage = nil;
  GURL tabURL = tab.webState->GetVisibleURL();
  // Toolbar snapshot is only used for the UIRefresh animation.
  UIView* toolbarSnapshot;

  if (tabURL == kChromeUINewTabURL && !_isOffTheRecord &&
      ![self canShowTabStrip]) {
    // Add a snapshot of the primary toolbar to the background as the
    // animation runs.
    UIViewController* toolbarViewController =
        self.primaryToolbarCoordinator.viewController;
    toolbarSnapshot =
        [toolbarViewController.view snapshotViewAfterScreenUpdates:NO];
    toolbarSnapshot.frame =
        [self.contentArea convertRect:toolbarSnapshot.frame fromView:self.view];
    [self.contentArea addSubview:toolbarSnapshot];
    newPage = [self viewForTab:tab];
    newPage.userInteractionEnabled = NO;
    if (base::FeatureList::IsEnabled(
            web::features::kBrowserContainerFullscreen)) {
      newPage.frame = self.view.bounds;
    } else {
      // Compute a frame for the new page by removing the status bar height
      // from the bounds of |self.view|.
      CGRect viewBounds, remainder;
      CGRectDivide(self.view.bounds, &remainder, &viewBounds, StatusBarHeight(),
                   CGRectMinYEdge);
      newPage.frame = viewBounds;
    }
  } else {
    [self viewForTab:tab].frame = self.contentArea.bounds;
    // Setting the frame here doesn't trigger a layout pass. Trigger it manually
    // if needed. Not triggering it can create problem if the previous frame
    // wasn't the right one, for example in https://crbug.com/852106.
    [[self viewForTab:tab] layoutIfNeeded];
    newPage = [self viewForTab:tab];
    newPage.userInteractionEnabled = NO;
  }

  // Cleanup steps needed for both UI Refresh and stack-view style animations.
  auto commonCompletion = ^{
    [self viewForTab:tab].frame = self.contentArea.bounds;
    newPage.userInteractionEnabled = YES;
    self.inNewTabAnimation = NO;
    // Use the model's currentTab here because it is possible that it can
    // be reset to a new value before the new Tab animation finished (e.g.
    // if another Tab shows a dialog via |dialogPresenter|). However, that
    // tab's view hasn't been displayed yet because it was in a new tab
    // animation.
    Tab* currentTab = self.tabModel.currentTab;
    if (currentTab) {
      [self tabSelected:currentTab notifyToolbar:NO];
    }
    if (completion)
      completion();
    [self.bubblePresenter presentLongPressBubbleIfEligible];

    if (self.foregroundTabWasAddedCompletionBlock) {
      self.foregroundTabWasAddedCompletionBlock();
      self.foregroundTabWasAddedCompletionBlock = nil;
    }
  };

  CGPoint origin = [self lastTapPoint];

  // The animation will have the same frame as |self|, minus the status bar,
  // so shift it down and reduce its height accordingly.
  CGRect frame = self.view.bounds;
  if (!self.usesFullscreenContainer ||
      !base::FeatureList::IsEnabled(web::features::kOutOfWebFullscreen)) {
    frame.origin.y += StatusBarHeight();
    frame.size.height -= StatusBarHeight();
  }
  frame = [self.contentArea convertRect:frame fromView:self.view];
  ForegroundTabAnimationView* animatedView =
      [[ForegroundTabAnimationView alloc] initWithFrame:frame];
  animatedView.contentView = newPage;
  __weak UIView* weakAnimatedView = animatedView;
  auto completionBlock = ^() {
    [weakAnimatedView removeFromSuperview];
    [toolbarSnapshot removeFromSuperview];
    commonCompletion();
  };
  [self.contentArea addSubview:animatedView];
  [animatedView animateFrom:origin withCompletion:completionBlock];
  return;
}

#pragma mark - InfobarPositioner

- (UIView*)parentView {
  return self.contentArea;
}

#pragma mark - UIGestureRecognizerDelegate

// Always return yes, as this tap should work with various recognizers,
// including UITextTapRecognizer, UILongPressGestureRecognizer,
// UIScrollViewPanGestureRecognizer and others.
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

// Tap gestures should only be recognized within |contentArea|.
- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gesture {
  CGPoint location = [gesture locationInView:self.view];

  // Only allow touches on descendant views of |contentArea|.
  UIView* hitView = [self.view hitTest:location withEvent:nil];
  return [hitView isDescendantOfView:self.contentArea];
}

#pragma mark - SideSwipeControllerDelegate

- (void)sideSwipeViewDismissAnimationDidEnd:(UIView*)sideSwipeView {
  DCHECK(![self canShowTabStrip]);
  [self updateToolbar];

  // Reset horizontal stack view.
  [sideSwipeView removeFromSuperview];
  [self.sideSwipeController setInSwipe:NO];
}

- (UIView*)sideSwipeContentView {
  return self.contentArea;
}

- (void)sideSwipeRedisplayTab:(Tab*)tab {
  [self displayTab:tab];
}

- (BOOL)preventSideSwipe {
  if ([self.popupMenuCoordinator isShowingPopupMenu])
    return YES;

  if (_voiceSearchController && _voiceSearchController->IsVisible())
    return YES;

  if (!self.active)
    return YES;

  return NO;
}

- (void)updateAccessoryViewsForSideSwipeWithVisibility:(BOOL)visible {
  if (visible) {
    [self updateToolbar];
    [self.infobarContainerCoordinator hideContainer:NO];
  } else {
    // Hide UI accessories such as find bar and first visit overlays
    // for welcome page.
    [self hideFindBarWithAnimation:NO];
    [self.infobarContainerCoordinator hideContainer:YES];
  }
}

- (CGFloat)headerHeightForSideSwipe {
  // If the toolbar is hidden, only inset the side swipe navigation view by
  // |safeAreaInsets.top|.  Otherwise insetting by |self.headerHeight| would
  // show a grey strip where the toolbar would normally be.
  if (self.primaryToolbarCoordinator.viewController.view.hidden)
    return self.view.safeAreaInsets.top;
  return self.headerHeight;
}

- (BOOL)verifyToolbarViewPlacementInView:(UIView*)views {
  BOOL seenToolbar = NO;
  BOOL seenInfoBarContainer = NO;
  BOOL seenContentArea = NO;
  for (UIView* view in views.subviews) {
    if (view == [self.infobarContainerCoordinator legacyContainerView])
      seenInfoBarContainer = YES;
    else if (view == self.contentArea)
      seenContentArea = YES;
    if ((seenToolbar && !seenInfoBarContainer) ||
        (seenInfoBarContainer && !seenContentArea))
      return NO;
  }
  return YES;
}

- (BOOL)canBeginToolbarSwipe {
  return ![self.primaryToolbarCoordinator isOmniboxFirstResponder] &&
         ![self.primaryToolbarCoordinator showingOmniboxPopup];
}

- (UIView*)topToolbarView {
  return self.primaryToolbarCoordinator.viewController.view;
}

#pragma mark - PreloadControllerDelegate methods

- (BOOL)preloadShouldUseDesktopUserAgent {
  return [self userAgentType] == web::UserAgentType::DESKTOP;
}

- (BOOL)preloadHasNativeControllerForURL:(const GURL&)url {
  return [self hasControllerForURL:url];
}

#pragma mark - NetExportTabHelperDelegate

- (void)netExportTabHelper:(NetExportTabHelper*)tabHelper
    showMailComposerWithContext:(ShowMailComposerContext*)context {
  if (![MFMailComposeViewController canSendMail]) {
    NSString* alertTitle =
        l10n_util::GetNSString([context emailNotConfiguredAlertTitleId]);
    NSString* alertMessage =
        l10n_util::GetNSString([context emailNotConfiguredAlertMessageId]);
    [self showErrorAlertWithStringTitle:alertTitle message:alertMessage];
    return;
  }
  MFMailComposeViewController* mailViewController =
      [[MFMailComposeViewController alloc] init];
  [mailViewController setModalPresentationStyle:UIModalPresentationFormSheet];
  [mailViewController setToRecipients:[context toRecipients]];
  [mailViewController setSubject:[context subject]];
  [mailViewController setMessageBody:[context body] isHTML:NO];

  const base::FilePath& textFile = [context textFileToAttach];
  if (!textFile.empty()) {
    NSString* filename = base::SysUTF8ToNSString(textFile.value());
    NSData* data = [NSData dataWithContentsOfFile:filename];
    if (data) {
      NSString* displayName =
          base::SysUTF8ToNSString(textFile.BaseName().value());
      [mailViewController addAttachmentData:data
                                   mimeType:@"text/plain"
                                   fileName:displayName];
    }
  }

  [mailViewController setMailComposeDelegate:self];
  [self presentViewController:mailViewController animated:YES completion:nil];
}

#pragma mark - MFMailComposeViewControllerDelegate methods

- (void)mailComposeController:(MFMailComposeViewController*)controller
          didFinishWithResult:(MFMailComposeResult)result
                        error:(NSError*)error {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - TabDialogDelegate methods

- (void)cancelDialogForTab:(Tab*)tab {
  [self.dialogPresenter cancelDialogForWebState:tab.webState];
}

#pragma mark - LogoAnimationControllerOwnerOwner (Public)

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  id currentNativeController =
      [self nativeControllerForTab:self.tabModel.currentTab];
  Protocol* possibleOwnerProtocol =
      @protocol(LogoAnimationControllerOwnerOwner);
  if ([currentNativeController conformsToProtocol:possibleOwnerProtocol] &&
      [currentNativeController logoAnimationControllerOwner]) {
    // If the current native controller is showing a GLIF view (e.g. the NTP
    // when there is no doodle), use that GLIFControllerOwner.
    return [currentNativeController logoAnimationControllerOwner];
  }
  return nil;
}

#pragma mark - ActivityServicePresentation

- (void)presentActivityServiceViewController:(UIViewController*)controller {
  [self presentViewController:controller animated:YES completion:nil];
}

- (void)activityServiceDidEndPresenting {
  [self.dialogPresenter tryToPresent];
}

- (void)showActivityServiceErrorAlertWithStringTitle:(NSString*)title
                                             message:(NSString*)message {
  [self showErrorAlertWithStringTitle:title message:message];
}

#pragma mark - CaptivePortalDetectorTabHelperDelegate

- (void)captivePortalDetectorTabHelper:
            (CaptivePortalDetectorTabHelper*)tabHelper
                 connectWithLandingURL:(const GURL&)landingURL {
  [self.tabModel
      insertTabWithLoadParams:web_navigation_util::CreateWebLoadParams(
                                  landingURL, ui::PAGE_TRANSITION_TYPED,
                                  nullptr)
                       opener:nil
                  openedByDOM:NO
                      atIndex:self.tabModel.count
                 inBackground:NO];
}

#pragma mark - PageInfoPresentation

- (void)presentPageInfoView:(UIView*)pageInfoView {
  [pageInfoView setFrame:self.view.bounds];
  [self.view addSubview:pageInfoView];
}

- (void)prepareForPageInfoPresentation {
  // Dismiss the omnibox (if open).
  [self.dispatcher cancelOmniboxEdit];
}

- (CGPoint)convertToPresentationCoordinatesForOrigin:(CGPoint)origin {
  return [self.view convertPoint:origin fromView:nil];
}

#pragma mark - TabStripPresentation

- (BOOL)isTabStripFullyVisible {
  return ([self currentHeaderOffset] == 0.0f);
}

- (void)showTabStripView:(UIView*)tabStripView {
  DCHECK([self isViewLoaded]);
  DCHECK(tabStripView);
  self.tabStripView = tabStripView;
  CGRect tabStripFrame = [self.tabStripView frame];
  tabStripFrame.origin = CGPointZero;
  // TODO(crbug.com/256655): Move the origin.y below to -setUpViewLayout.
  // because the CGPointZero above will break reset the offset, but it's not
  // clear what removing that will do.
  tabStripFrame.origin.y = self.headerOffset;
  tabStripFrame.size.width = CGRectGetWidth([self view].bounds);
  [self.tabStripView setFrame:tabStripFrame];
  [[self view] addSubview:tabStripView];
}

#pragma mark - ManageAccountsDelegate

- (void)onManageAccounts {
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForBrowserState(self.browserState)
          ->GetState());
  [self.dispatcher showAccountsSettingsFromViewController:self];
}

- (void)onAddAccount {
  signin_metrics::LogAccountReconcilorStateOnGaiaResponse(
      ios::AccountReconcilorFactory::GetForBrowserState(self.browserState)
          ->GetState());
  [self.dispatcher showAddAccountFromViewController:self];
}

- (void)onGoIncognito:(const GURL&)url {
  // The user taps on go incognito from the mobile U-turn webpage (the web page
  // that displays all users accounts available in the content area). As the
  // user chooses to go to incognito, the mobile U-turn page is no longer
  // neeeded. The current solution is to go back in history. This has the
  // advantage of keeping the current browsing session and give a good user
  // experience when the user comes back from incognito.
  [self goBack];

  if (url.is_valid()) {
    OpenNewTabCommand* command = [[OpenNewTabCommand alloc]
         initWithURL:url
            referrer:web::Referrer()  // Strip referrer when switching modes.
         inIncognito:YES
        inBackground:NO
            appendTo:kLastTab];
    [self.dispatcher openURLInNewTab:command];
  } else {
    [self.dispatcher openURLInNewTab:[OpenNewTabCommand command]];
  }
}

#pragma mark - SyncPresenter (Public)

- (void)showReauthenticateSignin {
  [self.dispatcher
              showSignin:
                  [[ShowSigninCommand alloc]
                      initWithOperation:AUTHENTICATION_OPERATION_REAUTHENTICATE
                            accessPoint:signin_metrics::AccessPoint::
                                            ACCESS_POINT_UNKNOWN]
      baseViewController:self];
}

- (void)showSyncSettings {
  [self.dispatcher showSyncSettingsFromViewController:self];
}

- (void)showSyncPassphraseSettings {
  [self.dispatcher showSyncPassphraseSettingsFromViewController:self];
}

#pragma mark - NewTabPageTabHelperDelegate

- (void)newTabPageHelperDidChangeVisibility:(NewTabPageTabHelper*)NTPHelper
                                forWebState:(web::WebState*)webState {
  if (NTPHelper->IsActive()) {
    DCHECK(!_ntpCoordinatorsForWebStates[webState]);
    NewTabPageCoordinator* newTabPageCoordinator =
        [[NewTabPageCoordinator alloc] initWithBrowserState:_browserState];
    newTabPageCoordinator.dispatcher = self.dispatcher;
    newTabPageCoordinator.toolbarDelegate = self.toolbarInterface;
    newTabPageCoordinator.webStateList = self.tabModel.webStateList;
    _ntpCoordinatorsForWebStates[webState] = newTabPageCoordinator;
  } else {
    DCHECK(_ntpCoordinatorsForWebStates[webState]);
    _ntpCoordinatorsForWebStates.erase(webState);
  }
  if (self.active && self.currentWebState == webState) {
    [self displayTab:self.tabModel.currentTab];
  }
}

@end
