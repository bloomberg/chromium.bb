// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_COM_FAKES_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_COM_FAKES_H_

#include <atlcomcli.h>
#include <credentialprovider.h>
#include <propkey.h>

#include <vector>

#include "base/strings/string16.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"

namespace credential_provider {

#define DECLARE_IUNKOWN_NOQI_WITH_REF()                            \
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override; \
  ULONG STDMETHODCALLTYPE AddRef() override;                       \
  ULONG STDMETHODCALLTYPE Release(void) override;                  \
  ULONG ref_count_ = 1;

///////////////////////////////////////////////////////////////////////////////

// Fake the CredentialProviderUserArray COM object.
class FakeCredentialProviderUser : public ICredentialProviderUser {
 public:
  FakeCredentialProviderUser(const wchar_t* sid, const wchar_t* username);
  virtual ~FakeCredentialProviderUser();

 private:
  // ICredentialProviderUser
  DECLARE_IUNKOWN_NOQI_WITH_REF()
  HRESULT STDMETHODCALLTYPE GetSid(wchar_t** sid) override;
  HRESULT STDMETHODCALLTYPE GetProviderID(GUID* providerID) override;
  HRESULT STDMETHODCALLTYPE GetStringValue(REFPROPERTYKEY key,
                                           wchar_t** stringValue) override;
  HRESULT STDMETHODCALLTYPE GetValue(REFPROPERTYKEY key,
                                     PROPVARIANT* value) override;

  base::string16 sid_;
  base::string16 username_;
};

///////////////////////////////////////////////////////////////////////////////

// Fake the CredentialProviderUserArray COM object.
class FakeCredentialProviderUserArray : public ICredentialProviderUserArray {
 public:
  FakeCredentialProviderUserArray();
  virtual ~FakeCredentialProviderUserArray();

  void AddUser(const wchar_t* sid, const wchar_t* username) {
    users_.emplace_back(sid, username);
  }

  void SetAccountOptions(CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS cpao) {
    cpao_ = cpao;
  }

 private:
  // ICredentialProviderUserArray
  DECLARE_IUNKOWN_NOQI_WITH_REF()
  IFACEMETHODIMP SetProviderFilter(REFGUID guidProviderToFilterTo) override;
  IFACEMETHODIMP GetAccountOptions(
      CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS* cpao) override;
  IFACEMETHODIMP GetCount(DWORD* userCount) override;
  IFACEMETHODIMP GetAt(DWORD index, ICredentialProviderUser** user) override;

  std::vector<FakeCredentialProviderUser> users_;
  CREDENTIAL_PROVIDER_ACCOUNT_OPTIONS cpao_ = CPAO_NONE;
};

///////////////////////////////////////////////////////////////////////////////

// Fake OS imlpementation of ICredentialProviderEvents.
class FakeCredentialProviderEvents : public ICredentialProviderEvents {
 public:
  FakeCredentialProviderEvents();
  virtual ~FakeCredentialProviderEvents();

  // ICredentialProviderEvents
  DECLARE_IUNKOWN_NOQI_WITH_REF()
  IFACEMETHODIMP CredentialsChanged(UINT_PTR upAdviseContext) override;

  bool CredentialsChangedReceived() const { return did_change_; }
  void ResetCredentialsChangedReceived() { did_change_ = false; }

 private:
  bool did_change_ = false;
};

///////////////////////////////////////////////////////////////////////////////

// Fake the GaiaCredentialProvider COM object.
class FakeGaiaCredentialProvider : public IGaiaCredentialProvider {
 public:
  FakeGaiaCredentialProvider();
  virtual ~FakeGaiaCredentialProvider();

  const CComBSTR& username() const { return username_; }
  const CComBSTR& password() const { return password_; }
  const CComBSTR& sid() const { return sid_; }
  bool credentials_changed_fired() const { return credentials_changed_fired_; }
  void ResetCredentialsChangedFired() { credentials_changed_fired_ = FALSE; }
  void SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) {
    cpus_ = cpus;
  }

  // IGaiaCredentialProvider
  IFACEMETHODIMP GetUsageScenario(DWORD* cpus) override;
  DECLARE_IUNKOWN_NOQI_WITH_REF()
  IFACEMETHODIMP OnUserAuthenticated(IUnknown* credential,
                                     BSTR username,
                                     BSTR password,
                                     BSTR sid,
                                     BOOL fire_credentials_changed) override;

 private:
  CComBSTR username_;
  CComBSTR password_;
  CComBSTR sid_;
  BOOL credentials_changed_fired_ = FALSE;
  CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus_ = CPUS_LOGON;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_COM_FAKES_H_
