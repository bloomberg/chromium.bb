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
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/connected_socket.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/string_encoding.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace plugin {

namespace {

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

bool GetReadyStateProperty(void* obj, SrpcParams* params) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  params->outs()[0]->u.ival = plugin->nacl_ready_state();
  return true;
}

bool LaunchExecutableFromFd(void* obj, SrpcParams* params) {
  PLUGIN_PRINTF(("LaunchExecutableFromFd ()\n"));
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  NaClDescRef(params->ins()[0]->u.hval);
  nacl::scoped_ptr<nacl::DescWrapper>
      wrapper(plugin->wrapper_factory()->MakeGeneric(params->ins()[0]->u.hval));
  plugin->set_nacl_ready_state(Plugin::LOADING);
  // Generate the event stream for loading a module.
  plugin->EnqueueProgressEvent("loadstart",
                               Plugin::LENGTH_IS_NOT_COMPUTABLE,
                               Plugin::kUnknownBytes,
                               Plugin::kUnknownBytes);
  ErrorInfo error_info;
  bool was_successful = plugin->LoadNaClModule(wrapper.get(), &error_info);
  // Set the readyState attribute to indicate ready to start.
  if (was_successful) {
    plugin->ReportLoadSuccess(Plugin::LENGTH_IS_NOT_COMPUTABLE,
                              Plugin::kUnknownBytes,
                              Plugin::kUnknownBytes);
  } else {
    // For reasons unknown, the message is garbled on windows.
    // TODO(sehr): know the reasons, and fix this.
    error_info.PrependMessage("__launchExecutableFromFd failed: ");
    plugin->ReportLoadError(error_info);
  }
  return was_successful;
}

}  // namespace

