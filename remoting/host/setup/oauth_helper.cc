// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/oauth_helper.h"

#include "base/stringprintf.h"
#include "google_apis/google_api_keys.h"
#include "googleurl/src/url_parse.h"
#include "net/base/escape.h"

namespace {

std::string GetComponent(const std::string& url,
                         const url_parse::Component component) {
  if (component.len < 0) {
    return "";
  }
  return url.substr(component.begin, component.len);
}

}  // namespace

namespace remoting {

std::string GetOauthScope() {
  return
      "https://www.googleapis.com/auth/chromoting "
      "https://www.googleapis.com/auth/googletalk "
      "https://www.googleapis.com/auth/userinfo.email ";
}

std::string GetOauthStartUrl() {
  return base::StringPrintf(
    "https://accounts.google.com/o/oauth2/auth"
    "?scope=%s"
    "&redirect_uri=%s"
    "&response_type=code"
    "&client_id=%s"
    "&access_type=offline"
    "&approval_prompt=force",
    net::EscapeUrlEncodedData(GetOauthScope(), true).c_str(),
    net::EscapeUrlEncodedData(GetOauthRedirectUrl(), true).c_str(),
    net::EscapeUrlEncodedData(google_apis::GetOAuth2ClientID(
        google_apis::CLIENT_REMOTING), true).c_str());
}

std::string GetOauthRedirectUrl() {
  return
    "https://chromoting-oauth.talkgadget.google.com/talkgadget/oauth/"
    "chrome-remote-desktop/rel/kgngmbheleoaphbjbaiobfdepmghbfah";
}

std::string GetOauthCodeInUrl(const std::string& url) {
  url_parse::Parsed url_parsed;
  ParseStandardURL(url.c_str(), url.length(), &url_parsed);
  std::string redirect = GetOauthRedirectUrl();
  url_parse::Parsed redirect_parsed;
  ParseStandardURL(redirect.c_str(), redirect.length(), &redirect_parsed);
  if (GetComponent(url, url_parsed.scheme) !=
      GetComponent(redirect, redirect_parsed.scheme)) {
    return "";
  }
  if (GetComponent(url, url_parsed.host) !=
      GetComponent(redirect, redirect_parsed.host)) {
    return "";
  }
  url_parse::Component query = url_parsed.query;
  url_parse::Component key;
  url_parse::Component value;
  while (ExtractQueryKeyValue(url.c_str(), &query, &key, &value)) {
    if (GetComponent(url, key) == "code") {
      return GetComponent(url, value);
    }
  }
  return "";
}

}  // namespace remoting
