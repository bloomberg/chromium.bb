// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_network_request_manager.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/color_parser.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/escape.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "url/origin.h"

namespace content {

namespace {
// TODO(kenrb): These need to be defined in the explainer or draft spec and
// referenced here.

// Well-known configuration keys.
constexpr char kIdpEndpointKey[] = "idp_endpoint";
constexpr char kTokenEndpointKey[] = "idtoken_endpoint";
constexpr char kAccountsEndpointKey[] = "accounts_endpoint";
constexpr char kClientIdMetadataEndpointKey[] = "client_id_metadata_endpoint";
constexpr char kRevokeEndpoint[] = "revoke_endpoint";

// Client metadata keys.
constexpr char kPrivacyPolicyKey[] = "privacy_policy_url";
constexpr char kTermsOfServiceKey[] = "terms_of_service_url";

// Accounts endpoint response keys.
constexpr char kAccountsKey[] = "accounts";
constexpr char kIdpBrandingKey[] = "branding";

// Sign-in request response keys.
// TODO(majidvp): For consistency rename to signin_endpoint and move into
// `.well-known`.
constexpr char kSigninUrlKey[] = "signin_url";
constexpr char kIdTokenKey[] = "id_token";

// Token request body keys
constexpr char kAccountKey[] = "account_id";
constexpr char kRequestKey[] = "request";

// Revoke request body keys.
constexpr char kClientIdKey[] = "client_id";

constexpr char kRequestBodyContentType[] = "application/x-www-form-urlencoded";

// 1 MiB is an arbitrary upper bound that should account for any reasonable
// response size that is a part of this protocol.
constexpr int maxResponseSizeInKiB = 1024;

// safe_zone_diameter/icon_size as defined in
// https://www.w3.org/TR/appmanifest/#icon-masks
constexpr float kMaskableWebIconSafeZoneRatio = 0.8f;

net::NetworkTrafficAnnotationTag CreateTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("fedcm", R"(
        semantics {
          sender: "FedCM Backend"
          description:
            "The FedCM API allows websites to initiate user account login "
            "with identity providers which provide federated sign-in "
            "capabilities using OpenID Connect. The API provides a "
            "browser-mediated alternative to previously existing federated "
            "sign-in implementations."
          trigger:
            "A website executes the navigator.id.get() JavaScript method to "
            "initiate federated user sign-in to a designated provider."
          data:
            "An identity request contains a scope of claims specifying what "
            "user information is being requested from the identity provider, "
            "a label identifying the calling website application, and some "
            "OpenID Connect protocol functional fields."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "Not user controlled. But the verification is a trusted "
                   "API that doesn't use user data."
          policy_exception_justification:
            "Not implemented, considered not useful as no content is being "
            "uploaded; this request merely downloads the resources on the web."
        })");
}

std::unique_ptr<network::ResourceRequest> CreateCredentialedResourceRequest(
    GURL target_url,
    url::Origin initiator) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  auto target_origin = url::Origin::Create(target_url);
  auto site_for_cookies = net::SiteForCookies::FromOrigin(target_origin);
  resource_request->request_initiator = initiator;
  resource_request->url = target_url;
  resource_request->site_for_cookies = site_for_cookies;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kRequestBodyContentType);

  // Using a random 64-bit header value. This is just to keep service
  // implementations from assuming any particular static value.
  const int kBytes = 64 / 8;
  std::string webid_header_value;
  base::Base64Encode(base::RandBytesAsString(kBytes), &webid_header_value);
  resource_request->headers.SetHeader(kSecFedCmCsrfHeader, webid_header_value);
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, target_origin, target_origin,
      site_for_cookies);

  return resource_request;
}

absl::optional<content::IdentityRequestAccount> ParseAccount(
    const base::Value& account) {
  // TODO(yigu): Per spec the account id field should be "account_id" instead of
  // "sub". Using "sub" temporarily to unblock partner deployment.
  auto* account_id = account.FindStringKey("sub");
  auto* email = account.FindStringKey("email");
  auto* name = account.FindStringKey("name");
  auto* given_name = account.FindStringKey("given_name");
  auto* picture = account.FindStringKey("picture");

  // required fields
  if (!(account_id && email && name))
    return absl::nullopt;

  return content::IdentityRequestAccount(*account_id, *email, *name,
                                         given_name ? *given_name : "",
                                         picture ? GURL(*picture) : GURL());
}

