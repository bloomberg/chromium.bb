// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_
#define GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_

#include <string>
#include <unordered_map>

#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

class OAuthMultiloginResult {
 public:
  OAuthMultiloginResult();
  OAuthMultiloginResult(const OAuthMultiloginResult& other);
  ~OAuthMultiloginResult();

  std::vector<net::CanonicalCookie> cookies() const { return cookies_; }

  // Parses cookies and status from JSON response. Maps status to
  // GoogleServiceAuthError::State values or returns UNEXPECTED_SERVER_RESPONSE
  // if JSON string cannot be parsed.
  static GoogleServiceAuthError CreateOAuthMultiloginResultFromString(
      const std::string& data,
      OAuthMultiloginResult* result);

  void TryParseCookiesFromValue(base::DictionaryValue* dictionary_value);

 private:
  std::vector<net::CanonicalCookie> cookies_;

  // Response body that has a form of JSON contains protection characters
  // against XSSI that have to be removed. See go/xssi.
  static base::StringPiece StripXSSICharacters(const std::string& data);

  // Maps status in JSON response to one of the GoogleServiceAuthError state
  // values.
  static GoogleServiceAuthError TryParseStatusFromValue(
      base::DictionaryValue* dictionary_value);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_