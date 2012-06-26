// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A library to manage RLZ information for access-points shared
// across different client applications.
//
// All functions return true on success and false on error.
// This implemenation is thread safe.
//
// Each prototype mentions the registry access requirements:
//
// HKLM read:  Will work from any process and at any privilege level on Vista.
// HKCU read:  Calls made from the SYSTEM account must pass the current user's
//             SID as the optional 'sid' param. Can be called from low integrity
//             process on Vista.
// HKCU write: Calls made from the SYSTEM account must pass the current user's
//             SID as the optional 'sid' param. Calls require at least medium
//             integrity on Vista (e.g. Toolbar will need to use their broker)
// HKLM write: Calls must be made from an account with admin rights. No SID
//             need be passed when running as SYSTEM.
// Functions which do not access registry will be marked with "no restrictions".

#ifndef RLZ_WIN_LIB_RLZ_LIB_H_
#define RLZ_WIN_LIB_RLZ_LIB_H_

// Clients can get away by just including rlz/lib/rlz_lib.h. This file only
// contains function definitions for files used by tests. It's mostly kept
// around for backwards-compatibility.

#include "rlz/lib/rlz_lib.h"

#include "base/win/registry.h"

namespace rlz_lib {

#if defined(OS_WIN)

// Initialize temporary HKLM/HKCU registry hives used for testing.
// Testing RLZ requires reading and writing to the Windows registry.  To keep
// the tests isolated from the machine's state, as well as to prevent the tests
// from causing side effects in the registry, HKCU and HKLM are overridden for
// the duration of the tests. RLZ tests don't expect the HKCU and KHLM hives to
// be empty though, and this function initializes the minimum value needed so
// that the test will run successfully.
//
// The two arguments to this function should be the keys that will represent
// the HKLM and HKCU registry hives during the tests.  This function should be
// called *before* the hives are overridden.
void InitializeTempHivesForTesting(const base::win::RegKey& temp_hklm_key,
                                   const base::win::RegKey& temp_hkcu_key);
#endif  // defined(OS_WIN)

}  // namespace rlz_lib

#endif  // RLZ_WIN_LIB_RLZ_LIB_H_
