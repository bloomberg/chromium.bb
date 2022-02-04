// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_API_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "extensions/browser/extension_function.h"

namespace ash {
class AuthStatusConsumer;
class ExtendedAuthenticator;
}  // namespace ash

namespace extensions {

class QuickUnlockPrivateGetAuthTokenFunction : public ExtensionFunction {
 public:
  using AuthenticatorAllocator =
      base::RepeatingCallback<ash::ExtendedAuthenticator*(
          ash::AuthStatusConsumer* auth_status_consumer)>;

  QuickUnlockPrivateGetAuthTokenFunction();
  QuickUnlockPrivateGetAuthTokenFunction(
      const QuickUnlockPrivateGetAuthTokenFunction&) = delete;
  QuickUnlockPrivateGetAuthTokenFunction& operator=(
      const QuickUnlockPrivateGetAuthTokenFunction&) = delete;

  // Use the given |allocator| to create an ExtendedAuthenticator instance. This
  // lets tests intercept authentication calls.
  void SetAuthenticatorAllocatorForTesting(
      const AuthenticatorAllocator& allocator);

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.getAuthToken",
                             QUICKUNLOCKPRIVATE_GETAUTHTOKEN)

 protected:
  ~QuickUnlockPrivateGetAuthTokenFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // Continuation of Run(). Params match
  // QuickUnlockPrivateGetAuthTokenHelper::ResultCallback.
  void OnResult(
      bool success,
      std::unique_ptr<api::quick_unlock_private::TokenInfo> token_info,
      const std::string& error_message);

 private:
  ChromeExtensionFunctionDetails chrome_details_;
  scoped_refptr<ash::ExtendedAuthenticator> extended_authenticator_;
  AuthenticatorAllocator authenticator_allocator_;
};

class QuickUnlockPrivateSetLockScreenEnabledFunction
    : public ExtensionFunction {
 public:
  QuickUnlockPrivateSetLockScreenEnabledFunction();
  QuickUnlockPrivateSetLockScreenEnabledFunction(
      const QuickUnlockPrivateSetLockScreenEnabledFunction&) = delete;
  QuickUnlockPrivateSetLockScreenEnabledFunction& operator=(
      const QuickUnlockPrivateSetLockScreenEnabledFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.setLockScreenEnabled",
                             QUICKUNLOCKPRIVATE_SETLOCKSCREENENABLED)

 protected:
  ~QuickUnlockPrivateSetLockScreenEnabledFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;
};

class QuickUnlockPrivateSetPinAutosubmitEnabledFunction
    : public ExtensionFunction {
 public:
  QuickUnlockPrivateSetPinAutosubmitEnabledFunction();
  QuickUnlockPrivateSetPinAutosubmitEnabledFunction(
      const QuickUnlockPrivateSetPinAutosubmitEnabledFunction&) = delete;
  QuickUnlockPrivateSetPinAutosubmitEnabledFunction& operator=(
      const QuickUnlockPrivateSetPinAutosubmitEnabledFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.setPinAutosubmitEnabled",
                             QUICKUNLOCKPRIVATE_SETPINAUTOSUBMITENABLED)

 protected:
  ~QuickUnlockPrivateSetPinAutosubmitEnabledFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void HandleSetPinAutoSubmitResult(bool result);

  ChromeExtensionFunctionDetails chrome_details_;
};

class QuickUnlockPrivateCanAuthenticatePinFunction : public ExtensionFunction {
 public:
  QuickUnlockPrivateCanAuthenticatePinFunction();
  QuickUnlockPrivateCanAuthenticatePinFunction(
      const QuickUnlockPrivateCanAuthenticatePinFunction&) = delete;
  QuickUnlockPrivateCanAuthenticatePinFunction& operator=(
      const QuickUnlockPrivateCanAuthenticatePinFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.canAuthenticatePin",
                             QUICKUNLOCKPRIVATE_CANAUTHENTICATEPIN)

 protected:
  ~QuickUnlockPrivateCanAuthenticatePinFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void HandleCanAuthenticateResult(bool result);

  ChromeExtensionFunctionDetails chrome_details_;
};

