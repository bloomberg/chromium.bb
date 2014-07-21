// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Information about the current process.

#include "rlz/win/lib/process_info.h"

#include <windows.h>
#include <Sddl.h>  // For ConvertSidToStringSid.
#include <LMCons.h>  // For UNLEN

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process_handle.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "rlz/lib/assert.h"

namespace {

HRESULT GetCurrentUser(std::wstring* name,
                       std::wstring* domain,
                       std::wstring* sid) {
  DWORD err;

  // Get the current username & domain the hard way.  (GetUserNameEx would be
  // nice, but unfortunately requires connectivity to a domain controller.
  // Useless.)

  // (Following call doesn't work if running as a Service - because a Service
  // runs under special accounts like LOCAL_SYSTEM, not as the logged in user.
  // In which case, search for and use the process handle of a running
  // Explorer.exe.)
  HANDLE token;

  CHECK(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token));

  base::win::ScopedHandle scoped_process_token(token);

  // (Following call will fail with ERROR_INSUFFICIENT_BUFFER and give us the
  // required size.)
  scoped_ptr<char[]> token_user_bytes;
  DWORD token_user_size;
  DWORD token_user_size2;
  BOOL result = ::GetTokenInformation(token, TokenUser, NULL, 0,
                                      &token_user_size);
  err = ::GetLastError();
  CHECK(!result && err == ERROR_INSUFFICIENT_BUFFER);

  token_user_bytes.reset(new char[token_user_size]);
  CHECK(token_user_bytes.get());

  CHECK(::GetTokenInformation(token, TokenUser, token_user_bytes.get(),
                              token_user_size, &token_user_size2));

  WCHAR user_name[UNLEN + 1];  // max username length
  WCHAR domain_name[UNLEN + 1];
  DWORD user_name_size = UNLEN + 1;
  DWORD domain_name_size = UNLEN + 1;
  SID_NAME_USE sid_type;
  TOKEN_USER* token_user =
      reinterpret_cast<TOKEN_USER*>(token_user_bytes.get());
  CHECK(token_user);

  PSID user_sid = token_user->User.Sid;
  CHECK(::LookupAccountSidW(NULL, user_sid, user_name, &user_name_size,
                            domain_name, &domain_name_size, &sid_type));

  if (name != NULL) {
    *name = user_name;
  }
  if (domain != NULL) {
    *domain = domain_name;
  }
  if (sid != NULL) {
    LPWSTR string_sid;
    ConvertSidToStringSidW(user_sid, &string_sid);
    *sid = string_sid;  // copy out to cstring
    // free memory, as documented for ConvertSidToStringSid
    LocalFree(string_sid);
  }

  return S_OK;
}

HRESULT GetElevationType(PTOKEN_ELEVATION_TYPE elevation) {
  if (!elevation)
    return E_POINTER;

  *elevation = TokenElevationTypeDefault;

  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return E_FAIL;

  HANDLE process_token;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token))
    return HRESULT_FROM_WIN32(GetLastError());

  base::win::ScopedHandle scoped_process_token(process_token);

  DWORD size;
  TOKEN_ELEVATION_TYPE elevation_type;
  if (!GetTokenInformation(process_token, TokenElevationType, &elevation_type,
                           sizeof(elevation_type), &size)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  *elevation = elevation_type;
  return S_OK;
}

// based on http://msdn2.microsoft.com/en-us/library/aa376389.aspx
bool GetUserGroup(long* group) {
  if (!group)
    return false;

  *group = 0;

  // groups are listed in DECREASING order of importance
  // (eg. If a user is a member of both the admin group and
  // the power user group, it is more useful to list the user
  // as an admin)
  DWORD user_groups[] =  {DOMAIN_ALIAS_RID_ADMINS,
                          DOMAIN_ALIAS_RID_POWER_USERS};
  SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;

  for (int i = 0; i < arraysize(user_groups) && *group == 0; ++i) {
    PSID current_group;
    if (AllocateAndInitializeSid(&nt_authority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 user_groups[i], 0, 0, 0, 0,
                                 0, 0, &current_group)) {
      BOOL current_level;
      if (CheckTokenMembership(NULL, current_group, &current_level) &&
          current_level) {
        *group = user_groups[i];
      }

      FreeSid(current_group);
    }
  }

  return group != 0;
}
}  //anonymous


namespace rlz_lib {

bool ProcessInfo::IsRunningAsSystem() {
  static std::wstring name;
  static std::wstring domain;
  static std::wstring sid;
  if (name.empty())
    CHECK(SUCCEEDED(GetCurrentUser(&name, &domain, &sid)));

  return (name == L"SYSTEM");
}

bool ProcessInfo::HasAdminRights() {
  static bool evaluated = false;
  static bool has_rights = false;

  if (!evaluated) {
    if (IsRunningAsSystem()) {
      has_rights = true;
    } else if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
      TOKEN_ELEVATION_TYPE elevation;
      base::IntegrityLevel level;

      if (SUCCEEDED(GetElevationType(&elevation)) &&
        base::GetProcessIntegrityLevel(base::GetCurrentProcessHandle(), &level))
        has_rights = (elevation == TokenElevationTypeFull) ||
                     (level == base::HIGH_INTEGRITY);
    } else {
      long group = 0;
      if (GetUserGroup(&group))
        has_rights = (group == DOMAIN_ALIAS_RID_ADMINS);
    }
  }

  evaluated = true;
  if (!has_rights)
    ASSERT_STRING("ProcessInfo::HasAdminRights: Does not have admin rights.");

  return has_rights;
}

};  // namespace
