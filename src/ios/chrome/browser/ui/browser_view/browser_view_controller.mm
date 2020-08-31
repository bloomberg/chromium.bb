// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller+private.h"

#import <MessageUI/MessageUI.h>

#include "base/base64.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service_helper.h"
#include "components/signin/core/browser/account_reconcilor.h"
#import "components/signin/ios/browser/account_consistency_service.h"
#include "components/signin/ios/browser/active_state_manager.h"
#import "components/signin/ios/browser/manage_accounts_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_manager.h"
#include "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#include "ios/chrome/browser/feature_engagement/tracker_util.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#include "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/language/url_language_histogram_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/metrics/size_class_recorder.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/overscroll_actions/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/passwords/password_controller.h"
#include "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/prerender/preload_controller_delegate.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/search_engines_util.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/signin/account_consistency_service_factory.h"
#include "ios/chrome/browser/signin/account_reconcilor_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"
#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper_delegate.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/re_signin_infobar_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_interaction_controller.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_helper.h"
#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter_delegate.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/help_commands.h"
#import "ios/chrome/browser/ui/commands/reading_list_add_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"
#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/image_util/image_copier.h"
#import "ios/chrome/browser/ui/image_util/image_saver.h"
#import "ios/chrome/browser/ui/infobars/infobar_container_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_positioner.h"
#include "ios/chrome/browser/ui/location_bar/location_bar_model_delegate_ios.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui_broadcasting_util.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui_state.h"
#import "ios/chrome/browser/ui/main_content/web_scroll_view_main_content_ui_forwarder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"
#import "ios/chrome/browser/ui/presenters/vertical_animation_container.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_coordinator.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_coordinator.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"
#import "ios/chrome/browser/ui/side_swipe/swipe_view.h"
#import "ios/chrome/browser/ui/signin_interaction/public/signin_presenter.h"
#import "ios/chrome/browser/ui/tabs/background_tab_animation_view.h"
#import "ios/chrome/browser/ui/tabs/foreground_tab_animation_view.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_presentation.h"
#import "ios/chrome/browser/ui/tabs/switch_to_tab_animation_view.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_legacy_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_presenter.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/legacy_toolbar_ui_updater.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui_broadcasting_util.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_adaptor.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_coordinator.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_features.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ios/chrome/browser/ui/util/page_animation_util.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_playback_controller_factory.h"
#include "ios/chrome/browser/upgrade/upgrade_center.h"
#import "ios/chrome/browser/url_loading/image_search_param_generator.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_observer_bridge.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"
#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"
#import "ios/chrome/browser/web/blocked_popup_tab_helper.h"
#import "ios/chrome/browser/web/image_fetch_tab_helper.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/chrome/browser/web/web_state_delegate_tab_helper.h"
#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/webui/net_export_tab_helper.h"
#import "ios/chrome/browser/webui/net_export_tab_helper_delegate.h"
#import "ios/chrome/browser/webui/show_mail_composer_context.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/fullscreen_provider.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_controller.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ios/web/common/url_scheme_util.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/deprecated/crw_web_controller_util.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {

const size_t kMaxURLDisplayChars = 32 * 1024;

typedef NS_ENUM(NSInteger, ContextMenuHistogram) {
  // Note: these values must match the ContextMenuOptionIOS enum in enums.xml.
  ACTION_OPEN_IN_NEW_TAB = 0,
  ACTION_OPEN_IN_INCOGNITO_TAB = 1,
  ACTION_COPY_LINK_ADDRESS = 2,
  ACTION_SAVE_IMAGE = 3,
  ACTION_OPEN_IMAGE = 4,
  ACTION_OPEN_IMAGE_IN_NEW_TAB = 5,
  ACTION_COPY_IMAGE = 6,
  ACTION_SEARCH_BY_IMAGE = 7,
  ACTION_OPEN_JAVASCRIPT = 8,
  ACTION_READ_LATER = 9,
  NUM_ACTIONS = 10,
};

void Record(ContextMenuHistogram action, bool is_image, bool is_link) {
  if (is_image) {
    if (is_link) {
      UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOptionIOS.ImageLink",
                                action, NUM_ACTIONS);
    } else {
      UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOptionIOS.Image", action,
                                NUM_ACTIONS);
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOptionIOS.Link", action,
                              NUM_ACTIONS);
  }
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
  return UIColor.blackColor;
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

@interface BrowserViewController () <BubblePresenterDelegate,
                                     CaptivePortalDetectorTabHelperDelegate,
                                     CRWWebStateDelegate,
                                     CRWWebStateObserver,
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
                                     SigninPresenter,
                                     SnapshotGeneratorDelegate,
                                     TabStripPresentation,
                                     FindBarPresentationDelegate,
                                     ToolbarHeightProviderForFullscreen,
                                     WebStateListObserving,
                                     UIGestureRecognizerDelegate,
                                     URLLoadingObserver> {
  // The dependency factory passed on initialization.  Used to vend objects used
  // by the BVC.
  BrowserViewControllerDependencyFactory* _dependencyFactory;

  // Identifier for each animation of an NTP opening.
  NSInteger _NTPAnimationIdentifier;

  // Facade objects used by |_toolbarCoordinator|.
  // Must outlive |_toolbarCoordinator|.
  std::unique_ptr<LocationBarModelDelegateIOS> _locationBarModelDelegate;
  std::unique_ptr<LocationBarModel> _locationBarModel;

  // Controller for edge swipe gestures for page and tab navigation.
  SideSwipeController* _sideSwipeController;

  // Handles displaying the context menu for all form factors.
  ContextMenuCoordinator* _contextMenuCoordinator;

  // Handles presentation of JavaScript dialogs.
  std::unique_ptr<web::JavaScriptDialogPresenter> _javaScriptDialogPresenter;

  // Keyboard commands provider.  It offloads most of the keyboard commands
  // management off of the BVC.
  KeyCommandsProvider* _keyCommandsProvider;

  // Used to display the Voice Search UI.  Nil if not visible.
  scoped_refptr<VoiceSearchController> _voiceSearchController;

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

  // Whether or not Incognito* is enabled.
  BOOL _isOffTheRecord;

  // The last point within |contentArea| that's received a touch.
  CGPoint _lastTapPoint;

  // The time at which |_lastTapPoint| was most recently set.
  CFTimeInterval _lastTapTime;

  // The controller that shows the bookmarking UI after the user taps the star
  // button.
  BookmarkInteractionController* _bookmarkInteractionController;

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

  // Bridges C++ WebStateListObserver methods to this BrowserViewController.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Presenter for in-product help bubbles.
  BubblePresenter* _bubblePresenter;
}

// Activates/deactivates the object. This will enable/disable the ability for
// this object to browse, and to have live UIWebViews associated with it. While
// not active, the UI will not react to changes in the tab model, so generally
// an inactive BVC should not be visible.
@property(nonatomic, assign, getter=isActive) BOOL active;
// The Browser whose UI is managed by this instance.
@property(nonatomic, assign) Browser* browser;
// Browser container view controller.
@property(nonatomic, strong)
    BrowserContainerViewController* browserContainerViewController;
// Invisible button used to dismiss the keyboard.
@property(nonatomic, strong) UIButton* typingShield;
// Command dispatcher.
@property(nonatomic, weak) CommandDispatcher* commandDispatcher;
// The browser's side swipe controller.  Lazily instantiated on the first call.
@property(nonatomic, strong, readonly) SideSwipeController* sideSwipeController;
// The object that manages keyboard commands on behalf of the BVC.
@property(nonatomic, strong, readonly) KeyCommandsProvider* keyCommandsProvider;
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
// Whether web usage is enabled for the WebStates in |self.browser|.
@property(nonatomic, assign, getter=isWebUsageEnabled) BOOL webUsageEnabled;
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
// type is NONE if there is no visible page.
@property(nonatomic, assign, readonly) web::UserAgentType userAgentType;

// Returns the header views, all the chrome on top of the page, including the
// ones that cannot be scrolled off screen by full screen.
@property(nonatomic, strong, readonly) NSArray<HeaderDefinition*>* headerViews;

// Coordinator for the popup menus.
@property(nonatomic, strong) PopupMenuCoordinator* popupMenuCoordinator;

@property(nonatomic, strong) BubblePresenter* bubblePresenter;

// Command handler for text zoom commands
@property(nonatomic, weak) id<TextZoomCommands> textZoomHandler;

// Command handler for help commands
@property(nonatomic, weak) id<HelpCommands> helpHandler;

// Command handler for omnibox commands
@property(nonatomic, weak) id<OmniboxCommands> omniboxHandler;

// The FullscreenController.
@property(nonatomic, assign) FullscreenController* fullscreenController;

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

// Whether the keyboard observer helper is viewed
@property(nonatomic, strong) KeyboardObserverHelper* observer;

// Helper method to check whether the NewTabPageTabHelper is valid and Active
// for |self.currentWebState|.
@property(nonatomic, assign, readonly, getter=isNTPActiveForCurrentWebState)
    BOOL NTPActiveForCurrentWebState;

// The coordinator that shows the Send Tab To Self UI.
@property(nonatomic, strong) SendTabToSelfCoordinator* sendTabToSelfCoordinator;

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
// with DCHECKs) regarding |self.browserState|, |self.browser|, and [self
// isViewLoaded].

// Updates non-view-related functionality with the given browser and tab
// model.
// Does not matter whether or not the view has been loaded.
- (void)updateWithBrowser:(Browser*)browser;
// On iOS7, iPad should match iOS6 status bar.  Install a simple black bar under
// the status bar to mimic this layout.
- (void)installFakeStatusBar;
// Builds the UI parts of tab strip and the toolbar.  Does not matter whether
// or not browser state and tab model are valid.
- (void)buildToolbarAndTabStrip;
// Sets up the constraints on the toolbar.
- (void)addConstraintsToToolbar;
// Updates view-related functionality with the given tab model and browser
// state. The view must have been loaded.  Uses |self.browserState| and
// |self.browser|.
- (void)addUIFunctionalityForBrowserAndBrowserState;
// Sets the correct frame and hierarchy for subviews and helper views.  Only
// insert views on |initialLayout|.
- (void)setUpViewLayout:(BOOL)initialLayout;
// Makes |webState| the currently visible WebState, displaying its view.
- (void)displayWebState:(web::WebState*)webState;
// Initializes the bookmark interaction controller if not already initialized.
- (void)initializeBookmarkInteractionController;

// UI Configuration, update and Layout
// -----------------------------------
// Updates the toolbar display based on the current tab.
- (void)updateToolbar;
// Starts or stops broadcasting the toolbar UI and main content UI depending on
// whether the BVC is visible and active.
- (void)updateBroadcastState;
// Dismisses popups and modal dialogs that are displayed above the BVC upon size
// changes (e.g. rotation, resizing,…) or when the accessibility escape gesture
// is performed.
// TODO(crbug.com/522721): Support size changes for all popups and modal
// dialogs.
- (void)dismissPopups;
// Returns the footer view if one exists (e.g. the voice search bar).
- (UIView*)footerView;
// Returns the appropriate frame for the NTP.
- (CGRect)ntpFrameForWebState:(web::WebState*)webState;
// Sets the frame for the headers.
- (void)setFramesForHeaders:(NSArray<HeaderDefinition*>*)headers
                   atOffset:(CGFloat)headerOffset;

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
// Add all delegates to the provided |webState|.
- (void)installDelegatesForWebState:(web::WebState*)webState;
// Remove delegates from the provided |webState|.
- (void)uninstallDelegatesForWebState:(web::WebState*)webState;
// Called when a |webState| is selected in the WebStateList. Make any required
// view changes. The notification will not be sent when the |webState| is
// already the selected WebState. |notifyToolbar| indicates whether the toolbar
// is notified that the webState has changed.
- (void)webStateSelected:(web::WebState*)webState
           notifyToolbar:(BOOL)notifyToolbar;

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

