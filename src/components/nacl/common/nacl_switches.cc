// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_switches.h"

namespace switches {

// Disables crash throttling for Portable Native Client.
const char kDisablePnaclCrashThrottling[]   = "disable-pnacl-crash-throttling";

// Enables debugging via RSP over a socket.
const char kEnableNaClDebug[]               = "enable-nacl-debug";

// Enables Non-SFI mode, in which programs can be run without NaCl's SFI
// sandbox.
const char kEnableNaClNonSfiMode[]          = "enable-nacl-nonsfi-mode";

// Force use of the Subzero as the PNaCl translator instead of LLC.
const char kForcePNaClSubzero[] = "force-pnacl-subzero";

// Value for --type that causes the process to run as a NativeClient broker
// (used for launching NaCl loader processes on 64-bit Windows).
const char kNaClBrokerProcess[]             = "nacl-broker";

// Disable sandbox even for non SFI mode. This is particularly unsafe
// as non SFI NaCl heavily relies on the seccomp sandbox.
const char kNaClDangerousNoSandboxNonSfi[]  =
    "nacl-dangerous-no-sandbox-nonsfi";

// Uses NaCl manifest URL to choose whether NaCl program will be debugged by
// debug stub.
// Switch value format: [!]pattern1,pattern2,...,patternN. Each pattern uses
// the same syntax as patterns in Chrome extension manifest. The only difference
// is that * scheme matches all schemes instead of matching only http and https.
// If the value doesn't start with !, a program will be debugged if manifest URL
// matches any pattern. If the value starts with !, a program will be debugged
// if manifest URL does not match any pattern.
const char kNaClDebugMask[]                 = "nacl-debug-mask";

// GDB script to pass to the nacl-gdb debugger at startup.
const char kNaClGdbScript[]                 = "nacl-gdb-script";

// Native Client GDB debugger that will be launched automatically when needed.
const char kNaClGdb[]                       = "nacl-gdb";

// Value for --type that causes the process to run as a NativeClient loader
// for non SFI mode.
const char kNaClLoaderNonSfiProcess[]       = "nacl-loader-nonsfi";

// Value for --type that causes the process to run as a NativeClient loader
// for SFI mode.
const char kNaClLoaderProcess[]             = "nacl-loader";

}  // namespace switches
