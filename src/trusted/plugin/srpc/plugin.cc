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

#include "native_client/src/trusted/plugin/srpc/plugin.h"

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <string>

#include "native_client/src/include/portability_string.h"

#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/srpc.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc/closure.h"

#include "native_client/src/trusted/plugin/srpc/video.h"
#include "native_client/src/trusted/plugin/origin.h"

#include "native_client/src/trusted/plugin/srpc/connected_socket.h"
#include "native_client/src/trusted/plugin/srpc/service_runtime_interface.h"
#include "native_client/src/trusted/plugin/srpc/shared_memory.h"

#include "native_client/src/trusted/plugin/srpc/utility.h"

namespace nacl_srpc {

int Plugin::number_alive_counter = 0;

void Plugin::LoadMethods() {
  // the only method supported by PortableHandle
  AddMethodToMap(UrlAsNaClDesc, "__urlAsNaClDesc", METHOD_CALL, "so", "");
  AddMethodToMap(ShmFactory, "__shmFactory", METHOD_CALL, "i", "h");
  AddMethodToMap(SocketAddressFactory,
      "__socketAddressFactory", METHOD_CALL, "s", "h");
  AddMethodToMap(DefaultSocketAddress,
      "__defaultSocketAddress", METHOD_CALL, "", "h");
  AddMethodToMap(NullPluginMethod, "__nullPluginMethod", METHOD_CALL, "s", "i");
  AddMethodToMap(GetHeightProperty, "height", PROPERTY_GET, "", "i");
  AddMethodToMap(SetHeightProperty, "height", PROPERTY_SET, "i", "");
  AddMethodToMap(GetWidthProperty, "width", PROPERTY_GET, "", "i");
  AddMethodToMap(SetWidthProperty, "width", PROPERTY_SET, "i", "");
  AddMethodToMap(GetModuleReadyProperty,
      "__moduleReady", PROPERTY_GET, "", "i");
  AddMethodToMap(SetModuleReadyProperty,
      "__moduleReady", PROPERTY_SET, "i", "");
  AddMethodToMap(GetSrcProperty,
      "src", PROPERTY_GET, "", "s");
  AddMethodToMap(SetSrcProperty,
      "src", PROPERTY_SET, "s", "");
  AddMethodToMap(GetVideoUpdateModeProperty,
      "videoUpdateMode", PROPERTY_GET, "", "i");
  AddMethodToMap(SetVideoUpdateModeProperty,
      "videoUpdateMode", PROPERTY_SET, "i", "");
}

bool Plugin::HasMethodEx(uintptr_t method_id, CallType call_type) {
  // The requested method is not implemented by the Plugin,
  // maybe it is implemented by the NaCl module
  if (socket_) {
    ConnectedSocket* real_socket =
        static_cast<ConnectedSocket*>(socket_->get_handle());
    return real_socket->HasMethod(method_id, call_type);
  }
  return PortableHandle::HasMethodEx(method_id, call_type);
}

bool Plugin::InvokeEx(uintptr_t method_id,
                      CallType call_type,
                      SrpcParams *params) {
  if (socket_) {
    ConnectedSocket* real_socket =
        static_cast<ConnectedSocket*>(socket_->get_handle());
    return real_socket->Invoke(method_id, call_type, params);
  }
  // the method is not supported
  return PortableHandle::InvokeEx(method_id, call_type, params);
}

bool Plugin::InitParamsEx(uintptr_t method_id,
                          CallType call_type,
                          SrpcParams *params) {
  if (socket_) {
    ConnectedSocket* real_socket =
        static_cast<ConnectedSocket*>(socket_->get_handle());
    return real_socket->InitParams(method_id, call_type, params);
  }
  return false;
}


bool Plugin::ShmFactory(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);

  SharedMemoryInitializer init_info(plugin->GetPortablePluginInterface(),
                                    plugin, params->Input(0)->u.ival);
  ScriptableHandle<SharedMemory>* shared_memory =
      ScriptableHandle<SharedMemory>::New(&init_info);
  if (NULL == shared_memory) {
    params->SetExceptionInfo("out of memory");
    return false;
  }

  params->Output(0)->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->Output(0)->u.oval =
      static_cast<BrowserScriptableObject*>(shared_memory);
  return true;
}

