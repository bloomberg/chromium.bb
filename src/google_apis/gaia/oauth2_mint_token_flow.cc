// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_mint_token_flow.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_constants.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

const char kForceValueFalse[] = "false";
const char kForceValueTrue[] = "true";
const char kResponseTypeValueNone[] = "none";
const char kResponseTypeValueToken[] = "token";

const char kOAuth2IssueTokenBodyFormat[] =
    "force=%s"
    "&response_type=%s"
    "&scope=%s"
    "&client_id=%s"
    "&origin=%s"
    "&lib_ver=%s"
    "&release_channel=%s";
const char kOAuth2IssueTokenBodyFormatDeviceIdAddendum[] =
    "&device_id=%s&device_type=chrome";
const char kOAuth2IssueTokenBodyFormatConsentResultAddendum[] =
    "&consent_result=%s";
const char kIssueAdviceKey[] = "issueAdvice";
const char kIssueAdviceValueConsent[] = "consent";
const char kIssueAdviceValueRemoteConsent[] = "remoteConsent";
const char kAccessTokenKey[] = "token";
const char kConsentKey[] = "consent";
const char kExpiresInKey[] = "expiresIn";
const char kScopesKey[] = "scopes";
const char kDescriptionKey[] = "description";
const char kDetailKey[] = "detail";
const char kDetailSeparators[] = "\n";
const char kError[] = "error";
const char kMessage[] = "message";

static GoogleServiceAuthError CreateAuthError(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  if (net_error == net::ERR_ABORTED)
    return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);

  if (net_error != net::OK) {
    DLOG(WARNING) << "Server returned error: errno " << net_error;
    return GoogleServiceAuthError::FromConnectionError(net_error);
  }

  std::string response_body;
  if (body)
    response_body = std::move(*body);

  base::Optional<base::Value> value = base::JSONReader::Read(response_body);
  if (!value || !value->is_dict()) {
    int http_response_code = -1;
    if (head && head->headers)
      http_response_code = head->headers->response_code();
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        base::StringPrintf("Not able to parse a JSON object from "
                           "a service response. "
                           "HTTP Status of the response is: %d",
                           http_response_code));
  }
  const base::Value* error = value->FindDictKey(kError);
  if (!error) {
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find a detailed error in a service response.");
  }
  const std::string* message = error->FindStringKey(kMessage);
  if (!message) {
    return GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find an error message within a service error.");
  }
  return GoogleServiceAuthError::FromServiceError(*message);
}

bool AreCookiesEqual(const net::CanonicalCookie& lhs,
                     const net::CanonicalCookie& rhs) {
  return lhs.IsEquivalent(rhs);
}

void RecordApiCallResult(OAuth2MintTokenApiCallResult result) {
  base::UmaHistogramEnumeration(kOAuth2MintTokenApiCallResultHistogram, result);
}

}  // namespace

const char kOAuth2MintTokenApiCallResultHistogram[] =
    "Signin.OAuth2MintToken.ApiCallResult";

IssueAdviceInfoEntry::IssueAdviceInfoEntry() = default;
IssueAdviceInfoEntry::~IssueAdviceInfoEntry() = default;

IssueAdviceInfoEntry::IssueAdviceInfoEntry(const IssueAdviceInfoEntry& other) =
    default;
IssueAdviceInfoEntry& IssueAdviceInfoEntry::operator=(
    const IssueAdviceInfoEntry& other) = default;

bool IssueAdviceInfoEntry::operator ==(const IssueAdviceInfoEntry& rhs) const {
  return description == rhs.description && details == rhs.details;
}

RemoteConsentResolutionData::RemoteConsentResolutionData() = default;
RemoteConsentResolutionData::~RemoteConsentResolutionData() = default;
RemoteConsentResolutionData::RemoteConsentResolutionData(
    const RemoteConsentResolutionData& other) = default;
RemoteConsentResolutionData& RemoteConsentResolutionData::operator=(
    const RemoteConsentResolutionData& other) = default;

bool RemoteConsentResolutionData::operator==(
    const RemoteConsentResolutionData& rhs) const {
  return url == rhs.url && std::equal(cookies.begin(), cookies.end(),
                                      rhs.cookies.begin(), &AreCookiesEqual);
}

