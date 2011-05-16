/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/plugin.h"

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/plugin/origin.h"
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/connected_socket.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/shared_memory.h"
#include "native_client/src/trusted/plugin/socket_address.h"
#include "native_client/src/trusted/plugin/stream_shm_buffer.h"
#include "native_client/src/trusted/plugin/string_encoding.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace plugin {

namespace {

static int32_t stringToInt32(char* src) {
  return strtol(src, static_cast<char**>(NULL), 0);
}

bool ShmFactory(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);

  SharedMemory* portable_shared_memory =
      SharedMemory::New(plugin, params->ins()[0]->u.ival);
  ScriptableHandle* shared_memory =
      plugin->browser_interface()->NewScriptableHandle(portable_shared_memory);
  if (NULL == shared_memory) {
    params->set_exception_string("out of memory");
    portable_shared_memory->Delete();
    return false;
  }

  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->outs()[0]->arrays.oval = shared_memory;
  return true;
}

bool DefaultSocketAddress(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  if (NULL == plugin->socket_address()) {
    params->set_exception_string("no socket address");
    return false;
  }
  // Plug the scriptable object into the return values.
  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->outs()[0]->arrays.oval = plugin->socket_address()->AddRef();
  return true;
}

bool GetSandboxISAProperty(void* obj, SrpcParams* params) {
  UNREFERENCED_PARAMETER(obj);
  const char* isa = GetSandboxISA();
  PLUGIN_PRINTF(("GetSandboxISAProperty ('isa'='%s')\n", isa));
  params->outs()[0]->arrays.str = strdup(isa);
  return true;
}

// A method to test the cost of invoking a method in a plugin without
// making an RPC to the service runtime.  Used for performance evaluation.
bool NullPluginMethod(void* obj, SrpcParams* params) {
  UNREFERENCED_PARAMETER(obj);
  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_INT;
  params->outs()[0]->u.ival = 0;
  return true;
}

bool GetModuleReadyProperty(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  if (plugin->nacl_module_ready()) {
    params->outs()[0]->u.ival = 1;
  } else {
    params->outs()[0]->u.ival = 0;
  }
  return true;
}

bool SetModuleReadyProperty(void* obj, SrpcParams* params) {
  UNREFERENCED_PARAMETER(obj);
  params->set_exception_string("__moduleReady is a read-only property");
  return false;
}

bool GetSrcProperty(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  const char* url = plugin->manifest_url().c_str();
  PLUGIN_PRINTF(("GetSrcProperty ('src'='%s')\n", url));
  if (NACL_NO_URL != plugin->manifest_url()) {
    params->outs()[0]->arrays.str = strdup(url);
    return true;
  } else {
    // No url set for 'src'.
    return false;
  }
}

bool SetSrcProperty(void* obj, SrpcParams* params) {
  PLUGIN_PRINTF(("SetSrcProperty ()\n"));
  reinterpret_cast<Plugin*>(obj)->
      SetSrcPropertyImpl(params->ins()[0]->arrays.str);
  return true;
}

bool LaunchExecutableFromFd(void* obj, SrpcParams* params) {
  PLUGIN_PRINTF(("LaunchExecutableFromFd ()\n"));
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  NaClDescRef(params->ins()[0]->u.hval);
  nacl::scoped_ptr<nacl::DescWrapper>
      wrapper(plugin->wrapper_factory()->MakeGeneric(params->ins()[0]->u.hval));
  plugin->set_nacl_module_ready(false);
  // Generate the event stream for loading a module.
  plugin->DispatchProgressEvent("loadstart",
                                false,  // length_computable
                                Plugin::kUnknownBytes,
                                Plugin::kUnknownBytes);
  nacl::string error_string;
  bool was_successful = plugin->LoadNaClModule(wrapper.get(), &error_string);
  // Set the __moduleReady attribute to indicate ready to start.
  plugin->set_nacl_module_ready(was_successful);
  if (was_successful) {
    plugin->ReportLoadSuccess(false,  // length_computable
                              Plugin::kUnknownBytes,
                              Plugin::kUnknownBytes);
  } else {
    // For reasons unknown, the message is garbled on windows.
    // TODO(sehr): know the reasons, and fix this.
    plugin->ReportLoadError(nacl::string("__launchExecutableFromFd failed: ") +
                            error_string);
  }
  return was_successful;
}

