/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
// TODO(sehr): move video_update_mode_ to PluginNpapi
#include "native_client/src/trusted/plugin/npapi/video.h"
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

namespace {

static int32_t stringToInt32(char* src) {
  return strtol(src,  // NOLINT(runtime/deprecated_fn)
                static_cast<char**>(NULL), 0);
}

// TODO(sehr): using a static jmpbuf here has several problems.  Notably we
// cannot reliably handle errors when there are multiple plugins in the same
// process.  Issue 605.
PLUGIN_JMPBUF g_LoaderEnv;

bool ShmFactory(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);

  plugin::SharedMemory* portable_shared_memory =
      plugin::SharedMemory::New(plugin, params->ins()[0]->u.ival);
  plugin::ScriptableHandle* shared_memory =
      plugin->browser_interface()->NewScriptableHandle(portable_shared_memory);
  if (NULL == shared_memory) {
    params->set_exception_string("out of memory");
    portable_shared_memory->Delete();
    return false;
  }

  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->outs()[0]->u.oval = shared_memory;
  return true;
}

bool DefaultSocketAddress(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  if (NULL == plugin->socket_address()) {
    params->set_exception_string("no socket address");
    return false;
  }
  plugin->socket_address()->AddRef();
  // Plug the scriptable object into the return values.
  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->outs()[0]->u.oval = plugin->socket_address();
  return true;
}

// A method to test the cost of invoking a method in a plugin without
// making an RPC to the service runtime.  Used for performance evaluation.
bool NullPluginMethod(void* obj, plugin::SrpcParams* params) {
  UNREFERENCED_PARAMETER(obj);
  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_INT;
  params->outs()[0]->u.ival = 0;
  return true;
}

bool GetModuleReadyProperty(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  if (plugin->socket()) {
    params->outs()[0]->u.ival = 1;
  } else {
    params->outs()[0]->u.ival = 0;
  }
  return true;
}

bool SetModuleReadyProperty(void* obj, plugin::SrpcParams* params) {
  UNREFERENCED_PARAMETER(obj);
  params->set_exception_string("__moduleReady is a read-only property");
  return false;
}

bool GetNexesProperty(void* obj, plugin::SrpcParams* params) {
  UNREFERENCED_PARAMETER(obj);
  UNREFERENCED_PARAMETER(params);
  params->set_exception_string("__nexes is a write-only property");
  return false;
}

// Update "nexes", a write-only property that computes a value to
// assign to the "src" property based on the supported sandbox.
bool SetNexesProperty(void* obj, plugin::SrpcParams* params) {
  return reinterpret_cast<plugin::Plugin*>(obj)->
      SetNexesPropertyImpl(params->ins()[0]->u.sval);
}

bool GetSrcProperty(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  if (plugin->logical_url() != NULL) {
    params->outs()[0]->u.sval = strdup(plugin->logical_url());
    PLUGIN_PRINTF(("GetSrcProperty ('src'='%s')\n", plugin->logical_url()));
    return true;
  } else {
    // (NULL is not an acceptable SRPC result.)
    PLUGIN_PRINTF(("GetSrcProperty ('src' failed)\n"));
    return false;
  }
}

bool SetSrcProperty(void* obj, plugin::SrpcParams* params) {
  PLUGIN_PRINTF(("SetSrcProperty ()\n"));
  return reinterpret_cast<plugin::Plugin*>(obj)->
      SetSrcPropertyImpl(params->ins()[0]->u.sval);
}

bool GetHeightProperty(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  params->outs()[0]->u.ival = plugin->height();
  return true;
}

bool SetHeightProperty(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  plugin->set_height(params->ins()[0]->u.ival);
  return true;
}

bool GetWidthProperty(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  params->outs()[0]->u.ival = plugin->width();
  return true;
}

bool SetWidthProperty(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  plugin->set_width(params->ins()[0]->u.ival);
  return true;
}

bool GetVideoUpdateModeProperty(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  params->outs()[0]->u.ival = plugin->video_update_mode();
  return true;
}