// Parses accounts from given Value. Returns true if parse is successful and
// adds parsed accounts to the |account_list|.
bool ParseAccounts(const base::Value* accounts,
                   IdpNetworkRequestManager::AccountList& account_list) {
  DCHECK(account_list.empty());
  if (!accounts->is_list())
    return false;

  for (auto& account : accounts->GetList()) {
    if (!account.is_dict())
      return false;

    auto parsed_account = ParseAccount(account);
    if (parsed_account)
      account_list.push_back(parsed_account.value());
  }
  return !account_list.empty();
}

absl::optional<SkColor> ParseCssColor(const std::string* value) {
  if (value == nullptr)
    return absl::nullopt;

  SkColor color;
  if (!content::ParseCssColorString(*value, &color))
    return absl::nullopt;

  return SkColorSetA(color, 0xff);
}

// Parse IdentityProviderMetadata from given value. Overwrites |idp_metadata|
// with the parsed value.
void ParseIdentityProviderMetadata(const base::Value& idp_metadata_value,
                                   int brand_icon_ideal_size,
                                   int brand_icon_minimum_size,
                                   IdentityProviderMetadata& idp_metadata,
                                   GURL* brand_icon_url) {
  if (!idp_metadata_value.is_dict())
    return;

  idp_metadata.brand_background_color =
      ParseCssColor(idp_metadata_value.FindStringKey("background_color"));
  if (idp_metadata.brand_background_color) {
    idp_metadata.brand_text_color =
        ParseCssColor(idp_metadata_value.FindStringKey("foreground_color"));
    if (idp_metadata.brand_text_color) {
      float text_contrast_ratio = color_utils::GetContrastRatio(
          *idp_metadata.brand_background_color, *idp_metadata.brand_text_color);
      if (text_contrast_ratio < color_utils::kMinimumReadableContrastRatio)
        idp_metadata.brand_text_color = absl::nullopt;
    }
  }

  const base::Value* icons_value = idp_metadata_value.FindKey("icons");
  if (icons_value != nullptr && icons_value->is_list()) {
    std::vector<blink::Manifest::ImageResource> icons;
    for (const base::Value& icon_value : icons_value->GetList()) {
      if (!icon_value.is_dict())
        continue;

      const std::string* icon_src = icon_value.FindStringKey("url");
      if (icon_src == nullptr)
        continue;

      blink::Manifest::ImageResource icon;
      icon.src = GURL(*icon_src);
      if (!icon.src.is_valid())
        continue;

      icon.purpose = {blink::mojom::ManifestImageResource_Purpose::MASKABLE};

      absl::optional<int> icon_size = icon_value.FindIntKey("size");
      int icon_size_int = icon_size ? icon_size.value() : 0;
      icon.sizes.emplace_back(icon_size_int, icon_size_int);

      icons.push_back(icon);
    }

    *brand_icon_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
        icons, brand_icon_ideal_size / kMaskableWebIconSafeZoneRatio,
        brand_icon_minimum_size / kMaskableWebIconSafeZoneRatio,
        blink::mojom::ManifestImageResource_Purpose::MASKABLE);
  }
}

using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
FetchStatus GetResponseError(network::SimpleURLLoader* url_loader,
                             std::string* response_body) {
  int response_code = -1;
  auto* response_info = url_loader->ResponseInfo();
  if (response_info && response_info->headers)
    response_code = response_info->headers->response_code();

  if (response_code == net::HTTP_NOT_FOUND)
    return FetchStatus::kHttpNotFoundError;

  if (!response_body)
    return FetchStatus::kNoResponseError;

  return FetchStatus::kSuccess;
}

FetchStatus GetParsingError(
    const data_decoder::DataDecoder::ValueOrError& result) {
  if (!result.value)
    return FetchStatus::kInvalidResponseError;

  auto& response = *result.value;
  if (!response.is_dict())
    return FetchStatus::kInvalidResponseError;

  return FetchStatus::kSuccess;
}

}  // namespace

IdpNetworkRequestManager::Endpoints::Endpoints() = default;
IdpNetworkRequestManager::Endpoints::~Endpoints() = default;
IdpNetworkRequestManager::Endpoints::Endpoints(const Endpoints& other) =
    default;

IdpNetworkRequestManager::AccountRequestInfo::AccountRequestInfo(
    AccountsRequestCallback callback,
    int idp_brand_icon_ideal_size,
    int idp_brand_icon_minimum_size,
    IdpNetworkRequestManager::BrandIconDownloader idp_brand_icon_downloader)
    : callback(std::move(callback)),
      idp_brand_icon_ideal_size(idp_brand_icon_ideal_size),
      idp_brand_icon_minimum_size(idp_brand_icon_minimum_size),
      idp_brand_icon_downloader(std::move(idp_brand_icon_downloader)) {}
