// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "native_client/src/third_party/npapi/files/include/npupp.h"
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

  static NPPluginFuncs plugin_funcs;

 public:
  NPNavigator(NaClSrpcChannel* channel,
              int32_t peer_pid,
              int32_t peer_npvariant_size);
  ~NPNavigator();

  // Get the Navigator for the current module.
  static inline NPNavigator* GetNavigator() {
    return navigator;
  }

  // Get the NaCl module NPP from the browser NPP.  Since this is always used
  // with an NPP passed from the plugin (NPModule), we take it in the form
  // used by SRPC.
  static NPP GetNaClNPP(int32_t plugin_npp_int, bool add_to_map);
  // Get the plugin NPP from the NaCl module NPP.  Since this is always used
  // to pass the NPP back to the plugin (NPModule), we return it in the form
  // for use by SRPC.
  static int32_t GetPluginNPP(NPP nacl_npp);

  // Sets up recognition of the services exported from the browser.
  static NaClSrpcError SetUpcallServices(NaClSrpcChannel *channel,
                                         NaClSrpcArg **in_args,
                                         NaClSrpcArg **out_args);

  // Processes NP_Initialize() request from the plugin.
  NaClSrpcError Initialize(NaClSrpcChannel* channel,
                           NaClSrpcImcDescType upcall_desc);
  // Processes NPP_New() request from the plugin.
  NaClSrpcError New(char* mimetype,
                    NPP npp,
                    uint32_t argc,
                    nacl_abi_size_t argn_bytes,
                    char* argn_in,
                    nacl_abi_size_t argv_bytes,
                    char* argv_in,
                    int32_t* nperr);
  // Processes NPP_HandleEvent request from the plugin.
  NaClSrpcError HandleEvent(NPP npp,
                            nacl_abi_size_t npevent_bytes,
                            char* npevent,
                            int32_t* return_int16);
  // Processes responses to NPN_PluginThreadAsyncCall from the plugin.
  NaClSrpcError DoAsyncCall(int32_t number);
  // Processes NPP_SetWindow() request from the plugin.
  NPError SetWindow(NPP npp, int height, int width);
  // Processes NPP_Destroy() request from the plugin.
  NPError Destroy(NPP npp);
  // Processes NPP_URLNotify() request from the plugin.
  void URLNotify(NPP npp, NaClSrpcImcDescType received_handle, uint32_t reason);

  // Processes NPP_GetScriptableInstance() request from the plugin.
  NPCapability* GetScriptableInstance(NPP npp);

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