bool Plugin::DefaultSocketAddress(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  if (NULL == plugin->socket_address_) {
    params->SetExceptionInfo("no socket address");
    return false;
  }
  plugin->socket_address_->AddRef();
  // Plug the scriptable object into the return values.
  params->Output(0)->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->Output(0)->u.oval =
      static_cast<BrowserScriptableObject*>(plugin->socket_address_);
  return true;
}

// A method to test the cost of invoking a method in a plugin without
// making an RPC to the service runtime.  Used for performance evaluation.
bool Plugin::NullPluginMethod(void *obj, SrpcParams *params) {
  UNREFERENCED_PARAMETER(obj);
  params->Output(0)->tag = NACL_SRPC_ARG_TYPE_INT;
  params->Output(0)->u.ival = 0;
  return true;
}

bool Plugin::SocketAddressFactory(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  // Type check the input parameter.
  if (NACL_SRPC_ARG_TYPE_STRING != params->Input(0)->tag) {
    return false;
  }
  // Ensure it's a valid string short enough to be a socket address.
  char* str = params->Input(0)->u.sval;
  if (NULL == str) {
    return false;
  }
  size_t len = strnlen(str, NACL_PATH_MAX);
  // strnlen ensures NACL_PATH_MAX >= len.  If NACL_PATH_MAX == len, then
  // there is not enough room to hold the address.
  if (NACL_PATH_MAX == len) {
    return false;
  }
  // Create a NaClSocketAddress from the string.
  struct NaClSocketAddress nsap;
  // We need len + 1 to guarantee the zero byte is written.  This is safe,
  // since NACL_PATH_MAX >= len + 1 from above.
  strncpy(nsap.path, str, len + 1);
  // Create a NaClDescConnCap from the socket address.
  struct NaClDescConnCap* conn_cap =
      reinterpret_cast<struct NaClDescConnCap*>(
          malloc(sizeof(struct NaClDescConnCap)));
  if (NULL == conn_cap) {
    return false;
  }
  if (!NaClDescConnCapCtor(conn_cap, &nsap)) {
    free(conn_cap);
    return false;
  }
  // Create a scriptable object to return.
  DescHandleInitializer init_info(plugin->GetPortablePluginInterface(),
                                  reinterpret_cast<struct NaClDesc*>(conn_cap),
                                  plugin);
  ScriptableHandle<SocketAddress>* socket_address =
      ScriptableHandle<SocketAddress>::New(&init_info);
  if (NULL == socket_address) {
    NaClDescUnref(reinterpret_cast<struct NaClDesc*>(conn_cap));
    params->SetExceptionInfo("out of memory");
    return false;
  }
  // Plug the scriptable object into the return values.
  params->Output(0)->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->Output(0)->u.oval =
      static_cast<BrowserScriptableObject*>(socket_address);
  return true;
}

bool Plugin::UrlAsNaClDesc(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);

  const char* url = params->ins[0]->u.sval;
  BrowserScriptableObject* callback_obj =
      reinterpret_cast<BrowserScriptableObject*>(params->ins[1]->u.oval);
  dprintf(("loading %s as file\n", url));
  UrlAsNaClDescNotify* callback = new UrlAsNaClDescNotify(plugin,
                                                          url,
                                                          callback_obj);
  if (NULL == callback) {
    params->SetExceptionInfo("Out of memory in __urlAsNaClDesc");
    return false;
  }

  if (!callback->StartDownload()) {
    dprintf(("failed to load URL %s to local file.\n", url));
    params->SetExceptionInfo("specified url could not be loaded");
    // callback is always deleted in URLNotify
    return false;
  }
  return true;
}

int32_t Plugin::height() {
  return height_;
}

int32_t Plugin::width() {
  return width_;
}

bool Plugin::GetHeightProperty(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  params->outs[0]->u.ival = plugin->height_;
  return true;
}

bool Plugin::SetHeightProperty(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  plugin->height_ = params->ins[0]->u.ival;
  return true;
}

bool Plugin::GetModuleReadyProperty(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  if (plugin->socket_) {
      params->outs[0]->u.ival = 1;
    } else {
      params->outs[0]->u.ival = 0;
    }
  return true;
}

bool Plugin::SetModuleReadyProperty(void *obj, SrpcParams *params) {
  UNREFERENCED_PARAMETER(obj);
  params->SetExceptionInfo("__moduleReady is a read-only property");
  return false;
}