bool SetVideoUpdateModeProperty(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  plugin->set_video_update_mode(params->ins()[0]->u.ival);
  return true;
}

void SignalHandler(int value) {
  PLUGIN_PRINTF(("Plugin::SignalHandler ()\n"));
  PLUGIN_LONGJMP(g_LoaderEnv, value);
}

}  // namespace

namespace plugin {

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
  char* utf8string = params->ins()[0]->u.sval;
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
  nacl::DescWrapper* socket = plugin->service_runtime_->async_send_desc;
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
  fd_to_send->Delete();
  return result;
}

static int const kAbiHeaderBuffer = 256;  // must be at least EI_ABIVERSION + 1

void Plugin::LoadMethods() {
  PLUGIN_PRINTF(("Plugin::LoadMethods ()\n"));
  // Methods supported by Plugin.
  AddMethodCall(ShmFactory, "__shmFactory", "i", "h");
  AddMethodCall(DefaultSocketAddress, "__defaultSocketAddress", "", "h");
  AddMethodCall(NullPluginMethod, "__nullPluginMethod", "s", "i");
  AddMethodCall(SendAsyncMessage0, "__sendAsyncMessage0", "s", "");
  AddMethodCall(SendAsyncMessage1, "__sendAsyncMessage1", "sh", "");
  // Properties implemented by Plugin.
  AddPropertyGet(GetModuleReadyProperty, "__moduleReady", "i");
  AddPropertySet(SetModuleReadyProperty, "__moduleReady", "i");
  // With PPAPI plugin, we make sure all predeclared plugin properties start
  // with __ to avoid conflicts with the properties of the underlying object
  // (e.g. height).
  // TODO(polina): Make the PPAPI nexe inherit from the plugin to provide
  // access to these properties.
#if defined(NACL_PPAPI)  // TODO(polina): do this for NPAPI as well.
  AddPropertyGet(GetHeightProperty, "__height", "i");
  AddPropertySet(SetHeightProperty, "__height", "i");
  AddPropertyGet(GetNexesProperty, "__nexes", "s");
  AddPropertySet(SetNexesProperty, "__nexes", "s");
  AddPropertyGet(GetSrcProperty, "__src", "s");
  AddPropertySet(SetSrcProperty, "__src", "s");
  // TODO(polina): confirm that videoUpdateMode is irrelevant for PPAPI and
  // move the property registration and handling functions to PluginNPAPI.
  AddPropertyGet(GetVideoUpdateModeProperty, "__videoUpdateMode", "i");
  AddPropertySet(SetVideoUpdateModeProperty, "__videoUpdateMode", "i");
  AddPropertyGet(GetWidthProperty, "__width", "i");
  AddPropertySet(SetWidthProperty, "__width", "i");
#else
  AddPropertyGet(GetHeightProperty, "height", "i");
  AddPropertySet(SetHeightProperty, "height", "i");
  AddPropertyGet(GetNexesProperty, "nexes", "s");
  AddPropertySet(SetNexesProperty, "nexes", "s");
  AddPropertyGet(GetSrcProperty, "src", "s");
  AddPropertySet(SetSrcProperty, "src", "s");
  AddPropertyGet(GetVideoUpdateModeProperty, "videoUpdateMode", "i");
  AddPropertySet(SetVideoUpdateModeProperty, "videoUpdateMode", "i");
  AddPropertyGet(GetWidthProperty, "width", "i");
  AddPropertySet(SetWidthProperty, "width", "i");
#endif
}

