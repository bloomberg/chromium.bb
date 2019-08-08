// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <WebKit/WebKit.h>

#import <objc/runtime.h>
#include <stddef.h>

#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/mru_cache.h"
#include "base/feature_list.h"
#include "base/i18n/i18n_constants.h"
#import "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#import "base/ios/ns_error_util.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "crypto/symmetric_key.h"
#import "ios/net/http_response_headers_util.h"
#import "ios/web/browsing_data/browsing_data_remover.h"
#import "ios/web/browsing_data/browsing_data_remover_observer.h"
#import "ios/web/common/crw_content_view.h"
#import "ios/web/common/crw_web_view_content_view.h"
#include "ios/web/common/features.h"
#include "ios/web/common/referrer_util.h"
#include "ios/web/common/url_util.h"
#import "ios/web/find_in_page/find_in_page_manager_impl.h"
#include "ios/web/history_state_util.h"
#import "ios/web/interstitials/web_interstitial_impl.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#include "ios/web/navigation/error_retry_state_machine.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/navigation_manager_util.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/net/cert_host_pair.h"
#import "ios/web/net/crw_cert_verification_controller.h"
#import "ios/web/net/crw_ssl_status_updater.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/download/download_controller.h"
#include "ios/web/public/favicon_url.h"
#import "ios/web/public/java_script_dialog_presenter.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/page_display_state.h"
#import "ios/web/public/web_state/ui/crw_context_menu_delegate.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_frame.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_state/web_state_interface_provider.h"
#import "ios/web/public/web_state/web_state_policy_decider.h"
#include "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/web_state/error_translation_util.h"
#import "ios/web/web_state/js/crw_js_window_id_manager.h"
#import "ios/web/web_state/navigation_context_impl.h"
#import "ios/web/web_state/page_viewport_state.h"
#import "ios/web/web_state/session_certificate_policy_cache_impl.h"
#import "ios/web/web_state/ui/crw_context_menu_controller.h"
#import "ios/web/web_state/ui/crw_swipe_recognizer_provider.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"
#import "ios/web/web_state/ui/crw_wk_navigation_states.h"
#import "ios/web/web_state/ui/crw_wk_script_message_router.h"
#import "ios/web/web_state/ui/favicon_util.h"
#include "ios/web/web_state/ui/web_kit_constants.h"
#import "ios/web/web_state/ui/wk_back_forward_list_item_holder.h"
#import "ios/web/web_state/ui/wk_navigation_action_policy_util.h"
#import "ios/web/web_state/ui/wk_navigation_action_util.h"
#import "ios/web/web_state/ui/wk_security_origin_util.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_frame_impl.h"
#import "ios/web/web_state/web_frames_manager_impl.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#import "ios/web/webui/crw_web_ui_manager.h"
#import "ios/web/webui/mojo_facade.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_util_ios.h"
#include "net/ssl/ssl_info.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::NavigationManager;
using web::NavigationManagerImpl;
using web::WebState;
using web::WebStateImpl;

using web::wk_navigation_util::IsPlaceholderUrl;
using web::wk_navigation_util::CreatePlaceholderUrlForUrl;
using web::wk_navigation_util::ExtractUrlFromPlaceholderUrl;
using web::wk_navigation_util::IsRestoreSessionUrl;
using web::wk_navigation_util::IsWKInternalUrl;

namespace {

// Struct to capture data about a user interaction. Records the time of the
// interaction and the main document URL at that time.
struct UserInteractionEvent {
  UserInteractionEvent(GURL url)
      : main_document_url(url), time(CFAbsoluteTimeGetCurrent()) {}
  // Main document URL at the time the interaction occurred.
  GURL main_document_url;
  // Time that the interaction occurred, measured in seconds since Jan 1 2001.
  CFAbsoluteTime time;
};

// Keys for JavaScript command handlers context.
NSString* const kUserIsInteractingKey = @"userIsInteracting";
NSString* const kOriginURLKey = @"originURL";
NSString* const kIsMainFrame = @"isMainFrame";

// URL scheme for messages sent from javascript for asynchronous processing.
NSString* const kScriptMessageName = @"crwebinvoke";

// Message command sent when a frame becomes available.
NSString* const kFrameBecameAvailableMessageName = @"FrameBecameAvailable";
// Message command sent when a frame is unloading.
NSString* const kFrameBecameUnavailableMessageName = @"FrameBecameUnavailable";

NSString* const kReferrerHeaderName = @"Referer";  // [sic]

// The duration of the period following a screen touch during which the user is
// still considered to be interacting with the page.
const NSTimeInterval kMaximumDelayForUserInteractionInSeconds = 2;

// Values for the histogram that counts slow/fast back/forward navigations.
enum class BackForwardNavigationType {
  // Fast back navigation through WKWebView back-forward list.
  FAST_BACK = 0,
  // Slow back navigation when back-forward list navigation is not possible.
  SLOW_BACK = 1,
  // Fast forward navigation through WKWebView back-forward list.
  FAST_FORWARD = 2,
  // Slow forward navigation when back-forward list navigation is not possible.
  SLOW_FORWARD = 3,
  BACK_FORWARD_NAVIGATION_TYPE_COUNT
};

// Represents cert verification error, which happened inside
// |webView:didReceiveAuthenticationChallenge:completionHandler:| and should
// be checked inside |webView:didFailProvisionalNavigation:withError:|.
struct CertVerificationError {
  CertVerificationError(BOOL is_recoverable, net::CertStatus status)
      : is_recoverable(is_recoverable), status(status) {}

  BOOL is_recoverable;
  net::CertStatus status;
};

// Type of Cache object for storing cert verification errors.
typedef base::MRUCache<web::CertHostPair, CertVerificationError>
    CertVerificationErrorsCacheType;

// Maximum number of errors to store in cert verification errors cache.
// Cache holds errors only for pending navigations, so the actual number of
// stored errors is not expected to be high.
const CertVerificationErrorsCacheType::size_type kMaxCertErrorsCount = 100;

// URLs that are fed into UIWebView as history push/replace get escaped,
// potentially changing their format. Code that attempts to determine whether a
// URL hasn't changed can be confused by those differences though, so method
// will round-trip a URL through the escaping process so that it can be adjusted
// pre-storing, to allow later comparisons to work as expected.
GURL URLEscapedForHistory(const GURL& url) {
  // TODO(stuartmorgan): This is a very large hammer; see if limited unicode
  // escaping would be sufficient.
  return net::GURLWithNSURL(net::NSURLWithGURL(url));
}

}  // namespace

#pragma mark -

// A container object for any navigation information that is only available
// during pre-commit delegate callbacks, and thus must be held until the
// navigation commits and the informatino can be used.
@interface CRWWebControllerPendingNavigationInfo : NSObject {
}
// The referrer for the page.
@property(nonatomic, copy) NSString* referrer;
// The MIME type for the page.
@property(nonatomic, copy) NSString* MIMEType;
// The navigation type for the load.
@property(nonatomic, assign) WKNavigationType navigationType;
// HTTP request method for the load.
@property(nonatomic, copy) NSString* HTTPMethod;
// Whether the pending navigation has been directly cancelled before the
// navigation is committed.
// Cancelled navigations should be simply discarded without handling any
// specific error.
@property(nonatomic, assign) BOOL cancelled;
// Whether the navigation was initiated by a user gesture.
@property(nonatomic, assign) BOOL hasUserGesture;

@end

@implementation CRWWebControllerPendingNavigationInfo

- (instancetype)init {
  if ((self = [super init])) {
    _navigationType = WKNavigationTypeOther;
  }
  return self;
}

@end

@interface CRWWebController () <BrowsingDataRemoverObserver,
                                CRWContextMenuDelegate,
                                CRWNativeContentDelegate,
                                CRWSSLStatusUpdaterDataSource,
                                CRWSSLStatusUpdaterDelegate,
                                CRWWebControllerContainerViewDelegate,
                                CRWWebViewScrollViewProxyObserver,
                                WKNavigationDelegate,
                                WKUIDelegate> {
  // The view used to display content.  Must outlive |_webViewProxy|. The
  // container view should be accessed through this property rather than
  // |self.view| from within this class, as |self.view| triggers creation while
  // |self.containerView| will return nil if the view hasn't been instantiated.
  CRWWebControllerContainerView* _containerView;
  // YES if the current URL load was triggered in Web Controller. NO by default
  // and after web usage was disabled. Used by |-loadCurrentURLIfNecessary| to
  // prevent extra loads.
  BOOL _currentURLLoadWasTrigerred;
  BOOL _isHalted;  // YES if halted. Halting happens prior to destruction.
  BOOL _isBeingDestroyed;  // YES if in the process of closing.
  // YES if the user has interacted with the content area since the last URL
  // change.
  BOOL _interactionRegisteredSinceLastURLChange;
  // The actual URL of the document object (i.e., the last committed URL).
  // TODO(crbug.com/549616): Remove this in favor of just updating the
  // navigation manager and treating that as authoritative.
  GURL _documentURL;
  // Page loading phase.
  web::LoadPhase _loadPhase;
  // The web::PageDisplayState recorded when the page starts loading.
  web::PageDisplayState _displayStateOnStartLoading;
  // Whether or not the page has zoomed since the current navigation has been
  // committed, either by user interaction or via |-restoreStateFromHistory|.
  BOOL _pageHasZoomed;
  // Whether a PageDisplayState is currently being applied.
  BOOL _applyingPageState;
  // Actions to execute once the page load is complete.
  NSMutableArray* _pendingLoadCompleteActions;
  // Flag to say if browsing is enabled.
  BOOL _webUsageEnabled;
  // The controller that tracks long press and check context menu trigger.
  CRWContextMenuController* _contextMenuController;
  // Whether a click is in progress.
  BOOL _clickInProgress;
  // Data on the recorded last user interaction.
  std::unique_ptr<UserInteractionEvent> _lastUserInteraction;
  // YES if there has been user interaction with views owned by this controller.
  BOOL _userInteractedWithWebController;
  // The time of the last page transfer start, measured in seconds since Jan 1
  // 2001.
  CFAbsoluteTime _lastTransferTimeInSeconds;
  // Default URL (about:blank).
  GURL _defaultURL;
  // Whether the web page is currently performing window.history.pushState or
  // window.history.replaceState
  // Set to YES on window.history.willChangeState message. To NO on
  // window.history.didPushState or window.history.didReplaceState.
  BOOL _changingHistoryState;
  // Set to YES when a hashchange event is manually dispatched for same-document
  // history navigations.
  BOOL _dispatchingSameDocumentHashChangeEvent;

  // A set of script managers whose scripts have been injected into the current
  // page.
  // TODO(stuartmorgan): Revisit this approach; it's intended only as a stopgap
  // measure to make all the existing script managers work. Longer term, there
  // should probably be a couple of points where managers can register to have
  // things happen automatically based on page lifecycle, and if they don't want
  // to use one of those fixed points, they should make their scripts internally
  // idempotent.
  NSMutableSet* _injectedScriptManagers;

  // Script manager for setting the windowID.
  CRWJSWindowIDManager* _windowIDJSManager;

  // Backs up property with the same name.
  std::unique_ptr<web::MojoFacade> _mojoFacade;

  // Referrer for the current page; does not include the fragment.
  NSString* _currentReferrerString;

  // Pending information for an in-progress page navigation. The lifetime of
  // this object starts at |decidePolicyForNavigationAction| where the info is
  // extracted from the request, and ends at either |didCommitNavigation| or
  // |didFailProvisionalNavigation|.
  CRWWebControllerPendingNavigationInfo* _pendingNavigationInfo;

  // Holds all WKNavigation objects and their states which are currently in
  // flight.
  CRWWKNavigationStates* _navigationStates;

  // The WKNavigation captured when |stopLoading| was called. Used for reporting
  // WebController.EmptyNavigationManagerCausedByStopLoading UMA metric which
  // helps with diagnosing a navigation related crash (crbug.com/565457).
  __weak WKNavigation* _stoppedWKNavigation;

  // Used to poll for a SafeBrowsing warning being displayed. This is created in
  // |decidePolicyForNavigationAction| and destroyed once any of the following
  // happens: 1) a SafeBrowsing warning is detected; 2) any WKNavigationDelegate
  // method is called; 3) |abortLoad| is called.
  base::RepeatingTimer _safeBrowsingWarningDetectionTimer;

  // CRWWebUIManager object for loading WebUI pages.
  CRWWebUIManager* _webUIManager;

  // Updates SSLStatus for current navigation item.
  CRWSSLStatusUpdater* _SSLStatusUpdater;

  // Controller used for certs verification to help with blocking requests with
  // bad SSL cert, presenting SSL interstitials and determining SSL status for
  // Navigation Items.
  CRWCertVerificationController* _certVerificationController;

  // CertVerification errors which happened inside
  // |webView:didReceiveAuthenticationChallenge:completionHandler:|.
  // Key is leaf-cert/host pair. This storage is used to carry calculated
  // cert status from |didReceiveAuthenticationChallenge:| to
  // |didFailProvisionalNavigation:| delegate method.
  std::unique_ptr<CertVerificationErrorsCacheType> _certVerificationErrors;
}

// If |contentView_| contains a web view, this is the web view it contains.
// If not, it's nil. When setting the property, it performs basic setup.
@property(weak, nonatomic) WKWebView* webView;
// The scroll view of |webView|.
@property(weak, nonatomic, readonly) UIScrollView* webScrollView;
// The current page state of the web view. Writing to this property
// asynchronously applies the passed value to the current web view.
@property(nonatomic, readwrite) web::PageDisplayState pageDisplayState;
// The currently displayed native controller, if any.
@property(weak, nonatomic, readwrite) id<CRWNativeContent> nativeController;
// Dictionary where keys are the names of WKWebView properties and values are
// selector names which should be called when a corresponding property has
// changed. e.g. @{ @"URL" : @"webViewURLDidChange" } means that
// -[self webViewURLDidChange] must be called every time when WKWebView.URL is
// changed.
@property(weak, nonatomic, readonly) NSDictionary* WKWebViewObservers;

// The web view's view of the current URL. During page transitions
// this may not be the same as the session history's view of the current URL.
// This method can change the state of the CRWWebController, as it will display
// an error if the returned URL is not reliable from a security point of view.
// Note that this method is expensive, so it should always be cached locally if
// it's needed multiple times in a method.
@property(nonatomic, readonly) GURL currentURL;
// Returns the referrer for the current page.
@property(nonatomic, readonly) web::Referrer currentReferrer;

// Returns YES if the user interacted with the page recently.
@property(nonatomic, readonly) BOOL userClickedRecently;

// User agent type of the transient item if any, the pending item if a
// navigation is in progress or the last committed item otherwise.
// Returns MOBILE, the default type, if navigation manager is nullptr or empty.
@property(nonatomic, readonly) web::UserAgentType userAgentType;

// Facade for Mojo API.
@property(nonatomic, readonly) web::MojoFacade* mojoFacade;

// YES if a user interaction has been registered at any time since the page has
// loaded.
@property(nonatomic, readwrite) BOOL userInteractionRegistered;

@property(nonatomic, readonly) web::WebState* webState;
// WebStateImpl instance associated with this CRWWebController, web controller
// does not own this pointer.
@property(nonatomic, readonly) web::WebStateImpl* webStateImpl;

// Returns the x, y offset the content has been scrolled.
@property(nonatomic, readonly) CGPoint scrollPosition;

// The touch tracking recognizer allowing us to decide if a navigation has user
// gesture. Lazily created.
@property(nonatomic, strong, readonly)
    CRWTouchTrackingRecognizer* touchTrackingRecognizer;

// Session Information
// -------------------
// Returns NavigationManager's session controller.
@property(weak, nonatomic, readonly) CRWSessionController* sessionController;
// The associated NavigationManagerImpl.
@property(nonatomic, readonly) NavigationManagerImpl* navigationManagerImpl;
// Whether the associated WebState has an opener.
@property(nonatomic, readonly) BOOL hasOpener;
// TODO(crbug.com/692871): Remove these functions and replace with more
// appropriate NavigationItem getters.
// Returns the navigation item for the current page.
@property(nonatomic, readonly) web::NavigationItemImpl* currentNavItem;
// Returns the current transition type.
@property(nonatomic, readonly) ui::PageTransition currentTransition;
// Returns the referrer for current navigation item. May be empty.
@property(nonatomic, readonly) web::Referrer currentNavItemReferrer;
// The HTTP headers associated with the current navigation item. These are nil
// unless the request was a POST.
@property(weak, nonatomic, readonly) NSDictionary* currentHTTPHeaders;

// Extracts "Referer" [sic] value from WKNavigationAction request header.
- (NSString*)referrerFromNavigationAction:(WKNavigationAction*)action;

// Returns the current URL of the web view, and sets |trustLevel| accordingly
// based on the confidence in the verification.
- (GURL)webURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel;

// Returns a NSMutableURLRequest that represents the current NavigationItem.
- (NSMutableURLRequest*)requestForCurrentNavigationItem;

// Loads a blank page directly into WKWebView as a placeholder for a Native View
// or WebUI URL. This page has the URL about:blank?for=<encoded original URL>.
// If |originalContext| is provided, reuse it for the placeholder navigation
// instead of creating a new one. See "Handling App-specific URLs"
// section of go/bling-navigation-experiment for details.
- (web::NavigationContextImpl*)
    loadPlaceholderInWebViewForURL:(const GURL&)originalURL
                 rendererInitiated:(BOOL)rendererInitiated
                        forContext:(std::unique_ptr<web::NavigationContextImpl>)
                                       originalContext;
// Executes the command specified by the ErrorRetryStateMachine.
- (void)handleErrorRetryCommand:(web::ErrorRetryCommand)command
                 navigationItem:(web::NavigationItemImpl*)item
              navigationContext:(web::NavigationContextImpl*)context
             originalNavigation:(WKNavigation*)originalNavigation;
// Loads the error page.
- (void)loadErrorPageForNavigationItem:(web::NavigationItemImpl*)item
                     navigationContext:(web::NavigationContextImpl*)context;
// Aborts any load for both the web view and web controller.
- (void)abortLoad;
// Updates the internal state and informs the delegate that any outstanding load
// operations are cancelled.
- (void)loadCancelled;
// If YES, the page should be closed if it successfully redirects to a native
// application, for example if a new tab redirects to the App Store.
- (BOOL)shouldClosePageOnNativeApplicationLoad;
// Called following navigation completion to generate final navigation lifecycle
// events. Navigation is considered complete when the document has finished
// loading, or when other page load mechanics are completed on a
// non-document-changing URL change.
- (void)didFinishNavigation:(web::NavigationContextImpl*)context;
// Update the appropriate parts of the model and broadcast to the embedder. This
// may be called multiple times and thus must be idempotent.
- (void)loadCompleteWithSuccess:(BOOL)loadSuccess
                     forContext:(web::NavigationContextImpl*)context;
// Called after URL is finished loading and _loadPhase is set to PAGE_LOADED.
// |context| contains information about the navigation associated with the URL.
// It is nil if currentURL is invalid.
- (void)didFinishWithURL:(const GURL&)currentURL
             loadSuccess:(BOOL)loadSuccess
                 context:(nullable const web::NavigationContextImpl*)context;
// Acts on a single message from the JS object, parsed from JSON into a
// DictionaryValue. Returns NO if the format for the message was invalid.
- (BOOL)respondToMessage:(base::DictionaryValue*)crwMessage
       userIsInteracting:(BOOL)userIsInteracting
               originURL:(const GURL&)originURL
             isMainFrame:(BOOL)isMainFrame
             senderFrame:(web::WebFrame*)senderFrame;
// Called when web controller receives a new message from the web page.
- (void)didReceiveScriptMessage:(WKScriptMessage*)message;
// Returns a new script which wraps |script| with windowID check so |script| is
// not evaluated on windowID mismatch.
- (NSString*)scriptByAddingWindowIDCheckForScript:(NSString*)script;
// Attempts to handle a script message. Returns YES on success, NO otherwise.
- (BOOL)respondToWKScriptMessage:(WKScriptMessage*)scriptMessage;
// Handles frame became available message.
- (void)frameBecameAvailableWithMessage:(WKScriptMessage*)message;
// Handles frame became unavailable message.
- (void)frameBecameUnavailableWithMessage:(WKScriptMessage*)message;
// Clears the frames list.
- (void)removeAllWebFrames;
// Maps WKNavigationType to ui::PageTransition.
- (ui::PageTransition)pageTransitionFromNavigationType:
    (WKNavigationType)navigationType;

// Restores the state for this page from session history.
- (void)restoreStateFromHistory;
// Extracts the current page's viewport tag information and calls |completion|.
// If the page has changed before the viewport tag is successfully extracted,
// |completion| is called with nullptr.
typedef void (^ViewportStateCompletion)(const web::PageViewportState*);
- (void)extractViewportTagWithCompletion:(ViewportStateCompletion)completion;
// Called by NSNotificationCenter upon orientation changes.
- (void)orientationDidChange;
// Queries the web view for the user-scalable meta tag and calls
// |-applyPageDisplayState:userScalable:| with the result.
- (void)applyPageDisplayState:(const web::PageDisplayState&)displayState;
// Restores state of the web view's scroll view from |scrollState|.
// |isUserScalable| represents the value of user-scalable meta tag.
- (void)applyPageDisplayState:(const web::PageDisplayState&)displayState
                 userScalable:(BOOL)isUserScalable;
// Calls the zoom-preparation UIScrollViewDelegate callbacks on the web view.
// This is called before |-applyWebViewScrollZoomScaleFromScrollState:|.
- (void)prepareToApplyWebViewScrollZoomScale;
// Calls the zoom-completion UIScrollViewDelegate callbacks on the web view.
// This is called after |-applyWebViewScrollZoomScaleFromScrollState:|.
- (void)finishApplyingWebViewScrollZoomScale;
// Sets zoom scale value for webview scroll view from |zoomState|.
- (void)applyWebViewScrollZoomScaleFromZoomState:
    (const web::PageZoomState&)zoomState;
// Sets scroll offset value for webview scroll view from |scrollState|.
- (void)applyWebViewScrollOffsetFromScrollState:
    (const web::PageScrollState&)scrollState;
// Sets _documentURL to newURL, and updates any relevant state information.
- (void)setDocumentURL:(const GURL&)newURL
               context:(web::NavigationContextImpl*)context;
// Sets last committed NavigationItem's title to the given |title|, which can
// not be nil.
- (void)setNavigationItemTitle:(NSString*)title;
// Returns YES if the current navigation item corresponds to a web page
// loaded by a POST request.
- (BOOL)isCurrentNavigationItemPOST;
// Returns YES if current navigation item is WKNavigationTypeBackForward.
- (BOOL)isCurrentNavigationBackForward;
// Returns YES if the given WKBackForwardListItem is valid to use for
// navigation.
- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item;
// Finds all the scrollviews in the view hierarchy and makes sure they do not
// interfere with scroll to top when tapping the statusbar.
- (void)optOutScrollsToTopForSubviews;
// Returns YES if the navigation action is associated with a main frame request.
- (BOOL)isMainFrameNavigationAction:(WKNavigationAction*)action;
// Updates SSL status for the current navigation item based on the information
// provided by web view.
- (void)updateSSLStatusForCurrentNavigationItem;
// Called when a load ends in an SSL error and certificate chain.
- (void)handleSSLCertError:(NSError*)error
             forNavigation:(WKNavigation*)navigation;

// Used in webView:didReceiveAuthenticationChallenge:completionHandler: to
// reply with NSURLSessionAuthChallengeDisposition and credentials.
- (void)processAuthChallenge:(NSURLAuthenticationChallenge*)challenge
         forCertAcceptPolicy:(web::CertAcceptPolicy)policy
                  certStatus:(net::CertStatus)certStatus
           completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                       NSURLCredential*))completionHandler;
// Used in webView:didReceiveAuthenticationChallenge:completionHandler: to reply
// with NSURLSessionAuthChallengeDisposition and credentials.
- (void)handleHTTPAuthForChallenge:(NSURLAuthenticationChallenge*)challenge
                 completionHandler:
                     (void (^)(NSURLSessionAuthChallengeDisposition,
                               NSURLCredential*))completionHandler;
// Used in webView:didReceiveAuthenticationChallenge:completionHandler: to reply
// with NSURLSessionAuthChallengeDisposition and credentials.
+ (void)processHTTPAuthForUser:(NSString*)user
                      password:(NSString*)password
             completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                         NSURLCredential*))completionHandler;

// Returns YES if a SafeBrowsing warning is currently displayed within
// WKWebView.
- (BOOL)isSafeBrowsingWarningDisplayedInWebView;
// Loads request for the URL of the current navigation item. Subclasses may
// choose to build a new NSURLRequest and call |loadRequest| on the underlying
// web view, or use native web view navigation where possible (for example,
// going back and forward through the history stack).
- (void)loadRequestForCurrentNavigationItem;
// Reports Navigation.IOSWKWebViewSlowFastBackForward UMA. No-op if pending
// navigation is not back forward navigation.
- (void)reportBackForwardNavigationTypeForFastNavigation:(BOOL)isFast;

// Sets up WebUI for URL.
- (void)createWebUIForURL:(const GURL&)URL;
// Clears WebUI, if one exists.
- (void)clearWebUI;

@end

@implementation CRWWebController

// Synthesize as it is readonly.
@synthesize touchTrackingRecognizer = _touchTrackingRecognizer;

#pragma mark - Object lifecycle

