// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl.h"

#include <stddef.h>
#include <stdint.h>

#import "base/compiler_specific.h"
#include "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/session/session_certificate_policy_cache_impl.h"
#import "ios/web/web_state/global_web_state_event_tracker.h"
#import "ios/web/web_state/web_state_impl_realized_web_state.h"
#import "ios/web/web_state/web_state_impl_serialized_data.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {
// Function used to implement the default WebState getters.
WebState* ReturnWeakReference(base::WeakPtr<WebStateImpl> weak_web_state) {
  return weak_web_state.get();
}

// With |kEnableUnrealizedWebStates|, detect inefficient usage of WebState
// realization. Various bugs have triggered the realization of the entire
// WebStateList. Detect this by checking for the realization of 3 WebStates
// within one second. Only report this error once per launch.
constexpr size_t kMaxEvents = 3;
constexpr CFTimeInterval kWindowSizeInSeconds = 1.0f;
size_t g_last_realized_count = 0;
CFTimeInterval g_last_creation_time = 0;
bool g_has_reported_once = false;
void CheckForOverRealization() {
  if (g_has_reported_once)
    return;
  CFTimeInterval now = CACurrentMediaTime();
  if (now - g_last_creation_time < kWindowSizeInSeconds) {
    g_last_realized_count++;
    if (g_last_realized_count >= kMaxEvents) {
      base::debug::DumpWithoutCrashing();
      g_has_reported_once = true;
      NOTREACHED();
    }
  } else {
    g_last_creation_time = now;
    g_last_realized_count = 0;
  }
}

}  // namespace

void IgnoreOverRealizationCheck() {
  g_last_realized_count = 0;
}

#pragma mark - WebState factory methods

/* static */
std::unique_ptr<WebState> WebState::Create(const CreateParams& params) {
  return std::make_unique<WebStateImpl>(params);
}

/* static */
std::unique_ptr<WebState> WebState::CreateWithStorageSession(
    const CreateParams& params,
    CRWSessionStorage* session_storage) {
  DCHECK(session_storage);
  return std::make_unique<WebStateImpl>(params, session_storage);
}

#pragma mark - WebStateImpl public methods

WebStateImpl::WebStateImpl(const CreateParams& params)
    : WebStateImpl(params, nil) {}

WebStateImpl::WebStateImpl(const CreateParams& params,
                           CRWSessionStorage* session_storage) {
  if (session_storage &&
      base::FeatureList::IsEnabled(features::kEnableUnrealizedWebStates)) {
    saved_ = std::make_unique<SerializedData>(this, params, session_storage);
  } else {
    pimpl_ = std::make_unique<RealizedWebState>(this);
    pimpl_->Init(params, session_storage);
  }

  // Send creation event.
  GlobalWebStateEventTracker::GetInstance()->OnWebStateCreated(this);
}

WebStateImpl::~WebStateImpl() {
  is_being_destroyed_ = true;
  if (pimpl_) {
    pimpl_->TearDown();
  } else {
    saved_->TearDown();
  }
}

/* static */
std::unique_ptr<WebStateImpl>
WebStateImpl::CreateWithFakeWebViewNavigationProxyForTesting(
    const WebState::CreateParams& params,
    id<CRWWebViewNavigationProxy> web_view_for_testing) {
  DCHECK(web_view_for_testing);
  auto web_state = std::make_unique<WebStateImpl>(params);
  web_state->pimpl_->SetWebViewNavigationProxyForTesting(  // IN-TEST
      web_view_for_testing);
  return web_state;
}

#pragma mark - WebState implementation

CRWWebController* WebStateImpl::GetWebController() {
  return RealizedState()->GetWebController();
}

void WebStateImpl::SetWebController(CRWWebController* web_controller) {
  RealizedState()->SetWebController(web_controller);
}

void WebStateImpl::OnNavigationStarted(NavigationContextImpl* context) {
  RealizedState()->OnNavigationStarted(context);
}

