/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <iostream>
#include <vector>

#include "native_client/tests/posix_over_srpc/launcher.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: <posix_over_srpc_launcher> <path to sel_ldr>"
        << "<nacl module> [list of nacl module parameters]\n";
    return 0;
  }
  setenv("NACL_SEL_LDR", argv[1], 1);

  std::vector<nacl::string> app_argv(argv + 3, argv + argc);
  PosixOverSrpcLauncher psrpc_launcher;
  bool spawn_code = psrpc_launcher.SpawnNaClModule(argv[2], app_argv);
  return (true != spawn_code);
}
