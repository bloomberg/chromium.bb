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
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/shared/npruntime/npcapability.h"
#include "native_client/src/shared/npruntime/npn_gate.h"
#include "native_client/src/shared/npruntime/nprpc.h"
#include "native_client/src/shared/npruntime/npobject_stub.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

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

// We need a better way to make sure that SRPC method tables are exported from
// servers.  Unfortunately, incorporating the generator exposes the possibility
// that the tables are not referenced from outside of the SRPC library, and
// hence they do not get included at all.  This hack makes sure they are
// referenced.
// TODO(sehr): make SRPC method table exporting explicit.
void* hack =
    reinterpret_cast<void*>(NPNavigatorRpcs::srpc_methods);

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
uint32_t NPNavigator::next_pending_call = 0;

NPPluginFuncs NPNavigator::plugin_funcs;

NPNavigator::NPNavigator(NaClSrpcChannel* channel,
                         int32_t peer_pid,
                         int32_t peer_npvariant_size)
    : NPBridge(),
      notify_(0),
      notify_data_(NULL),
      url_(NULL),
      upcall_channel_(NULL) {
  DebugPrintf("NPNavigator(%u, %u)\n",
              static_cast<unsigned>(peer_pid),
              static_cast<unsigned>(peer_npvariant_size));
  set_channel(channel);
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
  // Initialize the pending call mutex.
  pthread_mutex_init(&pending_mu_, NULL);
}

NPNavigator::~NPNavigator() {
  DebugPrintf("~NPNavigator\n");
  pthread_mutex_destroy(&pending_mu_);
  // Note that all the mappings are per-module and only cleaned up on
  // shutdown.
}