void WebStateImpl::OnNavigationRedirected(NavigationContextImpl* context) {
  RealizedState()->OnNavigationRedirected(context);
}

void WebStateImpl::OnNavigationFinished(NavigationContextImpl* context) {
  RealizedState()->OnNavigationFinished(context);
}

void WebStateImpl::OnBackForwardStateChanged() {
  RealizedState()->OnBackForwardStateChanged();
}

void WebStateImpl::OnTitleChanged() {
  RealizedState()->OnTitleChanged();
}

void WebStateImpl::OnRenderProcessGone() {
  RealizedState()->OnRenderProcessGone();
}

void WebStateImpl::OnScriptCommandReceived(const std::string& command,
                                           const base::Value& value,
                                           const GURL& page_url,
                                           bool user_is_interacting,
                                           WebFrame* sender_frame) {
  RealizedState()->OnScriptCommandReceived(command, value, page_url,
                                           user_is_interacting, sender_frame);
}

void WebStateImpl::SetIsLoading(bool is_loading) {
  RealizedState()->SetIsLoading(is_loading);
}

void WebStateImpl::OnPageLoaded(const GURL& url, bool load_success) {
  RealizedState()->OnPageLoaded(url, load_success);
}

void WebStateImpl::OnFaviconUrlUpdated(
    const std::vector<FaviconURL>& candidates) {
  RealizedState()->OnFaviconUrlUpdated(candidates);
}

void WebStateImpl::OnStateChangedForPermission(Permission permission) {
  RealizedState()->OnStateChangedForPermission(permission);
}

NavigationManagerImpl& WebStateImpl::GetNavigationManagerImpl() {
  return RealizedState()->GetNavigationManager();
}

int WebStateImpl::GetNavigationItemCount() const {
  return LIKELY(pimpl_) ? pimpl_->GetNavigationItemCount()
                        : saved_->GetNavigationItemCount();
}

WebFramesManagerImpl& WebStateImpl::GetWebFramesManagerImpl() {
  return RealizedState()->GetWebFramesManager();
}

SessionCertificatePolicyCacheImpl&
WebStateImpl::GetSessionCertificatePolicyCacheImpl() {
  return RealizedState()->GetSessionCertificatePolicyCache();
}

void WebStateImpl::SetSessionCertificatePolicyCacheImpl(
    std::unique_ptr<SessionCertificatePolicyCacheImpl>
        session_certificate_policy_cache) {
  RealizedState()->SetSessionCertificatePolicyCache(
      std::move(session_certificate_policy_cache));
}

void WebStateImpl::CreateWebUI(const GURL& url) {
  RealizedState()->CreateWebUI(url);
}

void WebStateImpl::ClearWebUI() {
  RealizedState()->ClearWebUI();
}

bool WebStateImpl::HasWebUI() const {
  return LIKELY(pimpl_) ? pimpl_->HasWebUI() : false;
}

void WebStateImpl::SetContentsMimeType(const std::string& mime_type) {
  RealizedState()->SetContentsMimeType(mime_type);
}

void WebStateImpl::ShouldAllowRequest(
    NSURLRequest* request,
    WebStatePolicyDecider::RequestInfo request_info,
    WebStatePolicyDecider::PolicyDecisionCallback callback) {
  RealizedState()->ShouldAllowRequest(request, std::move(request_info),
                                      std::move(callback));
}

bool WebStateImpl::ShouldAllowErrorPageToBeDisplayed(NSURLResponse* response,
                                                     bool for_main_frame) {
  return RealizedState()->ShouldAllowErrorPageToBeDisplayed(response,
                                                            for_main_frame);
}

void WebStateImpl::ShouldAllowResponse(
    NSURLResponse* response,
    WebStatePolicyDecider::ResponseInfo response_info,
    WebStatePolicyDecider::PolicyDecisionCallback callback) {
  RealizedState()->ShouldAllowResponse(response, std::move(response_info),
                                       std::move(callback));
}