bool GetHeightProperty(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  params->outs()[0]->u.ival = plugin->height();
  return true;
}

bool SetHeightProperty(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  plugin->set_height(params->ins()[0]->u.ival);
  return true;
}

bool GetWidthProperty(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  params->outs()[0]->u.ival = plugin->width();
  return true;
}

bool SetWidthProperty(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  plugin->set_width(params->ins()[0]->u.ival);
  return true;
}

}  // namespace

// TODO(mseaborn): Although this will usually not block, it will
// block if the socket's buffer fills up.
bool Plugin::SendAsyncMessage(void* obj, SrpcParams* params,
                              nacl::DescWrapper** fds, int fds_count) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  if (plugin->service_runtime_ == NULL) {
    params->set_exception_string("No subprocess running");
    return false;
  }

  // TODO(mseaborn): Handle strings containing NULLs.  This might
  // involve using a different SRPC type.
  char* utf8string = params->ins()[0]->arrays.str;
  char* data;
  size_t data_size;
  if (!ByteStringFromUTF8(utf8string, strlen(utf8string), &data, &data_size)) {
    params->set_exception_string("Invalid string");
    return false;
  }
  nacl::DescWrapper::MsgIoVec iov;
  nacl::DescWrapper::MsgHeader message;
  iov.base = data;
  iov.length = static_cast<nacl_abi_size_t>(data_size);
  message.iov = &iov;
  message.iov_length = 1;
  message.ndescv = fds;
  message.ndescv_length = fds_count;
  message.flags = 0;
  nacl::DescWrapper* socket = plugin->service_runtime_->async_send_desc();
  ssize_t sent = socket->SendMsg(&message, 0);
  free(data);
  if (sent < 0) {
    params->set_exception_string("Error sending message");
    return false;
  }
  return true;
}

// TODO(mseaborn): Combine __sendAsyncMessage0 and __sendAsyncMessage1
// into a single method that takes an array of FDs.  SRPC does not
// provide a handle array type so there is not a simple way to do
// this.
bool Plugin::SendAsyncMessage0(void* obj, SrpcParams* params) {
  return SendAsyncMessage(obj, params, NULL, 0);
}

bool Plugin::SendAsyncMessage1(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  NaClDescRef(params->ins()[1]->u.hval);
  nacl::DescWrapper* fd_to_send =
    plugin->wrapper_factory()->MakeGeneric(params->ins()[1]->u.hval);
  bool result = SendAsyncMessage(obj, params, &fd_to_send, 1);
  // TODO(sehr,mseaborn): use scoped_ptr for management of DescWrappers.
  delete fd_to_send;
  return result;
}

bool Plugin::StartSrpcServicesWrapper(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  nacl::string error_string;
  if (!plugin->StartSrpcServices(&error_string)) {
    params->set_exception_string(error_string.c_str());
    return false;
  }
  return true;
}

bool Plugin::ExperimentalJavaScriptApisAreEnabled() {
  return getenv("NACL_ENABLE_EXPERIMENTAL_JAVASCRIPT_APIS") != NULL;
}

static int const kAbiHeaderBuffer = 256;  // must be at least EI_ABIVERSION + 1

