// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_USER_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_USER_MANAGER_H_

#include "base/win/windows_types.h"

typedef wchar_t* BSTR;

namespace credential_provider {

// Manages OS users on the system.
class OSUserManager {
 public:
  // Minimum length for password buffer when calling GenerateRandomPassword().
  static const int kMinPasswordLength = 24;

  static OSUserManager* Get();

  virtual ~OSUserManager();

  // Generates a cryptographically secure random password.
  virtual HRESULT GenerateRandomPassword(wchar_t* password, int length);

  // Creates a new OS user on the system with the given credentials.  If
  // |add_to_users_group| is true, the Os user is added to the machine's
  // "Users" group which allows interactive logon.  The OS user's SID is
  // returned in |sid|.
  virtual HRESULT AddUser(const wchar_t* username,
                          const wchar_t* password,
                          const wchar_t* fullname,
                          const wchar_t* comment,
                          bool add_to_users_group,
                          BSTR* sid,
                          DWORD* error);

  // Changes the password of the given OS user.
  virtual HRESULT SetUserPassword(const wchar_t* username,
                                  const wchar_t* password,
                                  DWORD* error);

  // Gets the SID of the given OS user.  The caller owns the pointer |sid| and
  // should free it with a call to LocalFree().
  virtual HRESULT GetUserSID(const wchar_t* username, PSID* sid);

  // Returns NERR_Success if a user with the given SID exists, and
  // NERR_BadUsername otherwise.
  virtual HRESULT FindUserBySID(const wchar_t* sid);

  // Removes the user from the machine.
  virtual HRESULT RemoveUser(const wchar_t* username, const wchar_t* password);

  // This method is called either from FakeOSUserManager or from dllmain.cc when
  // setting fakes from one module to another.
  static void SetInstanceForTesting(OSUserManager* factory);

 protected:
  OSUserManager() {}

  // Returns the storage used for the instance pointer.
  static OSUserManager** GetInstanceStorage();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_OS_USER_MANAGER_H_