- (instancetype)initWithWebState:(WebStateImpl*)webState {
  self = [super init];
  if (self) {
    _webStateImpl = webState;
    _webUsageEnabled = YES;

    if (web::GetWebClient()->IsSlimNavigationManagerEnabled())
      _allowsBackForwardNavigationGestures = YES;

    DCHECK(_webStateImpl);
    // Load phase when no WebView present is 'loaded' because this represents
    // the idle state.
    _loadPhase = web::PAGE_LOADED;
    // Content area is lazily instantiated.
    _defaultURL = GURL(url::kAboutBlankURL);
    _jsInjectionReceiver =
        [[CRWJSInjectionReceiver alloc] initWithEvaluator:self];
    _webViewProxy = [[CRWWebViewProxyImpl alloc] initWithWebController:self];
    [[_webViewProxy scrollViewProxy] addObserver:self];
    _pendingLoadCompleteActions = [[NSMutableArray alloc] init];
    web::BrowserState* browserState = _webStateImpl->GetBrowserState();
    _certVerificationController = [[CRWCertVerificationController alloc]
        initWithBrowserState:browserState];
    _certVerificationErrors =
        std::make_unique<CertVerificationErrorsCacheType>(kMaxCertErrorsCount);
    web::BrowsingDataRemover::FromBrowserState(browserState)->AddObserver(self);
    _navigationStates = [[CRWWKNavigationStates alloc] init];
    web::WebFramesManagerImpl::CreateForWebState(_webStateImpl);
    web::FindInPageManagerImpl::CreateForWebState(_webStateImpl);
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(orientationDidChange)
               name:UIApplicationDidChangeStatusBarOrientationNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  DCHECK([NSThread isMainThread]);
  DCHECK(_isBeingDestroyed);  // 'close' must have been called already.
  DCHECK(!_webView);
}

#pragma mark - Public property accessors

- (void)setWebUsageEnabled:(BOOL)enabled {
  if (_webUsageEnabled == enabled)
    return;
  // WKWebView autoreleases its WKProcessPool on removal from superview.
  // Deferring WKProcessPool deallocation may lead to issues with cookie
  // clearing and and Browsing Data Partitioning implementation.
  @autoreleasepool {
    if (!enabled) {
      [self removeWebView];
    }
  }

  _webUsageEnabled = enabled;

  // WKWebView autoreleases its WKProcessPool on removal from superview.
  // Deferring WKProcessPool deallocation may lead to issues with cookie
  // clearing and and Browsing Data Partitioning implementation.
  @autoreleasepool {
    [self setNativeControllerWebUsageEnabled:_webUsageEnabled];
    if (enabled) {
      // Don't create the web view; let it be lazy created as needed.
    } else {
      self.webStateImpl->ClearTransientContent();
      _touchTrackingRecognizer.touchTrackingDelegate = nil;
      _touchTrackingRecognizer = nil;
      _currentURLLoadWasTrigerred = NO;
    }
  }
}

- (UIView*)view {
  [self ensureContainerViewCreated];
  DCHECK(_containerView);
  return _containerView;
}

- (id<CRWWebViewNavigationProxy>)webViewNavigationProxy {
  return static_cast<id<CRWWebViewNavigationProxy>>(self.webView);
}

- (UIView*)viewForPrinting {
  // Printing is not supported for native controllers.
  return self.webView;
}

- (double)loadingProgress {
  return [self.webView estimatedProgress];
}

- (void)setAllowsBackForwardNavigationGestures:
    (BOOL)allowsBackForwardNavigationGestures {
  // Store it to an instance variable as well as
  // self.webView.allowsBackForwardNavigationGestures because self.webView may
  // be nil. When self.webView is nil, it will be set later in -setWebView:.
  _allowsBackForwardNavigationGestures = allowsBackForwardNavigationGestures;
  self.webView.allowsBackForwardNavigationGestures =
      allowsBackForwardNavigationGestures;
}

#pragma mark - Private properties accessors

- (void)setWebView:(WKWebView*)webView {
  DCHECK_NE(_webView, webView);

  // Unwind the old web view.
  // TODO(crbug.com/543374): Remove CRWWKScriptMessageRouter once
  // crbug.com/543374 is fixed.
  CRWWKScriptMessageRouter* messageRouter =
      [self webViewConfigurationProvider].GetScriptMessageRouter();
  if (_webView) {
    [messageRouter removeAllScriptMessageHandlersForWebView:_webView];
  }
  [_webView setNavigationDelegate:nil];
  [_webView setUIDelegate:nil];
  for (NSString* keyPath in self.WKWebViewObservers) {
    [_webView removeObserver:self forKeyPath:keyPath];
  }

  _webView = webView;

  // Set up the new web view.
  if (webView) {
    __weak CRWWebController* weakSelf = self;
    [messageRouter
        setScriptMessageHandler:^(WKScriptMessage* message) {
          [weakSelf didReceiveScriptMessage:message];
        }
                           name:kScriptMessageName
                        webView:webView];

    [messageRouter
        setScriptMessageHandler:^(WKScriptMessage* message) {
          [weakSelf frameBecameAvailableWithMessage:message];
        }
                           name:kFrameBecameAvailableMessageName
                        webView:webView];
    [messageRouter
        setScriptMessageHandler:^(WKScriptMessage* message) {
          [weakSelf frameBecameUnavailableWithMessage:message];
        }
                           name:kFrameBecameUnavailableMessageName
                        webView:webView];

    _windowIDJSManager = [[CRWJSWindowIDManager alloc] initWithWebView:webView];
  } else {
    _windowIDJSManager = nil;
  }
  [_webView setNavigationDelegate:self];
  [_webView setUIDelegate:self];
  for (NSString* keyPath in self.WKWebViewObservers) {
    [_webView addObserver:self forKeyPath:keyPath options:0 context:nullptr];
  }
  _webView.allowsBackForwardNavigationGestures =
      _allowsBackForwardNavigationGestures;
  _injectedScriptManagers = [[NSMutableSet alloc] init];
  [self setDocumentURL:_defaultURL context:nullptr];
}

- (UIScrollView*)webScrollView {
  return self.webView.scrollView;
}

- (web::PageDisplayState)pageDisplayState {
  web::PageDisplayState displayState;
  // If a native controller is present, record its display state instead of that
  // of the underlying placeholder webview.
  if (self.nativeController) {
    if ([self.nativeController respondsToSelector:@selector(contentOffset)]) {
      displayState.scroll_state().set_content_offset(
          [self.nativeController contentOffset]);
    }
    if ([self.nativeController respondsToSelector:@selector(contentInset)]) {
      displayState.scroll_state().set_content_inset(
          [self.nativeController contentInset]);
    }
  } else if (self.webView) {
    displayState.set_scroll_state(web::PageScrollState(
        self.scrollPosition, self.webScrollView.contentInset));
    UIScrollView* scrollView = self.webScrollView;
    displayState.zoom_state().set_minimum_zoom_scale(
        scrollView.minimumZoomScale);
    displayState.zoom_state().set_maximum_zoom_scale(
        scrollView.maximumZoomScale);
    displayState.zoom_state().set_zoom_scale(scrollView.zoomScale);
  }
  return displayState;
}

- (void)setPageDisplayState:(web::PageDisplayState)displayState {
  if (!displayState.IsValid())
    return;
  if (self.webView) {
    // Page state is restored after a page load completes.  If the user has
    // scrolled or changed the zoom scale while the page is still loading, don't
    // restore any state since it will confuse the user.
    web::PageDisplayState currentPageDisplayState = self.pageDisplayState;
    if (currentPageDisplayState.scroll_state() ==
            _displayStateOnStartLoading.scroll_state() &&
        !_pageHasZoomed) {
      [self applyPageDisplayState:displayState];
    }
  }
}

- (void)setNativeController:(id<CRWNativeContent>)nativeController {
  // Check for pointer equality.
  if (self.nativeController == nativeController)
    return;

  // Unset the delegate on the previous instance.
  if ([self.nativeController respondsToSelector:@selector(setDelegate:)])
    [self.nativeController setDelegate:nil];

  [_containerView displayNativeContent:nativeController];
  [self setNativeControllerWebUsageEnabled:_webUsageEnabled];
}

- (NSDictionary*)WKWebViewObservers {
  return @{
    @"serverTrust" : @"webViewSecurityFeaturesDidChange",
    @"estimatedProgress" : @"webViewEstimatedProgressDidChange",
    @"hasOnlySecureContent" : @"webViewSecurityFeaturesDidChange",
    @"title" : @"webViewTitleDidChange",
    @"loading" : @"webViewLoadingStateDidChange",
    @"URL" : @"webViewURLDidChange",
    @"canGoForward" : @"webViewBackForwardStateDidChange",
    @"canGoBack" : @"webViewBackForwardStateDidChange"
  };
}

- (GURL)currentURL {
  web::URLVerificationTrustLevel trustLevel =
      web::URLVerificationTrustLevel::kNone;
  return [self currentURLWithTrustLevel:&trustLevel];
}

- (web::Referrer)currentReferrer {
  // Referrer string doesn't include the fragment, so in cases where the
  // previous URL is equal to the current referrer plus the fragment the
  // previous URL is returned as current referrer.
  NSString* referrerString = _currentReferrerString;

  // In case of an error evaluating the JavaScript simply return empty string.
  if ([referrerString length] == 0)
    return web::Referrer();

  web::NavigationItem* item = self.currentNavItem;
  GURL navigationURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();
  NSString* previousURLString = base::SysUTF8ToNSString(navigationURL.spec());
  // Check if the referrer is equal to the previous URL minus the hash symbol.
  // L'#' is used to convert the char '#' to a unichar.
  if ([previousURLString length] > [referrerString length] &&
      [previousURLString hasPrefix:referrerString] &&
      [previousURLString characterAtIndex:[referrerString length]] == L'#') {
    referrerString = previousURLString;
  }
  // Since referrer is being extracted from the destination page, the correct
  // policy from the origin has *already* been applied. Since the extracted URL
  // is the post-policy value, and the source policy is no longer available,
  // the policy is set to Always so that whatever WebKit decided to send will be
  // re-sent when replaying the entry.
  // TODO(stuartmorgan): When possible, get the real referrer and policy in
  // advance and use that instead. https://crbug.com/227769.
  return web::Referrer(GURL(base::SysNSStringToUTF8(referrerString)),
                       web::ReferrerPolicyAlways);
}

- (BOOL)userClickedRecently {
  // Scrolling generates a pair of touch on/off event which causes
  // _lastUserInteraction to register that there was user interaction.
  // Checks for scrolling first to override time-based click heuristics.
  BOOL scrolling = [[self webScrollView] isDragging] ||
                   [[self webScrollView] isDecelerating];
  if (scrolling)
    return NO;
  if (!_lastUserInteraction)
    return NO;
  return _clickInProgress ||
         ((CFAbsoluteTimeGetCurrent() - _lastUserInteraction->time) <
          kMaximumDelayForUserInteractionInSeconds);
}

- (web::UserAgentType)userAgentType {
  web::NavigationItem* item = self.currentNavItem;
  return item ? item->GetUserAgentType() : web::UserAgentType::MOBILE;
}

- (web::MojoFacade*)mojoFacade {
  if (!_mojoFacade) {
    service_manager::mojom::InterfaceProvider* interfaceProvider =
        self.webStateImpl->GetWebStateInterfaceProvider();
    _mojoFacade =
        std::make_unique<web::MojoFacade>(interfaceProvider, self.webState);
  }
  return _mojoFacade.get();
}

- (void)setUserInteractionRegistered:(BOOL)flag {
  _userInteractionRegistered = flag;
  if (flag)
    _interactionRegisteredSinceLastURLChange = YES;
}

- (WebState*)webState {
  return _webStateImpl;
}

- (CGPoint)scrollPosition {
  return self.webScrollView.contentOffset;
}

- (CRWTouchTrackingRecognizer*)touchTrackingRecognizer {
  if (!_touchTrackingRecognizer) {
    _touchTrackingRecognizer =
        [[CRWTouchTrackingRecognizer alloc] initWithDelegate:self];
  }
  return _touchTrackingRecognizer;
}

#pragma mark Navigation and Session Information

- (CRWSessionController*)sessionController {
  NavigationManagerImpl* navigationManager = self.navigationManagerImpl;
  return navigationManager ? navigationManager->GetSessionController() : nil;
}

- (NavigationManagerImpl*)navigationManagerImpl {
  return self.webStateImpl ? &(self.webStateImpl->GetNavigationManagerImpl())
                           : nil;
}

- (BOOL)hasOpener {
  return self.webStateImpl ? self.webStateImpl->HasOpener() : NO;
}

- (web::NavigationItemImpl*)currentNavItem {
  return self.navigationManagerImpl
             ? self.navigationManagerImpl->GetCurrentItemImpl()
             : nullptr;
}

- (ui::PageTransition)currentTransition {
  if (self.currentNavItem)
    return self.currentNavItem->GetTransitionType();
  else
    return ui::PageTransitionFromInt(0);
}

- (web::Referrer)currentNavItemReferrer {
  web::NavigationItem* currentItem = self.currentNavItem;
  return currentItem ? currentItem->GetReferrer() : web::Referrer();
}

- (NSDictionary*)currentHTTPHeaders {
  web::NavigationItem* currentItem = self.currentNavItem;
  return currentItem ? currentItem->GetHttpRequestHeaders() : nil;
}

#pragma mark - ** Public Methods **

#pragma mark - Header public methods

- (web::NavigationItemImpl*)lastPendingItemForNewNavigation {
  WKNavigation* navigation =
      [_navigationStates lastNavigationWithPendingItemInNavigationContext];
  if (!navigation)
    return nullptr;
  web::NavigationContextImpl* context =
      [_navigationStates contextForNavigation:navigation];
  return context->GetItem();
}

- (void)showTransientContentView:(CRWContentView*)contentView {
  DCHECK(contentView);
  DCHECK(contentView.scrollView);
  // TODO(crbug.com/556848) Reenable DCHECK when |CRWWebControllerContainerView|
  // is restructured so that subviews are not added during |layoutSubviews|.
  // DCHECK([contentView.scrollView isDescendantOfView:contentView]);
  [_containerView displayTransientContent:contentView];
}

- (void)clearTransientContentView {
  // Early return if there is no transient content view.
  if (![_containerView transientContentView])
    return;

  // Remove the transient content view from the hierarchy.
  [_containerView clearTransientContentView];
}

// Stop doing stuff, especially network stuff. Close the request tracker.
- (void)terminateNetworkActivity {
  DCHECK(!_isHalted);
  _isHalted = YES;

  // Cancel all outstanding perform requests, and clear anything already queued
  // (since this may be called from within the handling loop) to prevent any
  // asynchronous JavaScript invocation handling from continuing.
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
}

- (void)dismissModals {
  if ([self.nativeController respondsToSelector:@selector(dismissModals)])
    [self.nativeController dismissModals];
}

// Caller must reset the delegate before calling.
- (void)close {
  self.webStateImpl->CancelDialogs();

  _SSLStatusUpdater = nil;
  _mojoFacade.reset();

  self.nativeProvider = nil;
  self.swipeRecognizerProvider = nil;
  if ([self.nativeController respondsToSelector:@selector(close)])
    [self.nativeController close];

  if (!_isHalted) {
    [self terminateNetworkActivity];
  }

  // Mark the destruction sequence has started, in case someone else holds a
  // strong reference and tries to continue using the tab.
  DCHECK(!_isBeingDestroyed);
  _isBeingDestroyed = YES;

  // Remove the web view now. Otherwise, delegate callbacks occur.
  [self removeWebView];

  // Explicitly reset content to clean up views and avoid dangling KVO
  // observers.
  [_containerView resetContent];

  _webStateImpl = nullptr;

  DCHECK(!self.webView);
  // TODO(crbug.com/662860): Don't set the delegate to nil.
  [_containerView setDelegate:nil];
  if ([self.nativeController respondsToSelector:@selector(setDelegate:)]) {
    [self.nativeController setDelegate:nil];
  }
  _touchTrackingRecognizer.touchTrackingDelegate = nil;
  [[_webViewProxy scrollViewProxy] removeObserver:self];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)isViewAlive {
  return !_webProcessCrashed && [_containerView isViewAlive];
}

- (BOOL)contentIsHTML {
  if (!self.webView)
    return NO;

  std::string MIMEType = self.webState->GetContentsMimeType();
  return MIMEType == "text/html" || MIMEType == "application/xhtml+xml" ||
         MIMEType == "application/xml";
}

- (GURL)currentURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel {
  DCHECK(trustLevel) << "Verification of the trustLevel state is mandatory";

  // The web view URL is the current URL only if it is neither a placeholder URL
  // (used to hold WKBackForwardListItem for WebUI and Native Content views) nor
  // a restore_session.html (used to replay session history in WKWebView).
  // TODO(crbug.com/738020): Investigate if this method is still needed and if
  // it can be implemented using NavigationManager API after removal of legacy
  // navigation stack.
  GURL webViewURL = net::GURLWithNSURL(self.webView.URL);
  if (self.webView && !IsWKInternalUrl(webViewURL)) {
    return [self webURLWithTrustLevel:trustLevel];
  }
  // Any non-web URL source is trusted.
  *trustLevel = web::URLVerificationTrustLevel::kAbsolute;
  if (self.nativeController) {
    if ([self.nativeController respondsToSelector:@selector(virtualURL)]) {
      return [self.nativeController virtualURL];
    } else {
      return [self.nativeController url];
    }
  }
  web::NavigationItem* item =
      self.navigationManagerImpl
          ->GetLastCommittedItemInCurrentOrRestoredSession();
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

- (void)reloadWithRendererInitiatedNavigation:(BOOL)rendererInitiated {
  // Clear last user interaction.
  // TODO(crbug.com/546337): Move to after the load commits, in the subclass
  // implementation. This will be inaccurate if the reload fails or is
  // cancelled.
  _lastUserInteraction = nullptr;
  base::RecordAction(base::UserMetricsAction("Reload"));
  GURL URL = self.currentNavItem->GetURL();
  if ([self shouldLoadURLInNativeView:URL]) {
    std::unique_ptr<web::NavigationContextImpl> navigationContext = [self
        registerLoadRequestForURL:URL
                         referrer:self.currentNavItemReferrer
                       transition:ui::PageTransition::PAGE_TRANSITION_RELOAD
           sameDocumentNavigation:NO
                   hasUserGesture:YES
                rendererInitiated:rendererInitiated
            placeholderNavigation:NO];
    self.webStateImpl->OnNavigationStarted(navigationContext.get());
    [self didStartLoading];
    self.navigationManagerImpl->CommitPendingItem(
        navigationContext->ReleaseItem());
    [self.nativeController reload];
    navigationContext->SetHasCommitted(true);
    self.webStateImpl->OnNavigationFinished(navigationContext.get());
    [self loadCompleteWithSuccess:YES forContext:nullptr];
  } else {
    web::NavigationItem* transientItem =
        self.navigationManagerImpl->GetTransientItem();
    if (transientItem) {
      // If there's a transient item, a reload is considered a new navigation to
      // the transient item's URL (as on other platforms).
      NavigationManager::WebLoadParams reloadParams(transientItem->GetURL());
      reloadParams.transition_type = ui::PAGE_TRANSITION_RELOAD;
      reloadParams.extra_headers =
          [transientItem->GetHttpRequestHeaders() copy];
      self.webState->GetNavigationManager()->LoadURLWithParams(reloadParams);
    } else {
      self.currentNavItem->SetTransitionType(
          ui::PageTransition::PAGE_TRANSITION_RELOAD);
      if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
          !web::GetWebClient()->IsAppSpecificURL(
              net::GURLWithNSURL(self.webView.URL))) {
        // New navigation manager can delegate directly to WKWebView to reload
        // for non-app-specific URLs. The necessary navigation states will be
        // updated in WKNavigationDelegate callbacks.
        WKNavigation* navigation = [self.webView reload];
        [_navigationStates setState:web::WKNavigationState::REQUESTED
                      forNavigation:navigation];
        std::unique_ptr<web::NavigationContextImpl> navigationContext = [self
            registerLoadRequestForURL:URL
                             referrer:self.currentNavItemReferrer
                           transition:ui::PageTransition::PAGE_TRANSITION_RELOAD
               sameDocumentNavigation:NO
                       hasUserGesture:YES
                    rendererInitiated:rendererInitiated
                placeholderNavigation:NO];
        [_navigationStates setContext:std::move(navigationContext)
                        forNavigation:navigation];
      } else {
        [self loadCurrentURLWithRendererInitiatedNavigation:rendererInitiated];
      }
    }
  }
}

- (void)stopLoading {
  _stoppedWKNavigation = [_navigationStates lastAddedNavigation];

  base::RecordAction(base::UserMetricsAction("Stop"));
  // Discard the pending and transient entried before notifying the tab model
  // observers of the change via |-abortLoad|.
  self.navigationManagerImpl->DiscardNonCommittedItems();
  [self abortLoad];
  web::NavigationItem* item = self.currentNavItem;
  GURL navigationURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();
  // If discarding the non-committed entries results in an app-specific URL,
  // reload it in its native view.
  if (!self.nativeController &&
      [self shouldLoadURLInNativeView:navigationURL]) {
    // RendererInitiated flag is meaningless for showing previous native
    // content page. RendererInitiated is used as less previledged.
    [self loadCurrentURLInNativeViewWithRendererInitiatedNavigation:YES];
  }
}

- (void)loadCurrentURLWithRendererInitiatedNavigation:(BOOL)rendererInitiated {
  // If the content view doesn't exist, the tab has either been evicted, or
  // never displayed. Bail, and let the URL be loaded when the tab is shown.
  if (!_containerView)
    return;

  // WKBasedNavigationManagerImpl needs WKWebView to load native views, but
  // WKWebView cannot be created while web usage is disabled to avoid breaking
  // clearing browser data. Bail now and let the URL be loaded when web
  // usage is enabled again. This can happen when purging web pages when an
  // interstitial is presented over a native view. See https://crbug.com/865985
  // for details.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      !_webUsageEnabled)
    return;

  _currentURLLoadWasTrigerred = YES;

  // Reset current WebUI if one exists.
  [self clearWebUI];

  // Abort any outstanding page load. This ensures the delegate gets informed
  // about the outgoing page, and further messages from the page are suppressed.
  if (_loadPhase != web::PAGE_LOADED)
    [self abortLoad];

  DCHECK(!_isHalted);
  self.webStateImpl->ClearTransientContent();

  web::NavigationItem* item = self.currentNavItem;
  const GURL currentURL = item ? item->GetURL() : GURL::EmptyGURL();
  const bool isCurrentURLAppSpecific =
      web::GetWebClient()->IsAppSpecificURL(currentURL);
  // If it's a chrome URL, but not a native one, create the WebUI instance.
  if (isCurrentURLAppSpecific &&
      ![_nativeProvider hasControllerForURL:currentURL]) {
    if (!(item->GetTransitionType() & ui::PAGE_TRANSITION_TYPED ||
          item->GetTransitionType() & ui::PAGE_TRANSITION_AUTO_BOOKMARK) &&
        self.hasOpener) {
      // WebUI URLs can not be opened by DOM to prevent cross-site scripting as
      // they have increased power. WebUI URLs may only be opened when the user
      // types in the URL or use bookmarks.
      self.navigationManagerImpl->DiscardNonCommittedItems();
      return;
    } else {
      [self createWebUIForURL:currentURL];
    }
  }

  // Loading a new url, must check here if it's a native chrome URL and
  // replace the appropriate view if so, or transition back to a web view from
  // a native view.
  if ([self shouldLoadURLInNativeView:currentURL]) {
    [self loadCurrentURLInNativeViewWithRendererInitiatedNavigation:
              rendererInitiated];
  } else if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
             isCurrentURLAppSpecific && self.webStateImpl->HasWebUI() &&
             !web::features::WebUISchemeHandlingEnabled()) {
    [self loadPlaceholderInWebViewForURL:currentURL
                       rendererInitiated:rendererInitiated
                              forContext:nullptr];
  } else {
    [self loadCurrentURLInWebView];
  }
}

- (void)loadCurrentURLIfNecessary {
  if (_webProcessCrashed) {
    [self loadCurrentURLWithRendererInitiatedNavigation:NO];
  } else if (!_currentURLLoadWasTrigerred) {
    [self ensureContainerViewCreated];

    // This method reloads last committed item, so make than item also pending.
    self.sessionController.pendingItemIndex =
        self.sessionController.lastCommittedItemIndex;

    // TODO(crbug.com/796608): end the practice of calling |loadCurrentURL|
    // when it is possible there is no current URL. If the call performs
    // necessary initialization, break that out.
    [self loadCurrentURLWithRendererInitiatedNavigation:NO];
  }
}

- (void)loadData:(NSData*)data
        MIMEType:(NSString*)MIMEType
          forURL:(const GURL&)URL {
  [self stopLoading];
  web::NavigationItemImpl* item =
      self.navigationManagerImpl->GetLastCommittedItemImpl();
  auto navigationContext = web::NavigationContextImpl::CreateNavigationContext(
      self.webStateImpl, URL,
      /*has_user_gesture=*/true, item->GetTransitionType(),
      /*is_renderer_initiated=*/false);
  _loadPhase = web::LOAD_REQUESTED;
  navigationContext->SetNavigationItemUniqueID(item->GetUniqueID());

  item->SetNavigationInitiationType(
      web::NavigationInitiationType::BROWSER_INITIATED);
  // The error_retry_state_machine may still be in the
  // |kDisplayingWebErrorForFailedNavigation| from the navigation that is being
  // replaced. As the navigation is now successful, the error can be cleared.
  item->error_retry_state_machine().SetNoNavigationError();
  // The load data call will replace the current navigation and the webView URL
  // of the navigation will be replaced by |URL|. Set the URL of the
  // navigationItem to keep them synced.
  // Note: it is possible that the URL in item already match |url|. But item can
  // also contain a placeholder URL intended to be replaced.
  item->SetURL(URL);
  navigationContext->SetMimeType(MIMEType);
  if (item->GetUserAgentType() == web::UserAgentType::NONE &&
      web::wk_navigation_util::URLNeedsUserAgentType(URL)) {
    item->SetUserAgentType(web::UserAgentType::MOBILE);
  }

  WKNavigation* navigation =
      [self.webView loadData:data
                       MIMEType:MIMEType
          characterEncodingName:base::SysUTF8ToNSString(base::kCodepageUTF8)
                        baseURL:net::NSURLWithGURL(URL)];

  [_navigationStates setContext:std::move(navigationContext)
                  forNavigation:navigation];
  [_navigationStates setState:web::WKNavigationState::REQUESTED
                forNavigation:navigation];
}

- (void)loadHTML:(NSString*)HTML forAppSpecificURL:(const GURL&)URL {
  CHECK(web::GetWebClient()->IsAppSpecificURL(URL));
  [self loadHTML:HTML forURL:URL];
}

// Loads the HTML into the page at the given URL. Extracted from
// loadHTML:forAppSpecificURL: for testing purpose.
- (void)loadHTML:(NSString*)HTML forURL:(const GURL&)URL {
  DCHECK(HTML.length);
  // Remove the transient content view.
  self.webStateImpl->ClearTransientContent();

  _loadPhase = web::LOAD_REQUESTED;

  // Web View should not be created for App Specific URLs.
  if (!web::GetWebClient()->IsAppSpecificURL(URL)) {
    [self ensureWebViewCreated];
    DCHECK(self.webView) << "self.webView null while trying to load HTML";
  }
  WKNavigation* navigation =
      [self.webView loadHTMLString:HTML baseURL:net::NSURLWithGURL(URL)];
  [_navigationStates setState:web::WKNavigationState::REQUESTED
                forNavigation:navigation];
  std::unique_ptr<web::NavigationContextImpl> context;
  const ui::PageTransition loadHTMLTransition =
      ui::PageTransition::PAGE_TRANSITION_TYPED;
  if (self.webStateImpl->HasWebUI()) {
    // WebUI uses |loadHTML:forURL:| to feed the content to web view. This
    // should not be treated as a navigation, but WKNavigationDelegate callbacks
    // still expect a valid context.
    context = web::NavigationContextImpl::CreateNavigationContext(
        self.webStateImpl, URL, /*has_user_gesture=*/true, loadHTMLTransition,
        /*is_renderer_initiated=*/false);
    context->SetNavigationItemUniqueID(self.currentNavItem->GetUniqueID());
    if (web::features::StorePendingItemInContext()) {
      // Transfer pending item ownership to NavigationContext.
      // NavigationManager owns pending item after navigation is requested and
      // until navigation context is created.
      context->SetItem(self.navigationManagerImpl->ReleasePendingItem());
    }
  } else {
    context = [self registerLoadRequestForURL:URL
                                     referrer:web::Referrer()
                                   transition:loadHTMLTransition
                       sameDocumentNavigation:NO
                               hasUserGesture:YES
                            rendererInitiated:NO
                        placeholderNavigation:NO];
  }
  context->SetLoadingHtmlString(true);
  context->SetMimeType(@"text/html");
  [_navigationStates setContext:std::move(context) forNavigation:navigation];
}

- (void)executeUserJavaScript:(NSString*)script
            completionHandler:(void (^)(id, NSError*))completion {
  // For security reasons, executing JavaScript on pages with app-specific URLs
  // is not allowed, because those pages may have elevated privileges.
  GURL lastCommittedURL = self.webState->GetLastCommittedURL();
  if (web::GetWebClient()->IsAppSpecificURL(lastCommittedURL)) {
    if (completion) {
      dispatch_async(dispatch_get_main_queue(), ^{
        NSError* error = [[NSError alloc]
            initWithDomain:web::kJSEvaluationErrorDomain
                      code:web::JS_EVALUATION_ERROR_CODE_NO_WEB_VIEW
                  userInfo:nil];
        completion(nil, error);
      });
    }
    return;
  }

  [self touched:YES];
  [self executeJavaScript:script completionHandler:completion];
}

- (void)requirePageReconstruction {
  // TODO(crbug.com/736103): Removing web view will destroy session history for
  // WKBasedNavigationManager.
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled())
    [self removeWebView];
}

- (void)recordStateInHistory {
  // Only record the state if:
  // - the current NavigationItem's URL matches the current URL, and
  // - the user has interacted with the page.
  web::NavigationItem* item = self.currentNavItem;
  if (item && item->GetURL() == [self currentURL] &&
      self.userInteractionRegistered) {
    item->SetPageDisplayState(self.pageDisplayState);
  }
}

- (void)wasShown {
  self.visible = YES;
  if ([self.nativeController respondsToSelector:@selector(wasShown)]) {
    [self.nativeController wasShown];
  }
}

- (void)wasHidden {
  self.visible = NO;
  if (_isHalted)
    return;
  [self recordStateInHistory];
  if ([self.nativeController respondsToSelector:@selector(wasHidden)]) {
    [self.nativeController wasHidden];
  }
}

- (id<CRWNativeContent>)nativeController {
  return [_containerView nativeController];
}

- (void)setKeepsRenderProcessAlive:(BOOL)keepsRenderProcessAlive {
  _keepsRenderProcessAlive = keepsRenderProcessAlive;
  [_containerView
      updateWebViewContentViewForContainerWindow:_containerView.window];
}

- (void)didFinishGoToIndexSameDocumentNavigationWithType:
            (web::NavigationInitiationType)type
                                          hasUserGesture:(BOOL)hasUserGesture {
  web::NavigationItem* item =
      self.webStateImpl->GetNavigationManager()->GetLastCommittedItem();
  GURL URL = item->GetVirtualURL();
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webStateImpl, URL, hasUserGesture,
          static_cast<ui::PageTransition>(
              item->GetTransitionType() |
              ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK),
          type == web::NavigationInitiationType::RENDERER_INITIATED);
  context->SetIsSameDocument(true);
  self.webStateImpl->SetIsLoading(true);
  self.webStateImpl->OnNavigationStarted(context.get());
  [self updateHTML5HistoryState];
  [self setDocumentURL:URL context:context.get()];
  context->SetHasCommitted(true);
  self.webStateImpl->OnNavigationFinished(context.get());
  [self didFinishWithURL:URL loadSuccess:YES context:context.get()];
}

- (void)goToBackForwardListItem:(WKBackForwardListItem*)wk_item
                 navigationItem:(web::NavigationItem*)item
       navigationInitiationType:(web::NavigationInitiationType)type
                 hasUserGesture:(BOOL)hasUserGesture {
  WKNavigation* navigation = [self.webView goToBackForwardListItem:wk_item];

  GURL URL = net::GURLWithNSURL(wk_item.URL);
  if (IsPlaceholderUrl(URL)) {
    // No need to create navigation context for placeholder back forward
    // navigations. Future callbacks do not expect that context will exist.
    return;
  }

  // This navigation can be an iframe navigation, but it's not possible to
  // distinguish it from the main frame navigation, so context still has to be
  // created.
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webStateImpl, URL, hasUserGesture,
          static_cast<ui::PageTransition>(
              item->GetTransitionType() |
              ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK),
          type == web::NavigationInitiationType::RENDERER_INITIATED);
  context->SetNavigationItemUniqueID(item->GetUniqueID());
  if (!navigation) {
    // goToBackForwardListItem: returns nil for same-document back forward
    // navigations.
    context->SetIsSameDocument(true);
  } else {
    self.webStateImpl->SetIsLoading(true);
    _loadPhase = web::LOAD_REQUESTED;
  }

  web::WKBackForwardListItemHolder* holder =
      web::WKBackForwardListItemHolder::FromNavigationItem(item);
  holder->set_navigation_type(WKNavigationTypeBackForward);
  context->SetIsPost((holder && [holder->http_method() isEqual:@"POST"]) ||
                     item->HasPostData());

  if (holder) {
    context->SetMimeType(holder->mime_type());
  }

  [_navigationStates setContext:std::move(context) forNavigation:navigation];
  [_navigationStates setState:web::WKNavigationState::REQUESTED
                forNavigation:navigation];
}

- (void)takeSnapshotWithRect:(CGRect)rect
                  completion:(void (^)(UIImage*))completion {
  if (!self.webView) {
    dispatch_async(dispatch_get_main_queue(), ^{
      completion(nil);
    });
  }

  WKSnapshotConfiguration* configuration =
      [[WKSnapshotConfiguration alloc] init];
  configuration.rect = [self.webView convertRect:rect fromView:self.view];
  __weak CRWWebController* weakSelf = self;
  [self.webView
      takeSnapshotWithConfiguration:configuration
                  completionHandler:^(UIImage* snapshot, NSError* error) {
                    // Pass nil to the completion block if there is an error
                    // or if the web view has been removed before the
                    // snapshot is finished.  |snapshot| can sometimes be
                    // corrupt if it's sent due to the WKWebView's
                    // deallocation, so callbacks received after
                    // |-removeWebView| are ignored to prevent crashing.
                    if (error || !weakSelf.webView) {
                      if (error) {
                        DLOG(ERROR) << "WKWebView snapshot error: "
                                    << error.description;
                      }
                      completion(nil);
                    } else {
                      completion(snapshot);
                    }
                  }];
}

#pragma mark - CRWJSInjectionEvaluator (Public)

- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))completionHandler {
  NSString* safeScript = [self scriptByAddingWindowIDCheckForScript:script];
  web::ExecuteJavaScript(self.webView, safeScript, completionHandler);
}

- (BOOL)scriptHasBeenInjectedForClass:(Class)injectionManagerClass {
  return [_injectedScriptManagers containsObject:injectionManagerClass];
}

- (void)injectScript:(NSString*)script forClass:(Class)JSInjectionManagerClass {
  DCHECK(script.length);
  // Script execution is an asynchronous operation which may pass sensitive
  // data to the page. executeJavaScript:completionHandler makes sure that
  // receiver page did not change by checking its window id.
  // |[self.webView executeJavaScript:completionHandler:]| is not used here
  // because it does not check that page is the same.
  [self executeJavaScript:script completionHandler:nil];
  [_injectedScriptManagers addObject:JSInjectionManagerClass];
}