void Plugin::LoadMethods() {
  PLUGIN_PRINTF(("Plugin::LoadMethods ()\n"));
  // Properties implemented by Plugin.
  AddPropertyGet(GetModuleReadyProperty, "__moduleReady", "i");
  AddPropertySet(SetModuleReadyProperty, "__moduleReady", "i");

  AddPropertyGet(GetSrcProperty, "src", "s");
  AddPropertySet(SetSrcProperty, "src", "s");
  if (!ExperimentalJavaScriptApisAreEnabled()) {
    return;
  }
  // Experimental methods supported by Plugin.
  // These methods are explicitly not included in shipping versions of Chrome.
  AddMethodCall(ShmFactory, "__shmFactory", "i", "h");
  AddMethodCall(DefaultSocketAddress, "__defaultSocketAddress", "", "h");
  AddMethodCall(GetSandboxISAProperty, "__getSandboxISA", "", "s");
  AddMethodCall(LaunchExecutableFromFd, "__launchExecutableFromFd", "h", "");
  AddMethodCall(NullPluginMethod, "__nullPluginMethod", "s", "i");
  AddMethodCall(SendAsyncMessage0, "__sendAsyncMessage0", "s", "");
  AddMethodCall(SendAsyncMessage1, "__sendAsyncMessage1", "sh", "");
  AddMethodCall(StartSrpcServicesWrapper, "__startSrpcServices", "", "");
  // With PPAPI plugin, we make sure all predeclared plugin properties start
  // with __ to avoid conflicts with the properties of the underlying object
  // (e.g. height).
  // TODO(polina): Make the PPAPI nexe inherit from the plugin to provide
  // access to these properties.
  AddPropertyGet(GetHeightProperty, "__height", "i");
  AddPropertySet(SetHeightProperty, "__height", "i");
  AddPropertyGet(GetWidthProperty, "__width", "i");
  AddPropertySet(SetWidthProperty, "__width", "i");
}

bool Plugin::HasMethodEx(uintptr_t method_id, CallType call_type) {
  if (NULL == socket_) {
    return false;
  }
  if (!ExperimentalJavaScriptApisAreEnabled()) {
    return false;
  }
  return socket_->handle()->HasMethod(method_id, call_type);
}

bool Plugin::InvokeEx(uintptr_t method_id,
                      CallType call_type,
                      SrpcParams* params) {
  if (NULL == socket_) {
    return false;
  }
  if (!ExperimentalJavaScriptApisAreEnabled()) {
    return false;
  }
  return socket_->handle()->Invoke(method_id, call_type, params);
}

bool Plugin::InitParamsEx(uintptr_t method_id,
                          CallType call_type,
                          SrpcParams* params) {
  if (NULL == socket_) {
    return false;
  }
  if (!ExperimentalJavaScriptApisAreEnabled()) {
    return false;
  }
  return socket_->handle()->InitParams(method_id, call_type, params);
}

void Plugin::SetSrcPropertyImpl(const nacl::string& url) {
  PLUGIN_PRINTF(("Plugin::SetSrcPropertyImpl (unloading previous)\n"));
  // We do not actually need to shut down the process here when
  // initiating the (asynchronous) download.  It is more important to
  // shut down the old process when the download completes and a new
  // process is launched.
  ShutDownSubprocess();
  RequestNaClManifest(url);
}

bool Plugin::Init(BrowserInterface* browser_interface,
                  InstanceIdentifier instance_id,
                  int argc,
                  char* argn[],
                  char* argv[]) {
  PLUGIN_PRINTF(("Plugin::Init (instance_id=%p)\n",
                 reinterpret_cast<void*>(instance_id)));

  browser_interface_ = browser_interface;
  instance_id_ = instance_id;
  // Remember the embed/object argn/argv pairs.
  argn_ = new(std::nothrow) char*[argc];
  argv_ = new(std::nothrow) char*[argc];
  argc_ = 0;
  // Set up the height and width attributes if passed (for Opera)
  for (int i = 0; i < argc; ++i) {
    if (!strncmp(argn[i], "height", 7)) {
      set_height(stringToInt32(argv[i]));
    } else if (!strncmp(argn[i], "width", 6)) {
      set_width(stringToInt32(argv[i]));
    } else {
      if (NULL != argn_ && NULL != argv_) {
        argn_[argc_] = strdup(argn[i]);
        argv_[argc_] = strdup(argv[i]);
        if (NULL == argn_[argc_] ||
            NULL == argv_[argc_]) {
          // Give up on passing arguments.
          free(argn_[argc_]);
          free(argv_[argc_]);
          continue;
        }
        ++argc_;
      }
    }
  }
  // TODO(sehr): this leaks strings if there is a subsequent failure.

  // Set up the factory used to produce DescWrappers.
  wrapper_factory_ = new nacl::DescWrapperFactory();
  if (NULL == wrapper_factory_) {
    return false;
  }
  PLUGIN_PRINTF(("Plugin::Init (wrapper_factory=%p)\n",
                 reinterpret_cast<void*>(wrapper_factory_)));

  // Check that the origin is allowed.
  origin_ = NACL_NO_URL;
  if (browser_interface_->GetOrigin(instance_id_, &origin_)) {
    PLUGIN_PRINTF(("Plugin::Init (origin='%s')\n", origin_.c_str()));
    // Check that origin is in the list of permitted origins.
    origin_valid_ = nacl::OriginIsInWhitelist(origin_);
    // This implementation of same-origin policy does not take
    // document.domain element into account.
  }

  // Set up the scriptable methods for the plugin.
  LoadMethods();

  PLUGIN_PRINTF(("Plugin::Init (return 1)\n"));
  // Return success.
  return true;
}

