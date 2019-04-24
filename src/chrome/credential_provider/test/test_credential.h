// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_TEST_CREDENTIAL_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_TEST_CREDENTIAL_H_

#include <memory>
#include <string>

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>
#include <credentialprovider.h>

#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/test/fake_gls_run_helper.h"

namespace base {
class CommandLine;
}

namespace credential_provider {

namespace testing {

class DECLSPEC_UUID("3710aa3a-13c7-44c2-bc38-09ba137804d8") ITestCredential
    : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE
  SetDefaultExitCode(UiExitCodes default_exit_code) = 0;
  virtual HRESULT STDMETHODCALLTYPE
  SetGlsEmailAddress(const std::string& email) = 0;
  virtual HRESULT STDMETHODCALLTYPE
  SetGaiaIdOverride(const std::string& gaia_id) = 0;
  virtual HRESULT STDMETHODCALLTYPE WaitForGls() = 0;
  virtual HRESULT STDMETHODCALLTYPE
  SetStartGlsEventName(const base::string16& event_name) = 0;
  virtual BSTR STDMETHODCALLTYPE GetFinalUsername() = 0;
  virtual std::string STDMETHODCALLTYPE GetFinalEmail() = 0;
  virtual BSTR STDMETHODCALLTYPE GetErrorText() = 0;
  virtual bool STDMETHODCALLTYPE AreCredentialsValid() = 0;
  virtual bool STDMETHODCALLTYPE CanAttemptWindowsLogon() = 0;
  virtual bool STDMETHODCALLTYPE IsWindowsPasswordValidForStoredUser() = 0;
  virtual void STDMETHODCALLTYPE
  SetWindowsPassword(const CComBSTR& windows_password) = 0;
  virtual bool STDMETHODCALLTYPE IsGlsRunning() = 0;
};

// Test implementation of an ICredentialProviderCredential backed by a Gaia
// account.  This class overrides some methods for testing purposes.
// The template parameter T specifies which class that implements
// CGaiaCredentialBase should be the base for this test class.
// A CGaiaCredentialBase is required to call base class functions in the
// following ITestCredential implementations:
// GetFinalUsername, GetFinalEmail, AreCredentialsValid,
// CanAttemptWindowsLogon, IsWindowsPasswordValidForStoredUser,
// SetWindowsPassword.
// Also the following IGaiaCredential/CGaiaCredentialBase functions need to be
// overridden: OnUserAuthenticated, ReportError, GetBaseGlsCommandline,
// DisplayErrorInUI, ForkGaiaLogonStub, ResetInternalState.
template <class T>
class ATL_NO_VTABLE CTestCredentialBase : public T, public ITestCredential {
 public:
  CTestCredentialBase();
  ~CTestCredentialBase();

  // ITestCredential.
  IFACEMETHODIMP SetDefaultExitCode(UiExitCodes default_exit_code) override;
  IFACEMETHODIMP SetGlsEmailAddress(const std::string& email) override;
  IFACEMETHODIMP SetGaiaIdOverride(const std::string& gaia_id) override;
  IFACEMETHODIMP WaitForGls() override;
  IFACEMETHODIMP SetStartGlsEventName(
      const base::string16& event_name) override;
  BSTR STDMETHODCALLTYPE GetFinalUsername() override;
  std::string STDMETHODCALLTYPE GetFinalEmail() override;
  BSTR STDMETHODCALLTYPE GetErrorText() override;
  bool STDMETHODCALLTYPE AreCredentialsValid() override;
  bool STDMETHODCALLTYPE CanAttemptWindowsLogon() override;
  bool STDMETHODCALLTYPE IsWindowsPasswordValidForStoredUser() override;
  void STDMETHODCALLTYPE
  SetWindowsPassword(const CComBSTR& windows_password) override;
  bool STDMETHODCALLTYPE IsGlsRunning() override;

  void SignalGlsCompletion();

  // IGaiaCredential.
  IFACEMETHODIMP OnUserAuthenticated(BSTR authentication_info,
                                     BSTR* status_text) override;
  IFACEMETHODIMP ReportError(LONG status,
                             LONG substatus,
                             BSTR status_text) override;
  // CGaiaCredentialBase.

  // Override to catch completion of the GLS process on failure and also log
  // the error message.
  void DisplayErrorInUI(LONG status, LONG substatus, BSTR status_text) override;

  // Overrides to build a dummy command line for testing.
  HRESULT GetBaseGlsCommandline(base::CommandLine* command_line) override;

  // Overrides to check correct startup of GLS process.
  HRESULT ForkGaiaLogonStub(
      OSProcessManager* process_manager,
      const base::CommandLine& command_line,
      CGaiaCredentialBase::UIProcessInfo* uiprocinfo) override;

  // Overrides to directly save to a fake scoped user profile.
  HRESULT ForkSaveAccountInfoStub(
      const std::unique_ptr<base::DictionaryValue>& dict,
      BSTR* status_text) override;

