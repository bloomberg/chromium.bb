/*
 * Copyright 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Routines for determining the most appropriate NaCl executable for
// the current CPU's architecture.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NEXE_ARCH_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NEXE_ARCH_H_

#include "native_client/src/include/portability.h"

namespace plugin {
// Returns the kind of SFI sandbox implemented by sel_ldr on this
// platform.  See the implementation in nexe_arch.cc for possible values.
//
// This is a function of the current CPU, OS, browser, installed
// sel_ldr(s).  It is not sufficient to derive the result only from
// build-time parameters since, for example, an x86-32 plugin is
// capable of launching a 64-bit NaCl sandbox if a 64-bit sel_ldr is
// installed (and indeed, may only be capable of launching a 64-bit
// sandbox).
//
// Note: The platform-sepcific implementations for this are under
// <platform>/nexe_arch.cc
const char* GetSandboxISA();
}  // namespace plugin

#if defined(NACL_STANDALONE)
// These symbols are for linking with things like unit test frameworks that
// want to get at the C++ symbol names defined above in an un-mangled way.
// TODO(dspringer): Figure out how to link the shared libs with the test
// harness directly, and get rid of these wrappers.
EXTERN_C_BEGIN
DLLEXPORT const char* NaClPluginGetSandboxISA();
EXTERN_C_END
#endif

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NEXE_ARCH_H_
