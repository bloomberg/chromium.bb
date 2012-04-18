// Copyright (c) 2012 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <map>
#include <string>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/sel_universal/pnacl_emu.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/srpc_helper.h"

using std::string;

namespace {

NaClCommandLoop* g_ncl;

// Maps from a saved key to the sel_universal variable name that refers to
// a file.
std::map<string, string> g_key_to_varname;

// LookupInputFile:s:h
void LookupInputFile(SRPC_PARAMS) {
  string key(ins[0]->arrays.str);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClLog(1, "LookupInputFile(%s)\n", key.c_str());
  if (g_key_to_varname.find(key) == g_key_to_varname.end()) {
    return;
  }
  string varname = g_key_to_varname[key];
  NaClLog(1, "LookupInputFile(%s) -> %s\n", key.c_str(), varname.c_str());
  outs[0]->u.hval = g_ncl->FindDescByName(varname);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

}  // end namespace


#define TUPLE(a, b) #a #b, a
void PnaclEmulateCoordinator(NaClCommandLoop* ncl) {
  // Register file lookup RPC.
  ncl->AddUpcallRpc(TUPLE(LookupInputFile, :s:h));
  g_ncl = ncl;
}

void PnaclEmulateAddKeyVarnameMapping(const std::string& key,
                                      const std::string& varname) {
  NaClLog(1, "AddKeyVarnameMapping(%s) -> %s\n", key.c_str(),
          varname.c_str());
  CHECK(g_key_to_varname.find(key) == g_key_to_varname.end());
  g_key_to_varname[string(key)] = string(varname);
}