#pragma mark - CRWSessionControllerDelegate (Public)

- (web::NavigationItemImpl*)pendingItemForSessionController:
    (CRWSessionController*)sessionController {
  return [self lastPendingItemForNewNavigation];
}

#pragma mark - CRWTouchTrackingDelegate (Public)

- (void)touched:(BOOL)touched {
  _clickInProgress = touched;
  if (touched) {
    self.userInteractionRegistered = YES;
    _userInteractedWithWebController = YES;
    if (_isBeingDestroyed)
      return;
    const NavigationManagerImpl* navigationManager = self.navigationManagerImpl;
    GURL mainDocumentURL =
        navigationManager->GetLastCommittedItem()
            ? navigationManager->GetLastCommittedItem()->GetURL()
            : [self currentURL];
    _lastUserInteraction =
        std::make_unique<UserInteractionEvent>(mainDocumentURL);
  }
}

#pragma mark - ** Private Methods **

#pragma mark -

- (BOOL)isSafeBrowsingWarningDisplayedInWebView {
  // A SafeBrowsing warning is a UIScrollView that is inserted on top of
  // WKWebView's scroll view. This method uses heuristics to detect this view.
  // It may break in the future if WebKit's implementation of SafeBrowsing
  // warnings changes.
  UIView* containingView = self.webView.scrollView.superview;
  if (!containingView)
    return NO;

  UIView* topView = containingView.subviews.lastObject;

  if (topView == self.webView.scrollView)
    return NO;

  return
      [topView isKindOfClass:[UIScrollView class]] &&
      [NSStringFromClass([topView class]) containsString:@"Warning"] &&
      topView.subviews.count > 0 &&
      [topView.subviews[0].subviews.lastObject isKindOfClass:[UIButton class]];
}

- (void)setDocumentURL:(const GURL&)newURL
               context:(web::NavigationContextImpl*)context {
  if (newURL != _documentURL && newURL.is_valid()) {
    _documentURL = newURL;
    _interactionRegisteredSinceLastURLChange = NO;
  }
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() && context &&
      !context->IsLoadingHtmlString() && !context->IsLoadingErrorPage() &&
      !IsWKInternalUrl(newURL) && !newURL.SchemeIs(url::kAboutScheme) &&
      self.webView) {
    GURL documentOrigin = newURL.GetOrigin();
    web::NavigationItem* committedItem =
        self.webStateImpl->GetNavigationManager()->GetLastCommittedItem();
    GURL committedOrigin =
        committedItem ? committedItem->GetURL().GetOrigin() : GURL::EmptyGURL();
    DCHECK_EQ(documentOrigin, committedOrigin)
        << "Old and new URL detection system have a mismatch";

    ukm::SourceId sourceID = ukm::ConvertToSourceId(
        context->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
    if (sourceID != ukm::kInvalidSourceId) {
      ukm::builders::IOS_URLMismatchInLegacyAndSlimNavigationManager(sourceID)
          .SetHasMismatch(documentOrigin != committedOrigin)
          .Record(ukm::UkmRecorder::Get());
    }
  }
}

- (void)setNavigationItemTitle:(NSString*)title {
  DCHECK(title);
  web::NavigationItem* item =
      self.navigationManagerImpl->GetLastCommittedItem();
  if (!item)
    return;

  item->SetTitle(base::SysNSStringToUTF16(title));
  self.webStateImpl->OnTitleChanged();
}

- (BOOL)isCurrentNavigationItemPOST {
  // |_pendingNavigationInfo| will be nil if the decidePolicy* delegate methods
  // were not called.
  NSString* HTTPMethod =
      _pendingNavigationInfo
          ? [_pendingNavigationInfo HTTPMethod]
          : [self currentBackForwardListItemHolder]->http_method();
  if ([HTTPMethod isEqual:@"POST"]) {
    return YES;
  }
  if (!self.currentNavItem) {
    return NO;
  }
  return self.currentNavItem->HasPostData();
}

- (BOOL)isCurrentNavigationBackForward {
  if (!self.currentNavItem)
    return NO;
  WKNavigationType currentNavigationType =
      [self currentBackForwardListItemHolder]->navigation_type();
  return currentNavigationType == WKNavigationTypeBackForward;
}

- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item {
  // The current back-forward list item MUST be in the WKWebView's back-forward
  // list to be valid.
  WKBackForwardList* list = self.webView.backForwardList;
  return list.currentItem == item ||
         [list.forwardList indexOfObject:item] != NSNotFound ||
         [list.backList indexOfObject:item] != NSNotFound;
}

// Returns the WKBackForwardListItemHolder for the current navigation item.
- (web::WKBackForwardListItemHolder*)currentBackForwardListItemHolder {
  web::NavigationItem* item = self.currentNavItem;
  DCHECK(item);
  web::WKBackForwardListItemHolder* holder =
      web::WKBackForwardListItemHolder::FromNavigationItem(item);
  DCHECK(holder);
  return holder;
}

- (GURL)webURLWithTrustLevel:(web::URLVerificationTrustLevel*)trustLevel {
  DCHECK(trustLevel);
  *trustLevel = web::URLVerificationTrustLevel::kAbsolute;
  // Placeholder URL is an implementation detail. Don't expose it to users of
  // web layer.
  if (IsPlaceholderUrl(_documentURL))
    return ExtractUrlFromPlaceholderUrl(_documentURL);
  return _documentURL;
}

- (BOOL)shouldClosePageOnNativeApplicationLoad {
  // The page should be closed if it was initiated by the DOM and there has been
  // no user interaction with the page since the web view was created, or if
  // the page has no navigation items, as occurs when an App Store link is
  // opened from another application.
  BOOL rendererInitiatedWithoutInteraction =
      self.hasOpener && !_userInteractedWithWebController;
  BOOL noNavigationItems = !(self.navigationManagerImpl->GetItemCount());
  return rendererInitiatedWithoutInteraction || noNavigationItems;
}

- (ui::PageTransition)pageTransitionFromNavigationType:
    (WKNavigationType)navigationType {
  switch (navigationType) {
    case WKNavigationTypeLinkActivated:
      return ui::PAGE_TRANSITION_LINK;
    case WKNavigationTypeFormSubmitted:
    case WKNavigationTypeFormResubmitted:
      return ui::PAGE_TRANSITION_FORM_SUBMIT;
    case WKNavigationTypeBackForward:
      return ui::PAGE_TRANSITION_FORWARD_BACK;
    case WKNavigationTypeReload:
      return ui::PAGE_TRANSITION_RELOAD;
    case WKNavigationTypeOther:
      // The "Other" type covers a variety of very different cases, which may
      // or may not be the result of user actions. For now, guess based on
      // whether there's been an interaction since the last URL change.
      // TODO(crbug.com/549301): See if this heuristic can be improved.
      return _interactionRegisteredSinceLastURLChange
                 ? ui::PAGE_TRANSITION_LINK
                 : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  }
}

#pragma mark - Navigation Helpers

// Registers load request with empty referrer and link or client redirect
// transition based on user interaction state. Returns navigation context for
// this request.
- (std::unique_ptr<web::NavigationContextImpl>)
    registerLoadRequestForURL:(const GURL&)URL
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated
        placeholderNavigation:(BOOL)placeholderNavigation {
  // Get the navigation type from the last main frame load request, and try to
  // map that to a PageTransition.
  WKNavigationType navigationType =
      _pendingNavigationInfo ? [_pendingNavigationInfo navigationType]
                             : WKNavigationTypeOther;
  ui::PageTransition transition =
      [self pageTransitionFromNavigationType:navigationType];

  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      navigationType == WKNavigationTypeBackForward &&
      self.webView.backForwardList.currentItem) {
    web::NavigationItem* currentItem = [[CRWNavigationItemHolder
        holderForBackForwardListItem:self.webView.backForwardList.currentItem]
        navigationItem];
    if (currentItem) {
      transition = ui::PageTransitionFromInt(transition |
                                             currentItem->GetTransitionType());
    }
  }

  // The referrer is not known yet, and will be updated later.
  const web::Referrer emptyReferrer;
  std::unique_ptr<web::NavigationContextImpl> context =
      [self registerLoadRequestForURL:URL
                             referrer:emptyReferrer
                           transition:transition
               sameDocumentNavigation:sameDocumentNavigation
                       hasUserGesture:hasUserGesture
                    rendererInitiated:rendererInitiated
                placeholderNavigation:placeholderNavigation];
  context->SetWKNavigationType(navigationType);
  return context;
}

// Prepares web controller and delegates for anticipated page change.
// Allows several methods to invoke webWill/DidAddPendingURL on anticipated page
// change, using the same cached request and calculated transition types.
// Returns navigation context for this request.
- (std::unique_ptr<web::NavigationContextImpl>)
    registerLoadRequestForURL:(const GURL&)requestURL
                     referrer:(const web::Referrer&)referrer
                   transition:(ui::PageTransition)transition
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)rendererInitiated
        placeholderNavigation:(BOOL)placeholderNavigation {
  // Transfer time is registered so that further transitions within the time
  // envelope are not also registered as links.
  _lastTransferTimeInSeconds = CFAbsoluteTimeGetCurrent();

  // Add or update pending item before any WebStateObserver callbacks.
  // See https://crbug.com/842151 for a scenario where this is important.
  web::NavigationItem* item =
      self.navigationManagerImpl->GetPendingItemInCurrentOrRestoredSession();
  if (item) {
    // Update the existing pending entry.
    // Typically on PAGE_TRANSITION_CLIENT_REDIRECT.
    // Don't update if request is a placeholder entry because the pending item
    // should have the original target URL.
    // Don't update if pending URL has a different origin, because client
    // redirects can not change the origin. It is possible to have more than one
    // pending navigations, so the redirect does not necesserily belong to the
    // pending navigation item.
    if (!placeholderNavigation &&
        item->GetURL().GetOrigin() == requestURL.GetOrigin()) {
      self.navigationManagerImpl->UpdatePendingItemUrl(requestURL);
    }
  } else {
    self.navigationManagerImpl->AddPendingItem(
        requestURL, referrer, transition,
        rendererInitiated ? web::NavigationInitiationType::RENDERER_INITIATED
                          : web::NavigationInitiationType::BROWSER_INITIATED,
        NavigationManager::UserAgentOverrideOption::INHERIT);
    item =
        self.navigationManagerImpl->GetPendingItemInCurrentOrRestoredSession();
  }

  bool redirect = transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK;
  if (!redirect) {
    // Before changing phases, the delegate should be informed that any existing
    // request is being cancelled before completion.
    [self loadCancelled];
    DCHECK(_loadPhase == web::PAGE_LOADED);
  }

  _loadPhase = web::LOAD_REQUESTED;

  // Record the state of outgoing web view. Do nothing if native controller
  // exists, because in that case recordStateInHistory will record the state
  // of incoming page as native controller is already inserted.
  // TODO(crbug.com/811770) Don't record state under WKBasedNavigationManager
  // because it may incorrectly clobber the incoming page if this is a
  // back/forward navigation. WKWebView restores page scroll state for web view
  // pages anyways so this only impacts user if WKWebView is deleted.
  if (!redirect && !self.nativeController &&
      !web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    [self recordStateInHistory];
  }

  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webStateImpl, requestURL, hasUserGesture, transition,
          rendererInitiated);
  context->SetPlaceholderNavigation(placeholderNavigation);

  // TODO(crbug.com/676129): LegacyNavigationManagerImpl::AddPendingItem does
  // not create a pending item in case of reload. Remove this workaround once
  // the bug is fixed or WKBasedNavigationManager is fully adopted.
  if (!item) {
    DCHECK(!web::GetWebClient()->IsSlimNavigationManagerEnabled());
    item = self.navigationManagerImpl->GetLastCommittedItem();
  }

  context->SetNavigationItemUniqueID(item->GetUniqueID());
  context->SetIsPost([self isCurrentNavigationItemPOST]);
  context->SetIsSameDocument(sameDocumentNavigation);

  if (!IsWKInternalUrl(requestURL) && !placeholderNavigation) {
    self.webStateImpl->SetIsLoading(true);

    // WKBasedNavigationManager triggers HTML load when placeholder navigation
    // finishes.
    if (!web::GetWebClient()->IsSlimNavigationManagerEnabled())
      [_webUIManager loadWebUIForURL:requestURL];
  }

  // WKWebView may have multiple pending items. Move pending item ownership from
  // NavigationManager to NavigationContext. NavigationManager owns pending item
  // after navigation was requested and until NavigationContext is created.
  if (web::features::StorePendingItemInContext()) {
    // No need to transfer the ownership for NativeContent URLs, because the
    // load of NativeContent is synchronous. No need to transfer the ownership
    // for WebUI navigations, because those navigation do not have access to
    // NavigationContext.
    if (![_nativeProvider hasControllerForURL:context->GetUrl()] &&
        !(_webUIManager &&
          web::GetWebClient()->IsAppSpecificURL(context->GetUrl()))) {
      if (self.navigationManagerImpl->GetPendingItemIndex() == -1) {
        context->SetItem(self.navigationManagerImpl->ReleasePendingItem());
      }
    }
  }

  return context;
}

// Loads the current URL in a web view, first ensuring the web view is visible.
- (void)loadCurrentURLInWebView {
  web::NavigationItem* item = self.currentNavItem;
  GURL targetURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();
  // Load the url. The UIWebView delegate callbacks take care of updating the
  // session history and UI.
  if (!targetURL.is_valid()) {
    [self didFinishWithURL:targetURL loadSuccess:NO context:nullptr];
    self.webStateImpl->SetIsLoading(false);
    self.webStateImpl->OnPageLoaded(targetURL, NO);
    return;
  }

  // JavaScript should never be evaluated here. User-entered JS should be
  // evaluated via stringByEvaluatingUserJavaScriptFromString.
  DCHECK(!targetURL.SchemeIs(url::kJavaScriptScheme));

  [self ensureWebViewCreated];

  [self loadRequestForCurrentNavigationItem];
}

- (void)loadRequestForCurrentNavigationItem {
  DCHECK(self.webView);
  DCHECK(self.currentNavItem);
  // If a load is kicked off on a WKWebView with a frame whose size is {0, 0} or
  // that has a negative dimension for a size, rendering issues occur that
  // manifest in erroneous scrolling and tap handling (crbug.com/574996,
  // crbug.com/577793).
  DCHECK_GT(CGRectGetWidth(self.webView.frame), 0.0);
  DCHECK_GT(CGRectGetHeight(self.webView.frame), 0.0);

  // If the current item uses a different user agent from that is currently used
  // in the web view, update |customUserAgent| property, which will be used by
  // the next request sent by this web view.
  web::UserAgentType itemUserAgentType =
      self.currentNavItem->GetUserAgentType();
  if (itemUserAgentType != web::UserAgentType::NONE) {
    NSString* userAgentString = base::SysUTF8ToNSString(
        web::GetWebClient()->GetUserAgent(itemUserAgentType));
    if (![self.webView.customUserAgent isEqualToString:userAgentString]) {
      self.webView.customUserAgent = userAgentString;
    }
  }

  web::WKBackForwardListItemHolder* holder =
      [self currentBackForwardListItemHolder];
  BOOL repostedForm =
      [holder->http_method() isEqual:@"POST"] &&
      (holder->navigation_type() == WKNavigationTypeFormResubmitted ||
       holder->navigation_type() == WKNavigationTypeFormSubmitted);
  web::NavigationItemImpl* currentItem = self.currentNavItem;
  NSData* POSTData = currentItem->GetPostData();
  NSMutableURLRequest* request = [self requestForCurrentNavigationItem];

  BOOL sameDocumentNavigation = currentItem->IsCreatedFromPushState() ||
                                currentItem->IsCreatedFromHashChange();

  if (holder->back_forward_list_item()) {
    // Check if holder's WKBackForwardListItem still correctly represents
    // navigation item. With LegacyNavigationManager, replaceState operation
    // creates a new navigation item, leaving the old item committed. That
    // old committed item will be associated with WKBackForwardListItem whose
    // state was replaced. So old item won't have correct WKBackForwardListItem.
    if (net::GURLWithNSURL(holder->back_forward_list_item().URL) !=
        currentItem->GetURL()) {
      // The state was replaced for this item. The item should not be a part of
      // committed items, but it's too late to remove the item. Cleaup
      // WKBackForwardListItem and mark item with "state replaced" flag.
      currentItem->SetHasStateBeenReplaced(true);
      holder->set_back_forward_list_item(nil);
    }
  }

  // If the request has POST data and is not a repost form, configure the POST
  // request.
  if (POSTData.length && !repostedForm) {
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:POSTData];
    [request setAllHTTPHeaderFields:self.currentHTTPHeaders];
  }

  ProceduralBlock defaultNavigationBlock = ^{
    web::NavigationItem* item = self.currentNavItem;
    GURL navigationURL = item ? item->GetURL() : GURL::EmptyGURL();
    GURL virtualURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();
    // Set |item| to nullptr here to avoid any use-after-free issues, as it can
    // be cleared by the call to -registerLoadRequestForURL below.
    item = nullptr;
    GURL contextURL = IsPlaceholderUrl(navigationURL)
                          ? ExtractUrlFromPlaceholderUrl(navigationURL)
                          : navigationURL;
    std::unique_ptr<web::NavigationContextImpl> navigationContext =
        [self registerLoadRequestForURL:contextURL
                               referrer:self.currentNavItemReferrer
                             transition:self.currentTransition
                 sameDocumentNavigation:sameDocumentNavigation
                         hasUserGesture:YES
                      rendererInitiated:NO
                  placeholderNavigation:IsPlaceholderUrl(navigationURL)];

    // Disable |allowsBackForwardNavigationGestures| during restore. Otherwise,
    // WebKit will trigger a snapshot for each (blank) page, and quickly
    // overload system memory.  Also disables the scroll proxy during session
    // restoration.
    if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
        self.navigationManagerImpl->IsRestoreSessionInProgress()) {
      _webView.allowsBackForwardNavigationGestures = NO;
      if (base::FeatureList::IsEnabled(
              web::features::kDisconnectScrollProxyDuringRestore))
        [_containerView disconnectScrollProxy];
    }

    WKNavigation* navigation = nil;
    if (navigationURL.SchemeIsFile() &&
        web::GetWebClient()->IsAppSpecificURL(virtualURL)) {
      // file:// URL navigations are allowed for app-specific URLs, which
      // already have elevated privileges.
      NSURL* navigationNSURL = net::NSURLWithGURL(navigationURL);
      navigation = [self.webView loadFileURL:navigationNSURL
                     allowingReadAccessToURL:navigationNSURL];
    } else {
      navigation = [self.webView loadRequest:request];
    }
    [_navigationStates setState:web::WKNavigationState::REQUESTED
                  forNavigation:navigation];
    [_navigationStates setContext:std::move(navigationContext)
                    forNavigation:navigation];
    [self reportBackForwardNavigationTypeForFastNavigation:NO];
  };

  // When navigating via WKBackForwardListItem to pages created or updated by
  // calls to pushState() and replaceState(), sometimes web_bundle.js is not
  // injected correctly.  This means that calling window.history navigation
  // functions will invoke WKWebView's non-overridden implementations, causing a
  // mismatch between the WKBackForwardList and NavigationManager.
  // TODO(crbug.com/659816): Figure out how to prevent web_bundle.js injection
  // flake.
  if (currentItem->HasStateBeenReplaced() ||
      currentItem->IsCreatedFromPushState()) {
    defaultNavigationBlock();
    return;
  }

  // If there is no corresponding WKBackForwardListItem, or the item is not in
  // the current WKWebView's back-forward list, navigating using WKWebView API
  // is not possible. In this case, fall back to the default navigation
  // mechanism.
  if (!holder->back_forward_list_item() ||
      ![self isBackForwardListItemValid:holder->back_forward_list_item()]) {
    defaultNavigationBlock();
    return;
  }

  ProceduralBlock webViewNavigationBlock = ^{
    // If the current navigation URL is the same as the URL of the visible
    // page, that means the user requested a reload. |goToBackForwardListItem|
    // will be a no-op when it is passed the current back forward list item,
    // so |reload| must be explicitly called.
    web::NavigationItem* item = self.currentNavItem;
    GURL navigationURL = item ? item->GetURL() : GURL::EmptyGURL();
    std::unique_ptr<web::NavigationContextImpl> navigationContext =
        [self registerLoadRequestForURL:navigationURL
                               referrer:self.currentNavItemReferrer
                             transition:self.currentTransition
                 sameDocumentNavigation:sameDocumentNavigation
                         hasUserGesture:YES
                      rendererInitiated:NO
                  placeholderNavigation:NO];
    WKNavigation* navigation = nil;
    if (navigationURL == net::GURLWithNSURL(self.webView.URL)) {
      navigation = [self.webView reload];
    } else {
      // |didCommitNavigation:| may not be called for fast navigation, so update
      // the navigation type now as it is already known.
      navigationContext->SetWKNavigationType(WKNavigationTypeBackForward);
      navigationContext->SetMimeType(holder->mime_type());
      holder->set_navigation_type(WKNavigationTypeBackForward);
      navigation = [self.webView
          goToBackForwardListItem:holder->back_forward_list_item()];
      [self reportBackForwardNavigationTypeForFastNavigation:YES];
    }
    [_navigationStates setState:web::WKNavigationState::REQUESTED
                  forNavigation:navigation];
    [_navigationStates setContext:std::move(navigationContext)
                    forNavigation:navigation];
  };

  // If the request is not a form submission or resubmission, or the user
  // doesn't need to confirm the load, then continue right away.

  if (!repostedForm || currentItem->ShouldSkipRepostFormConfirmation()) {
    webViewNavigationBlock();
    return;
  }

  // If the request is form submission or resubmission, then prompt the
  // user before proceeding.
  DCHECK(repostedForm);
  DCHECK(!web::GetWebClient()->IsSlimNavigationManagerEnabled());
  self.webStateImpl->ShowRepostFormWarningDialog(
      base::BindOnce(^(bool shouldContinue) {
        if (_isBeingDestroyed)
          return;

        if (shouldContinue)
          webViewNavigationBlock();
        else
          [self stopLoading];
      }));
}

- (void)reportBackForwardNavigationTypeForFastNavigation:(BOOL)isFast {
  NavigationManager* navigationManager = self.navigationManagerImpl;
  int pendingIndex = navigationManager->GetPendingItemIndex();
  if (pendingIndex == -1) {
    // Pending navigation is not a back forward navigation.
    return;
  }

  BOOL isBack = pendingIndex < navigationManager->GetLastCommittedItemIndex();
  BackForwardNavigationType type = BackForwardNavigationType::FAST_BACK;
  if (isBack) {
    type = isFast ? BackForwardNavigationType::FAST_BACK
                  : BackForwardNavigationType::SLOW_BACK;
  } else {
    type = isFast ? BackForwardNavigationType::FAST_FORWARD
                  : BackForwardNavigationType::SLOW_FORWARD;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.IOSWKWebViewSlowFastBackForward", type,
      BackForwardNavigationType::BACK_FORWARD_NAVIGATION_TYPE_COUNT);
}

- (NSMutableURLRequest*)requestForCurrentNavigationItem {
  web::NavigationItem* item = self.currentNavItem;
  const GURL currentNavigationURL = item ? item->GetURL() : GURL::EmptyGURL();
  NSMutableURLRequest* request = [NSMutableURLRequest
      requestWithURL:net::NSURLWithGURL(currentNavigationURL)];
  const web::Referrer referrer(self.currentNavItemReferrer);
  if (referrer.url.is_valid()) {
    std::string referrerValue =
        web::ReferrerHeaderValueForNavigation(currentNavigationURL, referrer);
    if (!referrerValue.empty()) {
      [request setValue:base::SysUTF8ToNSString(referrerValue)
          forHTTPHeaderField:kReferrerHeaderName];
    }
  }

  // If there are headers in the current session entry add them to |request|.
  // Headers that would overwrite fields already present in |request| are
  // skipped.
  NSDictionary* headers = self.currentHTTPHeaders;
  for (NSString* headerName in headers) {
    if (![request valueForHTTPHeaderField:headerName]) {
      [request setValue:[headers objectForKey:headerName]
          forHTTPHeaderField:headerName];
    }
  }

  return request;
}

- (web::NavigationContextImpl*)
    loadPlaceholderInWebViewForURL:(const GURL&)originalURL
                 rendererInitiated:(BOOL)rendererInitiated
                        forContext:(std::unique_ptr<web::NavigationContextImpl>)
                                       originalContext {
  GURL placeholderURL = CreatePlaceholderUrlForUrl(originalURL);
  [self ensureWebViewCreated];

  NSURLRequest* request =
      [NSURLRequest requestWithURL:net::NSURLWithGURL(placeholderURL)];
  WKNavigation* navigation = [self.webView loadRequest:request];
  [_navigationStates setState:web::WKNavigationState::REQUESTED
                forNavigation:navigation];
  std::unique_ptr<web::NavigationContextImpl> navigationContext;
  if (originalContext) {
    navigationContext = std::move(originalContext);
    navigationContext->SetPlaceholderNavigation(YES);
  } else {
    navigationContext = [self registerLoadRequestForURL:originalURL
                                 sameDocumentNavigation:NO
                                         hasUserGesture:NO
                                      rendererInitiated:rendererInitiated
                                  placeholderNavigation:YES];
  }
  [_navigationStates setContext:std::move(navigationContext)
                  forNavigation:navigation];
  return [_navigationStates contextForNavigation:navigation];
}

#pragma mark - End of loading

- (void)abortLoad {
  [self.webView stopLoading];
  [_pendingNavigationInfo setCancelled:YES];
  _certVerificationErrors->Clear();
  [self loadCancelled];
  _safeBrowsingWarningDetectionTimer.Stop();
}

- (void)loadCancelled {
  // TODO(crbug.com/821995):  Check if this function should be removed.
  if (_loadPhase != web::PAGE_LOADED) {
    _loadPhase = web::PAGE_LOADED;
    if (!_isHalted) {
      self.webStateImpl->SetIsLoading(false);
    }
  }
}

- (void)didFinishNavigation:(web::NavigationContextImpl*)context {
  // This can be called at multiple times after the document has loaded. Do
  // nothing if the document has already loaded.
  if (_loadPhase == web::PAGE_LOADED)
    return;

  // Restore allowsBackForwardNavigationGestures and the scroll proxy once
  // restoration is complete.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      !self.navigationManagerImpl->IsRestoreSessionInProgress()) {
    if (_webView.allowsBackForwardNavigationGestures !=
        _allowsBackForwardNavigationGestures) {
      _webView.allowsBackForwardNavigationGestures =
          _allowsBackForwardNavigationGestures;
    }

    if (base::FeatureList::IsEnabled(
            web::features::kDisconnectScrollProxyDuringRestore)) {
      [_containerView reconnectScrollProxy];
    }
  }

  BOOL success = !context || !context->GetError();
  [self loadCompleteWithSuccess:success forContext:context];
}

- (void)loadCompleteWithSuccess:(BOOL)loadSuccess
                     forContext:(web::NavigationContextImpl*)context {
  // The webView may have been torn down (or replaced by a native view). Be
  // safe and do nothing if that's happened.
  if (_loadPhase != web::PAGE_LOADING)
    return;

  const GURL currentURL([self currentURL]);

  _loadPhase = web::PAGE_LOADED;

  [self optOutScrollsToTopForSubviews];

  // Perform post-load-finished updates.
  [self didFinishWithURL:currentURL loadSuccess:loadSuccess context:context];

  // Execute the pending LoadCompleteActions.
  for (ProceduralBlock action in _pendingLoadCompleteActions) {
    action();
  }
  [_pendingLoadCompleteActions removeAllObjects];
}

- (void)didFinishWithURL:(const GURL&)currentURL
             loadSuccess:(BOOL)loadSuccess
                 context:(nullable const web::NavigationContextImpl*)context {
  DCHECK(_loadPhase == web::PAGE_LOADED);
  // Rather than creating a new WKBackForwardListItem when loading WebUI pages,
  // WKWebView will cache the WebUI HTML in the previous WKBackForwardListItem
  // since it's loaded via |-loadHTML:forURL:| instead of an NSURLRequest.  As a
  // result, the WebUI's HTML and URL will be loaded when navigating to that
  // WKBackForwardListItem, causing a mismatch between the visible content and
  // the visible URL (WebUI page will be visible, but URL will be the previous
  // page's URL).  To prevent this potential URL spoofing vulnerability, reset
  // the previous NavigationItem's WKBackForwardListItem to force loading via
  // NSURLRequest.
  if (_webUIManager) {
    web::NavigationItem* lastNavigationItem =
        self.sessionController.previousItem;
    if (lastNavigationItem) {
      web::WKBackForwardListItemHolder* holder =
          web::WKBackForwardListItemHolder::FromNavigationItem(
              lastNavigationItem);
      DCHECK(holder);
      holder->set_back_forward_list_item(nil);
    }
  }

  [self restoreStateFromHistory];

  // Placeholder and restore session URLs are implementation details so should
  // not notify WebStateObservers. If |context| is nullptr, don't skip
  // placeholder URLs because this may be the only opportunity to update
  // |isLoading| for native view reload.

  if (context && context->IsPlaceholderNavigation())
    return;

  if (context && IsRestoreSessionUrl(context->GetUrl()))
    return;

  if (IsRestoreSessionUrl(net::GURLWithNSURL(self.webView.URL)))
    return;

  if (context && context->IsLoadingErrorPage())
    return;

  if (!loadSuccess) {
    // WebStateObserver callbacks will be called for load failure after
    // loading placeholder URL.
    return;
  }

  if (![_navigationStates lastNavigationWithPendingItemInNavigationContext] ||
      !web::features::StorePendingItemInContext()) {
    self.webStateImpl->SetIsLoading(false);
  } else {
    // There is another pending navigation, so the state is still loading.
  }
  self.webStateImpl->OnPageLoaded(currentURL, YES);
}

#pragma mark - Error Helpers

