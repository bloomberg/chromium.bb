// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_

#include "components/prefs/pref_registry_simple.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace login_api {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace login_api

namespace login_api_errors {

extern const char kAlreadyActiveSession[];
extern const char kAnotherLoginAttemptInProgress[];
extern const char kNoManagedGuestSessionAccounts[];
extern const char kNoPermissionToLock[];
extern const char kSessionIsNotActive[];
extern const char kNoPermissionToUnlock[];
extern const char kSessionIsNotLocked[];
extern const char kAnotherUnlockAttemptInProgress[];
extern const char kAuthenticationFailed[];

}  // namespace login_api_errors

class LoginLaunchManagedGuestSessionFunction : public ExtensionFunction {
 public:
  LoginLaunchManagedGuestSessionFunction();

  LoginLaunchManagedGuestSessionFunction(
      const LoginLaunchManagedGuestSessionFunction&) = delete;

  LoginLaunchManagedGuestSessionFunction& operator=(
      const LoginLaunchManagedGuestSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.launchManagedGuestSession",
                             LOGIN_LAUNCHMANAGEDGUESTSESSION)

 protected:
  ~LoginLaunchManagedGuestSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginExitCurrentSessionFunction : public ExtensionFunction {
 public:
  LoginExitCurrentSessionFunction();

  LoginExitCurrentSessionFunction(const LoginExitCurrentSessionFunction&) =
      delete;

  LoginExitCurrentSessionFunction& operator=(
      const LoginExitCurrentSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.exitCurrentSession",
                             LOGIN_EXITCURRENTSESSION)

 protected:
  ~LoginExitCurrentSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginFetchDataForNextLoginAttemptFunction : public ExtensionFunction {
 public:
  LoginFetchDataForNextLoginAttemptFunction();

  LoginFetchDataForNextLoginAttemptFunction(
      const LoginFetchDataForNextLoginAttemptFunction&) = delete;

  LoginFetchDataForNextLoginAttemptFunction& operator=(
      const LoginFetchDataForNextLoginAttemptFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.fetchDataForNextLoginAttempt",
                             LOGIN_FETCHDATAFORNEXTLOGINATTEMPT)

 protected:
  ~LoginFetchDataForNextLoginAttemptFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginLockManagedGuestSessionFunction : public ExtensionFunction {
 public:
  LoginLockManagedGuestSessionFunction();

  LoginLockManagedGuestSessionFunction(
      const LoginLockManagedGuestSessionFunction&) = delete;

  LoginLockManagedGuestSessionFunction& operator=(
      const LoginLockManagedGuestSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.lockManagedGuestSession",
                             LOGIN_LOCKMANAGEDGUESTSESSION)

 protected:
  ~LoginLockManagedGuestSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginUnlockManagedGuestSessionFunction : public ExtensionFunction {
 public:
  LoginUnlockManagedGuestSessionFunction();

  LoginUnlockManagedGuestSessionFunction(
      const LoginUnlockManagedGuestSessionFunction&) = delete;

  LoginUnlockManagedGuestSessionFunction& operator=(
      const LoginUnlockManagedGuestSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.unlockManagedGuestSession",
                             LOGIN_UNLOCKMANAGEDGUESTSESSION)

 protected:
  ~LoginUnlockManagedGuestSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnAuthenticationComplete(bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_
