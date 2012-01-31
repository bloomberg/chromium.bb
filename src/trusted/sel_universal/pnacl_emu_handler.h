/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PNACL_EMU_HANDLER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PNACL_EMU_HANDLER_H_

#include <string>
#include <vector>

class NaClCommandLoop;

// These are used in emulating the pnacl coordinator, which provides a service
// to the translator nexes that allows them to pass a string key and get a
// file descriptor back.

// Initialize the pnacl key to file descriptor lookup subsystem.
bool HandlerPnaclEmuInitialize(NaClCommandLoop* ncl,
                               const std::vector<std::string>& args);

// Add a mapping from key to a descriptor varname.
// In sel_universal opening a file with either readonly_file or readwrite_file
// creates a variable name that can be used in RPCs (i.e., h(varname) to pass
// the descriptor).  This handler adds an entry for key args[1] that contains
// the varname in args[2].  The lookup service uses this mapping to pass back
// the file descriptor.
bool HandlerPnaclEmuAddVarnameMapping(NaClCommandLoop* ncl,
                                      const std::vector<std::string>& args);

bool HandlerPnaclFileStream(NaClCommandLoop* ncl,
                            const std::vector<std::string>& args);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PNACL_EMU_HANDLER_H_*/