- (void)loadErrorPageForNavigationItem:(web::NavigationItemImpl*)item
                     navigationContext:(web::NavigationContextImpl*)context {
  const GURL currentURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();
  NSError* error = context->GetError();
  DCHECK(error);
  DCHECK_EQ(item->GetUniqueID(), context->GetNavigationItemUniqueID());

  if (web::IsWKWebViewSSLCertError(error)) {
    // This could happen only if certificate is absent or could not be parsed.
    error = web::NetErrorFromError(error, net::ERR_SSL_SERVER_CERT_BAD_FORMAT);
#if defined(DEBUG)
    net::SSLInfo info;
    web::GetSSLInfoFromWKWebViewSSLCertError(error, &info);
    CHECK(!error.cert);
#endif
  } else {
    error = web::NetErrorFromError(error);
  }
  NSString* failingURLString =
      error.userInfo[NSURLErrorFailingURLStringErrorKey];
  GURL failingURL(base::SysNSStringToUTF8(failingURLString));
  NSString* errorHTML = nil;
  web::GetWebClient()->PrepareErrorPage(
      self.webStateImpl, failingURL, error, context->IsPost(),
      self.webStateImpl->GetBrowserState()->IsOffTheRecord(), &errorHTML);

  WKNavigation* navigation =
      [self.webView loadHTMLString:errorHTML
                           baseURL:net::NSURLWithGURL(failingURL)];

  auto loadHTMLContext = web::NavigationContextImpl::CreateNavigationContext(
      self.webStateImpl, failingURL,
      /*has_user_gesture=*/false, ui::PAGE_TRANSITION_FIRST,
      /*is_renderer_initiated=*/false);
  loadHTMLContext->SetLoadingErrorPage(true);
  loadHTMLContext->SetNavigationItemUniqueID(item->GetUniqueID());

  [_navigationStates setContext:std::move(loadHTMLContext)
                  forNavigation:navigation];
  [_navigationStates setState:web::WKNavigationState::REQUESTED
                forNavigation:navigation];

  // TODO(crbug.com/803503): only call these for placeholder navigation because
  // they should have already been triggered during navigation commit for
  // failures that happen after commit.
  [self didStartLoading];
  self.navigationManagerImpl->CommitPendingItem(context->ReleaseItem());
  web::NavigationItem* lastCommittedItem =
      self.navigationManagerImpl->GetLastCommittedItem();
  [self setDocumentURL:lastCommittedItem->GetURL() context:context];

  // If |context| is a placeholder navigation, this is the second part of the
  // error page load for a provisional load failure. Rewrite the context URL to
  // actual URL and trigger the deferred |OnNavigationFinished| callback. This
  // is also needed if |context| is not yet committed, which can happen on a
  // reload/back/forward load that failed in provisional navigation.
  if (context->IsPlaceholderNavigation() || !context->HasCommitted()) {
    context->SetUrl(item->GetURL());
    context->SetPlaceholderNavigation(false);
    context->SetHasCommitted(true);
    self.webStateImpl->OnNavigationFinished(context);
  }

  [self loadCompleteWithSuccess:NO forContext:context];
  self.webStateImpl->SetIsLoading(false);
  self.webStateImpl->OnPageLoaded(failingURL, NO);
}

- (void)handleErrorRetryCommand:(web::ErrorRetryCommand)command
                 navigationItem:(web::NavigationItemImpl*)item
              navigationContext:(web::NavigationContextImpl*)context
             originalNavigation:(WKNavigation*)originalNavigation {
  if (command == web::ErrorRetryCommand::kDoNothing)
    return;

  DCHECK_EQ(item->GetUniqueID(), context->GetNavigationItemUniqueID());
  switch (command) {
    case web::ErrorRetryCommand::kLoadPlaceholder: {
      // This case only happens when a new request failed in provisional
      // navigation. Disassociate the navigation context from the original
      // request and resuse it for the placeholder navigation.
      std::unique_ptr<web::NavigationContextImpl> originalContext =
          [_navigationStates removeNavigation:originalNavigation];
      [self loadPlaceholderInWebViewForURL:item->GetURL()
                         rendererInitiated:context->IsRendererInitiated()
                                forContext:std::move(originalContext)];
    } break;

    case web::ErrorRetryCommand::kLoadErrorView:
      [self loadErrorPageForNavigationItem:item navigationContext:context];
      break;

    case web::ErrorRetryCommand::kReload:
      [self.webView reload];
      break;

    case web::ErrorRetryCommand::kRewriteWebViewURL: {
      std::unique_ptr<web::NavigationContextImpl> navigationContext =
          [self registerLoadRequestForURL:item->GetURL()
                   sameDocumentNavigation:NO
                           hasUserGesture:NO
                        rendererInitiated:context->IsRendererInitiated()
                    placeholderNavigation:NO];
      WKNavigation* navigation =
          [self.webView loadHTMLString:@""
                               baseURL:net::NSURLWithGURL(item->GetURL())];
      navigationContext->SetError(context->GetError());
      navigationContext->SetIsPost(context->IsPost());
      [_navigationStates setContext:std::move(navigationContext)
                      forNavigation:navigation];
    } break;

    case web::ErrorRetryCommand::kDoNothing:
      NOTREACHED();
  }
}

#pragma mark - BrowsingDataRemoverObserver

- (void)willRemoveBrowsingData:(web::BrowsingDataRemover*)dataRemover {
  self.webUsageEnabled = NO;
}

- (void)didRemoveBrowsingData:(web::BrowsingDataRemover*)dataRemover {
  self.webUsageEnabled = YES;
}

#pragma mark - JavaScript history manipulation

// Updates the HTML5 history state of the page using the current NavigationItem.
// For same-document navigations and navigations affected by
// window.history.[push/replace]State(), the URL and serialized state object
// will be updated to the current NavigationItem's values.  A popState event
// will be triggered for all same-document navigations.  Additionally, a
// hashchange event will be triggered for same-document navigations where the
// only difference between the current and previous URL is the fragment.
// TODO(crbug.com/788465): Verify that the history state management here are not
// needed for WKBasedNavigationManagerImpl and delete this method. The
// OnNavigationItemCommitted() call is likely the only thing that needs to be
// retained.
- (void)updateHTML5HistoryState {
  web::NavigationItemImpl* currentItem = self.currentNavItem;
  if (!currentItem)
    return;

  // Same-document navigations must trigger a popState event.
  CRWSessionController* sessionController = self.sessionController;
  BOOL sameDocumentNavigation = [sessionController
      isSameDocumentNavigationBetweenItem:sessionController.currentItem
                                  andItem:sessionController.previousItem];
  // WKWebView doesn't send hashchange events for same-document non-BFLI
  // navigations, so one must be dispatched manually for hash change same-
  // document navigations.
  const GURL URL = currentItem->GetURL();
  web::NavigationItem* previousItem = self.sessionController.previousItem;
  const GURL oldURL = previousItem ? previousItem->GetURL() : GURL();
  BOOL shouldDispatchHashchange = sameDocumentNavigation && previousItem &&
                                  (web::GURLByRemovingRefFromGURL(URL) ==
                                   web::GURLByRemovingRefFromGURL(oldURL));
  // The URL and state object must be set for same-document navigations and
  // NavigationItems that were created or updated by calls to pushState() or
  // replaceState().
  BOOL shouldUpdateState = sameDocumentNavigation ||
                           currentItem->IsCreatedFromPushState() ||
                           currentItem->HasStateBeenReplaced();
  if (!shouldUpdateState)
    return;

  // TODO(stuartmorgan): Make CRWSessionController manage this internally (or
  // remove it; it's not clear this matches other platforms' behavior).
  self.navigationManagerImpl->OnNavigationItemCommitted();
  // Record that a same-document hashchange event will be fired.  This flag will
  // be reset when resonding to the hashchange message.  Note that resetting the
  // flag in the completion block below is too early, as that block is called
  // before hashchange event listeners have a chance to fire.
  _dispatchingSameDocumentHashChangeEvent = shouldDispatchHashchange;
  // Inject the JavaScript to update the state on the browser side.
  [self injectHTML5HistoryScriptWithHashChange:shouldDispatchHashchange
                        sameDocumentNavigation:sameDocumentNavigation];
}

// Generates the JavaScript string used to update the UIWebView's URL so that it
// matches the URL displayed in the omnibox and sets window.history.state to
// stateObject. Needed for history.pushState() and history.replaceState().
- (NSString*)javaScriptToReplaceWebViewURL:(const GURL&)URL
                           stateObjectJSON:(NSString*)stateObject {
  std::string outURL;
  base::EscapeJSONString(URL.spec(), true, &outURL);
  return
      [NSString stringWithFormat:@"__gCrWeb.replaceWebViewURL(%@, %@);",
                                 base::SysUTF8ToNSString(outURL), stateObject];
}

// Generates the JavaScript string used to manually dispatch a popstate event,
// using |stateObjectJSON| as the event parameter.
- (NSString*)javaScriptToDispatchPopStateWithObject:(NSString*)stateObjectJSON {
  std::string outState;
  base::EscapeJSONString(base::SysNSStringToUTF8(stateObjectJSON), true,
                         &outState);
  return [NSString stringWithFormat:@"__gCrWeb.dispatchPopstateEvent(%@);",
                                    base::SysUTF8ToNSString(outState)];
}

// Generates the JavaScript string used to manually dispatch a hashchange event,
// using |oldURL| and |newURL| as the event parameters.
- (NSString*)javaScriptToDispatchHashChangeWithOldURL:(const GURL&)oldURL
                                               newURL:(const GURL&)newURL {
  return [NSString
      stringWithFormat:@"__gCrWeb.dispatchHashchangeEvent(\'%s\', \'%s\');",
                       oldURL.spec().c_str(), newURL.spec().c_str()];
}

// Injects JavaScript to update the URL and state object of the webview to the
// values found in the current NavigationItem.  A hashchange event will be
// dispatched if |dispatchHashChange| is YES, and a popstate event will be
// dispatched if |sameDocument| is YES.  Upon the script's completion, resets
// |urlOnStartLoading_| and |_lastRegisteredRequestURL| to the current
// NavigationItem's URL.  This is necessary so that sites that depend on URL
// params/fragments continue to work correctly and that checks for the URL don't
// incorrectly trigger |-webPageChangedWithContext| calls.
- (void)injectHTML5HistoryScriptWithHashChange:(BOOL)dispatchHashChange
                        sameDocumentNavigation:(BOOL)sameDocumentNavigation {
  web::NavigationItemImpl* currentItem = self.currentNavItem;
  if (!currentItem)
    return;

  const GURL URL = currentItem->GetURL();
  NSString* stateObject = currentItem->GetSerializedStateObject();
  NSMutableString* script = [NSMutableString
      stringWithString:[self javaScriptToReplaceWebViewURL:URL
                                           stateObjectJSON:stateObject]];
  if (sameDocumentNavigation) {
    [script
        appendString:[self javaScriptToDispatchPopStateWithObject:stateObject]];
  }
  if (dispatchHashChange) {
    web::NavigationItemImpl* previousItem = self.sessionController.previousItem;
    const GURL oldURL = previousItem ? previousItem->GetURL() : GURL();
    [script appendString:[self javaScriptToDispatchHashChangeWithOldURL:oldURL
                                                                 newURL:URL]];
  }
  [self executeJavaScript:script completionHandler:nil];
}

#pragma mark - Native Content

// Returns |YES| if |url| should be loaded in a native view.
- (BOOL)shouldLoadURLInNativeView:(const GURL&)url {
  // App-specific URLs that don't require WebUI are loaded in native views.
  return web::GetWebClient()->IsAppSpecificURL(url) &&
         !self.webStateImpl->HasWebUI() &&
         [_nativeProvider hasControllerForURL:url];
}

// Informs the native controller if web usage is allowed or not.
- (void)setNativeControllerWebUsageEnabled:(BOOL)webUsageEnabled {
  if ([self.nativeController
          respondsToSelector:@selector(setWebUsageEnabled:)]) {
    [self.nativeController setWebUsageEnabled:webUsageEnabled];
  }
}

// Loads the current URL in a native controller if using the legacy navigation
// stack. If the new navigation stack is used, start loading a placeholder
// into the web view, upon the completion of which the native controller will
// be triggered.
- (void)loadCurrentURLInNativeViewWithRendererInitiatedNavigation:
    (BOOL)rendererInitiated {
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // Free the web view.
    [self removeWebView];
    [self presentNativeContentForNavigationItem:self.currentNavItem];
    [self didLoadNativeContentForNavigationItem:self.currentNavItem
                             placeholderContext:nullptr
                              rendererInitiated:rendererInitiated];
  } else {
    // Just present the native view now. Leave the rest of native content load
    // until the placeholder navigation finishes.
    [self presentNativeContentForNavigationItem:self.currentNavItem];
    web::NavigationContextImpl* context = [self
        loadPlaceholderInWebViewForURL:self.currentNavItem->GetVirtualURL()
                     rendererInitiated:rendererInitiated
                            forContext:nullptr];
    context->SetIsNativeContentPresented(true);
  }
}

// Presents native content using the native controller for |item| without
// notifying WebStateObservers. This method does not modify the underlying web
// view. It simply covers the web view with the native content.
// |-didLoadNativeContentForNavigationItem| must be called some time later
// to notify WebStateObservers.
- (void)presentNativeContentForNavigationItem:(web::NavigationItem*)item {
  const GURL targetURL = item ? item->GetURL() : GURL::EmptyGURL();
  id<CRWNativeContent> nativeContent =
      [_nativeProvider controllerForURL:targetURL webState:self.webState];
  // Unlike the WebView case, always create a new controller and view.
  // TODO(crbug.com/759178): What to do if this does return nil?
  [self setNativeController:nativeContent];
  if ([nativeContent respondsToSelector:@selector(virtualURL)]) {
    item->SetVirtualURL([nativeContent virtualURL]);
  }

  NSString* title = [self.nativeController title];
  if (title && item) {
    base::string16 newTitle = base::SysNSStringToUTF16(title);
    item->SetTitle(newTitle);
  }
}

// Notifies WebStateObservers the completion of this navigation.
- (void)didLoadNativeContentForNavigationItem:(web::NavigationItemImpl*)item
                           placeholderContext:
                               (web::NavigationContextImpl*)placeholderContext
                            rendererInitiated:(BOOL)rendererInitiated {
  DCHECK(!placeholderContext || placeholderContext->IsPlaceholderNavigation());
  const GURL targetURL = item ? item->GetURL() : GURL::EmptyGURL();
  const web::Referrer referrer;
  std::unique_ptr<web::NavigationContextImpl> context =
      [self registerLoadRequestForURL:targetURL
                             referrer:referrer
                           transition:self.currentTransition
               sameDocumentNavigation:NO
                       hasUserGesture:YES
                    rendererInitiated:rendererInitiated
                placeholderNavigation:NO];

  self.webStateImpl->OnNavigationStarted(context.get());
  [self didStartLoading];
  if (placeholderContext && placeholderContext->GetItem()) {
    DCHECK_EQ(placeholderContext->GetItem(), item);
    self.navigationManagerImpl->CommitPendingItem(
        placeholderContext->ReleaseItem());
  } else {
    self.navigationManagerImpl->CommitPendingItem();
  }
  context->SetHasCommitted(true);
  self.webStateImpl->OnNavigationFinished(context.get());

  if (item && web::GetWebClient()->IsAppSpecificURL(item->GetURL())) {
    // Report the successful navigation to the ErrorRetryStateMachine.
    item->error_retry_state_machine().SetNoNavigationError();
  }

  NSString* title = [self.nativeController title];
  if (title) {
    [self setNavigationItemTitle:title];
  }

  if ([self.nativeController respondsToSelector:@selector(setDelegate:)]) {
    [self.nativeController setDelegate:self];
  }

  _loadPhase = web::PAGE_LOADED;
  [self didFinishWithURL:targetURL loadSuccess:YES context:context.get()];
}

// Discards non committed items, only if the last committed URL was not loaded
// in native view. But if it was a native view, no discard will happen to avoid
// an ugly animation where the web view is inserted and quickly removed.
- (void)discardNonCommittedItemsIfLastCommittedWasNotNativeView {
  GURL lastCommittedURL = self.webState->GetLastCommittedURL();
  BOOL previousItemWasLoadedInNativeView =
      [self shouldLoadURLInNativeView:lastCommittedURL];
  if (!previousItemWasLoadedInNativeView)
    self.navigationManagerImpl->DiscardNonCommittedItems();
}

#pragma mark - CRWWebControllerContainerViewDelegate

- (CRWWebViewProxyImpl*)contentViewProxyForContainerView:
        (CRWWebControllerContainerView*)containerView {
  return _webViewProxy;
}

- (UIEdgeInsets)nativeContentInsetsForContainerView:
    (CRWWebControllerContainerView*)containerView {
  return [self.nativeProvider nativeContentInsetForWebState:self.webState];
}

- (BOOL)shouldKeepRenderProcessAliveForContainerView:
    (CRWWebControllerContainerView*)containerView {
  return self.shouldKeepRenderProcessAlive;
}

- (void)containerView:(CRWWebControllerContainerView*)containerView
    storeWebViewInWindow:(UIView*)viewToStash {
  [web::GetWebClient()->GetWindowedContainer() addSubview:viewToStash];
}

#pragma mark - JavaScript message Helpers (Private)

- (BOOL)respondToMessage:(base::DictionaryValue*)message
       userIsInteracting:(BOOL)userIsInteracting
               originURL:(const GURL&)originURL
             isMainFrame:(BOOL)isMainFrame
             senderFrame:(web::WebFrame*)senderFrame {
  std::string command;
  if (!message->GetString("command", &command)) {
    DLOG(WARNING) << "JS message parameter not found: command";
    return NO;
  }

  SEL handler = [self selectorToHandleJavaScriptCommand:command];
  if (!handler) {
    if (self.webStateImpl->OnScriptCommandReceived(command, *message, originURL,
                                                   userIsInteracting,
                                                   isMainFrame, senderFrame)) {
      return YES;
    }
    // Message was either unexpected or not correctly handled.
    // Page is reset as a precaution.
    DLOG(WARNING) << "Unexpected message received: " << command;
    return NO;
  }

  typedef BOOL (*HandlerType)(id, SEL, base::DictionaryValue*, NSDictionary*);
  HandlerType handlerImplementation =
      reinterpret_cast<HandlerType>([self methodForSelector:handler]);
  DCHECK(handlerImplementation);
  NSMutableDictionary* context =
      [NSMutableDictionary dictionaryWithObject:@(userIsInteracting)
                                         forKey:kUserIsInteractingKey];
  NSURL* originNSURL = net::NSURLWithGURL(originURL);
  if (originNSURL)
    context[kOriginURLKey] = originNSURL;
  context[kIsMainFrame] = @(isMainFrame);
  return handlerImplementation(self, handler, message, context);
}

- (SEL)selectorToHandleJavaScriptCommand:(const std::string&)command {
  static std::map<std::string, SEL>* handlers = nullptr;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    handlers = new std::map<std::string, SEL>();
    (*handlers)["chrome.send"] = @selector(handleChromeSendMessage:context:);
    (*handlers)["document.favicons"] =
        @selector(handleDocumentFaviconsMessage:context:);
    (*handlers)["window.error"] = @selector(handleWindowErrorMessage:context:);
    (*handlers)["window.hashchange"] =
        @selector(handleWindowHashChangeMessage:context:);
    (*handlers)["window.history.back"] =
        @selector(handleWindowHistoryBackMessage:context:);
    (*handlers)["window.history.willChangeState"] =
        @selector(handleWindowHistoryWillChangeStateMessage:context:);
    (*handlers)["window.history.didPushState"] =
        @selector(handleWindowHistoryDidPushStateMessage:context:);
    (*handlers)["window.history.didReplaceState"] =
        @selector(handleWindowHistoryDidReplaceStateMessage:context:);
    (*handlers)["window.history.forward"] =
        @selector(handleWindowHistoryForwardMessage:context:);
    (*handlers)["window.history.go"] =
        @selector(handleWindowHistoryGoMessage:context:);
    (*handlers)["restoresession.error"] =
        @selector(handleRestoreSessionErrorMessage:context:);
  });
  DCHECK(handlers);
  auto iter = handlers->find(command);
  return iter != handlers->end() ? iter->second : nullptr;
}

- (void)didReceiveScriptMessage:(WKScriptMessage*)message {
  // Broken out into separate method to catch errors.
  if (![self respondToWKScriptMessage:message]) {
    DLOG(WARNING) << "Message from JS not handled due to invalid format";
  }
}

- (NSString*)scriptByAddingWindowIDCheckForScript:(NSString*)script {
  NSString* kTemplate = @"if (__gCrWeb['windowId'] === '%@') { %@; }";
  return [NSString
      stringWithFormat:kTemplate, [_windowIDJSManager windowID], script];
}

- (BOOL)respondToWKScriptMessage:(WKScriptMessage*)scriptMessage {
  if (![scriptMessage.name isEqualToString:kScriptMessageName]) {
    return NO;
  }

  std::unique_ptr<base::Value> messageAsValue =
      web::ValueResultFromWKResult(scriptMessage.body);
  base::DictionaryValue* message = nullptr;
  if (!messageAsValue || !messageAsValue->GetAsDictionary(&message)) {
    return NO;
  }

  web::WebFrame* senderFrame = nullptr;
  std::string frameID;
  if (message->GetString("crwFrameId", &frameID)) {
    senderFrame = web::GetWebFrameWithId([self webState], frameID);
  }
  // Message must be associated with a current frame.
  if (!senderFrame) {
    return NO;
  }

  base::DictionaryValue* command = nullptr;
  if (!message->GetDictionary("crwCommand", &command)) {
    return NO;
  }
  return [self respondToMessage:command
              userIsInteracting:[self userIsInteracting]
                      originURL:net::GURLWithNSURL(self.webView.URL)
                    isMainFrame:scriptMessage.frameInfo.mainFrame
                    senderFrame:senderFrame];
}

- (BOOL)userIsInteracting {
  // If page transfer started after last click, user is deemed to be no longer
  // interacting.
  if (!_lastUserInteraction ||
      _lastTransferTimeInSeconds > _lastUserInteraction->time) {
    return NO;
  }
  return [self userClickedRecently];
}

#pragma mark - Web frames management

- (void)frameBecameAvailableWithMessage:(WKScriptMessage*)message {
  // Validate all expected message components because any frame could falsify
  // this message.
  // TODO(crbug.com/881816): Create a WebFrame even if key is empty.
  if (_isBeingDestroyed || ![message.body isKindOfClass:[NSDictionary class]] ||
      ![message.body[@"crwFrameId"] isKindOfClass:[NSString class]]) {
    // WebController is being destroyed or the message is invalid.
    return;
  }

  std::string frameID = base::SysNSStringToUTF8(message.body[@"crwFrameId"]);
  web::WebFramesManagerImpl* framesManager =
      web::WebFramesManagerImpl::FromWebState([self webState]);
  if (!framesManager->GetFrameWithId(frameID)) {
    GURL messageFrameOrigin =
        web::GURLOriginWithWKSecurityOrigin(message.frameInfo.securityOrigin);

    std::unique_ptr<crypto::SymmetricKey> frameKey;
    if ([message.body[@"crwFrameKey"] isKindOfClass:[NSString class]] &&
        [message.body[@"crwFrameKey"] length] > 0) {
      std::string decodedFrameKeyString;
      std::string encodedFrameKeyString =
          base::SysNSStringToUTF8(message.body[@"crwFrameKey"]);
      base::Base64Decode(encodedFrameKeyString, &decodedFrameKeyString);
      frameKey = crypto::SymmetricKey::Import(
          crypto::SymmetricKey::Algorithm::AES, decodedFrameKeyString);
    }

    auto newFrame = std::make_unique<web::WebFrameImpl>(
        frameID, message.frameInfo.mainFrame, messageFrameOrigin,
        self.webState);
    if (frameKey) {
      newFrame->SetEncryptionKey(std::move(frameKey));
    }

    NSNumber* lastSentMessageID =
        message.body[@"crwFrameLastReceivedMessageId"];
    if ([lastSentMessageID isKindOfClass:[NSNumber class]]) {
      int nextMessageID = std::max(0, lastSentMessageID.intValue + 1);
      newFrame->SetNextMessageId(nextMessageID);
    }

    framesManager->AddFrame(std::move(newFrame));
    self.webStateImpl->OnWebFrameAvailable(
        framesManager->GetFrameWithId(frameID));
  }
}

- (void)frameBecameUnavailableWithMessage:(WKScriptMessage*)message {
  if (_isBeingDestroyed || ![message.body isKindOfClass:[NSString class]]) {
    // WebController is being destroyed or message is invalid.
    return;
  }
  std::string frameID = base::SysNSStringToUTF8(message.body);
  web::WebFramesManagerImpl* framesManager =
      web::WebFramesManagerImpl::FromWebState([self webState]);

  if (framesManager->GetFrameWithId(frameID)) {
    self.webStateImpl->OnWebFrameUnavailable(
        framesManager->GetFrameWithId(frameID));
    framesManager->RemoveFrameWithId(frameID);
  }
}

- (void)removeAllWebFrames {
  web::WebFramesManagerImpl* framesManager =
      web::WebFramesManagerImpl::FromWebState([self webState]);
  for (auto* frame : framesManager->GetAllWebFrames()) {
    self.webStateImpl->OnWebFrameUnavailable(frame);
  }
  framesManager->RemoveAllWebFrames();
}

#pragma mark - JavaScript message handlers
// Handlers for JavaScript messages. |message| contains a JavaScript command and
// data relevant to the message, and |context| contains contextual information
// about web view state needed for some handlers.

// Handles 'chrome.send' message.
- (BOOL)handleChromeSendMessage:(base::DictionaryValue*)message
                        context:(NSDictionary*)context {
  // Chrome message are only handled if sent from the main frame.
  if (![context[kIsMainFrame] boolValue])
    return NO;
  if (self.webStateImpl->HasWebUI()) {
    const GURL currentURL([self currentURL]);
    if (web::GetWebClient()->IsAppSpecificURL(currentURL)) {
      std::string messageContent;
      base::ListValue* arguments = nullptr;
      if (!message->GetString("message", &messageContent)) {
        DLOG(WARNING) << "JS message parameter not found: message";
        return NO;
      }
      if (!message->GetList("arguments", &arguments)) {
        DLOG(WARNING) << "JS message parameter not found: arguments";
        return NO;
      }
      // WebFrame messaging is not supported in WebUI (as window.isSecureContext
      // is false. Pass nullptr as sender_frame.
      self.webStateImpl->OnScriptCommandReceived(
          messageContent, *message, currentURL, context[kUserIsInteractingKey],
          [context[kIsMainFrame] boolValue], nullptr);
      self.webStateImpl->ProcessWebUIMessage(currentURL, messageContent,
                                             *arguments);
      return YES;
    }
  }

  DLOG(WARNING)
      << "chrome.send message not handled because WebUI was not found.";
  return NO;
}

// Handles 'document.favicons' message.
- (BOOL)handleDocumentFaviconsMessage:(base::DictionaryValue*)message
                              context:(NSDictionary*)context {
  if (![context[kIsMainFrame] boolValue])
    return NO;

  std::vector<web::FaviconURL> URLs;
  GURL originGURL;
  id origin = context[kOriginURLKey];
  if (origin) {
    NSURL* originNSURL = base::mac::ObjCCastStrict<NSURL>(origin);
    originGURL = net::GURLWithNSURL(originNSURL);
  }
  if (!web::ExtractFaviconURL(message, originGURL, &URLs))
    return NO;

  if (!URLs.empty())
    self.webStateImpl->OnFaviconUrlUpdated(URLs);
  return YES;
}

// Handles 'window.error' message.
- (BOOL)handleWindowErrorMessage:(base::DictionaryValue*)message
                         context:(NSDictionary*)context {
  std::string errorMessage;
  if (!message->GetString("message", &errorMessage)) {
    DLOG(WARNING) << "JS message parameter not found: message";
    return NO;
  }
  DLOG(ERROR) << "JavaScript error: " << errorMessage
              << " URL:" << [self currentURL].spec();
  return YES;
}

// Handles 'window.hashchange' message.
- (BOOL)handleWindowHashChangeMessage:(base::DictionaryValue*)message
                              context:(NSDictionary*)context {
  if (![context[kIsMainFrame] boolValue])
    return NO;
  // Record that the current NavigationItem was created by a hash change, but
  // ignore hashchange events that are manually dispatched for same-document
  // navigations.
  if (_dispatchingSameDocumentHashChangeEvent) {
    _dispatchingSameDocumentHashChangeEvent = NO;
  } else {
    web::NavigationItemImpl* item = self.currentNavItem;
    DCHECK(item);
    item->SetIsCreatedFromHashChange(true);
  }

  return YES;
}

// Handles 'window.history.back' message.
- (BOOL)handleWindowHistoryBackMessage:(base::DictionaryValue*)message
                               context:(NSDictionary*)context {
  if (![context[kIsMainFrame] boolValue])
    return NO;
  [self rendererInitiatedGoDelta:-1
                  hasUserGesture:[context[kUserIsInteractingKey] boolValue]];
  return YES;
}

// Handles 'window.history.forward' message.
- (BOOL)handleWindowHistoryForwardMessage:(base::DictionaryValue*)message
                                  context:(NSDictionary*)context {
  if (![context[kIsMainFrame] boolValue])
    return NO;
  [self rendererInitiatedGoDelta:1
                  hasUserGesture:[context[kUserIsInteractingKey] boolValue]];
  return YES;
}

// Handles 'window.history.go' message.
- (BOOL)handleWindowHistoryGoMessage:(base::DictionaryValue*)message
                             context:(NSDictionary*)context {
  if (![context[kIsMainFrame] boolValue])
    return NO;
  double delta = 0;
  if (message->GetDouble("value", &delta)) {
    [self rendererInitiatedGoDelta:static_cast<int>(delta)
                    hasUserGesture:[context[kUserIsInteractingKey] boolValue]];
    return YES;
  }
  return NO;
}

// Handles 'window.history.willChangeState' message.
- (BOOL)handleWindowHistoryWillChangeStateMessage:(base::DictionaryValue*)unused
                                          context:(NSDictionary*)context {
  if (![context[kIsMainFrame] boolValue])
    return NO;
  _changingHistoryState = YES;
  return YES;
}

