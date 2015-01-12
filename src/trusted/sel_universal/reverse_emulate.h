/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_REVERSE_EMULATE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_REVERSE_EMULATE_H_

#include <string>
#include <vector>

#include "native_client/src/trusted/nonnacl_util/launcher_factory.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

class NaClCommandLoop;
struct NaClSrpcChannel;

namespace nacl {
class ReverseService;
}  // namespace nacl

// Starts an emulator for reverse service and handles the start up expected
// by nexes.  Returns true on success, false on failure.
bool ReverseEmulateInit(NaClSrpcChannel* command_channel,
                        nacl::SelLdrLauncherStandalone* launcher,
                        nacl::SelLdrLauncherStandaloneFactory* factory,
                        const std::vector<nacl::string>& prefix,
                        const std::vector<nacl::string>& sel_ldr_argv);

// Shuts down reverse service emulation.
void ReverseEmulateFini();

bool HandlerWaitForExit(NaClCommandLoop* ncl,
                        const std::vector<std::string>& args);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_REVERSE_EMULATE_H_ */