IdpNetworkRequestManager::AccountRequestInfo::~AccountRequestInfo() = default;
IdpNetworkRequestManager::AccountRequestInfo::AccountRequestInfo(
    AccountRequestInfo&&) = default;

// static
constexpr char IdpNetworkRequestManager::kWellKnownFilePath[];

// static
std::unique_ptr<IdpNetworkRequestManager> IdpNetworkRequestManager::Create(
    const GURL& provider,
    RenderFrameHost* host) {
  // FedCM is restricted to secure contexts.
  if (!network::IsOriginPotentiallyTrustworthy(url::Origin::Create(provider)))
    return nullptr;

  // Use the browser process URL loader factory because it has cross-origin
  // read blocking disabled.
  return std::make_unique<IdpNetworkRequestManager>(
      provider, host->GetLastCommittedOrigin(),
      host->GetStoragePartition()->GetURLLoaderFactoryForBrowserProcess());
}

IdpNetworkRequestManager::IdpNetworkRequestManager(
    const GURL& provider,
    const url::Origin& relying_party_origin,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
    : provider_(provider),
      relying_party_origin_(relying_party_origin),
      loader_factory_(loader_factory) {}

IdpNetworkRequestManager::~IdpNetworkRequestManager() = default;

void IdpNetworkRequestManager::FetchIdpWellKnown(
    FetchWellKnownCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!idp_well_known_callback_);

  idp_well_known_callback_ = std::move(callback);

  // TODO(yigu): Using .well-known in sub-directory (non-root) is common for multi-tenancy. However,
  // this may be an invalid use of .well-known and we should enforce it to be under root.
  // https://crbug.com/1277712.
  GURL target_url =
      provider_.Resolve(IdpNetworkRequestManager::kWellKnownFilePath);

  url_loader_ = CreateUncredentialedUrlLoader(target_url);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnWellKnownLoaded,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendSigninRequest(
    const GURL& signin_url,
    const std::string& request,
    SigninRequestCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!signin_request_callback_);

  signin_request_callback_ = std::move(callback);

  std::string escaped_request = net::EscapeUrlEncodedData(request, true);

  GURL target_url = GURL(signin_url.spec() + "?" + escaped_request);
  url_loader_ = CreateCredentialedUrlLoader(target_url);
  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnSigninRequestResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendAccountsRequest(
    const GURL& accounts_url,
    int idp_brand_icon_ideal_size,
    int idp_brand_icon_minimum_size,
    BrandIconDownloader idp_brand_icon_downloader,
    AccountsRequestCallback callback) {
  DCHECK(!url_loader_);

  // Use ReferrerPolicy::NO_REFERRER for this request so that relying party
  // identity is not exposed to the Identity provider via referrer.
  // TODO(cbiesinger): I don't think this does the right thing; per comments
  // in referrer_policy.h this only applies to redirects.
  net::ReferrerPolicy policy = net::ReferrerPolicy::NO_REFERRER;
  url_loader_ =
      CreateCredentialedUrlLoader(accounts_url, absl::nullopt, policy);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(
          &IdpNetworkRequestManager::OnAccountsRequestResponse,
          weak_ptr_factory_.GetWeakPtr(),
          AccountRequestInfo(std::move(callback), idp_brand_icon_ideal_size,
                             idp_brand_icon_minimum_size,
                             std::move(idp_brand_icon_downloader))),
      maxResponseSizeInKiB * 1024);
}

// TODO(majidvp): Should accept request in base::Value form instead of string.
std::string CreateTokenRequestBody(const std::string& account,
                                   const std::string& request) {
  // Given account and id_request creates the following JSON
  // ```json
  // {
  //   "account_id": "1234",
  //   "request": "nonce=abc987987cba&client_id=89898"
  //   }
  // }```
  base::Value request_data(base::Value::Type::DICTIONARY);
  request_data.SetStringKey(kAccountKey, account);
  if (!request.empty())
    request_data.SetStringKey(kRequestKey, request);

  std::string request_body;
  if (!base::JSONWriter::Write(request_data, &request_body)) {
    LOG(ERROR) << "Not able to serialize token request body.";
    return std::string();
  }
  return request_body;
}