UIView* WebStateImpl::GetWebViewContainer() {
  return LIKELY(pimpl_) ? pimpl_->GetWebViewContainer() : nil;
}

UserAgentType WebStateImpl::GetUserAgentForNextNavigation(const GURL& url) {
  return RealizedState()->GetUserAgentForNextNavigation(url);
}

UserAgentType WebStateImpl::GetUserAgentForSessionRestoration() const {
  return LIKELY(pimpl_) ? pimpl_->GetUserAgentForSessionRestoration()
                        : UserAgentType::AUTOMATIC;
}

void WebStateImpl::SetUserAgent(UserAgentType user_agent) {
  RealizedState()->SetWebStateUserAgent(user_agent);
}

void WebStateImpl::SendChangeLoadProgress(double progress) {
  RealizedState()->SendChangeLoadProgress(progress);
}

void WebStateImpl::ShowRepostFormWarningDialog(
    base::OnceCallback<void(bool)> callback) {
  RealizedState()->ShowRepostFormWarningDialog(std::move(callback));
}

void WebStateImpl::RunJavaScriptDialog(
    const GURL& origin_url,
    JavaScriptDialogType javascript_dialog_type,
    NSString* message_text,
    NSString* default_prompt_text,
    DialogClosedCallback callback) {
  RealizedState()->RunJavaScriptDialog(origin_url, javascript_dialog_type,
                                       message_text, default_prompt_text,
                                       std::move(callback));
}

bool WebStateImpl::IsJavaScriptDialogRunning() {
  return LIKELY(pimpl_) ? pimpl_->IsJavaScriptDialogRunning() : false;
}

WebState* WebStateImpl::CreateNewWebState(const GURL& url,
                                          const GURL& opener_url,
                                          bool initiated_by_user) {
  return RealizedState()->CreateNewWebState(url, opener_url, initiated_by_user);
}

void WebStateImpl::OnAuthRequired(NSURLProtectionSpace* protection_space,
                                  NSURLCredential* proposed_credential,
                                  WebStateDelegate::AuthCallback callback) {
  RealizedState()->OnAuthRequired(protection_space, proposed_credential,
                                  std::move(callback));
}

void WebStateImpl::CancelDialogs() {
  RealizedState()->ClearDialogs();
}

id<CRWWebViewNavigationProxy> WebStateImpl::GetWebViewNavigationProxy() const {
  return LIKELY(pimpl_) ? pimpl_->GetWebViewNavigationProxy() : nil;
}

#pragma mark - WebFrame management

void WebStateImpl::WebFrameBecameAvailable(std::unique_ptr<WebFrame> frame) {
  RealizedState()->WebFrameBecameAvailable(std::move(frame));
}

void WebStateImpl::WebFrameBecameUnavailable(const std::string& frame_id) {
  RealizedState()->WebFrameBecameUnavailable(frame_id);
}

void WebStateImpl::RemoveAllWebFrames() {
  RealizedState()->RemoveAllWebFrames();
}

#pragma mark - WebState implementation

WebState::Getter WebStateImpl::CreateDefaultGetter() {
  return base::BindRepeating(&ReturnWeakReference, weak_factory_.GetWeakPtr());
}

WebState::OnceGetter WebStateImpl::CreateDefaultOnceGetter() {
  return base::BindOnce(&ReturnWeakReference, weak_factory_.GetWeakPtr());
}

WebStateDelegate* WebStateImpl::GetDelegate() {
  return LIKELY(pimpl_) ? pimpl_->GetDelegate() : nullptr;
}

void WebStateImpl::SetDelegate(WebStateDelegate* delegate) {
  RealizedState()->SetDelegate(delegate);
}

bool WebStateImpl::IsRealized() const {
  return !!pimpl_;
}

