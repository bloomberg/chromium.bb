// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains snippets borrowed from the Vista SDK version of
// WinNT.h, (c) Microsoft (2006)

#ifndef RLZ_WIN_LIB_VISTA_WINNT_H_
#define RLZ_WIN_LIB_VISTA_WINNT_H_

#include <windows.h>

// If no Vista SDK yet, borrow these from Vista's version of WinNT.h
#ifndef SE_GROUP_INTEGRITY

// TOKEN_MANDATORY_LABEL.Label.Attributes = SE_GROUP_INTEGRITY
#define SE_GROUP_INTEGRITY                 (0x00000020L)
#define SE_GROUP_INTEGRITY_ENABLED         (0x00000040L)

typedef struct _TOKEN_MANDATORY_LABEL {
    SID_AND_ATTRIBUTES Label;
} TOKEN_MANDATORY_LABEL, *PTOKEN_MANDATORY_LABEL;

// These are a few new enums for TOKEN_INFORMATION_CLASS
#define TokenElevationType static_cast<TOKEN_INFORMATION_CLASS>(18)
#define TokenLinkedToken static_cast<TOKEN_INFORMATION_CLASS>(19)
#define TokenElevation static_cast<TOKEN_INFORMATION_CLASS>(20)
#define TokenHasRestrictions static_cast<TOKEN_INFORMATION_CLASS>(21)
#define TokenAccessInformation static_cast<TOKEN_INFORMATION_CLASS>(22)
#define TokenVirtualizationAllowed static_cast<TOKEN_INFORMATION_CLASS>(23)
#define TokenVirtualizationEnabled static_cast<TOKEN_INFORMATION_CLASS>(24)
// TokenIntegrityLevel is the proces's privilege level, low, med, or high
#define TokenIntegrityLevel static_cast<TOKEN_INFORMATION_CLASS>(25)
// TokenIntegrityLevelDeasktop is an alternate level used for access apis
// (screen readers, imes)
#define TokenIntegrityLevelDesktop static_cast<TOKEN_INFORMATION_CLASS>(26)

// This is a new flag to pass to GetNamedSecurityInfo or SetNamedSecurityInfo
// that puts the mandatory level label info in an access control list (ACL)
// structure in the parameter normally used for system acls (SACL)
#define LABEL_SECURITY_INFORMATION       (0x00000010L)

// The new Access Control Entry type identifier for mandatory labels
#define SYSTEM_MANDATORY_LABEL_ACE_TYPE         (0x11)

// The structure of mandatory label acess control binary entry
typedef struct _SYSTEM_MANDATORY_LABEL_ACE {
    ACE_HEADER Header;
    ACCESS_MASK Mask;
    DWORD SidStart;
} SYSTEM_MANDATORY_LABEL_ACE, *PSYSTEM_MANDATORY_LABEL_ACE;

// Masks for ACCESS_MASK above
#define SYSTEM_MANDATORY_LABEL_NO_WRITE_UP         0x1
#define SYSTEM_MANDATORY_LABEL_NO_READ_UP          0x2
#define SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP       0x4
#define SYSTEM_MANDATORY_LABEL_VALID_MASK \
    (SYSTEM_MANDATORY_LABEL_NO_WRITE_UP | \
     SYSTEM_MANDATORY_LABEL_NO_READ_UP | \
     SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP)

// The SID authority for mandatory labels
#define SECURITY_MANDATORY_LABEL_AUTHORITY          {0, 0, 0, 0, 0, 16}

// the RID values (sub authorities) that define mandatory label levels
#define SECURITY_MANDATORY_UNTRUSTED_RID            (0x00000000L)
#define SECURITY_MANDATORY_LOW_RID                  (0x00001000L)
#define SECURITY_MANDATORY_MEDIUM_RID               (0x00002000L)
#define SECURITY_MANDATORY_HIGH_RID                 (0x00003000L)
#define SECURITY_MANDATORY_SYSTEM_RID               (0x00004000L)
#define SECURITY_MANDATORY_UI_ACCESS_RID            (0x00004100L)
#define SECURITY_MANDATORY_PROTECTED_PROCESS_RID    (0x00005000L)

// Vista's mandatory labels, enumerated
typedef enum _MANDATORY_LEVEL {
    MandatoryLevelUntrusted = 0,
    MandatoryLevelLow,
    MandatoryLevelMedium,
    MandatoryLevelHigh,
    MandatoryLevelSystem,
    MandatoryLevelSecureProcess,
    MandatoryLevelCount
} MANDATORY_LEVEL, *PMANDATORY_LEVEL;


// Token elevation values describe the relative strength of a given token.
// A full token is a token with all groups and privileges to which the
// principal is authorized.  A limited token is one with some groups or
// privileges removed.

typedef enum _TOKEN_ELEVATION_TYPE {
    TokenElevationTypeDefault = 1,
    TokenElevationTypeFull,
    TokenElevationTypeLimited,
} TOKEN_ELEVATION_TYPE, *PTOKEN_ELEVATION_TYPE;

#endif  // #ifndef SE_GROUP_INTEGRITY

#endif  // RLZ_WIN_LIB_VISTA_WINNT_H_
