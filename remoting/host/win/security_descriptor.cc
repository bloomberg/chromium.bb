// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/security_descriptor.h"

#include <sddl.h>

#include "base/string16.h"
#include "base/utf_string_conversions.h"

namespace remoting {

ScopedSd ConvertSddlToSd(const std::string& sddl) {
  PSECURITY_DESCRIPTOR raw_sd = NULL;
  ULONG length = 0;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
          UTF8ToUTF16(sddl).c_str(), SDDL_REVISION_1, &raw_sd, &length)) {
    return ScopedSd();
  }

  ScopedSd sd(length);
  memcpy(sd.get(), raw_sd, length);

  LocalFree(raw_sd);
  return sd.Pass();
}

// Converts a SID into a text string.
std::string ConvertSidToString(SID* sid) {
  char16* c_sid_string = NULL;
  if (!ConvertSidToStringSid(sid, &c_sid_string))
    return std::string();

  string16 sid_string(c_sid_string);
  LocalFree(c_sid_string);
  return UTF16ToUTF8(sid_string);
}

// Returns the logon SID of a token. Returns NULL if the token does not specify
// a logon SID or in case of an error.
ScopedSid GetLogonSid(HANDLE token) {
  DWORD length = 0;
  if (GetTokenInformation(token, TokenGroups, NULL, 0, &length) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return ScopedSid();
  }

  TypedBuffer<TOKEN_GROUPS> groups(length);
  if (!GetTokenInformation(token, TokenGroups, groups.get(), length, &length))
    return ScopedSid();

  for (uint32 i = 0; i < groups->GroupCount; ++i) {
    if ((groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) ==
        SE_GROUP_LOGON_ID) {
      length = GetLengthSid(groups->Groups[i].Sid);
      ScopedSid logon_sid(length);
      if (!CopySid(length, logon_sid.get(), groups->Groups[i].Sid))
        return ScopedSid();

      return logon_sid.Pass();
    }
  }

  return ScopedSid();
}

}  // namespace remoting