// TODO(mseaborn): Although this will usually not block, it will
// block if the socket's buffer fills up.
bool Plugin::SendAsyncMessage(void* obj, SrpcParams* params,
                              nacl::DescWrapper** fds, int fds_count) {
  Plugin* plugin = reinterpret_cast<Plugin*>(obj);
  if (plugin->main_service_runtime() == NULL) {
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
  nacl::DescWrapper* socket =
      plugin->main_service_runtime()->async_send_desc();
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
  ErrorInfo error_info;
  if (!plugin->StartSrpcServices(&plugin->main_subprocess_, &error_info)) {
    params->set_exception_string(error_info.message().c_str());
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
  AddPropertyGet(GetReadyStateProperty, "readyState", "i");

  if (!ExperimentalJavaScriptApisAreEnabled()) {
    return;
  }
  // Experimental methods supported by Plugin.
  // These methods are explicitly not included in shipping versions of Chrome.
  AddMethodCall(GetSandboxISAProperty, "__getSandboxISA", "", "s");
  AddMethodCall(LaunchExecutableFromFd, "__launchExecutableFromFd", "h", "");
  AddMethodCall(NullPluginMethod, "__nullPluginMethod", "s", "i");
  AddMethodCall(SendAsyncMessage0, "__sendAsyncMessage0", "s", "");
  AddMethodCall(SendAsyncMessage1, "__sendAsyncMessage1", "sh", "");
  AddMethodCall(StartSrpcServicesWrapper, "__startSrpcServices", "", "");
}

bool Plugin::HasMethodEx(uintptr_t method_id, CallType call_type) {
  if (!ExperimentalJavaScriptApisAreEnabled()) {
    return false;
  }
  return main_subprocess_.HasMethod(method_id, call_type);
}

bool Plugin::InvokeEx(uintptr_t method_id,
                      CallType call_type,
                      SrpcParams* params) {
  if (!ExperimentalJavaScriptApisAreEnabled()) {
    return false;
  }
  return main_subprocess_.Invoke(method_id, call_type, params);
}

bool Plugin::InitParamsEx(uintptr_t method_id,
                          CallType call_type,
                          SrpcParams* params) {
  if (!ExperimentalJavaScriptApisAreEnabled()) {
    return false;
  }
  return main_subprocess_.InitParams(method_id, call_type, params);
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
  for (int i = 0; i < argc; ++i) {
    if (NULL != argn_ && NULL != argv_) {
      argn_[argc_] = strdup(argn[i]);
      argv_[argc_] = strdup(argv[i]);
      if (NULL == argn_[argc_] || NULL == argv_[argc_]) {
        // Give up on passing arguments.
        free(argn_[argc_]);
        free(argv_[argc_]);
        continue;
      }
      ++argc_;
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

  // Set up the scriptable methods for the plugin.
  LoadMethods();

  PLUGIN_PRINTF(("Plugin::Init (return 1)\n"));
  // Return success.
  return true;
}

Plugin::Plugin()
  : receive_thread_running_(false),
    browser_interface_(NULL),
    scriptable_handle_(NULL),
    argc_(-1),
    argn_(NULL),
    argv_(NULL),
    main_subprocess_(kMainSubprocessId, NULL, NULL),
    nacl_ready_state_(UNSENT),
    wrapper_factory_(NULL) {
  PLUGIN_PRINTF(("Plugin::Plugin (this=%p)\n", static_cast<void*>(this)));
}

// TODO(polina): move this to PluginNpapi.
void Plugin::Invalidate() {
  PLUGIN_PRINTF(("Plugin::Invalidate (this=%p)\n", static_cast<void*>(this)));
  main_subprocess_.set_socket(NULL);
  // Other subprocesses (helper subprocesses) are not Invalidate()'ed.
}

void Plugin::ShutDownSubprocesses() {
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocesses (this=%p)\n",
                 static_cast<void*>(this)));
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocesses %s\n",
                 main_subprocess_.description().c_str()));

  // Shutdown service runtime. This must be done before all other calls so
  // they don't block forever when waiting for the upcall thread to exit.
  main_subprocess_.Shutdown();
  for (size_t i = 0; i < nacl_subprocesses_.size(); ++i) {
    PLUGIN_PRINTF(("Plugin::ShutDownSubprocesses %s\n",
                   nacl_subprocesses_[i]->description().c_str()));
    delete nacl_subprocesses_[i];
  }
  nacl_subprocesses_.clear();

  if (receive_thread_running_) {
    NaClThreadJoin(&receive_thread_);
    receive_thread_running_ = false;
  }

  PLUGIN_PRINTF(("Plugin::ShutDownSubprocess (this=%p, return)\n",
                 static_cast<void*>(this)));
}


Plugin::~Plugin() {
  PLUGIN_PRINTF(("Plugin::~Plugin (this=%p)\n", static_cast<void*>(this)));

  // After invalidation, the browser does not respect reference counting,
  // so we shut down here what we can and prevent attempts to shut down
  // other linked structures in Deallocate.

  // TODO(sehr,polina): We should not need to call ShutDownSubprocesses() here.
  ShutDownSubprocesses();

  delete wrapper_factory_;
  delete browser_interface_;
  delete[] argv_;
  delete[] argn_;
  PLUGIN_PRINTF(("Plugin::~Plugin (this=%p, return)\n",
                 static_cast<void*>(this)));
}

bool Plugin::LoadNaClModuleCommon(nacl::DescWrapper* wrapper,
                                  NaClSubprocess* subprocess,
                                  ErrorInfo* error_info) {
  ServiceRuntime* new_service_runtime = new(std::nothrow) ServiceRuntime(this);
  subprocess->set_service_runtime(new_service_runtime);
  PLUGIN_PRINTF(("Plugin::LoadNaClModuleCommon (service_runtime=%p)\n",
                 static_cast<void*>(new_service_runtime)));
  if (NULL == new_service_runtime) {
    error_info->SetReport(ERROR_SEL_LDR_INIT,
                          "sel_ldr init failure " + subprocess->description());
    return false;
  }

  bool service_runtime_started =
      new_service_runtime->Start(wrapper, error_info);
  PLUGIN_PRINTF(("Plugin::LoadNaClModuleCommon (service_runtime_started=%d)\n",
                 service_runtime_started));
  if (!service_runtime_started) {
    return false;
  }
  return true;
}

