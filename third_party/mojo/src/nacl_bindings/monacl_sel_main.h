// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_NACL_MONACL_SEL_MAIN_H_
#define MOJO_NACL_MONACL_SEL_MAIN_H_

#include "mojo/public/c/system/types.h"

struct NaClDesc;

namespace mojo {

// Callee assumes ownership of |nexe_desc|, |irt_desc|, and |handle|.
int LaunchNaCl(NaClDesc* nexe_desc,
               NaClDesc* irt_desc,
               int app_argc,
               char* app_argv[],
               MojoHandle handle);

void NaClExit(int code);

}  // namespace mojo

#endif  // MOJO_NACL_MONACL_SEL_MAIN_H_
