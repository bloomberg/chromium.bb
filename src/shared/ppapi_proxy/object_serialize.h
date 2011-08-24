/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_SERIALIZE_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_SERIALIZE_H_

#include "ppapi/c/pp_var.h"

struct NaClSrpcChannel;

namespace ppapi_proxy {

const uint32_t kMaxVarSize = 64 * 1024;

// Serialize one PP_Var to the location given in "bytes", using no more
// than "*length" bytes .  If successful, "*length" reflects the number of
// bytes written and true is returned.  Otherwise returns false.
bool SerializeTo(const PP_Var* var, char* bytes, uint32_t* length);

// Serialize a vector of "argc" PP_Vars to a buffer to be allocated by new[].
// If successful, the address of a buffer is returned and "*length" is set
// to the number of bytes allocated.  Otherwise, NULL is returned.
char* Serialize(const PP_Var* vars, uint32_t argc, uint32_t* length);

// Deserialize a vector "bytes" of "length" bytes containing "argc" PP_Vars
// into the vector of PP_Vars pointed to by "vars".  Returns true if
// successful, or false otherwise.
bool DeserializeTo(NaClSrpcChannel* channel,
                   char* bytes,
                   uint32_t length,
                   uint32_t argc,
                   PP_Var* vars);

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_OBJECT_SERIALIZE_H_
