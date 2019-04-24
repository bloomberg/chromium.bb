// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_BASE_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_BASE_H_

#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"

namespace credential_provider {

// Base class for a a credential that is associated to a specific user on the
// system that needs a reauth. This credential may or may not be shown as a
// sign in option for a user tile so no interface map is defined for the class
// here. Instead it is the inherited classes that will determine if the
// credential can be shown on a user tile.
class ATL_NO_VTABLE CReauthCredentialBase : public CGaiaCredentialBase,
                                            public IReauthCredential {
 public:
  CReauthCredentialBase();
  ~CReauthCredentialBase();

  HRESULT FinalConstruct();
  void FinalRelease();

 protected:
  const CComBSTR& get_os_user_sid() const { return os_user_sid_; }
  const CComBSTR& get_os_user_domain() const { return os_user_domain_; }
  const CComBSTR& get_os_username() const { return os_username_; }

  // CGaiaCredentialBase

  // Adds additional command line switches to specify which gaia id to sign in
  // and which email is used to prefill the Gaia page.
  HRESULT GetUserGlsCommandline(base::CommandLine* command_line) override;

  // Checks if the information for the given |domain|\|username|, |sid| is
  // valid.
  // Returns S_OK if the user information stored in this credential matches
  // the user information that is being validated. Otherwise fills |error_text|
  // with an appropriate error message and returns an error.
  HRESULT ValidateExistingUser(const base::string16& username,
                               const base::string16& domain,
                               const base::string16& sid,
                               BSTR* error_text) override;
  HRESULT GetStringValueImpl(DWORD field_id, wchar_t** value) override;

  // IReauthCredential
  IFACEMETHODIMP SetOSUserInfo(BSTR sid, BSTR domain, BSTR username) override;
  IFACEMETHODIMP SetEmailForReauth(BSTR email) override;

 private:
  // Information about the OS user.
  CComBSTR os_user_domain_;
  CComBSTR os_username_;
  CComBSTR os_user_sid_;

  CComBSTR email_for_reauth_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_BASE_H_