class QuickUnlockPrivateGetAvailableModesFunction : public ExtensionFunction {
 public:
  QuickUnlockPrivateGetAvailableModesFunction();
  QuickUnlockPrivateGetAvailableModesFunction(
      const QuickUnlockPrivateGetAvailableModesFunction&) = delete;
  QuickUnlockPrivateGetAvailableModesFunction& operator=(
      const QuickUnlockPrivateGetAvailableModesFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.getAvailableModes",
                             QUICKUNLOCKPRIVATE_GETAVAILABLEMODES)

 protected:
  ~QuickUnlockPrivateGetAvailableModesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;
};

class QuickUnlockPrivateGetActiveModesFunction : public ExtensionFunction {
 public:
  QuickUnlockPrivateGetActiveModesFunction();
  QuickUnlockPrivateGetActiveModesFunction(
      const QuickUnlockPrivateGetActiveModesFunction&) = delete;
  QuickUnlockPrivateGetActiveModesFunction& operator=(
      const QuickUnlockPrivateGetActiveModesFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.getActiveModes",
                             QUICKUNLOCKPRIVATE_GETACTIVEMODES)

 protected:
  ~QuickUnlockPrivateGetActiveModesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnGetActiveModes(
      const std::vector<api::quick_unlock_private::QuickUnlockMode>& modes);

  ChromeExtensionFunctionDetails chrome_details_;
};

class QuickUnlockPrivateCheckCredentialFunction : public ExtensionFunction {
 public:
  QuickUnlockPrivateCheckCredentialFunction();
  QuickUnlockPrivateCheckCredentialFunction(
      const QuickUnlockPrivateCheckCredentialFunction&) = delete;
  QuickUnlockPrivateCheckCredentialFunction& operator=(
      const QuickUnlockPrivateCheckCredentialFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.checkCredential",
                             QUICKUNLOCKPRIVATE_CHECKCREDENTIAL)

 protected:
  ~QuickUnlockPrivateCheckCredentialFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class QuickUnlockPrivateGetCredentialRequirementsFunction
    : public ExtensionFunction {
 public:
  QuickUnlockPrivateGetCredentialRequirementsFunction();
  QuickUnlockPrivateGetCredentialRequirementsFunction(
      const QuickUnlockPrivateGetCredentialRequirementsFunction&) = delete;
  QuickUnlockPrivateGetCredentialRequirementsFunction& operator=(
      const QuickUnlockPrivateGetCredentialRequirementsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.getCredentialRequirements",
                             QUICKUNLOCKPRIVATE_GETCREDENTIALREQUIREMENTS)

 protected:
  ~QuickUnlockPrivateGetCredentialRequirementsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class QuickUnlockPrivateSetModesFunction : public ExtensionFunction {
 public:
  using QuickUnlockMode =
      extensions::api::quick_unlock_private::QuickUnlockMode;
  using ModesChangedEventHandler =
      base::RepeatingCallback<void(const std::vector<QuickUnlockMode>&)>;

  QuickUnlockPrivateSetModesFunction();
  QuickUnlockPrivateSetModesFunction(
      const QuickUnlockPrivateSetModesFunction&) = delete;
  QuickUnlockPrivateSetModesFunction& operator=(
      const QuickUnlockPrivateSetModesFunction&) = delete;

  // The given event handler will be called whenever a
  // quickUnlockPrivate.onActiveModesChanged event is raised instead of the
  // default event handling mechanism.
  void SetModesChangedEventHandlerForTesting(
      const ModesChangedEventHandler& handler);

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.setModes",
                             QUICKUNLOCKPRIVATE_SETMODES)

 protected:
  ~QuickUnlockPrivateSetModesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // Continuation of OnAuthSuccess after active modes have been fetched.
  void OnGetActiveModes(const std::vector<QuickUnlockMode>& modes);

  void PinSetCallComplete(bool result);
  void PinRemoveCallComplete(bool result);

  // Apply any changes specified in |params_|. Returns the new active modes.
  void ModeChangeComplete(const std::vector<QuickUnlockMode>& updated_modes);

 private:
  void FireEvent(const std::vector<QuickUnlockMode>& modes);

  ChromeExtensionFunctionDetails chrome_details_;
  std::unique_ptr<api::quick_unlock_private::SetModes::Params> params_;

  std::vector<QuickUnlockMode> initial_modes_;

  ModesChangedEventHandler modes_changed_handler_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_API_H_
