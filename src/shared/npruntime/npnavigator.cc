// Copyright (c) 2008 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NaCl module to NPAPI interface

#include "native_client/src/shared/npruntime/npnavigator.h"

#include <stdlib.h>
#include <inttypes.h>
#include <nacl/nacl_inttypes.h>

#include <utility>

#include "native_client/src/include/portability_string.h"
#include "gen/native_client/src/shared/npruntime/npmodule_rpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"
#include "gen/native_client/src/shared/npruntime/npupcall_rpc.h"
#include "native_client/src/shared/npruntime/audio.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npcapability.h"
#include "native_client/src/shared/npruntime/npn_gate.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "native_client/src/shared/npruntime/pointer_translations.h"
#include "native_client/src/shared/npruntime/structure_translations.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

using nacl::WireFormatToNPObject;

namespace {

static bool GetCharpArray(uint32_t count,
                          char* str,
                          nacl_abi_size_t total_len,
                          char* array[]) {
  char* p = str;
  for (uint32_t i = 0; i < count; ++i) {
    array[i] = p;
    // Find the end of the current array element.
    while ('\0' != *p) {
      // We know that p >= str, so the cast preserves sign.
      if (total_len <= static_cast<uint32_t>(p - str)) {
        // Reached the end of the string before finding NUL.
        nacl::DebugPrintf("  reached end of string\n");
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

}  // namespace

namespace nacl {

// Class static member declarations.
NPNavigator* NPNavigator::navigator = NULL;
std::map<int32_t, NPIdentifier>* NPNavigator::int_id_map = NULL;
std::map<NPIdentifier, int32_t>* NPNavigator::id_int_map = NULL;
std::map<const NPUTF8*, NPIdentifier>* NPNavigator::string_id_map = NULL;
std::map<NPIdentifier, const NPUTF8*>* NPNavigator::id_string_map = NULL;
std::set<const NPUTF8*, NPNavigator::StringCompare>*
    NPNavigator::string_set = NULL;

NPPluginFuncs NPNavigator::plugin_funcs;

NPNavigator::NPNavigator(NaClSrpcChannel* channel,
                         int32_t peer_pid)
    : NPBridge(),
      upcall_channel_(NULL) {
  DebugPrintf("NPNavigator(%p, %u)\n",
              static_cast<void*>(this),
              static_cast<unsigned>(peer_pid));
  set_channel(channel);
  set_peer_pid(peer_pid);
  navigator = this;
  // Set up the canonical identifier string table.
  string_set = new(std::nothrow) std::set<const NPUTF8*, StringCompare>;
  // Set up the identifier mappings.
  string_id_map = new(std::nothrow) std::map<const NPUTF8*, NPIdentifier>;
  id_string_map = new(std::nothrow) std::map<NPIdentifier, const NPUTF8*>;
  int_id_map = new(std::nothrow) std::map<int32_t, NPIdentifier>;
  id_int_map = new(std::nothrow) std::map<NPIdentifier, int32_t>;
  // Allocate the closure table.
  closure_table_ = new(std::nothrow) NPClosureTable();
  // Initialize the upcall mutex.
  pthread_mutex_init(&upcall_mu_, NULL);
}

NPNavigator::~NPNavigator() {
  DebugPrintf("~NPNavigator\n");
  pthread_mutex_destroy(&upcall_mu_);
  delete closure_table_;
  // Note that all the mappings are per-module and only cleaned up on
  // shutdown.
}

NaClSrpcError NPNavigator::Initialize(NaClSrpcChannel* channel,
                                      NaClSrpcImcDescType upcall_desc) {
  // Remember the upcall service port on the navigator.
  NaClSrpcChannel* upcall_channel = new(std::nothrow) NaClSrpcChannel;
  if (NULL == upcall_channel) {
    DebugPrintf("  Error: couldn't create upcall_channel \n");
    delete navigator;
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  if (!NaClSrpcClientCtor(upcall_channel, upcall_desc)) {
    DebugPrintf("  Error: couldn't create SRPC upcall client\n");
    delete upcall_channel;
    delete navigator;
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  navigator->upcall_channel_ = upcall_channel;
  // Invoke the client NP_Initialize.
  memset(reinterpret_cast<void*>(&plugin_funcs), 0, sizeof(plugin_funcs));
  plugin_funcs.size = static_cast<uint16_t>(sizeof(plugin_funcs));
  plugin_funcs.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  NPNetscapeFuncs* browser_funcs =
      const_cast<NPNetscapeFuncs*>(GetBrowserFuncs());
  NPError err = ::NP_Initialize(browser_funcs, &plugin_funcs);
  if (NPERR_NO_ERROR != err) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigator::New(char* mimetype,
                               NPP npp,
                               uint32_t argc,
                               nacl_abi_size_t argn_bytes,
                               char* argn_in,
                               nacl_abi_size_t argv_bytes,
                               char* argv_in,
                               int32_t* nperr) {
  DebugPrintf("NPP_New: npp=%p, mime=%s, argc=%"NACL_PRIu32"\n",
              reinterpret_cast<void*>(npp), mimetype, argc);

  *nperr = NPERR_NO_ERROR;

  // Build the argv and argn strutures.
  char* argn[kMaxArgc];
  char* argv[kMaxArgc];
  if (kMaxArgc < argc ||
      !GetCharpArray(argc, argn_in, argn_bytes, argn) ||
      !GetCharpArray(argc, argv_in, argv_bytes, argv)) {
    DebugPrintf("Range exceeded or array copy failed\n");
    *nperr = NPERR_GENERIC_ERROR;
    return NACL_SRPC_RESULT_OK;
  }
  for (uint32_t i = 0; i < argc; ++i) {
    // The values passed in need to persist after this method has returned.
    argn[i] = strdup(argn[i]);
    argv[i] = strdup(argv[i]);
    // Print them out for debugging.
    DebugPrintf("  %"NACL_PRIu32": argn=%s argv=%s\n", i, argn[i], argv[i]);
  }
  // Invoke the implementation
  if (NULL == plugin_funcs.newp) {
    *nperr = NPERR_GENERIC_ERROR;
  } else {
    *nperr = plugin_funcs.newp(mimetype,
                               npp,
                               0,  // TODO(sehr): what to do with mode?
                               argc,
                               argn,
                               argv,
                               NULL);
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigator::HandleEvent(NPP npp,
                                       nacl_abi_size_t npevent_bytes,
                                       char* npevent,
                                       int32_t* return_int16) {
  // NPP_HandleEvent returns an int16, and SRPC only knows about int32_t.
  DebugPrintf("NPP_HandleEvent(npp %p, %p)\n",
              reinterpret_cast<void*>(npp),
              reinterpret_cast<void*>(npevent));

  NPPepperEvent* event = reinterpret_cast<NPPepperEvent*>(npevent);
  if (NULL == plugin_funcs.event) {
    // Event was not handled.
    *return_int16 = 0;
  } else {
    *return_int16 = plugin_funcs.event(npp, reinterpret_cast<void*>(event));
  }

  return NACL_SRPC_RESULT_OK;
}

//
// The following methods handle requests from the browser to the NPAPI module
//

NPError NPNavigator::SetWindow(NPP npp,
                               int height,
                               int width) {
  DebugPrintf("NPP_SetWindow: npp=%p, height=%d, width=%d\n",
              reinterpret_cast<void*>(npp), height, width);

  if (NULL == plugin_funcs.setwindow) {
    return NPERR_GENERIC_ERROR;
  }
  // Populate an NPWindow object to pass to the instance.
  static NPWindow window;
  window.height = height;
  window.width = width;
  // Invoke the plugin's setwindow method.
  return plugin_funcs.setwindow(npp, &window);
}

NPError NPNavigator::Destroy(NPP npp) {
  DebugPrintf("NPP_Destroy\n");
  if (NULL == plugin_funcs.destroy) {
    return NPERR_GENERIC_ERROR;
  }
  return plugin_funcs.destroy(npp, NULL);
}

NPCapability* NPNavigator::GetScriptableInstance(NPP npp) {
  NPObject* object;
  NPError err = plugin_funcs.getvalue(npp,
                                      NPPVpluginScriptableNPObject,
                                      &object);
  DebugPrintf("GetScriptableInstance %d %p\n",
              static_cast<int>(err),
              reinterpret_cast<void*>(object));
  if (NPERR_NO_ERROR == err && NULL != object) {
    NPCapability* capability = new(std::nothrow) NPCapability;
    if (NULL != capability) {
      NPObjectStub::CreateStub(npp, object, capability);
    }
    return capability;
  }
  return NULL;
}

NPError NPNavigator::GetUrl(NPP npp, const char* url, const char* target) {
  if (NULL == target) {
    // We currently require the target to be a non-NULL value.  If the
    // target is a zero-length string, then it means call back on
    // NPP_StreamAsFile when the URL has been loaded.
    return NPERR_GENERIC_ERROR;
  }
  int32_t nperr;
  NaClSrpcError retval =
      NPModuleRpcClient::NPN_GetURL(channel(),
                                    NPPToWireFormat(npp),
                                    const_cast<char*>(url),
                                    const_cast<char*>(target),
                                    &nperr);
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return static_cast<NPError>(nperr);
}

NPError NPNavigator::GetUrlNotify(NPP npp,
                                  const char* url,
                                  const char* target,
                                  void* notify_data) {
  if (NULL == target) {
    // We currently require the target to be a non-NULL value.  If the
    // target is a zero-length string, then it means call back on
    // NPP_StreamAsFile when the URL has been loaded.
    // TODO(sehr): Flesh this out to add NPN_WriteReady, etc.
    return NPERR_GENERIC_ERROR;
  }
  int32_t nperr;
  NaClSrpcError retval =
      NPModuleRpcClient::NPN_GetURL(channel(),
                                    NPPToWireFormat(npp),
                                    const_cast<char*>(url),
                                    const_cast<char*>(target),
                                    &nperr);
  // TODO(sehr): add the GetURLNotify.
  if (NACL_SRPC_RESULT_OK != retval) {
    return NPERR_GENERIC_ERROR;
  }
  return NPERR_NO_ERROR;
}

void NPNavigator::StreamAsFile(NPP npp,
                               NaClSrpcImcDescType file,
                               char* url,
                               uint32_t size) {
  if (NULL != plugin_funcs.asfile) {
    NPStream stream;
    stream.pdata = NULL;
    stream.ndata = NULL;
    stream.url = url;
    stream.end = size;
    stream.lastmodified = 0;
    stream.notifyData = NULL;
    stream.headers = NULL;
    // We are abusing the API here.  There is no local file whose name we
    // refer to.  We instead return the file descriptor of the shared memory.
    // TODO(sehr): Find a better permanent method for passing this information.
    const int file_descriptor = file;
    plugin_funcs.asfile(npp,
                        &stream,
                        reinterpret_cast<const char*>(&file_descriptor));
  }
}

void NPNavigator::URLNotify(NPP npp,
                            NaClSrpcImcDescType received_handle,
                            uint32_t reason) {
  if (NPRES_DONE != reason) {
    received_handle = kNaClSrpcInvalidImcDesc;
  }
  if (NULL != plugin_funcs.urlnotify) {
    plugin_funcs.urlnotify(npp, "Unimplemented", reason, NULL);
  }
}

//
// The following methods are calls from the NPAPI module to the browser.
//

void NPNavigator::SetStatus(NPP npp, const char* message) {
  NPModuleRpcClient::NPN_SetStatus(channel(),
                                   NPPToWireFormat(npp),
                                   const_cast<char*>(message));
}

NPError NPNavigator::GetValue(NPP npp, NPNVariable variable, NPBool* value) {
  int32_t error_code;
  int32_t boolean_as_int32;

  if (NPNVisOfflineBool != variable && NPNVprivateModeBool != variable) {
    return NPERR_INVALID_PARAM;
  }
  if (NACL_SRPC_RESULT_OK !=
      NPModuleRpcClient::NPN_GetValueBoolean(
          channel(),
          NPPToWireFormat(npp),
          static_cast<int32_t>(variable),
          &error_code,
          &boolean_as_int32)) {
    return NPERR_GENERIC_ERROR;
  }
  if (NPERR_NO_ERROR != error_code) {
    return error_code;
  }
  *value = static_cast<NPBool>(boolean_as_int32);
  return NPERR_NO_ERROR;
}

NPError NPNavigator::GetValue(NPP npp, NPNVariable variable, NPObject** value) {
  char result_bytes[sizeof(NPCapability)];
  nacl_abi_size_t result_length =
      static_cast<nacl_abi_size_t>(sizeof(result_bytes));
  int32_t error_code;

  if (NPNVWindowNPObject != variable && NPNVPluginElementNPObject != variable) {
    return NPERR_INVALID_PARAM;
  }
  if (NACL_SRPC_RESULT_OK !=
      NPModuleRpcClient::NPN_GetValueObject(
          channel(),
          NPPToWireFormat(npp),
          static_cast<int32_t>(variable),
          &error_code,
          &result_length,
          result_bytes)) {
    return NPERR_GENERIC_ERROR;
  }
  if (NPERR_NO_ERROR != error_code) {
    return error_code;
  }
  *reinterpret_cast<NPObject**>(value) =
      WireFormatToNPObject(npp, result_bytes, result_length);
  return NPERR_NO_ERROR;
}

bool NPNavigator::Evaluate(NPP npp,
                           NPObject* object,
                           NPString* script,
                           NPVariant* result) {
  // TODO(sehr): Implement an RPC and a test for NPN_Evaluate.
  return false;
}

NPObject* NPNavigator::CreateArray(NPP npp) {
  char ret_bytes[sizeof(NPCapability)];
  nacl_abi_size_t ret_length = static_cast<nacl_abi_size_t>(sizeof(ret_bytes));
  int32_t success;

  if (NACL_SRPC_RESULT_OK !=
      NPModuleRpcClient::NPN_CreateArray(
          channel(),
          NPPToWireFormat(npp),
          &success,
          &ret_length,
          ret_bytes)) {
    return NULL;
  }
  if (!success) {
    return NULL;
  }
  return WireFormatToNPObject(npp, ret_bytes, ret_length);
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

void NPNavigator::AddIntIdentifierMapping(int32_t intid,
                                          NPIdentifier identifier) {
  (*int_id_map)[intid] = identifier;
  (*id_int_map)[identifier] = intid;
}

void NPNavigator::AddStringIdentifierMapping(const NPUTF8* name,
                                             NPIdentifier identifier) {
  const NPUTF8* str = InternString(name);
  (*string_id_map)[str] = identifier;
  (*id_string_map)[identifier] = str;
}

NPIdentifier NPNavigator::GetStringIdentifier(const NPUTF8* name) {
  // Get a unique name usable for maps.
  const NPUTF8* canonical_name = InternString(name);
  // Lookup the string.
  std::map<const NPUTF8*, NPIdentifier>::iterator i;
  i = string_id_map->find(name);
  if (string_id_map->end() == i) {
    int32_t int_id;
    NPNavigator* nav = GetNavigator();
    // Failed to find id in the map, so ask the browser.
    if (NACL_SRPC_RESULT_OK !=
        NPModuleRpcClient::NPN_GetStringIdentifier(
            nav->channel(),
            reinterpret_cast<char*>(const_cast<NPUTF8*>(canonical_name)),
            &int_id)) {
      return NULL;
    }
    NPIdentifier identifier = reinterpret_cast<NPIdentifier>(int_id);
    AddStringIdentifierMapping(canonical_name, identifier);
  }
  return (*string_id_map)[canonical_name];
}

bool NPNavigator::IdentifierIsString(NPIdentifier identifier) {
  if (id_string_map->end() == id_string_map->find(identifier)) {
    // Check to see if the identifier was interned in the browser.
    NPNavigator* nav = GetNavigator();
    int32_t isstring;
    if (NACL_SRPC_RESULT_OK !=
        NPModuleRpcClient::NPN_IdentifierIsString(
            nav->channel(),
            reinterpret_cast<int32_t>(identifier),
            &isstring) ||
        !isstring) {
      // Browser says it's not a string
      return false;
    }
  }
  return true;
}

NPUTF8* NPNavigator::UTF8FromIdentifier(NPIdentifier identifier) {
  // Check that the identifier is in the mapping.
  if (id_string_map->end() == id_string_map->find(identifier)) {
    // Not in the local mapping.  Check that the identifier is a string ident.
    if (IdentifierIsString(identifier)) {
      // If it's not maintained locally, ask the browser.
      NPNavigator* nav = GetNavigator();
      int32_t errcode;
      char* chr_str;
      if (NACL_SRPC_RESULT_OK !=
          NPModuleRpcClient::NPN_UTF8FromIdentifier(
              nav->channel(),
              reinterpret_cast<int32_t>(identifier),
              &errcode,
              &chr_str) ||
          NPERR_NO_ERROR != errcode) {
        // Is a string, but not found in the browser either.
        return NULL;
      }
      // Enter it into our cache if it was found.
      const NPUTF8* str = reinterpret_cast<NPUTF8*>(chr_str);
      AddStringIdentifierMapping(str, identifier);
    } else {
      // Browser says it is not a string.  Return an error.
      return NULL;
    }
  }
  const NPUTF8* str = (*id_string_map)[identifier];
  size_t length = strlen(static_cast<const char*>(str)) + 1;
  char* utf8 = static_cast<char*>(NPN_MemAlloc(length));
  if (NULL != utf8) {
    memmove(utf8, str, length);
  }
  return utf8;
}

NPIdentifier NPNavigator::GetIntIdentifier(int32_t value) {
  std::map<int32_t, NPIdentifier>::iterator i;
  i = int_id_map->find(value);
  if (int_id_map->end() == i) {
    NPNavigator* nav = GetNavigator();
    int32_t int_id;
    // Failed to find id in the map, so ask the browser.
    NaClSrpcError ret =
        NPModuleRpcClient::NPN_GetIntIdentifier(nav->channel(), value, &int_id);
    if (NACL_SRPC_RESULT_OK != ret) {
      return NULL;
    }
    NPIdentifier identifier = reinterpret_cast<NPIdentifier>(int_id);
    AddIntIdentifierMapping(value, identifier);
  }
  return (*int_id_map)[value];
}

int32_t NPNavigator::IntFromIdentifier(NPIdentifier identifier) {
  std::map<NPIdentifier, int32_t>::iterator i;
  i = id_int_map->find(identifier);
  if (id_int_map->end() == i) {
    // If it's not maintained locally, ask the browser.
    NPNavigator* nav = GetNavigator();
    int32_t int_id;
    if (NACL_SRPC_RESULT_OK !=
        NPModuleRpcClient::NPN_IntFromIdentifier(
            nav->channel(),
            reinterpret_cast<int32_t>(identifier),
            &int_id)) {
      // Not found in the browser either.
      return -1;
    }
    AddIntIdentifierMapping(int_id, identifier);
  }
  return (*id_int_map)[identifier];
}

void NPNavigator::PluginThreadAsyncCall(NPP instance,
                                        NPClosureTable::FunctionPointer func,
                                        void* user_data) {
  DebugPrintf("PluginThreadAsyncCall(%p, %p)\n",
              reinterpret_cast<void*>(instance),
              user_data);

  uint32_t id;
  // Add a closure to the table.
  if (NULL == closure_table_ ||
      NULL == func ||
      !closure_table_->Add(func, user_data, &id)) {
    return;
  }
  // RPCs through the NPUpcallRpcClient are guarded by a mutex.
  pthread_mutex_lock(&upcall_mu_);
  // Send an RPC to the NPAPI upcall thread in the browser plugin.
  NPUpcallRpcClient::NPN_PluginThreadAsyncCall(upcall_channel_,
                                               NPPToWireFormat(instance),
                                               static_cast<int32_t>(id));
  // Release the mutex.
  pthread_mutex_unlock(&upcall_mu_);
}

NaClSrpcError NPNavigator::Device2DFlush(int32_t wire_npp,
                                         int32_t* stride,
                                         int32_t* left,
                                         int32_t* top,
                                         int32_t* right,
                                         int32_t* bottom) {
  // RPCs through the NPUpcallRpcClient are guarded by a mutex.
  pthread_mutex_lock(&upcall_mu_);
  NaClSrpcError retval =
      NPUpcallRpcClient::Device2DFlush(upcall_channel_,
                                       wire_npp,
                                       stride,
                                       left,
                                       top,
                                       right,
                                       bottom);
  // Release the mutex.
  pthread_mutex_unlock(&upcall_mu_);
  // Return the RPC result.
  return retval;
}

NaClSrpcError NPNavigator::Device3DFlush(int32_t wire_npp,
                                         int32_t putOffset,
                                         int32_t* getOffset,
                                         int32_t* token,
                                         int32_t* error) {
  // RPCs through the NPUpcallRpcClient are guarded by a mutex.
  pthread_mutex_lock(&upcall_mu_);
  NaClSrpcError retval =
      NPUpcallRpcClient::Device3DFlush(upcall_channel_,
                                       wire_npp,
                                       putOffset,
                                       getOffset,
                                       token,
                                       error);
  // Release the mutex.
  pthread_mutex_unlock(&upcall_mu_);
  // Return the RPC result.
  return retval;
}

// We placed a closure on the browser's NPAPI thread when the NaCl module
// invoked NPN_PluginThreadAsyncCall.  When the browser runs that thunk, the
// browser will make an RPC to the NaCl module on the NPAPI channel.  That
// RPC invokes this method, which performs the NaCl-side portion of the
// NPN_PluginThreadAsyncCall.
NaClSrpcError NPNavigator::DoAsyncCall(int32_t number) {
  uint32_t id = static_cast<uint32_t>(number);
  NPClosureTable::FunctionPointer func;
  void* user_data;
  if (NULL == closure_table_ ||
      !closure_table_->Remove(id, &func, &user_data) ||
      NULL == func) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Invoke the function with the data.
  (*func)(user_data);
  // Return success.
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError NPNavigator::AudioCallback(int32_t number,
                                         int shm_desc,
                                         int32_t shm_size,
                                         int sync_desc) {
  if (nacl::DoAudioCallback(closure_table_,
                            number,
                            shm_desc,
                            shm_size,
                            sync_desc)) {
    return NACL_SRPC_RESULT_OK;
  } else {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
}

}  // namespace nacl
