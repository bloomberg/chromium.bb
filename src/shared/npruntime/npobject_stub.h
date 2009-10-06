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

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_STUB_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_STUB_H_

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npcapability.h"
#include "native_client/src/shared/npruntime/nprpc.h"

namespace nacl {

class NPBridge;
class RpcArg;

// An NPObjectStub wraps an NPObject and converts IMC messages from
// NPObjectProxy to calls to the object.
class NPObjectStub {
  // The NPP instance to which this stub belongs.
  NPP npp_;
  // The NPObject for which this stub instance is created.
  NPObject* object_;

 public:
  // Creates a new instance of NPObjectStub with the specified NPP for
  // the specified local NPObject.
  NPObjectStub(NPP npp, NPObject* object);
  ~NPObjectStub();

  // Returns true if this stub instance is created for the local NPObject
  // represented by the specified capability.
  bool IsMatch(const NPCapability& capability) const {
    return object_ == capability.object;
  }

  NPP npp() const {
    return npp_;
  }

  NPObject* object() const {
    return object_;
  }

  // Gets the NPObjectStub corresponding to a particular capability.
  // Used by the SRPC methods to determine the object stub to dispatch to.
  static NPObjectStub* Lookup(NaClSrpcChannel* channel, RpcArg* arg);

  // The SRPC methods for all of the NPObject interfaces.
  static NaClSrpcError Deallocate(NaClSrpcChannel* channel,
                                  NaClSrpcArg** inputs,
                                  NaClSrpcArg** outputs);
  static NaClSrpcError Invalidate(NaClSrpcChannel* channel,
                                  NaClSrpcArg** inputs,
                                  NaClSrpcArg** outputs);
  static NaClSrpcError HasMethod(NaClSrpcChannel* channel,
                                 NaClSrpcArg** inputs,
                                 NaClSrpcArg** outputs);
  static NaClSrpcError Invoke(NaClSrpcChannel* channel,
                              NaClSrpcArg** inputs,
                              NaClSrpcArg** outputs);
  static NaClSrpcError InvokeDefault(NaClSrpcChannel* channel,
                                     NaClSrpcArg** inputs,
                                     NaClSrpcArg** outputs);
  static NaClSrpcError HasProperty(NaClSrpcChannel* channel,
                                   NaClSrpcArg** inputs,
                                   NaClSrpcArg** outputs);
  static NaClSrpcError GetProperty(NaClSrpcChannel* channel,
                                   NaClSrpcArg** inputs,
                                   NaClSrpcArg** outputs);
  static NaClSrpcError SetProperty(NaClSrpcChannel* channel,
                                   NaClSrpcArg** inputs,
                                   NaClSrpcArg** outputs);
  static NaClSrpcError RemoveProperty(NaClSrpcChannel* channel,
                                      NaClSrpcArg** inputs,
                                      NaClSrpcArg** outputs);
  static NaClSrpcError Enumerate(NaClSrpcChannel* channel,
                                 NaClSrpcArg** inputs,
                                 NaClSrpcArg** outputs);
  static NaClSrpcError Construct(NaClSrpcChannel* channel,
                                 NaClSrpcArg** inputs,
                                 NaClSrpcArg** outputs);
  static NaClSrpcError SetException(NaClSrpcChannel* channel,
                                    NaClSrpcArg** inputs,
                                    NaClSrpcArg** outputs);

  // This implementation is an exception because it is also called from
  // npn_gate.cc
  void SetExceptionImpl(const NPUTF8* message);

 private:
  // NPClass methods. NPObjectStub forwards these RPC requests received from
  // the remote NPObjectProxy object to object_;
  void DeallocateImpl();
  void InvalidateImpl();
  bool HasMethodImpl(NPIdentifier name);
  NPError InvokeImpl(NPIdentifier name,
                     const NPVariant* args,
                     uint32_t arg_count,
                     NPVariant* result);
  NPError InvokeDefaultImpl(const NPVariant* args,
                            uint32_t arg_count,
                            NPVariant* result);
  bool HasPropertyImpl(NPIdentifier name);
  bool GetPropertyImpl(NPIdentifier name, NPVariant* result);
  bool SetPropertyImpl(NPIdentifier name, const NPVariant* variant);
  bool RemovePropertyImpl(NPIdentifier name);
  bool EnumerateImpl(NPIdentifier** identifiers,
                     uint32_t* identifier_count);
  bool ConstructImpl(const NPVariant* args,
                     uint32_t arg_count,
                     NPVariant* result);
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPOBJECT_STUB_H_