// Handles 'window.history.didPushState' message.
- (BOOL)handleWindowHistoryDidPushStateMessage:(base::DictionaryValue*)message
                                       context:(NSDictionary*)context {
  if (![context[kIsMainFrame] boolValue])
    return NO;
  DCHECK(_changingHistoryState);
  _changingHistoryState = NO;

  // If there is a pending entry, a new navigation has been registered but
  // hasn't begun loading.  Since the pushState message is coming from the
  // previous page, ignore it and allow the previously registered navigation to
  // continue.  This can ocur if a pushState is issued from an anchor tag
  // onClick event, as the click would have already been registered.
  if (self.navigationManagerImpl->GetPendingItem()) {
    return NO;
  }

  std::string pageURL;
  std::string baseURL;
  if (!message->GetString("pageUrl", &pageURL) ||
      !message->GetString("baseUrl", &baseURL)) {
    DLOG(WARNING) << "JS message parameter not found: pageUrl or baseUrl";
    return NO;
  }
  GURL pushURL = web::history_state_util::GetHistoryStateChangeUrl(
      [self currentURL], GURL(baseURL), pageURL);
  // UIWebView seems to choke on unicode characters that haven't been
  // escaped; escape the URL now so the expected load URL is correct.
  pushURL = URLEscapedForHistory(pushURL);
  if (!pushURL.is_valid())
    return YES;
  web::NavigationItem* navItem = self.currentNavItem;
  // PushState happened before first navigation entry or called when the
  // navigation entry does not contain a valid URL.
  if (!navItem || !navItem->GetURL().is_valid())
    return YES;
  if (!web::history_state_util::IsHistoryStateChangeValid(
          self.currentNavItem->GetURL(), pushURL)) {
    // If the current session entry URL origin still doesn't match pushURL's
    // origin, ignore the pushState. This can happen if a new URL is loaded
    // just before the pushState.
    return YES;
  }
  std::string stateObjectJSON;
  if (!message->GetString("stateObject", &stateObjectJSON)) {
    DLOG(WARNING) << "JS message parameter not found: stateObject";
    return NO;
  }
  NSString* stateObject = base::SysUTF8ToNSString(stateObjectJSON);

  // If the user interacted with the page, categorize it as a link navigation.
  // If not, categorize it is a client redirect as it occurred without user
  // input and should not be added to the history stack.
  // TODO(crbug.com/549301): Improve transition detection.
  ui::PageTransition transition = self.userInteractionRegistered
                                      ? ui::PAGE_TRANSITION_LINK
                                      : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  [self pushStateWithPageURL:pushURL
                 stateObject:stateObject
                  transition:transition
              hasUserGesture:[context[kUserIsInteractingKey] boolValue]];
  [self updateSSLStatusForCurrentNavigationItem];

  // This is needed for some special pushState. See http://crbug.com/949305 .
  NSString* replaceWebViewJS = [self javaScriptToReplaceWebViewURL:pushURL
                                                   stateObjectJSON:stateObject];
  __weak CRWWebController* weakSelf = self;
  [self executeJavaScript:replaceWebViewJS
        completionHandler:^(id, NSError*) {
          CRWWebController* strongSelf = weakSelf;
          if (strongSelf && !strongSelf->_isBeingDestroyed) {
            [strongSelf optOutScrollsToTopForSubviews];
            [strongSelf didFinishNavigation:nullptr];
          }
        }];
  return YES;
}

// Handles 'window.history.didReplaceState' message.
- (BOOL)handleWindowHistoryDidReplaceStateMessage:
    (base::DictionaryValue*)message
                                          context:(NSDictionary*)context {
  if (![context[kIsMainFrame] boolValue])
    return NO;
  DCHECK(_changingHistoryState);
  _changingHistoryState = NO;

  std::string pageURL;
  std::string baseURL;
  if (!message->GetString("pageUrl", &pageURL) ||
      !message->GetString("baseUrl", &baseURL)) {
    DLOG(WARNING) << "JS message parameter not found: pageUrl or baseUrl";
    return NO;
  }
  GURL replaceURL = web::history_state_util::GetHistoryStateChangeUrl(
      [self currentURL], GURL(baseURL), pageURL);
  // UIWebView seems to choke on unicode characters that haven't been
  // escaped; escape the URL now so the expected load URL is correct.
  replaceURL = URLEscapedForHistory(replaceURL);
  if (!replaceURL.is_valid())
    return YES;

  web::NavigationItem* navItem = self.currentNavItem;
  // ReplaceState happened before first navigation entry or called right
  // after window.open when the url is empty/not valid.
  if (!navItem || (self.navigationManagerImpl->GetItemCount() <= 1 &&
                   navItem->GetURL().is_empty()))
    return YES;
  if (!web::history_state_util::IsHistoryStateChangeValid(
          self.currentNavItem->GetURL(), replaceURL)) {
    // If the current session entry URL origin still doesn't match
    // replaceURL's origin, ignore the replaceState. This can happen if a
    // new URL is loaded just before the replaceState.
    return YES;
  }
  std::string stateObjectJSON;
  if (!message->GetString("stateObject", &stateObjectJSON)) {
    DLOG(WARNING) << "JS message parameter not found: stateObject";
    return NO;
  }
  NSString* stateObject = base::SysUTF8ToNSString(stateObjectJSON);
  [self replaceStateWithPageURL:replaceURL
                    stateObject:stateObject
                 hasUserGesture:[context[kUserIsInteractingKey] boolValue]];
  NSString* replaceStateJS = [self javaScriptToReplaceWebViewURL:replaceURL
                                                 stateObjectJSON:stateObject];
  __weak CRWWebController* weakSelf = self;
  [self executeJavaScript:replaceStateJS
        completionHandler:^(id, NSError*) {
          CRWWebController* strongSelf = weakSelf;
          if (!strongSelf || strongSelf->_isBeingDestroyed)
            return;
          [strongSelf didFinishNavigation:nullptr];
        }];
  return YES;
}

// Handles 'restoresession.error' message.
- (BOOL)handleRestoreSessionErrorMessage:(base::DictionaryValue*)message
                                 context:(NSDictionary*)context {
  if (![context[kIsMainFrame] boolValue])
    return NO;
  std::string errorMessage;
  if (!message->GetString("message", &errorMessage)) {
    DLOG(WARNING) << "JS message parameter not found: message";
    return NO;
  }

  // Restore session error is likely a result of coding error. Log diagnostics
  // information that is sent back by the page to aid debugging.
  NOTREACHED()
      << "Session restore failed unexpectedly with error: " << errorMessage
      << ". Web view URL: "
      << (self.webView
              ? net::GURLWithNSURL(self.webView.URL).possibly_invalid_spec()
              : " N/A");
  return YES;
}

#pragma mark - Navigation Helpers

// Adds a new NavigationItem with the given URL and state object to the history
// stack. A state object is a serialized generic JavaScript object that contains
// details of the UI's state for a given NavigationItem/URL.
// TODO(stuartmorgan): Move the pushState/replaceState logic into
// NavigationManager.
- (void)pushStateWithPageURL:(const GURL&)pageURL
                 stateObject:(NSString*)stateObject
                  transition:(ui::PageTransition)transition
              hasUserGesture:(BOOL)hasUserGesture {
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webStateImpl, pageURL, hasUserGesture, transition,
          /*is_renderer_initiated=*/true);
  context->SetIsSameDocument(true);
  self.webStateImpl->OnNavigationStarted(context.get());
  self.navigationManagerImpl->AddPushStateItemIfNecessary(pageURL, stateObject,
                                                          transition);
  context->SetHasCommitted(true);
  self.webStateImpl->OnNavigationFinished(context.get());
  self.userInteractionRegistered = NO;
}

// Assigns the given URL and state object to the current NavigationItem.
- (void)replaceStateWithPageURL:(const GURL&)pageURL
                    stateObject:(NSString*)stateObject
                 hasUserGesture:(BOOL)hasUserGesture {
  std::unique_ptr<web::NavigationContextImpl> context =
      web::NavigationContextImpl::CreateNavigationContext(
          self.webStateImpl, pageURL, hasUserGesture,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*is_renderer_initiated=*/true);
  context->SetIsSameDocument(true);
  self.webStateImpl->OnNavigationStarted(context.get());
  self.navigationManagerImpl->UpdateCurrentItemForReplaceState(pageURL,
                                                               stateObject);
  context->SetHasCommitted(true);
  self.webStateImpl->OnNavigationFinished(context.get());
}

// Navigates forwards or backwards by |delta| pages. No-op if delta is out of
// bounds. Reloads if delta is 0.
// TODO(crbug.com/661316): Move this method to NavigationManager.
- (void)rendererInitiatedGoDelta:(int)delta
                  hasUserGesture:(BOOL)hasUserGesture {
  if (_isBeingDestroyed)
    return;

  if (delta == 0) {
    [self reloadWithRendererInitiatedNavigation:YES];
    return;
  }

  if (self.navigationManagerImpl->CanGoToOffset(delta)) {
    int index = self.navigationManagerImpl->GetIndexForOffset(delta);
    self.navigationManagerImpl->GoToIndex(
        index, web::NavigationInitiationType::RENDERER_INITIATED,
        /*has_user_gesture=*/hasUserGesture);
  }
}

#pragma mark - WebUI

- (void)createWebUIForURL:(const GURL&)URL {
  // |CreateWebUI| will do nothing if |URL| is not a WebUI URL and then
  // |HasWebUI| will return false.
  self.webStateImpl->CreateWebUI(URL);
  bool HasWebUIIOSControllerForURL = self.webStateImpl->HasWebUI();
  if (HasWebUIIOSControllerForURL &&
      !web::features::WebUISchemeHandlingEnabled()) {
    _webUIManager =
        [[CRWWebUIManager alloc] initWithWebState:self.webStateImpl];
  }
}

- (void)clearWebUI {
  self.webStateImpl->ClearWebUI();
  _webUIManager = nil;
}

#pragma mark - Auth Challenge

- (void)processAuthChallenge:(NSURLAuthenticationChallenge*)challenge
         forCertAcceptPolicy:(web::CertAcceptPolicy)policy
                  certStatus:(net::CertStatus)certStatus
           completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                       NSURLCredential*))completionHandler {
  SecTrustRef trust = challenge.protectionSpace.serverTrust;
  if (policy == web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_ACCEPTED_BY_USER) {
    // Cert is invalid, but user agreed to proceed, override default behavior.
    completionHandler(NSURLSessionAuthChallengeUseCredential,
                      [NSURLCredential credentialForTrust:trust]);
    return;
  }

  if (policy != web::CERT_ACCEPT_POLICY_ALLOW &&
      SecTrustGetCertificateCount(trust)) {
    // The cert is invalid and the user has not agreed to proceed. Cache the
    // cert verification result in |_certVerificationErrors|, so that it can
    // later be reused inside |didFailProvisionalNavigation:|.
    // The leaf cert is used as the key, because the chain provided by
    // |didFailProvisionalNavigation:| will differ (it is the server-supplied
    // chain), thus if intermediates were considered, the keys would mismatch.
    scoped_refptr<net::X509Certificate> leafCert =
        net::x509_util::CreateX509CertificateFromSecCertificate(
            SecTrustGetCertificateAtIndex(trust, 0),
            std::vector<SecCertificateRef>());
    if (leafCert) {
      BOOL is_recoverable =
          policy == web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_UNDECIDED_BY_USER;
      std::string host =
          base::SysNSStringToUTF8(challenge.protectionSpace.host);
      _certVerificationErrors->Put(
          web::CertHostPair(leafCert, host),
          CertVerificationError(is_recoverable, certStatus));
    }
  }
  completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
}

- (void)handleHTTPAuthForChallenge:(NSURLAuthenticationChallenge*)challenge
                 completionHandler:
                     (void (^)(NSURLSessionAuthChallengeDisposition,
                               NSURLCredential*))completionHandler {
  NSURLProtectionSpace* space = challenge.protectionSpace;
  DCHECK(
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodHTTPBasic] ||
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodNTLM] ||
      [space.authenticationMethod isEqual:NSURLAuthenticationMethodHTTPDigest]);

  self.webStateImpl->OnAuthRequired(
      space, challenge.proposedCredential,
      base::BindRepeating(^(NSString* user, NSString* password) {
        [CRWWebController processHTTPAuthForUser:user
                                        password:password
                               completionHandler:completionHandler];
      }));
}

+ (void)processHTTPAuthForUser:(NSString*)user
                      password:(NSString*)password
             completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition,
                                         NSURLCredential*))completionHandler {
  DCHECK_EQ(user == nil, password == nil);
  if (!user || !password) {
    // Embedder cancelled authentication.
    completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    return;
  }
  completionHandler(
      NSURLSessionAuthChallengeUseCredential,
      [NSURLCredential
          credentialWithUser:user
                    password:password
                 persistence:NSURLCredentialPersistenceForSession]);
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewDidZoom:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  _pageHasZoomed = YES;

  __weak UIScrollView* weakScrollView = self.webScrollView;
  [self extractViewportTagWithCompletion:^(
            const web::PageViewportState* viewportState) {
    if (!weakScrollView)
      return;
    UIScrollView* scrollView = weakScrollView;
    if (viewportState && !viewportState->viewport_tag_present() &&
        [scrollView minimumZoomScale] == [scrollView maximumZoomScale] &&
        [scrollView zoomScale] > 1.0) {
      UMA_HISTOGRAM_BOOLEAN("Renderer.ViewportZoomBugCount", true);
    }
  }];
}

- (void)webViewScrollViewDidResetContentSize:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  web::NavigationItem* currentItem = self.currentNavItem;
  if (webViewScrollViewProxy.isZooming || _applyingPageState || !currentItem)
    return;
  CGSize contentSize = webViewScrollViewProxy.contentSize;
  if (contentSize.width + 1 < CGRectGetWidth(webViewScrollViewProxy.frame)) {
    // The content area should never be narrower than the frame, but floating
    // point error from non-integer zoom values can cause it to be at most 1
    // pixel narrower. If it's any narrower than that, the renderer incorrectly
    // resized the content area. Resetting the scroll view's zoom scale will
    // force a re-rendering.  rdar://23963992
    _applyingPageState = YES;
    web::PageZoomState zoomState =
        currentItem->GetPageDisplayState().zoom_state();
    if (!zoomState.IsValid())
      zoomState = web::PageZoomState(1.0, 1.0, 1.0);
    [self applyWebViewScrollZoomScaleFromZoomState:zoomState];
    _applyingPageState = NO;
  }
}

// Under WKWebView, JavaScript can execute asynchronously. User can start
// scrolling and calls to window.scrollTo executed during scrolling will be
// treated as "during user interaction" and can cause app to go fullscreen.
// This is a workaround to use this webViewScrollViewIsDragging flag to ignore
// window.scrollTo while user is scrolling. See crbug.com/554257
- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self executeJavaScript:@"__gCrWeb.setWebViewScrollViewIsDragging(true)"
        completionHandler:nil];
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  [self executeJavaScript:@"__gCrWeb.setWebViewScrollViewIsDragging(false)"
        completionHandler:nil];
}

#pragma mark - Page State

- (void)restoreStateFromHistory {
  web::NavigationItem* item = self.currentNavItem;
  if (item)
    self.pageDisplayState = item->GetPageDisplayState();
}

- (void)extractViewportTagWithCompletion:(ViewportStateCompletion)completion {
  DCHECK(completion);
  web::NavigationItem* currentItem = self.currentNavItem;
  if (!currentItem) {
    completion(nullptr);
    return;
  }
  NSString* const kViewportContentQuery =
      @"var viewport = document.querySelector('meta[name=\"viewport\"]');"
       "viewport ? viewport.content : '';";
  __weak CRWWebController* weakSelf = self;
  int itemID = currentItem->GetUniqueID();
  [self executeJavaScript:kViewportContentQuery
        completionHandler:^(id viewportContent, NSError*) {
          web::NavigationItem* item = [weakSelf currentNavItem];
          if (item && item->GetUniqueID() == itemID) {
            web::PageViewportState viewportState(
                base::mac::ObjCCast<NSString>(viewportContent));
            completion(&viewportState);
          } else {
            completion(nullptr);
          }
        }];
}

- (void)orientationDidChange {
  // When rotating, the available zoom scale range may change, zoomScale's
  // percentage into this range should remain constant.  However, there are
  // two known bugs with respect to adjusting the zoomScale on rotation:
  // - WKWebView sometimes erroneously resets the scroll view's zoom scale to
  // an incorrect value ( rdar://20100815 ).
  // - After zooming occurs in a UIWebView that's displaying a page with a hard-
  // coded viewport width, the zoom will not be updated upon rotation
  // ( crbug.com/485055 ).
  if (!self.webView)
    return;
  web::NavigationItem* currentItem = self.currentNavItem;
  if (!currentItem)
    return;
  web::PageDisplayState displayState = currentItem->GetPageDisplayState();
  if (!displayState.IsValid())
    return;
  CGFloat zoomPercentage = (displayState.zoom_state().zoom_scale() -
                            displayState.zoom_state().minimum_zoom_scale()) /
                           displayState.zoom_state().GetMinMaxZoomDifference();
  displayState.zoom_state().set_minimum_zoom_scale(
      self.webScrollView.minimumZoomScale);
  displayState.zoom_state().set_maximum_zoom_scale(
      self.webScrollView.maximumZoomScale);
  displayState.zoom_state().set_zoom_scale(
      displayState.zoom_state().minimum_zoom_scale() +
      zoomPercentage * displayState.zoom_state().GetMinMaxZoomDifference());
  currentItem->SetPageDisplayState(displayState);
  [self applyPageDisplayState:currentItem->GetPageDisplayState()];
}

- (void)applyPageDisplayState:(const web::PageDisplayState&)displayState {
  if (!displayState.IsValid())
    return;
  __weak CRWWebController* weakSelf = self;
  web::PageDisplayState displayStateCopy = displayState;
  [self extractViewportTagWithCompletion:^(
            const web::PageViewportState* viewportState) {
    if (viewportState) {
      [weakSelf applyPageDisplayState:displayStateCopy
                         userScalable:viewportState->user_scalable()];
    }
  }];
}

- (void)applyPageDisplayState:(const web::PageDisplayState&)displayState
                 userScalable:(BOOL)isUserScalable {
  // Early return if |scrollState| doesn't match the current NavigationItem.
  // This can sometimes occur in tests, as navigation occurs programmatically
  // and |-applyPageScrollState:| is asynchronous.
  web::NavigationItem* currentItem = self.currentNavItem;
  if (currentItem && currentItem->GetPageDisplayState() != displayState)
    return;
  DCHECK(displayState.IsValid());
  _applyingPageState = YES;
  if (isUserScalable) {
    [self prepareToApplyWebViewScrollZoomScale];
    [self applyWebViewScrollZoomScaleFromZoomState:displayState.zoom_state()];
    [self finishApplyingWebViewScrollZoomScale];
  }
  [self applyWebViewScrollOffsetFromScrollState:displayState.scroll_state()];
  _applyingPageState = NO;
}

- (void)prepareToApplyWebViewScrollZoomScale {
  id webView = self.webView;
  if (![webView respondsToSelector:@selector(viewForZoomingInScrollView:)]) {
    return;
  }

  UIView* contentView = [webView viewForZoomingInScrollView:self.webScrollView];

  if ([webView respondsToSelector:@selector(scrollViewWillBeginZooming:
                                                              withView:)]) {
    [webView scrollViewWillBeginZooming:self.webScrollView
                               withView:contentView];
  }
}

- (void)finishApplyingWebViewScrollZoomScale {
  id webView = self.webView;
  if ([webView respondsToSelector:@selector
               (scrollViewDidEndZooming:withView:atScale:)] &&
      [webView respondsToSelector:@selector(viewForZoomingInScrollView:)]) {
    // This correctly sets the content's frame in the scroll view to
    // fit the web page and upscales the content so that it isn't
    // blurry.
    UIView* contentView =
        [webView viewForZoomingInScrollView:self.webScrollView];
    [webView scrollViewDidEndZooming:self.webScrollView
                            withView:contentView
                             atScale:self.webScrollView.zoomScale];
  }
}

- (void)applyWebViewScrollZoomScaleFromZoomState:
    (const web::PageZoomState&)zoomState {
  // After rendering a web page, WKWebView keeps the |minimumZoomScale| and
  // |maximumZoomScale| properties of its scroll view constant while adjusting
  // the |zoomScale| property accordingly.  The maximum-scale or minimum-scale
  // meta tags of a page may have changed since the state was recorded, so clamp
  // the zoom scale to the current range if necessary.
  DCHECK(zoomState.IsValid());
  CGFloat zoomScale = zoomState.zoom_scale();
  if (zoomScale < self.webScrollView.minimumZoomScale)
    zoomScale = self.webScrollView.minimumZoomScale;
  if (zoomScale > self.webScrollView.maximumZoomScale)
    zoomScale = self.webScrollView.maximumZoomScale;
  self.webScrollView.zoomScale = zoomScale;
}

- (void)applyWebViewScrollOffsetFromScrollState:
    (const web::PageScrollState&)scrollState {
  DCHECK(scrollState.IsValid());
  CGPoint contentOffset = scrollState.GetEffectiveContentOffsetForContentInset(
      self.webScrollView.contentInset);
  if (_loadPhase == web::PAGE_LOADED) {
    // If the page is loaded, update the scroll immediately.
    self.webScrollView.contentOffset = contentOffset;
  } else {
    // If the page isn't loaded, store the action to update the scroll
    // when the page finishes loading.
    __weak UIScrollView* weakScrollView = self.webScrollView;
    ProceduralBlock action = [^{
      weakScrollView.contentOffset = contentOffset;
    } copy];
    [_pendingLoadCompleteActions addObject:action];
  }
}

#pragma mark - Fullscreen

- (void)optOutScrollsToTopForSubviews {
  NSMutableArray* stack =
      [NSMutableArray arrayWithArray:[self.webScrollView subviews]];
  while (stack.count) {
    UIView* current = [stack lastObject];
    [stack removeLastObject];
    [stack addObjectsFromArray:[current subviews]];
    if ([current isKindOfClass:[UIScrollView class]])
      static_cast<UIScrollView*>(current).scrollsToTop = NO;
  }
}

#pragma mark - WKNavigationDelegate Helpers

- (BOOL)isMainFrameNavigationAction:(WKNavigationAction*)action {
  if (action.targetFrame) {
    return action.targetFrame.mainFrame;
  }
  // According to WKNavigationAction documentation, in the case of a new window
  // navigation, target frame will be nil. In this case check if the
  // |sourceFrame| is the mainFrame.
  return action.sourceFrame.mainFrame;
}

#pragma mark - Security Helpers

- (void)updateSSLStatusForCurrentNavigationItem {
  if (_isBeingDestroyed) {
    return;
  }

  NavigationManagerImpl* navManager = self.navigationManagerImpl;
  web::NavigationItem* currentNavItem = navManager->GetLastCommittedItem();
  if (!currentNavItem) {
    return;
  }

  if (!_SSLStatusUpdater) {
    _SSLStatusUpdater =
        [[CRWSSLStatusUpdater alloc] initWithDataSource:self
                                      navigationManager:navManager];
    [_SSLStatusUpdater setDelegate:self];
  }
  NSString* host = base::SysUTF8ToNSString(_documentURL.host());
  BOOL hasOnlySecureContent = [self.webView hasOnlySecureContent];
  base::ScopedCFTypeRef<SecTrustRef> trust;
  trust.reset([self.webView serverTrust], base::scoped_policy::RETAIN);

  [_SSLStatusUpdater updateSSLStatusForNavigationItem:currentNavItem
                                         withCertHost:host
                                                trust:std::move(trust)
                                 hasOnlySecureContent:hasOnlySecureContent];
}

- (void)handleSSLCertError:(NSError*)error
             forNavigation:(WKNavigation*)navigation {
  CHECK(web::IsWKWebViewSSLCertError(error));

  net::SSLInfo info;
  web::GetSSLInfoFromWKWebViewSSLCertError(error, &info);

  if (!info.cert) {
    // |info.cert| can be null if certChain in NSError is empty or can not be
    // parsed, in this case do not ask delegate if error should be allowed, it
    // should not be.
    [self handleLoadError:error forNavigation:navigation provisionalLoad:YES];
    return;
  }

  // Retrieve verification results from _certVerificationErrors cache to avoid
  // unnecessary recalculations. Verification results are cached for the leaf
  // cert, because the cert chain in |didReceiveAuthenticationChallenge:| is
  // the OS constructed chain, while |chain| is the chain from the server.
  NSArray* chain = error.userInfo[web::kNSErrorPeerCertificateChainKey];
  NSURL* requestURL = error.userInfo[web::kNSErrorFailingURLKey];
  NSString* host = [requestURL host];
  scoped_refptr<net::X509Certificate> leafCert;
  BOOL recoverable = NO;
  if (chain.count && host.length) {
    // The complete cert chain may not be available, so the leaf cert is used
    // as a key to retrieve _certVerificationErrors, as well as for storing the
    // cert decision.
    leafCert = web::CreateCertFromChain(@[ chain.firstObject ]);
    if (leafCert) {
      auto error = _certVerificationErrors->Get(
          {leafCert, base::SysNSStringToUTF8(host)});
      bool cacheHit = error != _certVerificationErrors->end();
      if (cacheHit) {
        recoverable = error->second.is_recoverable;
        info.cert_status = error->second.status;
      }
      UMA_HISTOGRAM_BOOLEAN("WebController.CertVerificationErrorsCacheHit",
                            cacheHit);
    }
  }

  // If the current navigation item is in error state, update the error retry
  // state machine to indicate that SSL interstitial error will be displayed to
  // make sure subsequent back/forward navigation to this item starts with the
  // correct error retry state.
  web::NavigationContextImpl* context =
      [_navigationStates contextForNavigation:navigation];
  if (context) {
    if (web::features::StorePendingItemInContext()) {
      // This NavigationContext will be destroyed, so return pending item
      // ownership to NavigationManager. NavigationContext can only own pending
      // item until the navigation has committed or aborted.
      self.navigationManagerImpl->SetPendingItem(context->ReleaseItem());
    }
    web::NavigationItemImpl* item =
        web::GetItemWithUniqueID(self.navigationManagerImpl, context);
    if (item && item->error_retry_state_machine().state() ==
                    web::ErrorRetryState::kRetryFailedNavigationItem) {
      item->error_retry_state_machine().SetDisplayingWebError();
    }
  }

  // Ask web client if this cert error should be allowed.
  web::GetWebClient()->AllowCertificateError(
      self.webStateImpl, net::MapCertStatusToNetError(info.cert_status), info,
      net::GURLWithNSURL(requestURL), recoverable,
      base::BindRepeating(^(bool proceed) {
        if (proceed) {
          DCHECK(recoverable);
          [_certVerificationController allowCert:leafCert
                                         forHost:host
                                          status:info.cert_status];
          self.webStateImpl->GetSessionCertificatePolicyCacheImpl()
              .RegisterAllowedCertificate(
                  leafCert, base::SysNSStringToUTF8(host), info.cert_status);
          // New navigation is a different navigation from the original one.
          // The new navigation is always browser-initiated and happens when
          // the browser allows to proceed with the load.
          [self loadCurrentURLWithRendererInitiatedNavigation:NO];
        } else {
          // If discarding non-committed items results in a NavigationItem that
          // should be loaded via a native controller, load that URL, as its
          // native controller will need to be recreated.  Note that a
          // successful preload of a page with an certificate error will result
          // in this block executing on a CRWWebController with no
          // NavigationManager.  Additionally, if a page with a certificate
          // error is opened in a new tab, its last committed NavigationItem
          // will be null.
          NavigationManager* navigationManager = self.navigationManagerImpl;
          web::NavigationItem* item =
              navigationManager ? navigationManager->GetLastCommittedItem()
                                : nullptr;
          if (item && [self shouldLoadURLInNativeView:item->GetURL()]) {
            // RendererInitiated flag is meaningless for showing previous native
            // content page. RendererInitiated is used as less previledged.
            [self
                loadCurrentURLInNativeViewWithRendererInitiatedNavigation:YES];
          }
        }
      }));

  [self loadCancelled];
}

#pragma mark - WebView Helpers

// Creates a container view if it's not yet created.
- (void)ensureContainerViewCreated {
  if (_containerView)
    return;

  DCHECK(!_isBeingDestroyed);
  // Create the top-level parent view, which will contain the content (whether
  // native or web). Note, this needs to be created with a non-zero size
  // to allow for (native) subviews with autosize constraints to be correctly
  // processed.
  _containerView =
      [[CRWWebControllerContainerView alloc] initWithDelegate:self];

  // This will be resized later, but matching the final frame will minimize
  // re-rendering. Use the screen size because the application's key window
  // may still be nil.
  _containerView.frame = UIApplication.sharedApplication.keyWindow
                             ? UIApplication.sharedApplication.keyWindow.bounds
                             : UIScreen.mainScreen.bounds;

  DCHECK(!CGRectIsEmpty(_containerView.frame));

  [_containerView addGestureRecognizer:[self touchTrackingRecognizer]];
}

// Creates a web view if it's not yet created.
- (void)ensureWebViewCreated {
  WKWebViewConfiguration* config =
      [self webViewConfigurationProvider].GetWebViewConfiguration();
  [self ensureWebViewCreatedWithConfiguration:config];
}

// Creates a web view with given |config|. No-op if web view is already created.
- (void)ensureWebViewCreatedWithConfiguration:(WKWebViewConfiguration*)config {
  if (!self.webView) {
    [self setWebView:[self webViewWithConfiguration:config]];
    // The following is not called in -setWebView: as the latter used in unit
    // tests with fake web view, which cannot be added to view hierarchy.
    CHECK(_webUsageEnabled) << "Tried to create a web view while suspended!";

    DCHECK(self.webView);

    [self.webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                      UIViewAutoresizingFlexibleHeight];

    // Create a dependency between the |webView| pan gesture and BVC side swipe
    // gestures. Note: This needs to be added before the longPress recognizers
    // below, or the longPress appears to deadlock the remaining recognizers,
    // thereby breaking scroll.
    NSSet* recognizers = [_swipeRecognizerProvider swipeRecognizers];
    for (UISwipeGestureRecognizer* swipeRecognizer in recognizers) {
      [self.webScrollView.panGestureRecognizer
          requireGestureRecognizerToFail:swipeRecognizer];
    }

    web::BrowserState* browserState = self.webStateImpl->GetBrowserState();
    _contextMenuController =
        [[CRWContextMenuController alloc] initWithWebView:self.webView
                                             browserState:browserState
                                                 delegate:self];

    // WKWebViews with invalid or empty frames have exhibited rendering bugs, so
    // resize the view to match the container view upon creation.
    [self.webView setFrame:[_containerView bounds]];
  }

  // If web view is not currently displayed and if the visible NavigationItem
  // should be loaded in this web view, display it immediately.  Otherwise, it
  // will be displayed when the pending load is committed.
  if (![_containerView webViewContentView]) {
    web::NavigationItem* visibleItem =
        self.navigationManagerImpl->GetVisibleItem();
    const GURL& visibleURL =
        visibleItem ? visibleItem->GetURL() : GURL::EmptyGURL();
    if (![self shouldLoadURLInNativeView:visibleURL])
      [self displayWebView];
  }
}

// Returns a new autoreleased web view created with given configuration.
- (WKWebView*)webViewWithConfiguration:(WKWebViewConfiguration*)config {
  // Do not attach the context menu controller immediately as the JavaScript
  // delegate must be specified.
  return web::BuildWKWebView(CGRectZero, config,
                             self.webStateImpl->GetBrowserState(),
                             [self userAgentType]);
}