  void ResetInternalState() override;

  UiExitCodes default_exit_code_ = kUiecSuccess;
  std::string gls_email_;
  std::string gaia_id_override_;
  base::WaitableEvent gls_done_;
  base::win::ScopedHandle process_continue_event_;
  base::string16 start_gls_event_name_;
  CComBSTR error_text_;
  bool gls_process_started_ = false;
};

template <class T>
CTestCredentialBase<T>::CTestCredentialBase()
    : gls_email_(kDefaultEmail),
      gls_done_(base::WaitableEvent::ResetPolicy::MANUAL,
                base::WaitableEvent::InitialState::NOT_SIGNALED) {}

template <class T>
CTestCredentialBase<T>::~CTestCredentialBase() {}

template <class T>
HRESULT CTestCredentialBase<T>::SetDefaultExitCode(
    UiExitCodes default_exit_code) {
  default_exit_code_ = default_exit_code;
  return S_OK;
}

template <class T>
HRESULT CTestCredentialBase<T>::SetGlsEmailAddress(const std::string& email) {
  gls_email_ = email;
  return S_OK;
}

template <class T>
HRESULT CTestCredentialBase<T>::SetGaiaIdOverride(const std::string& gaia_id) {
  gaia_id_override_ = gaia_id;
  return S_OK;
}

template <class T>
HRESULT CTestCredentialBase<T>::WaitForGls() {
  return !gls_process_started_ ||
                 gls_done_.TimedWait(base::TimeDelta::FromSeconds(30))
             ? S_OK
             : HRESULT_FROM_WIN32(WAIT_TIMEOUT);
}

template <class T>
HRESULT CTestCredentialBase<T>::SetStartGlsEventName(
    const base::string16& event_name) {
  if (!start_gls_event_name_.empty())
    return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
  start_gls_event_name_ = event_name;
  return S_OK;
}

template <class T>
BSTR CTestCredentialBase<T>::GetFinalUsername() {
  return this->get_username();
}

template <class T>
std::string CTestCredentialBase<T>::GetFinalEmail() {
  const base::DictionaryValue* results = this->get_authentication_results();

  if (!results)
    return std::string();

  const base::Value* email_value =
      results->FindKeyOfType(kKeyEmail, base::Value::Type::STRING);

  if (!email_value)
    return std::string();

  return email_value->GetString();
}

template <class T>
BSTR CTestCredentialBase<T>::GetErrorText() {
  return error_text_;
}

template <class T>
bool CTestCredentialBase<T>::AreCredentialsValid() {
  return T::AreCredentialsValid();
}

template <class T>
bool CTestCredentialBase<T>::CanAttemptWindowsLogon() {
  return T::CanAttemptWindowsLogon();
}

template <class T>
bool CTestCredentialBase<T>::IsWindowsPasswordValidForStoredUser() {
  return T::IsWindowsPasswordValidForStoredUser(
             this->get_current_windows_password()) == S_OK;
}

template <class T>
void CTestCredentialBase<T>::SetWindowsPassword(
    const CComBSTR& windows_password) {
  this->set_current_windows_password(windows_password);
}

template <class T>
bool CTestCredentialBase<T>::IsGlsRunning() {
  return this->IsGaiaLogonStubRunning();
}

template <class T>
void CTestCredentialBase<T>::SignalGlsCompletion() {
  gls_done_.Signal();
}

template <class T>
HRESULT CTestCredentialBase<T>::GetBaseGlsCommandline(
    base::CommandLine* command_line) {
  return FakeGlsRunHelper::GetFakeGlsCommandline(
      default_exit_code_, gls_email_, gaia_id_override_, start_gls_event_name_,
      command_line);
}

template <class T>
HRESULT CTestCredentialBase<T>::ForkGaiaLogonStub(
    OSProcessManager* process_manager,
    const base::CommandLine& command_line,
    CGaiaCredentialBase::UIProcessInfo* uiprocinfo) {
  HRESULT hr = T::ForkGaiaLogonStub(process_manager, command_line, uiprocinfo);

  if (SUCCEEDED(hr)) {
    gls_process_started_ = true;
    // Reset the manual event since GLS has started.
    gls_done_.Reset();
  }

  return hr;
}

template <class T>
HRESULT CTestCredentialBase<T>::ForkSaveAccountInfoStub(
    const std::unique_ptr<base::DictionaryValue>& dict,
    BSTR* status_text) {
  return CGaiaCredentialBase::SaveAccountInfo(*dict);
}

template <class T>
HRESULT CTestCredentialBase<T>::OnUserAuthenticated(BSTR authentication_info,
                                                    BSTR* status_text) {
  HRESULT hr = T::OnUserAuthenticated(authentication_info, status_text);
  // Only signal completion if OnUserAuthenticated succeeded otherwise
  // there will be a call to ReportError right after which should signal
  // the completion. This is needed to prevent a race condition in tests
  // where it checks for a failure message, but the failure message is
  // set after gls_done_ is signalled causing the test to be flaky.
  if (SUCCEEDED(hr))
    SignalGlsCompletion();
  return hr;
}

template <class T>
HRESULT CTestCredentialBase<T>::ReportError(LONG status,
                                            LONG substatus,
                                            BSTR status_text) {
  // This function is called instead of (or after) OnUserAuthenticated() when
  // errors occur, so signal that GLS is done.
  HRESULT hr = T::ReportError(status, substatus, status_text);
  SignalGlsCompletion();
  return hr;
}

template <class T>
void CTestCredentialBase<T>::DisplayErrorInUI(LONG status,
                                              LONG substatus,
                                              BSTR status_text) {
  error_text_ = status_text;
  T::DisplayErrorInUI(status, substatus, status_text);
}

template <class T>
void CTestCredentialBase<T>::ResetInternalState() {
  gls_process_started_ = false;
  T::ResetInternalState();
}

// This class is used to implement a test credential based off a fully
// implemented CGaiaCredentialBase class.
template <class T>
class ATL_NO_VTABLE CTestCredentialForBaseInherited
    : public CTestCredentialBase<T> {
 public:
  DECLARE_NO_REGISTRY()

  CTestCredentialForBaseInherited();
  ~CTestCredentialForBaseInherited();

 private:
  BEGIN_COM_MAP(CTestCredentialForBaseInherited)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential2)
  COM_INTERFACE_ENTRY(ITestCredential)
  END_COM_MAP()
};