WebState* WebStateImpl::ForceRealized() {
  DCHECK(!is_being_destroyed_);

  if (UNLIKELY(!pimpl_)) {
    DCHECK(saved_);
    const CreateParams params = saved_->GetCreateParams();
    CRWSessionStorage* session_storage = saved_->GetSessionStorage();
    FaviconStatus favicon_status = saved_->GetFaviconStatus();
    DCHECK(session_storage);

    // Create the RealizedWebState. At this point the WebStateImpl has
    // both `pimpl_` and `saved_` that are non-null. This is one of the
    // reason why the initialisation of the RealizedWebState needs to
    // be done after the constructor is done.
    pimpl_ = std::make_unique<RealizedWebState>(this);

    // Delete the SerializedData without calling TearDown() as the WebState
    // itself is not destroyed. The TearDown() method will be called on the
    // RealizedWebState in WebStateImpl destructor.
    saved_.reset();

    // Perform the initialisation of the RealizedWebState. No outside
    // code should be able to observe the WebStateImpl with both `saved_`
    // and `pimpl_` set.
    pimpl_->Init(params, session_storage);
    pimpl_->SetFaviconStatus(favicon_status);

    // Notify all observers that the WebState has become realized.
    for (auto& observer : observers_)
      observer.WebStateRealized(this);

    CheckForOverRealization();
  }

  return this;
}

bool WebStateImpl::IsWebUsageEnabled() const {
  return LIKELY(pimpl_) ? pimpl_->IsWebUsageEnabled() : true;
}

void WebStateImpl::SetWebUsageEnabled(bool enabled) {
  if (IsWebUsageEnabled() != enabled) {
    RealizedState()->SetWebUsageEnabled(enabled);
  }
}

UIView* WebStateImpl::GetView() {
  return LIKELY(pimpl_) ? pimpl_->GetView() : nil;
}

void WebStateImpl::DidCoverWebContent() {
  RealizedState()->DidCoverWebContent();
}

void WebStateImpl::DidRevealWebContent() {
  RealizedState()->DidRevealWebContent();
}

base::Time WebStateImpl::GetLastActiveTime() const {
  return LIKELY(pimpl_) ? pimpl_->GetLastActiveTime()
                        : saved_->GetLastActiveTime();
}

void WebStateImpl::WasShown() {
  RealizedState()->WasShown();
}

void WebStateImpl::WasHidden() {
  RealizedState()->WasHidden();
}

void WebStateImpl::SetKeepRenderProcessAlive(bool keep_alive) {
  RealizedState()->SetKeepRenderProcessAlive(keep_alive);
}

BrowserState* WebStateImpl::GetBrowserState() const {
  return LIKELY(pimpl_) ? pimpl_->GetBrowserState() : saved_->GetBrowserState();
}

void WebStateImpl::OpenURL(const WebState::OpenURLParams& params) {
  RealizedState()->OpenURL(params);
}

void WebStateImpl::Stop() {
  RealizedState()->Stop();
}

const NavigationManager* WebStateImpl::GetNavigationManager() const {
  return LIKELY(pimpl_) ? &pimpl_->GetNavigationManager() : nullptr;
}

NavigationManager* WebStateImpl::GetNavigationManager() {
  return &RealizedState()->GetNavigationManager();
}

const WebFramesManager* WebStateImpl::GetWebFramesManager() const {
  return LIKELY(pimpl_) ? &pimpl_->GetWebFramesManager() : nullptr;
}

WebFramesManager* WebStateImpl::GetWebFramesManager() {
  return &RealizedState()->GetWebFramesManager();
}

const SessionCertificatePolicyCache*
WebStateImpl::GetSessionCertificatePolicyCache() const {
  return LIKELY(pimpl_) ? &pimpl_->GetSessionCertificatePolicyCache() : nullptr;
}

SessionCertificatePolicyCache*
WebStateImpl::GetSessionCertificatePolicyCache() {
  return &RealizedState()->GetSessionCertificatePolicyCache();
}