NPP NPNavigator::GetNaClNPP(int32_t plugin_npp_int, bool add_to_map) {
  if (NULL == nacl_npp_map) {
    DebugPrintf("No NPP map allocated.\n");
    return NULL;
  }
  NPP plugin_npp = NPBridge::IntToNpp(plugin_npp_int);
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

int32_t NPNavigator::GetPluginNPP(NPP nacl_npp) {
  if (NULL == plugin_npp_map) {
    DebugPrintf("No NPP map allocated.\n");
    return 0;
  }
  std::map<NPP, NPP>::iterator i = nacl_npp_map->find(nacl_npp);
  if (plugin_npp_map->end() == i) {
    DebugPrintf("No entry in NPP map.\n");
    return 0;
  }
  return reinterpret_cast<int32_t>((*plugin_npp_map)[nacl_npp]);
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
  DebugPrintf("NPP_New: npp=%p, mime=%s, argc=%"PRIu32"\n",
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
    DebugPrintf("  %"PRIu32": argn=%s argv=%s\n", i, argn[i], argv[i]);
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
  DebugPrintf("NPP_HandleEvent(npp %p, %p)\n", npp, npevent);

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
  DebugPrintf("NPP_SetWindow: npp=%d, height=%d, width=%d\n",
              npp, height, width);

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
  DebugPrintf("SII:\n");
  NPObject* object = NPP_GetScriptableInstance(npp);
  DebugPrintf("SII: %p\n", reinterpret_cast<void*>(object));
  if (NULL != object) {
    NPCapability* capability = new(std::nothrow) NPCapability;
    if (NULL != capability) {
      NPObjectStub::CreateStub(npp, object, capability);
    }
    return capability;
  }
  DebugPrintf("SII: was null\n");
  return NULL;
}

void NPNavigator::URLNotify(NPP npp,
                            NaClSrpcImcDescType received_handle,
                            uint32_t reason) {
  if (NPRES_DONE != reason) {
    // Close(received_handle);
    received_handle = kNaClSrpcInvalidImcDesc;
    // Cleanup, ensuring that the notify_ callback won't get a closed
    // handle; This is also how the callback knows that there was a
    // problem.
  }
  if (NULL != plugin_funcs.urlnotify) {
    plugin_funcs.urlnotify(npp, url_, reason, notify_data_);
    // TODO(sehr): remove these vestigial bits.
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
  NPModuleRpcClient::NPN_SetStatus(channel(),
                                   GetPluginNPP(npp),
                                   const_cast<char*>(message));
}

NPError NPNavigator::GetValue(NPP npp, NPNVariable variable, void* value) {
  char buf[sizeof(NPCapability)];
  nacl_abi_size_t bufsize = static_cast<nacl_abi_size_t>(sizeof(buf));
  int32_t error_code;

  if (NACL_SRPC_RESULT_OK !=
      NPModuleRpcClient::NPN_GetValue(
          channel(),
          GetPluginNPP(npp),
          static_cast<int32_t>(variable),
          &error_code,
          &bufsize,
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
  NPModuleRpcClient::NPN_InvalidateRect(
      channel(),
      GetPluginNPP(npp),
      static_cast<nacl_abi_size_t>(sizeof(*invalid_rect)),
      reinterpret_cast<char*>(invalid_rect));
}

void NPNavigator::ForceRedraw(NPP npp) {
  NPModuleRpcClient::NPN_ForceRedraw(channel(), GetPluginNPP(npp));
}

NPObject* NPNavigator::CreateArray(NPP npp) {
  char buf[sizeof(NPCapability)];
  int32_t success;
  nacl_abi_size_t bufsize = static_cast<nacl_abi_size_t>(sizeof(buf));
  if (NACL_SRPC_RESULT_OK !=
      NPModuleRpcClient::NPN_CreateArray(
          channel(),
          GetPluginNPP(npp),
          &success,
          &bufsize,
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
  int32_t error_code;
  if (NACL_SRPC_RESULT_OK !=
      NPModuleRpcClient::NPN_OpenURL(
          channel(),
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

class NPPendingCallClosure {
 public:
  NPPendingCallClosure(NPNavigator::AsyncCallFunc func, void* user_data) :
    func_(func), user_data_(user_data) {
    }
  NPNavigator::AsyncCallFunc func() const { return func_; }
  void* user_data() const { return user_data_; }

 private:
  NPNavigator::AsyncCallFunc func_;
  void* user_data_;
};

// TODO(sehr): this should move to platform.
class MutexLock {
  pthread_mutex_t* mutex_;
 public:
  explicit MutexLock(pthread_mutex_t* mutex) : mutex_(mutex) {
    if (NULL == mutex_) {
      abort();
    } else {
      pthread_mutex_lock(mutex_);
    }
  }
  ~MutexLock() {
    if (NULL == mutex_) {
      abort();
    } else {
      pthread_mutex_unlock(mutex_);
    }
  }
};

void NPNavigator::PluginThreadAsyncCall(NPP instance,
                                        AsyncCallFunc func,
                                        void* user_data) {
  uint32_t next_call;
  DebugPrintf("PluginThreadAsyncCall %p %p %p\n", instance, func, user_data);
  // The map of pending calls.
  NPPendingCallClosure* closure =
      new(std::nothrow) NPPendingCallClosure(func, user_data);
  if (NULL == closure)
    return;
  // There may be out-of-order responses to calls from different threads.
  // Hence we guard the accesses by a mutex.
  MutexLock ml(&pending_mu_);
  next_call = next_pending_call++;
  pending_calls_[next_call] = closure;
  // Send an RPC to the NPAPI upcall thread in the browser plugin.
  NaClSrpcInvokeByName(
      upcall_channel_,
      "NPN_PluginThreadAsyncCall",
      GetPluginNPP(instance),
      static_cast<int32_t>(next_call));
}

NaClSrpcError NPNavigator::DoAsyncCall(int32_t number) {
  uint32_t key = static_cast<uint32_t>(number);
  AsyncCallFunc func;
  void* user_data;
  {  /* SCOPE */
    MutexLock ml(&pending_mu_);
    std::map<uint32_t, NPPendingCallClosure*>::iterator i;
    i = pending_calls_.find(key);
    if (pending_calls_.end() == i)
      return NACL_SRPC_RESULT_APP_ERROR;
    NPPendingCallClosure* closure = i->second;
    func = closure->func();
    user_data = closure->user_data();
    delete closure;
    pending_calls_.erase(i);
  }
  // Invoke the function with the data.
  (*func)(user_data);
  // Return success.
  return NACL_SRPC_RESULT_OK;
}

}  // namespace nacl