template <class T>
CTestCredentialForBaseInherited<T>::CTestCredentialForBaseInherited() = default;

template <class T>
CTestCredentialForBaseInherited<T>::~CTestCredentialForBaseInherited() =
    default;

template <class T>
HRESULT CreateBaseInheritedCredential(
    ICredentialProviderCredential** credential) {
  return CComCreator<CComObject<testing::CTestCredentialForBaseInherited<T>>>::
      CreateInstance(nullptr, IID_ICredentialProviderCredential,
                     reinterpret_cast<void**>(credential));
}

template <class T>
HRESULT CreateBaseInheritedCredentialWithProvider(
    IGaiaCredentialProvider* provider,
    IGaiaCredential** gaia_credential,
    ICredentialProviderCredential** credential) {
  HRESULT hr = CreateBaseInheritedCredential<T>(credential);
  if (SUCCEEDED(hr)) {
    hr = (*credential)
             ->QueryInterface(IID_IGaiaCredential,
                              reinterpret_cast<void**>(gaia_credential));
    if (SUCCEEDED(hr))
      hr = (*gaia_credential)->Initialize(provider);
  }
  return hr;
}

// This class is used to implement a test credential based off a fully
// implemented CGaiaCredentialBase class. The additional InterfaceT parameter
// is used to specify any additional interfaces that should be registerd for
// this class that is not part of CGaiaCredentialBase (this is used to
// implement a test credential for CReauthCredential which implements the
// additional IReauthCredential interface)
template <class T, class InterfaceT>
class ATL_NO_VTABLE CTestCredentialForInherited
    : public CTestCredentialBase<T> {
 public:
  DECLARE_NO_REGISTRY()

  CTestCredentialForInherited();
  ~CTestCredentialForInherited();

 private:
  BEGIN_COM_MAP(CTestCredentialForInherited)
  COM_INTERFACE_ENTRY(IGaiaCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential)
  COM_INTERFACE_ENTRY(ICredentialProviderCredential2)
  COM_INTERFACE_ENTRY(InterfaceT)
  COM_INTERFACE_ENTRY(ITestCredential)
  END_COM_MAP()
};

template <class T, class InterfaceT>
CTestCredentialForInherited<T, InterfaceT>::CTestCredentialForInherited() =
    default;

template <class T, class InterfaceT>
CTestCredentialForInherited<T, InterfaceT>::~CTestCredentialForInherited() =
    default;

template <class T, class InterfaceT>
HRESULT CreateInheritedCredential(ICredentialProviderCredential** credential) {
  return CComCreator<CComObject<testing::CTestCredentialForInherited<
      T, InterfaceT>>>::CreateInstance(nullptr,
                                       IID_ICredentialProviderCredential,
                                       reinterpret_cast<void**>(credential));
}

template <class T, class InterfaceT>
HRESULT CreateInheritedCredentialWithProvider(
    IGaiaCredentialProvider* provider,
    IGaiaCredential** gaia_credential,
    ICredentialProviderCredential** credential) {
  HRESULT hr = CreateInheritedCredential<T, InterfaceT>(credential);
  if (SUCCEEDED(hr)) {
    hr = (*credential)
             ->QueryInterface(IID_IGaiaCredential,
                              reinterpret_cast<void**>(gaia_credential));
    if (SUCCEEDED(hr))
      hr = (*gaia_credential)->Initialize(provider);
  }
  return hr;
}

}  // namespace testing
}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_TEST_CREDENTIAL_H_
