// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_

#import <Foundation/Foundation.h>

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/values.h"
#import "ios/web/js_messaging/web_frames_manager_impl.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/ui/java_script_dialog_callback.h"
#include "ios/web/public/ui/java_script_dialog_type.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate.h"
#include "url/gurl.h"

@class CRWSessionStorage;
@class CRWWebController;
@protocol CRWWebViewProxy;
@protocol CRWWebViewNavigationProxy;
@class UIViewController;

namespace web {

class BrowserState;
struct FaviconURL;
class NavigationContextImpl;
class NavigationManager;
enum Permission : NSUInteger;
enum PermissionState : NSUInteger;
class SessionCertificatePolicyCacheImpl;
class WebFrame;

// Implementation of WebState.
// Generally mirrors //content's WebContents implementation.
// General notes on expected WebStateImpl ownership patterns:
//  - Outside of tests, WebStateImpls are created
//      (a) By @Tab, when creating a new Tab.
//      (b) By @SessionWindow, when decoding a saved session.
//      (c) By the Copy() method, below, used when marshalling a session
//          in preparation for saving.
//  - WebControllers are the eventual long-term owners of WebStateImpls.
//  - SessionWindows are transient owners, passing ownership into WebControllers
//    during session restore, and discarding owned copies of WebStateImpls after
//    writing them out for session saves.
class WebStateImpl final : public WebState {
 public:
  // Constructor for WebStateImpls created for new sessions.
  explicit WebStateImpl(const CreateParams& params);

  // Constructor for WebStateImpls created for deserialized sessions
  WebStateImpl(const CreateParams& params, CRWSessionStorage* session_storage);

  WebStateImpl(const WebStateImpl&) = delete;
  WebStateImpl& operator=(const WebStateImpl&) = delete;

  ~WebStateImpl() final;

  // Factory function creating a WebStateImpl with a fake
  // CRWWebViewNavigationProxy for testing.
  static std::unique_ptr<WebStateImpl>
  CreateWithFakeWebViewNavigationProxyForTesting(
      const CreateParams& params,
      id<CRWWebViewNavigationProxy> web_view_for_testing);

  // Gets/Sets the CRWWebController that backs this object.
  CRWWebController* GetWebController();
  void SetWebController(CRWWebController* web_controller);

  // Notifies the observers that a navigation has started.
  void OnNavigationStarted(NavigationContextImpl* context);

  // Notifies the observers that a navigation was redirected.
  void OnNavigationRedirected(NavigationContextImpl* context);

  // Notifies the observers that a navigation has finished. For same-document
  // navigations notifies the observers about favicon URLs update using
  // candidates received in OnFaviconUrlUpdated.
  void OnNavigationFinished(NavigationContextImpl* context);

  // Called when current window's canGoBack / canGoForward state was changed.
  void OnBackForwardStateChanged();

  // Called when page title was changed.
  void OnTitleChanged();

  // Notifies the observers that the render process was terminated.
  void OnRenderProcessGone();

  // Called when a script command is received.
  void OnScriptCommandReceived(const std::string& command,
                               const base::Value& value,
                               const GURL& page_url,
                               bool user_is_interacting,
                               WebFrame* sender_frame);

  // Marks the WebState as loading/not loading.
  void SetIsLoading(bool is_loading);

  // Called when a page is loaded. Must be called only once per page.
  void OnPageLoaded(const GURL& url, bool load_success);

  // Called when new FaviconURL candidates are received.
  void OnFaviconUrlUpdated(const std::vector<FaviconURL>& candidates);

  // Notifies web state observers when any of the web state's permission has
  // changed.
  void OnStateChangedForPermission(Permission permission)
      API_AVAILABLE(ios(15.0));

  // Returns the NavigationManager for this WebState.
  NavigationManagerImpl& GetNavigationManagerImpl();

  // Returns the associated WebFramesManagerImpl.
  WebFramesManagerImpl& GetWebFramesManagerImpl();

