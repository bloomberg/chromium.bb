// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/reauth_credential.h"

#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

CReauthCredential::CReauthCredential() = default;

CReauthCredential::~CReauthCredential() = default;

HRESULT CReauthCredential::FinalConstruct() {
  LOGFN(INFO);
  return S_OK;
}

void CReauthCredential::FinalRelease() {
  LOGFN(INFO);
}

// ICredentialProviderCredential2 //////////////////////////////////////////////
HRESULT CReauthCredential::GetUserSid(wchar_t** sid) {
  USES_CONVERSION;
  DCHECK(sid);
  LOGFN(INFO) << "sid=" << OLE2CW(get_os_user_sid());

  HRESULT hr = ::SHStrDupW(OLE2CW(get_os_user_sid()), sid);
  if (FAILED(hr))
    LOGFN(ERROR) << "SHStrDupW hr=" << putHR(hr);

  return hr;
}

// IReauthCredential //////////////////////////////////////////////
HRESULT CReauthCredential::SetOSUserInfo(BSTR sid, BSTR domain, BSTR username) {
  HRESULT original_hr =
      CReauthCredentialBase::SetOSUserInfo(sid, domain, username);

  // Set the default credential provider for this tile.
  HRESULT hr =
      SetLogonUiUserTileEntry(OLE2W(sid), CLSID_GaiaCredentialProvider);
  if (FAILED(hr))
    LOGFN(ERROR) << "SetLogonUIUserTileEntry hr=" << putHR(hr);

  return original_hr;
}

}  // namespace credential_provider
