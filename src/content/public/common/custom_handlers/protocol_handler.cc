// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/custom_handlers/protocol_handler.h"

#include "base/json/values_util.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"

namespace content {

ProtocolHandler::ProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    base::Time last_modified,
    blink::ProtocolHandlerSecurityLevel security_level)
    : protocol_(base::ToLowerASCII(protocol)),
      url_(url),
      last_modified_(last_modified),
      security_level_(security_level) {}

ProtocolHandler::ProtocolHandler(const ProtocolHandler&) = default;
ProtocolHandler::~ProtocolHandler() = default;

ProtocolHandler ProtocolHandler::CreateProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    blink::ProtocolHandlerSecurityLevel security_level) {
  return ProtocolHandler(protocol, url, base::Time::Now(), security_level);
}

ProtocolHandler::ProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    const std::string& app_id,
    base::Time last_modified,
    blink::ProtocolHandlerSecurityLevel security_level)
    : protocol_(base::ToLowerASCII(protocol)),
      url_(url),
      web_app_id_(app_id),
      last_modified_(last_modified),
      security_level_(security_level) {}

// static
ProtocolHandler ProtocolHandler::CreateWebAppProtocolHandler(
    const std::string& protocol,
    const GURL& url,
    const std::string& app_id) {
  return ProtocolHandler(protocol, url, app_id, base::Time::Now(),
                         blink::ProtocolHandlerSecurityLevel::kStrict);
}

ProtocolHandler::ProtocolHandler() = default;

bool ProtocolHandler::IsValidDict(const base::DictionaryValue* value) {
  // Note that "title" parameter is ignored.
  // The |last_modified| field is optional as it was introduced in M68.
  return value->FindKey("protocol") && value->FindKey("url");
}

bool ProtocolHandler::IsValid() const {
  // This matches VerifyCustomHandlerURLSecurity() in blink's
  // NavigatorContentUtils.
  // https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
  bool has_valid_scheme =
      url_.SchemeIsHTTPOrHTTPS() ||
      (security_level_ ==
           blink::ProtocolHandlerSecurityLevel::kExtensionFeatures &&
       blink::CommonSchemeRegistry::IsExtensionScheme(url_.scheme()));
  if (!has_valid_scheme || !network::IsUrlPotentiallyTrustworthy(url_))
    return false;

  bool has_custom_scheme_prefix = false;
  bool allow_ext_plus_prefix =
      (security_level_ >=
       blink::ProtocolHandlerSecurityLevel::kExtensionFeatures);
  return blink::IsValidCustomHandlerScheme(protocol_, allow_ext_plus_prefix,
                                           has_custom_scheme_prefix);
}

bool ProtocolHandler::IsSameOrigin(
    const ProtocolHandler& handler) const {
  return handler.url().DeprecatedGetOriginAsURL() ==
         url_.DeprecatedGetOriginAsURL();
}

const ProtocolHandler& ProtocolHandler::EmptyProtocolHandler() {
  static const ProtocolHandler* const kEmpty = new ProtocolHandler();
  return *kEmpty;
}

ProtocolHandler ProtocolHandler::CreateProtocolHandler(
    const base::DictionaryValue* value) {
  if (!IsValidDict(value)) {
    return EmptyProtocolHandler();
  }
  std::string protocol, url;
  // |time| defaults to the beginning of time if it is not specified.
  base::Time time;
  blink::ProtocolHandlerSecurityLevel security_level =
      blink::ProtocolHandlerSecurityLevel::kStrict;
  if (const std::string* protocol_in = value->FindStringKey("protocol"))
    protocol = *protocol_in;
  if (const std::string* url_in = value->FindStringKey("url"))
    url = *url_in;
  absl::optional<base::Time> time_value =
      base::ValueToTime(value->FindKey("last_modified"));
  // Treat invalid times as the default value.
  if (time_value)
    time = *time_value;
  absl::optional<int> security_level_value =
      value->FindIntKey("security_level");
  if (security_level_value) {
    security_level =
        blink::ProtocolHandlerSecurityLevelFrom(*security_level_value);
  }

  if (const base::Value* app_id_val = value->FindKey("app_id")) {
    std::string app_id;
    if (app_id_val->is_string())
      app_id = app_id_val->GetString();
    return ProtocolHandler(protocol, GURL(url), app_id, time, security_level);
  }

  return ProtocolHandler(protocol, GURL(url), time, security_level);
}

GURL ProtocolHandler::TranslateUrl(const GURL& url) const {
  std::string translatedUrlSpec(url_.spec());
  base::ReplaceFirstSubstringAfterOffset(
      &translatedUrlSpec, 0, "%s",
      net::EscapeQueryParamValue(url.spec(), false));
  return GURL(translatedUrlSpec);
}

std::unique_ptr<base::DictionaryValue> ProtocolHandler::Encode() const {
  auto d = std::make_unique<base::DictionaryValue>();
  d->SetString("protocol", protocol_);
  d->SetString("url", url_.spec());
  d->SetKey("last_modified", base::TimeToValue(last_modified_));
  d->SetIntPath("security_level", static_cast<int>(security_level_));

  if (web_app_id_.has_value())
    d->SetString("app_id", web_app_id_.value());

  return d;
}

std::u16string ProtocolHandler::GetProtocolDisplayName(
    const std::string& protocol) {
  return content::GetContentClient()->GetLocalizedProtocolName(protocol);
}

std::u16string ProtocolHandler::GetProtocolDisplayName() const {
  return GetProtocolDisplayName(protocol_);
}

#if !defined(NDEBUG)
std::string ProtocolHandler::ToString() const {
  return "{ protocol=" + protocol_ +
         ", url=" + url_.spec() +
         " }";
}
#endif

bool ProtocolHandler::operator==(const ProtocolHandler& other) const {
  return protocol_ == other.protocol_ && url_ == other.url_;
}

bool ProtocolHandler::IsEquivalent(const ProtocolHandler& other) const {
  return protocol_ == other.protocol_ && url_ == other.url_;
}

bool ProtocolHandler::operator<(const ProtocolHandler& other) const {
  return url_ < other.url_;
}

}  // namespace content
