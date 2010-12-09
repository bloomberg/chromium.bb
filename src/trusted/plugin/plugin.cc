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
#include "native_client/src/shared/platform/nacl_check.h"
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
  // Plug the scriptable object into the return values.
  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_OBJECT;
  params->outs()[0]->u.oval = plugin->socket_address()->AddRef();
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
      SetNexesPropertyImpl(params->ins()[0]->u.sval.str);
}

bool GetSrcProperty(void* obj, plugin::SrpcParams* params) {
  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  const char* url = plugin->nacl_module_url().c_str();
  PLUGIN_PRINTF(("GetSrcProperty ('src'='%s')\n", url));
  if (NACL_NO_URL != plugin->nacl_module_url()) {
    params->outs()[0]->u.sval.str = strdup(url);
    return true;
  } else {
    // No url to set 'src' to.
    return false;
  }
}

bool SetSrcProperty(void* obj, plugin::SrpcParams* params) {
  PLUGIN_PRINTF(("SetSrcProperty ()\n"));
  return reinterpret_cast<plugin::Plugin*>(obj)->
      SetSrcPropertyImpl(params->ins()[0]->u.sval.str);
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
  char* utf8string = params->ins()[0]->u.sval.str;
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

static int const kAbiHeaderBuffer = 256;  // must be at least EI_ABIVERSION + 1

void Plugin::LoadMethods() {
  PLUGIN_PRINTF(("Plugin::LoadMethods ()\n"));
  // Methods supported by Plugin.
  AddMethodCall(ShmFactory, "__shmFactory", "i", "h");
  AddMethodCall(DefaultSocketAddress, "__defaultSocketAddress", "", "h");
  AddMethodCall(NullPluginMethod, "__nullPluginMethod", "s", "i");
  AddMethodCall(SendAsyncMessage0, "__sendAsyncMessage0", "s", "");
  AddMethodCall(SendAsyncMessage1, "__sendAsyncMessage1", "sh", "");
  AddMethodCall(StartSrpcServicesWrapper, "__startSrpcServices", "", "");
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
    PLUGIN_PRINTF(("Plugin::SetNexesPropertyImpl (result='%s')\n",
                   result.c_str()));
    browser_interface()->AddToConsole(instance_id(), result);
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

// Firefox allows us to call NPN_GetUrlNotify during initialization, so if the
// "src" property has not been specified, we choose a path from the "nexes"
// list here and start downloading the right nexe immediately.
#if defined(NACL_STANDALONE)
  // If the <embed src='...'> attr was defined, the browser would have
  // implicitly called GET on it, which calls LoadNaClModule() and
  // set_nacl_module_url().
  // In the absence of this attr, we use the "nexes" attribute if present.
  if (NACL_NO_URL == nacl_module_url()) {
    const char* nexes_attr = LookupArgument("nexes");
    if (nexes_attr != NULL) {
      SetNexesPropertyImpl(nexes_attr);
    }
  }
#endif

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
    height_(0),
    width_(0),
    video_update_mode_(kVideoUpdatePluginPaint),
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

bool Plugin::LoadNaClModule(nacl::string full_url, nacl::string local_path) {
  return LoadNaClModule(full_url,
                        local_path,
                        static_cast<StreamShmBuffer*>(NULL),
                        NACL_NO_FILE_DESC);
}

bool Plugin::LoadNaClModule(nacl::string full_url, StreamShmBuffer* buffer) {
  return LoadNaClModule(full_url,
                        NACL_NO_FILE_PATH,
                        buffer,
                        NACL_NO_FILE_DESC);
}

bool Plugin::LoadNaClModule(nacl::string full_url, int file_desc) {
  return LoadNaClModule(full_url,
                        NACL_NO_FILE_PATH,
                        static_cast<StreamShmBuffer*>(NULL),
                        file_desc);
}

bool Plugin::LoadNaClModule(nacl::string full_url,
                            nacl::string local_path,
                            StreamShmBuffer* shm_buffer,
                            int file_desc) {
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (full_url='%s')\n",
                 full_url.c_str()));
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (local_path='%s')\n",
                 local_path.c_str()));
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (shm_buffer=%p)\n",
                 reinterpret_cast<void*>(shm_buffer)));
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (file_desc=%d)\n",
                 file_desc));

  CHECK(NACL_NO_URL != full_url);
  set_nacl_module_origin(nacl::UrlToOrigin(full_url));
  set_nacl_module_url(full_url);
  set_nacl_module_path(local_path);

  bool module_origin_valid = nacl::OriginIsInWhitelist(nacl_module_origin_);
  PLUGIN_PRINTF(("Plugin::LoadNaClModule "
                 "(page_origin='%s', page_origin_valid=%d)\n",
                 origin_.c_str(), origin_valid_));
  PLUGIN_PRINTF(("Plugin::LoadNaClModule "
                 "(nacl_origin='%s', nacl_origin_valid=%d)\n",
                 nacl_module_origin_.c_str(), module_origin_valid));

  // If the origin of the nacl module or of the page with <embed>/<object>
  // NaCl plugin element is not in the whitelist, refuse to load.
  // TODO(adonovan): JavaScript permits cross-origin loading, and so
  // does Chrome; why don't we?
  if (!origin_valid_ || !module_origin_valid) {
    nacl::string message = nacl::string("Load failed: NaCl module ") +
        nacl_module_url_ + " does not come from a whitelisted source. "
        "See native_client/src/trusted/plugin/origin.cc for the list.";
    browser_interface_->AddToConsole(instance_id(), message.c_str());
    return false;
  }

  // Catch any bad accesses while loading.
  ScopedCatchSignals sigcatcher(
      static_cast<ScopedCatchSignals::SigHandlerType>(SignalHandler));
  if (PLUGIN_SETJMP(g_LoaderEnv, 1)) {
    return false;
  }

  // Check ELF magic and ABI version compatibility.
  bool might_be_elf_exe = false;
  nacl::string error_string;
  if (NACL_NO_FILE_PATH != local_path) {
    CHECK(NULL == shm_buffer);
    CHECK(NACL_NO_FILE_DESC == file_desc);
    might_be_elf_exe = browser_interface_->MightBeElfExecutable(
        nacl_module_path_, &error_string);
  } else if (NULL != shm_buffer) {
    CHECK(NACL_NO_FILE_DESC == file_desc);
    // TODO(polina): add another MightBeElfExecutable that operates
    // on shared memory buffers.
    // It suffices to read out just the 1st header chunk.
    char elf_hdr_buf[kAbiHeaderBuffer];
    ssize_t result = shm_buffer->read(0, sizeof elf_hdr_buf, elf_hdr_buf);
    if (sizeof elf_hdr_buf == result) {  // (const char*)(elf_hdr_buf)
      might_be_elf_exe = browser_interface_->MightBeElfExecutable(
          elf_hdr_buf, sizeof elf_hdr_buf, &error_string);
    }
  } else if (file_desc > NACL_NO_FILE_DESC) {
    might_be_elf_exe =  browser_interface_->MightBeElfExecutable(
        file_desc, &error_string);
  } else {
    NACL_NOTREACHED();
  }
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (might_be_elf_exe=%d)\n",
                 might_be_elf_exe));
  if (!might_be_elf_exe) {
    browser_interface_->AddToConsole(instance_id(), error_string);
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
    const char* message = "Load: Failed to instantiate service runtime";
    browser_interface_->AddToConsole(instance_id(), message);
    return false;
  }

  bool service_runtime_started = false;
  if (NACL_NO_FILE_PATH != local_path) {
    service_runtime_started = service_runtime_->StartFromCommandLine(
        nacl_module_path_, static_cast<nacl::DescWrapper*>(NULL));
  } else if (NULL != shm_buffer) {
    int32_t size;
    NaClDesc* shm_nacl_desc = shm_buffer->shm(&size);
    PLUGIN_PRINTF(("Plugin::LoadNaClModule (shm_nacl_desc=%p)\n",
                   reinterpret_cast<void*>(shm_nacl_desc)));
    if (NULL == shm_nacl_desc) {
      return false;
    }
    nacl::DescWrapper* wrapped_shm_desc =
        wrapper_factory_->MakeGeneric(NaClDescRef(shm_nacl_desc));
    service_runtime_started = service_runtime_->StartFromBrowser(
        nacl_module_url_, wrapped_shm_desc);
    delete wrapped_shm_desc;
  } else if (file_desc > NACL_NO_FILE_DESC) {
    nacl::DescWrapper* wrapped_file_desc =
        wrapper_factory_->MakeFileDesc(file_desc, O_RDONLY);
    // TODO(polina): switch to StartFromBrowser when we fix:
    // http://code.google.com/p/nativeclient/issues/detail?id=934
    service_runtime_started = service_runtime_->StartFromCommandLine(
        NACL_NO_FILE_PATH, wrapped_file_desc);
    delete wrapped_file_desc;
  } else {
    NACL_NOTREACHED();
  }
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (service_runtime_started=%d)\n",
                 service_runtime_started));

  // Plugin takes ownership of socket_address_ from service_runtime_ and will
  // Unref it at deletion. This must be done here because service_runtime_
  // start-up might fail after default_socket_address() was already created.
  socket_address_ = service_runtime_->default_socket_address();
  if (!service_runtime_started) {
    const char* message = "Load: Failed to start service runtime";
    browser_interface_->AddToConsole(instance_id(), message);
    return false;
  }
  CHECK(NULL != socket_address_);
  if (!StartSrpcServices(&error_string)) {  // sets socket_
    browser_interface_->AddToConsole(instance_id(), error_string);
    return false;
  }
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (socket_address=%p, socket=%p)\n",
                 static_cast<void*>(socket_address_),
                 static_cast<void*>(socket_)));
  return true;
}

bool Plugin::StartSrpcServices(nacl::string* error) {
  UnrefScriptableHandle(&socket_);
  socket_ = socket_address_->handle()->Connect();
  if (socket_ == NULL) {
    *error = "Load: Failed to connect using SRPC";
    return false;
  }
  socket_->handle()->StartJSObjectProxy(this);
  // Create the listener thread and initialize the nacl module.
  if (!InitializeModuleMultimedia(socket_, service_runtime_)) {
    *error = "Load: Failed to initialise multimedia";
    return false;
  }
  PLUGIN_PRINTF(("Plugin::Load (established socket %p)\n",
                 static_cast<void*>(socket_)));
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