// Wraps the web view in a CRWWebViewContentView and adds it to the container
// view.
- (void)displayWebView {
  if (!self.webView || [_containerView webViewContentView])
    return;

  CRWWebViewContentView* webViewContentView =
      [[CRWWebViewContentView alloc] initWithWebView:self.webView
                                          scrollView:self.webScrollView];
  [_containerView displayWebViewContentView:webViewContentView];

  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      self.navigationManagerImpl->IsRestoreSessionInProgress() &&
      base::FeatureList::IsEnabled(
          web::features::kDisconnectScrollProxyDuringRestore)) {
    [_containerView disconnectScrollProxy];
  }
}

- (void)removeWebView {
  if (!self.webView)
    return;

  self.webStateImpl->CancelDialogs();
  self.navigationManagerImpl->DetachFromWebView();

  [self abortLoad];
  [self.webView removeFromSuperview];
  [_containerView resetContent];
  [self setWebView:nil];

  if (web::features::StorePendingItemInContext()) {
    // webView:didFailProvisionalNavigation:withError: may never be called after
    // resetting WKWebView, so it is important to clear pending navigations now.
    for (__strong id navigation in [_navigationStates pendingNavigations]) {
      [_navigationStates removeNavigation:navigation];
    }
  }
}

// Called when web view process has been terminated.
- (void)webViewWebProcessDidCrash {
  // On iOS 11 WKWebView does not repaint after crash and reload. Recreating
  // web view fixes the issue. TODO(crbug.com/770914): Remove this workaround
  // once rdar://35063950 is fixed.
  [self removeWebView];

  _webProcessCrashed = YES;
  self.webStateImpl->CancelDialogs();
  self.webStateImpl->OnRenderProcessGone();
}

// Returns the WKWebViewConfigurationProvider associated with the web
// controller's BrowserState.
- (web::WKWebViewConfigurationProvider&)webViewConfigurationProvider {
  web::BrowserState* browserState = self.webStateImpl->GetBrowserState();
  return web::WKWebViewConfigurationProvider::FromBrowserState(browserState);
}

#pragma mark - WKUIDelegate Methods

- (WKWebView*)webView:(WKWebView*)webView
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
               forNavigationAction:(WKNavigationAction*)action
                    windowFeatures:(WKWindowFeatures*)windowFeatures {
  // Do not create windows for non-empty invalid URLs.
  GURL requestURL = net::GURLWithNSURL(action.request.URL);
  if (!requestURL.is_empty() && !requestURL.is_valid()) {
    DLOG(WARNING) << "Unable to open a window with invalid URL: "
                  << requestURL.spec();
    return nil;
  }

  NSString* referrer = [self referrerFromNavigationAction:action];
  GURL openerURL =
      referrer.length ? GURL(base::SysNSStringToUTF8(referrer)) : _documentURL;

  // There is no reliable way to tell if there was a user gesture, so this code
  // checks if user has recently tapped on web view. TODO(crbug.com/809706):
  // Remove the usage of -userIsInteracting when rdar://19989909 is fixed.
  bool initiatedByUser = [self isUserInitiatedAction:action];

  if (UIAccessibilityIsVoiceOverRunning()) {
    // -userIsInteracting returns NO if VoiceOver is On. Inspect action's
    // description, which may contain the information about user gesture for
    // certain link clicks.
    initiatedByUser = initiatedByUser ||
                      web::GetNavigationActionInitiationTypeWithVoiceOverOn(
                          action.description) ==
                          web::NavigationActionInitiationType::kUserInitiated;
  }

  WebState* childWebState = self.webStateImpl->CreateNewWebState(
      requestURL, openerURL, initiatedByUser);
  if (!childWebState)
    return nil;

  CRWWebController* childWebController =
      static_cast<WebStateImpl*>(childWebState)->GetWebController();

  DCHECK(!childWebController || childWebController.hasOpener);

  // WKWebView requires WKUIDelegate to return a child view created with
  // exactly the same |configuration| object (exception is raised if config is
  // different). |configuration| param and config returned by
  // WKWebViewConfigurationProvider are different objects because WKWebView
  // makes a shallow copy of the config inside init, so every WKWebView
  // owns a separate shallow copy of WKWebViewConfiguration.
  [childWebController ensureWebViewCreatedWithConfiguration:configuration];
  return childWebController.webView;
}

- (void)webViewDidClose:(WKWebView*)webView {
  if (self.hasOpener)
    self.webStateImpl->CloseWebState();
}

- (void)webView:(WKWebView*)webView
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                      initiatedByFrame:(WKFrameInfo*)frame
                     completionHandler:(void (^)())completionHandler {
  [self runJavaScriptDialogOfType:web::JAVASCRIPT_DIALOG_TYPE_ALERT
                 initiatedByFrame:frame
                          message:message
                      defaultText:nil
                       completion:^(BOOL, NSString*) {
                         completionHandler();
                       }];
}

- (void)webView:(WKWebView*)webView
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                        initiatedByFrame:(WKFrameInfo*)frame
                       completionHandler:
                           (void (^)(BOOL result))completionHandler {
  [self runJavaScriptDialogOfType:web::JAVASCRIPT_DIALOG_TYPE_CONFIRM
                 initiatedByFrame:frame
                          message:message
                      defaultText:nil
                       completion:^(BOOL success, NSString*) {
                         if (completionHandler) {
                           completionHandler(success);
                         }
                       }];
}

- (void)webView:(WKWebView*)webView
    runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
                              defaultText:(NSString*)defaultText
                         initiatedByFrame:(WKFrameInfo*)frame
                        completionHandler:
                            (void (^)(NSString* result))completionHandler {
  GURL origin(web::GURLOriginWithWKSecurityOrigin(frame.securityOrigin));
  if (web::GetWebClient()->IsAppSpecificURL(origin) &&
      (_webUIManager || web::features::WebUISchemeHandlingEnabled())) {
    std::string mojoResponse =
        self.mojoFacade->HandleMojoMessage(base::SysNSStringToUTF8(prompt));
    completionHandler(base::SysUTF8ToNSString(mojoResponse));
    return;
  }

  [self runJavaScriptDialogOfType:web::JAVASCRIPT_DIALOG_TYPE_PROMPT
                 initiatedByFrame:frame
                          message:prompt
                      defaultText:defaultText
                       completion:^(BOOL, NSString* input) {
                         if (completionHandler) {
                           completionHandler(input);
                         }
                       }];
}

- (BOOL)webView:(WKWebView*)webView
    shouldPreviewElement:(WKPreviewElementInfo*)elementInfo
    API_AVAILABLE(ios(10.0)) {
  return self.webStateImpl->ShouldPreviewLink(
      net::GURLWithNSURL(elementInfo.linkURL));
}

- (UIViewController*)webView:(WKWebView*)webView
    previewingViewControllerForElement:(WKPreviewElementInfo*)elementInfo
                        defaultActions:
                            (NSArray<id<WKPreviewActionItem>>*)previewActions
    API_AVAILABLE(ios(10.0)) {
  // Prevent |_contextMenuController| from intercepting the default behavior for
  // the current on-going touch. Otherwise it would cancel the on-going Peek&Pop
  // action and show its own context menu instead (crbug.com/770619).
  [_contextMenuController allowSystemUIForCurrentGesture];

  return self.webStateImpl->GetPreviewingViewController(
      net::GURLWithNSURL(elementInfo.linkURL));
}

- (void)webView:(WKWebView*)webView
    commitPreviewingViewController:(UIViewController*)previewingViewController {
  return self.webStateImpl->CommitPreviewingViewController(
      previewingViewController);
}

#pragma mark - WKUIDelegate Helpers

- (BOOL)isUserInitiatedAction:(WKNavigationAction*)action {
  return [self userIsInteracting];
}

// Helper to respond to |webView:runJavaScript...| delegate methods.
// |completionHandler| must not be nil.
- (void)runJavaScriptDialogOfType:(web::JavaScriptDialogType)type
                 initiatedByFrame:(WKFrameInfo*)frame
                          message:(NSString*)message
                      defaultText:(NSString*)defaultText
                       completion:(void (^)(BOOL, NSString*))completionHandler {
  DCHECK(completionHandler);

  // JavaScript dialogs should not be presented if there is no information about
  // the requesting page's URL.
  GURL requestURL = net::GURLWithNSURL(frame.request.URL);
  if (!requestURL.is_valid()) {
    completionHandler(NO, nil);
    return;
  }

  self.webStateImpl->RunJavaScriptDialog(
      requestURL, type, message, defaultText,
      base::BindOnce(^(bool success, NSString* input) {
        completionHandler(success, input);
      }));
}

#pragma mark - WKNavigationDelegate Methods

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)action
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  [self didReceiveWebViewNavigationDelegateCallback];

  _webProcessCrashed = NO;
  if (_isBeingDestroyed) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }

  GURL requestURL = net::GURLWithNSURL(action.request.URL);

  // Workaround for a WKWebView bug where the web content loaded using
  // |-loadHTMLString:baseURL| clobbers the next WKBackForwardListItem. It works
  // by detecting back/forward navigation to a clobbered item and replacing the
  // clobberred item and its forward history using a partial session restore in
  // the current web view. There is an unfortunate caveat: if the workaround is
  // triggered in a back navigation to a clobbered item, the restored forward
  // session is inserted after the current item before the back navigation, so
  // it doesn't fully replaces the "bad" history, even though user will be
  // navigated to the expected URL and may not notice the issue until they
  // review the back history by long pressing on "Back" button.
  //
  // TODO(crbug.com/887497): remove this workaround once iOS ships the fix.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      action.targetFrame.mainFrame) {
    GURL webViewURL = net::GURLWithNSURL(webView.URL);
    GURL currentWKItemURL =
        net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
    GURL backItemURL = net::GURLWithNSURL(webView.backForwardList.backItem.URL);
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:webViewURL];
    bool willClobberHistory =
        action.navigationType == WKNavigationTypeBackForward &&
        requestURL == backItemURL && webView.backForwardList.currentItem &&
        requestURL != currentWKItemURL && currentWKItemURL == webViewURL &&
        context &&
        (context->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK);

    UMA_HISTOGRAM_BOOLEAN("IOS.WKWebViewClobberedHistory", willClobberHistory);

    if (willClobberHistory && base::FeatureList::IsEnabled(
                                  web::features::kHistoryClobberWorkaround)) {
      decisionHandler(WKNavigationActionPolicyCancel);
      self.navigationManagerImpl
          ->ApplyWKWebViewForwardHistoryClobberWorkaround();
      return;
    }
  }

  // The page will not be changed until this navigation is committed, so the
  // retrieved state will be pending until |didCommitNavigation| callback.
  [self updatePendingNavigationInfoFromNavigationAction:action];

  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      action.targetFrame.mainFrame &&
      action.navigationType == WKNavigationTypeBackForward) {
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:requestURL];
    if (context) {
      // Context is null for renderer-initiated navigations.
      int index = web::GetCommittedItemIndexWithUniqueID(
          self.navigationManagerImpl, context->GetNavigationItemUniqueID());
      self.navigationManagerImpl->SetPendingItemIndex(index);
    }
  }

  // If this is a placeholder navigation, pass through.
  if (IsPlaceholderUrl(requestURL)) {
    decisionHandler(WKNavigationActionPolicyAllow);
    return;
  }

  ui::PageTransition transition =
      [self pageTransitionFromNavigationType:action.navigationType];
  BOOL isMainFrameNavigationAction = [self isMainFrameNavigationAction:action];
  if (isMainFrameNavigationAction) {
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:requestURL];
    if (context) {
      DCHECK(!context->IsRendererInitiated() ||
             (context->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK));
      transition = context->GetPageTransition();
      if (context->IsLoadingErrorPage()) {
        // loadHTMLString: navigation which loads error page into WKWebView.
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
      }
    }
  }

  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // WKBasedNavigationManager doesn't use |loadCurrentURL| for reload or back/
    // forward navigation. So this is the first point where a form repost would
    // be detected. Display the confirmation dialog.
    if ([action.request.HTTPMethod isEqual:@"POST"] &&
        (action.navigationType == WKNavigationTypeFormResubmitted)) {
      self.webStateImpl->ShowRepostFormWarningDialog(
          base::BindOnce(^(bool shouldContinue) {
            if (shouldContinue) {
              decisionHandler(WKNavigationActionPolicyAllow);
            } else {
              decisionHandler(WKNavigationActionPolicyCancel);
              if (action.targetFrame.mainFrame) {
                [_pendingNavigationInfo setCancelled:YES];
                self.webStateImpl->SetIsLoading(false);
              }
            }
          }));
      return;
    }
  }

  // Invalid URLs should not be loaded.
  if (!requestURL.is_valid()) {
    decisionHandler(WKNavigationActionPolicyCancel);
    // The HTML5 spec indicates that window.open with an invalid URL should open
    // about:blank.
    BOOL isFirstLoadInOpenedWindow =
        self.webState->HasOpener() &&
        !self.webState->GetNavigationManager()->GetLastCommittedItem();
    BOOL isMainFrame = action.targetFrame.mainFrame;
    if (isFirstLoadInOpenedWindow && isMainFrame) {
      GURL aboutBlankURL(url::kAboutBlankURL);
      NavigationManager::WebLoadParams loadParams(aboutBlankURL);
      loadParams.referrer = [self currentReferrer];
      self.webState->GetNavigationManager()->LoadURLWithParams(loadParams);
    }
    return;
  }

  // First check if the navigation action should be blocked by the controller
  // and make sure to update the controller in the case that the controller
  // can't handle the request URL. Then use the embedders' policyDeciders to
  // either: 1- Handle the URL it self and return false to stop the controller
  // from proceeding with the navigation if needed. or 2- return true to allow
  // the navigation to be proceeded by the web controller.
  BOOL allowLoad = YES;
  if (web::GetWebClient()->IsAppSpecificURL(requestURL)) {
    allowLoad = [self shouldAllowAppSpecificURLNavigationAction:action
                                                     transition:transition];
    if (web::features::WebUISchemeHandlingEnabled() && allowLoad &&
        !self.webStateImpl->HasWebUI()) {
      [self createWebUIForURL:requestURL];
    }
  }

  BOOL webControllerCanShow =
      web::UrlHasWebScheme(requestURL) ||
      web::GetWebClient()->IsAppSpecificURL(requestURL) ||
      requestURL.SchemeIs(url::kFileScheme) ||
      requestURL.SchemeIs(url::kAboutScheme) ||
      requestURL.SchemeIs(url::kBlobScheme);

  if (allowLoad) {
    // If the URL doesn't look like one that can be shown as a web page, it may
    // handled by the embedder. In that case, update the web controller to
    // correctly reflect the current state.
    if (!webControllerCanShow) {
      if (!web::features::StorePendingItemInContext()) {
        if ([self isMainFrameNavigationAction:action]) {
          [self stopLoading];
        }
      }

      // Purge web view if last committed URL is different from the document
      // URL. This can happen if external URL was added to the navigation stack
      // and was loaded using Go Back or Go Forward navigation (in which case
      // document URL will point to the previous page).  If this is the first
      // load for a NavigationManager, there will be no last committed item, so
      // check here.
      // TODO(crbug.com/850760): Check if this code is still needed. The current
      // implementation doesn't put external apps URLs in the history, so they
      // shouldn't be accessable by Go Back or Go Forward navigation.
      web::NavigationItem* lastCommittedItem =
          self.webState->GetNavigationManager()->GetLastCommittedItem();
      if (lastCommittedItem) {
        GURL lastCommittedURL = lastCommittedItem->GetURL();
        if (lastCommittedURL != _documentURL) {
          [self requirePageReconstruction];
          [self setDocumentURL:lastCommittedURL context:nullptr];
        }
      }
    }
  }

  if (allowLoad) {
    BOOL userInteractedWithRequestMainFrame =
        [self userClickedRecently] &&
        net::GURLWithNSURL(action.request.mainDocumentURL) ==
            _lastUserInteraction->main_document_url;
    web::WebStatePolicyDecider::RequestInfo requestInfo(
        transition, isMainFrameNavigationAction,
        userInteractedWithRequestMainFrame);

    allowLoad =
        self.webStateImpl->ShouldAllowRequest(action.request, requestInfo);
    // The WebState may have been closed in the ShouldAllowRequest callback.
    if (_isBeingDestroyed) {
      decisionHandler(WKNavigationActionPolicyCancel);
      return;
    }
  }

  if (!webControllerCanShow && web::features::StorePendingItemInContext()) {
    allowLoad = NO;
  }

  if (allowLoad) {
    if ([[action.request HTTPMethod] isEqualToString:@"POST"]) {
      web::NavigationItemImpl* item = self.currentNavItem;
      // TODO(crbug.com/570699): Remove this check once it's no longer possible
      // to have no current entries.
      if (item)
        [self cachePOSTDataForRequest:action.request inNavigationItem:item];
    }
  } else {
    if (action.targetFrame.mainFrame) {
      [_pendingNavigationInfo setCancelled:YES];
      // Discard the pending item to ensure that the current URL is not
      // different from what is displayed on the view. Discard only happens
      // if the last item was not a native view, to avoid ugly animation of
      // inserting the webview.
      [self discardNonCommittedItemsIfLastCommittedWasNotNativeView];

      web::NavigationContextImpl* context =
          [self contextForPendingMainFrameNavigationWithURL:requestURL];
      if (context) {
        // Destroy associated pending item, because this will be the last
        // WKWebView callback for this navigation context.
        context->ReleaseItem();
      }

      if (!_isBeingDestroyed && [self shouldClosePageOnNativeApplicationLoad]) {
        // Loading was started for user initiated navigations and should be
        // stopped because no other WKWebView callbacks are called.
        // TODO(crbug.com/767092): Loading should not start until
        // webView.loading is changed to YES.
        self.webStateImpl->SetIsLoading(false);

        self.webStateImpl->CloseWebState();
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
      }
    }

    if (!_isBeingDestroyed) {
      // Loading was started for user initiated navigations and should be
      // stopped because no other WKWebView callbacks are called.
      // TODO(crbug.com/767092): Loading should not start until webView.loading
      // is changed to YES.
      self.webStateImpl->SetIsLoading(false);
    }
  }

  // Only try to detect a SafeBrowsing warning if one isn't already displayed,
  // since the detection logic won't be able to distinguish between the current
  // warning and a warning for the page that's about to be loaded. Also, since
  // the purpose of running this logic is to ensure that the right URL is
  // displayed in the omnibox, don't try to detect a SafeBrowsing warning for
  // iframe navigations, because the omnibox already shows the correct main
  // frame URL in that case.
  if (allowLoad && isMainFrameNavigationAction &&
      ![self isSafeBrowsingWarningDisplayedInWebView]) {
    __weak CRWWebController* weakSelf = self;
    const base::TimeDelta kDelayUntilSafeBrowsingWarningCheck =
        base::TimeDelta::FromMilliseconds(20);
    _safeBrowsingWarningDetectionTimer.Start(
        FROM_HERE, kDelayUntilSafeBrowsingWarningCheck, base::BindRepeating(^{
          __strong __typeof(weakSelf) strongSelf = weakSelf;
          if ([strongSelf isSafeBrowsingWarningDisplayedInWebView]) {
            // Extract state from an existing navigation context if one exists.
            // Create a new context rather than just re-using the existing one,
            // since the existing context will continue to be used if the user
            // decides to proceed to the unsafe page. In that case, WebKit
            // continues the navigation with the same WKNavigation* that's
            // associated with the existing context.
            web::NavigationContextImpl* existingContext = [strongSelf
                contextForPendingMainFrameNavigationWithURL:requestURL];
            bool hasUserGesture =
                existingContext ? existingContext->HasUserGesture() : false;
            bool isRendererInitiated =
                existingContext ? existingContext->IsRendererInitiated() : true;
            std::unique_ptr<web::NavigationContextImpl> context =
                web::NavigationContextImpl::CreateNavigationContext(
                    strongSelf->_webStateImpl, requestURL, hasUserGesture,
                    transition, isRendererInitiated);
            strongSelf.navigationManagerImpl->AddTransientItem(requestURL);
            strongSelf.webStateImpl->OnNavigationStarted(context.get());
            strongSelf.webStateImpl->OnNavigationFinished(context.get());
            strongSelf->_safeBrowsingWarningDetectionTimer.Stop();
            if (!existingContext) {
              // If there's an existing context, observers will already be aware
              // of a load in progress. Otherwise, observers need to be notified
              // here, so that if the user decides to go back to the previous
              // page (stopping the load), observers will be aware of a possible
              // URL change and the URL displayed in the omnibox will get
              // updated.
              DCHECK(strongSelf->_webView.loading);
              strongSelf->_webStateImpl->SetIsLoading(true);
            }
          }
        }));
  }

  if (!allowLoad) {
    decisionHandler(WKNavigationActionPolicyCancel);
    return;
  }
  BOOL isOffTheRecord = self.webState->GetBrowserState()->IsOffTheRecord();
  decisionHandler(web::GetAllowNavigationActionPolicy(isOffTheRecord));
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse*)WKResponse
                      decisionHandler:
                          (void (^)(WKNavigationResponsePolicy))handler {
  [self didReceiveWebViewNavigationDelegateCallback];

  // If this is a placeholder navigation, pass through.
  GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  if (IsPlaceholderUrl(responseURL)) {
    handler(WKNavigationResponsePolicyAllow);
    return;
  }

  scoped_refptr<net::HttpResponseHeaders> headers;
  if ([WKResponse.response isKindOfClass:[NSHTTPURLResponse class]]) {
    headers = net::CreateHeadersFromNSHTTPURLResponse(
        static_cast<NSHTTPURLResponse*>(WKResponse.response));
    // TODO(crbug.com/551677): remove |OnHttpResponseHeadersReceived| and attach
    // headers to web::NavigationContext.
    self.webStateImpl->OnHttpResponseHeadersReceived(headers.get(),
                                                     responseURL);
  }

  // The page will not be changed until this navigation is committed, so the
  // retrieved state will be pending until |didCommitNavigation| callback.
  [self updatePendingNavigationInfoFromNavigationResponse:WKResponse];

  BOOL shouldRenderResponse = [self shouldRenderResponse:WKResponse];
  if (!shouldRenderResponse) {
    if (web::UrlHasWebScheme(responseURL)) {
      [self createDownloadTaskForResponse:WKResponse HTTPHeaders:headers.get()];
    } else {
      // DownloadTask only supports web schemes, so do nothing.
    }
    // Discard the pending item to ensure that the current URL is not different
    // from what is displayed on the view.
    [self discardNonCommittedItemsIfLastCommittedWasNotNativeView];
    if (!web::features::StorePendingItemInContext()) {
      // Loading will be stopped in webView:didFinishNavigation: callback. This
      // call is here to preserve the original behavior when pending item is not
      // stored in NavigationContext.
      self.webStateImpl->SetIsLoading(false);
    }
  } else {
    shouldRenderResponse = self.webStateImpl->ShouldAllowResponse(
        WKResponse.response, WKResponse.forMainFrame);
  }

  if (!shouldRenderResponse && WKResponse.canShowMIMEType &&
      WKResponse.forMainFrame) {
    [_pendingNavigationInfo setCancelled:YES];
  }

  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      !WKResponse.forMainFrame && !self.webView.loading) {
    // This is the terminal callback for iframe navigation and there is no
    // pending main frame navigation. Last chance to flip IsLoading to false.
    self.webStateImpl->SetIsLoading(false);
  }

  handler(shouldRenderResponse ? WKNavigationResponsePolicyAllow
                               : WKNavigationResponsePolicyCancel);
}

- (void)webView:(WKWebView*)webView
    didStartProvisionalNavigation:(WKNavigation*)navigation {
  [self didReceiveWebViewNavigationDelegateCallback];

  GURL webViewURL = net::GURLWithNSURL(webView.URL);

  [_navigationStates setState:web::WKNavigationState::STARTED
                forNavigation:navigation];

  if (webViewURL.is_empty()) {
    // May happen on iOS9, however in didCommitNavigation: callback the URL
    // will be "about:blank".
    webViewURL = GURL(url::kAboutBlankURL);
  }

  web::NavigationContextImpl* context =
      [_navigationStates contextForNavigation:navigation];

  if (context) {
    // This is already seen and registered navigation.

    if (context->IsLoadingErrorPage()) {
      // This is loadHTMLString: navigation to display error page in web view.
      _loadPhase = web::LOAD_REQUESTED;
      return;
    }

    if (!context->IsPlaceholderNavigation() &&
        context->GetUrl() != webViewURL) {
      // Update last seen URL because it may be changed by WKWebView (f.e. by
      // performing characters escaping).
      web::NavigationItem* item =
          web::GetItemWithUniqueID(self.navigationManagerImpl, context);
      if (!web::wk_navigation_util::IsWKInternalUrl(webViewURL)) {
        if (item) {
          item->SetURL(webViewURL);
        }
        context->SetUrl(webViewURL);
      }
    }
    self.webStateImpl->OnNavigationStarted(context);
    return;
  }

  // This is renderer-initiated navigation which was not seen before and
  // should be registered.

  // When using WKBasedNavigationManager, renderer-initiated app-specific loads
  // should be allowed in two specific cases:
  // 1) if |backForwardList.currentItem| is a placeholder URL for the
  //    provisional load URL (i.e. webView.URL), then this is an in-progress
  //    app-specific load and should not be restarted.
  // 2) back/forward navigation to an app-specific URL should be allowed.
  bool exemptedAppSpecificLoad = false;
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    bool currentItemIsPlaceholder =
        CreatePlaceholderUrlForUrl(webViewURL) ==
        net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
    bool isBackForward =
        _pendingNavigationInfo.navigationType == WKNavigationTypeBackForward;
    bool isRestoringSession =
        web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
        IsRestoreSessionUrl(_documentURL);
    exemptedAppSpecificLoad =
        currentItemIsPlaceholder || isBackForward || isRestoringSession;
  }

  if (!web::GetWebClient()->IsAppSpecificURL(webViewURL) ||
      !exemptedAppSpecificLoad) {
    [self clearWebUI];
  }

  if (web::GetWebClient()->IsAppSpecificURL(webViewURL) &&
      !exemptedAppSpecificLoad) {
    // Restart app specific URL loads to properly capture state.
    // TODO(crbug.com/546347): Extract necessary tasks for app specific URL
    // navigation rather than restarting the load.

    // Renderer-initiated loads of WebUI can be done only from other WebUI
    // pages. WebUI pages may have increased power and using the same web
    // process (which may potentially be controller by an attacker) is
    // dangerous.
    if (web::GetWebClient()->IsAppSpecificURL(_documentURL)) {
      [self abortLoad];
      NavigationManager::WebLoadParams params(webViewURL);
      self.webState->GetNavigationManager()->LoadURLWithParams(params);
    }
    return;
  }

  self.webStateImpl->GetNavigationManagerImpl()
      .OnRendererInitiatedNavigationStarted(webViewURL);

  // When a client-side redirect occurs while an interstitial warning is
  // displayed, clear the warning and its navigation item, so that a new
  // pending item is created for |context| in |registerLoadRequestForURL|. See
  // crbug.com/861836.
  self.webStateImpl->ClearTransientContent();

  std::unique_ptr<web::NavigationContextImpl> navigationContext =
      [self registerLoadRequestForURL:webViewURL
               sameDocumentNavigation:NO
                       hasUserGesture:[_pendingNavigationInfo hasUserGesture]
                    rendererInitiated:YES
                placeholderNavigation:IsPlaceholderUrl(webViewURL)];
  web::NavigationContextImpl* navigationContextPtr = navigationContext.get();
  // GetPendingItem which may be called inside OnNavigationStarted relies on
  // association between NavigationContextImpl and WKNavigation.
  [_navigationStates setContext:std::move(navigationContext)
                  forNavigation:navigation];
  self.webStateImpl->OnNavigationStarted(navigationContextPtr);
  DCHECK(self.loadPhase == web::LOAD_REQUESTED);
}

- (void)webView:(WKWebView*)webView
    didReceiveServerRedirectForProvisionalNavigation:(WKNavigation*)navigation {
  [self didReceiveWebViewNavigationDelegateCallback];

  GURL webViewURL = net::GURLWithNSURL(webView.URL);

  // This callback should never be triggered for placeholder navigations.
  DCHECK(!(web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
           IsPlaceholderUrl(webViewURL)));

  [_navigationStates setState:web::WKNavigationState::REDIRECTED
                forNavigation:navigation];

  web::NavigationContextImpl* context =
      [_navigationStates contextForNavigation:navigation];
  [self didReceiveRedirectForNavigation:context withURL:webViewURL];
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  [self didReceiveWebViewNavigationDelegateCallback];
  [_navigationStates setState:web::WKNavigationState::PROVISIONALY_FAILED
                forNavigation:navigation];

  // Ignore provisional navigation failure if a new navigation has been started,
  // for example, if a page is reloaded after the start of the provisional
  // load but before the load has been committed.
  if (![[_navigationStates lastAddedNavigation] isEqual:navigation]) {
    return;
  }

  // TODO(crbug.com/570699): Remove this workaround once |stopLoading| does not
  // discard pending navigation items.
  if ((!self.webStateImpl ||
       !self.webStateImpl->GetNavigationManagerImpl().GetVisibleItem()) &&
      _stoppedWKNavigation &&
      [error.domain isEqual:base::SysUTF8ToNSString(web::kWebKitErrorDomain)] &&
      error.code == web::kWebKitErrorFrameLoadInterruptedByPolicyChange) {
    // App is going to crash in this state (crbug.com/565457). Crash will occur
    // on dereferencing visible navigation item, which is null. This scenario is
    // possible after pending load was stopped for a child window. Early return
    // to prevent the crash and report UMA metric to check if crash happening
    // because the load was stopped.
    UMA_HISTOGRAM_BOOLEAN(
        "WebController.EmptyNavigationManagerCausedByStopLoading",
        [_stoppedWKNavigation isEqual:navigation]);
    return;
  }

  // Handle load cancellation for directly cancelled navigations without
  // handling their potential errors. Otherwise, handle the error.
  if ([_pendingNavigationInfo cancelled]) {
    [self handleCancelledError:error
                 forNavigation:navigation
               provisionalLoad:YES];
  } else if (error.code == NSURLErrorUnsupportedURL &&
             self.webStateImpl->HasWebUI()) {
    // This is a navigation to WebUI page.
    DCHECK(web::GetWebClient()->IsAppSpecificURL(
        net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey])));
  } else {
    if (web::IsWKWebViewSSLCertError(error)) {
      [self handleSSLCertError:error forNavigation:navigation];
    } else {
      [self handleLoadError:error forNavigation:navigation provisionalLoad:YES];
    }
  }

  [self removeAllWebFrames];
  // This must be reset at the end, since code above may need information about
  // the pending load.
  _pendingNavigationInfo = nil;
  _certVerificationErrors->Clear();
  if (web::features::StorePendingItemInContext()) {
    // Remove the navigation to immediately get rid of pending item.
    if (web::WKNavigationState::NONE !=
        [_navigationStates stateForNavigation:navigation]) {
      [_navigationStates removeNavigation:navigation];
    }
  } else {
    [self forgetNullWKNavigation:navigation];
  }
}

