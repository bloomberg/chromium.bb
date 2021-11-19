// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_state_delegate_browser_agent.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/ui/dialogs/nsurl_protection_space_util.h"

#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_configuration_provider.h"
#import "ios/chrome/browser/web/blocked_popup_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/web_state_container_view_provider.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/web/public/ui/context_menu_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(WebStateDelegateBrowserAgent)

namespace {
// Callback for HTTP authentication dialogs. This callback is a standalone
// function rather than an instance method. This is to ensure that the callback
// can be executed regardless of whether the browser agent has been destroyed.
void OnHTTPAuthOverlayFinished(web::WebStateDelegate::AuthCallback callback,
                               OverlayResponse* response) {
  if (response) {
    HTTPAuthOverlayResponseInfo* auth_info =
        response->GetInfo<HTTPAuthOverlayResponseInfo>();
    if (auth_info) {
      std::move(callback).Run(base::SysUTF8ToNSString(auth_info->username()),
                              base::SysUTF8ToNSString(auth_info->password()));
      return;
    }
  }
  std::move(callback).Run(nil, nil);
}
}  // namespace

WebStateDelegateBrowserAgent::WebStateDelegateBrowserAgent(Browser* browser)
    : web_state_list_(browser->GetWebStateList()),
      tab_insertion_agent_(TabInsertionBrowserAgent::FromBrowser(browser)) {
  DCHECK(tab_insertion_agent_);
  browser->AddObserver(this);
  web_state_list_->AddObserver(this);
  for (int index = 0; index < web_state_list_->count(); ++index)
    web_state_list_->GetWebStateAt(index)->SetDelegate(this);
}

WebStateDelegateBrowserAgent::~WebStateDelegateBrowserAgent() {}

void WebStateDelegateBrowserAgent::SetUIProviders(
    ContextMenuConfigurationProvider* context_menu_provider,
    id<CRWResponderInputView> input_view_provider,
    id<WebStateContainerViewProvider> container_view_provider) {
  context_menu_provider_ = context_menu_provider;
  input_view_provider_ = input_view_provider;
  container_view_provider_ = container_view_provider;
}

void WebStateDelegateBrowserAgent::ClearUIProviders() {
  context_menu_provider_ = nil;
  input_view_provider_ = nil;
  container_view_provider_ = nil;
}

// WebStateListObserver::
void WebStateDelegateBrowserAgent::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  web_state->SetDelegate(this);
}

void WebStateDelegateBrowserAgent::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  old_web_state->SetDelegate(nullptr);
  new_web_state->SetDelegate(this);
}

void WebStateDelegateBrowserAgent::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  web_state->SetDelegate(nullptr);
}

// BrowserObserver::
void WebStateDelegateBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser->GetWebStateList(), web_state_list_);
  // Remove all web state delegates.
  for (int index = 0; index < web_state_list_->count(); ++index)
    web_state_list_->GetWebStateAt(index)->SetDelegate(nullptr);

  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
}

// WebStateDelegate::
web::WebState* WebStateDelegateBrowserAgent::CreateNewWebState(
    web::WebState* source,
    const GURL& url,
    const GURL& opener_url,
    bool initiated_by_user) {
  // Under some circumstances, this callback may be triggered from WebKit
  // synchronously as part of handling some other WebStateList mutation
  // (typically deleting a WebState and then activating another as a side
  // effect). See crbug.com/988504 for details. In this case, the request to
  // create a new WebState is silently dropped.
  if (web_state_list_->IsMutating())
    return nullptr;

  // Check if requested web state is a popup and block it if necessary.
  if (!initiated_by_user) {
    auto* helper = BlockedPopupTabHelper::FromWebState(source);
    if (helper->ShouldBlockPopup(opener_url)) {
      // It's possible for a page to inject a popup into a window created via
      // window.open before its initial load is committed.  Rather than relying
      // on the last committed or pending NavigationItem's referrer policy, just
      // use ReferrerPolicyDefault.
      // TODO(crbug.com/719993): Update this to a more appropriate referrer
      // policy once referrer policies are correctly recorded in
      // NavigationItems.
      web::Referrer referrer(opener_url, web::ReferrerPolicyDefault);
      helper->HandlePopup(url, referrer);
      return nullptr;
    }
  }

  // Requested web state should not be blocked from opening.
  SnapshotTabHelper::FromWebState(source)->UpdateSnapshotWithCallback(nil);

  return tab_insertion_agent_->InsertWebStateOpenedByDOM(source);
}

