// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_H_

#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/gaiacp/grit/gaia_resources.h"

namespace credential_provider {

// Implementation of an ICredentialProviderCredential backed by a Gaia account.
class ATL_NO_VTABLE CGaiaCredential
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CComCoClass<CGaiaCredential, &CLSID_GaiaCredential>,
      public CGaiaCredentialBase {
 public:
  DECLARE_NO_REGISTRY()

  CGaiaCredential();
  ~CGaiaCredential();

  HRESULT FinalConstruct();
  void FinalRelease();

 private:
  // This class does not say it implements ICredentialProviderCredential2.
  // It only implements ICredentialProviderCredential.  Otherwise the
  // credential will show up on the welcome screen only for domain joined
  // machines.
  BEGIN_COM_MAP(CGaiaCredential)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  // IGaiaCredential
  IFACEMETHODIMP FinishAuthentication(BSTR username,
                                      BSTR password,
                                      BSTR fullname,
                                      BSTR* sid,
                                      BSTR* error_text) override;
};

OBJECT_ENTRY_AUTO(__uuidof(GaiaCredential), CGaiaCredential)

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GAIA_CREDENTIAL_H_