- (void)webView:(WKWebView*)webView
    didCommitNavigation:(WKNavigation*)navigation {
  [self didReceiveWebViewNavigationDelegateCallback];

  // For reasons not yet fully understood, sometimes WKWebView triggers
  // |webView:didFinishNavigation| before |webView:didCommitNavigation|. If a
  // navigation is already finished, stop processing
  // (https://crbug.com/818796#c2).
  if ([_navigationStates stateForNavigation:navigation] ==
      web::WKNavigationState::FINISHED)
    return;

  BOOL committedNavigation =
      [_navigationStates isCommittedNavigation:navigation];
  if (!web::features::StorePendingItemInContext()) {
    // Code in this method relies on existance of pending item.
    [_navigationStates setState:web::WKNavigationState::COMMITTED
                  forNavigation:navigation];
  }

  DCHECK_EQ(self.webView, webView);
  _certVerificationErrors->Clear();

  // Invariant: Every |navigation| should have a |context|. Note that violation
  // of this invariant is currently observed in production, but the cause is not
  // well understood. This DCHECK is meant to catch such cases in testing if
  // they arise.
  // TODO(crbug.com/864769): Remove nullptr checks on |context| in this method
  // once the root cause of the invariant violation is found.
  web::NavigationContextImpl* context =
      [_navigationStates contextForNavigation:navigation];
  DCHECK(context);
  UMA_HISTOGRAM_BOOLEAN("IOS.CommittedNavigationHasContext", context);

  GURL webViewURL = net::GURLWithNSURL(webView.URL);
  GURL currentWKItemURL =
      net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
  UMA_HISTOGRAM_BOOLEAN("IOS.CommittedURLMatchesCurrentItem",
                        webViewURL == currentWKItemURL);

  // TODO(crbug.com/787497): Always use webView.backForwardList.currentItem.URL
  // to obtain lastCommittedURL once loadHTML: is no longer user for WebUI.
  if (webViewURL.is_empty()) {
    // It is possible for |webView.URL| to be nil, in which case
    // webView.backForwardList.currentItem.URL will return the right committed
    // URL (crbug.com/784480).
    webViewURL = currentWKItemURL;
  } else if (context && !context->IsPlaceholderNavigation() &&
             context->GetUrl() == currentWKItemURL) {
    // If webView.backForwardList.currentItem.URL matches |context|, then this
    // is a known edge case where |webView.URL| is wrong.
    // TODO(crbug.com/826013): Remove this workaround.
    webViewURL = currentWKItemURL;
  }

  if (_pendingNavigationInfo.MIMEType)
    context->SetMimeType(_pendingNavigationInfo.MIMEType);

  // Don't show webview for placeholder navigation to avoid covering the native
  // content, which may have already been shown.
  if (!IsPlaceholderUrl(webViewURL))
    [self displayWebView];

  // Update HTTP response headers.
  self.webStateImpl->UpdateHttpResponseHeaders(webViewURL);

  if (@available(iOS 11.3, *)) {
    // On iOS 11.3 didReceiveServerRedirectForProvisionalNavigation: is not
    // always called. So if URL was unexpectedly changed then it's probably
    // because redirect callback was not called.
    if (@available(iOS 12, *)) {
      // rdar://37547029 was fixed on iOS 12.
    } else if (context && !context->IsPlaceholderNavigation() &&
               context->GetUrl() != webViewURL) {
      [self didReceiveRedirectForNavigation:context withURL:webViewURL];
    }
  }

  // |context| will be nil if this navigation has been already committed and
  // finished.
  if (context) {
    web::NavigationManager* navigationManager =
        self.webStateImpl->GetNavigationManager();
    GURL pendingURL;
    if (web::features::StorePendingItemInContext() &&
        navigationManager->GetPendingItemIndex() == -1) {
      if (context->GetItem()) {
        pendingURL = context->GetItem()->GetURL();
      }
    } else {
      if (navigationManager->GetPendingItem()) {
        pendingURL = navigationManager->GetPendingItem()->GetURL();
      }
    }
    if ((pendingURL == webViewURL) || (context->IsLoadingHtmlString()) ||
        (!web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
         ui::PageTransitionCoreTypeIs(context->GetPageTransition(),
                                      ui::PAGE_TRANSITION_RELOAD) &&
         navigationManager->GetLastCommittedItem())) {
      // Commit navigation if at least one of these is true:
      //  - Navigation has pending item (this should always be true, but
      //    pending item may not exist due to crbug.com/925304).
      //  - Navigation is loadHTMLString:baseURL: navigation, which does not
      //    create a pending item, but modifies committed item instead.
      //  - Transition type is reload with Legacy Navigation Manager (Legacy
      //    Navigation Manager does not create pending item for reload due to
      //    crbug.com/676129)
      context->SetHasCommitted(true);
    }
    context->SetResponseHeaders(self.webStateImpl->GetHttpResponseHeaders());
    self.webStateImpl->SetContentsMimeType(
        base::SysNSStringToUTF8(context->GetMimeType()));
  }

  [self commitPendingNavigationInfo];

  [self removeAllWebFrames];

  // This point should closely approximate the document object change, so reset
  // the list of injected scripts to those that are automatically injected.
  // Do not inject window ID if this is a placeholder URL: window ID is not
  // needed for native view. For WebUI, let the window ID be injected when the
  // |loadHTMLString:baseURL| navigation is committed.
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled() ||
      !IsPlaceholderUrl(webViewURL)) {
    _injectedScriptManagers = [[NSMutableSet alloc] init];
    if ([self contentIsHTML] || [self contentIsImage] ||
        self.webState->GetContentsMimeType().empty()) {
      // In unit tests MIME type will be empty, because loadHTML:forURL: does
      // not notify web view delegate about received response, so web controller
      // does not get a chance to properly update MIME type.
      [_windowIDJSManager inject];
      web::WebFramesManagerImpl::FromWebState(self.webState)
          ->RegisterExistingFrames();
    }
  }

  if (committedNavigation) {
    // WKWebView called didCommitNavigation: with incorrect WKNavigation object.
    // Correct WKNavigation object for this navigation was deallocated because
    // WKWebView mistakenly cancelled the navigation and called
    // didFailProvisionalNavigation. As a result web::NavigationContext for this
    // navigation does not exist anymore. Find correct navigation item and make
    // it committed.
    if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
      bool found_correct_navigation_item = false;
      for (size_t i = 0; i < self.sessionController.items.size(); i++) {
        web::NavigationItem* item = self.sessionController.items[i].get();
        found_correct_navigation_item = item->GetURL() == webViewURL;
        if (found_correct_navigation_item) {
          [self.sessionController goToItemAtIndex:i
                         discardNonCommittedItems:NO];
          break;
        }
      }
      DCHECK(found_correct_navigation_item);
    }
    [self resetDocumentSpecificState];
    [self didStartLoading];
  } else if (context) {
    // If |navigation| is nil (which happens for windows open by DOM), then it
    // should be the first and the only pending navigation.
    BOOL isLastNavigation =
        !navigation ||
        [[_navigationStates lastAddedNavigation] isEqual:navigation];
    if (isLastNavigation ||
        (web::features::StorePendingItemInContext() &&
         self.webState->GetNavigationManager()->GetPendingItemIndex() == -1)) {
      [self webPageChangedWithContext:context];
    } else if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
      // WKWebView has more than one in progress navigation, and committed
      // navigation was not the latest. Change last committed item to one that
      // corresponds to committed navigation.
      int itemIndex = web::GetCommittedItemIndexWithUniqueID(
          self.navigationManagerImpl, context->GetNavigationItemUniqueID());
      // Do not discard pending entry, because another pending navigation is
      // still in progress and will commit or fail soon.
      [self.sessionController goToItemAtIndex:itemIndex
                     discardNonCommittedItems:NO];
    }
  }

  if (web::features::StorePendingItemInContext()) {
    // No further code relies an existance of pending item, so this navigation
    // can be marked as "committed".
    [_navigationStates setState:web::WKNavigationState::COMMITTED
                  forNavigation:navigation];
  }

  // This is the point where the document's URL has actually changed.
  [self setDocumentURL:webViewURL context:context];

  if (!committedNavigation && context && !context->IsLoadingErrorPage()) {
    self.webStateImpl->OnNavigationFinished(context);
  }

  // Do not update the HTML5 history state or states of the last committed item
  // for placeholder page because the actual navigation item will not be
  // committed until the native content or WebUI is shown.
  if (context && !context->IsPlaceholderNavigation() &&
      !context->IsLoadingErrorPage() &&
      !context->GetUrl().SchemeIs(url::kAboutScheme) &&
      !IsRestoreSessionUrl(context->GetUrl())) {
    [self updateSSLStatusForCurrentNavigationItem];
    [self updateHTML5HistoryState];
    if (!context->IsLoadingErrorPage() && !IsRestoreSessionUrl(webViewURL)) {
      [self setNavigationItemTitle:self.webView.title];
    }
  }

  // Report cases where SSL cert is missing for a secure connection.
  if (_documentURL.SchemeIsCryptographic()) {
    scoped_refptr<net::X509Certificate> cert;
    cert = web::CreateCertFromTrust(self.webView.serverTrust);
    UMA_HISTOGRAM_BOOLEAN("WebController.WKWebViewHasCertForSecureConnection",
                          static_cast<bool>(cert));
  }
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  [self didReceiveWebViewNavigationDelegateCallback];

  // Sometimes |webView:didFinishNavigation| arrives before
  // |webView:didCommitNavigation|. Explicitly trigger post-commit processing.
  bool navigationCommitted =
      [_navigationStates stateForNavigation:navigation] ==
      web::WKNavigationState::COMMITTED;
  UMA_HISTOGRAM_BOOLEAN("IOS.WKWebViewFinishBeforeCommit",
                        !navigationCommitted);
  if (!navigationCommitted) {
    [self webView:webView didCommitNavigation:navigation];
    DCHECK_EQ(web::WKNavigationState::COMMITTED,
              [_navigationStates stateForNavigation:navigation]);
  }

  // Sometimes |didFinishNavigation| callback arrives after |stopLoading| has
  // been called. Abort in this case.
  if ([_navigationStates stateForNavigation:navigation] ==
      web::WKNavigationState::NONE) {
    return;
  }

  GURL webViewURL = net::GURLWithNSURL(webView.URL);
  GURL currentWKItemURL =
      net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
  UMA_HISTOGRAM_BOOLEAN("IOS.FinishedURLMatchesCurrentItem",
                        webViewURL == currentWKItemURL);

  web::NavigationContextImpl* context =
      [_navigationStates contextForNavigation:navigation];
  web::NavigationItemImpl* item =
      context ? web::GetItemWithUniqueID(self.navigationManagerImpl, context)
              : nullptr;

  // Invariant: every |navigation| should have a |context| and a |item|.
  // TODO(crbug.com/899383) Fix invariant violation when a new pending item is
  // created before a placeholder load finishes.
  if (IsPlaceholderUrl(webViewURL)) {
    GURL originalURL = ExtractUrlFromPlaceholderUrl(webViewURL);
    if (self.currentNavItem != item &&
        self.currentNavItem->GetVirtualURL() != originalURL) {
      // The |didFinishNavigation| callback for placeholder navigation can
      // arrive after another navigation has started. Abort in this case.
      return;
    }
  }
  DCHECK(context);
  DCHECK(item);
  UMA_HISTOGRAM_BOOLEAN("IOS.FinishedNavigationHasContext", context);
  UMA_HISTOGRAM_BOOLEAN("IOS.FinishedNavigationHasItem", item);

  // TODO(crbug.com/864769): Remove this guard after fixing root cause of
  // invariant violation in production.
  if (context && item) {
    GURL navigationURL = context->IsPlaceholderNavigation()
                             ? CreatePlaceholderUrlForUrl(context->GetUrl())
                             : context->GetUrl();
    if (navigationURL == currentWKItemURL) {
      // If webView.backForwardList.currentItem.URL matches |context|, then this
      // is a known edge case where |webView.URL| is wrong.
      // TODO(crbug.com/826013): Remove this workaround.
      webViewURL = currentWKItemURL;
    }

    if (!IsWKInternalUrl(currentWKItemURL) && currentWKItemURL == webViewURL &&
        currentWKItemURL != context->GetUrl() &&
        item == self.navigationManagerImpl->GetLastCommittedItem() &&
        item->GetURL().GetOrigin() == currentWKItemURL.GetOrigin()) {
      // WKWebView sometimes changes URL on the same navigation, likely due to
      // location.replace() or history.replaceState in onload handler that does
      // not change the origin. It's safe to update |item| and |context| URL
      // because they are both associated to WKNavigation*, which is a stable ID
      // for the navigation. See https://crbug.com/869540 for a real-world case.
      item->SetURL(currentWKItemURL);
      context->SetUrl(currentWKItemURL);
    }

    if (IsPlaceholderUrl(webViewURL)) {
      if (item->GetURL() == webViewURL) {
        // Current navigation item is restored from a placeholder URL as part
        // of session restoration. It is now safe to update the navigation
        // item URL to the original app-specific URL.
        item->SetURL(ExtractUrlFromPlaceholderUrl(webViewURL));
      }

      if (web::GetWebClient()->IsAppSpecificURL(item->GetURL()) &&
          ![_nativeProvider hasControllerForURL:item->GetURL()] &&
          !_webUIManager) {
        // WebUIManager is normally created when initiating a new load (in
        // |loadCurrentURL|. If user navigates to a WebUI URL via back/forward
        // navigation, the WebUI manager would have not been created. Attempt
        // to create WebUI now. Not all app-specific URLs are WebUI, so WebUI
        // creation may fail.
        [self createWebUIForURL:item->GetURL()];
      }

      if ([self shouldLoadURLInNativeView:item->GetURL()]) {
        // Native content may have already been presented if this navigation is
        // started in
        // |-loadCurrentURLInNativeViewWithRendererInitiatedNavigation:|. If
        // not, present it now.
        if (!context->IsNativeContentPresented()) {
          [self presentNativeContentForNavigationItem:item];
        }
        bool rendererInitiated = context->IsRendererInitiated();
        [self didLoadNativeContentForNavigationItem:item
                                 placeholderContext:context
                                  rendererInitiated:rendererInitiated];
      } else if (_webUIManager) {
        [_webUIManager loadWebUIForURL:item->GetURL()];
        // Pending item is stored in NavigationManager for WebUI navigations,
        // because WebUIManager does not have access to NavigationContext.
        if (web::features::StorePendingItemInContext() && context->GetItem()) {
          self.navigationManagerImpl->SetPendingItem(context->ReleaseItem());
        }
      } else if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
                 item->error_retry_state_machine().state() ==
                     web::ErrorRetryState::kNoNavigationError) {
        // Offline pages can leave the WKBackForwardList current item as a
        // placeholder with no saved content.  In this case, trigger a retry
        // on that navigation with an update |item| url and |context| error.
        item->SetURL(
            ExtractUrlFromPlaceholderUrl(net::GURLWithNSURL(self.webView.URL)));
        item->SetVirtualURL(item->GetURL());
        context->SetError([NSError
            errorWithDomain:NSURLErrorDomain
                       code:NSURLErrorNetworkConnectionLost
                   userInfo:@{
                     NSURLErrorFailingURLStringErrorKey :
                         base::SysUTF8ToNSString(item->GetURL().spec())
                   }]);
        item->error_retry_state_machine().SetRetryPlaceholderNavigation();
      }
    }

    web::ErrorRetryCommand command =
        item->error_retry_state_machine().DidFinishNavigation(webViewURL);
    [self handleErrorRetryCommand:command
                   navigationItem:item
                navigationContext:context
               originalNavigation:navigation];
  }

  [_navigationStates setState:web::WKNavigationState::FINISHED
                forNavigation:navigation];

  DCHECK(!_isHalted);
  // Trigger JavaScript driven post-document-load-completion tasks.
  // TODO(crbug.com/546350): Investigate using
  // WKUserScriptInjectionTimeAtDocumentEnd to inject this material at the
  // appropriate time rather than invoking here.
  web::ExecuteJavaScript(webView, @"__gCrWeb.didFinishNavigation()", nil);
  [self didFinishNavigation:context];

  if (web::features::StorePendingItemInContext()) {
    // Remove the navigation to immediately get rid of pending item.
    if (web::WKNavigationState::NONE !=
        [_navigationStates stateForNavigation:navigation]) {
      [_navigationStates removeNavigation:navigation];
    }
  } else {
    [self forgetNullWKNavigation:navigation];
  }
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self didReceiveWebViewNavigationDelegateCallback];

  [_navigationStates setState:web::WKNavigationState::FAILED
                forNavigation:navigation];

  [self handleLoadError:error forNavigation:navigation provisionalLoad:NO];

  [self removeAllWebFrames];
  _certVerificationErrors->Clear();
  [self forgetNullWKNavigation:navigation];
}

- (void)webView:(WKWebView*)webView
    didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge*)challenge
                    completionHandler:
                        (void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler {
  [self didReceiveWebViewNavigationDelegateCallback];

  NSString* authMethod = challenge.protectionSpace.authenticationMethod;
  if ([authMethod isEqual:NSURLAuthenticationMethodHTTPBasic] ||
      [authMethod isEqual:NSURLAuthenticationMethodNTLM] ||
      [authMethod isEqual:NSURLAuthenticationMethodHTTPDigest]) {
    [self handleHTTPAuthForChallenge:challenge
                   completionHandler:completionHandler];
    return;
  }

  if (![authMethod isEqual:NSURLAuthenticationMethodServerTrust]) {
    completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
    return;
  }

  SecTrustRef trust = challenge.protectionSpace.serverTrust;
  base::ScopedCFTypeRef<SecTrustRef> scopedTrust(trust,
                                                 base::scoped_policy::RETAIN);
  __weak CRWWebController* weakSelf = self;
  [_certVerificationController
      decideLoadPolicyForTrust:scopedTrust
                          host:challenge.protectionSpace.host
             completionHandler:^(web::CertAcceptPolicy policy,
                                 net::CertStatus status) {
               CRWWebController* strongSelf = weakSelf;
               if (!strongSelf) {
                 completionHandler(
                     NSURLSessionAuthChallengeRejectProtectionSpace, nil);
                 return;
               }
               [strongSelf processAuthChallenge:challenge
                            forCertAcceptPolicy:policy
                                     certStatus:status
                              completionHandler:completionHandler];
             }];
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView {
  [self didReceiveWebViewNavigationDelegateCallback];

  _certVerificationErrors->Clear();
  [self removeAllWebFrames];
  [self webViewWebProcessDidCrash];
}

#pragma mark - WKNavigationDelegate Helpers

// Called when the web page has changed document and/or URL, and so the page
// navigation should be reported to the delegate, and internal state updated to
// reflect the fact that the navigation has occurred. |context| contains
// information about the navigation that triggered the document/URL change.
// TODO(stuartmorgan): This method conflates document changes and URL changes;
// we should be distinguishing better, and be clear about the expected
// WebDelegate and WCO callbacks in each case.
- (void)webPageChangedWithContext:(web::NavigationContextImpl*)context {
  web::Referrer referrer = [self currentReferrer];
  // If no referrer was known in advance, record it now. (If there was one,
  // keep it since it will have a more accurate URL and policy than what can
  // be extracted from the landing page.)
  web::NavigationItem* currentItem = self.currentNavItem;

  // TODO(crbug.com/925304): Pending item (which should be used here) should be
  // owned by NavigationContext object. Pending item should never be null.
  if (currentItem && !currentItem->GetReferrer().url.is_valid()) {
    currentItem->SetReferrer(referrer);
  }

  // TODO(stuartmorgan): This shouldn't be called for hash state or
  // push/replaceState.
  [self resetDocumentSpecificState];

  [self didStartLoading];
  // Do not commit pending item in the middle of loading a placeholder URL. The
  // item will be committed when the native content or webUI is displayed.
  if (!context->IsPlaceholderNavigation()) {
    self.navigationManagerImpl->CommitPendingItem(context->ReleaseItem());
    if (web::features::StorePendingItemInContext() &&
        context->IsLoadingHtmlString()) {
      self.navigationManagerImpl->GetLastCommittedItem()->SetURL(
          context->GetUrl());
    }
    // If a SafeBrowsing warning is currently displayed, the user has tapped
    // the button on the warning page to proceed to the site, the site has
    // started loading, and the warning is about to be removed. In this case,
    // the transient item for the warning needs to be removed too.
    if ([self isSafeBrowsingWarningDisplayedInWebView])
      self.navigationManagerImpl->DiscardNonCommittedItems();
  }
}

// Resets any state that is associated with a specific document object (e.g.,
// page interaction tracking).
- (void)resetDocumentSpecificState {
  _lastUserInteraction = nullptr;
  _clickInProgress = NO;
}

// Called when a page (native or web) has actually started loading (i.e., for
// a web page the document has actually changed), or after the load request has
// been registered for a non-document-changing URL change. Updates internal
// state not specific to web pages.
- (void)didStartLoading {
  _loadPhase = web::PAGE_LOADING;
  _displayStateOnStartLoading = self.pageDisplayState;

  self.userInteractionRegistered = NO;
  _pageHasZoomed = NO;
}

// Caches request POST data in the given session entry.
- (void)cachePOSTDataForRequest:(NSURLRequest*)request
               inNavigationItem:(web::NavigationItemImpl*)item {
  NSUInteger maxPOSTDataSizeInBytes = 4096;
  NSString* cookieHeaderName = @"cookie";

  DCHECK(item);
  const bool shouldUpdateEntry =
      ui::PageTransitionCoreTypeIs(item->GetTransitionType(),
                                   ui::PAGE_TRANSITION_FORM_SUBMIT) &&
      ![request HTTPBodyStream] &&  // Don't cache streams.
      !item->HasPostData() &&
      item->GetURL() == net::GURLWithNSURL([request URL]);
  const bool belowSizeCap =
      [[request HTTPBody] length] < maxPOSTDataSizeInBytes;
  DLOG_IF(WARNING, shouldUpdateEntry && !belowSizeCap)
      << "Data in POST request exceeds the size cap (" << maxPOSTDataSizeInBytes
      << " bytes), and will not be cached.";

  if (shouldUpdateEntry && belowSizeCap) {
    item->SetPostData([request HTTPBody]);
    item->ResetHttpRequestHeaders();
    item->AddHttpRequestHeaders([request allHTTPHeaderFields]);
    // Don't cache the "Cookie" header.
    // According to NSURLRequest documentation, |-valueForHTTPHeaderField:| is
    // case insensitive, so it's enough to test the lower case only.
    if ([request valueForHTTPHeaderField:cookieHeaderName]) {
      // Case insensitive search in |headers|.
      NSSet* cookieKeys = [item->GetHttpRequestHeaders()
          keysOfEntriesPassingTest:^(id key, id obj, BOOL* stop) {
            NSString* header = (NSString*)key;
            const BOOL found =
                [header caseInsensitiveCompare:cookieHeaderName] ==
                NSOrderedSame;
            *stop = found;
            return found;
          }];
      DCHECK_EQ(1u, [cookieKeys count]);
      item->RemoveHttpRequestHeaderForKey([cookieKeys anyObject]);
    }
  }
}

// Returns YES if the given |action| should be allowed to continue for app
// specific URL. If this returns NO, the navigation should be cancelled.
// App specific pages have elevated privileges and WKWebView uses the same
// renderer process for all page frames. With that Chromium does not allow
// running App specific pages in the same process as a web site from the
// internet. Allows navigation to app specific URL in the following cases:
//   - last committed URL is app specific
//   - navigation not a new navigation (back-forward or reload)
//   - navigation is typed, generated or bookmark
//   - navigation is performed in iframe and main frame is app-specific page
- (BOOL)shouldAllowAppSpecificURLNavigationAction:(WKNavigationAction*)action
                                       transition:
                                           (ui::PageTransition)pageTransition {
  GURL requestURL = net::GURLWithNSURL(action.request.URL);
  DCHECK(web::GetWebClient()->IsAppSpecificURL(requestURL));
  if (web::GetWebClient()->IsAppSpecificURL(
          self.webStateImpl->GetLastCommittedURL())) {
    // Last committed page is also app specific and navigation should be
    // allowed.
    return YES;
  }

  if (!ui::PageTransitionIsNewNavigation(pageTransition)) {
    // Allow reloads and back-forward navigations.
    return YES;
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(pageTransition,
                                                  ui::PAGE_TRANSITION_TYPED)) {
    return YES;
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(
          pageTransition, ui::PAGE_TRANSITION_GENERATED)) {
    return YES;
  }

  if (ui::PageTransitionTypeIncludingQualifiersIs(
          pageTransition, ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    return YES;
  }

  // If the session is being restored, allow the navigation.
  if (web::features::WebUISchemeHandlingEnabled() &&
      IsRestoreSessionUrl(_documentURL)) {
    return YES;
  }

  GURL mainDocumentURL = net::GURLWithNSURL(action.request.mainDocumentURL);
  if (web::GetWebClient()->IsAppSpecificURL(mainDocumentURL) &&
      !action.sourceFrame.mainFrame) {
    // AppSpecific URLs are allowed inside iframe if the main frame is also
    // app specific page.
    return YES;
  }

  return NO;
}

// Called when a load ends in an error.
- (void)handleLoadError:(NSError*)error
          forNavigation:(WKNavigation*)navigation
        provisionalLoad:(BOOL)provisionalLoad {
  if (error.code == NSURLErrorCancelled) {
    [self handleCancelledError:error
                 forNavigation:navigation
               provisionalLoad:provisionalLoad];
    // NSURLErrorCancelled errors that aren't handled by aborting the load will
    // automatically be retried by the web view, so early return in this case.
    return;
  }

  web::NavigationContextImpl* navigationContext =
      [_navigationStates contextForNavigation:navigation];
  navigationContext->SetError(error);
  navigationContext->SetIsPost([self isCurrentNavigationItemPOST]);
  // TODO(crbug.com/803631) DCHECK that self.currentNavItem is the navigation
  // item associated with navigationContext.

  if ([error.domain isEqual:base::SysUTF8ToNSString(web::kWebKitErrorDomain)]) {
    if (error.code == web::kWebKitErrorPlugInLoadFailed) {
      // In cases where a Plug-in handles the load do not take any further
      // action.
      return;
    }

    if (error.code == web::kWebKitErrorUrlBlockedByContentFilter) {
      DCHECK(provisionalLoad);
      if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
        // If URL is blocked due to Restriction, do not take any further action
        // as WKWebView will show a built-in error.
      } else if (web::features::StorePendingItemInContext()) {
        ui::PageTransition transition = navigationContext->GetPageTransition();
        if (transition & ui::PAGE_TRANSITION_RELOAD &&
            !(transition & ui::PAGE_TRANSITION_FORWARD_BACK)) {
          // There is no pending item for reload (see crbug.com/676129). So
          // the is nothing to do.
          DCHECK(!self.navigationManagerImpl->GetPendingItem());
        } else {
          // A new or back-forward navigation, which requires navigation item
          // commit.
          DCHECK(self.navigationManagerImpl->GetPendingItem());
          DCHECK(transition & ui::PAGE_TRANSITION_FORWARD_BACK ||
                 PageTransitionIsNewNavigation(transition));
          self.navigationManagerImpl->CommitPendingItem(
              navigationContext->ReleaseItem());
        }
        // WKWebView will show the error page, so no further action is required.
        return;
      }
    }

    if (error.code == web::kWebKitErrorFrameLoadInterruptedByPolicyChange) {
      // This method should not be called if the navigation was cancelled by
      // embedder.
      DCHECK(_pendingNavigationInfo && ![_pendingNavigationInfo cancelled]);

      // Handle Frame Load Interrupted errors from WebView. This block is
      // executed when web controller rejected the load inside
      // decidePolicyForNavigationResponse: to handle download or WKWebView
      // opened a Universal Link.
      if (!navigationContext->IsDownload()) {
        // Non-download navigation was cancelled because WKWebView has opened a
        // Universal Link and called webView:didFailProvisionalNavigation:.
        self.navigationManagerImpl->DiscardNonCommittedItems();
      }
      self.webStateImpl->SetIsLoading(false);
      return;
    }
  }

  NavigationManager* navManager = self.webState->GetNavigationManager();
  web::NavigationItem* lastCommittedItem = navManager->GetLastCommittedItem();
  if (lastCommittedItem) {
    // Reset SSL status for last committed navigation to avoid showing security
    // status for error pages.
    if (!lastCommittedItem->GetSSL().Equals(web::SSLStatus())) {
      lastCommittedItem->GetSSL() = web::SSLStatus();
      self.webStateImpl->DidChangeVisibleSecurityState();
    }
  }

  web::NavigationItemImpl* item =
      web::GetItemWithUniqueID(self.navigationManagerImpl, navigationContext);

  if (item) {
    GURL errorURL =
        net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey]);
    web::ErrorRetryCommand command = web::ErrorRetryCommand::kDoNothing;
    if (provisionalLoad) {
      command = item->error_retry_state_machine().DidFailProvisionalNavigation(
          net::GURLWithNSURL(self.webView.URL), errorURL);
    } else {
      command = item->error_retry_state_machine().DidFailNavigation(
          net::GURLWithNSURL(self.webView.URL), errorURL);
    }
    [self handleErrorRetryCommand:command
                   navigationItem:item
                navigationContext:navigationContext
               originalNavigation:navigation];
  }

  // Don't commit the pending item or call OnNavigationFinished until the
  // placeholder navigation finishes loading.
}

// Handles cancelled load in WKWebView (error with NSURLErrorCancelled code).
- (void)handleCancelledError:(NSError*)error
               forNavigation:(WKNavigation*)navigation
             provisionalLoad:(BOOL)provisionalLoad {
  if ([self shouldCancelLoadForCancelledError:error
                              provisionalLoad:provisionalLoad]) {
    web::NavigationContextImpl* navigationContext =
        [_navigationStates contextForNavigation:navigation];
    [self loadCancelled];
    self.navigationManagerImpl->DiscardNonCommittedItems();
    // If discarding the non-committed entries results in native content URL,
    // reload it in its native view. For WKBasedNavigationManager, this is not
    // necessary because WKWebView takes care of reloading the placeholder URL,
    // which triggers native view upon completion.
    if (!web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
        !self.nativeController) {
      GURL lastCommittedURL = self.webState->GetLastCommittedURL();
      if ([self shouldLoadURLInNativeView:lastCommittedURL]) {
        [self loadCurrentURLInNativeViewWithRendererInitiatedNavigation:
                  navigationContext->IsRendererInitiated()];
      }
    }

    if (provisionalLoad) {
      self.webStateImpl->OnNavigationFinished(navigationContext);
    }
  }
}

