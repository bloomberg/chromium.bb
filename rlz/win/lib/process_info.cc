// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Information about the current process.

#include "rlz/win/lib/process_info.h"

#include <windows.h>

#include "base/memory/scoped_ptr.h"
#include "base/process/process_info.h"
#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "rlz/lib/assert.h"

namespace {

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
  SID_IDENTIFIER_AUTHORITY nt_authority = {SECURITY_NT_AUTHORITY};

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
  static base::string16 user_sid;
  if (user_sid.empty()) {
    if (!base::win::GetUserSidString(&user_sid))
      return false;
  }
  return (user_sid == L"S-1-5-18");
}

bool ProcessInfo::HasAdminRights() {
  static bool evaluated = false;
  static bool has_rights = false;

  if (!evaluated) {
    if (IsRunningAsSystem()) {
      has_rights = true;
    } else if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
      TOKEN_ELEVATION_TYPE elevation;
      if (SUCCEEDED(GetElevationType(&elevation))) {
        base::IntegrityLevel level = base::GetCurrentProcessIntegrityLevel();
        if (level != base::INTEGRITY_UNKNOWN) {
          has_rights = (elevation == TokenElevationTypeFull) ||
                       (level == base::HIGH_INTEGRITY);
        }
      }
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
