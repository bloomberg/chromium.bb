// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

class SkBitmap;

namespace net {
enum class ReferrerPolicy;
}

namespace network {
class SimpleURLLoader;
}

namespace content {

class RenderFrameHost;

// Manages network requests and maintains relevant state for interaction with
// the Identity Provider across a FedCM transaction. Owned by
// FederatedAuthRequestImpl and has a lifetime limited to a single identity
// transaction between an RP and an IDP.
//
// Diagram of the permission-based data flows between the browser and the IDP:
//  .-------.                           .---.
//  |Browser|                           |IDP|
//  '-------'                           '---'
//      |                                 |
//      |     GET /.well-known/fedcm      |
//      |-------------------------------->|
//      |                                 |
//      |        JSON{idp_url}            |
//      |<--------------------------------|
//      |                                 |
//      | POST /idp_url with OIDC request |
//      |-------------------------------->|
//      |                                 |
//      |      id_token or signin_url     |
//      |<--------------------------------|
//  .-------.                           .---.
//  |Browser|                           |IDP|
//  '-------'                           '---'
//
// If the IDP returns an id_token, the sequence finishes. If it returns a
// signin_url, that URL is loaded as a rendered Document into a new window
// for the user to interact with the IDP.
class CONTENT_EXPORT IdpNetworkRequestManager {
 public:
  enum class FetchStatus {
    kSuccess,
    kHttpNotFoundError,
    kNoResponseError,
    kInvalidResponseError,
    kInvalidRequestError,
  };

  enum class SigninResponse {
    kLoadIdp,
    kTokenGranted,
    kSigninError,
    kInvalidResponseError,
  };

  enum class LogoutResponse {
    kSuccess,
    kError,
  };

  enum class RevokeResponse {
    kSuccess,
    kError,
  };

  struct CONTENT_EXPORT Endpoints {
    Endpoints();
    ~Endpoints();
    Endpoints(const Endpoints&);

    std::string idp;
    std::string token;
    std::string accounts;
    std::string client_id_metadata;
    std::string revoke;
  };

  struct ClientIdMetadata {
    std::string privacy_policy_url;
    std::string terms_of_service_url;
  };

  static constexpr char kWellKnownFilePath[] = ".well-known/fedcm";

  using AccountList = std::vector<content::IdentityRequestAccount>;
  using BrandIconDownloader =
      base::OnceCallback<void(const GURL& /*icon_url*/,
                              int /*ideal_icon_size*/,
                              WebContents::ImageDownloadCallback)>;
  using FetchWellKnownCallback =
      base::OnceCallback<void(FetchStatus, Endpoints)>;
  using FetchClientIdMetadataCallback =
      base::OnceCallback<void(FetchStatus, ClientIdMetadata)>;
  using SigninRequestCallback =
      base::OnceCallback<void(SigninResponse, const std::string&)>;
  using AccountsRequestCallback = base::OnceCallback<
      void(FetchStatus, AccountList, IdentityProviderMetadata)>;
  using TokenRequestCallback =
      base::OnceCallback<void(FetchStatus, const std::string&)>;
  using RevokeCallback = base::OnceCallback<void(RevokeResponse)>;
  using LogoutCallback = base::OnceCallback<void()>;

  static std::unique_ptr<IdpNetworkRequestManager> Create(
      const GURL& provider,
      RenderFrameHost* host);

  IdpNetworkRequestManager(
      const GURL& provider,
      const url::Origin& relying_party,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);

  virtual ~IdpNetworkRequestManager();

  IdpNetworkRequestManager(const IdpNetworkRequestManager&) = delete;
  IdpNetworkRequestManager& operator=(const IdpNetworkRequestManager&) = delete;

  // Attempt to fetch the IDP's WebID parameters from the its .well-known file.
  virtual void FetchIdpWellKnown(FetchWellKnownCallback);

  virtual void FetchClientIdMetadata(const GURL& endpoint,
                                     const std::string& client_id,
                                     FetchClientIdMetadataCallback);

  // Transmit the OAuth request to the IDP.
  virtual void SendSigninRequest(const GURL& signin_url,
                                 const std::string& request,
                                 SigninRequestCallback);

  // Fetch accounts list for this user from the IDP.
  virtual void SendAccountsRequest(const GURL& accounts_url,
                                   int idp_brand_icon_ideal_size,
                                   int idp_brand_icon_minimum_size,
                                   BrandIconDownloader icon_downloader,
                                   AccountsRequestCallback callback);

  // Request a new token for this user account and RP from the IDP.
  virtual void SendTokenRequest(const GURL& token_url,
                                const std::string& account,
                                const std::string& request,
                                TokenRequestCallback callback);

  // Send a revoke token request to the IDP.
  virtual void SendRevokeRequest(const GURL& revoke_url,
                                 const std::string& client_id,
                                 const std::string& account_id,
                                 RevokeCallback callback);

  // Send logout request to a single target.
  virtual void SendLogout(const GURL& logout_url, LogoutCallback);

 private:
  struct AccountRequestInfo {
    AccountRequestInfo(AccountsRequestCallback callback,
                       int idp_brand_icon_ideal_size,
                       int idp_brand_icon_minimum_size,
                       BrandIconDownloader idp_brand_icon_downloader);
    ~AccountRequestInfo();
    AccountRequestInfo(AccountRequestInfo&&);

    AccountsRequestCallback callback;
    int idp_brand_icon_ideal_size;
    int idp_brand_icon_minimum_size;
    BrandIconDownloader idp_brand_icon_downloader;
  };

  void OnWellKnownLoaded(std::unique_ptr<std::string> response_body);
  void OnWellKnownParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnClientIdMetadataLoaded(std::unique_ptr<std::string> response_body);
  void OnClientIdMetadataParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnSigninRequestResponse(std::unique_ptr<std::string> response_body);
  void OnSigninRequestParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnAccountsRequestResponse(AccountRequestInfo request_info,
                                 std::unique_ptr<std::string> response_body);
  void OnAccountsRequestParsed(AccountRequestInfo request_info,
                               data_decoder::DataDecoder::ValueOrError result);
  void OnIdentityProviderBrandIconFetched(AccountRequestInfo request_info,
                                          AccountList account_list,
                                          IdentityProviderMetadata idp_metadata,
                                          int id,
                                          int http_status_code,
                                          const GURL& image_url,
                                          const std::vector<SkBitmap>& bitmaps,
                                          const std::vector<gfx::Size>& sizes);
  void OnTokenRequestResponse(std::unique_ptr<std::string> response_body);
  void OnTokenRequestParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnRevokeResponse(std::unique_ptr<std::string> response_body);
  void OnLogoutCompleted(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> CreateUncredentialedUrlLoader(
      const GURL& url) const;
  std::unique_ptr<network::SimpleURLLoader> CreateCredentialedUrlLoader(
      const GURL& url,
      absl::optional<std::string> request_body = absl::nullopt,
      absl::optional<net::ReferrerPolicy> policy = absl::nullopt) const;

  // URL of the Identity Provider.
  GURL provider_;

  url::Origin relying_party_origin_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  FetchWellKnownCallback idp_well_known_callback_;
  FetchClientIdMetadataCallback client_metadata_callback_;
  SigninRequestCallback signin_request_callback_;
  TokenRequestCallback token_request_callback_;
  RevokeCallback revoke_callback_;
  LogoutCallback logout_callback_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<IdpNetworkRequestManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
