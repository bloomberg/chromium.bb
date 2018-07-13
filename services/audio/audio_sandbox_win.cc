// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_sandbox_win.h"

#include "sandbox/win/src/sandbox_policy.h"

// NOTE: changes to this code need to be reviewed by the security team.

namespace audio {

//------------------------------------------------------------------------------
// Public audio service sandbox configuration extension functions.
//------------------------------------------------------------------------------
//
//  Default policy:
//
//  lockdown_level_(sandbox::USER_LOCKDOWN),
//  initial_level_(sandbox::USER_RESTRICTED_SAME_ACCESS),
//
//  job_level_(sandbox::JOB_LOCKDOWN),
//
//  integrity_level_(sandbox::INTEGRITY_LEVEL_LOW),
//  delayed_integrity_level_(sandbox::INTEGRITY_LEVEL_UNTRUSTED),

bool AudioPreSpawnTarget(sandbox::TargetPolicy* policy) {
  // Audio process privilege requirements:
  //  - Lockdown level of USER_LIMITED
  //  - Delayed integrity level of INTEGRITY_LEVEL_LOW
  //
  // For audio streams to create shared memory regions, both settings are
  // needed. If either of them is not set, CreateFileMapping() will fail with
  // error code ERROR_ACCESS_DENIED (0x5).
  policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                        sandbox::USER_LIMITED);
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  return true;
}

}  // namespace audio