bool Plugin::StartSrpcServicesCommon(NaClSubprocess* subprocess,
                                     ErrorInfo* error_info) {
  if (!subprocess->StartSrpcServices()) {
    error_info->SetReport(ERROR_SRPC_CONNECTION_FAIL,
                          "SRPC connection failure for " +
                          subprocess->description());
    return false;
  }
  PLUGIN_PRINTF(("Plugin::StartSrpcServicesCommon (established socket %p)\n",
                 static_cast<void*>(subprocess->socket())));
  return true;
}

bool Plugin::StartSrpcServices(NaClSubprocess* subprocess,
                               ErrorInfo* error_info) {
  if (!StartSrpcServicesCommon(subprocess, error_info)) {
    return false;
  }
  // TODO(jvoung): This next bit is likely not needed...
  // If StartSrpcServices is only in the JS API that is just for SRPC nexes
  // (namely __startSrpcServices), then attempts to start the JS proxy
  // will fail anyway?
  // If that is the case, by removing the following line,
  // the StartSrpcServices == StartSrpcServicesCommon.
  // We still need this function though, to launch helper SRPC nexes within
  // the plugin.
  return StartJSObjectProxy(subprocess, error_info);
}

bool Plugin::StartJSObjectProxy(NaClSubprocess* subprocess,
                                ErrorInfo* error_info) {
  if (!subprocess->StartJSObjectProxy(this, error_info)) {
    // TODO(sehr,polina): rename the below to ExperimentalSRPCApisAreEnabled.
    if (ExperimentalJavaScriptApisAreEnabled()) {
      // It is not an error for the proxy to fail to start if experimental
      // APIs are enabled.  This means we have an SRPC nexe.
      error_info->Reset();
    } else {
      return false;
    }
  }
  return true;
}

bool Plugin::LoadNaClModule(nacl::DescWrapper* wrapper,
                            ErrorInfo* error_info) {
  // Before forking a new sel_ldr process, ensure that we do not leak
  // the ServiceRuntime object for an existing subprocess, and that any
  // associated listener threads do not go unjoined because if they
  // outlive the Plugin object, they will not be memory safe.
  ShutDownSubprocesses();
  if (!(LoadNaClModuleCommon(wrapper, &main_subprocess_, error_info)
        && StartSrpcServicesCommon(&main_subprocess_, error_info)
        && StartJSObjectProxy(&main_subprocess_, error_info))) {
    return false;
  }
  PLUGIN_PRINTF(("Plugin::LoadNaClModule finished %s\n",
                 main_subprocess_.description().c_str()));
  return true;
}

NaClSubprocessId Plugin::LoadHelperNaClModule(nacl::DescWrapper* wrapper,
                                              ErrorInfo* error_info) {
  NaClSubprocessId next_id = next_nacl_subprocess_id();
  nacl::scoped_ptr<NaClSubprocess> nacl_subprocess(
      new(std::nothrow) NaClSubprocess(next_id, NULL, NULL));
  if (NULL == nacl_subprocess.get()) {
    error_info->SetReport(ERROR_SEL_LDR_INIT,
                          "unable to allocate helper subprocess.");
    return kInvalidNaClSubprocessId;
  }

  if (!(LoadNaClModuleCommon(wrapper, nacl_subprocess.get(), error_info)
        && StartSrpcServicesCommon(nacl_subprocess.get(), error_info))) {
    return kInvalidNaClSubprocessId;
  }

  PLUGIN_PRINTF(("Plugin::LoadHelperNaClModule finished %s\n",
                 nacl_subprocess.get()->description().c_str()));

  nacl_subprocesses_.push_back(nacl_subprocess.release());
  return next_id;
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
