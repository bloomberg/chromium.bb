// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_utils.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/download_utils_impl.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_error.h"
#include "content/browser/web_package/signed_exchange_request_handler.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"

namespace content {
namespace signed_exchange_utils {

void ReportErrorAndTraceEvent(
    SignedExchangeDevToolsProxy* devtools_proxy,
    const std::string& error_message,
    base::Optional<SignedExchangeError::FieldIndexPair> error_field) {
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("loading"),
                       "SignedExchangeError", TRACE_EVENT_SCOPE_THREAD, "error",
                       error_message);
  if (devtools_proxy)
    devtools_proxy->ReportError(error_message, std::move(error_field));
}

bool IsSignedExchangeHandlingEnabledOnIO(ResourceContext* context) {
  if (!GetContentClient()->browser()->AllowSignedExchangeOnIO(context))
    return false;

  return base::FeatureList::IsEnabled(features::kSignedHTTPExchange) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableExperimentalWebPlatformFeatures);
}

bool IsSignedExchangeHandlingEnabled(BrowserContext* context) {
  if (!GetContentClient()->browser()->AllowSignedExchange(context))
    return false;

  return base::FeatureList::IsEnabled(features::kSignedHTTPExchange) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableExperimentalWebPlatformFeatures);
}

bool IsSignedExchangeReportingForDistributorsEnabled() {
  return base::FeatureList::IsEnabled(network::features::kReporting) &&
         (base::FeatureList::IsEnabled(
              features::kSignedExchangeReportingForDistributors) ||
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableExperimentalWebPlatformFeatures));
}

bool ShouldHandleAsSignedHTTPExchange(
    const GURL& request_url,
    const network::ResourceResponseHead& head) {
  // Currently we don't support the signed exchange which is returned from a
  // service worker.
  // TODO(crbug/803774): Decide whether we should support it or not.
  if (head.was_fetched_via_service_worker)
    return false;
  if (!SignedExchangeRequestHandler::IsSupportedMimeType(head.mime_type))
    return false;
  // Do not handle responses without HttpResponseHeaders.
  // (Example: data:application/signed-exchange,)
  if (!head.headers.get())
    return false;
  if (download_utils::MustDownload(request_url, head.headers.get(),
                                   head.mime_type)) {
    return false;
  }
  return true;
}

base::Optional<SignedExchangeVersion> GetSignedExchangeVersion(
    const std::string& content_type) {
  // https://wicg.github.io/webpackage/loading.html#signed-exchange-version
  // Step 1. Let mimeType be the supplied MIME type of response. [spec text]
  // |content_type| is the supplied MIME type.
  // Step 2. If mimeType is undefined, return undefined. [spec text]
  // Step 3. If mimeType's essence is not "application/signed-exchange", return
  //         undefined. [spec text]
  const std::string::size_type semicolon = content_type.find(';');
  const std::string essence = base::ToLowerASCII(base::TrimWhitespaceASCII(
      content_type.substr(0, semicolon), base::TRIM_ALL));
  if (essence != "application/signed-exchange")
    return base::nullopt;

  // Step 4.Let params be mimeType's parameters. [spec text]
  std::map<std::string, std::string> params;
  if (semicolon != base::StringPiece::npos) {
    net::HttpUtil::NameValuePairsIterator parser(
        content_type.begin() + semicolon + 1, content_type.end(), ';');
    while (parser.GetNext()) {
      const base::StringPiece name = parser.name_piece();
      params[base::ToLowerASCII(name)] = parser.value();
    }
    if (!parser.valid())
      return base::nullopt;
  }
  // Step 5. If params["v"] exists, return it. Otherwise, return undefined.
  //        [spec text]
  auto iter = params.find("v");
  if (iter != params.end()) {
    if (iter->second == "b3")
      return base::make_optional(SignedExchangeVersion::kB3);
    return base::make_optional(SignedExchangeVersion::kUnknown);
  }
  return base::nullopt;
}

