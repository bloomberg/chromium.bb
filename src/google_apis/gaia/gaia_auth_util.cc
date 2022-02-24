// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_util.h"

#include <stddef.h>

#include <memory>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_mint_token_consent_result.pb.h"
#include "url/gurl.h"

namespace gaia {

namespace {

const char kGmailDomain[] = "gmail.com";
const char kGoogleDomain[] = "google.com";
const char kGooglemailDomain[] = "googlemail.com";

std::string CanonicalizeEmailImpl(const std::string& email_address,
                                  bool change_googlemail_to_gmail) {
  std::string lower_case_email = base::ToLowerASCII(email_address);
  std::vector<std::string> parts = base::SplitString(
      lower_case_email, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2U)
    return lower_case_email;

  if (change_googlemail_to_gmail && parts[1] == kGooglemailDomain)
    parts[1] = kGmailDomain;

  if (parts[1] == kGmailDomain)  // only strip '.' for gmail accounts.
    base::RemoveChars(parts[0], ".", &parts[0]);

  std::string new_email = base::JoinString(parts, "@");
  VLOG(1) << "Canonicalized " << email_address << " to " << new_email;
  return new_email;
}

}  // namespace


ListedAccount::ListedAccount() {}

ListedAccount::ListedAccount(const ListedAccount& other) = default;

ListedAccount::~ListedAccount() {}

std::string CanonicalizeEmail(const std::string& email_address) {
  // CanonicalizeEmail() is called to process email strings that are eventually
  // shown to the user, and may also be used in persisting email strings.  To
  // avoid breaking this existing behavior, this function will not try to
  // change googlemail to gmail.
  return CanonicalizeEmailImpl(email_address, false);
}

std::string CanonicalizeDomain(const std::string& domain) {
  // Canonicalization of domain names means lower-casing them. Make sure to
  // update this function in sync with Canonicalize if this ever changes.
  return base::ToLowerASCII(domain);
}

std::string SanitizeEmail(const std::string& email_address) {
  std::string sanitized(email_address);

  // Apply a default domain if necessary.
  if (sanitized.find('@') == std::string::npos) {
    sanitized += '@';
    sanitized += kGmailDomain;
  }

  return sanitized;
}

bool AreEmailsSame(const std::string& email1, const std::string& email2) {
  return CanonicalizeEmailImpl(gaia::SanitizeEmail(email1), true) ==
      CanonicalizeEmailImpl(gaia::SanitizeEmail(email2), true);
}

std::string ExtractDomainName(const std::string& email_address) {
  // First canonicalize which will also verify we have proper domain part.
  std::string email = CanonicalizeEmail(email_address);
  size_t separator_pos = email.find('@');
  if (separator_pos != email.npos && separator_pos < email.length() - 1)
    return email.substr(separator_pos + 1);
  else
    NOTREACHED() << "Not a proper email address: " << email;
  return std::string();
}

bool IsGoogleInternalAccountEmail(const std::string& email) {
  return ExtractDomainName(SanitizeEmail(email)) == kGoogleDomain;
}

bool IsGaiaSignonRealm(const GURL& url) {
  if (!url.SchemeIsCryptographic())
    return false;

  return url == GaiaUrls::GetInstance()->gaia_url();
}


bool ParseListAccountsData(const std::string& data,
                           std::vector<ListedAccount>* accounts,
                           std::vector<ListedAccount>* signed_out_accounts) {
  if (accounts)
    accounts->clear();

  if (signed_out_accounts)
    signed_out_accounts->clear();

  // Parse returned data and make sure we have data.
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(data);
  if (!value)
    return false;

  if (!value->is_list())
    return false;
  base::Value::ConstListView list = value->GetListDeprecated();
  if (list.size() < 2u)
    return false;

  // Get list of account info.
  if (!list[1].is_list())
    return false;
  base::Value::ConstListView account_list = list[1].GetListDeprecated();

  // Build a vector of accounts from the cookie.  Order is important: the first
  // account in the list is the primary account.
  for (size_t i = 0; i < account_list.size(); ++i) {
    if (account_list[i].is_list()) {
      base::Value::ConstListView account = account_list[i].GetListDeprecated();
      std::string email;
      // Canonicalize the email since ListAccounts returns "display email".
      if (3u < account.size() && account[3].is_string() &&
          !(email = account[3].GetString()).empty()) {
        // New version if ListAccounts indicates whether the email's session
        // is still valid or not.  If this value is present and false, assume
        // its invalid.  Otherwise assume it's valid to remain compatible with
        // old version.
        int is_email_valid = 1;
        if (9u < account.size() && account[9].is_int())
          is_email_valid = account[9].GetInt();

        int signed_out = 0;
        if (14u < account.size() && account[14].is_int())
          signed_out = account[14].GetInt();

        int verified = 1;
        if (15u < account.size() && account[15].is_int())
          verified = account[15].GetInt();

        std::string gaia_id;
        // ListAccounts must also return the Gaia Id.
        if (10u < account.size() && account[10].is_string() &&
            !(gaia_id = account[10].GetString()).empty()) {
          ListedAccount listed_account;
          listed_account.email = CanonicalizeEmail(email);
          listed_account.gaia_id = gaia_id;
          listed_account.valid = is_email_valid != 0;
          listed_account.signed_out = signed_out != 0;
          listed_account.verified = verified != 0;
          listed_account.raw_email = email;
          auto* accounts_ptr =
              listed_account.signed_out ? signed_out_accounts : accounts;
          if (accounts_ptr)
            accounts_ptr->push_back(listed_account);
        }
      }
    }
  }

  return true;
}

bool ParseOAuth2MintTokenConsentResult(const std::string& consent_result,
                                       bool* approved,
                                       std::string* gaia_id) {
  DCHECK(approved);
  DCHECK(gaia_id);

  std::string decoded_result;
  if (!base::Base64UrlDecode(consent_result,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &decoded_result)) {
    VLOG(1) << "Base64UrlDecode() failed to decode the consent result";
    return false;
  }

  OAuth2MintTokenConsentResult parsed_result;
  if (!parsed_result.ParseFromString(decoded_result)) {
    VLOG(1) << "Failed to parse the consent result protobuf message";
    return false;
  }

  *approved = parsed_result.approved();
  *gaia_id = parsed_result.obfuscated_id();
  return true;
}

}  // namespace gaia