bool Plugin::GetSrcProperty(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  params->outs[0]->u.sval =
    PortablePluginInterface::MemAllocStrdup(plugin->local_url_);
  return true;
}

bool Plugin::SetSrcProperty(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  if (NULL != plugin->service_runtime_interface_) {
    dprintf(("Plugin::SetProperty: unloading previous\n"));
    // Plugin owns socket_address_ and socket_, so when we change to a new
    // socket we need to give up ownership of the old one.
    plugin->socket_address_->Unref();
    plugin->socket_address_ = NULL;
    plugin->socket_->Unref();
    plugin->socket_ = NULL;
    plugin->service_runtime_interface_ = NULL;
  }
  // Load the new module if the origin of the page is valid.
  const char* url = params->ins[0]->u.sval;
  dprintf(("Plugin::SetProperty src = '%s'\n", url));
  LoadNaClAppNotify* callback = new LoadNaClAppNotify(plugin, url);
  if (!callback->StartDownload()) {
    dprintf(("Failed to load URL to local file.\n"));
    // callback is always deleted in URLNotify
    return false;
  }
  return true;
}

bool Plugin::GetVideoUpdateModeProperty(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  params->outs[0]->u.ival = plugin->video_update_mode_;
  return true;
}

bool Plugin::SetVideoUpdateModeProperty(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  plugin->video_update_mode_ = params->ins[0]->u.ival;
  return true;
}

bool Plugin::GetWidthProperty(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  params->outs[0]->u.ival = plugin->width_;
  return true;
}

bool Plugin::SetWidthProperty(void *obj, SrpcParams *params) {
  Plugin *plugin = reinterpret_cast<Plugin*>(obj);
  plugin->width_ = params->ins[0]->u.ival;
  return true;
}


bool Plugin::Init(struct PortableHandleInitializer* init_info) {
  dprintf(("Plugin::Init()\n"));

  if (!PortableHandle::Init(init_info)) {
    return false;
  }

  if (!Start()) {
    return false;
  }

  std::string *href = NULL;
  if (PortablePluginInterface::GetOrigin(
          init_info->plugin_interface_->GetPluginIdentifier(), &href)) {
    origin_ = nacl::UrlToOrigin(*href);
    dprintf(("Plugin::New: origin %s\n", origin_.c_str()));
    // Check that origin is in the list of permitted origins.
    origin_valid_ = nacl::OriginIsInWhitelist(origin_);
    // this implementation of same-origin policy does not take
    // document.domain element into account.
    //
    delete href;
  }
  LoadMethods();
  return true;
}


Plugin::Plugin()
  : effp_(NULL),
    socket_address_(NULL),
    socket_(NULL),
    service_runtime_interface_(NULL),
    local_url_(NULL),
    height_(0),
    video_update_mode_(nacl::kVideoUpdatePluginPaint),
    width_(0) {
  dprintf(("Plugin::Plugin(%p, %d)\n",
           static_cast<void *>(this),
           ++number_alive_counter));
}

bool Plugin::Start() {
  struct NaClDesc* pair[2];

  if (0 != NaClCommonDescMakeBoundSock(pair)) {
    dprintf(("Plugin::Plugin: make bound sock failed.\n"));
    return false;
  }
  if (!NaClNrdXferEffectorCtor(&eff_, pair[0])) {
    dprintf(("Plugin::Plugin: EffectorCtor failed.\n"));
    return false;
  }
  effp_ = (struct NaClDescEffector*) &eff_;
  return true;
}


Plugin::~Plugin() {
  dprintf(("Plugin::~Plugin(%p, %d)\n",
           static_cast<void *>(this),
           --number_alive_counter));

  // After invalidation, the browser does not respect reference counting,
  // so we shut down here what we can and prevent attempts to shut down
  // other linked structures in Deallocate.

  // hard shutdown
  if (NULL != service_runtime_interface_) {
    service_runtime_interface_->Shutdown();
    // TODO(sehr): this needs to free the interface and set it to NULL.
  }


  // Free the socket address for this plugin, if any.
  if (NULL != socket_address_) {
    dprintf(("Plugin::~Plugin: unloading\n"));
    // Deallocating a plugin releases ownership of the socket address.
    socket_address_->Unref();
  }
  // Free the connected socket for this plugin, if any.
  if (NULL != socket_) {
    dprintf(("Plugin::~Plugin: unloading\n"));
    // Deallocating a plugin releases ownership of the socket.
    socket_->Unref();
  }
  // Clear the pointers to the connected socket and service runtime interface.
  socket_address_ = NULL;
  socket_ = NULL;
  service_runtime_interface_ = NULL;
  dprintf(("Plugin::~Plugin(%p)\n", static_cast<void *>(this)));
  free(local_url_);
}

