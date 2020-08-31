// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_manifest_downloader.h"

#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/link_header_util/link_header_util.h"
#include "components/payments/core/error_logger.h"
#include "components/payments/core/native_error_strings.h"
#include "components/payments/core/url_util.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/url_constants.h"

namespace payments {
namespace {

// Invokes |callback| with |error_format|.
void RespondWithError(const base::StringPiece& error_format,
                      const GURL& final_url,
                      const ErrorLogger& log,
                      PaymentManifestDownloadCallback callback) {
  std::string error_message = base::ReplaceStringPlaceholders(
      error_format, {final_url.spec()}, nullptr);
  log.Error(error_message);
  std::move(callback).Run(final_url, std::string(), error_message);
}

// Invokes the |callback| with |response_body|. If |response_body| is empty,
// then invokes |callback| with |empty_error_format|.
void RespondWithContent(const std::string& response_body,
                        const base::StringPiece& empty_error_format,
                        const GURL& final_url,
                        const ErrorLogger& log,
                        PaymentManifestDownloadCallback callback) {
  if (response_body.empty()) {
    RespondWithError(empty_error_format, final_url, log, std::move(callback));
  } else {
    std::move(callback).Run(final_url, response_body, std::string());
  }
}

bool IsValidManifestUrl(const GURL& url,
                        const ErrorLogger& log,
                        std::string* out_error_message) {
  bool is_valid = UrlUtil::IsValidManifestUrl(url);
  if (!is_valid) {
    *out_error_message = base::ReplaceStringPlaceholders(
        errors::kInvalidManifestUrl, {url.spec()}, nullptr);
    log.Error(*out_error_message);
  }
  return is_valid;
}

GURL ParseRedirectUrl(const net::RedirectInfo& redirect_info,
                      const GURL& original_url,
                      const ErrorLogger& log,
                      std::string* out_error_message) {
  if (redirect_info.status_code != net::HTTP_MOVED_PERMANENTLY &&   // 301
      redirect_info.status_code != net::HTTP_FOUND &&               // 302
      redirect_info.status_code != net::HTTP_SEE_OTHER &&           // 303
      redirect_info.status_code != net::HTTP_TEMPORARY_REDIRECT &&  // 307
      redirect_info.status_code != net::HTTP_PERMANENT_REDIRECT) {  // 308
    *out_error_message = base::ReplaceStringPlaceholders(
        errors::kHttpStatusCodeNotAllowed,
        {base::NumberToString(redirect_info.status_code),
         net::GetHttpReasonPhrase(
             static_cast<net::HttpStatusCode>(redirect_info.status_code)),
         original_url.spec()},
        nullptr);
    log.Error(*out_error_message);
    return GURL();
  }

  if (!IsValidManifestUrl(redirect_info.new_url, log, out_error_message))
    return GURL();

  return redirect_info.new_url;
}

}  // namespace

PaymentManifestDownloader::PaymentManifestDownloader(
    std::unique_ptr<ErrorLogger> log,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : log_(std::move(log)), url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(log_);
  DCHECK(url_loader_factory_);
}

PaymentManifestDownloader::~PaymentManifestDownloader() {}

void PaymentManifestDownloader::DownloadPaymentMethodManifest(
    const url::Origin& merchant_origin,
    const GURL& url,
    PaymentManifestDownloadCallback callback) {
  DCHECK(UrlUtil::IsValidManifestUrl(url));
  // Restrict number of redirects for efficiency and breaking circle.
  InitiateDownload(merchant_origin, url,
                   Download::Type::RESPONSE_BODY_OR_LINK_HEADER,
                   /*allowed_number_of_redirects=*/3, std::move(callback));
}

void PaymentManifestDownloader::DownloadWebAppManifest(
    const url::Origin& payment_method_manifest_origin,
    const GURL& url,
    PaymentManifestDownloadCallback callback) {
  DCHECK(UrlUtil::IsValidManifestUrl(url));
  InitiateDownload(payment_method_manifest_origin, url,
                   Download::Type::RESPONSE_BODY,
                   /*allowed_number_of_redirects=*/0, std::move(callback));
}

GURL PaymentManifestDownloader::FindTestServerURL(const GURL& url) const {
  return url;
}

PaymentManifestDownloader::Download::Download() {}

PaymentManifestDownloader::Download::~Download() {}

void PaymentManifestDownloader::OnURLLoaderRedirect(
    network::SimpleURLLoader* url_loader,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  auto download_it = downloads_.find(url_loader);
  DCHECK(download_it != downloads_.end());

  std::unique_ptr<Download> download = std::move(download_it->second);
  downloads_.erase(download_it);

  // Manually follow some type of redirects.
  std::string error_message;
  if (download->allowed_number_of_redirects > 0) {
    DCHECK_EQ(Download::Type::RESPONSE_BODY_OR_LINK_HEADER, download->type);
    GURL redirect_url = ParseRedirectUrl(redirect_info, download->original_url,
                                         *log_, &error_message);
    if (!redirect_url.is_empty()) {
      // Do not allow cross site redirects.
      if (net::registry_controlled_domains::SameDomainOrHost(
              download->original_url, redirect_url,
              net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
        // Redirects preserve the original request initiator.
        InitiateDownload(download->request_initiator, redirect_url,
                         Download::Type::RESPONSE_BODY_OR_LINK_HEADER,
                         --download->allowed_number_of_redirects,
                         std::move(download->callback));
        return;
      }
      error_message = base::ReplaceStringPlaceholders(
          errors::kPaymentManifestCrossSiteRedirectNotAllowed,
          {download->original_url.spec(), redirect_url.spec()}, nullptr);
    }
  } else {
    error_message = errors::kReachedMaximumNumberOfRedirects;
  }
  log_->Error(error_message);
  std::move(download->callback)
      .Run(download->original_url, std::string(), error_message);
}

void PaymentManifestDownloader::OnURLLoaderComplete(
    network::SimpleURLLoader* url_loader,
    std::unique_ptr<std::string> response_body) {
  scoped_refptr<net::HttpResponseHeaders> headers;
  if (url_loader->ResponseInfo())
    headers = url_loader->ResponseInfo()->headers;

  std::string response_body_str;
  if (response_body.get())
    response_body_str = std::move(*response_body);

  OnURLLoaderCompleteInternal(url_loader, url_loader->GetFinalURL(),
                              response_body_str, headers,
                              url_loader->NetError());
}

void PaymentManifestDownloader::OnURLLoaderCompleteInternal(
    network::SimpleURLLoader* url_loader,
    const GURL& final_url,
    const std::string& response_body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    int net_error) {
  auto download_it = downloads_.find(url_loader);
  DCHECK(download_it != downloads_.end());

  std::unique_ptr<Download> download = std::move(download_it->second);
  downloads_.erase(download_it);

  std::string error_message;
  if (download->type == Download::Type::RESPONSE_BODY) {
    if (!headers || headers->response_code() != net::HTTP_OK) {
      RespondWithError(errors::kPaymentManifestDownloadFailed, final_url, *log_,
                       std::move(download->callback));
    } else {
      RespondWithContent(response_body, errors::kNoContentInPaymentManifest,
                         final_url, *log_, std::move(download->callback));
    }
    return;
  }

  DCHECK_EQ(Download::Type::RESPONSE_BODY_OR_LINK_HEADER, download->type);

  if (!headers) {
    RespondWithContent(response_body, errors::kNoContentAndNoLinkHeader,
                       final_url, *log_, std::move(download->callback));
    return;
  }

  if (headers->response_code() != net::HTTP_OK &&
      headers->response_code() != net::HTTP_NO_CONTENT) {
    RespondWithError(errors::kPaymentManifestDownloadFailed, final_url, *log_,
                     std::move(download->callback));
    return;
  }

  std::string link_header;
  headers->GetNormalizedHeader("link", &link_header);
  if (link_header.empty()) {
    RespondWithContent(response_body, errors::kNoContentAndNoLinkHeader,
                       final_url, *log_, std::move(download->callback));
    return;
  }

  for (const auto& value : link_header_util::SplitLinkHeader(link_header)) {
    std::string link_url;
    std::unordered_map<std::string, base::Optional<std::string>> params;
    if (!link_header_util::ParseLinkHeaderValue(value.first, value.second,
                                                &link_url, &params)) {
      continue;
    }

    auto rel = params.find("rel");
    if (rel == params.end())
      continue;

    std::vector<std::string> rel_parts =
        base::SplitString(rel->second.value_or(""), HTTP_LWS,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (base::Contains(rel_parts, "payment-method-manifest")) {
      GURL payment_method_manifest_url = final_url.Resolve(link_url);

      if (!IsValidManifestUrl(payment_method_manifest_url, *log_,
                              &error_message)) {
        std::move(download->callback)
            .Run(final_url, std::string(), error_message);
        return;
      }

      if (!url::IsSameOriginWith(final_url, payment_method_manifest_url)) {
        error_message = base::ReplaceStringPlaceholders(
            errors::kCrossOriginPaymentMethodManifestNotAllowed,
            {payment_method_manifest_url.spec(), final_url.spec()}, nullptr);
        log_->Error(error_message);
        std::move(download->callback)
            .Run(final_url, std::string(), error_message);
        return;
      }

      // The request initiator for the payment method manifest is the origin of
      // the GET request with the HTTP link header.
      // https://github.com/w3c/webappsec-fetch-metadata/issues/30
      InitiateDownload(
          url::Origin::Create(final_url), payment_method_manifest_url,
          Download::Type::RESPONSE_BODY,
          /*allowed_number_of_redirects=*/0, std::move(download->callback));
      return;
    }
  }

  RespondWithContent(response_body, errors::kNoContentAndNoLinkHeader,
                     final_url, *log_, std::move(download->callback));
}

network::SimpleURLLoader* PaymentManifestDownloader::GetLoaderForTesting() {
  CHECK_EQ(downloads_.size(), 1u);
  return downloads_.begin()->second->loader.get();
}

GURL PaymentManifestDownloader::GetLoaderOriginalURLForTesting() {
  CHECK_EQ(downloads_.size(), 1u);
  return downloads_.begin()->second->original_url;
}

void PaymentManifestDownloader::InitiateDownload(
    const url::Origin& request_initiator,
    const GURL& url,
    Download::Type download_type,
    int allowed_number_of_redirects,
    PaymentManifestDownloadCallback callback) {
  DCHECK(UrlUtil::IsValidManifestUrl(url));

  // Only initial download of the payment method manifest (which might contain
  // an HTTP Link header) is allowed to redirect.
  DCHECK(allowed_number_of_redirects == 0 ||
         download_type == Download::Type::RESPONSE_BODY_OR_LINK_HEADER);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("payment_manifest_downloader", R"(
        semantics {
          sender: "Web Payments"
          description:
            "Chromium downloads manifest files for web payments API to help "
            "users make secure and convenient payments on the web."
          trigger:
            "A user that has a payment app visits a website that uses the web "
            "payments API."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings. Users can uninstall/"
            "disable all payment apps to stop this feature."
          policy_exception_justification: "Not implemented."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->request_initiator = request_initiator;
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  loader->SetOnRedirectCallback(
      base::BindRepeating(&PaymentManifestDownloader::OnURLLoaderRedirect,
                          weak_ptr_factory_.GetWeakPtr(), loader.get()));
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PaymentManifestDownloader::OnURLLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr(), loader.get()));

  auto download = std::make_unique<Download>();
  download->request_initiator = request_initiator;
  download->type = download_type;
  download->original_url = url;
  download->loader = std::move(loader);
  download->callback = std::move(callback);
  download->allowed_number_of_redirects = allowed_number_of_redirects;

  const network::SimpleURLLoader* identifier = download->loader.get();
  auto insert_result =
      downloads_.insert(std::make_pair(identifier, std::move(download)));
  DCHECK(insert_result.second);  // Whether the insert has succeeded.
}

}  // namespace payments