Plugin::Plugin()
  : service_runtime_(NULL),
    receive_thread_running_(false),
    browser_interface_(NULL),
    scriptable_handle_(NULL),
    argc_(-1),
    argn_(NULL),
    argv_(NULL),
    socket_address_(NULL),
    socket_(NULL),
    origin_valid_(false),
    nacl_module_ready_(false),
    height_(0),
    width_(0),
    wrapper_factory_(NULL) {
  PLUGIN_PRINTF(("Plugin::Plugin (this=%p)\n", static_cast<void*>(this)));
}

// TODO(polina): move this to PluginNpapi.
void Plugin::Invalidate() {
  PLUGIN_PRINTF(("Plugin::Invalidate (this=%p)\n", static_cast<void*>(this)));
  socket_address_ = NULL;
  socket_ = NULL;
}

void Plugin::ShutDownSubprocess() {
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocess (this=%p)\n",
                 static_cast<void*>(this)));
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocess "
                 "(socket_address=%p, socket=%p)\n",
                 static_cast<void*>(socket_address_),
                 static_cast<void*>(socket_)));
  UnrefScriptableHandle(&socket_address_);
  UnrefScriptableHandle(&socket_);
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocess (service_runtime=%p)\n",
                 static_cast<void*>(service_runtime_)));
  // Shutdown service runtime. This must be done before all other calls so
  // they don't block forever when waiting for the upcall thread to exit.
  if (service_runtime_ != NULL) {
    service_runtime_->Shutdown();
    delete service_runtime_;
    service_runtime_ = NULL;
  }
  ShutdownMultimedia();
  if (receive_thread_running_) {
    NaClThreadJoin(&receive_thread_);
    receive_thread_running_ = false;
  }
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocess (this=%p, return)\n",
                 static_cast<void*>(this)));
}

void Plugin::UnrefScriptableHandle(ScriptableHandle** handle) {
  if (*handle != NULL) {
    (*handle)->Unref();
    *handle = NULL;
  }
}

Plugin::~Plugin() {
  PLUGIN_PRINTF(("Plugin::~Plugin (this=%p)\n", static_cast<void*>(this)));

  // After invalidation, the browser does not respect reference counting,
  // so we shut down here what we can and prevent attempts to shut down
  // other linked structures in Deallocate.

  // We should not need to call ShutDownSubprocess() here.  In the
  // NPAPI plugin, it should have already been called in
  // NPP_Destroy().
  ShutDownSubprocess();

  delete wrapper_factory_;
  delete browser_interface_;
  delete[] argv_;
  delete[] argn_;
  PLUGIN_PRINTF(("Plugin::~Plugin (this=%p, return)\n",
                 static_cast<void*>(this)));
}

