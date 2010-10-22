/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Routines for determining the most appropriate NaCl executable for
// the current CPU's architecture.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NEXE_ARCH_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NEXE_ARCH_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_string.h"
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

// Parses the JSON pointed at by |nexe_manifest_json| and determines the URL of
// the nexe module appropriate for the NaCl sandbox implemented by the installed
// sel_ldr.  On success, |true| is returned and |*result| is updated with the
// URL taken out of the JSON.  On failure, |false| is returned, and |*result|
// is updated with an informative error message.
bool GetNexeURLFromManifest(const nacl::string& nexe_manifest_json,
                            nacl::string* result);

// Parses the deprecated <embed nexes="..."> attribute and determines the URL
// of the nexe module appropriate for the NaCl sandbox implemented by the
// installed sel_ldr.  On success, |true| is returned and |*result| is updated
// with the URL.  On failure, |false| is returned, and |*result| is updated with
// an informative error message.
bool GetNexeURL(const char* nexes_attr, nacl::string* result);

}  // namespace plugin

// These symbols are for linking with things like unit test frameworks that
// want to get at the C++ symbol names defined above in an un-mangled way.
// TODO(dspringer): Figure out how to link the shared libs with the test
// harness directly, and get rid of these wrappers.
EXTERN_C_BEGIN
DLLEXPORT const char* NaClPluginGetSandboxISA();
DLLEXPORT bool NaClPluginGetNexeURLFromManifest(
    const nacl::string& nexe_manifest_json, nacl::string* result);
EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NEXE_ARCH_H_
