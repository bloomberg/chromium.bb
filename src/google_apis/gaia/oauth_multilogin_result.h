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
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

class OAuthMultiloginResult {
 public:
  OAuthMultiloginResult();
  ~OAuthMultiloginResult();

  std::vector<net::CanonicalCookie> cookies() const { return cookies_; }

  // Returns true in case of success and false when there is a parse error.
  static bool CreateOAuthMultiloginResultFromString(
      const std::string& data,
      OAuthMultiloginResult* result);

  void TryParseCookiesFromValue(base::DictionaryValue* dictionary_value);

 private:
  std::vector<net::CanonicalCookie> cookies_;

  // Response body that has a form of JSON contains protection characters
  // against XSSI that have to be removed. See go/xssi.
  static base::StringPiece StripXSSICharacters(const std::string& data);
};

#endif  // GOOGLE_APIS_GAIA_OAUTH_MULTILOGIN_RESULT_H_