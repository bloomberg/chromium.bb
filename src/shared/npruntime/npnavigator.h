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
#include "native_client/src/shared/npruntime/npclosure.h"
#include "native_client/src/third_party/npapi/files/include/npupp.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace nacl {

// Represents the NaCl module end of the connection to the browser plugin. The
// opposite end is the NPModule.
class NPNavigator : public NPBridge {
 public:
  // Canonical representation of strings used in identifier lookups.
  static const NPUTF8* InternString(const NPUTF8* name);
  // Compares two strings in string_set.
  struct StringCompare {
    bool operator()(const NPUTF8* left, const NPUTF8* right) const {
      return (strcmp(left, right) < 0) ? true : false;
    }
  };

  NPNavigator(NaClSrpcChannel* channel,
              int32_t peer_pid);
  ~NPNavigator();

  // Get the Navigator for the current module.
  static inline NPNavigator* GetNavigator() {
    return navigator;
  }

  NPClosureTable* closure_table() { return closure_table_; }

  // Processes NP_Initialize request from the plugin.
  NaClSrpcError Initialize(NaClSrpcChannel* channel,
                           NaClSrpcImcDescType upcall_desc);
  // Processes NPP_New request from the plugin.
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
  NaClSrpcError AudioCallback(int32_t number,
                              int shm_desc,
                              int32_t shm_size,
                              int sync_desc);
  // Processes NPP_SetWindow request from the plugin.
  NPError SetWindow(NPP npp, int height, int width);
  // Processes NPP_Destroy request from the plugin.
  NPError Destroy(NPP npp);
  // Processes NPP_StreamAsFile request from the plugin.
  void StreamAsFile(NPP npp,
                    NaClSrpcImcDescType file,
                    char* url,
                    uint32_t size);
  // Processes NPP_URLNotify request from the plugin.
  void URLNotify(NPP npp, NaClSrpcImcDescType received_handle, uint32_t reason);

  // Processes GetScriptableInstance request from the plugin.
  NPCapability* GetScriptableInstance(NPP npp);

  // Sends NPN_Status request to the plugin.
  void SetStatus(NPP npp, const char* message);
  // Sends NPN_GetValue request for a boolean to the plugin.
  NPError GetValue(NPP npp, NPNVariable var, NPBool* value);
  // Sends NPN_GetValue request for an NPObject to the plugin.
  NPError GetValue(NPP npp, NPNVariable var, NPObject** value);
  // Sends NaClNPN_CreateArray request to the plugin.
  NPObject* CreateArray(NPP npp);
  // Sends NPN_GetURL request to the plugin.
  NPError GetUrl(NPP npp, const char* url, const char* target);
  // Sends NPN_GetURLNotify request to the plugin.
  NPError GetUrlNotify(NPP npp,
                       const char* url,
                       const char* target,
                       void* notifyData);

  // The following three methods send RPCs to the browser on the upcall thread,
  // which allows them to be invoked from any thread in the NaCl module.  These
  // APIs synchronize the calls on the upcall channel.
  void PluginThreadAsyncCall(NPP instance,
                             NPClosureTable::FunctionPointer func,
                             void* user_data);

  NaClSrpcError Device2DFlush(int32_t wire_npp,
                              int32_t* stride,
                              int32_t* left,
                              int32_t* top,
                              int32_t* right,
                              int32_t* bottom);

  NaClSrpcError Device3DFlush(int32_t wire_npp,
                              int32_t putOffset,
                              int32_t* getOffset,
                              int32_t* token,
                              int32_t* error);

  // Perform NPN_Evaluate.
  bool Evaluate(NPP npp, NPObject* object, NPString* script, NPVariant* result);

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

  // The one navigator in this NaCl module.
  static NPNavigator* navigator;

  // NPIdentifiers:
  // The browser controls the master identifier representation.
  // The navigator retains two local idetifier caches to reduce RPC traffic.
  static std::map<const NPUTF8*, NPIdentifier>* string_id_map;
  static std::map<int32_t, NPIdentifier>* int_id_map;
  // It also retains a mapping from NPIdentifier to NPUTF8* for name retrieval.
  static std::map<NPIdentifier, const NPUTF8*>* id_string_map;
  // And retains a mapping from NPIdentifier to int32_t for value retrieval.
  static std::map<NPIdentifier, int32_t>* id_int_map;
  // The set of character strings for NPIdentifier names.
  static std::set<const NPUTF8*, StringCompare>* string_set;

 private:
  // The SRPC service used to send upcalls to the browser plugin.
  // This is used, among other things, to implement NPN_PluginThreadAsyncCall.
  NaClSrpcChannel* upcall_channel_;
  // The mutex guard for making upcalls.
  pthread_mutex_t upcall_mu_;
  // The table of closures created for calls back from the browser.
  NPClosureTable* closure_table_;

  static NPPluginFuncs plugin_funcs;
};

}  // namespace nacl

#endif  // NATIVE_CLIENT_SRC_SHARED_NPRUNTIME_NPNAVIGATOR_H_
