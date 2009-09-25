/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// NaCl-NPAPI Interface

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPRPC_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPRPC_H_

// TODO(sehr): prune the included files.
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npcapability.h"

namespace nacl {

// The RPC message types.
enum RpcType {
  RPC_GET_WINDOW_OBJECT,          // NPN_GetValue() - NPNVWindowNPObject
  RPC_GET_PLUGIN_ELEMENT_OBJECT,  // NPN_GetValue() - NPNVPluginElementNPObject
  RPC_SET_EXCEPTION,              // NPNSetException()
  RPC_SET_STATUS,                 // NPN_Status()
  RPC_NEW,                        // NPP_New()
  RPC_DESTROY,                    // NPP_Destroy()
  RPC_CREATE_ARRAY,               // NaClNPN_CreateArray()
  RPC_OPEN_URL,                   // NaClNPN_OpenURL()
  RPC_NOTIFY_URL,                 // URLNotify()
  RPC_DEALLOCATE,                 // Deallocate
  RPC_INVALIDATE,                 // Invalidate
  RPC_HAS_METHOD,                 // HasMethod
  RPC_INVOKE,                     // Invoke
  RPC_INVOKE_DEFAULT,             // InvokeDefault
  RPC_HAS_PROPERTY,               // HasProperty
  RPC_GET_PROPERTY,               // GetProperty
  RPC_SET_PROPERTY,               // SetProperty
  RPC_REMOVE_PROPERTY,            // RemoveProperty
  RPC_ENUMERATION,                // Enumeration
  RPC_CONSTRUCT,                  // Construct
  RPC_INVALIDATE_RECT,            // NPN_InvalidateRect(NPRect*)
  RPC_FORCE_REDRAW,               // NPN_ForceRedraw()
  RPC_END_ENUM
};

// The RPC message header.
struct RpcHeader {
  RpcType   type;        // The RPC message type.
  int       tag;         // The tag to distinguish each message.
  int       pid;         // The process ID that sends the request message.
  int       error_code;  // The error code of this message.

  // Compares an RPC request with an RPC response and returns true if they
  // match, and vice versa.
  bool Equals(const RpcHeader& header) const {
    return type == header.type && tag == header.tag && pid == header.pid;
  }
};

// The maximum number of NPVariant parameters passed by RPC_INVOKE or
// RPC_INVOKE_DEFAULT.
const size_t kParamMax = 256;

// The maximum size of the NPVariant structure in bytes among various platforms.
const size_t kNPVariantSizeMax = 16;

// Converts an array of NPVariant into another array of NPVariant that packed
// differently to support different ABIs between the NaCl module and the
// browser plugin. The target NPVariant size is given by peer_npvariant_size.
//
// Currently ConvertNPVariants() supports only 16 byte NPVariant (Win32) to
// 12 byte NPVariant (Linux and OS X) conversion and vice versa.
void* ConvertNPVariants(const NPVariant* variant, void* target,
                        size_t peer_npvariant_size,
                        size_t arg_count);

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPRPC_H_
