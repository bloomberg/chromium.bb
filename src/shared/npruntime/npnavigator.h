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

#ifndef NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPNAVIGATOR_H_
#define NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPNAVIGATOR_H_

#ifndef __native_client__
#error "This file is only for inclusion in Native Client modules"
#endif  // __native_client__

#include <pthread.h>
#include <string.h>
#include <map>
#include <set>

#include "native_client/src/shared/npruntime/npbridge.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl {

// Closures used by PluginThreadAsyncCall.
class NPPendingCallClosure;

// Represents the NaCl module end of the connection to the browser plugin. The
// opposite end is the NPModule.
class NPNavigator : public NPBridge {
 public:
  // A typedef used to define callbacks used by NPN_PluginThreadAsyncCall.
  typedef void (*AsyncCallFunc)(void* user_data);

  // The map of pending calls for NPN_PluginThreadAsyncCalls.
  std::map<uint32_t, NPPendingCallClosure*> pending_calls_;
  // The identifier of the next pending call.
  static uint32_t next_pending_call;
  // The lock that guards pending call insertion/deletion.
  pthread_mutex_t pending_mu_;

  // The one navigator in this NaCl module.
  static NPNavigator* navigator;

  // The lookup table from browser NPP to NaCl module NPP.
  static std::map<NPP, NPP> *nacl_npp_map;
  // The lookup table from NaCl module NPP to browser plugin NPP.
  static std::map<NPP, NPP> *plugin_npp_map;

  // The SRPC methods the navigator exports to the browser plugin.
  static NACL_SRPC_METHOD_ARRAY(srpc_methods);

  // NPIdentifiers:
  // The browser controls the master identifier representation.
  // The navigator retains two local idetifier caches to reduce RPC traffic.
  static std::map<const NPUTF8*, NPIdentifier>* string_id_map;
  static std::map<int32_t, NPIdentifier>* int_id_map;
  // It also retains a mapping from NPIdentifier to NPUTF8* for name retrieval.
  static std::map<NPIdentifier, const NPUTF8*>* id_string_map;
  // And retains a mapping from NPIdentifier to int32_t for value retrieval.
  static std::map<NPIdentifier, int32_t>* id_int_map;

  // Canonical representation of strings used in identifier lookups.
  static const NPUTF8* InternString(const NPUTF8* name);
  // Compares two strings in string_set.
  struct StringCompare {
    bool operator()(const NPUTF8* left, const NPUTF8* right) const {
      return (strcmp(left, right) < 0) ? true : false;
    }
  };
  // The set of character strings for NPIdentifier names.
  static std::set<const NPUTF8*, StringCompare>* string_set;

 private:
  // The following three member variables save the user parameters passed to
  // OpenURL().
  //
  // The callback function to be called upon a successful OpenURL() invocation.
  void (*notify_)(const char* url,
                  void* notify_data,
                  NaClSrpcImcDescType handle);
  // The user supplied pointer to the user data.
  void* notify_data_;
  // The URL to open.
  char* url_;

  // The SRPC service used to send upcalls to the browser plugin.
  // This is used, among other things, to implement NPN_PluginThreadAsyncCall.
  NaClSrpcChannel* upcall_channel_;

 public:
  // Creates a new instance of NPNavigator. argc and argv should be unmodified
  // variables from NaClNP_Init(). npp must be a pointer to an NPP_t structure
  // to be used for the NaCl module.
  NPNavigator(uint32_t peer_pid, size_t peer_npvariant_size);
  ~NPNavigator();

  // Get the Navigator for the current module.
  static inline NPNavigator* GetNavigator() {
    return navigator;
  }

  // Get the NaCl module NPP from the browser NPP.
  static NPP GetNaClNPP(NPP plugin_npp, bool add_to_map);
  // Get the plugin NPP from the NaCl module NPP.
  static NPP GetPluginNPP(NPP nacl_npp);

  // Sets up recognition of the services exported from the browser.
  static NaClSrpcError SetUpcallServices(NaClSrpcChannel *channel,
                                         NaClSrpcArg **in_args,
                                         NaClSrpcArg **out_args);