CRWSessionStorage* WebStateImpl::BuildSessionStorage() {
  return LIKELY(pimpl_) ? pimpl_->BuildSessionStorage()
                        : saved_->GetSessionStorage();
}

CRWJSInjectionReceiver* WebStateImpl::GetJSInjectionReceiver() const {
  return LIKELY(pimpl_) ? pimpl_->GetJSInjectionReceiver() : nullptr;
}

void WebStateImpl::LoadData(NSData* data,
                            NSString* mime_type,
                            const GURL& url) {
  RealizedState()->LoadData(data, mime_type, url);
}

void WebStateImpl::ExecuteJavaScript(const std::u16string& javascript) {
  RealizedState()->ExecuteJavaScript(javascript);
}

void WebStateImpl::ExecuteJavaScript(const std::u16string& javascript,
                                     JavaScriptResultCallback callback) {
  RealizedState()->ExecuteJavaScript(javascript, std::move(callback));
}

void WebStateImpl::ExecuteUserJavaScript(NSString* javascript) {
  RealizedState()->ExecuteUserJavaScript(javascript);
}

NSString* WebStateImpl::GetStableIdentifier() const {
  return LIKELY(pimpl_) ? pimpl_->GetStableIdentifier()
                        : saved_->GetStableIdentifier();
}

const std::string& WebStateImpl::GetContentsMimeType() const {
  static std::string kEmptyString;
  return LIKELY(pimpl_) ? pimpl_->GetContentsMimeType() : kEmptyString;
}

bool WebStateImpl::ContentIsHTML() const {
  return LIKELY(pimpl_) ? pimpl_->ContentIsHTML() : false;
}

const std::u16string& WebStateImpl::GetTitle() const {
  return LIKELY(pimpl_) ? pimpl_->GetTitle() : saved_->GetTitle();
}

bool WebStateImpl::IsLoading() const {
  return LIKELY(pimpl_) ? pimpl_->IsLoading() : false;
}

double WebStateImpl::GetLoadingProgress() const {
  return LIKELY(pimpl_) ? pimpl_->GetLoadingProgress() : 0.0;
}

bool WebStateImpl::IsVisible() const {
  return LIKELY(pimpl_) ? pimpl_->IsVisible() : false;
}

bool WebStateImpl::IsCrashed() const {
  return LIKELY(pimpl_) ? pimpl_->IsCrashed() : false;
}

bool WebStateImpl::IsEvicted() const {
  return LIKELY(pimpl_) ? pimpl_->IsEvicted() : true;
}

bool WebStateImpl::IsBeingDestroyed() const {
  return is_being_destroyed_;
}

const FaviconStatus& WebStateImpl::GetFaviconStatus() const {
  return LIKELY(pimpl_) ? pimpl_->GetFaviconStatus()
                        : saved_->GetFaviconStatus();
}

void WebStateImpl::SetFaviconStatus(const FaviconStatus& favicon_status) {
  if (LIKELY(pimpl_)) {
    pimpl_->SetFaviconStatus(favicon_status);
  } else {
    saved_->SetFaviconStatus(favicon_status);
  }
}

const GURL& WebStateImpl::GetVisibleURL() const {
  return LIKELY(pimpl_) ? pimpl_->GetVisibleURL() : saved_->GetVisibleURL();
}

const GURL& WebStateImpl::GetLastCommittedURL() const {
  return LIKELY(pimpl_) ? pimpl_->GetLastCommittedURL()
                        : saved_->GetLastCommittedURL();
}

GURL WebStateImpl::GetCurrentURL(URLVerificationTrustLevel* trust_level) const {
  return LIKELY(pimpl_) ? pimpl_->GetCurrentURL(trust_level) : GURL();
}

