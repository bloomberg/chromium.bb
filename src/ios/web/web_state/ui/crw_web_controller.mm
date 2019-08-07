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
#import "ios/web/js_messaging/web_frame_impl.h"
#import "ios/web/js_messaging/web_frames_manager_impl.h"
#import "ios/web/navigation/crw_navigation_item_holder.h"
#import "ios/web/navigation/crw_pending_navigation_info.h"
#import "ios/web/navigation/crw_wk_navigation_handler.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#include "ios/web/navigation/error_retry_state_machine.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/navigation_manager_util.h"
#include "ios/web/navigation/web_kit_constants.h"
#import "ios/web/navigation/wk_back_forward_list_item_holder.h"
#import "ios/web/navigation/wk_navigation_action_policy_util.h"
#import "ios/web/navigation/wk_navigation_action_util.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/deprecated/crw_context_menu_delegate.h"
#import "ios/web/public/deprecated/crw_native_content.h"
#import "ios/web/public/deprecated/crw_native_content_provider.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/public/download/download_controller.h"
#include "ios/web/public/favicon_url.h"
#import "ios/web/public/java_script_dialog_presenter.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/public/web_state/page_display_state.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_policy_decider.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/security/cert_host_pair.h"
#include "ios/web/security/cert_verification_error.h"
#import "ios/web/security/crw_cert_verification_controller.h"
#import "ios/web/security/crw_ssl_status_updater.h"
#import "ios/web/security/web_interstitial_impl.h"
#import "ios/web/security/wk_web_view_security_util.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"
#import "ios/web/web_state/page_viewport_state.h"
#import "ios/web/web_state/ui/controller/crw_legacy_native_content_controller.h"
#import "ios/web/web_state/ui/controller/crw_legacy_native_content_controller_delegate.h"
#import "ios/web/web_state/ui/crw_context_menu_controller.h"
#import "ios/web/web_state/ui/crw_js_injector.h"
#import "ios/web/web_state/ui/crw_swipe_recognizer_provider.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"
#import "ios/web/web_state/ui/crw_wk_script_message_router.h"
#import "ios/web/web_state/ui/crw_wk_ui_handler.h"
#import "ios/web/web_state/ui/crw_wk_ui_handler_delegate.h"
#import "ios/web/web_state/ui/favicon_util.h"
#import "ios/web/web_state/ui/wk_security_origin_util.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/user_interaction_state.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "ios/web/web_view/error_translation_util.h"
#import "ios/web/web_view/wk_web_view_util.h"
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
using web::wk_navigation_util::kReferrerHeaderName;
using web::wk_navigation_util::URLNeedsUserAgentType;

namespace {

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

// Maximum number of errors to store in cert verification errors cache.
// Cache holds errors only for pending navigations, so the actual number of
// stored errors is not expected to be high.
const web::CertVerificationErrorsCacheType::size_type kMaxCertErrorsCount = 100;

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

@interface CRWWebController () <BrowsingDataRemoverObserver,
                                CRWWKNavigationHandlerDelegate,
                                CRWContextMenuDelegate,
                                CRWJSInjectorDelegate,
                                CRWLegacyNativeContentControllerDelegate,
                                CRWSSLStatusUpdaterDataSource,
                                CRWSSLStatusUpdaterDelegate,
                                CRWWebControllerContainerViewDelegate,
                                CRWWebViewScrollViewProxyObserver,
                                WKNavigationDelegate,
                                CRWWKNavigationHandlerDelegate,
                                CRWWKUIHandlerDelegate> {
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
  // The actual URL of the document object (i.e., the last committed URL).
  // TODO(crbug.com/549616): Remove this in favor of just updating the
  // navigation manager and treating that as authoritative.
  GURL _documentURL;
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
  std::unique_ptr<web::CertVerificationErrorsCacheType> _certVerificationErrors;

  // State of user interaction with web content.
  web::UserInteractionState _userInteractionState;
}

// The WKNavigationDelegate handler class.
@property(nonatomic, readonly, strong)
    CRWWKNavigationHandler* navigationHandler;
// The WKUIDelegate handler class.
@property(nonatomic, readonly, strong) CRWWKUIHandler* UIHandler;

// YES if in the process of closing.
@property(nonatomic, readwrite, assign) BOOL beingDestroyed;

// If |contentView_| contains a web view, this is the web view it contains.
// If not, it's nil. When setting the property, it performs basic setup.
@property(weak, nonatomic) WKWebView* webView;
// The scroll view of |webView|.
@property(weak, nonatomic, readonly) UIScrollView* webScrollView;
// The current page state of the web view. Writing to this property
// asynchronously applies the passed value to the current web view.
@property(nonatomic, readwrite) web::PageDisplayState pageDisplayState;
@property(nonatomic, strong)
    CRWLegacyNativeContentController* legacyNativeController;
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

// User agent type of the transient item if any, the pending item if a
// navigation is in progress or the last committed item otherwise.
// Returns MOBILE, the default type, if navigation manager is nullptr or empty.
@property(nonatomic, readonly) web::UserAgentType userAgentType;

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
// Called following navigation completion to generate final navigation lifecycle
// events. Navigation is considered complete when the document has finished
// loading, or when other page load mechanics are completed on a
// non-document-changing URL change.
- (void)didFinishNavigation:(web::NavigationContextImpl*)context;
// Update the appropriate parts of the model and broadcast to the embedder. This
// may be called multiple times and thus must be idempotent.
- (void)loadCompleteWithSuccess:(BOOL)loadSuccess
                     forContext:(web::NavigationContextImpl*)context;
// Called after URL is finished loading and
// self.navigationHandler.navigationState is set to FINISHED. |context| contains
// information about the navigation associated with the URL. It is nil if
// currentURL is invalid.
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
// Attempts to handle a script message. Returns YES on success, NO otherwise.
- (BOOL)respondToWKScriptMessage:(WKScriptMessage*)scriptMessage;
// Handles frame became available message.
- (void)frameBecameAvailableWithMessage:(WKScriptMessage*)message;
// Handles frame became unavailable message.
- (void)frameBecameUnavailableWithMessage:(WKScriptMessage*)message;
// Clears the frames list.
- (void)removeAllWebFrames;

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
// Sets last committed NavigationItem's title to the given |title|, which can
// not be nil.
- (void)setNavigationItemTitle:(NSString*)title;
// Returns YES if the given WKBackForwardListItem is valid to use for
// navigation.
- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item;
// Finds all the scrollviews in the view hierarchy and makes sure they do not
// interfere with scroll to top when tapping the statusbar.
- (void)optOutScrollsToTopForSubviews;
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

// Loads request for the URL of the current navigation item. Subclasses may
// choose to build a new NSURLRequest and call |loadRequest| on the underlying
// web view, or use native web view navigation where possible (for example,
// going back and forward through the history stack).
- (void)loadRequestForCurrentNavigationItem;
// Reports Navigation.IOSWKWebViewSlowFastBackForward UMA. No-op if pending
// navigation is not back forward navigation.
- (void)reportBackForwardNavigationTypeForFastNavigation:(BOOL)isFast;

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
    // Content area is lazily instantiated.
    _defaultURL = GURL(url::kAboutBlankURL);
    _jsInjector = [[CRWJSInjector alloc] initWithDelegate:self];
    _webViewProxy = [[CRWWebViewProxyImpl alloc] initWithWebController:self];
    [[_webViewProxy scrollViewProxy] addObserver:self];
    _pendingLoadCompleteActions = [[NSMutableArray alloc] init];
    web::BrowserState* browserState = _webStateImpl->GetBrowserState();
    _certVerificationController = [[CRWCertVerificationController alloc]
        initWithBrowserState:browserState];
    _certVerificationErrors =
        std::make_unique<web::CertVerificationErrorsCacheType>(
            kMaxCertErrorsCount);
    web::BrowsingDataRemover::FromBrowserState(browserState)->AddObserver(self);
    web::WebFramesManagerImpl::CreateForWebState(_webStateImpl);
    web::FindInPageManagerImpl::CreateForWebState(_webStateImpl);
    _legacyNativeController =
        [[CRWLegacyNativeContentController alloc] initWithWebState:webState];
    _legacyNativeController.delegate = self;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(orientationDidChange)
               name:UIApplicationDidChangeStatusBarOrientationNotification
             object:nil];

    _navigationHandler = [[CRWWKNavigationHandler alloc] init];
    _navigationHandler.delegate = self;

    _UIHandler = [[CRWWKUIHandler alloc] init];
    _UIHandler.delegate = self;
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
    [self.legacyNativeController
        setNativeControllerWebUsageEnabled:_webUsageEnabled];
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

