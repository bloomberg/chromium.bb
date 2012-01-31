/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/sel_universal/pnacl_emu_handler.h"

#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/trusted/sel_universal/pnacl_emu.h"

// ======================================================================
bool HandlerPnaclEmuInitialize(NaClCommandLoop* ncl,
                               const vector<string>& args) {
  UNREFERENCED_PARAMETER(args);
  NaClLog(LOG_INFO, "HandlerPnaclEmuInitialize\n");
  PnaclEmulateCoordinator(ncl);
  return true;
}

bool HandlerPnaclEmuAddVarnameMapping(NaClCommandLoop* ncl,
                                      const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }
  PnaclEmulateAddKeyVarnameMapping(args[1], args[2]);
  return true;
}