void IdpNetworkRequestManager::SendTokenRequest(const GURL& token_url,
                                                const std::string& account,
                                                const std::string& request,
                                                TokenRequestCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!token_request_callback_);

  token_request_callback_ = std::move(callback);
  if (request.empty()) {
    std::move(token_request_callback_)
        .Run(FetchStatus::kInvalidRequestError, std::string());
    return;
  }

  url_loader_ = CreateCredentialedUrlLoader(token_url, request);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnTokenRequestResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

std::string CreateRevokeRequestBody(const std::string& client_id,
                                    const std::string& account) {
  // Given account and id_request creates the following JSON
  // ```json
  // {
  //   "account_id": "123",
  //   "request": {
  //     "client_id": "client1234"
  //   }
  // }```
  base::Value request_dict(base::Value::Type::DICTIONARY);
  request_dict.SetStringKey(kClientIdKey, client_id);

  base::Value request_data(base::Value::Type::DICTIONARY);
  request_data.SetStringKey(kAccountKey, account);
  request_data.SetKey(kRequestKey, std::move(request_dict));

  std::string request_body;
  if (!base::JSONWriter::Write(request_data, &request_body)) {
    LOG(ERROR) << "Not able to serialize token request body.";
    return std::string();
  }
  return request_body;
}

void IdpNetworkRequestManager::SendRevokeRequest(const GURL& revoke_url,
                                                 const std::string& client_id,
                                                 const std::string& account_id,
                                                 RevokeCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!token_request_callback_);

  revoke_callback_ = std::move(callback);

  std::string revoke_request_body;
  if (!client_id.empty())
    revoke_request_body += "client_id=" + client_id;

  if (!account_id.empty()) {
    if (!revoke_request_body.empty())
      revoke_request_body += "&";
    revoke_request_body += "account_id=" + account_id;
  }

  if (revoke_request_body.empty()) {
    std::move(revoke_callback_).Run(RevokeResponse::kError);
    return;
  }

  url_loader_ = CreateCredentialedUrlLoader(revoke_url, revoke_request_body);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnRevokeResponse,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::SendLogout(const GURL& logout_url,
                                          LogoutCallback callback) {
  // TODO(kenrb): Add browser test verifying that the response to this can
  // clear cookies. https://crbug.com/1155312.
  DCHECK(!url_loader_);
  DCHECK(!logout_callback_);

  logout_callback_ = std::move(callback);

  auto resource_request =
      CreateCredentialedResourceRequest(logout_url, relying_party_origin_);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept, "*/*");

  auto traffic_annotation = CreateTrafficAnnotation();

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnLogoutCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::OnWellKnownLoaded(
    std::unique_ptr<std::string> response_body) {
  FetchStatus response_error =
      GetResponseError(url_loader_.get(), response_body.get());
  url_loader_.reset();

  if (response_error != FetchStatus::kSuccess) {
    std::move(idp_well_known_callback_).Run(response_error, Endpoints());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnWellKnownParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IdpNetworkRequestManager::OnWellKnownParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (GetParsingError(result) == FetchStatus::kInvalidResponseError) {
    std::move(idp_well_known_callback_)
        .Run(FetchStatus::kInvalidResponseError, Endpoints());
    return;
  }

  auto& response = *result.value;
  auto ExtractEndpoint = [&](const char* key) {
    const base::Value* endpoint = response.FindKey(key);
    if (!endpoint || !endpoint->is_string()) {
      return std::string();
    }
    return endpoint->GetString();
  };

  Endpoints endpoints;
  endpoints.idp = ExtractEndpoint(kIdpEndpointKey);
  endpoints.token = ExtractEndpoint(kTokenEndpointKey);
  endpoints.accounts = ExtractEndpoint(kAccountsEndpointKey);
  endpoints.client_id_metadata = ExtractEndpoint(kClientIdMetadataEndpointKey);
  endpoints.revoke = ExtractEndpoint(kRevokeEndpoint);

  std::move(idp_well_known_callback_).Run(FetchStatus::kSuccess, endpoints);
}

void IdpNetworkRequestManager::OnSigninRequestResponse(
    std::unique_ptr<std::string> response_body) {
  url_loader_.reset();

  if (!response_body) {
    std::move(signin_request_callback_)
        .Run(SigninResponse::kSigninError, std::string());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnSigninRequestParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IdpNetworkRequestManager::OnSigninRequestParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    std::move(signin_request_callback_)
        .Run(SigninResponse::kInvalidResponseError, std::string());
    return;
  }

  auto& response = *result.value;

  if (!response.is_dict()) {
    std::move(signin_request_callback_)
        .Run(SigninResponse::kInvalidResponseError, std::string());
    return;
  }

  // TODO(kenrb): This possibly should be part of the well-known file, unless
  // IDPs ever have a reason to serve different URLs for sign-in pages.
  // https://crbug.com/1141125.
  const base::Value* signin_url = response.FindKey(kSigninUrlKey);
  const base::Value* id_token = response.FindKey(kIdTokenKey);

  // Only one of the fields should be present.
  bool signin_url_present = signin_url && signin_url->is_string();
  bool token_present = id_token && id_token->is_string();
  bool all_present = signin_url_present && token_present;
  if (!(signin_url_present || token_present) || all_present) {
    std::move(signin_request_callback_)
        .Run(SigninResponse::kInvalidResponseError, std::string());
    return;
  }

  if (signin_url) {
    std::move(signin_request_callback_)
        .Run(SigninResponse::kLoadIdp, signin_url->GetString());
    return;
  }

  std::move(signin_request_callback_)
      .Run(SigninResponse::kTokenGranted, id_token->GetString());
}

void IdpNetworkRequestManager::OnAccountsRequestResponse(
    AccountRequestInfo request_info,
    std::unique_ptr<std::string> response_body) {
  FetchStatus response_error =
      GetResponseError(url_loader_.get(), response_body.get());
  url_loader_.reset();

  if (response_error != FetchStatus::kSuccess) {
    std::move(request_info.callback)
        .Run(response_error, AccountList(), IdentityProviderMetadata());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnAccountsRequestParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request_info)));
}