OAuth2MintTokenFlow::Parameters::Parameters() : mode(MODE_ISSUE_ADVICE) {}

OAuth2MintTokenFlow::Parameters::Parameters(
    const std::string& eid,
    const std::string& cid,
    const std::vector<std::string>& scopes_arg,
    const std::string& device_id,
    const std::string& consent_result,
    const std::string& version,
    const std::string& channel,
    Mode mode_arg)
    : extension_id(eid),
      client_id(cid),
      scopes(scopes_arg),
      device_id(device_id),
      consent_result(consent_result),
      version(version),
      channel(channel),
      mode(mode_arg) {}

OAuth2MintTokenFlow::Parameters::Parameters(const Parameters& other) = default;

OAuth2MintTokenFlow::Parameters::~Parameters() {}

OAuth2MintTokenFlow::OAuth2MintTokenFlow(Delegate* delegate,
                                         const Parameters& parameters)
    : delegate_(delegate), parameters_(parameters) {}

OAuth2MintTokenFlow::~OAuth2MintTokenFlow() { }

void OAuth2MintTokenFlow::ReportSuccess(const std::string& access_token,
                                        int time_to_live) {
  if (delegate_)
    delegate_->OnMintTokenSuccess(access_token, time_to_live);

  // |this| may already be deleted.
}

void OAuth2MintTokenFlow::ReportIssueAdviceSuccess(
    const IssueAdviceInfo& issue_advice) {
  if (delegate_)
    delegate_->OnIssueAdviceSuccess(issue_advice);

  // |this| may already be deleted.
}

void OAuth2MintTokenFlow::ReportRemoteConsentSuccess(
    const RemoteConsentResolutionData& resolution_data) {
  if (delegate_)
    delegate_->OnRemoteConsentSuccess(resolution_data);

  // |this| may already be deleted;
}

void OAuth2MintTokenFlow::ReportFailure(
    const GoogleServiceAuthError& error) {
  if (delegate_)
    delegate_->OnMintTokenFailure(error);

  // |this| may already be deleted.
}

GURL OAuth2MintTokenFlow::CreateApiCallUrl() {
  return GaiaUrls::GetInstance()->oauth2_issue_token_url();
}

std::string OAuth2MintTokenFlow::CreateApiCallBody() {
  const char* force_value =
      (parameters_.mode == MODE_MINT_TOKEN_FORCE ||
       parameters_.mode == MODE_RECORD_GRANT)
          ? kForceValueTrue : kForceValueFalse;
  const char* response_type_value =
      (parameters_.mode == MODE_MINT_TOKEN_NO_FORCE ||
       parameters_.mode == MODE_MINT_TOKEN_FORCE)
          ? kResponseTypeValueToken : kResponseTypeValueNone;
  std::string body = base::StringPrintf(
      kOAuth2IssueTokenBodyFormat,
      net::EscapeUrlEncodedData(force_value, true).c_str(),
      net::EscapeUrlEncodedData(response_type_value, true).c_str(),
      net::EscapeUrlEncodedData(base::JoinString(parameters_.scopes, " "), true)
          .c_str(),
      net::EscapeUrlEncodedData(parameters_.client_id, true).c_str(),
      net::EscapeUrlEncodedData(parameters_.extension_id, true).c_str(),
      net::EscapeUrlEncodedData(parameters_.version, true).c_str(),
      net::EscapeUrlEncodedData(parameters_.channel, true).c_str());
  if (!parameters_.device_id.empty()) {
    body.append(base::StringPrintf(
        kOAuth2IssueTokenBodyFormatDeviceIdAddendum,
        net::EscapeUrlEncodedData(parameters_.device_id, true).c_str()));
  }
  if (!parameters_.consent_result.empty()) {
    body.append(base::StringPrintf(
        kOAuth2IssueTokenBodyFormatConsentResultAddendum,
        net::EscapeUrlEncodedData(parameters_.consent_result, true).c_str()));
  }
  return body;
}