bool Plugin::HasMethodEx(uintptr_t method_id, CallType call_type) {
  if (NULL == socket_) {
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
  return socket_->handle()->Invoke(method_id, call_type, params);
}

bool Plugin::InitParamsEx(uintptr_t method_id,
                          CallType call_type,
                          SrpcParams* params) {
  if (NULL == socket_) {
    return false;
  }
  return socket_->handle()->InitParams(method_id, call_type, params);
}


bool Plugin::SetNexesPropertyImpl(const char* nexes_attr) {
  PLUGIN_PRINTF(("Plugin::SetNexesPropertyImpl (nexes_attr='%s')\n",
                 nexes_attr));
  nacl::string result;
  if (!GetNexeURL(nexes_attr, &result)) {
    // TODO(adonovan): Ideally we would print to the browser's
    // JavaScript console: alert popups are annoying, and no-one can
    // be expected to read stderr.
    PLUGIN_PRINTF(("Plugin::SetNexesPropertyImpl (result='%s')\n",
                   result.c_str()));
    browser_interface()->Alert(instance_id(), result);
    return false;
  } else {
    return SetSrcPropertyImpl(result);
  }
}

bool Plugin::SetSrcPropertyImpl(const nacl::string& url) {
  PLUGIN_PRINTF(("Plugin::SetSrcPropertyImpl (unloading previous)\n"));
  // We do not actually need to shut down the process here when
  // initiating the (asynchronous) download.  It is more important to
  // shut down the old process when the download completes and a new
  // process is launched.
  ShutDownSubprocess();
  return RequestNaClModule(url);
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
    } else if (!strncmp(argn[i], "update", 7)) {
      set_video_update_mode(stringToInt32(argv[i]));
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
  origin_ = BrowserInterface::kUnknownURL;
  if (browser_interface_->GetOrigin(instance_id_, &origin_)) {
    PLUGIN_PRINTF(("Plugin::Init (origin='%s')\n", origin_.c_str()));
    // Check that origin is in the list of permitted origins.
    origin_valid_ = nacl::OriginIsInWhitelist(origin_);
    // This implementation of same-origin policy does not take
    // document.domain element into account.
  }

  // Set up the scriptable methods for the plugin.
  LoadMethods();

  // If the <embed src='...'> attr was defined, the browser would have
  // implicitly called GET on it, which calls Load() and set_logical_url().
  // In the absence of this attr, we use the "nexes" attribute if present.
  if (logical_url() == NULL) {
    const char* nexes_attr = LookupArgument("nexes");
    if (nexes_attr != NULL) {
      SetNexesPropertyImpl(nexes_attr);
    }
  }

  PLUGIN_PRINTF(("Plugin::Init (return 1)\n"));
  // Return success.
  return true;
}

Plugin::Plugin()
  : service_runtime_(NULL),
    receive_thread_running_(false),
    argc_(-1),
    argn_(NULL),
    argv_(NULL),
    socket_address_(NULL),
    socket_(NULL),
    local_url_(NULL),
    logical_url_(NULL),
    origin_valid_(false),
    height_(0),
    width_(0),
    video_update_mode_(kVideoUpdatePluginPaint),
    wrapper_factory_(NULL) {
  PLUGIN_PRINTF(("Plugin::Plugin (this=%p)\n", static_cast<void*>(this)));
}

void Plugin::Invalidate() {
  socket_address_ = NULL;
  socket_ = NULL;
}

void Plugin::ShutDownSubprocess() {
  if (socket_address_ != NULL) {
    socket_address_->Unref();
    socket_address_ = NULL;
  }
  if (socket_ != NULL) {
    socket_->Unref();
    socket_ = NULL;
  }
  if (service_runtime_ != NULL) {
    service_runtime_->Shutdown();
    delete service_runtime_;
    service_runtime_ = NULL;
  }
  // This waits for the upcall thread to exit so it must come after we
  // terminate the subprocess.
  ShutdownMultimedia();
  if (receive_thread_running_) {
    NaClThreadJoin(&receive_thread_);
    receive_thread_running_ = false;
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

  PLUGIN_PRINTF(("Plugin::~Plugin (this=%p)\n", static_cast<void*>(this)));
  free(local_url_);
  free(logical_url_);
  delete wrapper_factory_;
  delete browser_interface_;
  delete[] argv_;
  delete[] argn_;
}

void Plugin::set_local_url(const char* url) {
  if (local_url_ != NULL) free(local_url_);
  local_url_ = strdup(url);
}

void Plugin::set_logical_url(const char* url) {
  if (logical_url_ != NULL) free(logical_url_);
  logical_url_ = strdup(url);
}

// Create a new service node from a downloaded service.
bool Plugin::Load(nacl::string logical_url, const char* local_url) {
  return Load(logical_url, local_url, static_cast<StreamShmBuffer*>(NULL));
}

bool Plugin::Load(nacl::string logical_url,
                  const char* local_url,
                  StreamShmBuffer* shmbufp) {
  PLUGIN_PRINTF(("Plugin::Load (logical_url='%s')\n", logical_url.c_str()));
  PLUGIN_PRINTF(("Plugin::Load (local_url='%s')\n", local_url));
  PLUGIN_PRINTF(("Plugin::Load (shmbufp=%p)\n",
                 reinterpret_cast<void*>(shmbufp)));
  BrowserInterface* browser_interface = this->browser_interface();

  // Save the origin and local_url.
  set_nacl_module_origin(nacl::UrlToOrigin(logical_url));
  set_logical_url(logical_url.c_str());
  set_local_url(local_url);
  bool nacl_module_origin_valid =
      nacl::OriginIsInWhitelist(nacl_module_origin());
  PLUGIN_PRINTF(("Plugin::Load (origin='%s', origin_valid=%d)\n",
                 origin_.c_str(), origin_valid_));
  PLUGIN_PRINTF(("Plugin::Load "
                 "(nacl_module_origin_='%s', nacl_module_origin_valid=%d)\n",
                 nacl_module_origin_.c_str(), nacl_module_origin_valid));
  // If the page origin where the EMBED/OBJECT tag occurs is not in
  // the whitelist, refuse to load.  If the NaCl module's origin is
  // not in the whitelist, also refuse to load.
  // TODO(adonovan): JavaScript permits cross-origin loading, and so
  // does Chrome ; why don't we?
  if (!origin_valid_ || !nacl_module_origin_valid) {
    nacl::string message = nacl::string("Load failed: NaCl module ") +
        logical_url + " does not come ""from a whitelisted source. "
        "See native_client/src/trusted/plugin/origin.cc for the list.";
    browser_interface->Alert(instance_id(), message.c_str());
    PLUGIN_PRINTF(("Plugin::Load (return 0)"));
    return false;
  }
  // Catch any bad accesses, etc., while loading.
  ScopedCatchSignals sigcatcher(
      (ScopedCatchSignals::SigHandlerType) SignalHandler);
  if (PLUGIN_SETJMP(g_LoaderEnv, 1)) {
    return false;
  }

  // Check ELF magic and ABI version compatibility.
  bool success = false;
  nacl::string error_string;
  if (NULL == shmbufp) {
    success = browser_interface->MightBeElfExecutable(local_url_,
                                                      &error_string);
  } else {
    // Read out first chunk for MightBeElfExecutable; this suffices for
    // ELF headers etc.
    char elf_hdr_buf[kAbiHeaderBuffer];
    ssize_t result;
    result = shmbufp->read(0, sizeof elf_hdr_buf, elf_hdr_buf);
    if (sizeof elf_hdr_buf == result) {  // (const char*)(elf_hdr_buf)
      success = browser_interface->MightBeElfExecutable(elf_hdr_buf,
                                                        sizeof elf_hdr_buf,
                                                        &error_string);
    }
  }
  if (!success) {
    PLUGIN_PRINTF(("Plugin::Load (error_string='%s')\n", error_string.c_str()));
    browser_interface->Alert(instance_id(), error_string);
    return false;
  }

  // Ensure that we do not leak the ServiceRuntime object for an
  // existing subprocess.  Ensure that any associated listener threads
  // do not go unjoined, because if they outlive the Plugin object,
  // they will not be memory safe.
  ShutDownSubprocess();

  // Load a file via a forked sel_ldr process.
  service_runtime_ = new(std::nothrow) ServiceRuntime(browser_interface, this);
  if (NULL == service_runtime_) {
    PLUGIN_PRINTF(("Plugin::Load (ServiceRuntime Ctor failed)\n"));
    browser_interface->Alert(instance_id(), "ServiceRuntime Ctor failed");
    return false;
  }
  bool service_runtime_started = false;
  if (NULL == shmbufp) {
    service_runtime_started = service_runtime_->Start(local_url_);
  } else {
    int32_t size;
    NaClDesc* raw_desc = shmbufp->shm(&size);
    if (NULL == raw_desc) {
      PLUGIN_PRINTF(("Plugin::Load (extracting shm failed)\n"));
      return false;
    }
    nacl::DescWrapper* wrapped_shm =
        wrapper_factory_->MakeGeneric(NaClDescRef(raw_desc));
    service_runtime_started =
        service_runtime_->StartUnderChromium(local_url_, wrapped_shm);
    // Start consumes the wrapped_shm.
  }
  if (!service_runtime_started) {
    PLUGIN_PRINTF(("Plugin::Load (failed to start service runtime)\n"));
    browser_interface->Alert(instance_id(),
                             "Load: FAILED to start service runtime");
    return false;
  }

  PLUGIN_PRINTF(("Plugin::Load (started sel_ldr)\n"));
  socket_address_ = service_runtime_->default_socket_address();
  PLUGIN_PRINTF(("Plugin::Load (established socket address %p)\n",
                 static_cast<void*>(socket_address_)));

  socket_ = socket_address_->handle()->Connect();
  if (socket_ == NULL) {
    browser_interface->Alert(instance_id(),
                             "Load: Failed to connect using SRPC");
    return false;
  }
  socket_->handle()->StartJSObjectProxy(this);
  // Create the listener thread and initialize the nacl module.
  if (!InitializeModuleMultimedia(socket_, service_runtime_)) {
    browser_interface->Alert(instance_id(),
                             "Load: Failed to initialise multimedia");
    return false;
  }
  PLUGIN_PRINTF(("Plugin::Load (established socket %p)\n",
                 static_cast<void*>(socket_)));
  // Plugin takes ownership of socket_ from service_runtime_,
  // so we do not need to call NPN_RetainObject.
  // Invoke the onload handler, if any.
  RunOnloadHandler();
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

bool Plugin::RunOnloadHandler() {
  BrowserInterface* browser = browser_interface();
  const char* onload_handler = LookupArgument("onload");
  if (onload_handler == NULL) {
    return true;
  }
  return browser->EvalString(instance_id(), onload_handler);
}

bool Plugin::RunOnfailHandler() {
  BrowserInterface* browser = browser_interface();
  const char* onfail_handler = LookupArgument("onfail");
  if (onfail_handler == NULL) {
    return true;
  }
  return browser->EvalString(instance_id(), onfail_handler);
}

// The NaCl audio/video interface uses a global mutex to protect observed
// timing-sensitive accesses to the plugin's window when starting up and
// shutting down.  Because there is a relatively complex relationship between
// ConnectedSocket and Plugin to synchronize accesses, this code is built in
// both the NPAPI and PPAPI plugins.  This is unfortunate, because only
// the NPAPI plugin (when not used as part of the Chrome integration) actually
// needs the locking.
// TODO(sehr): move this code to somewhere in the npapi directory.
class GlobalVideoMutex {
 public:
  GlobalVideoMutex() { NaClMutexCtor(&mutex_); }
  ~GlobalVideoMutex() { NaClMutexDtor(&mutex_); }
  void Lock() { NaClMutexLock(&mutex_); }
  void Unlock() { NaClMutexUnlock(&mutex_); }

 private:
  NaClMutex mutex_;
};

static GlobalVideoMutex g_VideoMutex;

void VideoGlobalLock() {
  g_VideoMutex.Lock();
}

void VideoGlobalUnlock() {
  g_VideoMutex.Unlock();
}

}  // namespace plugin
