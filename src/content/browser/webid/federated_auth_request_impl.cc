// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include "base/callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/id_token_request_callback_data.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/federated_identity_active_session_permission_context_delegate.h"
#include "content/public/browser/federated_identity_request_permission_context_delegate.h"
#include "content/public/browser/federated_identity_sharing_permission_context_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "url/url_constants.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::LogoutRpsStatus;
using blink::mojom::RequestIdTokenStatus;
using blink::mojom::RequestMode;
using blink::mojom::RevokeStatus;
using UserApproval = content::IdentityRequestDialogController::UserApproval;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using IdTokenStatus = content::FedCmRequestIdTokenStatus;
using RevokeStatusForMetrics = content::FedCmRevokeStatus;

namespace content {

namespace {
static constexpr base::TimeDelta kIdTokenRequestDelay = base::Seconds(3);

std::string FormatRequestParams(const std::string& client_id,
                                const std::string& nonce) {
  std::string query;
  // 'openid' scope is required for IdPs using OpenID Connect. We hope that IdPs
  // using OAuth will ignore it, although some might return errors.
  query += "scope=openid profile email";
  if (client_id.length() > 0) {
    query += "&client_id=" + client_id;
  }
  if (nonce.length() > 0) {
    query += "&nonce=" + nonce;
  }
  return query;
}

std::string FormatRequestParamsWithoutScope(const std::string& client_id,
                                            const std::string& nonce,
                                            const std::string& account_id,
                                            bool is_sign_in) {
  std::string query;
  if (!client_id.empty())
    query += "client_id=" + client_id;

  if (!nonce.empty()) {
    if (!query.empty())
      query += "&";
    query += "nonce=" + nonce;
  }

  if (!account_id.empty()) {
    if (!query.empty())
      query += "&";
    query += "account_id=" + account_id;
  }
  // For returning users who are signing in instead of signing up, we do not
  // show the privacy policy and terms of service on the consent sheet. This
  // field indicates in the request that whether the user has granted consent
  // after seeing the sheet with privacy policy and terms of service.
  std::string consent_acquired = is_sign_in ? "false" : "true";
  if (!query.empty())
    query += "&consent_acquired=" + consent_acquired;
  return query;
}

std::string GetConsoleErrorMessage(FederatedAuthRequestResult status) {
  switch (status) {
    case FederatedAuthRequestResult::kApprovalDeclined: {
      return "User declined the sign-in attempt.";
    }
    case FederatedAuthRequestResult::kErrorTooManyRequests: {
      return "Only one navigator.credentials.get request may be outstanding at "
             "one time.";
    }
    case FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound: {
      return "The provider's FedCM manifest configuration cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingManifestNoResponse: {
      return "The response body is empty when fetching the provider's "
             "FedCM manifest configuration.";
    }
    case FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse: {
      return "Provider's FedCM manifest configuration is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound: {
      return "The provider's client metadata endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse: {
      return "The response body is empty when fetching the provider's client "
             "metadata.";
    }
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse: {
      return "Provider's client metadata is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingSignin: {
      return "Error attempting to reach the provider's sign-in endpoint.";
    }
    case FederatedAuthRequestResult::kErrorInvalidSigninResponse: {
      return "Provider's sign-in response is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound: {
      return "The provider's accounts list endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse: {
      return "The response body is empty when fetching the provider's accounts "
             "list.";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse: {
      return "Provider's accounts list is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound: {
      return "The provider's id token endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse: {
      return "The response body is empty when fetching the provider's id "
             "token.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse: {
      return "Provider's id token is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidRequest: {
      return "The id token fetching request is invalid.";
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return "The request has been aborted.";
    }
    case FederatedAuthRequestResult::kError: {
      return "Error retrieving an id token.";
    }
    case FederatedAuthRequestResult::kSuccess: {
      DCHECK(false);
      return "";
    }
  }
}

RequestIdTokenStatus FederatedAuthRequestResultToRequestIdTokenStatus(
    FederatedAuthRequestResult result) {
  // Avoids exposing to renderer detailed error messages which may leak cross
  // site information to the API call site.
  switch (result) {
    case FederatedAuthRequestResult::kSuccess: {
      return RequestIdTokenStatus::kSuccess;
    }
    case FederatedAuthRequestResult::kApprovalDeclined: {
      return RequestIdTokenStatus::kApprovalDeclined;
    }
    case FederatedAuthRequestResult::kErrorTooManyRequests: {
      return RequestIdTokenStatus::kErrorTooManyRequests;
    }
    case FederatedAuthRequestResult::kErrorFetchingSignin: {
      return RequestIdTokenStatus::kErrorFetchingSignin;
    }
    case FederatedAuthRequestResult::kErrorInvalidSigninResponse: {
      return RequestIdTokenStatus::kErrorInvalidSigninResponse;
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return RequestIdTokenStatus::kErrorCanceled;
    }
    case FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingManifestNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse:
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse:
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidRequest:
    case FederatedAuthRequestResult::kError: {
      return RequestIdTokenStatus::kError;
    }
  }
}

}  // namespace

FederatedAuthRequestImpl::FederatedAuthRequestImpl(RenderFrameHostImpl* host,
                                                   const url::Origin& origin)
    : render_frame_host_(host), origin_(origin) {}

FederatedAuthRequestImpl::~FederatedAuthRequestImpl() {
  // Ensures key data members are destructed in proper order and resolves any
  // pending promise.
  if (auth_request_callback_) {
    DCHECK(!revoke_callback_);
    DCHECK(!logout_callback_);
    RecordRequestIdTokenStatus(IdTokenStatus::kUnhandledRequest,
                               render_frame_host_->GetPageUkmSourceId());
  }
  if (revoke_callback_) {
    DCHECK(!auth_request_callback_);
    DCHECK(!logout_callback_);
    RecordRevokeStatus(RevokeStatusForMetrics::kUnhandledRequest,
                       render_frame_host_->GetPageUkmSourceId());
  }
  CompleteRequest(FederatedAuthRequestResult::kError, "");
}

void FederatedAuthRequestImpl::RequestIdToken(
    const GURL& provider,
    const std::string& client_id,
    const std::string& nonce,
    RequestMode mode,
    bool prefer_auto_sign_in,
    blink::mojom::FederatedAuthRequest::RequestIdTokenCallback callback) {
  if (HasPendingRequest()) {
    RecordRequestIdTokenStatus(IdTokenStatus::kTooManyRequests,
                               render_frame_host_->GetPageUkmSourceId());
    std::move(callback).Run(RequestIdTokenStatus::kErrorTooManyRequests, "");
    return;
  }

  auth_request_callback_ = std::move(callback);
  provider_ = provider;
  client_id_ = client_id;
  nonce_ = nonce;
  mode_ = mode;
  prefer_auto_sign_in_ = prefer_auto_sign_in && IsFedCmAutoSigninEnabled();
  start_time_ = base::TimeTicks::Now();

  network_manager_ = CreateNetworkManager(provider);
  if (!network_manager_) {
    RecordRequestIdTokenStatus(IdTokenStatus::kNoNetworkManager,
                               render_frame_host_->GetPageUkmSourceId());
    CompleteRequest(FederatedAuthRequestResult::kError, "");
    return;
  }

  request_dialog_controller_ = CreateDialogController();

  // Skip request permission if it is already given for this IDP or if we are
  // using the mediated mode in which case the permission is combined with
  // account selector UI.
  if (mode_ == RequestMode::kMediated ||
      (GetRequestPermissionContext() &&
       GetRequestPermissionContext()->HasRequestPermission(
           origin_, url::Origin::Create(provider_)))) {
    FetchManifest(base::BindOnce(&FederatedAuthRequestImpl::OnManifestFetched,
                                 weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DCHECK_EQ(mode_, RequestMode::kPermission);
  // Use the web contents of the page that initiated the WebID request (i.e.
  // the Relying Party) for showing the initial permission dialog.
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host_);

  request_dialog_controller_->ShowInitialPermissionDialog(
      web_contents, provider_,
      IdentityRequestDialogController::PermissionDialogMode::kStateful,
      base::BindOnce(&FederatedAuthRequestImpl::OnSigninApproved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::CancelTokenRequest() {
  if (!auth_request_callback_)
    return;

  // Dialog will be hidden by the destructor for request_dialog_controller_,
  // triggered by CompleteRequest.
  RecordRequestIdTokenStatus(IdTokenStatus::kAborted,
                             render_frame_host_->GetPageUkmSourceId());
  CompleteRequest(FederatedAuthRequestResult::kErrorCanceled, "");
}

void FederatedAuthRequestImpl::Revoke(
    const GURL& provider,
    const std::string& client_id,
    const std::string& account_id,
    blink::mojom::FederatedAuthRequest::RevokeCallback callback) {
  if (HasPendingRequest()) {
    RecordRevokeStatus(RevokeStatusForMetrics::kTooManyRequests,
                       render_frame_host_->GetPageUkmSourceId());
    std::move(callback).Run(RevokeStatus::kError);
    return;
  }

  provider_ = provider;
  client_id_ = client_id;
  account_id_ = account_id;
  revoke_callback_ = std::move(callback);

  network_manager_ = CreateNetworkManager(provider);
  if (!network_manager_) {
    RecordRevokeStatus(RevokeStatusForMetrics::kNoNetworkManager,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(RevokeStatus::kError);
    return;
  }

  if (!GetRequestPermissionContext() ||
      !GetRequestPermissionContext()->HasRequestPermission(
          origin_, url::Origin::Create(provider_))) {
    RecordRevokeStatus(RevokeStatusForMetrics::kNoAccountToRevoke,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(RevokeStatus::kError);
    return;
  }

  FetchManifest(
      base::BindOnce(&FederatedAuthRequestImpl::OnManifestFetchedForRevoke,
                     weak_ptr_factory_.GetWeakPtr()));
}

// TODO(kenrb): Depending on how this code evolves, it might make sense to
// spin session management code into its own service. The prohibition on
// making authentication requests and logout requests at the same time, while
// not problematic for any plausible use case, need not be strictly necessary
// if there is a good way to not have to resource contention between requests.
// https://crbug.com/1200581
void FederatedAuthRequestImpl::LogoutRps(
    std::vector<blink::mojom::LogoutRpsRequestPtr> logout_requests,
    blink::mojom::FederatedAuthRequest::LogoutRpsCallback callback) {
  if (HasPendingRequest()) {
    std::move(callback).Run(LogoutRpsStatus::kErrorTooManyRequests);
    return;
  }

  DCHECK(logout_requests_.empty());

  logout_callback_ = std::move(callback);

  if (logout_requests.empty()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (base::ranges::any_of(logout_requests, [](auto& request) {
        return !request->url.is_valid();
      })) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::FARI_LOGOUT_BAD_ENDPOINT);
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  for (auto& request : logout_requests) {
    logout_requests_.push(std::move(request));
  }

  network_manager_ = CreateNetworkManager(origin_.GetURL());
  if (!network_manager_) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  // TODO(kenrb): These should be parallelized rather than being dispatched
  // serially. https://crbug.com/1200581.
  DispatchOneLogout();
}

bool FederatedAuthRequestImpl::HasPendingRequest() const {
  return auth_request_callback_ || logout_callback_ || revoke_callback_;
}

GURL FederatedAuthRequestImpl::ResolveManifestUrl(const std::string& endpoint) {
  if (endpoint.empty())
    return GURL();
  const url::Origin& idp_origin = url::Origin::Create(provider_);
  GURL manifest_url =
      idp_origin.GetURL().Resolve(IdpNetworkRequestManager::kManifestFilePath);
  return manifest_url.Resolve(endpoint);
}

bool FederatedAuthRequestImpl::IsEndpointUrlValid(const GURL& endpoint_url) {
  return url::Origin::Create(provider_).IsSameOriginWith(endpoint_url);
}

void FederatedAuthRequestImpl::FetchManifest(
    IdpNetworkRequestManager::FetchManifestCallback callback) {
  absl::optional<int> icon_ideal_size = absl::nullopt;
  absl::optional<int> icon_minimum_size = absl::nullopt;
  if (request_dialog_controller_) {
    icon_ideal_size = request_dialog_controller_->GetBrandIconIdealSize();
    icon_minimum_size = request_dialog_controller_->GetBrandIconMinimumSize();
  }

  network_manager_->FetchManifest(icon_ideal_size, icon_minimum_size,
                                  std::move(callback));
}

void FederatedAuthRequestImpl::OnManifestFetched(
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata idp_metadata) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kManifestHttpNotFound,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingManifestHttpNotFound, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kManifestNoResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingManifestNoResponse, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kManifestInvalidResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
          "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidRequestError: {
      NOTREACHED();
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Intentional fall-through.
    }
  }

  endpoints_.idp = ResolveManifestUrl(endpoints.idp);
  endpoints_.token = ResolveManifestUrl(endpoints.token);
  endpoints_.accounts = ResolveManifestUrl(endpoints.accounts);
  endpoints_.client_metadata = ResolveManifestUrl(endpoints.client_metadata);

  switch (mode_) {
    case RequestMode::kMediated: {
      // For Mediated mode we require accounts, token and client ID endpoints.
      if (endpoints_.token.is_empty() || endpoints_.accounts.is_empty() ||
          endpoints_.client_metadata.is_empty()) {
        RecordRequestIdTokenStatus(IdTokenStatus::kManifestInvalidResponse,
                                   render_frame_host_->GetPageUkmSourceId());
        CompleteRequest(
            FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
            "");
        return;
      }
      if (!IsEndpointUrlValid(endpoints_.token) ||
          !IsEndpointUrlValid(endpoints_.accounts) ||
          !IsEndpointUrlValid(endpoints_.client_metadata)) {
        RecordRequestIdTokenStatus(IdTokenStatus::kManifestInvalidResponse,
                                   render_frame_host_->GetPageUkmSourceId());
        CompleteRequest(
            FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
            "");
        return;
      }
      GURL brand_icon_url = idp_metadata.brand_icon_url;
      DownloadBitmap(
          brand_icon_url, request_dialog_controller_->GetBrandIconIdealSize(),
          base::BindOnce(&FederatedAuthRequestImpl::OnBrandIconDownloaded,
                         weak_ptr_factory_.GetWeakPtr(),
                         request_dialog_controller_->GetBrandIconMinimumSize(),
                         std::move(idp_metadata)));
      break;
    }
    case RequestMode::kPermission: {
      // For Permission mode we require both accounts and token endpoints.
      if (endpoints_.idp.is_empty()) {
        CompleteRequest(
            FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
            "");
        return;
      }
      if (!IsEndpointUrlValid(endpoints_.idp)) {
        CompleteRequest(
            FederatedAuthRequestResult::kErrorFetchingManifestInvalidResponse,
            "");
        return;
      }

      network_manager_->SendSigninRequest(
          endpoints_.idp, FormatRequestParams(client_id_, nonce_),
          base::BindOnce(&FederatedAuthRequestImpl::OnSigninResponseReceived,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    }
  }
}

void FederatedAuthRequestImpl::OnManifestFetchedForRevoke(
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata idp_metadata) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      RecordRevokeStatus(RevokeStatusForMetrics::kManifestHttpNotFound,
                         render_frame_host_->GetPageUkmSourceId());
      CompleteRevokeRequest(RevokeStatus::kError);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      RecordRevokeStatus(RevokeStatusForMetrics::kManifestNoResponse,
                         render_frame_host_->GetPageUkmSourceId());
      CompleteRevokeRequest(RevokeStatus::kError);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      RecordRevokeStatus(RevokeStatusForMetrics::kManifestInvalidResponse,
                         render_frame_host_->GetPageUkmSourceId());
      CompleteRevokeRequest(RevokeStatus::kError);
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidRequestError: {
      NOTREACHED();
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      // Intentional fall-through.
    }
  }

  GURL revoke_url = ResolveManifestUrl(endpoints.revoke);
  if (!IsEndpointUrlValid(revoke_url)) {
    RecordRevokeStatus(RevokeStatusForMetrics::kRevokeUrlIsCrossOrigin,
                       render_frame_host_->GetPageUkmSourceId());
    CompleteRevokeRequest(RevokeStatus::kError);
    return;
  }
  network_manager_->SendRevokeRequest(
      revoke_url, client_id_, account_id_,
      base::BindOnce(&FederatedAuthRequestImpl::OnRevokeResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnRevokeResponse(
    IdpNetworkRequestManager::RevokeResponse response) {
  RevokeStatus status =
      response == IdpNetworkRequestManager::RevokeResponse::kSuccess
          ? RevokeStatus::kSuccess
          : RevokeStatus::kError;
  if (status == RevokeStatus::kSuccess) {
    url::Origin idp_origin{url::Origin::Create(provider_)};
    // Since the account is now deleted, revoke the permission.
    if (GetRequestPermissionContext()) {
      GetRequestPermissionContext()->RevokeRequestPermission(origin_,
                                                             idp_origin);
    }
    if (GetSharingPermissionContext()) {
      GetSharingPermissionContext()->RevokeSharingPermissionForAccount(
          idp_origin, origin_, account_id_);
    }
    if (GetActiveSessionPermissionContext()) {
      GetActiveSessionPermissionContext()->RevokeActiveSession(
          origin_, idp_origin, account_id_);
    }
    RecordRevokeStatus(RevokeStatusForMetrics::kSuccess,
                       render_frame_host_->GetPageUkmSourceId());
  } else {
    RecordRevokeStatus(RevokeStatusForMetrics::kRevocationFailedOnServer,
                       render_frame_host_->GetPageUkmSourceId());
  }
  CompleteRevokeRequest(status);
}

void FederatedAuthRequestImpl::CompleteRevokeRequest(RevokeStatus status) {
  network_manager_.reset();
  provider_ = GURL();
  account_id_ = std::string();
  client_id_ = std::string();
  if (revoke_callback_)
    std::move(revoke_callback_).Run(status);
}

void FederatedAuthRequestImpl::OnBrandIconDownloaded(
    int icon_minimum_size,
    IdentityProviderMetadata idp_metadata,
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  // For the sake of FedCM spec simplicity do not support multi-resolution .ico
  // files.
  if (bitmaps.size() == 1 && bitmaps[0].width() == bitmaps[0].height() &&
      bitmaps[0].width() >= icon_minimum_size) {
    idp_metadata.brand_icon = bitmaps[0];
  }

  network_manager_->FetchClientMetadata(
      endpoints_.client_metadata, client_id_,
      base::BindOnce(
          &FederatedAuthRequestImpl::OnClientMetadataResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(idp_metadata)));
}

void FederatedAuthRequestImpl::OnClientMetadataResponseReceived(
    IdentityProviderMetadata idp_metadata,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::ClientMetadata data) {
  // We purposefully do not check status; client metadata is optional.
  client_metadata_ = data;

  network_manager_->SendAccountsRequest(
      endpoints_.accounts, client_id_,
      base::BindOnce(&FederatedAuthRequestImpl::OnAccountsResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), idp_metadata));
}

void FederatedAuthRequestImpl::OnSigninApproved(
    IdentityRequestDialogController::UserApproval approval) {
  if (approval != IdentityRequestDialogController::UserApproval::kApproved) {
    CompleteRequest(FederatedAuthRequestResult::kApprovalDeclined, "");
    return;
  }

  if (GetRequestPermissionContext()) {
    GetRequestPermissionContext()->GrantRequestPermission(
        origin_, url::Origin::Create(provider_));
  }

  FetchManifest(base::BindOnce(&FederatedAuthRequestImpl::OnManifestFetched,
                               weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnSigninResponseReceived(
    IdpNetworkRequestManager::SigninResponse status,
    const std::string& url_or_token) {
  // |url_or_token| is either the URL for the sign-in page or the ID token,
  // depending on |status|.
  switch (status) {
    case IdpNetworkRequestManager::SigninResponse::kLoadIdp: {
      GURL idp_signin_page_url = endpoints_.idp.Resolve(url_or_token);
      if (!IsEndpointUrlValid(idp_signin_page_url)) {
        CompleteRequest(FederatedAuthRequestResult::kError, "");
        return;
      }
      WebContents* rp_web_contents =
          WebContents::FromRenderFrameHost(render_frame_host_);

      DCHECK(!idp_web_contents_);
      idp_web_contents_ = CreateIdpWebContents();
      request_dialog_controller_->ShowIdProviderWindow(
          rp_web_contents, idp_web_contents_.get(), idp_signin_page_url,
          base::BindOnce(&FederatedAuthRequestImpl::OnIdpPageClosed,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    case IdpNetworkRequestManager::SigninResponse::kTokenGranted: {
      // TODO(kenrb): Returning success here has to be dependent on whether
      // a WebID flow has succeeded in the past, otherwise jump to
      // the token permission dialog.
      CompleteRequest(FederatedAuthRequestResult::kSuccess, url_or_token);
      return;
    }
    case IdpNetworkRequestManager::SigninResponse::kSigninError: {
      CompleteRequest(FederatedAuthRequestResult::kErrorFetchingSignin, "");
      return;
    }
    case IdpNetworkRequestManager::SigninResponse::kInvalidResponseError: {
      CompleteRequest(FederatedAuthRequestResult::kErrorInvalidSigninResponse,
                      "");
      return;
    }
  }
}

void FederatedAuthRequestImpl::OnTokenProvided(const std::string& id_token) {
  id_token_ = id_token;

  // Close the IDP window which leads to OnIdpPageClosed which is our common.
  //
  // TODO(majidvp): Consider if we should not wait on the IDP window closing and
  // instead should directly call `OnIdpPageClosed` here.
  request_dialog_controller_->CloseIdProviderWindow();

  // Note that we always process the token on `OnIdpPageClosed()`.
  // It is possible to get there either via:
  //  (a) IDP providing a token as shown below, or
  //  (b) User closing the sign-in window.
  //
  // +-----------------------+     +-------------------+     +-----------------+
  // | FederatedAuthRequest  |     | DialogController  |     | IDPWebContents  |
  // +-----------------------+     +-------------------+     +-----------------+
  //             |                           |                        |
  //             | ShowIdProviderWindow()    |                        |
  //             |-------------------------->|                        |
  //             |                           |                        |
  //             |                           | navigate to idp.com    |
  //             |                           |----------------------->|
  //             |                           |                        |
  //             |                           |  OnTokenProvided(token)|
  //             |<---------------------------------------------------|
  //             |                           |                        |
  //             | CloseIdProviderWindow()   |                        |
  //             |-------------------------->|                        |
  //             |                           |                        |
  //             |                    closed |                        |
  //             |<--------------------------|                        |
  //             |                           |                        |
  //     OnIdpPageClosed()                   |                        |
  //             |                           |                        |
  //
}

void FederatedAuthRequestImpl::OnIdpPageClosed() {
  // This could happen if provider didn't provide any token or user closed the
  // IdP window before it could.
  if (id_token_.empty()) {
    CompleteRequest(FederatedAuthRequestResult::kError, "");
    return;
  }

  WebContents* rp_web_contents =
      WebContents::FromRenderFrameHost(render_frame_host_);

  if (GetSharingPermissionContext() &&
      GetSharingPermissionContext()->HasSharingPermission(
          url::Origin::Create(provider_), origin_)) {
    CompleteRequest(FederatedAuthRequestResult::kSuccess, id_token_);
    return;
  }

  request_dialog_controller_->ShowTokenExchangePermissionDialog(
      rp_web_contents, provider_,
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenProvisionApproved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnTokenProvisionApproved(
    IdentityRequestDialogController::UserApproval approval) {
  if (approval != IdentityRequestDialogController::UserApproval::kApproved) {
    CompleteRequest(FederatedAuthRequestResult::kApprovalDeclined, "");
    return;
  }

  if (GetSharingPermissionContext()) {
    // Grant sharing permission for RP/IDP pair without a specific account.
    GetSharingPermissionContext()->GrantSharingPermission(
        url::Origin::Create(provider_), origin_);
  }

  CompleteRequest(FederatedAuthRequestResult::kSuccess, id_token_);
}

void FederatedAuthRequestImpl::DownloadBitmap(
    const GURL& icon_url,
    int ideal_icon_size,
    WebContents::ImageDownloadCallback callback) {
  if (!icon_url.is_valid()) {
    std::move(callback).Run(0 /* id */, 404 /* http_status_code */, icon_url,
                            std::vector<SkBitmap>(), std::vector<gfx::Size>());
    return;
  }

  WebContents::FromRenderFrameHost(render_frame_host_)
      ->DownloadImage(icon_url, /*is_favicon*/ false,
                      gfx::Size(ideal_icon_size, ideal_icon_size),
                      0 /* max_bitmap_size */, false /* bypass_cache */,
                      std::move(callback));
}

void FederatedAuthRequestImpl::OnAccountsResponseReceived(
    IdentityProviderMetadata idp_metadata,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::AccountList accounts) {
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kAccountsHttpNotFound,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kAccountsNoResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kAccountsInvalidResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse,
          "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      WebContents* rp_web_contents =
          WebContents::FromRenderFrameHost(render_frame_host_);
      bool is_visible = rp_web_contents && (rp_web_contents->GetVisibility() ==
                                            Visibility::VISIBLE);
      RecordWebContentsVisibilityUponReadyToShowDialog(is_visible);
      // Does not show the dialog if the user has left the page. e.g. they may
      // open a new tab before browser is ready to show the dialog.
      if (!is_visible) {
        CompleteRequest(FederatedAuthRequestResult::kError, "");
        return;
      }

      // Populate the accounts login state.
      for (auto& account : accounts) {
        // We set the login state based on the IDP response if it sends
        // back an approved_clients list. If it does not, we need to set
        // it here based on browser state.
        if (account.login_state)
          continue;
        LoginState login_state = LoginState::kSignUp;
        // Consider this a sign-in if we have seen a successful sign-up for
        // this account before.
        if (GetSharingPermissionContext() &&
            GetSharingPermissionContext()->HasSharingPermissionForAccount(
                url::Origin::Create(provider_), origin_, account.id)) {
          login_state = LoginState::kSignIn;
        }
        account.login_state = login_state;
      }

      DCHECK(!idp_web_contents_);
      idp_web_contents_ = CreateIdpWebContents();
      bool screen_reader_is_on =
          rp_web_contents->GetAccessibilityMode().has_mode(
              ui::AXMode::kScreenReader);
      // Auto signs in returning users if they have a single account and are
      // signing in.
      // TODO(yigu): Add additional controls for RP/IDP/User for this flow.
      // https://crbug.com/1236678.
      bool is_auto_sign_in = prefer_auto_sign_in_ && accounts.size() == 1 &&
                             accounts[0].login_state == LoginState::kSignIn &&
                             !screen_reader_is_on;
      // TODO(cbiesinger): Check that the URLs are valid.
      ClientIdData data{GURL(client_metadata_.terms_of_service_url),
                        GURL(client_metadata_.privacy_policy_url)};
      show_accounts_dialog_time_ = base::TimeTicks::Now();
      RecordShowAccountsDialogTime(show_accounts_dialog_time_ - start_time_,
                                   render_frame_host_->GetPageUkmSourceId());

      request_dialog_controller_->ShowAccountsDialog(
          rp_web_contents, idp_web_contents_.get(), provider_, accounts,
          idp_metadata, data,
          is_auto_sign_in ? SignInMode::kAuto : SignInMode::kExplicit,
          base::BindOnce(&FederatedAuthRequestImpl::OnAccountSelected,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidRequestError: {
      NOTREACHED();
    }
  }
}

void FederatedAuthRequestImpl::OnAccountSelected(const std::string& account_id,
                                                 bool is_sign_in) {
  // This could happen if user didn't select any accounts.
  if (account_id.empty()) {
    base::TimeTicks dismiss_dialog_time = base::TimeTicks::Now();
    RecordCancelOnDialogTime(dismiss_dialog_time - show_accounts_dialog_time_,
                             render_frame_host_->GetPageUkmSourceId());
    RecordRequestIdTokenStatus(IdTokenStatus::kNotSelectAccount,
                               render_frame_host_->GetPageUkmSourceId());
    CompleteRequest(FederatedAuthRequestResult::kError, "");
    return;
  }

  // Account selection is considered sufficient for granting request permission
  // (which also implies the logout permission).
  if (GetRequestPermissionContext()) {
    GetRequestPermissionContext()->GrantRequestPermission(
        origin_, url::Origin::Create(provider_));
  }

  account_id_ = account_id;
  select_account_time_ = base::TimeTicks::Now();
  RecordContinueOnDialogTime(select_account_time_ - show_accounts_dialog_time_,
                             render_frame_host_->GetPageUkmSourceId());

  network_manager_->SendTokenRequest(
      endpoints_.token, account_id_,
      FormatRequestParamsWithoutScope(client_id_, nonce_, account_id,
                                      is_sign_in),
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenResponseReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FederatedAuthRequestImpl::OnTokenResponseReceived(
    IdpNetworkRequestManager::FetchStatus status,
    const std::string& id_token) {
  // When fetching id tokens we show a "Verify" sheet to users in case fetching
  // takes a long time due to latency etc.. In case that the fetching process is
  // fast, we still want to show the "Verify" sheet for at least
  // |kIdTokenRequestDelay| seconds for better UX.
  id_token_response_time_ = base::TimeTicks::Now();
  base::TimeDelta fetch_time = id_token_response_time_ - select_account_time_;
  if (fetch_time >= kIdTokenRequestDelay) {
    CompleteIdTokenRequest(status, id_token);
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FederatedAuthRequestImpl::CompleteIdTokenRequest,
                     weak_ptr_factory_.GetWeakPtr(), status, id_token),
      kIdTokenRequestDelay - fetch_time);
}

void FederatedAuthRequestImpl::CompleteIdTokenRequest(
    IdpNetworkRequestManager::FetchStatus status,
    const std::string& id_token) {
  DCHECK(!start_time_.is_null());
  switch (status) {
    case IdpNetworkRequestManager::FetchStatus::kHttpNotFoundError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kIdTokenHttpNotFound,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kNoResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kIdTokenNoResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidRequestError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kIdTokenInvalidRequest,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidRequest, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kInvalidResponseError: {
      RecordRequestIdTokenStatus(IdTokenStatus::kIdTokenInvalidResponse,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(
          FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse, "");
      return;
    }
    case IdpNetworkRequestManager::FetchStatus::kSuccess: {
      if (GetSharingPermissionContext()) {
        DCHECK_EQ(mode_, RequestMode::kMediated);
        // Grant sharing permission specific to *this account*.
        //
        // TODO(majidvp): But wait which account?
        //   1) The account that user selected in our UI (i.e., account_id_) or
        //   2) The one for which the IDP generated a token.
        //
        // Ideally these are one and the same but currently there is no
        // enforcement for that equality so they could be different. In the
        // future we may want to enforce that the token account (aka subject)
        // matches the user selected account. But for now these questions are
        // moot since we don't actually inspect the returned idtoken.
        // https://crbug.com/1199088
        CHECK(!account_id_.empty());
        GetSharingPermissionContext()->GrantSharingPermissionForAccount(
            url::Origin::Create(provider_), origin_, account_id_);
      }

      if (GetActiveSessionPermissionContext()) {
        GetActiveSessionPermissionContext()->GrantActiveSession(
            origin_, url::Origin::Create(provider_), account_id_);
      }

      RecordIdTokenResponseAndTurnaroundTime(
          id_token_response_time_ - select_account_time_,
          id_token_response_time_ - start_time_,
          render_frame_host_->GetPageUkmSourceId());
      id_token_ = id_token;
      RecordRequestIdTokenStatus(IdTokenStatus::kSuccess,
                                 render_frame_host_->GetPageUkmSourceId());
      CompleteRequest(FederatedAuthRequestResult::kSuccess, id_token_);
      return;
    }
  }
}

void FederatedAuthRequestImpl::DispatchOneLogout() {
  auto logout_request = std::move(logout_requests_.front());
  DCHECK(logout_request->url.is_valid());
  std::string account_id = logout_request->account_id;
  auto logout_origin = url::Origin::Create(logout_request->url);
  logout_requests_.pop();

  if (!GetActiveSessionPermissionContext()) {
    CompleteLogoutRequest(LogoutRpsStatus::kError);
    return;
  }

  if (GetActiveSessionPermissionContext()->HasActiveSession(
          logout_origin, origin_, account_id)) {
    network_manager_->SendLogout(
        logout_request->url,
        base::BindOnce(&FederatedAuthRequestImpl::OnLogoutCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
    GetActiveSessionPermissionContext()->RevokeActiveSession(
        logout_origin, origin_, account_id);
  } else {
    if (logout_requests_.empty()) {
      CompleteLogoutRequest(LogoutRpsStatus::kSuccess);
      return;
    }

    DispatchOneLogout();
  }
}

void FederatedAuthRequestImpl::OnLogoutCompleted() {
  if (logout_requests_.empty()) {
    CompleteLogoutRequest(LogoutRpsStatus::kSuccess);
    return;
  }

  DispatchOneLogout();
}

std::unique_ptr<WebContents> FederatedAuthRequestImpl::CreateIdpWebContents() {
  auto idp_web_contents = content::WebContents::Create(
      WebContents::CreateParams(render_frame_host_->GetBrowserContext()));

  // Store the callback on the provider web contents so that it can be
  // used later.
  IdTokenRequestCallbackData::Set(
      idp_web_contents.get(),
      base::BindOnce(&FederatedAuthRequestImpl::OnTokenProvided,
                     weak_ptr_factory_.GetWeakPtr()));
  return idp_web_contents;
}

void FederatedAuthRequestImpl::CompleteRequest(
    blink::mojom::FederatedAuthRequestResult result,
    const std::string& id_token) {
  DCHECK(result == FederatedAuthRequestResult::kSuccess || id_token.empty());

  if (result != FederatedAuthRequestResult::kSuccess) {
    // It would be possible to add this inspector issue on the renderer, which
    // will receive the callback. However, it is preferable to do so on the
    // browser because this is closer to the source, which means adding
    // additional metadata is easier. In addition, in the future we may only
    // need to pass a small amount of information to the renderer in the case of
    // an error, so it would be cleaner to do this by reporting the inspector
    // issue from the browser.
    AddInspectorIssue(result);
    AddConsoleErrorMessage(result);
  }

  CleanUp();

  if (!auth_request_callback_)
    return;

  RequestIdTokenStatus status =
      FederatedAuthRequestResultToRequestIdTokenStatus(result);
  std::move(auth_request_callback_).Run(status, id_token);
}

void FederatedAuthRequestImpl::CleanUp() {
  request_dialog_controller_.reset();
  network_manager_.reset();
  // Given that |request_dialog_controller_| has reference to this web content
  // instance we destroy that first.
  idp_web_contents_.reset();
  account_id_ = std::string();
  start_time_ = base::TimeTicks();
  show_accounts_dialog_time_ = base::TimeTicks();
  select_account_time_ = base::TimeTicks();
  id_token_response_time_ = base::TimeTicks();
}

void FederatedAuthRequestImpl::AddInspectorIssue(
    FederatedAuthRequestResult result) {
  DCHECK_NE(result, FederatedAuthRequestResult::kSuccess);
  auto details = blink::mojom::InspectorIssueDetails::New();
  auto federated_auth_request_details =
      blink::mojom::FederatedAuthRequestIssueDetails::New(result);
  details->federated_auth_request_details =
      std::move(federated_auth_request_details);
  render_frame_host_->ReportInspectorIssue(
      blink::mojom::InspectorIssueInfo::New(
          blink::mojom::InspectorIssueCode::kFederatedAuthRequestIssue,
          std::move(details)));
}

void FederatedAuthRequestImpl::AddConsoleErrorMessage(
    FederatedAuthRequestResult result) {
  std::string message = GetConsoleErrorMessage(result);
  render_frame_host_->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, message);
}

void FederatedAuthRequestImpl::CompleteLogoutRequest(
    blink::mojom::LogoutRpsStatus status) {
  network_manager_.reset();
  base::queue<blink::mojom::LogoutRpsRequestPtr>().swap(logout_requests_);
  if (logout_callback_)
    std::move(logout_callback_).Run(status);
}

std::unique_ptr<IdpNetworkRequestManager>
FederatedAuthRequestImpl::CreateNetworkManager(const GURL& provider) {
  if (mock_network_manager_)
    return std::move(mock_network_manager_);

  return IdpNetworkRequestManager::Create(provider, render_frame_host_);
}

std::unique_ptr<IdentityRequestDialogController>
FederatedAuthRequestImpl::CreateDialogController() {
  if (mock_dialog_controller_)
    return std::move(mock_dialog_controller_);

  return GetContentClient()->browser()->CreateIdentityRequestDialogController();
}

void FederatedAuthRequestImpl::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> manager) {
  mock_network_manager_ = std::move(manager);
}

void FederatedAuthRequestImpl::SetDialogControllerForTests(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  mock_dialog_controller_ = std::move(controller);
}

void FederatedAuthRequestImpl::SetActiveSessionPermissionDelegateForTests(
    FederatedIdentityActiveSessionPermissionContextDelegate*
        active_session_permission_delegate) {
  active_session_permission_delegate_ = active_session_permission_delegate;
}

void FederatedAuthRequestImpl::SetRequestPermissionDelegateForTests(
    FederatedIdentityRequestPermissionContextDelegate*
        request_permission_delegate) {
  request_permission_delegate_ = request_permission_delegate;
}

void FederatedAuthRequestImpl::SetSharingPermissionDelegateForTests(
    FederatedIdentitySharingPermissionContextDelegate*
        sharing_permission_delegate) {
  sharing_permission_delegate_ = sharing_permission_delegate;
}

FederatedIdentityActiveSessionPermissionContextDelegate*
FederatedAuthRequestImpl::GetActiveSessionPermissionContext() {
  if (!active_session_permission_delegate_) {
    active_session_permission_delegate_ =
        render_frame_host_->GetBrowserContext()
            ->GetFederatedIdentityActiveSessionPermissionContext();
  }
  return active_session_permission_delegate_;
}

FederatedIdentityRequestPermissionContextDelegate*
FederatedAuthRequestImpl::GetRequestPermissionContext() {
  if (!request_permission_delegate_) {
    request_permission_delegate_ =
        render_frame_host_->GetBrowserContext()
            ->GetFederatedIdentityRequestPermissionContext();
  }
  return request_permission_delegate_;
}

FederatedIdentitySharingPermissionContextDelegate*
FederatedAuthRequestImpl::GetSharingPermissionContext() {
  if (!sharing_permission_delegate_) {
    sharing_permission_delegate_ =
        render_frame_host_->GetBrowserContext()
            ->GetFederatedIdentitySharingPermissionContext();
  }
  return sharing_permission_delegate_;
}

}  // namespace content