void WebStateDelegateBrowserAgent::CloseWebState(web::WebState* source) {
  security_interstitials::IOSBlockingPageTabHelper* helper =
      security_interstitials::IOSBlockingPageTabHelper::FromWebState(source);
  DCHECK(source->HasOpener() ||
         !source->GetNavigationManager()->GetItemCount() ||
         helper->GetCurrentBlockingPage() != nullptr);
  int index = web_state_list_->GetIndexOfWebState(source);
  if (index != WebStateList::kInvalidIndex)
    web_state_list_->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

web::WebState* WebStateDelegateBrowserAgent::OpenURLFromWebState(
    web::WebState* source,
    const web::WebState::OpenURLParams& params) {
  web::NavigationManager::WebLoadParams load_params(params.url);
  load_params.referrer = params.referrer;
  load_params.transition_type = params.transition;
  load_params.is_renderer_initiated = params.is_renderer_initiated;
  load_params.virtual_url = params.virtual_url;

  switch (params.disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB: {
      return tab_insertion_agent_->InsertWebState(
          load_params, source, false, TabInsertion::kPositionAutomatically,
          (params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB),
          /*inherit_opener=*/false);
    }
    case WindowOpenDisposition::CURRENT_TAB: {
      source->GetNavigationManager()->LoadURLWithParams(load_params);
      return source;
    }
    case WindowOpenDisposition::NEW_POPUP: {
      return tab_insertion_agent_->InsertWebState(
          load_params, source, true, TabInsertion::kPositionAutomatically,
          /*in_background=*/false, /*inherit_opener=*/false);
    }
    default:
      NOTIMPLEMENTED();
      return nullptr;
  };
}

void WebStateDelegateBrowserAgent::HandleContextMenu(
    web::WebState* source,
    const web::ContextMenuParams& params) {
  [context_menu_provider_ showLegacyContextMenuForWebState:source
                                                    params:params];
}

void WebStateDelegateBrowserAgent::ShowRepostFormWarningDialog(
    web::WebState* source,
    base::OnceCallback<void(bool)> callback) {
  if (!container_view_provider_) {
    // There's no way to show the dialog so treat it as if the user said no.
    std::move(callback).Run(false);
    return;
  }
  // TODO(crbug.com/1266052) : Clean up this API.
  RepostFormTabHelper::FromWebState(source)->PresentDialog(
      [container_view_provider_ dialogLocation], std::move(callback));
}

web::JavaScriptDialogPresenter*
WebStateDelegateBrowserAgent::GetJavaScriptDialogPresenter(
    web::WebState* source) {
  return &java_script_dialog_presenter_;
}

void WebStateDelegateBrowserAgent::OnAuthRequired(
    web::WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* proposed_credential,
    web::WebStateDelegate::AuthCallback callback) {
  std::string message = base::SysNSStringToUTF8(
      nsurlprotectionspace_util::MessageForHTTPAuth(protection_space));
  std::string default_username;
  if (proposed_credential.user)
    default_username = base::SysNSStringToUTF8(proposed_credential.user);
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<HTTPAuthOverlayRequestConfig>(
          nsurlprotectionspace_util::RequesterOrigin(protection_space), message,
          default_username);
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&OnHTTPAuthOverlayFinished, std::move(callback)));
  OverlayRequestQueue::FromWebState(source, OverlayModality::kWebContentArea)
      ->AddRequest(std::move(request));
}

UIView* WebStateDelegateBrowserAgent::GetWebViewContainer(
    web::WebState* source) {
  return [container_view_provider_ containerView];
}

void WebStateDelegateBrowserAgent::ContextMenuConfiguration(
    web::WebState* source,
    const web::ContextMenuParams& params,
    void (^completion_handler)(UIContextMenuConfiguration*)) {
  UIContextMenuConfiguration* configuration =
      [context_menu_provider_ contextMenuConfigurationForWebState:source
                                                           params:params];
  completion_handler(configuration);
}

void WebStateDelegateBrowserAgent::ContextMenuWillCommitWithAnimator(
    web::WebState* source,
    id<UIContextMenuInteractionCommitAnimating> animator) {
  [context_menu_provider_ commitPreview];
}

id<CRWResponderInputView> WebStateDelegateBrowserAgent::GetResponderInputView(
    web::WebState* source) {
  return input_view_provider_;
}