base::CallbackListSubscription WebStateImpl::AddScriptCommandCallback(
    const ScriptCommandCallback& callback,
    const std::string& command_prefix) {
  DCHECK(!command_prefix.empty());
  DCHECK(command_prefix.find_first_of('.') == std::string::npos);
  DCHECK(script_command_callbacks_.count(command_prefix) == 0 ||
         script_command_callbacks_[command_prefix].empty());
  return script_command_callbacks_[command_prefix].Add(callback);
}

id<CRWWebViewProxy> WebStateImpl::GetWebViewProxy() const {
  return LIKELY(pimpl_) ? pimpl_->GetWebViewProxy() : nil;
}

void WebStateImpl::DidChangeVisibleSecurityState() {
  RealizedState()->DidChangeVisibleSecurityState();
}

WebState::InterfaceBinder* WebStateImpl::GetInterfaceBinderForMainFrame() {
  return RealizedState()->GetInterfaceBinderForMainFrame();
}

bool WebStateImpl::HasOpener() const {
  return LIKELY(pimpl_) ? pimpl_->HasOpener() : false;
}

void WebStateImpl::SetHasOpener(bool has_opener) {
  RealizedState()->SetHasOpener(has_opener);
}

bool WebStateImpl::CanTakeSnapshot() const {
  return LIKELY(pimpl_) ? pimpl_->CanTakeSnapshot() : false;
}

void WebStateImpl::TakeSnapshot(const gfx::RectF& rect,
                                SnapshotCallback callback) {
  RealizedState()->TakeSnapshot(rect, std::move(callback));
}

void WebStateImpl::CreateFullPagePdf(
    base::OnceCallback<void(NSData*)> callback) {
  RealizedState()->CreateFullPagePdf(std::move(callback));
}

void WebStateImpl::CloseMediaPresentations() {
  if (pimpl_) {
    pimpl_->CloseMediaPresentations();
  }
}

void WebStateImpl::AddObserver(WebStateObserver* observer) {
  observers_.AddObserver(observer);
}

void WebStateImpl::RemoveObserver(WebStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebStateImpl::CloseWebState() {
  RealizedState()->CloseWebState();
}

bool WebStateImpl::SetSessionStateData(NSData* data) {
  return RealizedState()->SetSessionStateData(data);
}

NSData* WebStateImpl::SessionStateData() {
  return LIKELY(pimpl_) ? pimpl_->SessionStateData() : nil;
}

PermissionState WebStateImpl::GetStateForPermission(
    Permission permission) const {
  return LIKELY(pimpl_) ? pimpl_->GetStateForPermission(permission)
                        : PermissionStateNotAccessible;
}

void WebStateImpl::SetStateForPermission(PermissionState state,
                                         Permission permission) {
  RealizedState()->SetStateForPermission(state, permission);
}

NSDictionary<NSNumber*, NSNumber*>* WebStateImpl::GetStatesForAllPermissions()
    const {
  return LIKELY(pimpl_) ? pimpl_->GetStatesForAllPermissions()
                        : [NSDictionary dictionary];
}

void WebStateImpl::AddPolicyDecider(WebStatePolicyDecider* decider) {
  // Despite the name, ObserverList is actually generic, so it is used for
  // deciders. This makes the call here odd looking, but it's really just
  // managing the list, not setting observers on deciders.
  policy_deciders_.AddObserver(decider);
}

void WebStateImpl::RemovePolicyDecider(WebStatePolicyDecider* decider) {
  // Despite the name, ObserverList is actually generic, so it is used for
  // deciders. This makes the call here odd looking, but it's really just
  // managing the list, not setting observers on deciders.
  policy_deciders_.RemoveObserver(decider);
}

#pragma mark - WebStateImpl private methods

WebStateImpl::RealizedWebState* WebStateImpl::RealizedState() {
  if (UNLIKELY(!IsRealized())) {
    ForceRealized();
  }

  DCHECK(pimpl_);
  return pimpl_.get();
}

}  // namespace web
