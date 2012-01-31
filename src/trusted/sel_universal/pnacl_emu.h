/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PNACL_EMU_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PNACL_EMU_H_

#include <string>
class NaClCommandLoop;

// The PNaCl coordinator (for in-browser translation) exports a service that
// the sandboxed translator uses to lookup file descriptors.

// Start the PNaCl coordinator emulation.
void PnaclEmulateCoordinator(NaClCommandLoop* ncl);

// Add an entry for use by the lookup service.  The lookup service uses a
// key that the sandboxed module uses to lookup a descriptor.  The
// descriptor in sel_universal is named by a command-line variable 'varname'.
void PnaclEmulateAddKeyVarnameMapping(const std::string& key,
                                      const std::string& varname);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PNACL_EMU_H_ */
