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


// NaCl module to NPAPI interface

#include "native_client/src/shared/npruntime/npnavigator.h"

#include <stdlib.h>
#include <inttypes.h>
#include <nacl/nacl_inttypes.h>
#include <utility>

#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npcapability.h"
#include "native_client/src/shared/npruntime/nprpc.h"

namespace nacl {

// Class static member declarations.
NPNavigator* NPNavigator::navigator = NULL;
std::map<NPP, NPP>* NPNavigator::nacl_npp_map = NULL;
std::map<NPP, NPP>* NPNavigator::plugin_npp_map = NULL;
std::map<int32_t, NPIdentifier>* NPNavigator::int_id_map = NULL;
std::map<NPIdentifier, int32_t>* NPNavigator::id_int_map = NULL;
std::map<const NPUTF8*, NPIdentifier>* NPNavigator::string_id_map = NULL;
std::map<NPIdentifier, const NPUTF8*>* NPNavigator::id_string_map = NULL;
std::set<const NPUTF8*, NPNavigator::StringCompare>*
    NPNavigator::string_set = NULL;

static void DebugPrintf(const char *fmt, ...) {
  va_list argptr;
  fprintf(stderr, "@@@ NAVIGATOR ");

  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);
  fflush(stderr);
}

NPNavigator::NPNavigator(uint32_t peer_pid,
                         size_t peer_npvariant_size)
    : NPBridge(),
      notify_(0),
      notify_data_(NULL),
      url_(NULL) {
  set_peer_pid(peer_pid);
  set_peer_npvariant_size(peer_npvariant_size);
  navigator = this;
  // Set up the browser NPP to/from NaCl module NPP mapping.
  nacl_npp_map = new(std::nothrow) std::map<NPP, NPP>;
  plugin_npp_map = new(std::nothrow) std::map<NPP, NPP>;
  // Set up the canonical identifier string table.
  string_set = new(std::nothrow) std::set<const NPUTF8*, StringCompare>;
  // Set up the identifier mappings.
  string_id_map = new(std::nothrow) std::map<const NPUTF8*, NPIdentifier>;
  id_string_map = new(std::nothrow) std::map<NPIdentifier, const NPUTF8*>;
  int_id_map = new(std::nothrow) std::map<int32_t, NPIdentifier>;
  id_int_map = new(std::nothrow) std::map<NPIdentifier, int32_t>;
}

NPNavigator::~NPNavigator() {
  // Note that all the mappings are per-module and only cleaned up on
  // shutdown.
}

NPP NPNavigator::GetNaClNPP(NPP plugin_npp, bool add_to_map) {
  if (NULL == nacl_npp_map) {
    DebugPrintf("No NPP map allocated.\n");
    return NULL;
  }
  std::map<NPP, NPP>::iterator i = nacl_npp_map->find(plugin_npp);
  if (nacl_npp_map->end() == i) {
    if (add_to_map) {
      // Allocate a new NPP for the NaCl module.
      NPP nacl_npp = static_cast<NPP>(malloc(sizeof(*nacl_npp)));
      if (NULL == nacl_npp) {
        DebugPrintf("No memory to allocate entry in NPP map.\n");
        return NULL;
      }
      nacl_npp->ndata = static_cast<void*>(GetNavigator());
      // And set up the mappings between them.
      (*nacl_npp_map)[plugin_npp] = nacl_npp;
      (*plugin_npp_map)[nacl_npp] = plugin_npp;
    } else {
      DebugPrintf("No entry in NPP map.\n");
      return NULL;
    }
  }
  return (*nacl_npp_map)[plugin_npp];
}

NPP NPNavigator::GetPluginNPP(NPP nacl_npp) {
  if (NULL == plugin_npp_map) {
    DebugPrintf("No NPP map allocated.\n");
    return NULL;
  }
  std::map<NPP, NPP>::iterator i = nacl_npp_map->find(nacl_npp);
  if (plugin_npp_map->end() == i) {
    DebugPrintf("No entry in NPP map.\n");
    return NULL;
  }
  return (*plugin_npp_map)[nacl_npp];
}

NACL_SRPC_METHOD_ARRAY(NPNavigator::srpc_methods) = {
    { "NP_Initialize:ii:", Initialize },
    { "NPP_New:siiCC:i", New },
    { "NPP_Destroy:i:i", Destroy },
    { "NPP_GetScriptableInstance:i:C", GetScriptableInstance },
    { "NPP_URLNotify:ihi:", URLNotify } };

//
// The following methods are the SRPC dispatchers for the request handlers
//