void Plugin::set_local_url(const char* name) {
  dprintf(("Plugin::set_local_url(%s)\n", name));
  local_url_ = PortablePluginInterface::MemAllocStrdup(name);
}

// Create a new service node from a downloaded service.
bool Plugin::Load() {
  return Load(NULL, 0);
}

bool Plugin::Load(const void* buffer, int32_t size) {
  if (NULL == buffer) {
    dprintf(("Plugin::Load(%s)\n", local_url_));
  } else {
    dprintf(("Plugin::Load(%p, %d)\n", buffer, size));
  }

  // If the page origin where the EMBED/OBJECT tag occurs is not in
  // the whitelist, refuse to load.  If the NaCl module's origin is
  // not in the whitelist, also refuse to load.
  if (!origin_valid_ || !nacl::OriginIsInWhitelist(nacl_module_origin())) {
    printf("Load failed: NaCl module did not come from a whitelisted"
           " source.\nSee nacl_plugin/origin.cc for the list.");
    const char *message = "Load failed: NaCl module did not"
                          " come from a whitelisted source.\\n"
                          "See nacl_plugin/origin.cc for the list');";
    PortablePluginInterface::Alert(
      GetPortablePluginInterface()->GetPluginIdentifier(),
      message,
      strlen(message));
    return false;
  }
  // Catch any bad accesses, etc., while loading.
  nacl_srpc::ScopedCatchSignals sigcatcher(
    (nacl_srpc::ScopedCatchSignals::SigHandlerType) SignalHandler);
  if (PLUGIN_SETJMP(loader_env, 1)) {
    return false;
  }

  dprintf(("Load: NaCl module from '%s'\n", local_url_));

  // check ABI version compatibility
  PortablePluginInterface *plugin_interface = GetPortablePluginInterface();
  bool success = false;
  if (NULL == buffer) {
    success = PortablePluginInterface::CheckExecutableVersion(
        plugin_interface->GetPluginIdentifier(), local_url_);
  } else {
    success = PortablePluginInterface::CheckExecutableVersion(
        plugin_interface->GetPluginIdentifier(), buffer, size);
  }
  if (!success) {
    dprintf(("Load: FAILED due to possible ABI version mismatch\n"));
    return false;
  }
  // Load a file via a forked sel_ldr process.
  service_runtime_interface_ =
    new(std::nothrow) ServiceRuntimeInterface(GetPortablePluginInterface(),
                                              this);
  if (NULL == service_runtime_interface_) {
    dprintf((" ServiceRuntimeInterface Ctor failed\n"));
    return false;
  }
  bool service_runtime_started = false;
  if (NULL == buffer) {
    service_runtime_started = service_runtime_interface_->Start(local_url_);
  } else {
    service_runtime_started = service_runtime_interface_->Start(local_url_,
                                                                buffer,
                                                                size);
  }
  if (!service_runtime_started) {
    dprintf(("  Load: FAILED to start service runtime"));
    return false;
  }

  dprintf(("  Load: started sel_ldr\n"));
  socket_address_ = service_runtime_interface_->default_socket_address();
  dprintf(("  Load: established socket address %p\n",
           static_cast<void *>(socket_address_)));
  socket_ = service_runtime_interface_->default_socket();
  dprintf(("  Load: established socket %p\n", static_cast<void *>(socket_)));
  // Plugin takes ownership of socket_ from service_runtime_interface_,
  // so we do not need to call NPN_RetainObject.
  return true;
}

bool Plugin::LogAtServiceRuntime(int severity, std::string msg) {
  return service_runtime_interface_->LogAtServiceRuntime(severity, msg);
}

void Plugin::SignalHandler(int value) {
  dprintf(("Plugin::SignalHandler()\n"));
  PLUGIN_LONGJMP(loader_env, value);
}

Plugin* Plugin::GetPlugin() {
  return this;
}


PLUGIN_JMPBUF Plugin::loader_env;


}  // namespace nacl_srpc