SignedExchangeLoadResult GetLoadResultFromSignatureVerifierResult(
    SignedExchangeSignatureVerifier::Result verify_result) {
  switch (verify_result) {
    case SignedExchangeSignatureVerifier::Result::kSuccess:
      return SignedExchangeLoadResult::kSuccess;
    case SignedExchangeSignatureVerifier::Result::kErrCertificateSHA256Mismatch:
      // "Handling the certificate reference
      //   ...
      //   - If the SHA-256 hash of chain’s leaf's certificate is not equal to
      //     certSha256, return "signature_verification_error"." [spec text]
      return SignedExchangeLoadResult::kSignatureVerificationError;
    case SignedExchangeSignatureVerifier::Result::
        kErrSignatureVerificationFailed:
      // "Validating a signature
      //   ...
      //   - If parsedSignature’s signature is not a valid signature of message
      //     by publicKey using the ecdsa_secp256r1_sha256 algorithm, return
      //     invalid." [spec text]
      //
      // "Parsing signed exchanges
      //   - ...
      //   - If parsedSignature is not valid for headerBytes and
      //     requestUrlBytes, and signed exchange version version, return
      //     "signature_verification_error"." [spec text]
      return SignedExchangeLoadResult::kSignatureVerificationError;
    case SignedExchangeSignatureVerifier::Result::kErrUnsupportedCertType:
      // "Validating a signature
      //   ...
      //   - If parsedSignature’s signature is not a valid signature of message
      //     by publicKey using the ecdsa_secp256r1_sha256 algorithm, return
      //     invalid." [spec text]
      //
      // "Parsing signed exchanges
      //   - ...
      //   - If parsedSignature is not valid for headerBytes and
      //     requestUrlBytes, and signed exchange version version, return
      //     "signature_verification_error"." [spec text]
      return SignedExchangeLoadResult::kSignatureVerificationError;
    case SignedExchangeSignatureVerifier::Result::kErrValidityPeriodTooLong:
      // "Cross-origin trust
      //   ...
      //   - If signature’s expiration time is more than 604800 seconds (7 days)
      //     after signature’s date, return "untrusted"." [spec text]
      //
      // "Parsing signed exchanges
      //   - ...
      //   - If parsedSignature does not establish cross-origin trust for
      //     parsedExchange, return "cert_verification_error"." [spec text]
      return SignedExchangeLoadResult::kCertVerificationError;
    case SignedExchangeSignatureVerifier::Result::kErrFutureDate:
    case SignedExchangeSignatureVerifier::Result::kErrExpired:
      // "Validating a signature
      //   ...
      //   - If the UA’s estimate of the current time is more than clockSkew
      //     before signature’s date, return "untrusted".
      //   - If the UA’s estimate of the current time is after signature’s
      //     expiration time, return "untrusted"." [spec text]
      //
      // "Parsing signed exchanges
      //   - ...
      //   - If parsedSignature is not valid for headerBytes and
      //     requestUrlBytes, and signed exchange version version, return
      //     "signature_verification_error"." [spec text]
      return SignedExchangeLoadResult::kSignatureVerificationError;

    // Deprecated error results.
    case SignedExchangeSignatureVerifier::Result::kErrNoCertificate_deprecated:
    case SignedExchangeSignatureVerifier::Result::
        kErrNoCertificateSHA256_deprecated:
    case SignedExchangeSignatureVerifier::Result::
        kErrInvalidSignatureFormat_deprecated:
    case SignedExchangeSignatureVerifier::Result::
        kErrInvalidSignatureIntegrity_deprecated:
    case SignedExchangeSignatureVerifier::Result::
        kErrInvalidTimestamp_deprecated:
      NOTREACHED();
      return SignedExchangeLoadResult::kSignatureVerificationError;
  }

  NOTREACHED();
  return SignedExchangeLoadResult::kSignatureVerificationError;
}

net::RedirectInfo CreateRedirectInfo(
    const GURL& new_url,
    const network::ResourceRequest& outer_request,
    const network::ResourceResponseHead& outer_response,
    bool is_fallback_redirect) {
  // https://wicg.github.io/webpackage/loading.html#mp-http-fetch
  // Step 3. Set actualResponse's status to 303. [spec text]
  return net::RedirectInfo::ComputeRedirectInfo(
      "GET", outer_request.url, outer_request.site_for_cookies,
      outer_request.top_frame_origin,
      outer_request.update_first_party_url_on_redirect
          ? net::URLRequest::FirstPartyURLPolicy::
                UPDATE_FIRST_PARTY_URL_ON_REDIRECT
          : net::URLRequest::FirstPartyURLPolicy::NEVER_CHANGE_FIRST_PARTY_URL,
      outer_request.referrer_policy, outer_request.referrer.spec(), 303,
      new_url,
      net::RedirectUtil::GetReferrerPolicyHeader(outer_response.headers.get()),
      false /* insecure_scheme_was_upgraded */, true /* copy_fragment */,
      is_fallback_redirect);
}

network::ResourceResponseHead CreateRedirectResponseHead(
    const network::ResourceResponseHead& outer_response,
    bool is_fallback_redirect) {
  network::ResourceResponseHead response_head;
  response_head.encoded_data_length = 0;
  std::string buf;
  std::string link_header;
  if (!is_fallback_redirect &&
      outer_response.headers) {
    outer_response.headers->GetNormalizedHeader("link", &link_header);
  }
  if (link_header.empty()) {
    buf = base::StringPrintf("HTTP/1.1 %d %s\r\n", 303, "See Other");
  } else {
    buf = base::StringPrintf(
        "HTTP/1.1 %d %s\r\n"
        "link: %s\r\n",
        303, "See Other", link_header.c_str());
  }
  response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(buf));
  response_head.encoded_data_length = 0;
  response_head.request_start = outer_response.request_start;
  response_head.response_start = outer_response.response_start;
  response_head.request_time = outer_response.request_time;
  response_head.response_time = outer_response.response_time;
  response_head.load_timing = outer_response.load_timing;
  return response_head;
}

}  // namespace signed_exchange_utils
}  // namespace content