bool Plugin::IsValidNexeOrigin(nacl::string full_url,
                               nacl::string* error_string) {
  PLUGIN_PRINTF(("Plugin::IsValidNexeOrigin (full_url='%s')\n",
                 full_url.c_str()));
  CHECK(NACL_NO_URL != full_url);
  set_nacl_module_origin(nacl::UrlToOrigin(full_url));
  set_manifest_url(full_url);

  bool module_origin_valid = nacl::OriginIsInWhitelist(nacl_module_origin_);
  PLUGIN_PRINTF(("Plugin::IsValidNexeOrigin "
                 "(page_origin='%s', page_origin_valid=%d)\n",
                 origin_.c_str(), origin_valid_));
  PLUGIN_PRINTF(("Plugin::IsValidNexeOrigin "
                 "(nacl_origin='%s', nacl_origin_valid=%d)\n",
                 nacl_module_origin_.c_str(), module_origin_valid));

  // If the origin of the nacl module or of the page with <embed>/<object>
  // NaCl plugin element is not in the whitelist, refuse to load.
  // TODO(adonovan): JavaScript permits cross-origin loading, and so
  // does Chrome; why don't we?
  if (!origin_valid_ || !module_origin_valid) {
    *error_string = nacl::string("module URL ") +
        manifest_url() + " uses an unsupported protocol. "
        "Only http, https, and chrome-extension are currently supported.";
    return false;
  }
  return true;
}

bool Plugin::LoadNaClModule(nacl::DescWrapper* wrapper,
                            nacl::string* error_string) {
  // Check ELF magic and ABI version compatibility.
  bool might_be_elf_exe =
      browser_interface_->MightBeElfExecutable(wrapper, error_string);
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (might_be_elf_exe=%d)\n",
                 might_be_elf_exe));
  if (!might_be_elf_exe) {
    return false;
  }

  // Before forking a new sel_ldr process, ensure that we do not leak
  // the ServiceRuntime object for an existing suprocess, and that any
  // associated listener threads do not go unjoined because if they
  // outlive the Plugin object, they will not be memory safe.
  ShutDownSubprocess();
  service_runtime_ =
      new(std::nothrow) ServiceRuntime(this);
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (service_runtime=%p)\n",
                 static_cast<void*>(service_runtime_)));
  if (NULL == service_runtime_) {
    *error_string = "sel_ldr init failure.";
    return false;
  }

  bool service_runtime_started = false;
#if defined(NACL_STANDALONE)
  service_runtime_started =
      service_runtime_->StartFromCommandLine(NACL_NO_FILE_PATH,
                                             wrapper,
                                             error_string);
#else
  service_runtime_started =
      service_runtime_->StartFromBrowser(NACL_NO_FILE_PATH,
                                         wrapper,
                                         error_string);
#endif
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (service_runtime_started=%d)\n",
                 service_runtime_started));

  // Plugin takes ownership of socket_address_ from service_runtime_ and will
  // Unref it at deletion. This must be done here because service_runtime_
  // start-up might fail after default_socket_address() was already created.
  socket_address_ = service_runtime_->default_socket_address();
  if (!service_runtime_started) {
    return false;
  }
  CHECK(NULL != socket_address_);
  if (!StartSrpcServices(error_string)) {  // sets socket_
    return false;
  }
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (socket_address=%p, socket=%p)\n",
                 static_cast<void*>(socket_address_),
                 static_cast<void*>(socket_)));
  return true;
}

bool Plugin::StartSrpcServices(nacl::string* error_string) {
  UnrefScriptableHandle(&socket_);
  socket_ = socket_address_->handle()->Connect();
  if (socket_ == NULL) {
    *error_string = "SRPC connection failure.";
    return false;
  }
  if (!socket_->handle()->StartJSObjectProxy(this, error_string)) {
    // TODO(sehr,polina): rename the below to ExperimentalSRPCApisAreEnabled.
    if (ExperimentalJavaScriptApisAreEnabled()) {
      // It is not an error for the proxy to fail to start if experimental
      // APIs are enabled.  This means we have an SRPC nexe.
      *error_string = "";
    } else {
      return false;
    }
  }
  PLUGIN_PRINTF(("Plugin::Load (established socket %p)\n",
                 static_cast<void*>(socket_)));
  return true;
}

char* Plugin::LookupArgument(const char* key) {
  char** keys = argn();
  for (int ii = 0, len = argc(); ii < len; ++ii) {
    if (!strcmp(keys[ii], key)) {
      return argv()[ii];
    }
  }
  return NULL;
}

}  // namespace plugin
