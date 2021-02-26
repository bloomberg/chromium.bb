// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/custom_handlers/protocol_handler.h"

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/util/values/values_util.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/origin_util.h"
#include "extensions/common/constants.h"
#include "net/base/escape.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "ui/base/l10n/l10n_util.h"

ProtocolHandler::ProtocolHandler(const std::string& protocol,
                                 const GURL& url,
                                 base::Time last_modified)
    : protocol_(base::ToLowerASCII(protocol)),
      url_(url),
      last_modified_(last_modified) {}

ProtocolHandler ProtocolHandler::CreateProtocolHandler(
    const std::string& protocol,
    const GURL& url) {
  return ProtocolHandler(protocol, url, base::Time::Now());
}

ProtocolHandler::ProtocolHandler() {
}

bool ProtocolHandler::IsValidDict(const base::DictionaryValue* value) {
  // Note that "title" parameter is ignored.
  // The |last_modified| field is optional as it was introduced in M68.
  return value->HasKey("protocol") && value->HasKey("url");
}

bool ProtocolHandler::IsValid() const {
  // TODO(https://crbug.com/977083): Consider limiting to secure contexts.

  // This matches VerifyCustomHandlerURLSecurity() in blink's
  // NavigatorContentUtils.
  if (!url_.SchemeIsHTTPOrHTTPS() &&
      !url_.SchemeIs(extensions::kExtensionScheme)) {
    return false;
  }

  bool has_custom_scheme_prefix;
  return blink::IsValidCustomHandlerScheme(protocol_, has_custom_scheme_prefix);
}

bool ProtocolHandler::IsSameOrigin(
    const ProtocolHandler& handler) const {
  return handler.url().GetOrigin() == url_.GetOrigin();
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
  value->GetString("protocol", &protocol);
  value->GetString("url", &url);
  base::Optional<base::Time> time_value =
      util::ValueToTime(value->FindKey("last_modified"));
  // Treat invalid times as the default value.
  if (time_value)
    time = *time_value;
  return ProtocolHandler(protocol, GURL(url), time);
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
  d->SetKey("last_modified", util::TimeToValue(last_modified_));
  return d;
}

base::string16 ProtocolHandler::GetProtocolDisplayName(
    const std::string& protocol) {
  if (protocol == "mailto")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_MAILTO_NAME);
  if (protocol == "webcal")
    return l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_WEBCAL_NAME);
  return base::UTF8ToUTF16(protocol);
}

base::string16 ProtocolHandler::GetProtocolDisplayName() const {
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