- (BOOL)isWebProcessCrashed {
  return self.navigationHandler.webProcessCrashed;
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
  }
  [_jsInjector setWebView:webView];
  [_webView setNavigationDelegate:self];
  [_webView setUIDelegate:self.UIHandler];
  for (NSString* keyPath in self.WKWebViewObservers) {
    [_webView addObserver:self forKeyPath:keyPath options:0 context:nullptr];
  }
  _webView.allowsBackForwardNavigationGestures =
      _allowsBackForwardNavigationGestures;
  [self setDocumentURL:_defaultURL context:nullptr];
}

- (UIScrollView*)webScrollView {
  return self.webView.scrollView;
}

- (web::PageDisplayState)pageDisplayState {
  web::PageDisplayState displayState;
  // If a native controller is present, record its display state instead of that
  // of the underlying placeholder webview.
  if ([self.legacyNativeController hasController]) {
    displayState.scroll_state().set_content_offset(
        [self.legacyNativeController contentOffset]);
    displayState.scroll_state().set_content_inset(
        [self.legacyNativeController contentInset]);
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

- (web::UserAgentType)userAgentType {
  web::NavigationItem* item = self.currentNavItem;
  return item ? item->GetUserAgentType() : web::UserAgentType::MOBILE;
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
      [self.navigationHandler.navigationStates
              lastNavigationWithPendingItemInNavigationContext];
  if (!navigation)
    return nullptr;
  web::NavigationContextImpl* context =
      [self.navigationHandler.navigationStates contextForNavigation:navigation];
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
  [self.legacyNativeController dismissModals];
}

// Caller must reset the delegate before calling.
- (void)close {
  self.webStateImpl->CancelDialogs();

  _SSLStatusUpdater = nil;
  [self.UIHandler close];

  self.swipeRecognizerProvider = nil;
  [self.legacyNativeController close];

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
  _touchTrackingRecognizer.touchTrackingDelegate = nil;
  [[_webViewProxy scrollViewProxy] removeObserver:self];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)isViewAlive {
  return !self.navigationHandler.webProcessCrashed &&
         [_containerView isViewAlive];
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
  if ([self.legacyNativeController hasController]) {
    return [self.legacyNativeController URL];
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
  _userInteractionState.SetLastUserInteraction(nullptr);
  base::RecordAction(base::UserMetricsAction("Reload"));
  GURL URL = self.currentNavItem->GetURL();
  if ([self.legacyNativeController shouldLoadURLInNativeView:URL]) {
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
    [self.legacyNativeController reload];
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
        [self.navigationHandler.navigationStates
                 setState:web::WKNavigationState::REQUESTED
            forNavigation:navigation];
        std::unique_ptr<web::NavigationContextImpl> navigationContext = [self
            registerLoadRequestForURL:URL
                             referrer:self.currentNavItemReferrer
                           transition:ui::PageTransition::PAGE_TRANSITION_RELOAD
               sameDocumentNavigation:NO
                       hasUserGesture:YES
                    rendererInitiated:rendererInitiated
                placeholderNavigation:NO];
        [self.navigationHandler.navigationStates
               setContext:std::move(navigationContext)
            forNavigation:navigation];
      } else {
        [self loadCurrentURLWithRendererInitiatedNavigation:rendererInitiated];
      }
    }
  }
}

- (void)stopLoading {
  base::RecordAction(base::UserMetricsAction("Stop"));
  // Discard the pending and transient entried before notifying the tab model
  // observers of the change via |-abortLoad|.
  self.navigationManagerImpl->DiscardNonCommittedItems();
  [self abortLoad];
  [self.legacyNativeController stopLoading];
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
  if (self.navigationHandler.navigationState !=
      web::WKNavigationState::FINISHED)
    [self abortLoad];

  DCHECK(!_isHalted);
  self.webStateImpl->ClearTransientContent();

  web::NavigationItem* item = self.currentNavItem;
  const GURL currentURL = item ? item->GetURL() : GURL::EmptyGURL();
  const bool isCurrentURLAppSpecific =
      web::GetWebClient()->IsAppSpecificURL(currentURL);
  // If it's a chrome URL, but not a native one, create the WebUI instance.
  if (isCurrentURLAppSpecific &&
      ![self.legacyNativeController shouldLoadURLInNativeView:currentURL]) {
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
  if ([self.legacyNativeController shouldLoadURLInNativeView:currentURL]) {
    [self.legacyNativeController
        loadCurrentURLInNativeViewWithRendererInitiatedNavigation:
            rendererInitiated];
  } else {
    [self loadCurrentURLInWebView];
  }
}

- (void)loadCurrentURLIfNecessary {
  if (self.navigationHandler.webProcessCrashed) {
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
  self.navigationHandler.navigationState = web::WKNavigationState::REQUESTED;
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
      URLNeedsUserAgentType(URL)) {
    item->SetUserAgentType(web::UserAgentType::MOBILE);
  }

  WKNavigation* navigation =
      [self.webView loadData:data
                       MIMEType:MIMEType
          characterEncodingName:base::SysUTF8ToNSString(base::kCodepageUTF8)
                        baseURL:net::NSURLWithGURL(URL)];

  [self.navigationHandler.navigationStates
         setContext:std::move(navigationContext)
      forNavigation:navigation];
  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::REQUESTED
      forNavigation:navigation];
}

// Loads the HTML into the page at the given URL. Only for testing purpose.
- (void)loadHTML:(NSString*)HTML forURL:(const GURL&)URL {
  DCHECK(HTML.length);
  // Remove the transient content view.
  self.webStateImpl->ClearTransientContent();

  self.navigationHandler.navigationState = web::WKNavigationState::REQUESTED;

  // Web View should not be created for App Specific URLs.
  if (!web::GetWebClient()->IsAppSpecificURL(URL)) {
    [self ensureWebViewCreated];
    DCHECK(self.webView) << "self.webView null while trying to load HTML";
  }
  WKNavigation* navigation =
      [self.webView loadHTMLString:HTML baseURL:net::NSURLWithGURL(URL)];
  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::REQUESTED
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
  [self.navigationHandler.navigationStates setContext:std::move(context)
                                        forNavigation:navigation];
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
      _userInteractionState.UserInteractionRegisteredSincePageLoaded()) {
    item->SetPageDisplayState(self.pageDisplayState);
  }
}