  // Processes NP_Initialize() request from the plugin.
  static NaClSrpcError Initialize(NaClSrpcChannel* channel,
                                  NaClSrpcArg** inputs,
                                  NaClSrpcArg** outputs);
  // Processes NPP_New() request from the plugin.
  static NaClSrpcError New(NaClSrpcChannel* channel,
                           NaClSrpcArg** inputs,
                           NaClSrpcArg** outputs);
  // Processes NPP_Destroy() request from the plugin.
  static NaClSrpcError Destroy(NaClSrpcChannel* channel,
                               NaClSrpcArg** inputs,
                               NaClSrpcArg** outputs);
  // Processes NPP_GetScriptableInstance() request from the plugin.
  static NaClSrpcError GetScriptableInstance(NaClSrpcChannel* channel,
                                             NaClSrpcArg** inputs,
                                             NaClSrpcArg** outputs);
  // Processes NPP_URLNotify() request from the plugin.
  static NaClSrpcError URLNotify(NaClSrpcChannel* channel,
                                 NaClSrpcArg** inputs,
                                 NaClSrpcArg** outputs);

  // Processes responses to NPN_PluginThreadAsyncCall by the browser.
  static NaClSrpcError DoAsyncCall(NaClSrpcChannel* channel,
                                   NaClSrpcArg** inputs,
                                   NaClSrpcArg** outputs);

  // Implements NPP_New() request from the plugin.
  NPError NewImpl(NPMIMEType mimetype,
                  NPP npp,
                  uint32_t argc,
                  char* argn[],
                  char* argv[],
                  NaClSrpcArg* result_object);
  // Implements NPP_Destroy() request from the plugin.
  NPError DestroyImpl(NPP npp);
  // Implements NPP_URLNotify() request from the plugin.
  void URLNotifyImpl(NPP npp,
                     NaClSrpcImcDescType received_handle,
                     uint32_t reason);

  // Implements NPP_GetScriptableInstance() request from the plugin.
  NPCapability* GetScriptableInstanceImpl(NPP npp);

  // Sends NPN_Status() request to the plugin.
  void SetStatus(NPP npp, const char* message);
  // Sends NPN_GetValue() request to the plugin.
  NPError GetValue(NPP npp, NPNVariable var, void* value);
  // Sends NaClNPN_CreateArray() request to the plugin.
  NPObject* CreateArray(NPP npp);
  // Sends NaClNPN_OpenURL() request to the plugin.
  NPError OpenURL(NPP npp,
                  const char* url,
                  void* notify_data,
                  void (*notify)(const char* url,
                                 void* notify_data,
                                 NaClSrpcImcDescType handle));
  // Sends NPN_InvalidateRect(NPP npp) request to the plugin.
  void InvalidateRect(NPP npp, NPRect* invalid_rect);
  // Sends NPN_ForceRedraw() request to the plugin.
  void ForceRedraw(NPP npp);

  // Signals the browser to invoke a function on the navigator thread.
  void PluginThreadAsyncCall(NPP instance,
                             AsyncCallFunc func,
                             void* user_data);

  static void AddIntIdentifierMapping(int32_t intid, NPIdentifier identifier);
  static void AddStringIdentifierMapping(const NPUTF8* name,
                                         NPIdentifier identifier);

  // Gets the NPIdentifier that matches the specified string name.
  static NPIdentifier GetStringIdentifier(const NPUTF8* name);
  // Gets the NPUTF8 representation of a string identifier
  static NPUTF8* UTF8FromIdentifier(NPIdentifier identifier);
  // Returns true if name is a string NPIdentifier.
  static bool IdentifierIsString(NPIdentifier name);

  // Gets the NPIdentifier that matches the specified integer value.
  static NPIdentifier GetIntIdentifier(int32_t value);
  // Gets the NPUTF8 representation of a string identifier
  static int32_t IntFromIdentifier(NPIdentifier identifier);
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPNAVIGATOR_H_