void OAuth2MintTokenFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  std::string response_body;
  if (body)
    response_body = std::move(*body);

  base::Optional<base::Value> value = base::JSONReader::Read(response_body);
  if (!value || !value->is_dict()) {
    RecordApiCallResult(OAuth2MintTokenApiCallResult::kParseJsonFailure);
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to parse a JSON object from a service response."));
    return;
  }

  std::string* issue_advice_value = value->FindStringKey(kIssueAdviceKey);
  if (!issue_advice_value) {
    RecordApiCallResult(
        OAuth2MintTokenApiCallResult::kIssueAdviceKeyNotFoundFailure);
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to find an issueAdvice in a service response."));
    return;
  }

  if (*issue_advice_value == kIssueAdviceValueConsent) {
    IssueAdviceInfo issue_advice;
    if (ParseIssueAdviceResponse(&(*value), &issue_advice)) {
      RecordApiCallResult(OAuth2MintTokenApiCallResult::kIssueAdviceSuccess);
      ReportIssueAdviceSuccess(issue_advice);
    } else {
      RecordApiCallResult(
          OAuth2MintTokenApiCallResult::kParseIssueAdviceFailure);
      ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
          "Not able to parse the contents of consent "
          "from a service response."));
    }
    return;
  }

  if (*issue_advice_value == kIssueAdviceValueRemoteConsent) {
    RemoteConsentResolutionData resolution_data;
    if (ParseRemoteConsentResponse(&(*value), &resolution_data)) {
      RecordApiCallResult(OAuth2MintTokenApiCallResult::kRemoteConsentSuccess);
      ReportRemoteConsentSuccess(resolution_data);
    } else {
      // Fallback to the issue advice flow.
      // TODO(https://crbug.com/1026237): Remove the fallback after making sure
      // that the new flow works correctly.
      RecordApiCallResult(OAuth2MintTokenApiCallResult::kRemoteConsentFallback);
      IssueAdviceInfo empty_issue_advice;
      ReportIssueAdviceSuccess(empty_issue_advice);
    }
    return;
  }

  std::string access_token;
  int time_to_live;
  if (ParseMintTokenResponse(&(*value), &access_token, &time_to_live)) {
    RecordApiCallResult(OAuth2MintTokenApiCallResult::kMintTokenSuccess);
    ReportSuccess(access_token, time_to_live);
  } else {
    RecordApiCallResult(OAuth2MintTokenApiCallResult::kParseMintTokenFailure);
    ReportFailure(GoogleServiceAuthError::FromUnexpectedServiceResponse(
        "Not able to parse the contents of access token "
        "from a service response."));
  }

  // |this| may be deleted!
}

void OAuth2MintTokenFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  RecordApiCallResult(OAuth2MintTokenApiCallResult::kApiCallFailure);
  ReportFailure(CreateAuthError(net_error, head, std::move(body)));
}

// static
bool OAuth2MintTokenFlow::ParseMintTokenResponse(const base::Value* dict,
                                                 std::string* access_token,
                                                 int* time_to_live) {
  CHECK(dict);
  CHECK(dict->is_dict());
  CHECK(access_token);
  CHECK(time_to_live);

  const std::string* ttl_string = dict->FindStringKey(kExpiresInKey);
  if (!ttl_string || !base::StringToInt(*ttl_string, time_to_live))
    return false;

  const std::string* access_token_ptr = dict->FindStringKey(kAccessTokenKey);
  if (!access_token_ptr)
    return false;

  *access_token = *access_token_ptr;
  return true;
}