- (void)wasShown {
  self.visible = YES;
  [self.legacyNativeController wasShown];
}

- (void)wasHidden {
  self.visible = NO;
  if (_isHalted)
    return;
  [self recordStateInHistory];
  [self.legacyNativeController wasHidden];
}

- (id<CRWNativeContentHolder>)nativeContentHolder {
  return self.legacyNativeController;
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
    self.navigationHandler.navigationState = web::WKNavigationState::REQUESTED;
  }

  web::WKBackForwardListItemHolder* holder =
      web::WKBackForwardListItemHolder::FromNavigationItem(item);
  holder->set_navigation_type(WKNavigationTypeBackForward);
  context->SetIsPost((holder && [holder->http_method() isEqual:@"POST"]) ||
                     item->HasPostData());

  if (holder) {
    context->SetMimeType(holder->mime_type());
  }

  [self.navigationHandler.navigationStates setContext:std::move(context)
                                        forNavigation:navigation];
  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::REQUESTED
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

#pragma mark - CRWSessionControllerDelegate (Public)

- (web::NavigationItemImpl*)pendingItemForSessionController:
    (CRWSessionController*)sessionController {
  return [self lastPendingItemForNewNavigation];
}

#pragma mark - CRWTouchTrackingDelegate (Public)

- (void)touched:(BOOL)touched {
  _userInteractionState.SetTapInProgress(touched);
  if (touched) {
    _userInteractionState.SetUserInteractionRegisteredSincePageLoaded(true);
    if (_isBeingDestroyed)
      return;
    const NavigationManagerImpl* navigationManager = self.navigationManagerImpl;
    GURL mainDocumentURL =
        navigationManager->GetLastCommittedItem()
            ? navigationManager->GetLastCommittedItem()->GetURL()
            : [self currentURL];
    _userInteractionState.SetLastUserInteraction(
        std::make_unique<web::UserInteractionEvent>(mainDocumentURL));
  }
}

#pragma mark - ** Private Methods **

