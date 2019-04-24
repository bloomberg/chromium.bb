// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/reauth_credential_base.h"

#include "base/command_line.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_resources.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

CReauthCredentialBase::CReauthCredentialBase() = default;

CReauthCredentialBase::~CReauthCredentialBase() = default;

// CGaiaCredentialBase /////////////////////////////////////////////////////////

HRESULT CReauthCredentialBase::GetUserGlsCommandline(
    base::CommandLine* command_line) {
  DCHECK(command_line);
  DCHECK(os_user_sid_.Length());

  // If this is an existing user with an SID, try to get its gaia id and pass
  // it to the GLS for verification.
  base::string16 gaia_id;
  if (GetIdFromSid(OLE2CW(os_user_sid_), &gaia_id) == S_OK) {
    command_line->AppendSwitchNative(kGaiaIdSwitch, gaia_id);
    if (email_for_reauth_.Length()) {
      command_line->AppendSwitchNative(kPrefillEmailSwitch,
                                       (BSTR)email_for_reauth_);
    }
  } else {
    LOGFN(ERROR) << "Reauth credential on user=" << os_username_
                 << " does not have an associated Gaia id";
    return E_UNEXPECTED;
  }
  return CGaiaCredentialBase::GetUserGlsCommandline(command_line);
}

HRESULT CReauthCredentialBase::ValidateExistingUser(
    const base::string16& username,
    const base::string16& domain,
    const base::string16& sid,
    BSTR* error_text) {
  DCHECK(os_username_.Length());
  DCHECK(os_user_sid_.Length());

  // SID, domain and username found must match what is stored in this
  // credential.
  if ((os_username_ != W2COLE(username.c_str())) ||
      (os_user_domain_.Length() && os_user_domain_ != W2COLE(domain.c_str()))) {
    LOGFN(ERROR) << "Username calculated '" << domain << "\\" << username
                 << "' does not match the "
                 << "username that is set '" << os_user_domain_ << "\\"
                 << os_username_ << "'";
    *error_text = AllocErrorString(IDS_ACCOUNT_IN_USE_BASE);
    return E_UNEXPECTED;
  }

  return S_OK;
}

HRESULT CReauthCredentialBase::GetStringValueImpl(DWORD field_id,
                                                  wchar_t** value) {
  if (field_id == FID_PROVIDER_LABEL) {
    base::string16 label(
        GetStringResource(IDS_EXISTING_AUTH_FID_PROVIDER_LABEL_BASE));
    return ::SHStrDupW(label.c_str(), value);
  }

  return CGaiaCredentialBase::GetStringValueImpl(field_id, value);
}

// IReauthCredential ///////////////////////////////////////////////////////////

HRESULT CReauthCredentialBase::SetOSUserInfo(BSTR sid,
                                             BSTR domain,
                                             BSTR username) {
  DCHECK(sid);
  DCHECK(domain);
  DCHECK(username);

  os_user_domain_ = domain;
  os_user_sid_ = sid;
  os_username_ = username;
  return S_OK;
}

IFACEMETHODIMP CReauthCredentialBase::SetEmailForReauth(BSTR email) {
  DCHECK(email);

  email_for_reauth_ = email;
  return S_OK;
}

}  // namespace credential_provider