void IdpNetworkRequestManager::OnAccountsRequestParsed(
    AccountRequestInfo request_info,
    data_decoder::DataDecoder::ValueOrError result) {
  auto Fail = [&]() {
    std::move(request_info.callback)
        .Run(FetchStatus::kInvalidResponseError, AccountList(),
             IdentityProviderMetadata());
  };

  if (GetParsingError(result) == FetchStatus::kInvalidResponseError) {
    Fail();
    return;
  }

  AccountList account_list;
  auto& response = *result.value;
  const base::Value* accounts = response.FindKey(kAccountsKey);
  bool accounts_present = accounts && ParseAccounts(accounts, account_list);

  if (!accounts_present) {
    Fail();
    return;
  }

  IdentityProviderMetadata idp_metadata;
  GURL idp_icon_url;
  const base::Value* idp_metadata_value = response.FindKey(kIdpBrandingKey);
  if (idp_metadata_value)
    ParseIdentityProviderMetadata(
        *idp_metadata_value, request_info.idp_brand_icon_ideal_size,
        request_info.idp_brand_icon_minimum_size, idp_metadata, &idp_icon_url);

  auto fetch_icon_callback =
      base::BindOnce(std::move(request_info.idp_brand_icon_downloader),
                     idp_icon_url, request_info.idp_brand_icon_ideal_size);
  auto on_icon_fetched_callback = base::BindOnce(
      &IdpNetworkRequestManager::OnIdentityProviderBrandIconFetched,
      weak_ptr_factory_.GetWeakPtr(), std::move(request_info),
      std::move(account_list), std::move(idp_metadata));

  if (idp_icon_url.is_valid()) {
    std::move(fetch_icon_callback).Run(std::move(on_icon_fetched_callback));
    return;
  }

  std::move(on_icon_fetched_callback).Run(0, 404, GURL(), {}, {});
}

void IdpNetworkRequestManager::OnIdentityProviderBrandIconFetched(
    AccountRequestInfo request_info,
    AccountList account_list,
    IdentityProviderMetadata idp_metadata,
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  if (bitmaps.size() == 1 && bitmaps[0].width() == bitmaps[0].height() &&
      bitmaps[0].width() >= request_info.idp_brand_icon_minimum_size) {
    idp_metadata.brand_icon = bitmaps[0];
  }

  std::move(request_info.callback)
      .Run(FetchStatus::kSuccess, std::move(account_list),
           std::move(idp_metadata));
}