// static
bool OAuth2MintTokenFlow::ParseIssueAdviceResponse(
    const base::Value* dict,
    IssueAdviceInfo* issue_advice) {
  CHECK(dict);
  CHECK(dict->is_dict());
  CHECK(issue_advice);

  const base::Value* consent_dict = dict->FindDictKey(kConsentKey);
  if (!consent_dict)
    return false;

  const base::Value* scopes_list = consent_dict->FindListKey(kScopesKey);
  if (!scopes_list)
    return false;

  bool success = true;
  for (const auto& scopes_entry : scopes_list->GetList()) {
    if (!scopes_entry.is_dict()) {
      success = false;
      break;
    }

    const std::string* description =
        scopes_entry.FindStringKey(kDescriptionKey);
    const std::string* detail = scopes_entry.FindStringKey(kDetailKey);
    if (!description || !detail) {
      success = false;
      break;
    }

    IssueAdviceInfoEntry entry;
    entry.description = base::UTF8ToUTF16(*description);
    base::TrimWhitespace(entry.description, base::TRIM_ALL, &entry.description);
    entry.details = base::SplitString(
        base::UTF8ToUTF16(*detail), base::ASCIIToUTF16(kDetailSeparators),
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    issue_advice->push_back(std::move(entry));
  }

  if (!success)
    issue_advice->clear();

  return success;
}

// static
bool OAuth2MintTokenFlow::ParseRemoteConsentResponse(
    const base::Value* dict,
    RemoteConsentResolutionData* resolution_data) {
  CHECK(dict);
  CHECK(resolution_data);

  const base::Value* resolution_dict = dict->FindDictKey("resolutionData");
  if (!resolution_dict)
    return false;

  const std::string* resolution_approach =
      resolution_dict->FindStringKey("resolutionApproach");
  if (!resolution_approach || *resolution_approach != "resolveInBrowser")
    return false;

  const std::string* resolution_url_string =
      resolution_dict->FindStringKey("resolutionUrl");
  if (!resolution_url_string)
    return false;
  GURL resolution_url(*resolution_url_string);
  if (!resolution_url.is_valid())
    return false;

  const base::Value* browser_cookies =
      resolution_dict->FindListKey("browserCookies");
  base::span<const base::Value> cookie_list;
  if (browser_cookies)
    cookie_list = browser_cookies->GetList();

  base::Time time_now = base::Time::Now();
  bool success = true;
  std::vector<net::CanonicalCookie> cookies;
  for (const auto& cookie_dict : cookie_list) {
    if (!cookie_dict.is_dict()) {
      success = false;
      break;
    }

    // Required parameters:
    const std::string* name = cookie_dict.FindStringKey("name");
    const std::string* value = cookie_dict.FindStringKey("value");
    const std::string* domain = cookie_dict.FindStringKey("domain");

    if (!name || !value || !domain) {
      success = false;
      break;
    }

    // Optional parameters:
    const std::string* path = cookie_dict.FindStringKey("path");
    const std::string* max_age_seconds =
        cookie_dict.FindStringKey("maxAgeSeconds");
    base::Optional<bool> is_secure = cookie_dict.FindBoolKey("isSecure");
    base::Optional<bool> is_http_only = cookie_dict.FindBoolKey("isHttpOnly");
    const std::string* same_site = cookie_dict.FindStringKey("sameSite");

    int64_t max_age = -1;
    if (max_age_seconds && !base::StringToInt64(*max_age_seconds, &max_age)) {
      success = false;
      break;
    }

    base::Time expiration_time = base::Time();
    if (max_age > 0)
      expiration_time = time_now + base::TimeDelta::FromSeconds(max_age);

    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateSanitizedCookie(
            resolution_url, *name, *value, *domain, path ? *path : "/",
            time_now, expiration_time, time_now, is_secure ? *is_secure : false,
            is_http_only ? *is_http_only : false,
            net::StringToCookieSameSite(same_site ? *same_site : ""),
            net::COOKIE_PRIORITY_DEFAULT);
    cookies.push_back(*cookie);
  }

  if (success) {
    resolution_data->url = std::move(resolution_url);
    resolution_data->cookies = std::move(cookies);
  }

  return success;
}

net::PartialNetworkTrafficAnnotationTag
OAuth2MintTokenFlow::GetNetworkTrafficAnnotationTag() {
  return net::DefinePartialNetworkTrafficAnnotation(
      "oauth2_mint_token_flow", "oauth2_api_call_flow", R"(
      semantics {
        sender: "Chrome Identity API"
        description:
          "Requests a token from gaia allowing an app or extension to act as "
          "the user when calling other google APIs."
        trigger: "API call from the app/extension."
        data:
          "User's login token, the identity of a chrome app/extension, and a "
          "list of oauth scopes requested by the app/extension."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        setting:
          "This feature cannot be disabled by settings, however the request is "
          "made only for signed-in users."
        chrome_policy {
          SigninAllowed {
            policy_options {mode: MANDATORY}
            SigninAllowed: false
          }
        }
      })");
}