- (void)setDocumentURL:(const GURL&)newURL
               context:(web::NavigationContextImpl*)context {
  if (newURL != _documentURL && newURL.is_valid()) {
    _documentURL = newURL;
    _userInteractionState.SetUserInteractionRegisteredSinceLastUrlChange(false);
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

- (BOOL)isBackForwardListItemValid:(WKBackForwardListItem*)item {
  // The current back-forward list item MUST be in the WKWebView's back-forward
  // list to be valid.
  WKBackForwardList* list = self.webView.backForwardList;
  return list.currentItem == item ||
         [list.forwardList indexOfObject:item] != NSNotFound ||
         [list.backList indexOfObject:item] != NSNotFound;
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

// Maps WKNavigationType to ui::PageTransition.
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
      return _userInteractionState.UserInteractionRegisteredSinceLastUrlChange()
                 ? ui::PAGE_TRANSITION_LINK
                 : ui::PAGE_TRANSITION_CLIENT_REDIRECT;
  }
}

- (BOOL)isUserInitiatedAction:(WKNavigationAction*)action {
  return _userInteractionState.IsUserInteracting(self.webView);
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
      self.navigationHandler.pendingNavigationInfo
          ? self.navigationHandler.pendingNavigationInfo.navigationType
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
  _userInteractionState.ResetLastTransferTime();

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
    // Before changing navigation state, the delegate should be informed that
    // any existing request is being cancelled before completion.
    [self.navigationHandler loadCancelled];
    DCHECK_EQ(web::WKNavigationState::FINISHED,
              self.navigationHandler.navigationState);
  }

  self.navigationHandler.navigationState = web::WKNavigationState::REQUESTED;

  // Record the state of outgoing web view. Do nothing if native controller
  // exists, because in that case recordStateInHistory will record the state
  // of incoming page as native controller is already inserted.
  // TODO(crbug.com/811770) Don't record state under WKBasedNavigationManager
  // because it may incorrectly clobber the incoming page if this is a
  // back/forward navigation. WKWebView restores page scroll state for web view
  // pages anyways so this only impacts user if WKWebView is deleted.
  if (!redirect && ![self.legacyNativeController hasController] &&
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
  context->SetIsPost([self.navigationHandler isCurrentNavigationItemPOST]);
  context->SetIsSameDocument(sameDocumentNavigation);

  if (!IsWKInternalUrl(requestURL) && !placeholderNavigation) {
    self.webStateImpl->SetIsLoading(true);
  }

  // WKWebView may have multiple pending items. Move pending item ownership from
  // NavigationManager to NavigationContext. NavigationManager owns pending item
  // after navigation was requested and until NavigationContext is created.
  if (web::features::StorePendingItemInContext()) {
    // No need to transfer the ownership for NativeContent URLs, because the
    // load of NativeContent is synchronous. No need to transfer the ownership
    // for WebUI navigations, because those navigation do not have access to
    // NavigationContext.
    if (![self.legacyNativeController
            shouldLoadURLInNativeView:context->GetUrl()]) {
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
      self.navigationHandler.currentBackForwardListItemHolder;
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
    [self.navigationHandler.navigationStates
             setState:web::WKNavigationState::REQUESTED
        forNavigation:navigation];
    [self.navigationHandler.navigationStates
           setContext:std::move(navigationContext)
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
    [self.navigationHandler.navigationStates
             setState:web::WKNavigationState::REQUESTED
        forNavigation:navigation];
    [self.navigationHandler.navigationStates
           setContext:std::move(navigationContext)
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

  NSError* error = originalContext ? originalContext->GetError() : nil;
  if (web::RequiresContentFilterBlockingWorkaround() &&
      [error.domain isEqual:base::SysUTF8ToNSString(web::kWebKitErrorDomain)] &&
      error.code == web::kWebKitErrorUrlBlockedByContentFilter) {
    GURL currentWKItemURL =
        net::GURLWithNSURL(self.webView.backForwardList.currentItem.URL);
    if (currentWKItemURL.SchemeIs(url::kAboutScheme)) {
      // WKWebView will pass nil WKNavigation objects to WKNavigationDelegate
      // callback for this navigation. TODO(crbug.com/954332): Remove the
      // workaround when https://bugs.webkit.org/show_bug.cgi?id=196930 is
      // fixed.
      navigation = nil;
    }
  }

  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::REQUESTED
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
  [self.navigationHandler.navigationStates
         setContext:std::move(navigationContext)
      forNavigation:navigation];
  return
      [self.navigationHandler.navigationStates contextForNavigation:navigation];
}

#pragma mark - End of loading

- (void)abortLoad {
  [self.webView stopLoading];
  [self.navigationHandler stopLoading];
  _certVerificationErrors->Clear();
}

- (void)didFinishNavigation:(web::NavigationContextImpl*)context {
  // This can be called at multiple times after the document has loaded. Do
  // nothing if the document has already loaded.
  if (self.navigationHandler.navigationState ==
      web::WKNavigationState::FINISHED)
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
  if (self.navigationHandler.navigationState != web::WKNavigationState::STARTED)
    return;

  const GURL currentURL([self currentURL]);

  self.navigationHandler.navigationState = web::WKNavigationState::FINISHED;

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
  DCHECK_EQ(web::WKNavigationState::FINISHED,
            self.navigationHandler.navigationState);

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

  if (![self.navigationHandler.navigationStates
              lastNavigationWithPendingItemInNavigationContext] ||
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

  [self.navigationHandler.navigationStates setContext:std::move(loadHTMLContext)
                                        forNavigation:navigation];
  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::REQUESTED
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
          [self.navigationHandler.navigationStates
              removeNavigation:originalNavigation];
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

    case web::ErrorRetryCommand::kRewriteToWebViewURL: {
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
      [self.navigationHandler.navigationStates
             setContext:std::move(navigationContext)
          forNavigation:navigation];
    } break;

    case web::ErrorRetryCommand::kRewriteToPlaceholderURL: {
      std::unique_ptr<web::NavigationContextImpl> originalContext =
          [self.navigationHandler.navigationStates
              removeNavigation:originalNavigation];
      originalContext->SetPlaceholderNavigation(YES);
      GURL placeholderURL = CreatePlaceholderUrlForUrl(item->GetURL());

      WKNavigation* navigation =
          [self.webView loadHTMLString:@""
                               baseURL:net::NSURLWithGURL(placeholderURL)];
      [self.navigationHandler.navigationStates
             setContext:std::move(originalContext)
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
  [_jsInjector executeJavaScript:script completionHandler:nil];
}

#pragma mark - CRWLegacyNativeContentControllerDelegate

- (BOOL)legacyNativeContentControllerWebUsageEnabled:
    (CRWLegacyNativeContentController*)contentController {
  return [self webUsageEnabled];
}

- (BOOL)legacyNativeContentControllerIsBeingDestroyed:
    (CRWLegacyNativeContentController*)contentController {
  return _isBeingDestroyed;
}

- (void)legacyNativeContentControllerRemoveWebView:
    (CRWLegacyNativeContentController*)contentController {
  [self removeWebView];
}

- (void)legacyNativeContentControllerDidStartLoading:
    (CRWLegacyNativeContentController*)contentController {
  [self didStartLoading];
}

- (web::NavigationContextImpl*)
     legacyNativeContentController:
         (CRWLegacyNativeContentController*)contentController
    loadPlaceholderInWebViewForURL:(const GURL&)originalURL
                 rendererInitiated:(BOOL)rendererInitiated
                        forContext:(std::unique_ptr<web::NavigationContextImpl>)
                                       originalContext {
  return [self loadPlaceholderInWebViewForURL:originalURL
                            rendererInitiated:rendererInitiated
                                   forContext:std::move(originalContext)];
}

- (std::unique_ptr<web::NavigationContextImpl>)
    legacyNativeContentController:
        (CRWLegacyNativeContentController*)contentController
        registerLoadRequestForURL:(const GURL&)requestURL
                         referrer:(const web::Referrer&)referrer
                       transition:(ui::PageTransition)transition
           sameDocumentNavigation:(BOOL)sameDocumentNavigation
                   hasUserGesture:(BOOL)hasUserGesture
                rendererInitiated:(BOOL)rendererInitiated
            placeholderNavigation:(BOOL)placeholderNavigation {
  return [self registerLoadRequestForURL:requestURL
                                referrer:referrer
                              transition:transition
                  sameDocumentNavigation:sameDocumentNavigation
                          hasUserGesture:hasUserGesture
                       rendererInitiated:rendererInitiated
                   placeholderNavigation:placeholderNavigation];
}

- (void)legacyNativeContentController:
            (CRWLegacyNativeContentController*)contentController
                setNativeContentTitle:(NSString*)title {
  [self setNavigationItemTitle:title];
}

- (void)legacyNativeContentController:
            (CRWLegacyNativeContentController*)contentController
               nativeContentDidChange:
                   (id<CRWNativeContent>)previousNativeController {
  [_containerView nativeContentDidChange:previousNativeController];
}

- (void)legacyNativeContentController:
            (CRWLegacyNativeContentController*)contentController
    nativeContentLoadDidFinishWithURL:(const GURL&)targetURL
                              context:(web::NavigationContextImpl*)context {
  self.navigationHandler.navigationState = web::WKNavigationState::FINISHED;
  [self didFinishWithURL:targetURL loadSuccess:YES context:context];
}

#pragma mark - CRWWebControllerContainerViewDelegate

- (CRWWebViewProxyImpl*)contentViewProxyForContainerView:
        (CRWWebControllerContainerView*)containerView {
  return _webViewProxy;
}

- (UIEdgeInsets)nativeContentInsetsForContainerView:
    (CRWWebControllerContainerView*)containerView {
  return [[self nativeContentHolder].nativeProvider
      nativeContentInsetForWebState:self.webState];
}

- (BOOL)shouldKeepRenderProcessAliveForContainerView:
    (CRWWebControllerContainerView*)containerView {
  return self.shouldKeepRenderProcessAlive;
}

- (void)containerView:(CRWWebControllerContainerView*)containerView
    storeWebViewInWindow:(UIView*)viewToStash {
  [web::GetWebClient()->GetWindowedContainer() addSubview:viewToStash];
}

- (void)containerViewResetNativeController:
    (CRWWebControllerContainerView*)containerView {
  [self.legacyNativeController resetNativeController];
}

- (id<CRWNativeContentHolder>)containerViewNativeContentHolder:
    (CRWWebControllerContainerView*)containerView {
  return self.nativeContentHolder;
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
  return [self
       respondToMessage:command
      userIsInteracting:_userInteractionState.IsUserInteracting(self.webView)
              originURL:net::GURLWithNSURL(self.webView.URL)
            isMainFrame:scriptMessage.frameInfo.mainFrame
            senderFrame:senderFrame];
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
  ui::PageTransition transition =
      _userInteractionState.UserInteractionRegisteredSincePageLoaded()
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
  [_jsInjector executeJavaScript:replaceWebViewJS
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
  [_jsInjector executeJavaScript:replaceStateJS
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
  _userInteractionState.SetUserInteractionRegisteredSincePageLoaded(false);
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

// Sets up WebUI for URL.
- (void)createWebUIForURL:(const GURL&)URL {
  // |CreateWebUI| will do nothing if |URL| is not a WebUI URL and then
  // |HasWebUI| will return false.
  self.webStateImpl->CreateWebUI(URL);
}

- (void)clearWebUI {
  self.webStateImpl->ClearWebUI();
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
      bool is_recoverable =
          policy == web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_UNDECIDED_BY_USER;
      std::string host =
          base::SysNSStringToUTF8(challenge.protectionSpace.host);
      _certVerificationErrors->Put(
          web::CertHostPair(leafCert, host),
          web::CertVerificationError(is_recoverable, certStatus));
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
  [_jsInjector
      executeJavaScript:@"__gCrWeb.setWebViewScrollViewIsDragging(true)"
      completionHandler:nil];
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  [_jsInjector
      executeJavaScript:@"__gCrWeb.setWebViewScrollViewIsDragging(false)"
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
  [_jsInjector executeJavaScript:kViewportContentQuery
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
  if (self.navigationHandler.navigationState ==
      web::WKNavigationState::FINISHED) {
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
  bool recoverable = false;
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
      [self.navigationHandler.navigationStates contextForNavigation:navigation];
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
          [self.legacyNativeController handleSSLError];
        }
      }));

  [self.navigationHandler loadCancelled];
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
    self.UIHandler.contextMenuController =
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
    if (![self.legacyNativeController shouldLoadURLInNativeView:visibleURL])
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
    for (__strong id navigation in
         [self.navigationHandler.navigationStates pendingNavigations]) {
      [self.navigationHandler.navigationStates removeNavigation:navigation];
    }
  }
}

// Called when web view process has been terminated.
- (void)webViewWebProcessDidCrash {
  // On iOS 11 WKWebView does not repaint after crash and reload. Recreating
  // web view fixes the issue. TODO(crbug.com/770914): Remove this workaround
  // once rdar://35063950 is fixed.
  [self removeWebView];

  self.navigationHandler.webProcessCrashed = YES;
  self.webStateImpl->CancelDialogs();
  self.webStateImpl->OnRenderProcessGone();
}

// Returns the WKWebViewConfigurationProvider associated with the web
// controller's BrowserState.
- (web::WKWebViewConfigurationProvider&)webViewConfigurationProvider {
  web::BrowserState* browserState = self.webStateImpl->GetBrowserState();
  return web::WKWebViewConfigurationProvider::FromBrowserState(browserState);
}

#pragma mark - CRWWKUIHandlerDelegate

- (const GURL&)documentURLForUIHandler:(CRWWKUIHandler*)UIHandler {
  return _documentURL;
}

- (WKWebView*)UIHandler:(CRWWKUIHandler*)UIHandler
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
                       forWebState:(web::WebState*)webState {
  CRWWebController* webController =
      static_cast<web::WebStateImpl*>(webState)->GetWebController();
  DCHECK(!webController || webState->HasOpener());

  [webController ensureWebViewCreatedWithConfiguration:configuration];
  return webController.webView;
}

- (BOOL)UIHandler:(CRWWKUIHandler*)UIHandler
    isUserInitiatedAction:(WKNavigationAction*)action {
  return [self isUserInitiatedAction:action];
}

- (web::WebStateImpl*)webStateImplForUIHandler:(CRWWKUIHandler*)UIHandler {
  return self.webStateImpl;
}

#pragma mark - WKNavigationDelegate Methods

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)navigationAction
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  [self.navigationHandler webView:webView
      decidePolicyForNavigationAction:navigationAction
                      decisionHandler:decisionHandler];
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse*)WKResponse
                      decisionHandler:
                          (void (^)(WKNavigationResponsePolicy))handler {
  [self.navigationHandler webView:webView
      decidePolicyForNavigationResponse:WKResponse
                        decisionHandler:handler];
}

- (void)webView:(WKWebView*)webView
    didStartProvisionalNavigation:(WKNavigation*)navigation {
  [self.navigationHandler webView:webView
      didStartProvisionalNavigation:navigation];
}

- (void)webView:(WKWebView*)webView
    didReceiveServerRedirectForProvisionalNavigation:(WKNavigation*)navigation {
  [self.navigationHandler webView:webView
      didReceiveServerRedirectForProvisionalNavigation:navigation];
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
  [self.navigationHandler webView:webView
      didFailProvisionalNavigation:navigation
                         withError:error];
}

- (void)webView:(WKWebView*)webView
    didCommitNavigation:(WKNavigation*)navigation {
  [self.navigationHandler webView:webView didCommitNavigation:navigation];

  // For reasons not yet fully understood, sometimes WKWebView triggers
  // |webView:didFinishNavigation| before |webView:didCommitNavigation|. If a
  // navigation is already finished, stop processing
  // (https://crbug.com/818796#c2).
  if ([self.navigationHandler.navigationStates stateForNavigation:navigation] ==
      web::WKNavigationState::FINISHED)
    return;

  BOOL committedNavigation = [self.navigationHandler.navigationStates
      isCommittedNavigation:navigation];
  if (!web::features::StorePendingItemInContext()) {
    // Code in this method relies on existance of pending item.
    [self.navigationHandler.navigationStates
             setState:web::WKNavigationState::COMMITTED
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
      [self.navigationHandler.navigationStates contextForNavigation:navigation];
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

  if (self.navigationHandler.pendingNavigationInfo.MIMEType)
    context->SetMimeType(self.navigationHandler.pendingNavigationInfo.MIMEType);

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
      [self.navigationHandler didReceiveRedirectForNavigation:context
                                                      withURL:webViewURL];
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

  [self.navigationHandler commitPendingNavigationInfoInWebView:webView];

  [self removeAllWebFrames];

  // This point should closely approximate the document object change, so reset
  // the list of injected scripts to those that are automatically injected.
  // Do not inject window ID if this is a placeholder URL: window ID is not
  // needed for native view. For WebUI, let the window ID be injected when the
  // |loadHTMLString:baseURL| navigation is committed.
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled() ||
      !IsPlaceholderUrl(webViewURL)) {
    [_jsInjector resetInjectedScriptSet];
    if ([self contentIsHTML] || [self contentIsImage] ||
        self.webState->GetContentsMimeType().empty()) {
      // In unit tests MIME type will be empty, because loadHTML:forURL: does
      // not notify web view delegate about received response, so web controller
      // does not get a chance to properly update MIME type.
      [_jsInjector injectWindowID];
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
        [[self.navigationHandler.navigationStates lastAddedNavigation]
            isEqual:navigation];
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
    [self.navigationHandler.navigationStates
             setState:web::WKNavigationState::COMMITTED
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
  [self.navigationHandler webView:webView didFinishNavigation:navigation];

  // Sometimes |webView:didFinishNavigation| arrives before
  // |webView:didCommitNavigation|. Explicitly trigger post-commit processing.
  bool navigationCommitted =
      [self.navigationHandler.navigationStates stateForNavigation:navigation] ==
      web::WKNavigationState::COMMITTED;
  UMA_HISTOGRAM_BOOLEAN("IOS.WKWebViewFinishBeforeCommit",
                        !navigationCommitted);
  if (!navigationCommitted) {
    [self webView:webView didCommitNavigation:navigation];
    DCHECK_EQ(web::WKNavigationState::COMMITTED,
              [self.navigationHandler.navigationStates
                  stateForNavigation:navigation]);
  }

  // Sometimes |didFinishNavigation| callback arrives after |stopLoading| has
  // been called. Abort in this case.
  if ([self.navigationHandler.navigationStates stateForNavigation:navigation] ==
      web::WKNavigationState::NONE) {
    return;
  }

  GURL webViewURL = net::GURLWithNSURL(webView.URL);
  GURL currentWKItemURL =
      net::GURLWithNSURL(webView.backForwardList.currentItem.URL);
  UMA_HISTOGRAM_BOOLEAN("IOS.FinishedURLMatchesCurrentItem",
                        webViewURL == currentWKItemURL);

  web::NavigationContextImpl* context =
      [self.navigationHandler.navigationStates contextForNavigation:navigation];
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

      if ([self.legacyNativeController
              shouldLoadURLInNativeView:item->GetURL()]) {
        [self.legacyNativeController
            webViewDidFinishNavigationWithContext:context
                                          andItem:item];
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

  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::FINISHED
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
    if (web::WKNavigationState::NONE != [self.navigationHandler.navigationStates
                                            stateForNavigation:navigation]) {
      [self.navigationHandler.navigationStates removeNavigation:navigation];
    }
  } else {
    [self.navigationHandler forgetNullWKNavigation:navigation];
  }
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
  [self.navigationHandler webView:webView
                didFailNavigation:navigation
                        withError:error];

  [self.navigationHandler.navigationStates
           setState:web::WKNavigationState::FAILED
      forNavigation:navigation];

  [self handleLoadError:error forNavigation:navigation provisionalLoad:NO];

  [self removeAllWebFrames];
  _certVerificationErrors->Clear();
  [self.navigationHandler forgetNullWKNavigation:navigation];
}

- (void)webView:(WKWebView*)webView
    didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge*)challenge
                    completionHandler:
                        (void (^)(NSURLSessionAuthChallengeDisposition,
                                  NSURLCredential*))completionHandler {
  [self.navigationHandler webView:webView
      didReceiveAuthenticationChallenge:challenge
                      completionHandler:completionHandler];

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
  [self.navigationHandler webViewWebContentProcessDidTerminate:webView];

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
  web::Referrer referrer = self.navigationHandler.currentReferrer;
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
    if (web::IsSafeBrowsingWarningDisplayedInWebView(self.webView))
      self.navigationManagerImpl->DiscardNonCommittedItems();
  }
}

// Resets any state that is associated with a specific document object (e.g.,
// page interaction tracking).
- (void)resetDocumentSpecificState {
  _userInteractionState.SetLastUserInteraction(nullptr);
  _userInteractionState.SetTapInProgress(false);
}

// Called when a page (native or web) has actually started loading (i.e., for
// a web page the document has actually changed), or after the load request has
// been registered for a non-document-changing URL change. Updates internal
// state not specific to web pages.
- (void)didStartLoading {
  self.navigationHandler.navigationState = web::WKNavigationState::STARTED;
  _displayStateOnStartLoading = self.pageDisplayState;

  _userInteractionState.SetUserInteractionRegisteredSincePageLoaded(false);
  _pageHasZoomed = NO;
}

// Called when a load ends in an error.
- (void)handleLoadError:(NSError*)error
          forNavigation:(WKNavigation*)navigation
        provisionalLoad:(BOOL)provisionalLoad {
  if (error.code == NSURLErrorCancelled) {
    [self handleCancelledError:error
                 forNavigation:navigation
               provisionalLoad:provisionalLoad];
    // TODO(crbug.com/957032): This might get fixed at some point. Check if
    // there is a iOS version for which we don't need it any more.
    if (@available(iOS 12.2, *)) {
      if (![self.webView.backForwardList.currentItem.URL
              isEqual:self.webView.URL] &&
          [self.navigationHandler isCurrentNavigationItemPOST]) {
        UMA_HISTOGRAM_BOOLEAN("WebController.BackForwardListOutOfSync", true);
        // Sometimes on error the backForward list is out of sync with the
        // webView, go back or forward to fix it. See crbug.com/951880.
        if ([self.webView.backForwardList.backItem.URL
                isEqual:self.webView.URL]) {
          [self.webView goBack];
        } else if ([self.webView.backForwardList.forwardItem.URL
                       isEqual:self.webView.URL]) {
          [self.webView goForward];
        }
      }
    }
    // NSURLErrorCancelled errors that aren't handled by aborting the load will
    // automatically be retried by the web view, so early return in this case.
    return;
  }

  web::NavigationContextImpl* navigationContext =
      [self.navigationHandler.navigationStates contextForNavigation:navigation];
  navigationContext->SetError(error);
  navigationContext->SetIsPost(
      [self.navigationHandler isCurrentNavigationItemPOST]);
  // TODO(crbug.com/803631) DCHECK that self.currentNavItem is the navigation
  // item associated with navigationContext.

  if ([error.domain isEqual:base::SysUTF8ToNSString(web::kWebKitErrorDomain)]) {
    if (error.code == web::kWebKitErrorPlugInLoadFailed) {
      // In cases where a Plug-in handles the load do not take any further
      // action.
      return;
    }

    ui::PageTransition transition = navigationContext->GetPageTransition();
    if (error.code == web::kWebKitErrorUrlBlockedByContentFilter) {
      DCHECK(provisionalLoad);
      if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
        // If URL is blocked due to Restriction, do not take any further
        // action as WKWebView will show a built-in error.
        if (!web::RequiresContentFilterBlockingWorkaround()) {
          return;
        } else if (!PageTransitionIsNewNavigation(transition)) {
          if (transition & ui::PAGE_TRANSITION_RELOAD) {
            self.webStateImpl->SetIsLoading(false);
          }
          return;
        }
      } else if (web::features::StorePendingItemInContext()) {
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
      DCHECK(self.navigationHandler.pendingNavigationInfo &&
             !self.navigationHandler.pendingNavigationInfo.cancelled);

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
  web::NavigationContextImpl* navigationContext =
      [self.navigationHandler.navigationStates contextForNavigation:navigation];
  if ([self shouldCancelLoadForCancelledError:error
                              provisionalLoad:provisionalLoad]) {
    [self.navigationHandler loadCancelled];
    self.navigationManagerImpl->DiscardNonCommittedItems();

    [self.legacyNativeController
        handleCancelledErrorForContext:navigationContext];

    if (provisionalLoad) {
      self.webStateImpl->OnNavigationFinished(navigationContext);
    }
  } else if (!provisionalLoad) {
    web::NavigationItemImpl* item =
        web::GetItemWithUniqueID(self.navigationManagerImpl, navigationContext);
    if (item) {
      // Since the navigation has already been committed, it will retain its
      // back / forward item even though the load has been cancelled. Update the
      // error state machine so that if future loads of this item fail, the same
      // item will be reused for the error view rather than loading a
      // placeholder URL into a new navigation item, since the latter would
      // destroy the forward list.
      item->error_retry_state_machine().SetNoNavigationError();
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

// Returns YES if the current live view is a web view with an image MIME type.
- (BOOL)contentIsImage {
  if (!self.webView)
    return NO;

  const std::string image = "image";
  std::string MIMEType = self.webState->GetContentsMimeType();
  return MIMEType.compare(0, image.length(), image) == 0;
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
  [_jsInjector executeJavaScript:javaScript
               completionHandler:completionHandler];
}

#pragma mark - CRWJSInjectorDelegate methods

- (GURL)lastCommittedURLForJSInjector:(CRWJSInjector*)injector {
  return self.webState->GetLastCommittedURL();
}

- (void)willExecuteUserScriptForJSInjector:(CRWJSInjector*)injector {
  [self touched:YES];
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
  if (self.navigationHandler.navigationState ==
      web::WKNavigationState::REQUESTED) {
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

  if (![self.navigationHandler isCurrentNavigationBackForward])
    return;

  web::NavigationContextImpl* existingContext = [self.navigationHandler
      contextForPendingMainFrameNavigationWithURL:webViewURL];

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
      self.navigationHandler.navigationState =
          web::WKNavigationState::REQUESTED;
      return;
    }
  }

  // For failed navigations, WKWebView will sometimes revert to the previous URL
  // before committing the current navigation or resetting the web view's
  // |isLoading| property to NO.  If this is the first navigation for the web
  // view, this will result in an empty URL.
  BOOL navigationWasCommitted = self.navigationHandler.navigationState !=
                                web::WKNavigationState::REQUESTED;
  if (!navigationWasCommitted &&
      (webViewURL.is_empty() || webViewURL == _documentURL)) {
    return;
  }

  if (!navigationWasCommitted &&
      !self.navigationHandler.pendingNavigationInfo.cancelled) {
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
      // deprecating self.navigationHandler.pendingNavigationInfo.
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
  if (self.navigationHandler.webProcessCrashed) {
    DCHECK_EQ(self.webView.title.length, 0U);
    return;
  }

  web::WKNavigationState lastNavigationState =
      [self.navigationHandler.navigationStates lastAddedNavigationState];
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
      if (!web::IsSafeBrowsingWarningDisplayedInWebView(self.webView))
        return;

      self.navigationManagerImpl->DiscardNonCommittedItems();
      self.webStateImpl->SetIsLoading(false);
      self.navigationHandler.pendingNavigationInfo = nil;
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
    WKNavigation* navigation =
        [self.navigationHandler.navigationStates lastAddedNavigation];
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
           // TODO(crbug.com/788465): simplify hisgtory state handling to avoid
           // this hack.
           WKNavigation* last_added_navigation =
               [self.navigationHandler.navigationStates lastAddedNavigation];
           BOOL differentDocumentNavigationStarted =
               navigation != last_added_navigation &&
               [self.navigationHandler.navigationStates
                   stateForNavigation:last_added_navigation] >=
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
  if (self.navigationHandler.navigationState ==
      web::WKNavigationState::REQUESTED) {
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
    if ([self.navigationHandler
            contextForPendingMainFrameNavigationWithURL:newURL]) {
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
      navigationContext = [self.navigationHandler
          contextForPendingMainFrameNavigationWithURL:newURL];
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

#pragma mark - CRWWKNavigationHandlerDelegate

- (BOOL)navigationHandlerWebViewIsHalted:
    (CRWWKNavigationHandler*)navigationHandler {
  return _isHalted;
}

- (BOOL)navigationHandlerWebViewBeingDestroyed:
    (CRWWKNavigationHandler*)navigationHandler {
  return _isBeingDestroyed;
}

- (web::WebStateImpl*)webStateImplForNavigationHandler:
    (CRWWKNavigationHandler*)navigationHandler {
  return self.webStateImpl;
}

- (web::UserInteractionState*)userInteractionStateForNavigationHandler:
    (CRWWKNavigationHandler*)navigationHandler {
  return &_userInteractionState;
}

- (web::CertVerificationErrorsCacheType*)
    certVerificationErrorsForNavigationHandler:
        (CRWWKNavigationHandler*)navigationHandler {
  return _certVerificationErrors.get();
}

- (GURL)navigationHandlerDocumentURL:
    (CRWWKNavigationHandler*)navigationHandler {
  return _documentURL;
}

- (ui::PageTransition)navigationHandler:
                          (CRWWKNavigationHandler*)navigationHandler
       pageTransitionFromNavigationType:(WKNavigationType)navigationType {
  return [self pageTransitionFromNavigationType:navigationType];
}

- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
        createWebUIForURL:(const GURL&)URL {
  [self createWebUIForURL:URL];
}

- (void)navigationHandlerStopLoading:
    (CRWWKNavigationHandler*)navigationHandler {
  [self stopLoading];
}

- (void)navigationHandlerAbortLoading:
    (CRWWKNavigationHandler*)navigationHandler {
  [self abortLoad];
}

- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
           setDocumentURL:(const GURL&)newURL
                  context:(web::NavigationContextImpl*)context {
  [self setDocumentURL:newURL context:context];
}

- (BOOL)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    shouldLoadURLInNativeView:(const GURL&)url {
  return [self.legacyNativeController shouldLoadURLInNativeView:url];
}

- (void)navigationHandlerRequirePageReconstruction:
    (CRWWKNavigationHandler*)navigationHandler {
  [self requirePageReconstruction];
}

- (std::unique_ptr<web::NavigationContextImpl>)
            navigationHandler:(CRWWKNavigationHandler*)navigationHandler
    registerLoadRequestForURL:(const GURL&)URL
       sameDocumentNavigation:(BOOL)sameDocumentNavigation
               hasUserGesture:(BOOL)hasUserGesture
            rendererInitiated:(BOOL)renderedInitiated
        placeholderNavigation:(BOOL)placeholderNavigation {
  return [self registerLoadRequestForURL:URL
                  sameDocumentNavigation:sameDocumentNavigation
                          hasUserGesture:hasUserGesture
                       rendererInitiated:renderedInitiated
                   placeholderNavigation:placeholderNavigation];
}

- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
     handleCancelledError:(NSError*)error
            forNavigation:(WKNavigation*)navigation
          provisionalLoad:(BOOL)provisionalLoad {
  [self handleCancelledError:error
               forNavigation:navigation
             provisionalLoad:provisionalLoad];
}

- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
       handleSSLCertError:(NSError*)error
            forNavigation:(WKNavigation*)navigation {
  [self handleSSLCertError:error forNavigation:navigation];
}

- (void)navigationHandler:(CRWWKNavigationHandler*)navigationHandler
          handleLoadError:(NSError*)error
            forNavigation:(WKNavigation*)navigation
          provisionalLoad:(BOOL)provisionalLoad {
  [self handleLoadError:error
          forNavigation:navigation
        provisionalLoad:provisionalLoad];
}
- (void)navigationHandlerRemoveAllWebFrames:
    (CRWWKNavigationHandler*)navigationHandler {
  [self removeAllWebFrames];
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

- (web::WKNavigationState)navigationState {
  return self.navigationHandler.navigationState;
}

@end