  // Returns/Sets the SessionCertificatePolicyCacheImpl for this WebStateImpl.
  SessionCertificatePolicyCacheImpl& GetSessionCertificatePolicyCacheImpl();
  void SetSessionCertificatePolicyCacheImpl(
      std::unique_ptr<SessionCertificatePolicyCacheImpl>
          certificate_policy_cache);

  // Creates a WebUI page for the given url, owned by this object.
  void CreateWebUI(const GURL& url);

  // Clears any current WebUI. Should be called when the page changes.
  // TODO(stuartmorgan): Remove once more logic is moved from WebController
  // into this class.
  void ClearWebUI();

  // Returns true if there is a WebUI active.
  bool HasWebUI() const;

  // Explicitly sets the MIME type, overwriting any MIME type that was set by
  // headers. Note that this should be called after OnNavigationCommitted, as
  // that is the point where MIME type is set from HTTP headers.
  void SetContentsMimeType(const std::string& mime_type);

  // Decides whether the navigation corresponding to |request| should be
  // allowed to continue by asking its policy deciders, and calls |callback|
  // with the decision. Defaults to PolicyDecision::Allow(). If at least one
  // policy decider's decision is PolicyDecision::Cancel(), the final result is
  // PolicyDecision::Cancel(). Otherwise, if at least one policy decider's
  // decision is PolicyDecision::CancelAndDisplayError(), the final result is
  // PolicyDecision::CancelAndDisplayError(), with the error corresponding to
  // the first PolicyDecision::CancelAndDisplayError() result that was received.
  void ShouldAllowRequest(
      NSURLRequest* request,
      WebStatePolicyDecider::RequestInfo request_info,
      WebStatePolicyDecider::PolicyDecisionCallback callback);

  // Decides whether the navigation corresponding to |response| should
  // be allowed to display an error page if an error occurs, by asking its
  // policy deciders. If at least one policy decider's decision is false,
  // returns false; otherwise returns true.
  bool ShouldAllowErrorPageToBeDisplayed(NSURLResponse* response,
                                         bool for_main_frame);

  // Decides whether the navigation corresponding to |response| should be
  // allowed to continue by asking its policy deciders, and calls |callback|
  // with the decision. Defaults to PolicyDecision::Allow(). If at least one
  // policy decider's decision is PolicyDecision::Cancel(), the final result is
  // PolicyDecision::Cancel(). Otherwise, if at least one policy decider's
  // decision is PolicyDecision::CancelAndDisplayError(), the final result is
  // PolicyDecision::CancelAndDisplayError(), with the error corresponding to
  // the first PolicyDecision::CancelAndDisplayError() result that was received.
  void ShouldAllowResponse(
      NSURLResponse* response,
      WebStatePolicyDecider::ResponseInfo response_info,
      WebStatePolicyDecider::PolicyDecisionCallback callback);

  // Returns the UIView used to contain the WebView for sizing purposes. Can be
  // nil.
  UIView* GetWebViewContainer();

  // Returns the UserAgent that should be used to load the |url| if it is a new
  // navigation. This will be Mobile or Desktop.
  UserAgentType GetUserAgentForNextNavigation(const GURL& url);

  // Returns the UserAgent type actually used by this WebState, mostly use for
  // restoration.
  UserAgentType GetUserAgentForSessionRestoration() const;

  // Sets the UserAgent type that should be used by the WebState. If
  // |user_agent| is AUTOMATIC, GetUserAgentForNextNavigation() will return
  // MOBILE or DESKTOP based on the size class of the WebView. Otherwise, it
  // will return |user_agent|.
  // GetUserAgentForSessionRestoration() will always return |user_agent|.
  void SetUserAgent(UserAgentType user_agent);

  // Notifies the delegate that the load progress was updated.
  void SendChangeLoadProgress(double progress);

  // Notifies the delegate that a Form Repost dialog needs to be presented.
  void ShowRepostFormWarningDialog(base::OnceCallback<void(bool)> callback);