- (instancetype)initWithBrowser:(Browser*)browser
                 dependencyFactory:
                     (BrowserViewControllerDependencyFactory*)factory
    browserContainerViewController:
        (BrowserContainerViewController*)browserContainerViewController {
  self = [super initWithNibName:nil bundle:base::mac::FrameworkBundle()];
  if (self) {
    DCHECK(factory);

    _browserContainerViewController = browserContainerViewController;
    _dependencyFactory = factory;
    self.commandDispatcher = browser->GetCommandDispatcher();
    self.textZoomHandler =
        HandlerForProtocol(self.commandDispatcher, TextZoomCommands);
    [self.commandDispatcher
        startDispatchingToTarget:self
                     forProtocol:@protocol(BrowserCommands)];

    _toolbarCoordinatorAdaptor =
        [[ToolbarCoordinatorAdaptor alloc] initWithDispatcher:self.dispatcher];
    self.toolbarInterface = _toolbarCoordinatorAdaptor;

    // TODO(crbug.com/1024288): Remove these lines along the legacy code
    // removal.
    if (!IsDownloadInfobarMessagesUIEnabled()) {
      _downloadManagerCoordinator = [[DownloadManagerCoordinator alloc]
          initWithBaseViewController:_browserContainerViewController
                             browser:browser];
      _downloadManagerCoordinator.presenter =
          [[VerticalAnimationContainer alloc] init];
    }

    _webStateDelegate.reset(new web::WebStateDelegateBridge(self));
    _inNewTabAnimation = NO;

    if (fullscreen::features::ShouldScopeFullscreenControllerToBrowser()) {
      _fullscreenController = FullscreenController::FromBrowser(browser);
    } else {
      _fullscreenController =
          FullscreenController::FromBrowserState(browser->GetBrowserState());
    }

    _footerFullscreenProgress = 1.0;

    _observer = [[KeyboardObserverHelper alloc] init];
    if (browser)
      [self updateWithBrowser:browser];
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
      FindInPageCommands,
      PasswordBreachCommands,
      ToolbarCommands>)dispatcher {
  return static_cast<
      id<ApplicationCommands, BrowserCommands, FindInPageCommands,
         PasswordBreachCommands, ToolbarCommands>>(self.commandDispatcher);
}

- (UIView*)contentArea {
  return self.browserContainerViewController.view;
}

- (BOOL)isPlayingTTS {
  return _voiceSearchController && _voiceSearchController->IsPlayingAudio();
}

- (ChromeBrowserState*)browserState {
  return self.browser ? self.browser->GetBrowserState() : nullptr;
}

- (void)setInfobarBannerOverlayContainerViewController:
    (UIViewController*)infobarBannerOverlayContainerViewController {
  if (_infobarBannerOverlayContainerViewController ==
      infobarBannerOverlayContainerViewController) {
    return;
  }

  _infobarBannerOverlayContainerViewController =
      infobarBannerOverlayContainerViewController;
  if (!_infobarBannerOverlayContainerViewController)
    return;

  DCHECK_EQ(_infobarBannerOverlayContainerViewController.parentViewController,
            self);
  DCHECK_EQ(_infobarBannerOverlayContainerViewController.view.superview,
            self.view);
  [self updateOverlayContainerOrder];
}

- (void)setInfobarModalOverlayContainerViewController:
    (UIViewController*)infobarModalOverlayContainerViewController {
  if (_infobarModalOverlayContainerViewController ==
      infobarModalOverlayContainerViewController) {
    return;
  }

  _infobarModalOverlayContainerViewController =
      infobarModalOverlayContainerViewController;
  if (!_infobarModalOverlayContainerViewController)
    return;

  DCHECK_EQ(_infobarModalOverlayContainerViewController.parentViewController,
            self);
  DCHECK_EQ(_infobarModalOverlayContainerViewController.view.superview,
            self.view);
  [self updateOverlayContainerOrder];
}

#pragma mark - Private Properties

- (SideSwipeController*)sideSwipeController {
  if (!_sideSwipeController) {
    _sideSwipeController =
        [[SideSwipeController alloc] initWithBrowser:self.browser];
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

- (KeyCommandsProvider*)keyCommandsProvider {
  if (!_keyCommandsProvider) {
    _keyCommandsProvider = [_dependencyFactory newKeyCommandsProvider];
  }
  return _keyCommandsProvider;
}

- (BOOL)canShowTabStrip {
  return IsRegularXRegularSizeClass(self);
}

- (web::UserAgentType)userAgentType {
  web::WebState* webState = self.currentWebState;
  if (!webState)
    return web::UserAgentType::NONE;
  web::NavigationItem* visibleItem =
      webState->GetNavigationManager()->GetVisibleItem();
  if (!visibleItem)
    return web::UserAgentType::NONE;

  return visibleItem->GetUserAgentType(self.view);
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
  [self updateBroadcastState];
}

- (void)setBroadcasting:(BOOL)broadcasting {
  if (_broadcasting == broadcasting)
    return;
  _broadcasting = broadcasting;

  ChromeBroadcaster* broadcaster = self.fullscreenController->broadcaster();
  if (_broadcasting) {
    _toolbarUIUpdater = [[LegacyToolbarUIUpdater alloc]
        initWithToolbarUI:[[ToolbarUIState alloc] init]
             toolbarOwner:self
             webStateList:self.browser->GetWebStateList()];
    [_toolbarUIUpdater startUpdating];
    StartBroadcastingToolbarUI(_toolbarUIUpdater.toolbarUI, broadcaster);

    _mainContentUIUpdater = [[MainContentUIStateUpdater alloc]
        initWithState:[[MainContentUIState alloc] init]];
    _webMainContentUIForwarder = [[WebScrollViewMainContentUIForwarder alloc]
        initWithUpdater:_mainContentUIUpdater
           webStateList:self.browser->GetWebStateList()];
    StartBroadcastingMainContentUI(self, broadcaster);

    if (!fullscreen::features::ShouldScopeFullscreenControllerToBrowser()) {
      self.fullscreenController->SetWebStateList(
          self.browser->GetWebStateList());
    }

    _fullscreenUIUpdater =
        std::make_unique<FullscreenUIUpdater>(self.fullscreenController, self);
    [self updateForFullscreenProgress:self.fullscreenController->GetProgress()];
  } else {
    StopBroadcastingToolbarUI(broadcaster);
    StopBroadcastingMainContentUI(broadcaster);
    [_toolbarUIUpdater stopUpdating];
    _toolbarUIUpdater = nil;
    _mainContentUIUpdater = nil;
    [_webMainContentUIForwarder disconnect];
    _webMainContentUIForwarder = nil;

    _fullscreenUIUpdater = nullptr;

    if (!fullscreen::features::ShouldScopeFullscreenControllerToBrowser()) {
      self.fullscreenController->SetWebStateList(nullptr);
    }
  }
}

- (BOOL)isWebUsageEnabled {
  return self.browserState && !_isShutdown &&
         WebUsageEnablerBrowserAgent::FromBrowser(self.browser)
             ->IsWebUsageEnabled();
}

- (void)setWebUsageEnabled:(BOOL)webUsageEnabled {
  if (!self.browserState || _isShutdown)
    return;
  WebUsageEnablerBrowserAgent::FromBrowser(self.browser)
      ->SetWebUsageEnabled(webUsageEnabled);
}

- (void)setInNewTabAnimation:(BOOL)inNewTabAnimation {
  if (_inNewTabAnimation == inNewTabAnimation)
    return;
  _inNewTabAnimation = inNewTabAnimation;
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
    if (self.toolbarAccessoryPresenter.isPresenting) {
      [results addObject:[HeaderDefinition
                             definitionWithView:self.toolbarAccessoryPresenter
                                                    .backgroundView
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
  return height - statusBarOffset;
}

- (web::WebState*)currentWebState {
  return self.browser ? self.browser->GetWebStateList()->GetActiveWebState()
                      : nullptr;
}

- (BOOL)isNTPActiveForCurrentWebState {
  if (self.currentWebState) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(self.currentWebState);
    return (NTPHelper && NTPHelper->IsActive());
  }
  return NO;
}

#pragma mark - Public methods

- (id<ActivityServicePositioner>)activityServicePositioner {
  return [self.primaryToolbarCoordinator activityServicePositioner];
}

- (void)setPrimary:(BOOL)primary {
  TabUsageRecorderBrowserAgent* tabUsageRecorder =
      TabUsageRecorderBrowserAgent::FromBrowser(_browser);
  if (tabUsageRecorder) {
    tabUsageRecorder->RecordPrimaryTabModelChange(
        primary, _browser->GetWebStateList()->GetActiveWebState());
  }
  if (primary) {
    [self updateBroadcastState];
  }
}

- (void)shieldWasTapped:(id)sender {
  [self.omniboxHandler cancelOmniboxEdit];
}

- (void)userEnteredTabSwitcher {
  // TODO(crbug.com/977761): In preparation for dismissing BVC, make sure any
  // ongoing ViewController presentations are stopped.
  if (IsInfobarUIRebootEnabled() &&
      (self.infobarContainerCoordinator.infobarBannerState !=
       InfobarBannerPresentationState::NotPresented)) {
    [self.infobarContainerCoordinator dismissInfobarBannerAnimated:NO
                                                        completion:nil];
  }
  [self.bubblePresenter userEnteredTabSwitcher];
}

- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox {
  NSTimeInterval startTime = [NSDate timeIntervalSinceReferenceDate];
  BOOL offTheRecord = self.isOffTheRecord;
  ProceduralBlock oldForegroundTabWasAddedCompletionBlock =
      self.foregroundTabWasAddedCompletionBlock;
  id<OmniboxCommands> omniboxCommandHandler = self.omniboxHandler;
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
      [omniboxCommandHandler focusOmnibox];
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

  UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
  params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.in_incognito = self.isOffTheRecord;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
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
  [self.dispatcher closeFindInPage];
  [self.textZoomHandler closeTextZoom];
  [[self viewForWebState:self.currentWebState] endEditing:NO];

  // Ensure that voice search objects are created.
  [self ensureVoiceSearchControllerCreated];

  // Present voice search.
  _voiceSearchController->StartRecognition(self, self.currentWebState,
                                           self.browser);
  [self.omniboxHandler cancelOmniboxEdit];
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
  [self showActivityOverlay:!active];

  if (self.browserState) {
    ActiveStateManager* active_state_manager =
        ActiveStateManager::FromBrowserState(self.browserState);
    active_state_manager->SetActive(active);

    TextToSpeechPlaybackControllerFactory::GetInstance()
        ->GetForBrowserState(self.browserState)
        ->SetEnabled(_active);
  }

  self.webUsageEnabled = active;
  [self updateBroadcastState];

  // Stop the NTP on web usage toggle. This happens when clearing browser
  // data, and forces the NTP to be recreated in -displayWebState below.
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
    if (self.currentWebState)
      [self displayWebState:self.currentWebState];
  }

  [self setNeedsStatusBarAppearanceUpdate];
}

- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  [_bookmarkInteractionController dismissBookmarkModalControllerAnimated:NO];
  [_bookmarkInteractionController dismissSnackbar];
  if (dismissOmnibox) {
    [self.omniboxHandler cancelOmniboxEdit];
  }
  [self.helpHandler hideAllHelpBubbles];
  if (_voiceSearchController)
    _voiceSearchController->DismissMicPermissionsHelp();

  web::WebState* webState = self.currentWebState;

  if (webState) {
    if (self.isNTPActiveForCurrentWebState) {
      [_ntpCoordinatorsForWebStates[webState] dismissModals];
    }
    [self.dispatcher closeFindInPage];
    [self.textZoomHandler closeTextZoom];
  }

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
    self.fullscreenController->ExitFullscreen();
    const CGFloat kAnimatedViewSize = 50;
    BackgroundTabAnimationView* animatedView =
        [[BackgroundTabAnimationView alloc]
            initWithFrame:CGRectMake(0, 0, kAnimatedViewSize, kAnimatedViewSize)
                incognito:self.isOffTheRecord];
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

  if (self.browserState) {
    TextToSpeechPlaybackController* controller =
        TextToSpeechPlaybackControllerFactory::GetInstance()
            ->GetForBrowserState(self.browserState);
    if (controller)
      controller->SetWebStateList(nullptr);

    UrlLoadingNotifierBrowserAgent* notifier =
        UrlLoadingNotifierBrowserAgent::FromBrowser(self.browser);
    if (notifier)
      notifier->RemoveObserver(_URLLoadingObserverBridge.get());
  }

  // Uninstall delegates so that any delegate callbacks triggered by subsequent
  // WebStateDestroyed() signals are not handled.
  WebStateList* webStateList = self.browser->GetWebStateList();
  for (int index = 0; index < webStateList->count(); ++index)
    [self uninstallDelegatesForWebState:webStateList->GetWebStateAt(index)];

  // Disconnect child coordinators.
  [self.popupMenuCoordinator stop];
  [self.tabStripCoordinator stop];
  self.tabStripCoordinator = nil;
  self.tabStripView = nil;

  [self.commandDispatcher stopDispatchingToTarget:self.bubblePresenter];
  self.bubblePresenter = nil;

  [self.commandDispatcher stopDispatchingToTarget:self];
  self.browser->GetWebStateList()->RemoveObserver(_webStateListObserver.get());
  self.browser = nullptr;

  [self.primaryToolbarCoordinator stop];
  self.primaryToolbarCoordinator = nil;
  [self.secondaryToolbarContainerCoordinator stop];
  self.secondaryToolbarContainerCoordinator = nil;
  [self.secondaryToolbarCoordinator stop];
  self.secondaryToolbarCoordinator = nil;
  [_downloadManagerCoordinator stop];
  _downloadManagerCoordinator = nil;
  self.toolbarInterface = nil;
  [self.infobarContainerCoordinator stop];
  self.infobarContainerCoordinator = nil;
  _sideSwipeController = nil;
  _webStateListObserver.reset();
  _allWebStateObservationForwarder = nullptr;
  if (_voiceSearchController) {
    _voiceSearchController->SetDispatcher(nil);
    _voiceSearchController = nullptr;
  }
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - NSObject

- (BOOL)accessibilityPerformEscape {
  [self dismissPopups];
  return YES;
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  if (![self shouldRegisterKeyboardCommands]) {
    return nil;
  }

  UIResponder* firstResponder = GetFirstResponder();

  return [self.keyCommandsProvider
      keyCommandsForConsumer:self
          baseViewController:self
                  dispatcher:self.dispatcher
              omniboxHandler:self.omniboxHandler
                 editingText:[firstResponder
                                 isKindOfClass:[UITextField class]] ||
                             [firstResponder
                                 isKindOfClass:[UITextView class]] ||
                             [self.observer isKeyboardOnScreen]];
}

#pragma mark - UIResponder helpers

// Whether the BVC should declare keyboard commands.
- (BOOL)shouldRegisterKeyboardCommands {
  if ([self presentedViewController])
    return NO;

  if (_voiceSearchController && _voiceSearchController->IsVisible())
    return NO;

  return YES;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  CGRect initialViewsRect = self.view.bounds;
  UIViewAutoresizing initialViewAutoresizing =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  // Clip the content to the bounds of the view. This prevents the WebView to
  // overflow outside of the BVC, which is particularly visible during rotation.
  // The WebView is overflowing its bounds to be displayed below the toolbars.
  self.view.clipsToBounds = YES;

  self.contentArea.frame = initialViewsRect;

  // Create the typing shield.  It is initially hidden, and is made visible when
  // the keyboard appears.
  self.typingShield = [[UIButton alloc] initWithFrame:initialViewsRect];
  self.typingShield.hidden = YES;
  self.typingShield.autoresizingMask = initialViewAutoresizing;
  self.typingShield.accessibilityIdentifier = @"Typing Shield";
  self.typingShield.accessibilityLabel = l10n_util::GetNSString(IDS_CANCEL);

  [self.typingShield addTarget:self
                        action:@selector(shieldWasTapped:)
              forControlEvents:UIControlEventTouchUpInside];
  self.view.autoresizingMask = initialViewAutoresizing;
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

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
  if (self.browser && self.browserState)
    [self addUIFunctionalityForBrowserAndBrowserState];

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

  if (self.isNTPActiveForCurrentWebState && self.webUsageEnabled) {
    _ntpCoordinatorsForWebStates[self.currentWebState]
        .viewController.view.frame =
        [self ntpFrameForWebState:self.currentWebState];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.viewVisible = YES;
  [self updateBroadcastState];
  [_toolbarUIUpdater updateState];
  [self.infobarContainerCoordinator baseViewDidAppear];

  // |viewDidAppear| can be called after |browserState| is destroyed. Since
  // |presentBubblesIfEligible| requires that |self.browserState| is not NULL,
  // check for |self.browserState| before calling the presenting the bubbles.
  if (self.browserState) {
    [self.helpHandler showHelpBubbleIfEligible];
    [self.helpHandler showLongPressHelpBubbleIfEligible];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  self.visible = YES;

  // If the controller is suspended, or has been paged out due to low memory,
  // updating the view will be handled when it's displayed again.
  if (!self.webUsageEnabled || !self.contentArea)
    return;
  // Update the displayed WebState (if any; the switcher may not have created
  // one yet) in case it changed while showing the switcher.
  if (self.currentWebState)
    [self displayWebState:self.currentWebState];
}

- (void)viewWillDisappear:(BOOL)animated {
  self.viewVisible = NO;
  [self updateBroadcastState];
  web::WebState* activeWebState =
      self.browser ? self.browser->GetWebStateList()->GetActiveWebState()
                   : nullptr;
  if (activeWebState) {
    activeWebState->WasHidden();
    if (!self.presentedViewController)
      activeWebState->SetKeepRenderProcessAlive(false);
  }

  // TODO(crbug.com/976411):This should probably move to the BannerVC once/if
  // the dismiss event from BVC is observable.
  if (IsInfobarUIRebootEnabled() &&
      !base::FeatureList::IsEnabled(kInfobarOverlayUI)) {
    [self.infobarContainerCoordinator baseViewWillDisappear];
    if (self.infobarContainerCoordinator.infobarBannerState !=
        InfobarBannerPresentationState::NotPresented) {
      [self.infobarContainerCoordinator dismissInfobarBannerAnimated:NO
                                                          completion:nil];
    }
  }
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

  // After |-shutdown| is called, |self.browserState| is invalid and will cause
  // a crash.
  if (!self.browserState || _isShutdown)
    return;

  self.fullscreenController->BrowserTraitCollectionChangedBegin();

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
  if (self.currentWebState) {
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
    [self.dispatcher hideFindUI];
    [self.textZoomHandler hideTextZoomUI];
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

  self.fullscreenController->BrowserTraitCollectionChangedEnd();
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self dismissPopups];

  [coordinator
      animateAlongsideTransition:^(
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
      NSArray* topObjects = [mainBundle loadNibNamed:@"LaunchScreen"
                                               owner:self
                                             options:nil];
      UIViewController* launchScreenController =
          base::mac::ObjCCastStrict<UIViewController>([topObjects lastObject]);
      // |launchScreenView| is loaded as an autoreleased object, and is retained
      // by the |completion| block below.
      UIView* launchScreenView = launchScreenController.view;
      launchScreenView.userInteractionEnabled = NO;
      // TODO(crbug.com/1011155): Displaying the launch screen is a hack to hide
      // the build up of the UI from the user. To implement the hack, this view
      // controller uses information that it should not know or care about: this
      // BVC is contained and its parent bounds to the full screen.
      launchScreenView.frame = self.parentViewController.view.bounds;
      [self.parentViewController.view addSubview:launchScreenView];
      [launchScreenView setNeedsLayout];
      [launchScreenView layoutIfNeeded];

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

  // TODO(crbug.com/965688): An Infobar message is currently the only presented
  // controller that allows interaction with the rest of the App while its being
  // presented. Dismiss it in case the user or system has triggered another
  // presentation.
  if (IsInfobarUIRebootEnabled() &&
      !base::FeatureList::IsEnabled(kInfobarOverlayUI) &&
      (self.infobarContainerCoordinator.infobarBannerState !=
       InfobarBannerPresentationState::NotPresented)) {
    [self.infobarContainerCoordinator
        dismissInfobarBannerAnimated:NO
                          completion:^{
                            [super
                                presentViewController:viewControllerToPresent
                                             animated:flag
                                           completion:finalCompletionHandler];
                          }];
  } else {
    [super presentViewController:viewControllerToPresent
                        animated:flag
                      completion:finalCompletionHandler];
  }
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

- (void)updateWithBrowser:(Browser*)browser {
  DCHECK(browser);
  DCHECK(!self.browser);
  self.browser = browser;
  _isOffTheRecord = self.browserState->IsOffTheRecord();

  _webStateObserverBridge = std::make_unique<web::WebStateObserverBridge>(self);
  _allWebStateObservationForwarder =
      std::make_unique<AllWebStateObservationForwarder>(
          self.browser->GetWebStateList(), _webStateObserverBridge.get());

  _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  self.browser->GetWebStateList()->AddObserver(_webStateListObserver.get());
  _URLLoadingObserverBridge = std::make_unique<UrlLoadingObserverBridge>(self);
  UrlLoadingNotifierBrowserAgent::FromBrowser(self.browser)
      ->AddObserver(_URLLoadingObserverBridge.get());

  WebStateList* webStateList = self.browser->GetWebStateList();
  for (int index = 0; index < webStateList->count(); ++index)
    [self installDelegatesForWebState:webStateList->GetWebStateAt(index)];

  self.imageSaver =
      [[ImageSaver alloc] initWithBaseViewController:self browser:self.browser];
  self.imageCopier =
      [[ImageCopier alloc] initWithBaseViewController:self
                                              browser:self.browser];

  // Set the TTS playback controller's WebStateList.
  TextToSpeechPlaybackControllerFactory::GetInstance()
      ->GetForBrowserState(self.browserState)
      ->SetWebStateList(self.browser->GetWebStateList());

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
      new LocationBarModelDelegateIOS(self.browser->GetWebStateList()));
  _locationBarModel = std::make_unique<LocationBarModelImpl>(
      _locationBarModelDelegate.get(), kMaxURLDisplayChars);
  self.helper = [_dependencyFactory newBrowserViewControllerHelper];

  PrimaryToolbarCoordinator* topToolbarCoordinator =
      [[PrimaryToolbarCoordinator alloc] initWithBrowser:self.browser];
  self.primaryToolbarCoordinator = topToolbarCoordinator;
  topToolbarCoordinator.delegate = self;
  topToolbarCoordinator.popupPresenterDelegate = self;
  topToolbarCoordinator.longPressDelegate = self.popupMenuCoordinator;
  [topToolbarCoordinator start];

  SecondaryToolbarCoordinator* bottomToolbarCoordinator =
      [[SecondaryToolbarCoordinator alloc] initWithBrowser:self.browser];
  self.secondaryToolbarCoordinator = bottomToolbarCoordinator;
  bottomToolbarCoordinator.longPressDelegate = self.popupMenuCoordinator;

  if (base::FeatureList::IsEnabled(
          toolbar_container::kToolbarContainerEnabled)) {
    self.secondaryToolbarContainerCoordinator =
        [[ToolbarContainerCoordinator alloc]
            initWithBrowser:self.browser
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
    self.tabStripCoordinator = [[TabStripLegacyCoordinator alloc]
        initWithBaseViewController:self
                           browser:self.browser];
    self.tabStripCoordinator.presentationProvider = self;
    self.tabStripCoordinator.animationWaitDuration =
        kLegacyFullscreenControllerToolbarAnimationDuration;
    self.tabStripCoordinator.longPressDelegate = self.popupMenuCoordinator;

    UILayoutGuide* guide =
        [[NamedGuide alloc] initWithName:kTabStripTabSwitcherGuide];
    [self.view addLayoutGuide:guide];
    [self.tabStripCoordinator start];
  }

  if (!base::FeatureList::IsEnabled(kInfobarOverlayUI)) {
    // Create the Infobar Container Coordinator.
    self.infobarContainerCoordinator = [[InfobarContainerCoordinator alloc]
        initWithBaseViewController:self
                           browser:self.browser];
    self.infobarContainerCoordinator.positioner = self;
    self.infobarContainerCoordinator.syncPresenter = self;
    [self.infobarContainerCoordinator start];
  }
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
// both browser state and browser are valid.
- (void)addUIFunctionalityForBrowserAndBrowserState {
  DCHECK(self.browserState);
  DCHECK(_locationBarModel);
  DCHECK(self.browser);
  DCHECK([self isViewLoaded]);

  [self.sideSwipeController addHorizontalGesturesToView:self.view];

  // TODO(crbug.com/1024288): Remove these lines along the legacy code removal.
  if (!IsDownloadInfobarMessagesUIEnabled()) {
    // DownloadManagerCoordinator is already created.
    DCHECK(_downloadManagerCoordinator);
    _downloadManagerCoordinator.bottomMarginHeightAnchor =
        [NamedGuide guideWithName:kSecondaryToolbarGuide view:self.contentArea]
            .heightAnchor;
  }

  self.bubblePresenter =
      [[BubblePresenter alloc] initWithBrowserState:self.browserState
                                           delegate:self
                                 rootViewController:self];
  self.bubblePresenter.toolbarHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), ToolbarCommands);
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self.bubblePresenter
                   forProtocol:@protocol(HelpCommands)];
  self.helpHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), HelpCommands);

  self.popupMenuCoordinator =
      [[PopupMenuCoordinator alloc] initWithBaseViewController:self
                                                       browser:self.browser];
  self.popupMenuCoordinator.bubblePresenter = self.bubblePresenter;
  self.popupMenuCoordinator.UIUpdater = _toolbarCoordinatorAdaptor;
  [self.popupMenuCoordinator start];

  self.primaryToolbarCoordinator.longPressDelegate = self.popupMenuCoordinator;
  self.secondaryToolbarCoordinator.longPressDelegate =
      self.popupMenuCoordinator;
  self.tabStripCoordinator.longPressDelegate = self.popupMenuCoordinator;

  self.omniboxHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);

  _sadTabCoordinator = [[SadTabCoordinator alloc]
      initWithBaseViewController:self.browserContainerViewController
                         browser:self.browser];
  _sadTabCoordinator.overscrollDelegate = self;

  // If there are any existing SadTabHelpers in
  // |self.browser->GetWebStateList()|, update the helpers delegate with the new
  // |_sadTabCoordinator|.
  DCHECK(_sadTabCoordinator);
  WebStateList* webStateList = self.browser->GetWebStateList();
  for (int i = 0; i < webStateList->count(); i++) {
    SadTabTabHelper* sadTabHelper =
        SadTabTabHelper::FromWebState(webStateList->GetWebStateAt(i));
    sadTabHelper->SetDelegate(_sadTabCoordinator);
  }
}

// Set the frame for the various views. View must be loaded.
- (void)setUpViewLayout:(BOOL)initialLayout {
  DCHECK([self isViewLoaded]);

  CGFloat topInset = self.view.safeAreaInsets.top;

  // Update the fake toolbar background height.
  CGRect fakeStatusBarFrame = _fakeStatusBarView.frame;
  fakeStatusBarFrame.size.height = topInset;
  _fakeStatusBarView.frame = fakeStatusBarFrame;

  if (initialLayout) {
    // Add the toolbars as child view controllers.
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

    // Add the primary toolbar.  On iPad, it should be behind the tab strip.
    UIView* primaryToolbarView =
        self.primaryToolbarCoordinator.viewController.view;
    if (IsIPadIdiom()) {
      [self.view insertSubview:primaryToolbarView
                  belowSubview:self.tabStripView];
    } else {
      [self.view addSubview:primaryToolbarView];
    }

    // Add the secondary toolbar.
    if (self.secondaryToolbarCoordinator) {
      if (base::FeatureList::IsEnabled(
              toolbar_container::kToolbarContainerEnabled)) {
        // Add the container view to the hierarchy.
        UIView* containerView =
            self.secondaryToolbarContainerCoordinator.viewController.view;
        [self.view insertSubview:containerView aboveSubview:primaryToolbarView];
      } else {
        // Create the container view for the secondary toolbar and add it to
        // the hierarchy
        UIView* container = [[LegacyToolbarContainerView alloc] init];
        container.translatesAutoresizingMaskIntoConstraints = NO;
        [container
            addSubview:self.secondaryToolbarCoordinator.viewController.view];
        [self.view insertSubview:container aboveSubview:primaryToolbarView];
        self.secondaryToolbarContainerView = container;
      }
    }

    // Create the NamedGuides and add them to the browser view.
    NSArray<GuideName*>* guideNames = @[
      kContentAreaGuide,
      kPrimaryToolbarGuide,
      kBadgeOverflowMenuGuide,
      kOmniboxGuide,
      kOmniboxLeadingImageGuide,
      kOmniboxTextFieldGuide,
      kBackButtonGuide,
      kForwardButtonGuide,
      kToolsMenuGuide,
      kTabSwitcherGuide,
      kTranslateInfobarOptionsGuide,
      kNewTabButtonGuide,
      kSecondaryToolbarGuide,
      kVoiceSearchButtonGuide,
    ];
    AddNamedGuidesToView(guideNames, self.view);

    // Configure the content area guide.
    NamedGuide* contentAreaGuide = [NamedGuide guideWithName:kContentAreaGuide
                                                        view:self.view];

    // Constrain top to bottom of top toolbar.
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

    // Complete child UIViewController containment flow now that the views are
    // finished being added.
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

  // Resize the typing shield to cover the entire browser view and bring it to
  // the front.
  self.typingShield.frame = self.contentArea.frame;
  [self.view bringSubviewToFront:self.typingShield];

  // Move the overlay containers in front of the hierarchy.
  [self updateOverlayContainerOrder];
}

- (void)displayWebState:(web::WebState*)webState {
  DCHECK(webState);
  [self loadViewIfNeeded];

  // Set this before triggering any of the possible page loads below.
  webState->SetKeepRenderProcessAlive(true);

  if (!self.inNewTabAnimation) {
    // Hide findbar.  |updateToolbar| will restore the findbar later.
    [self.dispatcher hideFindUI];
    [self.textZoomHandler hideTextZoomUI];

    // Make new content visible, resizing it first as the orientation may
    // have changed from the last time it was displayed.
    CGRect webStateViewFrame = self.contentArea.bounds;
    if (!ios::GetChromeBrowserProvider()
             ->GetFullscreenProvider()
             ->IsInitialized()) {
      // If the FullscreenProvider is initialized, the WebState view is not
      // resized, and should always match the bounds of the content area.  When
      // the provider is not initialized, viewport insets resize the webview, so
      // they should be accounted for here to prevent animation jitter.
      UIEdgeInsets viewportInsets =
          self.fullscreenController->GetCurrentViewportInsets();
      webStateViewFrame =
          UIEdgeInsetsInsetRect(webStateViewFrame, viewportInsets);
    }
    [self viewForWebState:webState].frame = webStateViewFrame;

    [_toolbarUIUpdater updateState];
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(webState);
    if (NTPHelper && NTPHelper->IsActive()) {
      UIViewController* viewController =
          _ntpCoordinatorsForWebStates[webState].viewController;
      viewController.view.frame = [self ntpFrameForWebState:webState];
      // TODO(crbug.com/873729): For a newly created WebState, the session will
      // not be restored until LoadIfNecessary call. Remove when fixed.
      webState->GetNavigationManager()->LoadIfNecessary();
      self.browserContainerViewController.contentView = nil;
      self.browserContainerViewController.contentViewController =
          viewController;
    } else {
      self.browserContainerViewController.contentView =
          [self viewForWebState:webState];
    }
  }
  [self updateToolbar];

  // TODO(crbug.com/971364): The webState is not necessarily added to the view
  // hierarchy, even though the bookkeeping says that the WebState is visible.
  // Do not DCHECK([webState->GetView() window]) here since this is a known
  // issue.
  webState->WasShown();
}

- (void)initializeBookmarkInteractionController {
  if (_bookmarkInteractionController)
    return;
  _bookmarkInteractionController =
      [[BookmarkInteractionController alloc] initWithBrowser:self.browser
                                            parentController:self];
}

- (void)updateOverlayContainerOrder {
  // Both infobar overlay container views should exist in front of the entire
  // browser UI, and the banner container should appear behind the modal
  // container.
  [self bringOverlayContainerToFront:
            self.infobarBannerOverlayContainerViewController];
  [self bringOverlayContainerToFront:
            self.infobarModalOverlayContainerViewController];
}

- (void)bringOverlayContainerToFront:
    (UIViewController*)containerViewController {
  [self.view bringSubviewToFront:containerViewController.view];
  // If |containerViewController| is presenting a view over its current context,
  // its presentation container view is added as a sibling to
  // |containerViewController|'s view. This presented view should be brought in
  // front of the container view.
  UIView* presentedContainerView =
      containerViewController.presentedViewController.presentationController
          .containerView;
  if (presentedContainerView.superview == self.view)
    [self.view bringSubviewToFront:presentedContainerView];
}

#pragma mark - Private Methods: UI Configuration, update and Layout

// Update the state of back and forward buttons, hiding the forward button if
// there is nowhere to go. Assumes the model's current tab is up to date.
- (void)updateToolbar {
  // If the BVC has been partially torn down for low memory, wait for the
  // view rebuild to handle toolbar updates.
  if (!(self.helper && self.browserState))
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

  [self.dispatcher showFindUIIfActive];
  [self.textZoomHandler showTextZoomUIIfActive];

  BOOL hideToolbar = NO;
  if (webState) {
    // There are times when the NTP can be hidden but before the visibleURL
    // changes.  This can leave the BVC in a blank state where only the bottom
    // toolbar is visible. Instead, if possible, use the NewTabPageTabHelper
    // IsActive() value rather than checking -IsVisibleURLNewTabPage.
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(webState);
    BOOL isNTP = NTPHelper && NTPHelper->IsActive();
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

- (void)dismissPopups {
  // The dispatcher may not be fully connected during shutdown, so selectors may
  // be unrecognized.
  if (_isShutdown)
    return;
  if ([self.dispatcher respondsToSelector:@selector(hidePageInfo)])
    [self.dispatcher hidePageInfo];
  [self.dispatcher dismissPopupMenuAnimated:NO];
  [self.helpHandler hideAllHelpBubbles];
}

- (UIView*)footerView {
  return self.secondaryToolbarCoordinator.viewController.view;
}

- (CGRect)ntpFrameForWebState:(web::WebState*)webState {
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  DCHECK(NTPHelper && NTPHelper->IsActive());
  // NTP is laid out only in the visible part of the screen.
  UIEdgeInsets viewportInsets = UIEdgeInsetsZero;
  if (!IsRegularXRegularSizeClass())
    viewportInsets.bottom = [self bottomToolbarHeight];
  if (!IsSplitToolbarMode(self))
    viewportInsets.top = [self expandedTopToolbarHeight];
  return UIEdgeInsetsInsetRect(self.contentArea.bounds, viewportInsets);
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

- (UIView*)viewForWebState:(web::WebState*)webState {
  if (!webState)
    return nil;
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive()) {
    return _ntpCoordinatorsForWebStates[webState].viewController.view;
  }
  DCHECK(self.browser->GetWebStateList()->GetIndexOfWebState(webState) !=
         WebStateList::kInvalidIndex);
  TabUsageRecorderBrowserAgent* tabUsageRecoder =
      TabUsageRecorderBrowserAgent::FromBrowser(_browser);
  // TODO(crbug.com/904588): Move |RecordPageLoadStart| to TabUsageRecorder.
  if (webState->IsEvicted() && tabUsageRecoder) {
    tabUsageRecoder->RecordPageLoadStart(webState);
  }
  if (!webState->IsCrashed()) {
    // Load the page if it was evicted by browsing data clearing logic.
    webState->GetNavigationManager()->LoadIfNecessary();
  }
  return webState->GetView();
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
    _lastTapPoint = [self.view.window convertPoint:originPoint
                                            toView:self.view];
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
  _lastTapPoint = [[view superview] convertPoint:viewCoordinate
                                          toView:self.view];
  _lastTapTime = CACurrentMediaTime();
}

#pragma mark - Private Methods: Tab creation and selection

- (void)installDelegatesForWebState:(web::WebState*)webState {
  // Unregistration happens when the WebState is removed from the WebStateList.
  DCHECK_NE(webState->GetDelegate(), _webStateDelegate.get());

  // There should be no pre-rendered Tabs in TabModel.
  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(self.browserState);
  DCHECK(!prerenderService ||
         !prerenderService->IsWebStatePrerendered(webState));

  SnapshotTabHelper::FromWebState(webState)->SetDelegate(self);

  // TODO(crbug.com/1069763): do not pass the browser to PasswordTabHelper.
  if (PasswordTabHelper* passwordTabHelper =
          PasswordTabHelper::FromWebState(webState)) {
    passwordTabHelper->SetBaseViewController(self);
    passwordTabHelper->SetPasswordControllerDelegate(self);
    passwordTabHelper->SetBrowser(self.browser);
  }

  if (!IsIPadIdiom()) {
    OverscrollActionsTabHelper::FromWebState(webState)->SetDelegate(self);
  }

  web_deprecated::SetSwipeRecognizerProvider(webState,
                                             self.sideSwipeController);
  webState->SetDelegate(_webStateDelegate.get());
  SadTabTabHelper::FromWebState(webState)->SetDelegate(_sadTabCoordinator);
  NetExportTabHelper::CreateForWebState(webState, self);
  CaptivePortalDetectorTabHelper::CreateForWebState(webState, self);

  OfflinePageTabHelper::CreateForWebState(
      webState, ReadingListModelFactory::GetForBrowserState(self.browserState));

  // TODO(crbug.com/1024288): Remove these lines along the legacy code removal.
  if (!IsDownloadInfobarMessagesUIEnabled()) {
    // DownloadManagerTabHelper cannot function without delegate.
    DCHECK(_downloadManagerCoordinator);
    DownloadManagerTabHelper::CreateForWebState(webState,
                                                _downloadManagerCoordinator);
  }

  NewTabPageTabHelper::CreateForWebState(webState, self);

  // The language detection helper accepts a callback from the translate
  // client, so must be created after it.
  // This will explode if the webState doesn't have a JS injection manager
  // (this only comes up in unit tests), so check for that and bypass the
  // init of the translation helpers if needed.
  // TODO(crbug.com/785238): Remove the need for this check.
  if (webState->GetJSInjectionReceiver()) {
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        webState,
        UrlLanguageHistogramFactory::GetForBrowserState(self.browserState));
    ChromeIOSTranslateClient::CreateForWebState(webState);
  }

  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              self.browserState)) {
    accountConsistencyService->SetWebStateHandler(webState, self);
  }
}

- (void)uninstallDelegatesForWebState:(web::WebState*)webState {
  DCHECK_EQ(webState->GetDelegate(), _webStateDelegate.get());

  // TODO(crbug.com/1069763): do not pass the browser to PasswordTabHelper.
  if (PasswordTabHelper* passwordTabHelper =
          PasswordTabHelper::FromWebState(webState)) {
    passwordTabHelper->SetBrowser(self.browser);
  }

  if (!IsIPadIdiom()) {
    OverscrollActionsTabHelper::FromWebState(webState)->SetDelegate(nil);
  }

  web_deprecated::SetSwipeRecognizerProvider(webState, nil);
  webState->SetDelegate(nullptr);
  if (AccountConsistencyService* accountConsistencyService =
          ios::AccountConsistencyServiceFactory::GetForBrowserState(
              self.browserState)) {
    accountConsistencyService->RemoveWebStateHandler(webState);
  }

  SnapshotTabHelper::FromWebState(webState)->SetDelegate(nil);
}

- (void)webStateSelected:(web::WebState*)webState
           notifyToolbar:(BOOL)notifyToolbar {
  DCHECK(webState);

  // Ignore changes while the tab stack view is visible (or while suspended).
  // The display will be refreshed when this view becomes active again.
  if (!self.visible || !self.webUsageEnabled)
    return;

  [self displayWebState:webState];

  if (_expectingForegroundTab && !self.inNewTabAnimation) {
    // Now that the new tab has been displayed, return to normal. Rather than
    // keep a reference to the previous tab, just turn off preview mode for all
    // tabs (since doing so is a no-op for the tabs that don't have it set).
    _expectingForegroundTab = NO;

    WebStateList* webStateList = self.browser->GetWebStateList();
    for (int index = 0; index < webStateList->count(); ++index) {
      web::WebState* webState = webStateList->GetWebStateAt(index);
      PagePlaceholderTabHelper::FromWebState(webState)
          ->CancelPlaceholderForNextNavigation();
    }
  }
}

#pragma mark - Private Methods: Voice Search

- (void)ensureVoiceSearchControllerCreated {
  if (!_voiceSearchController) {
    VoiceSearchProvider* provider =
        ios::GetChromeBrowserProvider()->GetVoiceSearchProvider();
    if (provider) {
      _voiceSearchController =
          provider->CreateVoiceSearchController(self.browser);
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
      ReadingListModelFactory::GetForBrowserState(self.browserState);
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

  // If NTP exists, use NTP coordinator's scroll offset.
  if (self.isNTPActiveForCurrentWebState) {
    NewTabPageCoordinator* coordinator =
        _ntpCoordinatorsForWebStates[self.currentWebState];
    CGFloat scrolledToTopOffset = [coordinator contentInset].top;
    return [coordinator contentOffset].y == scrolledToTopOffset;
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

  UIEdgeInsets maxViewportInsets =
      self.fullscreenController->GetMaxViewportInsets();

  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive()) {
    // If the NTP is active, then it's used as the base view for snapshotting.
    // When the tab strip is visible, the NTP is laid out below the toolbars, so
    // it should not be inset while snapshotting.  When the tab strip is not
    // used, the NTP is laid out fullscreen and the top portion of the view will
    // be obstructed by the toolbars when the snapshot is displayed in the tab
    // grid.  In that case, the NTP should be inset by the maximum viewport
    /// insets.
    // The NTP always sits above the bottom toolbar (when there is one) so the
    // insets should not take into account the bottom toolbar.
    maxViewportInsets.bottom = 0;
    return [self canShowTabStrip] ? UIEdgeInsetsZero : maxViewportInsets;
  } else {
    // If the NTP is inactive, the WebState's view is used as the base view for
    // snapshotting.  If fullscreen is implemented by resizing the scroll view,
    // then the WebState view is already laid out within the visible viewport
    // and doesn't need to be inset.  If fullscreen uses the content inset, then
    // the WebState view is laid out fullscreen and should be inset by the
    // viewport insets.
    return self.fullscreenController->ResizesScrollView() ? UIEdgeInsetsZero
                                                          : maxViewportInsets;
  }
}

- (NSArray<UIView*>*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
           snapshotOverlaysForWebState:(web::WebState*)webState {
  DCHECK(webState);
  WebStateList* webStateList = self.browser->GetWebStateList();
  DCHECK_NE(webStateList->GetIndexOfWebState(webState),
            WebStateList::kInvalidIndex);
  if (!self.webUsageEnabled || webState != webStateList->GetActiveWebState())
    return @[];

  NSMutableArray<UIView*>* overlays = [NSMutableArray array];
  UIView* infoBarView = [self infoBarOverlayViewForWebState:webState];
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

  UIViewController* overlayContainerViewController =
      self.browserContainerViewController
          .webContentsOverlayContainerViewController;
  UIView* presentedOverlayView =
      overlayContainerViewController.presentedViewController.view;
  if (presentedOverlayView) {
    [overlays addObject:presentedOverlayView];
  }

  UIView* childOverlayView =
      overlayContainerViewController.childViewControllers.firstObject.view;
  if (childOverlayView) {
    DCHECK_EQ(1U, overlayContainerViewController.childViewControllers.count);
    [overlays addObject:childOverlayView];
  }

  return overlays;
}

- (void)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
    willUpdateSnapshotForWebState:(web::WebState*)webState {
  DCHECK(webState);
  if (self.isNTPActiveForCurrentWebState) {
    [_ntpCoordinatorsForWebStates[self.currentWebState] willUpdateSnapshot];
  }
  OverscrollActionsTabHelper::FromWebState(webState)->Clear();
}

- (UIView*)snapshotGenerator:(SnapshotGenerator*)snapshotGenerator
         baseViewForWebState:(web::WebState*)webState {
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (NTPHelper && NTPHelper->IsActive())
    return _ntpCoordinatorsForWebStates[webState].viewController.view;
  return webState->GetView();
}

#pragma mark - SnapshotGeneratorDelegate helpers

// Provides a view that encompasses currently displayed infobar(s) or nil
// if no infobar is presented.
- (UIView*)infoBarOverlayViewForWebState:(web::WebState*)webState {
  if (!webState || self.currentWebState != webState)
    return nil;

  if (base::FeatureList::IsEnabled(kInfobarOverlayUI))
    return self.infobarBannerOverlayContainerViewController.view;

  DCHECK(self.infobarContainerCoordinator);
  if ([self.infobarContainerCoordinator
          isInfobarPresentingForWebState:self.currentWebState]) {
    return [self.infobarContainerCoordinator legacyContainerView];
  }

  return nil;
}

#pragma mark - PasswordControllerDelegate methods

- (BOOL)displaySignInNotification:(UIViewController*)viewController
                        fromTabId:(NSString*)tabId {
  // Check if the call comes from currently visible tab.
  NSString* visibleTabId =
      TabIdTabHelper::FromWebState(self.currentWebState)->tab_id();
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
  // Under some circumstances, this callback may be triggered from WebKit
  // synchronously as part of handling some other WebStateList mutation
  // (typically deleting a WebState and then activating another as a side
  // effect). See crbug.com/988504 for details. In this case, the request to
  // create a new WebState is silently dropped.
  if (self.browser->GetWebStateList() &&
      self.browser->GetWebStateList()->IsMutating())
    return nil;

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
  SnapshotTabHelper::FromWebState(webState)->UpdateSnapshotWithCallback(nil);

  TabInsertionBrowserAgent* insertionAgent =
      TabInsertionBrowserAgent::FromBrowser(self.browser);
  return insertionAgent->InsertWebStateOpenedByDOM(webState);
}

- (void)closeWebState:(web::WebState*)webState {
  // Only allow a web page to close itself if it was opened by DOM, if there
  // are no navigation items, or if an interstitial is showing.
  security_interstitials::IOSBlockingPageTabHelper* helper =
      security_interstitials::IOSBlockingPageTabHelper::FromWebState(webState);
  DCHECK(webState->HasOpener() ||
         !webState->GetNavigationManager()->GetItemCount() ||
         helper->GetCurrentBlockingPage() != nullptr);
  if (!self.browser)
    return;
  WebStateList* webStateList = self.browser->GetWebStateList();
  int index = webStateList->GetIndexOfWebState(webState);
  if (index != WebStateList::kInvalidIndex)
    webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

- (web::WebState*)webState:(web::WebState*)webState
         openURLWithParams:(const web::WebState::OpenURLParams&)params {
  web::NavigationManager::WebLoadParams loadParams(params.url);
  loadParams.referrer = params.referrer;
  loadParams.transition_type = params.transition;
  loadParams.is_renderer_initiated = params.is_renderer_initiated;
  loadParams.virtual_url = params.virtual_url;
  TabInsertionBrowserAgent* insertionAgent =
      TabInsertionBrowserAgent::FromBrowser(self.browser);
  switch (params.disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB: {
      return insertionAgent->InsertWebState(
          loadParams, webState, false, TabInsertion::kPositionAutomatically,
          (params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB));
    }
    case WindowOpenDisposition::CURRENT_TAB: {
      webState->GetNavigationManager()->LoadURLWithParams(loadParams);
      return webState;
    }
    case WindowOpenDisposition::NEW_POPUP: {
      return insertionAgent->InsertWebState(
          loadParams, webState, true, TabInsertion::kPositionAutomatically,
          false);
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

  DCHECK(self.browserState);

  _contextMenuCoordinator = [[ContextMenuCoordinator alloc]
      initWithBaseViewController:self
                         browser:self.browser
                           title:params.menu_title
                          inView:params.view
                      atLocation:params.location];

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
    base::RecordAction(
        base::UserMetricsAction("MobileWebContextMenuLinkImpression"));
    if (link.SchemeIs(url::kJavaScriptScheme)) {
      // Open
      title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_OPEN);
      action = ^{
        base::RecordAction(
            base::UserMetricsAction("MobileWebContextMenuOpenJS"));
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
        base::RecordAction(
            base::UserMetricsAction("MobileWebContextMenuOpenInNewTab"));
        Record(ACTION_OPEN_IN_NEW_TAB, isImage, isLink);
        // The "New Tab" item in the context menu opens a new tab in the current
        // browser state. |isOffTheRecord| indicates whether or not the current
        // browser state is incognito.
        BrowserViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;

        UrlLoadParams params = UrlLoadParams::InNewTab(link);
        params.SetInBackground(YES);
        params.web_params.referrer = referrer;
        params.in_incognito = strongSelf.isOffTheRecord;
        params.append_to = kCurrentTab;
        params.origin_point = originPoint;
        UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
      };
      [_contextMenuCoordinator addItemWithTitle:title action:action];

      if (IsMultiwindowSupported()) {
        // Open in New Window.
        title = l10n_util::GetNSStringWithFixup(
            IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW);
        action = ^{
          // TODO(crbug.com/1073410): Record this in the
          //   MobileWebContextMenuOpenInNewTab histogram.
          // The "Open In New Window" item in the context menu opens a new tab
          // in a new window. This will be (according to |isOffTheRecord|)
          // incognito if the originating browser is incognito.
          BrowserViewController* strongSelf = weakSelf;
          if (!strongSelf)
            return;

          NSUserActivity* loadURLActivity =
              ActivityToLoadURL(link, referrer, strongSelf.isOffTheRecord);
          [strongSelf.dispatcher openNewWindowWithActivity:loadURLActivity];
        };
        [_contextMenuCoordinator addItemWithTitle:title action:action];
      }
      if (!_isOffTheRecord) {
        // Open in Incognito Tab.
        title = l10n_util::GetNSStringWithFixup(
            IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
        action = ^{
          base::RecordAction(base::UserMetricsAction(
              "MobileWebContextMenuOpenInIncognitoTab"));
          BrowserViewController* strongSelf = weakSelf;
          if (!strongSelf)
            return;

          Record(ACTION_OPEN_IN_INCOGNITO_TAB, isImage, isLink);

          UrlLoadParams params = UrlLoadParams::InNewTab(link);
          params.web_params.referrer = referrer;
          params.in_incognito = YES;
          params.append_to = kCurrentTab;
          UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
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
          base::RecordAction(
              base::UserMetricsAction("MobileWebContextMenuReadLater"));
          Record(ACTION_READ_LATER, isImage, isLink);
          [weakSelf addToReadingListURL:link title:innerText];
        };
        [_contextMenuCoordinator addItemWithTitle:title action:action];
      }
    }
    // Copy Link.
    title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_COPY);
    action = ^{
      base::RecordAction(
          base::UserMetricsAction("MobileWebContextMenuCopyLink"));
      Record(ACTION_COPY_LINK_ADDRESS, isImage, isLink);
      StoreURLInPasteboard(link);
    };
    [_contextMenuCoordinator addItemWithTitle:title action:action];
  }
  if (isImage) {
    base::RecordAction(
        base::UserMetricsAction("MobileWebContextMenuImageImpression"));
    web::Referrer referrer(lastCommittedURL, params.referrer_policy);
    // Save Image.
    title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_SAVEIMAGE);
    action = ^{
      base::RecordAction(
          base::UserMetricsAction("MobileWebContextMenuSaveImage"));
      Record(ACTION_SAVE_IMAGE, isImage, isLink);
      [weakSelf.imageSaver saveImageAtURL:imageUrl
                                 referrer:referrer
                                 webState:weakSelf.currentWebState];
    };
    [_contextMenuCoordinator addItemWithTitle:title action:action];
    // Copy Image.
    title = l10n_util::GetNSStringWithFixup(IDS_IOS_CONTENT_CONTEXT_COPYIMAGE);
    action = ^{
      base::RecordAction(
          base::UserMetricsAction("MobileWebContextMenuCopyImage"));
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
      base::RecordAction(
          base::UserMetricsAction("MobileWebContextMenuOpenImage"));
      BrowserViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;

      Record(ACTION_OPEN_IMAGE, isImage, isLink);
      UrlLoadingBrowserAgent::FromBrowser(self.browser)
          ->Load(UrlLoadParams::InCurrentTab(imageUrl));
    };
    [_contextMenuCoordinator addItemWithTitle:title action:action];
    // Open Image In New Tab.
    title = l10n_util::GetNSStringWithFixup(
        IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
    action = ^{
      base::RecordAction(
          base::UserMetricsAction("MobileWebContextMenuOpenImageInNewTab"));
      Record(ACTION_OPEN_IMAGE_IN_NEW_TAB, isImage, isLink);
      BrowserViewController* strongSelf = weakSelf;
      if (!strongSelf)
        return;

      UrlLoadParams params = UrlLoadParams::InNewTab(imageUrl);
      params.SetInBackground(YES);
      params.web_params.referrer = referrer;
      params.in_incognito = strongSelf.isOffTheRecord;
      params.append_to = kCurrentTab;
      params.origin_point = originPoint;
      UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
    };
    [_contextMenuCoordinator addItemWithTitle:title action:action];

    TemplateURLService* service =
        ios::TemplateURLServiceFactory::GetForBrowserState(self.browserState);
    if (search_engines::SupportsSearchByImage(service)) {
      const TemplateURL* defaultURL = service->GetDefaultSearchProvider();
      title = l10n_util::GetNSStringF(IDS_IOS_CONTEXT_MENU_SEARCHWEBFORIMAGE,
                                      defaultURL->short_name());
      action = ^{
        base::RecordAction(
            base::UserMetricsAction("MobileWebContextMenuSearchByImage"));
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
  ProceduralBlock presentDialog = ^{
    helper->PresentDialog(dialogLocation,
                          base::BindOnce(^(bool shouldContinue) {
                            handler(shouldContinue);
                          }));
  };

  // TODO(crbug.com/965688): An Infobar message is currently the only presented
  // controller that allows interaction with the rest of the App while its being
  // presented. Dismiss it in case the user or system has triggered repost form.
  if (IsInfobarUIRebootEnabled() &&
      !base::FeatureList::IsEnabled(kInfobarOverlayUI) &&
      (self.infobarContainerCoordinator.infobarBannerState !=
       InfobarBannerPresentationState::NotPresented)) {
    [self.infobarContainerCoordinator
        dismissInfobarBannerAnimated:NO
                          completion:presentDialog];
  } else {
    presentDialog();
  }
}

- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState {
  return WebStateDelegateTabHelper::FromWebState(webState)
      ->GetJavaScriptDialogPresenter(webState);
}

- (void)webState:(web::WebState*)webState
    didRequestHTTPAuthForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                      proposedCredential:(NSURLCredential*)proposedCredential
                       completionHandler:(void (^)(NSString* username,
                                                   NSString* password))handler {
  DCHECK(handler);
  web::WebStateDelegate::AuthCallback callback =
      base::BindRepeating(^(NSString* user, NSString* password) {
        handler(user, password);
      });
  WebStateDelegateTabHelper::FromWebState(webState)->OnAuthRequired(
      webState, protectionSpace, proposedCredential, callback);
}

- (BOOL)webState:(web::WebState*)webState
    shouldPreviewLinkWithURL:(const GURL&)linkURL {
  // Link previews are not currently supported.  The kPreviewAttempted histogram
  // is used to gauge user interest in implementing this feature.
  Record(WKWebViewLinkPreviewAction::kPreviewAttempted);
  return NO;
}

- (UIView*)webViewContainerForWebState:(web::WebState*)webState {
  return self.contentArea;
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
  web::NavigationManager::WebLoadParams loadParams =
      ImageSearchParamGenerator::LoadParamsForImageData(
          data, imageURL,
          ios::TemplateURLServiceFactory::GetForBrowserState(
              self.browserState));
  [self searchByImageWithWebLoadParams:loadParams inNewTab:YES];
}

// Performs a search with the given image data. The data should alread have
// been scaled down in |ResizedImageForSearchByImage|.
- (void)searchByImageWithWebLoadParams:
            (web::NavigationManager::WebLoadParams)webParams
                              inNewTab:(BOOL)inNewTab {
  if (inNewTab) {
    UrlLoadParams params = UrlLoadParams::InNewTab(webParams);
    params.in_incognito = self.isOffTheRecord;
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  } else {
    UrlLoadingBrowserAgent::FromBrowser(self.browser)
        ->Load(UrlLoadParams::InCurrentTab(webParams));
  }
}

#pragma mark - URLLoadingObserver

// TODO(crbug.com/907527): consider moving these separate functional blurbs
// closer to their main component (using localized observers)

- (void)tabWillLoadURL:(GURL)URL
        transitionType:(ui::PageTransition)transitionType {
  [_bookmarkInteractionController dismissBookmarkModalControllerAnimated:YES];

  WebStateList* webStateList = self.browser->GetWebStateList();
  web::WebState* current_web_state = webStateList->GetActiveWebState();
  if (current_web_state &&
      (transitionType & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    bool isExpectingVoiceSearch =
        VoiceSearchNavigationTabHelper::FromWebState(current_web_state)
            ->IsExpectingVoiceSearch();
    new_tab_page_uma::RecordActionFromOmnibox(
        self.browserState, current_web_state, URL, transitionType,
        isExpectingVoiceSearch);
  }
}

- (void)tabDidLoadURL:(GURL)URL
       transitionType:(ui::PageTransition)transitionType {
  // Deactivate the NTP immediately on a load to hide the NTP quickly, but
  // after calling UrlLoadingService::Load.  Otherwise, if the
  // webState has never been visible (such as during startup with an NTP), it's
  // possible the webView can trigger a unnecessary load for chrome://newtab.
  if (self.currentWebState->GetVisibleURL() != kChromeUINewTabURL) {
    if (self.isNTPActiveForCurrentWebState) {
      NewTabPageTabHelper::FromWebState(self.currentWebState)->Deactivate();
    }
  }
}

- (void)newTabWillLoadURL:(GURL)URL isUserInitiated:(BOOL)isUserInitiated {
  if (isUserInitiated) {
    // Send either the "New Tab Opened" or "New Incognito Tab" opened to the
    // feature_engagement::Tracker based on |inIncognito|.
    feature_engagement::NotifyNewTabEvent(self.browserState,
                                          self.isOffTheRecord);
  }
}

- (void)willSwitchToTabWithURL:(GURL)URL
              newWebStateIndex:(NSInteger)newWebStateIndex {
  if ([self canShowTabStrip])
    return;

  WebStateList* webStateList = self.browser->GetWebStateList();
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

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  if (webState == self.currentWebState)
    [self updateToolbar];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  SessionRestorationBrowserAgent::FromBrowser(self.browser)
      ->SaveSession(
          /*immediately=*/false);
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [_toolbarUIUpdater updateState];
  if ([self canShowTabStrip]) {
    UIUserInterfaceSizeClass sizeClass =
        self.view.window.traitCollection.horizontalSizeClass;
    [SizeClassRecorder pageLoadedWithHorizontalSizeClass:sizeClass];
  }

  // If there is no first responder, try to make the webview or the NTP first
  // responder to have it answer keyboard commands (e.g. space bar to scroll).
  if (!GetFirstResponder() && self.currentWebState) {
    NewTabPageTabHelper* NTPHelper =
        NewTabPageTabHelper::FromWebState(webState);
    if (NTPHelper && NTPHelper->IsActive()) {
      UIViewController* viewController =
          _ntpCoordinatorsForWebStates[webState].viewController;
      [viewController becomeFirstResponder];
    } else {
      [self.currentWebState->GetWebViewProxy() becomeFirstResponder];
    }
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

- (BOOL)shouldAllowOverscrollActionsForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return !self.toolbarAccessoryPresenter.presenting;
}

- (UIView*)headerViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return self.primaryToolbarCoordinator.viewController.view;
}

- (UIView*)toolbarSnapshotViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return [self.primaryToolbarCoordinator.viewController.view
      snapshotViewAfterScreenUpdates:NO];
}

- (CGFloat)headerInsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  // The current WebState can be nil if the Browser's WebStateList is empty
  // (e.g. after closing the last tab, etc).
  web::WebState* currentWebState = self.currentWebState;
  if (!currentWebState)
    return 0.0;

  OverscrollActionsTabHelper* activeTabHelper =
      OverscrollActionsTabHelper::FromWebState(currentWebState);
  if (controller == activeTabHelper->GetOverscrollActionsController()) {
    return self.headerHeight;
  } else
    return 0;
}

- (CGFloat)headerHeightForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return self.headerHeight;
}

- (FullscreenController*)fullscreenControllerForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return self.fullscreenController;
}

#pragma mark - ToolbarHeightProviderForFullscreen

- (CGFloat)collapsedTopToolbarHeight {
  return self.view.safeAreaInsets.top +
         ToolbarCollapsedHeight(
             self.traitCollection.preferredContentSizeCategory);
}

- (CGFloat)expandedTopToolbarHeight {
  return [self primaryToolbarHeightWithInset] +
         ([self canShowTabStrip] ? self.tabStripView.frame.size.height : 0.0) +
         self.headerOffset;
}

- (CGFloat)bottomToolbarHeight {
  return [self secondaryToolbarHeightWithInset];
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

- (void)updateForFullscreenMinViewportInsets:(UIEdgeInsets)minViewportInsets
                           maxViewportInsets:(UIEdgeInsets)maxViewportInsets {
  [self updateForFullscreenProgress:self.fullscreenController->GetProgress()];
}

#pragma mark - FullscreenUIElement helpers

// Returns the height difference between the fully expanded and fully collapsed
// primary toolbar.
- (CGFloat)primaryToolbarHeightDelta {
  CGFloat fullyExpandedHeight =
      self.fullscreenController->GetMaxViewportInsets().top;
  CGFloat fullyCollapsedHeight =
      self.fullscreenController->GetMinViewportInsets().top;
  return std::max(0.0, fullyExpandedHeight - fullyCollapsedHeight);
}

// Translates the header views up and down according to |progress|, where a
// progress of 1.0 fully shows the headers and a progress of 0.0 fully hides
// them.
- (void)updateHeadersForFullscreenProgress:(CGFloat)progress {
  CGFloat offset =
      AlignValueToPixel((1.0 - progress) * [self primaryToolbarHeightDelta]);
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
      self.headerHeight + (progress - 1.0) * [self primaryToolbarHeightDelta]);
  CGFloat bottom =
      AlignValueToPixel(progress * [self secondaryToolbarHeightWithInset]);

  [self updateContentPaddingForTopToolbarHeight:top bottomToolbarHeight:bottom];
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
  UIEdgeInsets viewportInsets =
      self.fullscreenController->GetCurrentViewportInsets();
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

  FindTabHelper* helper = FindTabHelper::FromWebState(self.currentWebState);
  return (helper && helper->CurrentPageSupportsFindInPage());
}

- (NSUInteger)tabsCount {
  if (_isShutdown)
    return 0;
  return self.browser->GetWebStateList()->count();
}

- (BOOL)canGoBack {
  return self.currentWebState &&
         self.currentWebState->GetNavigationManager()->CanGoBack();
}

- (BOOL)canGoForward {
  return self.currentWebState &&
         self.currentWebState->GetNavigationManager()->CanGoForward();
}

- (void)focusTabAtIndex:(NSUInteger)index {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (webStateList->ContainsIndex(index)) {
    webStateList->ActivateWebStateAt(static_cast<int>(index));
  }
}

- (void)focusNextTab {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (!webStateList)
    return;

  int activeIndex = webStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex)
    return;

  // If the active index isn't the last index, activate the next index.
  // (the last index is always |count() - 1|).
  // Otherwise activate the first index.
  if (activeIndex < (webStateList->count() - 1)) {
    webStateList->ActivateWebStateAt(activeIndex + 1);
  } else {
    webStateList->ActivateWebStateAt(0);
  }
}

- (void)focusPreviousTab {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (!webStateList)
    return;

  int activeIndex = webStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex)
    return;

  // If the active index isn't the first index, activate the prior index.
  // Otherwise index the last index (|count() - 1|).
  if (activeIndex > 0) {
    webStateList->ActivateWebStateAt(activeIndex - 1);
  } else {
    webStateList->ActivateWebStateAt(webStateList->count() - 1);
  }
}

- (void)reopenClosedTab {
  sessions::TabRestoreService* const tabRestoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(self.browserState);
  if (!tabRestoreService || tabRestoreService->entries().empty())
    return;

  const std::unique_ptr<sessions::TabRestoreService::Entry>& entry =
      tabRestoreService->entries().front();
  // Only handle the TAB type.
  // TODO(crbug.com/1056596) : Support WINDOW restoration under multi-window.
  if (entry->type != sessions::TabRestoreService::TAB)
    return;

  [self.dispatcher openURLInNewTab:[OpenNewTabCommand command]];
  RestoreTab(entry->id, WindowOpenDisposition::CURRENT_TAB, self.browser);
}

#pragma mark - MainContentUI

- (MainContentUIState*)mainContentUIState {
  return _mainContentUIUpdater.state;
}

#pragma mark - ToolbarCoordinatorDelegate (Public)

- (void)locationBarDidBecomeFirstResponder {
  if (self.isNTPActiveForCurrentWebState) {
    NewTabPageCoordinator* coordinator =
        _ntpCoordinatorsForWebStates[self.currentWebState];
    [coordinator locationBarDidBecomeFirstResponder];
  }
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
      locationBarDidBecomeFirstResponder:self.browserState];

  [self.primaryToolbarCoordinator transitionToLocationBarFocusedState:YES];

  self.keyCommandsProvider.canDismissModals = YES;
}

- (void)locationBarDidResignFirstResponder {
  self.keyCommandsProvider.canDismissModals = NO;
  [self.sideSwipeController setEnabled:YES];

  if (self.isNTPActiveForCurrentWebState) {
    NewTabPageCoordinator* coordinator =
        _ntpCoordinatorsForWebStates[self.currentWebState];
    [coordinator locationBarDidResignFirstResponder];
  }
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
      locationBarDidResignFirstResponder:self.browserState];

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
  if (self.currentWebState)
    web_navigation_util::GoBack(self.currentWebState);
}

- (void)goForward {
  if (self.currentWebState)
    web_navigation_util::GoForward(self.currentWebState);
}

- (void)stopLoading {
  self.currentWebState->Stop();
}

- (void)reload {
  if ([self isNTPActiveForCurrentWebState]) {
    NewTabPageCoordinator* coordinator =
        _ntpCoordinatorsForWebStates[self.currentWebState];
    [coordinator reload];
  } else if (self.currentWebState) {
    // |check_for_repost| is true because the reload is explicitly initiated
    // by the user.
    self.currentWebState->GetNavigationManager()->Reload(
        web::ReloadType::NORMAL, true /* check_for_repost */);
  }
}

- (void)bookmarkPage {
  [self initializeBookmarkInteractionController];
  [_bookmarkInteractionController
      presentBookmarkEditorForWebState:self.currentWebState
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
  DCHECK(self.currentWebState);
  NSString* script = @"document.documentElement.outerHTML;";
  __weak BrowserViewController* weakSelf = self;
  auto completionHandlerBlock = ^(id result, NSError*) {
    web::WebState* webState = weakSelf.currentWebState;
    if (!webState)
      return;
    if (![result isKindOfClass:[NSString class]])
      result = @"Not an HTML page";
    std::string base64HTML;
    base::Base64Encode(base::SysNSStringToUTF8(result), &base64HTML);
    GURL URL(std::string("data:text/plain;charset=utf-8;base64,") + base64HTML);
    web::Referrer referrer(webState->GetLastCommittedURL(),
                           web::ReferrerPolicyDefault);
    web::NavigationManager::WebLoadParams loadParams(URL);
    loadParams.referrer = referrer;
    loadParams.transition_type = ui::PAGE_TRANSITION_LINK;
    TabInsertionBrowserAgent* insertionAgent =
        TabInsertionBrowserAgent::FromBrowser(self.browser);
    insertionAgent->InsertWebState(loadParams, webState, true,
                                   TabInsertion::kPositionAutomatically, false);
  };
  [self.currentWebState->GetJSInjectionReceiver()
      executeJavaScript:script
      completionHandler:completionHandlerBlock];
}
#endif  // !defined(NDEBUG)

- (void)showTranslate {
  feature_engagement::Tracker* engagement_tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(self.browserState);
  engagement_tracker->NotifyEvent(
      feature_engagement::events::kTriggeredTranslateInfobar);

  DCHECK(self.currentWebState);
  ChromeIOSTranslateClient* translateClient =
      ChromeIOSTranslateClient::FromWebState(self.currentWebState);
  if (translateClient) {
    translate::TranslateManager* translateManager =
        translateClient->GetTranslateManager();
    DCHECK(translateManager);
    translateManager->InitiateManualTranslation(/*auto_translate=*/true);
  }
}

- (void)showHelpPage {
  GURL helpUrl(l10n_util::GetStringUTF16(IDS_IOS_TOOLS_MENU_HELP_URL));
  UrlLoadParams params = UrlLoadParams::InNewTab(helpUrl);
  params.append_to = kCurrentTab;
  params.user_initiated = NO;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

- (void)showBookmarksManager {
  [self initializeBookmarkInteractionController];
  [_bookmarkInteractionController presentBookmarks];
}

- (void)showSendTabToSelfUI {
  // TODO(crbug.com/972114) Move or reroute to browserCoordinator.
  self.sendTabToSelfCoordinator = [[SendTabToSelfCoordinator alloc]
      initWithBaseViewController:self
                         browser:self.browser];
  [self.sendTabToSelfCoordinator start];
}

- (void)requestDesktopSite {
  [self reloadWithUserAgentType:web::UserAgentType::DESKTOP];
}

- (void)requestMobileSite {
  [self reloadWithUserAgentType:web::UserAgentType::MOBILE];
}

- (void)closeCurrentTab {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (!webStateList)
    return;

  int active_index = webStateList->active_index();
  if (active_index == WebStateList::kInvalidIndex)
    return;

  UIView* snapshotView = [self.contentArea snapshotViewAfterScreenUpdates:NO];
  snapshotView.frame = self.contentArea.frame;

  webStateList->CloseWebStateAt(active_index, WebStateList::CLOSE_USER_ACTION);

  if (![self canShowTabStrip]) {
    [self.contentArea addSubview:snapshotView];
    page_animation_util::AnimateOutWithCompletion(snapshotView, ^{
      [snapshotView removeFromSuperview];
    });
  }
}

- (void)prepareForPopupMenuPresentation:(PopupMenuCommandType)type {
  DCHECK(self.browserState);
  DCHECK(self.visible || self.dismissingModal);

  // Dismiss the omnibox (if open).
  [self.omniboxHandler cancelOmniboxEdit];
  // Dismiss the soft keyboard (if open).
  [[self viewForWebState:self.currentWebState] endEditing:NO];
  // Dismiss Find in Page focus.
  [self.dispatcher defocusFindInPage];

  if (type == PopupMenuCommandTypeToolsMenu) {
    [self.bubblePresenter toolsMenuDisplayed];
  }
}

- (void)focusFakebox {
  if (self.isNTPActiveForCurrentWebState) {
    [_ntpCoordinatorsForWebStates[self.currentWebState] focusFakebox];
  }
}

- (void)searchByImage:(UIImage*)image {
  [self searchByImageWithWebLoadParams:
            ImageSearchParamGenerator::LoadParamsForImage(
                image, ios::TemplateURLServiceFactory::GetForBrowserState(
                           self.browserState))
                              inNewTab:NO];
}

- (void)showActivityOverlay:(BOOL)show {
  if (!show) {
    [self.activityOverlayCoordinator stop];
    self.activityOverlayCoordinator = nil;
  } else if (!self.activityOverlayCoordinator) {
    self.activityOverlayCoordinator = [[ActivityOverlayCoordinator alloc]
        initWithBaseViewController:self
                           browser:self.browser];
    [self.activityOverlayCoordinator start];
  }
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

#pragma mark - WebStateListObserving methods

// Observer method, active WebState changed.
- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  if (oldWebState) {
    oldWebState->WasHidden();
    oldWebState->SetKeepRenderProcessAlive(false);
    [self dismissPopups];
  }
  // NOTE: webStateSelected expects to always be called with a
  // non-null WebState.
  if (!newWebState)
    return;

  self.currentWebState->GetWebViewProxy().scrollViewProxy.clipsToBounds = NO;

  [self webStateSelected:newWebState notifyToolbar:YES];
}

// A WebState has been removed, remove its views from display if necessary.
- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  webState->WasHidden();
  webState->SetKeepRenderProcessAlive(false);

  [self uninstallDelegatesForWebState:webState];

  // Ignore changes while the tab grid is visible (or while suspended).
  // The display will be refreshed when this view becomes active again.
  if (!self.visible || !self.webUsageEnabled)
    return;
}

- (void)webStateList:(WebStateList*)webStateList
    willDetachWebState:(web::WebState*)webState
               atIndex:(int)atIndex {
  if (webState == self.currentWebState) {
    self.browserContainerViewController.contentView = nil;
  }

  [[UpgradeCenter sharedInstance]
      tabWillClose:TabIdTabHelper::FromWebState(webState)->tab_id()];
}

// Observer method, WebState replaced in |webStateList|.
- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  [self uninstallDelegatesForWebState:oldWebState];
  [self installDelegatesForWebState:newWebState];

  // Add |newTab|'s view to the hierarchy if it's the current Tab.
  if (self.active && self.currentWebState == newWebState)
    [self displayWebState:newWebState];
}

// Observer method, |webState| inserted in |webStateList|.
- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  DCHECK(webState);
  [self installDelegatesForWebState:webState];

  DCHECK_EQ(self.browser->GetWebStateList(), webStateList);

  // Don't initiate Tab animation while session restoration is in progress
  // (see crbug.com/763964).
  if (SessionRestorationBrowserAgent::FromBrowser(self.browser)
          ->IsRestoringSession()) {
    return;
  }
  // When adding new tabs, check what kind of reminder infobar should
  // be added to the new tab. Try to add only one of them.
  // This check is done when a new tab is added either through the Tools Menu
  // "New Tab", through a long press on the Tab Switcher button "New Tab", and
  // through creating a New Tab from the Tab Switcher. This logic needs to
  // happen after a new WebState has added and finished initial navigation. If
  // this happens earlier, the initial navigation may end up clearing the
  // infobar(s) that are just added.
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  NSString* tabID = TabIdTabHelper::FromWebState(webState)->tab_id();
  [[UpgradeCenter sharedInstance] addInfoBarToManager:infoBarManager
                                             forTabId:tabID];
  if (!ReSignInInfoBarDelegate::Create(self.browserState, webState,
                                       self /* id<SigninPresenter> */)) {
    DisplaySyncErrors(self.browserState, webState,
                      self /* id<SyncPresenter> */);
  }

  [self initiateNewTabAnimationForWebState:webState
                      willOpenInBackground:!activating];
}

#pragma mark - WebStateListObserver helpers (new tab animations)

- (void)initiateNewTabAnimationForWebState:(web::WebState*)webState
                      willOpenInBackground:(BOOL)background {
  DCHECK(webState);

  // The rest of this function initiates the new tab animation, which is
  // phone-specific.  Call the foreground tab added completion block; for
  // iPhones, this will get executed after the animation has finished.
  if ([self canShowTabStrip]) {
    if (self.foregroundTabWasAddedCompletionBlock) {
      // This callback is called before webState is activated. Dispatch the
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
    [self animateNewTabForWebState:webState
        inForegroundWithCompletion:startVoiceSearchIfNecessary];
  }
}

- (void)animateNewTabForWebState:(web::WebState*)webState
      inForegroundWithCompletion:(ProceduralBlock)completion {
  // Create the new page image, and load with the new tab snapshot except if
  // it is the NTP.
  UIView* newPage = nil;
  GURL tabURL = webState->GetVisibleURL();
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
    toolbarSnapshot.frame = [self.contentArea convertRect:toolbarSnapshot.frame
                                                 fromView:self.view];
    [self.contentArea addSubview:toolbarSnapshot];
    newPage = [self viewForWebState:webState];
    newPage.userInteractionEnabled = NO;
    newPage.frame = self.view.bounds;
  } else {
    [self viewForWebState:webState].frame = self.contentArea.bounds;
    // Setting the frame here doesn't trigger a layout pass. Trigger it manually
    // if needed. Not triggering it can create problem if the previous frame
    // wasn't the right one, for example in https://crbug.com/852106.
    [[self viewForWebState:webState] layoutIfNeeded];
    newPage = [self viewForWebState:webState];
    newPage.userInteractionEnabled = NO;
  }

  NSInteger currentAnimationIdentifier = ++_NTPAnimationIdentifier;

  // Cleanup steps needed for both UI Refresh and stack-view style animations.
  UIView* webStateView = [self viewForWebState:webState];
  auto commonCompletion = ^{
    webStateView.frame = self.contentArea.bounds;
    newPage.userInteractionEnabled = YES;
    if (currentAnimationIdentifier != _NTPAnimationIdentifier) {
      // Prevent the completion block from being executed if a new animation has
      // started in between. |self.foregroundTabWasAddedCompletionBlock| isn't
      // called because it is overridden when a new animation is started.
      // Calling it here would call the block from the lastest animation that
      // haved started.
      return;
    }

    self.inNewTabAnimation = NO;
    // Use the model's currentWebState here because it is possible that it can
    // be reset to a new value before the new Tab animation finished (e.g.
    // if another Tab shows a dialog via |dialogPresenter|). However, that
    // webState's view hasn't been displayed yet because it was in a new tab
    // animation.
    web::WebState* currentWebState = self.currentWebState;

    if (currentWebState) {
      [self webStateSelected:currentWebState notifyToolbar:NO];
    }
    if (completion)
      completion();

    if (self.foregroundTabWasAddedCompletionBlock) {
      self.foregroundTabWasAddedCompletionBlock();
      self.foregroundTabWasAddedCompletionBlock = nil;
    }
  };

  CGPoint origin = [self lastTapPoint];

  CGRect frame = [self.contentArea convertRect:self.view.bounds
                                      fromView:self.view];
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

- (void)sideSwipeRedisplayWebState:(web::WebState*)webState {
  [self displayWebState:webState];
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
    [self.dispatcher hideFindUI];
    [self.textZoomHandler hideTextZoomUI];
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

- (web::WebState*)webStateToReplace {
  return self.currentWebState;
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

#pragma mark - LogoAnimationControllerOwnerOwner (Public)

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  if (self.isNTPActiveForCurrentWebState) {
    NewTabPageCoordinator* coordinator =
        _ntpCoordinatorsForWebStates[self.currentWebState];
    if ([coordinator logoAnimationControllerOwner]) {
      // If NTP coordinator is showing a GLIF view (e.g. the NTP when there is
      // no doodle), use that GLIFControllerOwner.
      return [coordinator logoAnimationControllerOwner];
    }
  }
  return nil;
}

#pragma mark - CaptivePortalDetectorTabHelperDelegate

- (void)captivePortalDetectorTabHelper:
            (CaptivePortalDetectorTabHelper*)tabHelper
                 connectWithLandingURL:(const GURL&)landingURL {
  TabInsertionBrowserAgent* insertionAgent =
      TabInsertionBrowserAgent::FromBrowser(self.browser);
  insertionAgent->InsertWebState(
      web_navigation_util::CreateWebLoadParams(
          landingURL, ui::PAGE_TRANSITION_TYPED, nullptr),
      nil, false, self.browser->GetWebStateList()->count(), false);
}

#pragma mark - PageInfoPresentation

- (void)presentPageInfoView:(UIView*)pageInfoView {
  [pageInfoView setFrame:self.view.bounds];
  [self.view addSubview:pageInfoView];
}

- (void)prepareForPageInfoPresentation {
  // Dismiss the omnibox (if open).
  [self.omniboxHandler cancelOmniboxEdit];
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

#pragma mark - FindBarPresentationDelegate

- (void)setHeadersForFindBarCoordinator:
    (FindBarCoordinator*)findBarCoordinator {
  [self setFramesForHeaders:[self headerViews]
                   atOffset:[self currentHeaderOffset]];
}

#pragma mark - Toolbar Accessory Methods

- (ToolbarAccessoryPresenter*)toolbarAccessoryPresenter {
  if (_toolbarAccessoryPresenter) {
    return _toolbarAccessoryPresenter;
  }

  _toolbarAccessoryPresenter =
      [[ToolbarAccessoryPresenter alloc] initWithIsIncognito:_isOffTheRecord];
  _toolbarAccessoryPresenter.baseViewController = self;
  return _toolbarAccessoryPresenter;
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

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [self.dispatcher showSignin:command baseViewController:self];
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

- (void)showSyncPassphraseSettings {
  [self.dispatcher showSyncPassphraseSettingsFromViewController:self];
}

- (void)showGoogleServicesSettings {
  [self.dispatcher showGoogleServicesSettingsFromViewController:self];
}

- (void)showTrustedVaultReauthenticationWithRetrievalTrigger:
    (syncer::KeyRetrievalTriggerForUMA)retrievalTrigger {
  [self.dispatcher
      showTrustedVaultReauthenticationFromViewController:self
                                        retrievalTrigger:retrievalTrigger];
}

#pragma mark - NewTabPageTabHelperDelegate

- (void)newTabPageHelperDidChangeVisibility:(NewTabPageTabHelper*)NTPHelper
                                forWebState:(web::WebState*)webState {
  if (NTPHelper->IsActive()) {
    DCHECK(!_ntpCoordinatorsForWebStates[webState]);
    NewTabPageCoordinator* newTabPageCoordinator =
        [[NewTabPageCoordinator alloc] initWithBrowser:self.browser];
    newTabPageCoordinator.toolbarDelegate = self.toolbarInterface;
    newTabPageCoordinator.webState = webState;
    _ntpCoordinatorsForWebStates[webState] = newTabPageCoordinator;
  } else {
    NewTabPageCoordinator* newTabPageCoordinator =
        _ntpCoordinatorsForWebStates[webState];
    DCHECK(newTabPageCoordinator);
    [newTabPageCoordinator stop];
    _ntpCoordinatorsForWebStates.erase(webState);
  }
  if (self.active && self.currentWebState == webState) {
    [self displayWebState:webState];
  }
}

@end