// Used to decide whether a load that generates errors with the
// NSURLErrorCancelled code should be cancelled.
- (BOOL)shouldCancelLoadForCancelledError:(NSError*)error
                          provisionalLoad:(BOOL)provisionalLoad {
  DCHECK(error.code == NSURLErrorCancelled ||
         error.code == web::kWebKitErrorFrameLoadInterruptedByPolicyChange);
  // Do not cancel the load if it is for an app specific URL, as such errors
  // are produced during the app specific URL load process.
  const GURL errorURL =
      net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey]);
  if (web::GetWebClient()->IsAppSpecificURL(errorURL))
    return NO;

  return provisionalLoad;
}

// This method should be called on receiving WKNavigationDelegate callbacks. It
// will log a metric if the callback occurs after the reciever has already been
// closed. It also stops the SafeBrowsing warning detection timer, since after
// this point it's too late for a SafeBrowsing warning to be displayed for the
// navigation for which the timer was started.
- (void)didReceiveWebViewNavigationDelegateCallback {
  if (_isBeingDestroyed) {
    UMA_HISTOGRAM_BOOLEAN("Renderer.WKWebViewCallbackAfterDestroy", true);
  }
  _safeBrowsingWarningDetectionTimer.Stop();
}

// Returns YES if response should be rendered in WKWebView.
- (BOOL)shouldRenderResponse:(WKNavigationResponse*)WKResponse {
  if (!WKResponse.canShowMIMEType) {
    return NO;
  }

  GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  if (responseURL.SchemeIs(url::kDataScheme) && WKResponse.forMainFrame) {
    // Block rendering data URLs for renderer-initiated navigations in main
    // frame to prevent abusive behavior (crbug.com/890558).
    web::NavigationContext* context =
        [self contextForPendingMainFrameNavigationWithURL:responseURL];
    if (context->IsRendererInitiated()) {
      return NO;
    }
  }

  return YES;
}

// Creates DownloadTask for the given navigation response. Headers are passed
// as argument to avoid extra NSDictionary -> net::HttpResponseHeaders
// conversion.
- (void)createDownloadTaskForResponse:(WKNavigationResponse*)WKResponse
                          HTTPHeaders:(net::HttpResponseHeaders*)headers {
  const GURL responseURL = net::GURLWithNSURL(WKResponse.response.URL);
  const int64_t contentLength = WKResponse.response.expectedContentLength;
  const std::string MIMEType =
      base::SysNSStringToUTF8(WKResponse.response.MIMEType);

  std::string contentDisposition;
  if (headers) {
    headers->GetNormalizedHeader("content-disposition", &contentDisposition);
  }

  ui::PageTransition transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  if (WKResponse.forMainFrame) {
    web::NavigationContextImpl* context =
        [self contextForPendingMainFrameNavigationWithURL:responseURL];
    context->SetIsDownload(true);
    context->ReleaseItem();
    // Navigation callbacks can only be called for the main frame.
    self.webStateImpl->OnNavigationFinished(context);
    transition = context->GetPageTransition();
    bool transitionIsLink = ui::PageTransitionTypeIncludingQualifiersIs(
        transition, ui::PAGE_TRANSITION_LINK);
    if (transitionIsLink && !context->HasUserGesture()) {
      // Link click is not possible without user gesture, so this transition
      // was incorrectly classified and should be "client redirect" instead.
      // TODO(crbug.com/549301): Remove this workaround when transition
      // detection is fixed.
      transition = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
    }
  }
  web::DownloadController::FromBrowserState(
      self.webStateImpl->GetBrowserState())
      ->CreateDownloadTask(self.webStateImpl, [NSUUID UUID].UUIDString,
                           responseURL, contentDisposition, contentLength,
                           MIMEType, transition);
}

// Extracts navigation info from WKNavigationAction and sets it as a pending.
// Some pieces of navigation information are only known in
// |decidePolicyForNavigationAction|, but must be in a pending state until
// |didgo/Navigation| where it becames current.
- (void)updatePendingNavigationInfoFromNavigationAction:
    (WKNavigationAction*)action {
  if (action.targetFrame.mainFrame) {
    _pendingNavigationInfo =
        [[CRWWebControllerPendingNavigationInfo alloc] init];
    [_pendingNavigationInfo
        setReferrer:[self referrerFromNavigationAction:action]];
    [_pendingNavigationInfo setNavigationType:action.navigationType];
    [_pendingNavigationInfo setHTTPMethod:action.request.HTTPMethod];
    BOOL hasUserGesture = web::GetNavigationActionInitiationType(action) ==
                          web::NavigationActionInitiationType::kUserInitiated;
    [_pendingNavigationInfo setHasUserGesture:hasUserGesture];
  }
}

// Extracts navigation info from WKNavigationResponse and sets it as a pending.
// Some pieces of navigation information are only known in
// |decidePolicyForNavigationResponse|, but must be in a pending state until
// |didCommitNavigation| where it becames current.
- (void)updatePendingNavigationInfoFromNavigationResponse:
    (WKNavigationResponse*)response {
  if (response.isForMainFrame) {
    if (!_pendingNavigationInfo) {
      _pendingNavigationInfo =
          [[CRWWebControllerPendingNavigationInfo alloc] init];
    }
    [_pendingNavigationInfo setMIMEType:response.response.MIMEType];
  }
}

// Updates current state with any pending information. Should be called when a
// navigation is committed.
- (void)commitPendingNavigationInfo {
  if ([_pendingNavigationInfo referrer]) {
    _currentReferrerString = [[_pendingNavigationInfo referrer] copy];
  }
  [self updateCurrentBackForwardListItemHolder];

  _pendingNavigationInfo = nil;
}

// Updates the WKBackForwardListItemHolder navigation item.
- (void)updateCurrentBackForwardListItemHolder {
  // WebUI pages (which are loaded via loadHTMLString:baseURL:) have no entry
  // in the back/forward list, so the current item will still be the previous
  // page, and should not be associated.
  if (_webUIManager)
    return;

  if (!self.currentNavItem) {
    // TODO(crbug.com/925304): Pending item (which stores the holder) should be
    // owned by NavigationContext object. Pending item should never be null.
    return;
  }

  web::WKBackForwardListItemHolder* holder =
      [self currentBackForwardListItemHolder];

  WKNavigationType navigationType =
      _pendingNavigationInfo ? [_pendingNavigationInfo navigationType]
                             : WKNavigationTypeOther;
  holder->set_back_forward_list_item(self.webView.backForwardList.currentItem);
  holder->set_navigation_type(navigationType);
  holder->set_http_method([_pendingNavigationInfo HTTPMethod]);

  // Only update the MIME type in the holder if there was MIME type information
  // as part of this pending load. It will be nil when doing a fast
  // back/forward navigation, for instance, because the callback that would
  // populate it is not called in that flow.
  if ([_pendingNavigationInfo MIMEType])
    holder->set_mime_type([_pendingNavigationInfo MIMEType]);
}

// Returns context for pending navigation that has |URL|. null if there is no
// matching pending navigation.
- (web::NavigationContextImpl*)contextForPendingMainFrameNavigationWithURL:
    (const GURL&)URL {
  // Here the enumeration variable |navigation| is __strong to allow setting it
  // to nil.
  for (__strong id navigation in [_navigationStates pendingNavigations]) {
    if (navigation == [NSNull null]) {
      // null is a valid navigation object passed to WKNavigationDelegate
      // callbacks and represents window opening action.
      navigation = nil;
    }

    web::NavigationContextImpl* context =
        [_navigationStates contextForNavigation:navigation];
    if (context && context->GetUrl() == URL) {
      return context;
    }
  }
  return nullptr;
}

// Updates URL for navigation context and navigation item.
- (void)didReceiveRedirectForNavigation:(web::NavigationContextImpl*)context
                                withURL:(const GURL&)URL {
  context->SetUrl(URL);
  web::NavigationItemImpl* item =
      web::GetItemWithUniqueID(self.navigationManagerImpl, context);

  // Associated item can be a pending item, previously discarded by another
  // navigation. WKWebView allows multiple provisional navigations, while
  // Navigation Manager has only one pending navigation.
  if (item) {
    if (!web::wk_navigation_util::IsWKInternalUrl(URL)) {
      item->SetVirtualURL(URL);
      item->SetURL(URL);
    }
    // Redirects (3xx response code), must change POST requests to GETs.
    item->SetPostData(nil);
    item->ResetHttpRequestHeaders();
  }

  _lastTransferTimeInSeconds = CFAbsoluteTimeGetCurrent();
}

// Returns YES if the current live view is a web view with an image MIME type.
- (BOOL)contentIsImage {
  if (!self.webView)
    return NO;

  const std::string image = "image";
  std::string MIMEType = self.webState->GetContentsMimeType();
  return MIMEType.compare(0, image.length(), image) == 0;
}

// WKNavigation objects are used as a weak key to store web::NavigationContext.
// WKWebView manages WKNavigation lifetime and destroys them after the
// navigation is finished. However for window opening navigations WKWebView
// passes null WKNavigation to WKNavigationDelegate callbacks and strong key is
// used to store web::NavigationContext. Those "null" navigations have to be
// cleaned up manually by calling this method.
- (void)forgetNullWKNavigation:(WKNavigation*)navigation {
  if (!navigation)
    [_navigationStates removeNavigation:navigation];
}

#pragma mark - CRWSSLStatusUpdaterDataSource

- (void)SSLStatusUpdater:(CRWSSLStatusUpdater*)SSLStatusUpdater
    querySSLStatusForTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                      host:(NSString*)host
         completionHandler:(StatusQueryHandler)completionHandler {
  [_certVerificationController querySSLStatusForTrust:std::move(trust)
                                                 host:host
                                    completionHandler:completionHandler];
}

#pragma mark - CRWSSLStatusUpdaterDelegate

- (void)SSLStatusUpdater:(CRWSSLStatusUpdater*)SSLStatusUpdater
    didChangeSSLStatusForNavigationItem:(web::NavigationItem*)navigationItem {
  web::NavigationItem* visibleItem =
      self.webStateImpl->GetNavigationManager()->GetVisibleItem();
  if (navigationItem == visibleItem)
    self.webStateImpl->DidChangeVisibleSecurityState();
}

#pragma mark - CRWContextMenuDelegate methods

- (void)webView:(WKWebView*)webView
    handleContextMenu:(const web::ContextMenuParams&)params {
  DCHECK(webView == self.webView);
  if (_isBeingDestroyed) {
    return;
  }
  self.webStateImpl->HandleContextMenu(params);
}

- (void)webView:(WKWebView*)webView
    executeJavaScript:(NSString*)javaScript
    completionHandler:(void (^)(id, NSError*))completionHandler {
  [self executeJavaScript:javaScript completionHandler:completionHandler];
}

#pragma mark - CRWNativeContentDelegate methods

- (void)nativeContent:(id)content titleDidChange:(NSString*)title {
  [self setNavigationItemTitle:title];
}

- (void)nativeContent:(id)content
    handleContextMenu:(const web::ContextMenuParams&)params {
  if (_isBeingDestroyed) {
    return;
  }
  self.webStateImpl->HandleContextMenu(params);
}

#pragma mark - KVO Observation

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  NSString* dispatcherSelectorName = self.WKWebViewObservers[keyPath];
  DCHECK(dispatcherSelectorName);
  if (dispatcherSelectorName) {
    // With ARC memory management, it is not known what a method called
    // via a selector will return. If a method returns a retained value
    // (e.g. NS_RETURNS_RETAINED) that returned object will leak as ARC is
    // unable to property insert the correct release calls for it.
    // All selectors used here return void and take no parameters so it's safe
    // to call a function mapping to the method implementation manually.
    SEL selector = NSSelectorFromString(dispatcherSelectorName);
    IMP methodImplementation = [self methodForSelector:selector];
    if (methodImplementation) {
      void (*methodCallFunction)(id, SEL) =
          reinterpret_cast<void (*)(id, SEL)>(methodImplementation);
      methodCallFunction(self, selector);
    }
  }
}

// Called when WKWebView estimatedProgress has been changed.
- (void)webViewEstimatedProgressDidChange {
  if (!_isBeingDestroyed) {
    self.webStateImpl->SendChangeLoadProgress(self.webView.estimatedProgress);
  }
}

// Called when WKWebView certificateChain or hasOnlySecureContent property has
// changed.
- (void)webViewSecurityFeaturesDidChange {
  if (self.loadPhase == web::LOAD_REQUESTED) {
    // Do not update SSL Status for pending load. It will be updated in
    // |webView:didCommitNavigation:| callback.
    return;
  }
  [self updateSSLStatusForCurrentNavigationItem];
}

// Called when WKWebView loading state has been changed.
- (void)webViewLoadingStateDidChange {
  if (self.webView.loading)
    return;

  GURL webViewURL = net::GURLWithNSURL(self.webView.URL);

  if (![self isCurrentNavigationBackForward])
    return;

  web::NavigationContextImpl* existingContext =
      [self contextForPendingMainFrameNavigationWithURL:webViewURL];

  // When traversing history restored from a previous session, WKWebView does
  // not fire 'pageshow', 'onload', 'popstate' or any of the
  // WKNavigationDelegate callbacks for back/forward navigation from an about:
  // scheme placeholder URL to another entry or if either of the redirect fails
  // to load (e.g. in airplane mode). Loading state KVO is the only observable
  // event in this scenario, so force a reload to trigger redirect from
  // restore_session.html to the restored URL.
  bool previousURLHasAboutScheme =
      _documentURL.SchemeIs(url::kAboutScheme) ||
      IsPlaceholderUrl(_documentURL) ||
      web::GetWebClient()->IsAppSpecificURL(_documentURL);
  bool is_back_forward_navigation =
      existingContext &&
      (existingContext->GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK);
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      IsRestoreSessionUrl(webViewURL)) {
    if (previousURLHasAboutScheme || is_back_forward_navigation) {
      [self.webView reload];
      _loadPhase = web::LOAD_REQUESTED;
      return;
    }
  }

  // For failed navigations, WKWebView will sometimes revert to the previous URL
  // before committing the current navigation or resetting the web view's
  // |isLoading| property to NO.  If this is the first navigation for the web
  // view, this will result in an empty URL.
  BOOL navigationWasCommitted = _loadPhase != web::LOAD_REQUESTED;
  if (!navigationWasCommitted &&
      (webViewURL.is_empty() || webViewURL == _documentURL)) {
    return;
  }

  if (!navigationWasCommitted && ![_pendingNavigationInfo cancelled]) {
    // A fast back-forward navigation does not call |didCommitNavigation:|, so
    // signal page change explicitly.
    DCHECK_EQ(_documentURL.GetOrigin(), webViewURL.GetOrigin());
    BOOL isSameDocumentNavigation =
        [self isKVOChangePotentialSameDocumentNavigationToURL:webViewURL];

    [self setDocumentURL:webViewURL context:existingContext];
    if (!existingContext) {
      // This URL was not seen before, so register new load request.
      std::unique_ptr<web::NavigationContextImpl> newContext =
          [self registerLoadRequestForURL:webViewURL
                   sameDocumentNavigation:isSameDocumentNavigation
                           hasUserGesture:NO
                        rendererInitiated:YES
                    placeholderNavigation:IsPlaceholderUrl(webViewURL)];
      [self webPageChangedWithContext:newContext.get()];
      newContext->SetHasCommitted(!isSameDocumentNavigation);
      self.webStateImpl->OnNavigationFinished(newContext.get());
      // TODO(crbug.com/792515): It is OK, but very brittle, to call
      // |didFinishNavigation:| here because the gating condition is mutually
      // exclusive with the condition below. Refactor this method after
      // deprecating _pendingNavigationInfo.
      if (newContext->GetWKNavigationType() == WKNavigationTypeBackForward) {
        [self didFinishNavigation:newContext.get()];
      }
    } else {
      // Same document navigation does not contain response headers.
      net::HttpResponseHeaders* headers =
          isSameDocumentNavigation
              ? nullptr
              : self.webStateImpl->GetHttpResponseHeaders();
      existingContext->SetResponseHeaders(headers);
      existingContext->SetIsSameDocument(isSameDocumentNavigation);
      existingContext->SetHasCommitted(!isSameDocumentNavigation);
      self.webStateImpl->OnNavigationStarted(existingContext);
      [self webPageChangedWithContext:existingContext];
      self.webStateImpl->OnNavigationFinished(existingContext);
    }
  }

  [self updateSSLStatusForCurrentNavigationItem];
  [self didFinishNavigation:existingContext];
}

// Called when WKWebView title has been changed.
- (void)webViewTitleDidChange {
  // WKWebView's title becomes empty when the web process dies; ignore that
  // update.
  if (_webProcessCrashed) {
    DCHECK_EQ(self.webView.title.length, 0U);
    return;
  }

  web::WKNavigationState lastNavigationState =
      [_navigationStates lastAddedNavigationState];
  bool hasPendingNavigation =
      lastNavigationState == web::WKNavigationState::REQUESTED ||
      lastNavigationState == web::WKNavigationState::STARTED ||
      lastNavigationState == web::WKNavigationState::REDIRECTED;

  if (!hasPendingNavigation &&
      !IsPlaceholderUrl(net::GURLWithNSURL(self.webView.URL))) {
    // Do not update the title if there is a navigation in progress because
    // there is no way to tell if KVO change fired for new or previous page.
    [self setNavigationItemTitle:self.webView.title];
  }
}

// Called when WKWebView canGoForward/canGoBack state has been changed.
- (void)webViewBackForwardStateDidChange {
  // Don't trigger for LegacyNavigationManager because its back/foward state
  // doesn't always match that of WKWebView.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled())
    self.webStateImpl->OnBackForwardStateChanged();
}

// Called when WKWebView URL has been changed.
- (void)webViewURLDidChange {
  // TODO(stuartmorgan): Determine if there are any cases where this still
  // happens, and if so whether anything should be done when it does.
  if (!self.webView.URL) {
    DVLOG(1) << "Received nil URL callback";
    return;
  }
  GURL URL(net::GURLWithNSURL(self.webView.URL));
  // URL changes happen at four points:
  // 1) When a load starts; at this point, the load is provisional, and
  //    it should be ignored until it's committed, since the document/window
  //    objects haven't changed yet.
  // 2) When a non-document-changing URL change happens (hash change,
  //    history.pushState, etc.). This URL change happens instantly, so should
  //    be reported.
  // 3) When a navigation error occurs after provisional navigation starts,
  //    the URL reverts to the previous URL without triggering a new navigation.
  // 4) When a SafeBrowsing warning is displayed after
  //    decidePolicyForNavigationAction but before a provisional navigation
  //    starts, and the user clicks the "Go Back" link on the warning page.
  //
  // If |isLoading| is NO, then it must be case 2, 3, or 4. If the last
  // committed URL (_documentURL) matches the current URL, assume that it is
  // case 4 if a SafeBrowsing warning is currently displayed and case 3
  // otherwise. If the URL does not match, assume it is a non-document-changing
  // URL change, and handle accordingly.
  //
  // If |isLoading| is YES, then it could either be case 1, or it could be case
  // 2 on a page that hasn't finished loading yet. If it's possible that it
  // could be a same-page navigation (in which case there may not be any other
  // callback about the URL having changed), then check the actual page URL via
  // JavaScript. If the origin of the new URL matches the last committed URL,
  // then check window.location.href, and if it matches, trust it. The origin
  // check ensures that if a site somehow corrupts window.location.href it can't
  // do a redirect to a slow-loading target page while it is still loading to
  // spoof the origin. On a document-changing URL change, the
  // window.location.href will match the previous URL at this stage, not the web
  // view's current URL.
  if (!self.webView.loading) {
    if (_documentURL == URL) {
      if (![self isSafeBrowsingWarningDisplayedInWebView])
        return;

      self.navigationManagerImpl->DiscardNonCommittedItems();
      self.webStateImpl->SetIsLoading(false);
      _pendingNavigationInfo = nil;
      if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
        // Right after a history navigation that gets cancelled by a tap on
        // "Go Back", WKWebView's current back/forward list item will still be
        // for the unsafe page; updating this is the responsibility of the
        // WebProcess, so only happens after an IPC round-trip to and from the
        // WebProcess with no notification to the embedder. This means that
        // WKBasedNavigationManagerImpl::WKWebViewCache::GetCurrentItemIndex()
        // will be the index of the unsafe page's item. To get back into a
        // consistent state, force a reload.
        [self.webView reload];
      } else {
        // Tapping "Go Back" on a SafeBrowsing interstitial can change whether
        // there are any forward or back items, e.g., by returning to or
        // moving away from the forward-most or back-most item.
        self.webStateImpl->OnBackForwardStateChanged();
      }
      return;
    }

    // At this point, self.webView, self.webView.backForwardList.currentItem and
    // its associated NavigationItem should all have the same URL, except in two
    // edge cases:
    // 1. location.replace that only changes hash: WebKit updates
    // self.webView.URL
    //    and currentItem.URL, and NavigationItem URL must be synced.
    // 2. location.replace to about: URL: a WebKit bug causes only
    // self.webView.URL,
    //    but not currentItem.URL to be updated. NavigationItem URL should be
    //    synced to self.webView.URL.
    // This needs to be done before |URLDidChangeWithoutDocumentChange| so any
    // WebStateObserver callbacks will see the updated URL.
    // TODO(crbug.com/809287) use currentItem.URL instead of self.webView.URL to
    // update NavigationItem URL.
    if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
      const GURL webViewURL = net::GURLWithNSURL(self.webView.URL);
      web::NavigationItem* currentItem = nullptr;
      if (self.webView.backForwardList.currentItem) {
        currentItem = [[CRWNavigationItemHolder
            holderForBackForwardListItem:self.webView.backForwardList
                                             .currentItem] navigationItem];
      } else {
        // WKBackForwardList.currentItem may be nil in a corner case when
        // location.replace is called with about:blank#hash in an empty window
        // open tab. See crbug.com/866142.
        DCHECK(self.webStateImpl->HasOpener());
        DCHECK(!self.navigationManagerImpl->GetTransientItem());
        DCHECK(!self.navigationManagerImpl->GetPendingItem());
        currentItem = self.navigationManagerImpl->GetLastCommittedItem();
      }
      if (currentItem && webViewURL != currentItem->GetURL())
        currentItem->SetURL(webViewURL);
    }

    [self URLDidChangeWithoutDocumentChange:URL];
  } else if ([self isKVOChangePotentialSameDocumentNavigationToURL:URL]) {
    WKNavigation* navigation = [_navigationStates lastAddedNavigation];
    [self.webView
        evaluateJavaScript:@"window.location.href"
         completionHandler:^(id result, NSError* error) {
           // If the web view has gone away, or the location
           // couldn't be retrieved, abort.
           if (!self.webView || ![result isKindOfClass:[NSString class]]) {
             return;
           }
           GURL JSURL(base::SysNSStringToUTF8(result));
           // Check that window.location matches the new URL. If
           // it does not, this is a document-changing URL change as
           // the window location would not have changed to the new
           // URL when the script was called.
           BOOL windowLocationMatchesNewURL = JSURL == URL;
           // Re-check origin in case navigaton has occurred since
           // start of JavaScript evaluation.
           BOOL newURLOriginMatchesDocumentURLOrigin =
               _documentURL.GetOrigin() == URL.GetOrigin();
           // Check that the web view URL still matches the new URL.
           // TODO(crbug.com/563568): webViewURLMatchesNewURL check
           // may drop same document URL changes if pending URL
           // change occurs immediately after. Revisit heuristics to
           // prevent this.
           BOOL webViewURLMatchesNewURL =
               net::GURLWithNSURL(self.webView.URL) == URL;
           // Check that the new URL is different from the current
           // document URL. If not, URL change should not be reported.
           BOOL URLDidChangeFromDocumentURL = URL != _documentURL;
           // Check if a new different document navigation started before the JS
           // completion block fires. Check WKNavigationState to make sure this
           // navigation has started in WKWebView. If so, don't run the block to
           // avoid clobbering global states. See crbug.com/788452.
           // TODO(crbug.com/788465): simplify history state handling to avoid
           // this hack.
           WKNavigation* last_added_navigation =
              [_navigationStates lastAddedNavigation];
           BOOL differentDocumentNavigationStarted =
             navigation != last_added_navigation &&
             [_navigationStates stateForNavigation:last_added_navigation] >=
             web::WKNavigationState::STARTED;
           if (windowLocationMatchesNewURL &&
               newURLOriginMatchesDocumentURLOrigin &&
               webViewURLMatchesNewURL && URLDidChangeFromDocumentURL &&
               !differentDocumentNavigationStarted) {
             [self URLDidChangeWithoutDocumentChange:URL];
           }
         }];
  }
}

#pragma mark - KVO Helpers

// Returns YES if a KVO change to |newURL| could be a 'navigation' within the
// document (hash change, pushState/replaceState, etc.). This should only be
// used in the context of a URL KVO callback firing, and only if |isLoading| is
// YES for the web view (since if it's not, no guesswork is needed).
- (BOOL)isKVOChangePotentialSameDocumentNavigationToURL:(const GURL&)newURL {
  // If the origin changes, it can't be same-document.
  if (_documentURL.GetOrigin().is_empty() ||
      _documentURL.GetOrigin() != newURL.GetOrigin()) {
    return NO;
  }
  if (self.loadPhase == web::LOAD_REQUESTED) {
    // Normally LOAD_REQUESTED indicates that this is a regular, pending
    // navigation, but it can also happen during a fast-back navigation across
    // a hash change, so that case is potentially a same-document navigation.
    return web::GURLByRemovingRefFromGURL(newURL) ==
           web::GURLByRemovingRefFromGURL(_documentURL);
  }
  // If it passes all the checks above, it might be (but there's no guarantee
  // that it is).
  return YES;
}

// Called when a non-document-changing URL change occurs. Updates the
// _documentURL, and informs the superclass of the change.
- (void)URLDidChangeWithoutDocumentChange:(const GURL&)newURL {
  DCHECK(newURL == net::GURLWithNSURL(self.webView.URL));

  if (base::FeatureList::IsEnabled(
          web::features::kCrashOnUnexpectedURLChange)) {
    if (_documentURL.GetOrigin() != newURL.GetOrigin()) {
      if (!_documentURL.host().empty() &&
          (newURL.username().find(_documentURL.host()) != std::string::npos ||
           newURL.password().find(_documentURL.host()) != std::string::npos)) {
        CHECK(false);
      }
    }
  }

  // Is it ok that newURL can be restore session URL?
  if (!IsRestoreSessionUrl(_documentURL) && !IsRestoreSessionUrl(newURL)) {
    DCHECK_EQ(_documentURL.host(), newURL.host());
  }
  DCHECK(_documentURL != newURL);

  // If called during window.history.pushState or window.history.replaceState
  // JavaScript evaluation, only update the document URL. This callback does not
  // have any information about the state object and cannot create (or edit) the
  // navigation entry for this page change. Web controller will sync with
  // history changes when a window.history.didPushState or
  // window.history.didReplaceState message is received, which should happen in
  // the next runloop.
  //
  // Otherwise, simulate the whole delegate flow for a load (since the
  // superclass currently doesn't have a clean separation between URL changes
  // and document changes). Note that the order of these calls is important:
  // registering a load request logically comes before updating the document
  // URL, but also must come first since it uses state that is reset on URL
  // changes.

  // |newNavigationContext| only exists if this method has to create a new
  // context object.
  std::unique_ptr<web::NavigationContextImpl> newNavigationContext;
  if (!_changingHistoryState) {
    if ([self contextForPendingMainFrameNavigationWithURL:newURL]) {
      // NavigationManager::LoadURLWithParams() was called with URL that has
      // different fragment comparing to the previous URL.
    } else {
      // This could be:
      //   1.) Renderer-initiated fragment change
      //   2.) Assigning same-origin URL to window.location
      //   3.) Incorrectly handled window.location.replace (crbug.com/307072)
      //   4.) Back-forward same document navigation
      newNavigationContext = [self registerLoadRequestForURL:newURL
                                      sameDocumentNavigation:YES
                                              hasUserGesture:NO
                                           rendererInitiated:YES
                                       placeholderNavigation:NO];

      // With slim nav, the web page title is stored in WKBackForwardListItem
      // and synced to Navigationitem when the web view title changes.
      // Otherwise, sync the current title for items created by same document
      // navigations.
      if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
        auto* pendingItem = web::features::StorePendingItemInContext()
                                ? newNavigationContext->GetItem()
                                : self.navigationManagerImpl->GetPendingItem();
        if (pendingItem)
          pendingItem->SetTitle(self.webStateImpl->GetTitle());
      }
    }
  }

  [self setDocumentURL:newURL context:newNavigationContext.get()];

  if (!_changingHistoryState) {
    // Pass either newly created context (if it exists) or context that already
    // existed before.
    web::NavigationContextImpl* navigationContext = newNavigationContext.get();
    if (!navigationContext) {
      navigationContext =
          [self contextForPendingMainFrameNavigationWithURL:newURL];
    }
    navigationContext->SetIsSameDocument(true);
    self.webStateImpl->OnNavigationStarted(navigationContext);
    [self didStartLoading];
    self.navigationManagerImpl->CommitPendingItem(
        navigationContext->ReleaseItem());
    navigationContext->SetHasCommitted(true);
    self.webStateImpl->OnNavigationFinished(navigationContext);

    [self updateSSLStatusForCurrentNavigationItem];
    [self didFinishNavigation:navigationContext];
  }
}

#pragma mark - Testing-Only Methods

- (void)injectWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  _currentURLLoadWasTrigerred = NO;
  [self removeWebView];

  [_containerView displayWebViewContentView:webViewContentView];
  [self setWebView:static_cast<WKWebView*>(webViewContentView.webView)];
}

- (void)resetInjectedWebViewContentView {
  _currentURLLoadWasTrigerred = NO;
  [self setWebView:nil];
  [_containerView removeFromSuperview];
  _containerView = nil;
}

- (NSString*)referrerFromNavigationAction:(WKNavigationAction*)action {
  return [action.request valueForHTTPHeaderField:kReferrerHeaderName];
}

@end