void IdpNetworkRequestManager::OnTokenRequestResponse(
    std::unique_ptr<std::string> response_body) {
  FetchStatus response_error =
      GetResponseError(url_loader_.get(), response_body.get());
  url_loader_.reset();

  if (response_error != FetchStatus::kSuccess) {
    std::move(token_request_callback_).Run(response_error, std::string());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnTokenRequestParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IdpNetworkRequestManager::OnTokenRequestParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  auto Fail = [&]() {
    std::move(token_request_callback_)
        .Run(FetchStatus::kInvalidResponseError, std::string());
  };

  if (GetParsingError(result) == FetchStatus::kInvalidResponseError) {
    Fail();
    return;
  }

  auto& response = *result.value;
  const base::Value* id_token = response.FindKey(kIdTokenKey);
  bool token_present = id_token && id_token->is_string();

  if (!token_present) {
    Fail();
    return;
  }
  std::move(token_request_callback_)
      .Run(FetchStatus::kSuccess, id_token->GetString());
}

void IdpNetworkRequestManager::OnRevokeResponse(
    std::unique_ptr<std::string> response_body) {
  url_loader_.reset();
  RevokeResponse status =
      response_body ? RevokeResponse::kSuccess : RevokeResponse::kError;
  std::move(revoke_callback_).Run(status);
}

void IdpNetworkRequestManager::OnLogoutCompleted(
    std::unique_ptr<std::string> response_body) {
  url_loader_.reset();
  std::move(logout_callback_).Run();
}

void IdpNetworkRequestManager::FetchClientIdMetadata(
    const GURL& endpoint,
    const std::string& client_id,
    FetchClientIdMetadataCallback callback) {
  DCHECK(!url_loader_);
  DCHECK(!client_metadata_callback_);

  client_metadata_callback_ = std::move(callback);

  GURL target_url = endpoint.Resolve(
      "?client_id=" + net::EscapeQueryParamValue(client_id, true));

  url_loader_ = CreateUncredentialedUrlLoader(target_url);

  url_loader_->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&IdpNetworkRequestManager::OnClientIdMetadataLoaded,
                     weak_ptr_factory_.GetWeakPtr()),
      maxResponseSizeInKiB * 1024);
}

void IdpNetworkRequestManager::OnClientIdMetadataLoaded(
    std::unique_ptr<std::string> response_body) {
  FetchStatus response_error =
      GetResponseError(url_loader_.get(), response_body.get());
  url_loader_.reset();

  if (response_error != FetchStatus::kSuccess) {
    std::move(client_metadata_callback_)
        .Run(response_error, ClientIdMetadata());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&IdpNetworkRequestManager::OnClientIdMetadataParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IdpNetworkRequestManager::OnClientIdMetadataParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (GetParsingError(result) == FetchStatus::kInvalidResponseError) {
    std::move(client_metadata_callback_)
        .Run(FetchStatus::kInvalidResponseError, ClientIdMetadata());
    return;
  }

  auto& response = *result.value;
  auto ExtractUrl = [&](const char* key) {
    const base::Value* endpoint = response.FindKey(key);
    if (!endpoint || !endpoint->is_string()) {
      return std::string();
    }
    return endpoint->GetString();
  };

  ClientIdMetadata data;
  data.privacy_policy_url = ExtractUrl(kPrivacyPolicyKey);
  data.terms_of_service_url = ExtractUrl(kTermsOfServiceKey);

  std::move(client_metadata_callback_).Run(FetchStatus::kSuccess, data);
}

std::unique_ptr<network::SimpleURLLoader>
IdpNetworkRequestManager::CreateUncredentialedUrlLoader(
    const GURL& target_url) const {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      CreateTrafficAnnotation();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  const url::Origin& idp_origin = url::Origin::Create(provider_);

  resource_request->url = target_url;
  // TODO(kenrb): credentials_mode should be kOmit, but for prototyping
  // purposes it is useful to be able to run test IdPs on services that always
  // require cookies. This needs to be changed back when a better solution is
  // found or those test IdPs are no longer required.
  // See https://crbug.com/1159177.
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kRequestBodyContentType);
  // TODO(kenrb): Not following redirects is important for security because
  // this bypasses CORB. Ensure there is a test added.
  // https://crbug.com/1155312.
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->request_initiator = relying_party_origin_;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 idp_origin, idp_origin, net::SiteForCookies());

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          traffic_annotation);
}

std::unique_ptr<network::SimpleURLLoader>
IdpNetworkRequestManager::CreateCredentialedUrlLoader(
    const GURL& target_url,
    absl::optional<std::string> request_body,
    absl::optional<net::ReferrerPolicy> policy) const {
  auto resource_request =
      CreateCredentialedResourceRequest(target_url, relying_party_origin_);
  if (policy)
    resource_request->referrer_policy = *policy;
  if (request_body) {
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                        kRequestBodyContentType);
  }

  auto traffic_annotation = CreateTrafficAnnotation();
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  if (request_body)
    loader->AttachStringForUpload(*request_body, kRequestBodyContentType);
  return loader;
}

}  // namespace content
