// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_bindings/monacl_sel_main.h"

#include "nacl_bindings/mojo_syscall.h"
#include "native_client/src/public/chrome_main.h"
#include "native_client/src/public/nacl_app.h"

namespace mojo {

int LaunchNaCl(NaClDesc* nexe_desc,
               NaClDesc* irt_desc,
               int app_argc,
               char* app_argv[],
               MojoHandle handle) {
  NaClChromeMainInit();

  struct NaClChromeMainArgs* args = NaClChromeMainArgsCreate();
  args->nexe_desc = nexe_desc;
  args->irt_desc = irt_desc;

  args->argc = app_argc;
  args->argv = app_argv;

  struct NaClApp* nap = NaClAppCreate();
  InjectMojo(nap, handle);

  int exit_status = 1;
  NaClChromeMainStart(nap, args, &exit_status);
  return exit_status;
}

void NaClExit(int code) {
  ::NaClExit(code);
}

}  // namespace mojo