  // Notifies the delegate that a JavaScript dialog needs to be presented.
  void RunJavaScriptDialog(const GURL& origin_url,
                           JavaScriptDialogType java_script_dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           DialogClosedCallback callback);

  // Returns true if a javascript dialog is running.
  bool IsJavaScriptDialogRunning();

  // Instructs the delegate to create a new web state. Called when this WebState
  // wants to open a new window. |url| is the URL of the new window;
  // |opener_url| is the URL of the page which requested a window to be open;
  // |initiated_by_user| is true if action was caused by the user.
  WebState* CreateNewWebState(const GURL& url,
                              const GURL& opener_url,
                              bool initiated_by_user);

  // Notifies the delegate that request receives an authentication challenge
  // and is unable to respond using cached credentials.
  void OnAuthRequired(NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      WebStateDelegate::AuthCallback callback);

  // Cancels all dialogs associated with this web_state.
  void CancelDialogs();

  // Returns a CRWWebViewNavigationProxy protocol that can be used to access
  // navigation related functions on the main WKWebView.
  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const;

  // Registers |frame| as a new web frame and notifies any observers.
  void WebFrameBecameAvailable(std::unique_ptr<WebFrame> frame);

  // Removes the web frame with |frame_id|, if one exists and notifies any
  // observers.
  void WebFrameBecameUnavailable(const std::string& frame_id);

  // Removes all current web frames.
  void RemoveAllWebFrames();

  // WebState:
  WebStateDelegate* GetDelegate() final;
  void SetDelegate(WebStateDelegate* delegate) final;
  bool IsRealized() const final;
  WebState* ForceRealized() final;
  bool IsWebUsageEnabled() const final;
  void SetWebUsageEnabled(bool enabled) final;
  UIView* GetView() final;
  void DidCoverWebContent() final;
  void DidRevealWebContent() final;
  base::Time GetLastActiveTime() const final;
  void WasShown() final;
  void WasHidden() final;
  void SetKeepRenderProcessAlive(bool keep_alive) final;
  BrowserState* GetBrowserState() const final;
  base::WeakPtr<WebState> GetWeakPtr() final;
  void OpenURL(const WebState::OpenURLParams& params) final;
  void LoadSimulatedRequest(const GURL& url,
                            NSString* response_html_string) final
      API_AVAILABLE(ios(15.0));
  void LoadSimulatedRequest(const GURL& url,
                            NSData* response_data,
                            NSString* mime_type) final API_AVAILABLE(ios(15.0));
  void Stop() final;
  const NavigationManager* GetNavigationManager() const final;
  NavigationManager* GetNavigationManager() final;
  const WebFramesManager* GetWebFramesManager() const final;
  WebFramesManager* GetWebFramesManager() final;
  const SessionCertificatePolicyCache* GetSessionCertificatePolicyCache()
      const final;
  SessionCertificatePolicyCache* GetSessionCertificatePolicyCache() final;
  CRWSessionStorage* BuildSessionStorage() final;
  CRWJSInjectionReceiver* GetJSInjectionReceiver() const final;
  void LoadData(NSData* data, NSString* mime_type, const GURL& url) final;
  void ExecuteJavaScript(const std::u16string& javascript) final;
  void ExecuteJavaScript(const std::u16string& javascript,
                         JavaScriptResultCallback callback) final;
  void ExecuteUserJavaScript(NSString* javaScript) final;
  NSString* GetStableIdentifier() const final;
  const std::string& GetContentsMimeType() const final;
  bool ContentIsHTML() const final;
  const std::u16string& GetTitle() const final;
  bool IsLoading() const final;
  double GetLoadingProgress() const final;
  bool IsVisible() const final;
  bool IsCrashed() const final;
  bool IsEvicted() const final;
  bool IsBeingDestroyed() const final;
  const FaviconStatus& GetFaviconStatus() const final;
  void SetFaviconStatus(const FaviconStatus& favicon_status) final;
  int GetNavigationItemCount() const final;
  const GURL& GetVisibleURL() const final;
  const GURL& GetLastCommittedURL() const final;
  GURL GetCurrentURL(URLVerificationTrustLevel* trust_level) const final;
  base::CallbackListSubscription AddScriptCommandCallback(
      const ScriptCommandCallback& callback,
      const std::string& command_prefix) final;
  id<CRWWebViewProxy> GetWebViewProxy() const final;
  void DidChangeVisibleSecurityState() final;
  InterfaceBinder* GetInterfaceBinderForMainFrame() final;
  bool HasOpener() const final;
  void SetHasOpener(bool has_opener) final;
  bool CanTakeSnapshot() const final;
  void TakeSnapshot(const gfx::RectF& rect, SnapshotCallback callback) final;
  void CreateFullPagePdf(base::OnceCallback<void(NSData*)> callback) final;
  void CloseMediaPresentations() final;
  void AddObserver(WebStateObserver* observer) final;
  void RemoveObserver(WebStateObserver* observer) final;
  void CloseWebState() final;
  bool SetSessionStateData(NSData* data) final;
  NSData* SessionStateData() final;
  PermissionState GetStateForPermission(Permission permission) const final
      API_AVAILABLE(ios(15.0));
  void SetStateForPermission(PermissionState state, Permission permission) final
      API_AVAILABLE(ios(15.0));
  NSDictionary<NSNumber*, NSNumber*>* GetStatesForAllPermissions() const final
      API_AVAILABLE(ios(15.0));