// inputs:
// (int) peer_pid
// (int) peer_npvariant_size
// outputs:
// (none)
NaClSrpcError NPNavigator::Initialize(NaClSrpcChannel* channel,
                                      NaClSrpcArg** inputs,
                                      NaClSrpcArg** outputs) {
  uint32_t peer_pid = static_cast<uint32_t>(inputs[0]->u.ival);
  uint32_t npvariant_size = static_cast<size_t>(inputs[1]->u.ival);

  DebugPrintf("NPP_Initialize: peer_pid=%d, size=%d\n",
              peer_pid,
              npvariant_size);
  // There is only one NPNavigator per call to Initialize, and it is
  // remembered on the SRPC channel that called it.
  // Make sure it is only set once.
  if (NULL != channel->server_instance_data) {
    // Instance data already set.
    DebugPrintf("  Error: instance data already set\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  NPNavigator* navigator =
      new(std::nothrow) NPNavigator(peer_pid, npvariant_size);
  if (NULL == navigator) {
    // Out of memory.
    DebugPrintf("  Error: couldn't create navigator\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  channel->server_instance_data = static_cast<void*>(navigator);
  return NACL_SRPC_RESULT_OK;
}

static bool GetCharpArray(uint32_t count,
                          NaClSrpcArg* arg,
                          char* array[]) {
  uint32_t total_len = arg->u.caval.count;
  char* str = arg->u.caval.carr;
  char* p = str;
  DebugPrintf("  %"PRIu32" arguments\n", count);
  for (uint32_t i = 0; i < count; ++i) {
    DebugPrintf("  %"PRIu32": %s\n", i, p);
    array[i] = p;
    // Find the end of the current array element.
    while ('\0' != *p) {
      // We know that p >= str, so the cast preserves sign.
      if (total_len <= static_cast<uint32_t>(p - str)) {
        // Reached the end of the string before finding NUL.
        DebugPrintf("  reached end of string\n");
        return false;
      }
      ++p;
    }
    // And find the next starting point (if any).
    // We know that p >= str, so the cast preserves sign.
    if (total_len > static_cast<uint32_t>(p - str)) {
      ++p;
    }
  }
  return true;
}

// inputs:
// (char*) mimetype
// (int) npp
// (int) argc
// (char[]) argn
// (char[]) argv
// outputs:
// (int) NPERR
NaClSrpcError NPNavigator::New(NaClSrpcChannel* channel,
                               NaClSrpcArg** inputs,
                               NaClSrpcArg** outputs) {
  char* mimetype = inputs[0]->u.sval;
  NPP npp = GetNaClNPP(reinterpret_cast<NPP>(inputs[1]->u.ival), true);
  uint32_t argc = static_cast<uint32_t>(inputs[2]->u.ival);
  DebugPrintf("NPP_New: mime=%s, argc=%"PRIu32"\n", mimetype, argc);

  NPNavigator* nav = static_cast<NPNavigator*>(channel->server_instance_data);
  // Build the argv and argn strutures.
  char* argn[kMaxArgc];
  char* argv[kMaxArgc];
  if (kMaxArgc < argc ||
      !GetCharpArray(argc, inputs[3], argn) ||
      !GetCharpArray(argc, inputs[4], argv)) {
    DebugPrintf("Range exceeded or array copy failed\n");
    outputs[0]->u.ival = NPERR_GENERIC_ERROR;
    return NACL_SRPC_RESULT_OK;
  }
  for (uint32_t i = 0; i < argc; ++i) {
    DebugPrintf("  %"PRIu32": argn=%s argv=%s\n", i, argn[i], argv[i]);
  }
  // Invoke the implementation
  outputs[0]->u.ival = nav->NewImpl(mimetype,
                                    npp,
                                    argc,
                                    argn,
                                    argv,
                                    outputs[1]);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// outputs:
// (int) NPERR
NaClSrpcError NPNavigator::Destroy(NaClSrpcChannel* channel,
                                   NaClSrpcArg** inputs,
                                   NaClSrpcArg** outputs) {
  DebugPrintf("NPP_Destroy\n");
  NPP npp = GetNaClNPP(reinterpret_cast<NPP>(inputs[0]->u.ival), false);
  NPNavigator* nav =
      static_cast<NPNavigator*>(channel->server_instance_data);
  outputs[0]->u.ival = nav->DestroyImpl(npp);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// outputs:
// (char[])
NaClSrpcError NPNavigator::GetScriptableInstance(NaClSrpcChannel* channel,
                                                 NaClSrpcArg** inputs,
                                                 NaClSrpcArg** outputs) {
  DebugPrintf("NPP_GetScriptableInstance\n");
  NPP npp = GetNaClNPP(reinterpret_cast<NPP>(inputs[0]->u.ival), false);
  NPNavigator* nav =
      reinterpret_cast<NPNavigator*>(channel->server_instance_data);
  NPCapability* capability = nav->GetScriptableInstanceImpl(npp);
  RpcArg ret(npp, outputs[0]);
  ret.PutCapability(capability);
  return NACL_SRPC_RESULT_OK;
}

// inputs:
// (int) npp
// (handle) file
// (int) reason
// outputs:
// (none)
NaClSrpcError NPNavigator::URLNotify(NaClSrpcChannel* channel,
                                     NaClSrpcArg** inputs,
                                     NaClSrpcArg** outputs) {
  // TODO(sehr): what happens when we fail to get a URL?
  DebugPrintf("NPP_URLNotify\n");
  NPP npp = GetNaClNPP(reinterpret_cast<NPP>(inputs[0]->u.ival), false);
  NPNavigator* nav =
      reinterpret_cast<NPNavigator*>(channel->server_instance_data);
  nav->URLNotifyImpl(npp, inputs[1]->u.hval, inputs[2]->u.ival);
  return NACL_SRPC_RESULT_OK;
}


//
// The following methods handle requests from the browser to the NPAPI module
//

NPError NPNavigator::NewImpl(NPMIMEType mimetype,
                             NPP npp,
                             uint32_t argc,
                             char* argn[],
                             char* argv[],
                             NaClSrpcArg* result_object) {
  NPError retval = NPP_New(mimetype,
                           npp,
                           0,  // TODO(sehr): what to do with mode?
                           argc,
                           argn,
                           argv,
                           NULL);
  return retval;
}

NPError NPNavigator::DestroyImpl(NPP npp) {
  return NPP_Destroy(npp, NULL);
}

NPCapability* NPNavigator::GetScriptableInstanceImpl(NPP npp) {
  NPObject* object = NPP_GetScriptableInstance(npp);
  if (object) {
    NPCapability* capability = new(std::nothrow) NPCapability;
    if (NULL != capability) {
      CreateStub(npp, object, capability);
    }
  }
  return NULL;
}

void NPNavigator::URLNotifyImpl(NPP npp,
                                NaClSrpcImcDescType received_handle,
                                uint32_t reason) {
  if (NPRES_DONE != reason) {
    // Close(received_handle);
    received_handle = kNaClSrpcInvalidImcDesc;
    // Cleanup, ensuring that the notify_ callback won't get a closed
    // handle; This is also how the callback knows that there was a
    // problem.
  }
  // Only one notify_ is allowed from a particular NPP, making it impossible
  // to handle multiple simultaneous GetURLNotify or PostURLNotify calls.
  if (notify_) {
    notify_(url_, notify_data_, received_handle);
    notify_ = NULL;
    notify_data_ = NULL;
    free(url_);
    url_ = NULL;
  }
}

//
// The following methods are calls from the NPAPI module to the browser.
//

void NPNavigator::SetStatus(NPP npp, const char* message) {
  NaClSrpcInvokeByName(channel(),
                       "NPN_Status",
                       GetPluginNPP(npp),
                       const_cast<char*>(message));
}

NPError NPNavigator::GetValue(NPP npp, NPNVariable variable, void* value) {
  char buf[sizeof(NPCapability)];
  uint32_t bufsize = static_cast<uint32_t>(sizeof(buf));
  int error_code;

  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeByName(channel(),
                                                  "NPN_GetValue",
                                                  GetPluginNPP(npp),
                                                  variable,
                                                  &error_code,
                                                  bufsize,
                                                  buf)) {
    return NPERR_GENERIC_ERROR;
  }
  if (error_code == NPERR_NO_ERROR) {
    RpcArg ret(npp, buf, bufsize);
    *static_cast<NPObject**>(value) = ret.GetObject();
    return NULL;
  }
  return error_code;
}

void NPNavigator::InvalidateRect(NPP npp, NPRect* invalid_rect) {
  NaClSrpcInvokeByName(channel(),
                       "NPN_InvalidateRect",
                       GetPluginNPP(npp),
                       sizeof(*invalid_rect),
                       reinterpret_cast<char*>(invalid_rect));
}

void NPNavigator::ForceRedraw(NPP npp) {
  NaClSrpcInvokeByName(channel(), "ForceRedraw", npp);
}

NPObject* NPNavigator::CreateArray(NPP npp) {
  char buf[sizeof(NPCapability)];
  int success;
  uint32_t bufsize = static_cast<uint32_t>(sizeof(buf));
  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeByName(channel(),
                                                  "NPN_CreateArray",
                                                  GetPluginNPP(npp),
                                                  &success,
                                                  bufsize,
                                                  buf)) {
    return NULL;
  }
  if (0 == success) {
    return NULL;
  }
  RpcArg ret(npp, buf, bufsize);
  return ret.GetObject();
}

NPError NPNavigator::OpenURL(NPP npp,
                             const char* url,
                             void* notify_data,
                             void (*notify)(const char* url,
                                            void* notify_data,
                                            NaClSrpcImcDescType handle)) {
  if (NULL != notify_ || NULL == url || NULL == notify) {
    return NPERR_GENERIC_ERROR;
  }
  int error_code;
  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeByName(channel(),
                                                  "NPN_OpenURL",
                                                  GetPluginNPP(npp),
                                                  const_cast<char*>(url),
                                                  &error_code)) {
    return NPERR_GENERIC_ERROR;
  }
  NPError nperr = static_cast<NPError>(error_code);
  if (NPERR_NO_ERROR == nperr) {
    notify_ = notify;
    notify_data_ = notify_data;
    url_ = STRDUP(url);
  }
  return nperr;
}

// Returns a canonical version of a string that can be used to lookup
// string identifiers, etc.
const NPUTF8* NPNavigator::InternString(const NPUTF8* name) {
  // Lookup whether a matching string is already interned in the set.
  std::set<const NPUTF8*, StringCompare>::iterator i;
  i = string_set->find(name);
  if (string_set->end() != i) {
    return *i;
  }
  // No matching string was found.  Create a copy to store, as the client
  // may free the string we were passed.
  name = STRDUP(name);
  if (NULL == name) {
    return NULL;
  }
  // Insert the copy into the set.
  std::pair<std::set<const NPUTF8*, StringCompare>::iterator, bool> result;
  result = string_set->insert(name);
  // Return the canonical string's address.
  return const_cast<const NPUTF8*>(name);
}

NPIdentifier NPNavigator::GetStringIdentifier(const NPUTF8* name) {
  // Get a unique name usable for maps.
  const NPUTF8* canonical_name = InternString(name);
  // Lookup the string.
  std::map<const NPUTF8*, NPIdentifier>::iterator i;
  i = string_id_map->find(name);
  if (string_id_map->end() == i) {
    NPIdentifier identifier;
    NPNavigator* nav = GetNavigator();
    // Failed to find id in the map, so ask the browser.
    if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeByName(nav->channel(),
                                                    "NPN_GetStringIdentifier",
                                                    canonical_name,
                                                    &identifier)) {
      DebugPrintf("GetIntIdentifier failed\n");
      return reinterpret_cast<NPIdentifier>(0);
    }
    (*string_id_map)[canonical_name] = identifier;
    (*id_string_map)[identifier] = canonical_name;
  }
  return (*string_id_map)[canonical_name];
}

bool NPNavigator::IdentifierIsString(NPIdentifier identifier) {
  return id_string_map->end() != id_string_map->find(identifier);
}

NPUTF8* NPNavigator::UTF8FromIdentifier(NPIdentifier identifier) {
  // Check that the identifier is a string ident.
  if (!IdentifierIsString(identifier)) {
    // If it's not a string identifier, the result is undefined.
    return NULL;
  }
  const NPUTF8* str = (*id_string_map)[identifier];
  size_t length = strlen(static_cast<const char*>(str)) + 1;
  char* utf8 = static_cast<char*>(NPN_MemAlloc(length));
  if (NULL != utf8) {
    memmove(utf8, identifier, length);
  }
  return utf8;
}

NPIdentifier NPNavigator::GetIntIdentifier(int32_t value) {
  std::map<int32_t, NPIdentifier>::iterator i;
  i = int_id_map->find(value);
  if (int_id_map->end() == i) {
    NPIdentifier identifier;
    NPNavigator* nav = GetNavigator();
    // Failed to find id in the map, so ask the browser.
    if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeByName(nav->channel(),
                                                    "NPN_GetIntIdentifier",
                                                    value,
                                                    &identifier)) {
      DebugPrintf("GetIntIdentifier failed\n");
      return reinterpret_cast<NPIdentifier>(0);
    }
    (*int_id_map)[value] = identifier;
    (*id_int_map)[identifier] = value;
  }
  return (*int_id_map)[value];
}

int32_t NPNavigator::IntFromIdentifier(NPIdentifier identifier) {
  std::map<NPIdentifier, int32_t>::iterator i;
  i = id_int_map->find(identifier);
  if (id_int_map->end() == i) {
    // Failed to find id in the map, so the result is undefined.
    return -1;
  }
  return (*id_int_map)[identifier];
}

}  // namespace nacl
