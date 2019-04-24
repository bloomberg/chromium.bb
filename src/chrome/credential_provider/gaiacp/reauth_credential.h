// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_

#include "chrome/credential_provider/gaiacp/reauth_credential_base.h"

namespace credential_provider {

// A credential for a user that exists on the system and is associated with a
// Gaia account.
class ATL_NO_VTABLE CReauthCredential
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CReauthCredentialBase {
 public:
  DECLARE_NO_REGISTRY()

  CReauthCredential();
  ~CReauthCredential();

  HRESULT FinalConstruct();
  void FinalRelease();

 private:
  // This class does not say it implements ICredentialProviderCredential2.
  // It only implements ICredentialProviderCredential.  Otherwise the
  // credential will show up on the welcome screen only for domain joined
  // machines.
  BEGIN_COM_MAP(CReauthCredential)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(IReauthCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential2)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  // ICredentialProviderCredential2
  IFACEMETHODIMP GetUserSid(wchar_t** sid) override;

  // IReauthCredential
  IFACEMETHODIMP SetOSUserInfo(BSTR sid, BSTR domain, BSTR username) override;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_REAUTH_CREDENTIAL_H_