 protected:
  // WebState:
  void AddPolicyDecider(WebStatePolicyDecider* decider) final;
  void RemovePolicyDecider(WebStatePolicyDecider* decider) final;

 private:
  // Forward-declaration of the two internal classes used to implement
  // the "unrealized" state of the WebState. See the documentation at
  // //docs/ios/unrealized_web_state.md for more details.
  class RealizedWebState;
  class SerializedData;

  // Type aliases for the various ObserverList or ScriptCommandCallback map
  // used by WebStateImpl (those are reused by the RealizedWebState class).
  using WebStateObserverList = base::ObserverList<WebStateObserver, true>;

  using WebStatePolicyDeciderList =
      base::ObserverList<WebStatePolicyDecider, true>;

  using ScriptCommandCallbackMap =
      std::map<std::string,
               base::RepeatingCallbackList<ScriptCommandCallbackSignature>>;

  // Force the WebState to become realized (if in "unrealized" state) and
  // then return a pointer to the RealizedWebState. Safe to call if the
  // WebState is already realized.
  RealizedWebState* RealizedState();

  // Stores whether the web state is currently being destroyed. This is not
  // stored in RealizedWebState/SerializedData as a WebState can be destroyed
  // before becoming realized.
  bool is_being_destroyed_ = false;

  // A list of observers notified when page state changes. Weak references.
  // This is not stored in RealizedWebState/SerializedData to allow adding
  // observers to an "unrealized" WebState (which is required to listen for
  // `WebStateRealized`).
  WebStateObserverList observers_;

  // All the WebStatePolicyDeciders asked for navigation decision. Weak
  // references. This is not stored in RealizedWebState/SerializedData to
  // allow adding policy decider to an "unrealized" WebState.
  WebStatePolicyDeciderList policy_deciders_;

  // Callbacks associated to command prefixes. This is not stored in
  // RealizedWebState/SerializedData to to allow registering command
  // callback on an "unrealized" WebState.
  ScriptCommandCallbackMap script_command_callbacks_;

  // The instances of the two internal classes used to implement the
  // "unrealized" state of the WebState. One important invariant is
  // that except at all point either `pimpl_` or `saved_` is valid
  // and not null (except right at the end of the destructor or at
  // the beginning of the constructor).
  std::unique_ptr<RealizedWebState> pimpl_;
  std::unique_ptr<SerializedData> saved_;

  base::WeakPtrFactory<WebStateImpl> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_
