// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef _MSC_VER
// Do not warn about use of std::copy with raw pointers.
#pragma warning(disable : 4996)
#endif

#include "native_client/src/trusted/plugin/plugin.h"

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <deque>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/manifest.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/string_encoding.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

#include "ppapi/c/dev/ppp_find_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/dev/ppp_scrollbar_dev.h"
#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/c/dev/ppp_widget_dev.h"
#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/c/private/ppb_uma_private.h"
#include "ppapi/cpp/dev/find_dev.h"
#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/dev/scrollbar_dev.h"
#include "ppapi/cpp/dev/selection_dev.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/dev/widget_client_dev.h"
#include "ppapi/cpp/dev/zoom_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/mouse_lock.h"
#include "ppapi/cpp/rect.h"

using ppapi_proxy::BrowserPpp;

namespace plugin {

namespace {

bool GetReadyStateProperty(void* obj, SrpcParams* params) {
  Plugin* plugin = static_cast<Plugin*>(obj);
  params->outs()[0]->u.ival = plugin->nacl_ready_state();
  return true;
}

const char* const kTypeAttribute = "type";
// The "src" attribute of the <embed> tag.  The value is expected to be either
// a URL or URI pointing to the manifest file (which is expected to contain
// JSON matching ISAs with .nexe URLs).
const char* const kSrcManifestAttribute = "src";
// The "nacl" attribute of the <embed> tag.  We use the value of this attribute
// to find the manifest file when NaCl is registered as a plug-in for another
// MIME type because the "src" attribute is used to supply us with the resource
// of that MIME type that we're supposed to display.
const char* const kNaClManifestAttribute = "nacl";
// This is a pretty arbitrary limit on the byte size of the NaCl manfest file.
// Note that the resulting string object has to have at least one byte extra
// for the null termination character.
const size_t kNaClManifestMaxFileBytes = 1024 * 1024;

// Define an argument name to enable 'dev' interfaces. To make sure it doesn't
// collide with any user-defined HTML attribute, make the first character '@'.
const char* const kDevAttribute = "@dev";

// URL schemes that we treat in special ways.
const char* const kChromeExtensionUriScheme = "chrome-extension";
const char* const kDataUriScheme = "data";

// The key used to find the dictionary nexe URLs in the manifest file.
const char* const kNexesKey = "nexes";

bool GetLastError(void* obj, SrpcParams* params) {
  NaClSrpcArg** outs = params->outs();
  PLUGIN_PRINTF(("GetLastError (obj=%p)\n", obj));

  Plugin* plugin = static_cast<Plugin*>(obj);
  outs[0]->arrays.str = strdup(plugin->last_error_string().c_str());
  return true;
}

bool GetExitStatus(void* obj, SrpcParams* params) {
  NaClSrpcArg** outs = params->outs();
  PLUGIN_PRINTF(("GetExitStatus (obj=%p)\n", obj));

  Plugin* plugin = static_cast<Plugin*>(obj);
  outs[0]->u.ival = plugin->exit_status();
  return true;
}

// Up to 20 seconds
const int64_t kTimeSmallMin = 1;         // in ms
const int64_t kTimeSmallMax = 20000;     // in ms
const uint32_t kTimeSmallBuckets = 100;

// Up to 3 minutes, 20 seconds
const int64_t kTimeMediumMin = 10;         // in ms
const int64_t kTimeMediumMax = 200000;     // in ms
const uint32_t kTimeMediumBuckets = 100;

// Up to 33 minutes.
const int64_t kTimeLargeMin = 100;         // in ms
const int64_t kTimeLargeMax = 2000000;     // in ms
const uint32_t kTimeLargeBuckets = 100;

const int64_t kSizeKBMin = 1;
const int64_t kSizeKBMax = 512*1024;     // very large .nexe
const uint32_t kSizeKBBuckets = 100;

const PPB_UMA_Private* GetUMAInterface() {
  pp::Module *module = pp::Module::Get();
  CHECK(module);
  return static_cast<const PPB_UMA_Private*>(
      module->GetBrowserInterface(PPB_UMA_PRIVATE_INTERFACE));
}

void HistogramTimeSmall(const std::string& name, int64_t ms) {
  if (ms < 0) return;

  const PPB_UMA_Private* ptr = GetUMAInterface();
  if (ptr == NULL) return;

  ptr->HistogramCustomTimes(pp::Var(name).pp_var(),
                            ms,
                            kTimeSmallMin, kTimeSmallMax,
                            kTimeSmallBuckets);
}

void HistogramTimeMedium(const std::string& name, int64_t ms) {
  if (ms < 0) return;

  const PPB_UMA_Private* ptr = GetUMAInterface();
  if (ptr == NULL) return;

  ptr->HistogramCustomTimes(pp::Var(name).pp_var(),
                            ms,
                            kTimeMediumMin, kTimeMediumMax,
                            kTimeMediumBuckets);
}

void HistogramTimeLarge(const std::string& name, int64_t ms) {
  if (ms < 0) return;

  const PPB_UMA_Private* ptr = GetUMAInterface();
  if (ptr == NULL) return;

  ptr->HistogramCustomTimes(pp::Var(name).pp_var(),
                            ms,
                            kTimeLargeMin, kTimeLargeMax,
                            kTimeLargeBuckets);
}

void HistogramSizeKB(const std::string& name, int32_t sample) {
  if (sample < 0) return;

  const PPB_UMA_Private* ptr = GetUMAInterface();
  if (ptr == NULL) return;

  ptr->HistogramCustomCounts(pp::Var(name).pp_var(),
                             sample,
                             kSizeKBMin, kSizeKBMax,
                             kSizeKBBuckets);
}

void HistogramEnumerate(const std::string& name, int sample, int maximum,
                        int out_of_range_replacement) {
  if (sample < 0 || sample >= maximum) {
    if (out_of_range_replacement < 0)
      // No replacement for bad input, abort.
      return;
    else
      // Use a specific value to signal a bad input.
      sample = out_of_range_replacement;
  }
  const PPB_UMA_Private* ptr = GetUMAInterface();
  if (ptr == NULL) return;
  ptr->HistogramEnumeration(pp::Var(name).pp_var(), sample, maximum);
}

void HistogramEnumerateOsArch(const std::string& sandbox_isa) {
  enum NaClOSArch {
    kNaClLinux32 = 0,
    kNaClLinux64,
    kNaClLinuxArm,
    kNaClMac32,
    kNaClMac64,
    kNaClMacArm,
    kNaClWin32,
    kNaClWin64,
    kNaClWinArm,
    kNaClOSArchMax
  };

  NaClOSArch os_arch = kNaClOSArchMax;
#if NACL_LINUX
  os_arch = kNaClLinux32;
#elif NACL_OSX
  os_arch = kNaClMac32;
#elif NACL_WINDOWS
  os_arch = kNaClWin32;
#endif

  if (sandbox_isa == "x86-64")
    os_arch = static_cast<NaClOSArch>(os_arch + 1);
  if (sandbox_isa == "arm")
    os_arch = static_cast<NaClOSArch>(os_arch + 2);

  HistogramEnumerate("NaCl.Client.OSArch", os_arch, kNaClOSArchMax, -1);
}

void HistogramEnumerateLoadStatus(PluginErrorCode error_code) {
  HistogramEnumerate("NaCl.LoadStatus.Plugin", error_code, ERROR_MAX,
                     ERROR_UNKNOWN);
}

void HistogramEnumerateSelLdrLoadStatus(NaClErrorCode error_code) {
  HistogramEnumerate("NaCl.LoadStatus.SelLdr", error_code, NACL_ERROR_CODE_MAX,
                     LOAD_STATUS_UNKNOWN);
}

void HistogramEnumerateManifestIsDataURI(bool is_data_uri) {
  HistogramEnumerate("NaCl.Manifest.IsDataURI", is_data_uri, 2, -1);
}

// Derive a class from pp::Find_Dev to forward PPP_Find_Dev calls to
// the plugin.
class FindAdapter : public pp::Find_Dev {
 public:
  explicit FindAdapter(Plugin* plugin)
    : pp::Find_Dev(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_find_ = static_cast<const PPP_Find_Dev*>(
        proxy->GetPluginInterface(PPP_FIND_DEV_INTERFACE));
  }

  bool StartFind(const std::string& text, bool case_sensitive) {
    if (ppp_find_ != NULL) {
      PP_Bool pp_success =
          ppp_find_->StartFind(plugin_->pp_instance(),
                               text.c_str(),
                               PP_FromBool(case_sensitive));
      return pp_success == PP_TRUE;
    }
    return false;
  }

  void SelectFindResult(bool forward) {
    if (ppp_find_ != NULL) {
      ppp_find_->SelectFindResult(plugin_->pp_instance(),
                                  PP_FromBool(forward));
    }
  }

  void StopFind() {
    if (ppp_find_ != NULL)
      ppp_find_->StopFind(plugin_->pp_instance());
  }

 private:
  Plugin* plugin_;
  const PPP_Find_Dev* ppp_find_;

  NACL_DISALLOW_COPY_AND_ASSIGN(FindAdapter);
};


// Derive a class from pp::MouseLock to forward PPP_MouseLock calls to
// the plugin.
class MouseLockAdapter : public pp::MouseLock {
 public:
  explicit MouseLockAdapter(Plugin* plugin)
    : pp::MouseLock(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_mouse_lock_ = static_cast<const PPP_MouseLock*>(
        proxy->GetPluginInterface(PPP_MOUSELOCK_INTERFACE));
  }

  void MouseLockLost() {
    if (ppp_mouse_lock_ != NULL)
      ppp_mouse_lock_->MouseLockLost(plugin_->pp_instance());
  }

 private:
  Plugin* plugin_;
  const PPP_MouseLock* ppp_mouse_lock_;

  NACL_DISALLOW_COPY_AND_ASSIGN(MouseLockAdapter);
};


// Derive a class from pp::Printing_Dev to forward PPP_Printing_Dev calls to
// the plugin.
class PrintingAdapter : public pp::Printing_Dev {
 public:
  explicit PrintingAdapter(Plugin* plugin)
    : pp::Printing_Dev(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_printing_ = static_cast<const PPP_Printing_Dev*>(
        proxy->GetPluginInterface(PPP_PRINTING_DEV_INTERFACE));
  }

  uint32_t QuerySupportedPrintOutputFormats() {
    if (ppp_printing_ != NULL) {
      return ppp_printing_->QuerySupportedFormats(plugin_->pp_instance());
    }
    return 0;
  }

  int32_t PrintBegin(const PP_PrintSettings_Dev& print_settings) {
    if (ppp_printing_ != NULL) {
      return ppp_printing_->Begin(plugin_->pp_instance(), &print_settings);
    }
    return 0;
  }

  pp::Resource PrintPages(const PP_PrintPageNumberRange_Dev* page_ranges,
                          uint32_t page_range_count) {
    if (ppp_printing_ != NULL) {
      PP_Resource image_data = ppp_printing_->PrintPages(plugin_->pp_instance(),
                                                         page_ranges,
                                                         page_range_count);
      return pp::ImageData(pp::ImageData::PassRef(), image_data);
    }
    return pp::Resource();
  }

  void PrintEnd() {
    if (ppp_printing_ != NULL)
      ppp_printing_->End(plugin_->pp_instance());
  }

  bool IsPrintScalingDisabled() {
    if (ppp_printing_ != NULL) {
      PP_Bool result = ppp_printing_->IsScalingDisabled(plugin_->pp_instance());
      return result == PP_TRUE;
    }
    return false;
  }

 private:
  Plugin* plugin_;
  const PPP_Printing_Dev* ppp_printing_;

  NACL_DISALLOW_COPY_AND_ASSIGN(PrintingAdapter);
};


// Derive a class from pp::Selection_Dev to forward PPP_Selection_Dev calls to
// the plugin.
class SelectionAdapter : public pp::Selection_Dev {
 public:
  explicit SelectionAdapter(Plugin* plugin)
    : pp::Selection_Dev(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_selection_ = static_cast<const PPP_Selection_Dev*>(
        proxy->GetPluginInterface(PPP_SELECTION_DEV_INTERFACE));
  }

  pp::Var GetSelectedText(bool html) {
    if (ppp_selection_ != NULL) {
      PP_Var var = ppp_selection_->GetSelectedText(plugin_->pp_instance(),
                                                   PP_FromBool(html));
      return pp::Var(pp::Var::PassRef(), var);
    }
    return pp::Var();
  }

 private:
  Plugin* plugin_;
  const PPP_Selection_Dev* ppp_selection_;

  NACL_DISALLOW_COPY_AND_ASSIGN(SelectionAdapter);
};


// Derive a class from pp::WidgetClient_Dev to forward PPP_Widget_Dev
// and PPP_Scrollbar_Dev calls to the plugin.
class WidgetClientAdapter : public pp::WidgetClient_Dev {
 public:
  explicit WidgetClientAdapter(Plugin* plugin)
    : pp::WidgetClient_Dev(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_widget_ = static_cast<const PPP_Widget_Dev*>(
        proxy->GetPluginInterface(PPP_WIDGET_DEV_INTERFACE));
    ppp_scrollbar_ = static_cast<const PPP_Scrollbar_Dev*>(
        proxy->GetPluginInterface(PPP_SCROLLBAR_DEV_INTERFACE));
  }

  void InvalidateWidget(pp::Widget_Dev widget, const pp::Rect& dirty_rect) {
    if (ppp_widget_ != NULL) {
      ppp_widget_->Invalidate(plugin_->pp_instance(),
                              widget.pp_resource(),
                              &dirty_rect.pp_rect());
    }
  }

  void ScrollbarValueChanged(pp::Scrollbar_Dev scrollbar, uint32_t value) {
    if (ppp_scrollbar_ != NULL) {
      ppp_scrollbar_->ValueChanged(plugin_->pp_instance(),
                                   scrollbar.pp_resource(),
                                   value);
    }
  }

  void ScrollbarOverlayChanged(pp::Scrollbar_Dev scrollbar, bool overlay) {
    if (ppp_scrollbar_ != NULL) {
      ppp_scrollbar_->OverlayChanged(plugin_->pp_instance(),
                                     scrollbar.pp_resource(),
                                     PP_FromBool(overlay));
    }
  }

 private:
  Plugin* plugin_;
  const PPP_Widget_Dev* ppp_widget_;
  const PPP_Scrollbar_Dev* ppp_scrollbar_;

  NACL_DISALLOW_COPY_AND_ASSIGN(WidgetClientAdapter);
};


// Derive a class from pp::Zoom_Dev to forward PPP_Zoom_Dev calls to
// the plugin.
class ZoomAdapter : public pp::Zoom_Dev {
 public:
  explicit ZoomAdapter(Plugin* plugin)
    : pp::Zoom_Dev(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_zoom_ = static_cast<const PPP_Zoom_Dev*>(
        proxy->GetPluginInterface(PPP_ZOOM_DEV_INTERFACE));
  }

  void Zoom(double factor, bool text_only) {
    if (ppp_zoom_ != NULL) {
      ppp_zoom_->Zoom(plugin_->pp_instance(),
                      factor,
                      PP_FromBool(text_only));
    }
  }

 private:
  Plugin* plugin_;
  const PPP_Zoom_Dev* ppp_zoom_;

  NACL_DISALLOW_COPY_AND_ASSIGN(ZoomAdapter);
};

}  // namespace

bool Plugin::ExperimentalJavaScriptApisAreEnabled() {
  return getenv("NACL_ENABLE_EXPERIMENTAL_JAVASCRIPT_APIS") != NULL;
}

static int const kAbiHeaderBuffer = 256;  // must be at least EI_ABIVERSION + 1

void Plugin::LoadMethods() {
  PLUGIN_PRINTF(("Plugin::LoadMethods ()\n"));
  // Properties implemented by Plugin.
  AddPropertyGet(GetReadyStateProperty, "readyState", "i");
}

bool Plugin::HasMethod(uintptr_t method_id, CallType call_type) {
  PLUGIN_PRINTF(("Plugin::HasMethod (method_id=%x)\n",
                 static_cast<int>(method_id)));
  if (GetMethodInfo(method_id, call_type)) {
    PLUGIN_PRINTF(("true\n"));
    return true;
  }
  if (!ExperimentalJavaScriptApisAreEnabled()) {
    PLUGIN_PRINTF(("false\n"));
    return false;
  }
  if (call_type != METHOD_CALL) {
    // SRPC nexes can only export methods.
    PLUGIN_PRINTF(("false\n"));
    return false;
  }
  bool has_method = main_subprocess_.HasMethod(method_id);
  PLUGIN_PRINTF(("%s\n", (has_method ? "true" : "false")));
  return has_method;
}

bool Plugin::InitParams(uintptr_t method_id,
                        CallType call_type,
                        SrpcParams* params) {
  MethodInfo* method_info = GetMethodInfo(method_id, call_type);
  PLUGIN_PRINTF(("Plugin::InitParams (id=%"NACL_PRIxPTR", method_info=%p)\n",
                 method_id, method_info));
  if (NULL != method_info) {
    return params->Init(method_info->ins(), method_info->outs());
  }
  if (!ExperimentalJavaScriptApisAreEnabled()) {
    return false;
  }
  if (call_type != METHOD_CALL) {
    // SRPC nexes can only export methods.
    return false;
  }
  return main_subprocess_.InitParams(method_id, params);
}

bool Plugin::Invoke(uintptr_t method_id,
                    CallType call_type,
                    SrpcParams* params) {
  MethodInfo* method_info = GetMethodInfo(method_id, call_type);

  if (NULL != method_info && NULL != method_info->function_ptr()) {
    return method_info->function_ptr()(static_cast<void*>(this), params);
  }
  if (!ExperimentalJavaScriptApisAreEnabled()) {
    return false;
  }
  if (call_type != METHOD_CALL) {
    // SRPC nexes can only export methods.
    return false;
  }
  return main_subprocess_.Invoke(method_id, params);
}

bool Plugin::Init(BrowserInterface* browser_interface,
                  int argc,
                  char* argn[],
                  char* argv[]) {
  PLUGIN_PRINTF(("Plugin::Init (instance=%p)\n", static_cast<void*>(this)));

  browser_interface_ = browser_interface;
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
                 static_cast<void*>(wrapper_factory_)));

  // Set up the scriptable methods for the plugin.
  LoadMethods();

  PLUGIN_PRINTF(("Plugin::Init (return 1)\n"));
  // Return success.
  return true;
}

void Plugin::ShutDownSubprocesses() {
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocesses (this=%p)\n",
                 static_cast<void*>(this)));
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocesses (%s)\n",
                 main_subprocess_.detailed_description().c_str()));

  // Shutdown service runtime. This must be done before all other calls so
  // they don't block forever when waiting for the upcall thread to exit.
  main_subprocess_.Shutdown();
  for (size_t i = 0; i < nacl_subprocesses_.size(); ++i) {
    PLUGIN_PRINTF(("Plugin::ShutDownSubprocesses (%s)\n",
                   nacl_subprocesses_[i]->detailed_description().c_str()));
    delete nacl_subprocesses_[i];
  }
  nacl_subprocesses_.clear();

  PLUGIN_PRINTF(("Plugin::ShutDownSubprocess (this=%p, return)\n",
                 static_cast<void*>(this)));
}

bool Plugin::LoadNaClModuleCommon(nacl::DescWrapper* wrapper,
                                  NaClSubprocess* subprocess,
                                  ErrorInfo* error_info,
                                  pp::CompletionCallback init_done_cb,
                                  pp::CompletionCallback crash_cb) {
  ServiceRuntime* new_service_runtime = new(std::nothrow) ServiceRuntime(
      this,
      init_done_cb,
      crash_cb);
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
  PLUGIN_PRINTF(("Plugin::StartSrpcServicesCommon (established srpc_client "
                 "%p)\n",
                 static_cast<void*>(subprocess->srpc_client())));
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
    // TODO(sehr,polina): rename the function and env var
    // to ExperimentalJavaScriptApisAreEnabled.
    if (error_info->error_code() == ERROR_START_PROXY_CHECK_PPP &&
        ExperimentalJavaScriptApisAreEnabled()) {
      // It is not an error for the proxy to fail to find PPP methods if
      // experimental APIs are enabled. This means we have an SRPC nexe.
      error_info->Reset();
    } else {
      return false;
    }
  }
  return true;
}

bool Plugin::LoadNaClModule(nacl::DescWrapper* wrapper,
                            ErrorInfo* error_info,
                            pp::CompletionCallback init_done_cb,
                            pp::CompletionCallback crash_cb) {
  // Before forking a new sel_ldr process, ensure that we do not leak
  // the ServiceRuntime object for an existing subprocess, and that any
  // associated listener threads do not go unjoined because if they
  // outlive the Plugin object, they will not be memory safe.
  ShutDownSubprocesses();
  if (!(LoadNaClModuleCommon(wrapper, &main_subprocess_, error_info,
                             init_done_cb, crash_cb))) {
    return false;
  }
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (%s)\n",
                 main_subprocess_.detailed_description().c_str()));
  return true;
}

bool Plugin::LoadNaClModuleContinuationIntern(ErrorInfo* error_info) {
  if (!(StartSrpcServicesCommon(&main_subprocess_, error_info)
        && StartJSObjectProxy(&main_subprocess_, error_info))) {
    return false;
  }
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (%s)\n",
                 main_subprocess_.detailed_description().c_str()));
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

  if (!(LoadNaClModuleCommon(wrapper, nacl_subprocess.get(), error_info,
                             pp::BlockUntilComplete(),
                             pp::BlockUntilComplete())
        // We need not wait for the init_done callback.  We can block
        // here in StartSrpcServicesCommon, since helper NaCl modules
        // are spawned from a private thread.
        //
        // TODO(bsy): if helper module crashes, we should abort.
        // crash_cb is not used here, so we are relying on crashes
        // being detected in StartSrpcServicesCommon or later.
        //
        // NB: More refactoring might be needed, however, if helper
        // NaCl modules have their own manifest.  Currently the
        // manifest is a per-plugin-instance object, not a per
        // NaClSubprocess object.
        && StartSrpcServicesCommon(nacl_subprocess.get(), error_info))) {
    return kInvalidNaClSubprocessId;
  }

  PLUGIN_PRINTF(("Plugin::LoadHelperNaClModule (%s)\n",
                 nacl_subprocess.get()->detailed_description().c_str()));

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

void Plugin::AddPropertyGet(RpcFunction function_ptr,
                            const char* name,
                            const char* outs) {
  uintptr_t method_id = browser_interface()->StringToIdentifier(name);
  PLUGIN_PRINTF(("Plugin::AddPropertyGet (name='%s', id=%"
                 NACL_PRIxPTR")\n", name, method_id));
  MethodInfo* new_method = new MethodInfo(function_ptr, name, "", outs);
  property_get_methods_.AddMethod(method_id, new_method);
}

MethodInfo* Plugin::GetMethodInfo(uintptr_t method_id, CallType call_type) {
  MethodInfo* method_info = NULL;
  switch (call_type) {
    case PROPERTY_GET:
      method_info = property_get_methods_.GetMethod(method_id);
      break;
    case PROPERTY_SET:
    case METHOD_CALL:
      break;
  }
  PLUGIN_PRINTF(("Plugin::GetMethodInfo (id=%"NACL_PRIxPTR", "
                 "return %p)\n", method_id, static_cast<void*>(method_info)));
  return method_info;
}

// Suggested names for progress event types, per
// http://www.w3.org/TR/progress-events/
const char* const Plugin::kProgressEventLoadStart = "loadstart";
const char* const Plugin::kProgressEventProgress =  "progress";
const char* const Plugin::kProgressEventError =     "error";
const char* const Plugin::kProgressEventAbort =     "abort";
const char* const Plugin::kProgressEventLoad =      "load";
const char* const Plugin::kProgressEventLoadEnd =   "loadend";
// Define a NaCl specific event type for .nexe crashes.
const char* const Plugin::kProgressEventCrash =     "crash";

class ProgressEvent {
 public:
  ProgressEvent(const char* event_type,
                const nacl::string& url,
                Plugin::LengthComputable length_computable,
                uint64_t loaded_bytes,
                uint64_t total_bytes) :
    event_type_(event_type),
    url_(url),
    length_computable_(length_computable),
    loaded_bytes_(loaded_bytes),
    total_bytes_(total_bytes) { }
  const char* event_type() const { return event_type_; }
  const char* url() const { return url_.c_str(); }
  Plugin::LengthComputable length_computable() const {
    return length_computable_;
  }
  uint64_t loaded_bytes() const { return loaded_bytes_; }
  uint64_t total_bytes() const { return total_bytes_; }

 private:
  // event_type_ is always passed from a string literal, so ownership is
  // not taken.  Hence it does not need to be deleted when ProgressEvent is
  // destroyed.
  const char* event_type_;
  nacl::string url_;
  Plugin::LengthComputable length_computable_;
  uint64_t loaded_bytes_;
  uint64_t total_bytes_;
};

const char* const Plugin::kNaClMIMEType = "application/x-nacl";

bool Plugin::IsForeignMIMEType() const {
  return
      !mime_type().empty() &&
      mime_type() != kNaClMIMEType;
}


Plugin* Plugin::New(PP_Instance pp_instance) {
  PLUGIN_PRINTF(("Plugin::New (pp_instance=%"NACL_PRId32")\n", pp_instance));
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  if (!NaClHandlePassBrowserCtor()) {
    return NULL;
  }
#endif
  Plugin* plugin = new(std::nothrow) Plugin(pp_instance);
  PLUGIN_PRINTF(("Plugin::New (plugin=%p)\n", static_cast<void*>(plugin)));
  if (plugin == NULL) {
    return NULL;
  }
  return plugin;
}


// All failures of this function will show up as "Missing Plugin-in", so
// there is no need to log to JS console that there was an initialization
// failure. Note that module loading functions will log their own errors.
bool Plugin::Init(uint32_t argc, const char* argn[], const char* argv[]) {
  PLUGIN_PRINTF(("Plugin::Init (argc=%"NACL_PRIu32")\n", argc));
  HistogramEnumerateOsArch(GetSandboxISA());
  init_time_ = NaClGetTimeOfDayMicroseconds();

  BrowserInterface* browser_interface = new(std::nothrow) BrowserInterface;
  if (browser_interface == NULL) {
    return false;
  }
  ScriptableHandle* handle = ScriptableHandle::NewPlugin(this);
  if (handle == NULL) {
    return false;
  }
  set_scriptable_handle(handle);
  PLUGIN_PRINTF(("Plugin::Init (scriptable_handle=%p)\n",
                 static_cast<void*>(scriptable_handle())));
  url_util_ = pp::URLUtil_Dev::Get();
  if (url_util_ == NULL) {
    return false;
  }
  PLUGIN_PRINTF(("Plugin::Init (url_util_=%p)\n",
                 static_cast<const void*>(url_util_)));

  bool status = Plugin::Init(
      browser_interface,
      static_cast<int>(argc),
      // TODO(polina): Can we change the args on our end to be const to
      // avoid these ugly casts?
      const_cast<char**>(argn),
      const_cast<char**>(argv));
  if (status) {
    // Look for the developer attribute; if it's present, enable 'dev'
    // interfaces.
    const char* dev_settings = LookupArgument(kDevAttribute);
    enable_dev_interfaces_ = (dev_settings != NULL);

    const char* type_attr = LookupArgument(kTypeAttribute);
    if (type_attr != NULL) {
      mime_type_ = nacl::string(type_attr);
      std::transform(mime_type_.begin(), mime_type_.end(), mime_type_.begin(),
                     tolower);
    }

    const char* manifest_url = LookupArgument(kSrcManifestAttribute);
    // If the MIME type is foreign, then 'src' will be the URL for the content
    // and 'nacl' will be the URL for the manifest.
    if (IsForeignMIMEType()) {
      manifest_url = LookupArgument(kNaClManifestAttribute);
    }
    // Use the document URL as the base for resolving relative URLs to find the
    // manifest.  This takes into account the setting of <base> tags that
    // precede the embed/object.
    CHECK(url_util_ != NULL);
    pp::Var base_var = url_util_->GetDocumentURL(*this);
    if (!base_var.is_string()) {
      PLUGIN_PRINTF(("Plugin::Init (unable to find document url)\n"));
      return false;
    }
    set_plugin_base_url(base_var.AsString());
    if (manifest_url == NULL) {
      // TODO(sehr,polina): this should be a hard error when scripting
      // the src property is no longer allowed.
      PLUGIN_PRINTF(("Plugin::Init:"
                     " WARNING: no 'src' property, so no manifest loaded.\n"));
      if (NULL != LookupArgument(kNaClManifestAttribute)) {
        PLUGIN_PRINTF(("Plugin::Init:"
                       " WARNING: 'nacl' property is incorrect. Use 'src'.\n"));
      }
    } else {
      // Issue a GET for the manifest_url.  The manifest file will be parsed to
      // determine the nexe URL.
      // Sets src property to full manifest URL.
      RequestNaClManifest(manifest_url);
    }
  }

  // Export a property to allow us to get the last error description.
  AddPropertyGet(GetLastError, "lastError", "s");
  // Export a property to allow us to get the nexe exit status.
  AddPropertyGet(GetExitStatus, "exitStatus", "i");

  PLUGIN_PRINTF(("Plugin::Init (status=%d)\n", status));
  return status;
}


Plugin::Plugin(PP_Instance pp_instance)
    : pp::InstancePrivate(pp_instance),
      browser_interface_(NULL),
      scriptable_handle_(NULL),
      argc_(-1),
      argn_(NULL),
      argv_(NULL),
      main_subprocess_(kMainSubprocessId, NULL, NULL),
      nacl_ready_state_(UNSENT),
      nexe_error_reported_(false),
      wrapper_factory_(NULL),
      last_error_string_(""),
      ppapi_proxy_(NULL),
      enable_dev_interfaces_(false),
      replayDidChangeView(false),
      replayHandleDocumentLoad(false),
      init_time_(0),
      ready_time_(0),
      nexe_size_(0),
      time_of_last_progress_event_(0) {
  PLUGIN_PRINTF(("Plugin::Plugin (this=%p, pp_instance=%"
                 NACL_PRId32")\n", static_cast<void*>(this), pp_instance));
  NaClSrpcModuleInit();
  nexe_downloader_.Initialize(this);
  pnacl_.Initialize(this);
  callback_factory_.Initialize(this);
}


Plugin::~Plugin() {
  int64_t shutdown_start = NaClGetTimeOfDayMicroseconds();

  PLUGIN_PRINTF(("Plugin::~Plugin (this=%p, scriptable_handle=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(scriptable_handle())));

  // If the proxy has been shutdown before now, it's likely the plugin suffered
  // an error while loading.
  if (ppapi_proxy_ != NULL) {
    HistogramTimeLarge(
        "NaCl.ModuleUptime.Normal",
        (shutdown_start - ready_time_) / NACL_MICROS_PER_MILLI);
  }

#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  NaClHandlePassBrowserDtor();
#endif

  url_downloaders_.erase(url_downloaders_.begin(), url_downloaders_.end());

  ShutdownProxy();
  ScriptableHandle* scriptable_handle_ = scriptable_handle();
  ScriptableHandle::Unref(&scriptable_handle_);

  // ShutDownSubprocesses shuts down the subprocesses, which shuts
  // down the main ServiceRuntime object, which kills the subprocess.
  // As a side effect of the subprocess being killed, the reverse
  // services thread(s) will get EOF on the reverse channel(s), and
  // the thread(s) will exit.  In ServiceRuntime::Shutdown, we invoke
  // ReverseService::WaitForServiceThreadsToExit(), so that there will
  // not be an extent thread(s) hanging around.  This means that the
  // ~Plugin will block until this happens.  This is a requirement,
  // since the renderer should be free to unload the plugin code, and
  // we cannot have threads running code that gets unloaded before
  // they exit.
  //
  // By waiting for the threads here, we also ensure that the Plugin
  // object and the subprocess and ServiceRuntime objects is not
  // (fully) destroyed while the threads are running, so resources
  // that are destroyed after ShutDownSubprocesses (below) are
  // guaranteed to be live and valid for access from the service
  // threads.
  //
  // The main_subprocess object, which wraps the main service_runtime
  // object, is dtor'd implicitly after the explicit code below runs,
  // so the main service runtime object will not have been dtor'd,
  // though the Shutdown method may have been called, during the
  // lifetime of the service threads.
  ShutDownSubprocesses();

  // Shutdown Srpc module only after the service threads are done.
  // NB: NaClSrpcModuleInit and Fini should be invoked at dll
  // load/unload and not in the Plugin ctor/dtor.
  NaClSrpcModuleFini();

  delete wrapper_factory_;
  delete browser_interface_;
  delete[] argv_;
  delete[] argn_;

  HistogramTimeSmall(
      "NaCl.Perf.ShutdownTime.Total",
      (NaClGetTimeOfDayMicroseconds() - shutdown_start)
          / NACL_MICROS_PER_MILLI);

  PLUGIN_PRINTF(("Plugin::~Plugin (this=%p, return)\n",
                 static_cast<void*>(this)));
}


void Plugin::DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
  PLUGIN_PRINTF(("Plugin::DidChangeView (this=%p)\n",
                 static_cast<void*>(this)));

  if (!BrowserPpp::is_valid(ppapi_proxy_)) {
    // Store this event and replay it when the proxy becomes available.
    replayDidChangeView = true;
    replayDidChangeViewPosition = position;
    replayDidChangeViewClip = clip;
    return;
  } else {
    ppapi_proxy_->ppp_instance_interface()->DidChangeView(
        pp_instance(), &(position.pp_rect()), &(clip.pp_rect()));
  }
}


void Plugin::DidChangeFocus(bool has_focus) {
  PLUGIN_PRINTF(("Plugin::DidChangeFocus (this=%p)\n",
                 static_cast<void*>(this)));
  if (!BrowserPpp::is_valid(ppapi_proxy_)) {
    return;
  } else {
    ppapi_proxy_->ppp_instance_interface()->DidChangeFocus(
        pp_instance(), PP_FromBool(has_focus));
  }
}


bool Plugin::HandleInputEvent(const pp::InputEvent& event) {
  PLUGIN_PRINTF(("Plugin::HandleInputEvent (this=%p)\n",
                 static_cast<void*>(this)));
  if (!BrowserPpp::is_valid(ppapi_proxy_) ||
      ppapi_proxy_->ppp_input_event_interface() == NULL) {
    return false;  // event is not handled here.
  } else {
    bool handled = PP_ToBool(
        ppapi_proxy_->ppp_input_event_interface()->HandleInputEvent(
            pp_instance(), event.pp_resource()));
    PLUGIN_PRINTF(("Plugin::HandleInputEvent (handled=%d)\n", handled));
    return handled;
  }
}


bool Plugin::HandleDocumentLoad(const pp::URLLoader& url_loader) {
  PLUGIN_PRINTF(("Plugin::HandleDocumentLoad (this=%p)\n",
                 static_cast<void*>(this)));
  if (!BrowserPpp::is_valid(ppapi_proxy_)) {
    // Store this event and replay it when the proxy becomes available.
    replayHandleDocumentLoad = true;
    replayHandleDocumentLoadURLLoader = url_loader;
    // Return true so that the browser keeps servicing this loader so we can
    // perform requests on it later.
    return true;
  } else {
    return PP_ToBool(
        ppapi_proxy_->ppp_instance_interface()->HandleDocumentLoad(
            pp_instance(), url_loader.pp_resource()));
  }
}


void Plugin::HandleMessage(const pp::Var& message) {
  PLUGIN_PRINTF(("Plugin::HandleMessage (this=%p)\n",
                 static_cast<void*>(this)));
  if (BrowserPpp::is_valid(ppapi_proxy_) &&
      ppapi_proxy_->ppp_messaging_interface() != NULL) {
    ppapi_proxy_->ppp_messaging_interface()->HandleMessage(
        pp_instance(), message.pp_var());
  }
}


pp::Var Plugin::GetInstanceObject() {
  PLUGIN_PRINTF(("Plugin::GetInstanceObject (this=%p)\n",
                 static_cast<void*>(this)));
  // The browser will unref when it discards the var for this object.
  ScriptableHandle* handle =
      static_cast<ScriptableHandle*>(scriptable_handle()->AddRef());
  pp::Var* handle_var = handle->var();
  PLUGIN_PRINTF(("Plugin::GetInstanceObject (handle=%p, handle_var=%p)\n",
                 static_cast<void*>(handle), static_cast<void*>(handle_var)));
  return *handle_var;  // make a copy
}

void Plugin::HistogramStartupTimeSmall(const std::string& name, float dt) {
  if (nexe_size_ > 0) {
    float size_in_MB = static_cast<float>(nexe_size_) / (1024.f * 1024.f);
    HistogramTimeSmall(name, static_cast<int64_t>(dt));
    HistogramTimeSmall(name + "PerMB", static_cast<int64_t>(dt / size_in_MB));
  }
}

void Plugin::HistogramStartupTimeMedium(const std::string& name, float dt) {
  if (nexe_size_ > 0) {
    float size_in_MB = static_cast<float>(nexe_size_) / (1024.f * 1024.f);
    HistogramTimeMedium(name, static_cast<int64_t>(dt));
    HistogramTimeMedium(name + "PerMB", static_cast<int64_t>(dt / size_in_MB));
  }
}

void Plugin::NexeFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::NexeFileDidOpen (pp_error=%"NACL_PRId32")\n",
                 pp_error));
  int32_t file_desc = nexe_downloader_.GetPOSIXFileDescriptor();
  PLUGIN_PRINTF(("Plugin::NexeFileDidOpen (file_desc=%"NACL_PRId32")\n",
                 file_desc));
  ErrorInfo error_info;
  if (pp_error != PP_OK || file_desc == NACL_NO_FILE_DESC) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else {
      error_info.SetReport(ERROR_NEXE_LOAD_URL, "could not load nexe url.");
      ReportLoadError(error_info);
    }
    return;
  }
  int32_t file_desc_ok_to_close = DUP(file_desc);
  if (file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    error_info.SetReport(ERROR_NEXE_FH_DUP,
                         "could not duplicate loaded file handle.");
    ReportLoadError(error_info);
    return;
  }
  struct stat stat_buf;
  if (0 != fstat(file_desc_ok_to_close, &stat_buf)) {
    CLOSE(file_desc_ok_to_close);
    error_info.SetReport(ERROR_NEXE_STAT, "could not stat nexe file.");
    ReportLoadError(error_info);
    return;
  }
  size_t nexe_bytes_read = static_cast<size_t>(stat_buf.st_size);

  nexe_size_ = nexe_bytes_read;
  HistogramSizeKB("NaCl.Perf.Size.Nexe",
                  static_cast<int32_t>(nexe_size_ / 1024));
  HistogramStartupTimeMedium(
      "NaCl.Perf.StartupTime.NexeDownload",
      static_cast<float>(nexe_downloader_.TimeSinceOpenMilliseconds()));

  // Inform JavaScript that we successfully downloaded the nacl module.
  EnqueueProgressEvent(kProgressEventProgress,
                       nexe_downloader_.url_to_open(),
                       LENGTH_IS_COMPUTABLE,
                       nexe_bytes_read,
                       nexe_bytes_read);

  load_start_ = NaClGetTimeOfDayMicroseconds();
  nacl::scoped_ptr<nacl::DescWrapper>
      wrapper(wrapper_factory()->MakeFileDesc(file_desc_ok_to_close, O_RDONLY));
  NaClLog(4, "NexeFileDidOpen: invoking LoadNaClModule\n");
  bool was_successful = LoadNaClModule(
      wrapper.get(), &error_info,
      callback_factory_.NewCallback(&Plugin::NexeFileDidOpenContinuation),
      callback_factory_.NewCallback(&Plugin::NexeDidCrash));

  if (!was_successful) {
    ReportLoadError(error_info);
  }
}

void Plugin::NexeFileDidOpenContinuation(int32_t pp_error) {
  ErrorInfo error_info;
  bool was_successful;

  UNREFERENCED_PARAMETER(pp_error);
  NaClLog(4, "Entered NexeFileDidOpenContinuation\n");
  NaClLog(4, "NexeFileDidOpenContinuation: invoking"
          " LoadNaClModuleContinuationIntern\n");
  was_successful = LoadNaClModuleContinuationIntern(&error_info);
  if (was_successful) {
    NaClLog(4, "NexeFileDidOpenContinuation: success;"
            " setting histograms\n");
    ready_time_ = NaClGetTimeOfDayMicroseconds();
    HistogramStartupTimeSmall(
        "NaCl.Perf.StartupTime.LoadModule",
        static_cast<float>(ready_time_ - load_start_) / NACL_MICROS_PER_MILLI);
    HistogramStartupTimeMedium(
        "NaCl.Perf.StartupTime.Total",
        static_cast<float>(ready_time_ - init_time_) / NACL_MICROS_PER_MILLI);

    ReportLoadSuccess(LENGTH_IS_COMPUTABLE, nexe_size_, nexe_size_);
  } else {
    NaClLog(4, "NexeFileDidOpenContinuation: failed.");
    ReportLoadError(error_info);
  }
  NaClLog(4, "Leaving NexeFileDidOpenContinuation\n");
}

void Plugin::NexeDidCrash(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::NexeDidCrash (pp_error=%"NACL_PRId32")\n",
                 pp_error));
  if (pp_error != PP_OK) {
    PLUGIN_PRINTF(("Plugin::NexeDidCrash: CallOnMainThread callback with"
                   " non-PP_OK arg -- SHOULD NOT HAPPEN\n"));
  }
  PLUGIN_PRINTF(("Plugin::NexeDidCrash: crash event!\n"));
  // If the crash occurs during load, we just want to report an error
  // that fits into our load progress event grammar.  If the crash
  // occurs after loaded/loadend, then we use ReportDeadNexe to send a
  // "crash" event.
  if (nexe_error_reported()) {
    PLUGIN_PRINTF(("Plugin::NexeDidCrash: error already reported;"
                   " suppressing\n"));
    return;
  }
  if (nacl_ready_state() == DONE) {
    ReportDeadNexe();
  } else {
    ErrorInfo error_info;
    error_info.SetReport(ERROR_START_PROXY_CRASH,  // Not quite right.
                         "Nexe crashed during startup");
    ReportLoadError(error_info);
  }
}

void Plugin::BitcodeDidTranslate(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::BitcodeDidTranslate (pp_error=%"NACL_PRId32")\n",
                 pp_error));
  if (pp_error != PP_OK) {
    // Error should have been reported by pnacl. Just return.
    PLUGIN_PRINTF(("Plugin::BitcodeDidTranslate error in Pnacl\n"));
    return;
  }
  // Inform JavaScript that we successfully translated the bitcode to a nexe.
  EnqueueProgressEvent(kProgressEventProgress);
  nacl::scoped_ptr<nacl::DescWrapper>
      wrapper(pnacl_.ReleaseTranslatedFD());
  ErrorInfo error_info;
  bool was_successful = LoadNaClModule(
      wrapper.get(), &error_info,
      callback_factory_.NewCallback(&Plugin::BitcodeDidTranslateContinuation),
      pp::BlockUntilComplete());

  if (!was_successful) {
    ReportLoadError(error_info);
  }
}

void Plugin::BitcodeDidTranslateContinuation(int32_t pp_error) {
  ErrorInfo error_info;
  bool was_successful = LoadNaClModuleContinuationIntern(&error_info);

  NaClLog(4, "Entered BitcodeDidTranslateContinuation\n");
  UNREFERENCED_PARAMETER(pp_error);
  if (was_successful) {
    ReportLoadSuccess(LENGTH_IS_NOT_COMPUTABLE,
                      kUnknownBytes,
                      kUnknownBytes);
  } else {
    ReportLoadError(error_info);
  }
}

bool Plugin::StartProxiedExecution(NaClSrpcChannel* srpc_channel,
                                   ErrorInfo* error_info) {
  PLUGIN_PRINTF(("Plugin::StartProxiedExecution (srpc_channel=%p)\n",
                 static_cast<void*>(srpc_channel)));

  // Log the amound of time that has passed between the trusted plugin being
  // initialized and the untrusted plugin being initialized.  This is (roughly)
  // the cost of using NaCl, in terms of startup time.
  HistogramStartupTimeMedium(
      "NaCl.Perf.StartupTime.NaClOverhead",
      static_cast<float>(NaClGetTimeOfDayMicroseconds() - init_time_)
          / NACL_MICROS_PER_MILLI);

  // Check that the .nexe exports the PPAPI intialization method.
  NaClSrpcService* client_service = srpc_channel->client;
  if (NaClSrpcServiceMethodIndex(client_service,
                                 "PPP_InitializeModule:iihs:ii") ==
      kNaClSrpcInvalidMethodIndex) {
    error_info->SetReport(
        ERROR_START_PROXY_CHECK_PPP,
        "could not find PPP_InitializeModule() - toolchain version mismatch?");
    PLUGIN_PRINTF(("Plugin::StartProxiedExecution (%s)\n",
                   error_info->message().c_str()));
    return false;
  }
  nacl::scoped_ptr<BrowserPpp> ppapi_proxy(
      new(std::nothrow) BrowserPpp(srpc_channel, this));
  PLUGIN_PRINTF(("Plugin::StartProxiedExecution (ppapi_proxy=%p)\n",
                 static_cast<void*>(ppapi_proxy.get())));
  if (ppapi_proxy.get() == NULL) {
    error_info->SetReport(ERROR_START_PROXY_ALLOC,
                          "could not allocate proxy memory.");
    return false;
  }
  pp::Module* module = pp::Module::Get();
  PLUGIN_PRINTF(("Plugin::StartProxiedExecution (module=%p)\n",
                 static_cast<void*>(module)));
  CHECK(module != NULL);  // We could not have gotten past init stage otherwise.
  int32_t pp_error =
      ppapi_proxy->InitializeModule(module->pp_module(),
                                     module->get_browser_interface());
  PLUGIN_PRINTF(("Plugin::StartProxiedExecution (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    error_info->SetReport(ERROR_START_PROXY_MODULE,
                          "could not initialize module.");
    return false;
  }
  const PPP_Instance* instance_interface =
      ppapi_proxy->ppp_instance_interface();
  PLUGIN_PRINTF(("Plugin::StartProxiedExecution (ppp_instance=%p)\n",
                 static_cast<const void*>(instance_interface)));
  CHECK(instance_interface != NULL);  // Verified on module initialization.
  PP_Bool did_create = instance_interface->DidCreate(
      pp_instance(),
      argc(),
      const_cast<const char**>(argn()),
      const_cast<const char**>(argv()));
  PLUGIN_PRINTF(("Plugin::StartProxiedExecution (did_create=%d)\n",
                 did_create));
  if (did_create == PP_FALSE) {
    error_info->SetReport(ERROR_START_PROXY_INSTANCE,
                          "could not create instance.");
    return false;
  }

  ppapi_proxy_ = ppapi_proxy.release();

  // Create PPP* interface adapters to forward calls to .nexe.
  find_adapter_.reset(new(std::nothrow) FindAdapter(this));
  mouse_lock_adapter_.reset(new(std::nothrow) MouseLockAdapter(this));
  printing_adapter_.reset(new(std::nothrow) PrintingAdapter(this));
  selection_adapter_.reset(new(std::nothrow) SelectionAdapter(this));
  widget_client_adapter_.reset(new(std::nothrow) WidgetClientAdapter(this));
  zoom_adapter_.reset(new(std::nothrow) ZoomAdapter(this));

  // Replay missed events.
  if (replayDidChangeView) {
    replayDidChangeView = false;
    DidChangeView(replayDidChangeViewPosition, replayDidChangeViewClip);
  }
  if (replayHandleDocumentLoad) {
    replayHandleDocumentLoad = false;
    HandleDocumentLoad(replayHandleDocumentLoadURLLoader);
    // Release our reference on this loader.
    replayHandleDocumentLoadURLLoader = pp::URLLoader();
  }
  bool is_valid_proxy = BrowserPpp::is_valid(ppapi_proxy_);
  PLUGIN_PRINTF(("Plugin::StartProxiedExecution (is_valid_proxy=%d)\n",
                 is_valid_proxy));
  if (!is_valid_proxy) {
    error_info->SetReport(ERROR_START_PROXY_CRASH,
                          "instance crashed after creation.");
  }
  return is_valid_proxy;
}

void Plugin::ReportDeadNexe() {
  PLUGIN_PRINTF(("Plugin::ReportDeadNexe\n"));
  if (ppapi_proxy_ != NULL)
    ppapi_proxy_->ReportDeadNexe();

  if (nacl_ready_state() == DONE && !nexe_error_reported()) {  // After loadEnd.
    int64_t crash_time = NaClGetTimeOfDayMicroseconds();
    // Crashes will be more likely near startup, so use a medium histogram
    // instead of a large one.
    HistogramTimeMedium(
        "NaCl.ModuleUptime.Crash",
        (crash_time - ready_time_) / NACL_MICROS_PER_MILLI);

    nacl::string message = nacl::string("NaCl module crashed");
    set_last_error_string(message);
    browser_interface()->AddToConsole(this, message);

    EnqueueProgressEvent(kProgressEventCrash);
    set_nexe_error_reported(true);
    CHECK(ppapi_proxy_ == NULL || !ppapi_proxy_->is_valid());
    ShutdownProxy();
  }
  // else ReportLoadError() and ReportAbortError() will be used by loading code
  // to provide error handling and proxy shutdown.
  //
  // NOTE: not all crashes during load will make it here.
  // Those in BrowserPpp::InitializeModule and creation of PPP interfaces
  // will just get reported back as PP_ERROR_FAILED.
}

void Plugin::ShutdownProxy() {
  PLUGIN_PRINTF(("Plugin::ShutdownProxy (ppapi_proxy=%p)\n",
                static_cast<void*>(ppapi_proxy_)));
  // We do not call remote PPP_Instance::DidDestroy because the untrusted
  // side can no longer take full advantage of mostly asynchronous Pepper
  // per-Instance interfaces at this point.
  if (ppapi_proxy_ != NULL) {
    ppapi_proxy_->ShutdownModule();
    delete ppapi_proxy_;
    ppapi_proxy_ = NULL;
  }
}

void Plugin::NaClManifestBufferReady(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::NaClManifestBufferReady (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  ErrorInfo error_info;
  set_manifest_url(nexe_downloader_.url());
  if (pp_error != PP_OK) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else {
      error_info.SetReport(ERROR_MANIFEST_LOAD_URL,
                           "could not load manifest url.");
      ReportLoadError(error_info);
    }
    return;
  }

  const std::deque<char>& buffer = nexe_downloader_.buffer();
  size_t buffer_size = buffer.size();
  if (buffer_size > kNaClManifestMaxFileBytes) {
    error_info.SetReport(ERROR_MANIFEST_TOO_LARGE,
                         "manifest file too large.");
    ReportLoadError(error_info);
    return;
  }
  nacl::scoped_array<char> json_buffer(new char[buffer_size + 1]);
  if (json_buffer == NULL) {
    error_info.SetReport(ERROR_MANIFEST_MEMORY_ALLOC,
                         "could not allocate manifest memory.");
    ReportLoadError(error_info);
    return;
  }
  std::copy(buffer.begin(), buffer.begin() + buffer_size, &json_buffer[0]);
  json_buffer[buffer_size] = '\0';

  ProcessNaClManifest(json_buffer.get());
}

void Plugin::NaClManifestFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::NaClManifestFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  HistogramTimeSmall("NaCl.Perf.StartupTime.ManifestDownload",
                     nexe_downloader_.TimeSinceOpenMilliseconds());
  ErrorInfo error_info;
  // The manifest file was successfully opened.  Set the src property on the
  // plugin now, so that the full url is available to error handlers.
  set_manifest_url(nexe_downloader_.url());
  int32_t file_desc = nexe_downloader_.GetPOSIXFileDescriptor();
  PLUGIN_PRINTF(("Plugin::NaClManifestFileDidOpen (file_desc=%"
                 NACL_PRId32")\n", file_desc));
  if (pp_error != PP_OK || file_desc == NACL_NO_FILE_DESC) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else {
      error_info.SetReport(ERROR_MANIFEST_LOAD_URL,
                           "could not load manifest url.");
      ReportLoadError(error_info);
    }
    return;
  }
  // Duplicate the file descriptor in order to create a FILE stream with it
  // that can later be closed without closing the original descriptor.  The
  // browser will take care of the original descriptor.
  int dup_file_desc = DUP(file_desc);
  struct stat stat_buf;
  if (0 != fstat(dup_file_desc, &stat_buf)) {
    CLOSE(dup_file_desc);
    error_info.SetReport(ERROR_MANIFEST_STAT,
                         "could not stat manifest file.");
    ReportLoadError(error_info);
    return;
  }
  size_t bytes_to_read = static_cast<size_t>(stat_buf.st_size);
  if (bytes_to_read > kNaClManifestMaxFileBytes) {
    CLOSE(dup_file_desc);
    error_info.SetReport(ERROR_MANIFEST_TOO_LARGE,
                         "manifest file too large.");
    ReportLoadError(error_info);
    return;
  }
  FILE* json_file = fdopen(dup_file_desc, "rb");
  PLUGIN_PRINTF(("Plugin::NaClManifestFileDidOpen "
                 "(dup_file_desc=%"NACL_PRId32", json_file=%p)\n",
                 dup_file_desc, static_cast<void*>(json_file)));
  if (json_file == NULL) {
    CLOSE(dup_file_desc);
    error_info.SetReport(ERROR_MANIFEST_OPEN,
                         "could not open manifest file.");
    ReportLoadError(error_info);
    return;
  }
  nacl::scoped_array<char> json_buffer(new char[bytes_to_read + 1]);
  if (json_buffer == NULL) {
    fclose(json_file);
    error_info.SetReport(ERROR_MANIFEST_MEMORY_ALLOC,
                         "could not allocate manifest memory.");
    ReportLoadError(error_info);
    return;
  }
  // json_buffer could hold a large enough buffer that the system might need
  // multiple reads to fill it, so iterate through reads.
  size_t total_bytes_read = 0;
  while (0 < bytes_to_read) {
    size_t bytes_this_read = fread(&json_buffer[total_bytes_read],
                                   sizeof(char),
                                   bytes_to_read,
                                   json_file);
    if (bytes_this_read < bytes_to_read &&
        (feof(json_file) || ferror(json_file))) {
      PLUGIN_PRINTF(("Plugin::NaClManifestFileDidOpen failed: "
                     "total_bytes_read=%"NACL_PRIuS" "
                     "bytes_to_read=%"NACL_PRIuS"\n",
                     total_bytes_read, bytes_to_read));
      fclose(json_file);
      error_info.SetReport(ERROR_MANIFEST_READ,
                           "could not read manifest file.");
      ReportLoadError(error_info);
      return;
    }
    total_bytes_read += bytes_this_read;
    bytes_to_read -= bytes_this_read;
  }
  // Once the bytes are read, the FILE is no longer needed, so close it.  This
  // allows for early returns without leaking the |json_file| FILE object.
  fclose(json_file);
  // No need to close |file_desc|, that is handled by |nexe_downloader_|.
  json_buffer[total_bytes_read] = '\0';  // Force null termination.

  ProcessNaClManifest(json_buffer.get());
}

void Plugin::ProcessNaClManifest(const nacl::string& manifest_json) {
  HistogramSizeKB("NaCl.Perf.Size.Manifest",
                  static_cast<int32_t>(manifest_json.length() / 1024));
  nacl::string program_url;
  bool is_portable;
  ErrorInfo error_info;
  if (!SetManifestObject(manifest_json, &error_info)) {
    ReportLoadError(error_info);
    return;
  }

  if (SelectProgramURLFromManifest(&program_url, &error_info, &is_portable)) {
    set_nacl_ready_state(LOADING);
    // Inform JavaScript that we found a nexe URL to load.
    EnqueueProgressEvent(kProgressEventProgress);
    if (is_portable) {
      pp::CompletionCallback translate_callback =
          callback_factory_.NewCallback(&Plugin::BitcodeDidTranslate);
      // Will always call the callback on success or failure.
      pnacl_.BitcodeToNative(program_url, translate_callback);
      return;
    } else {
      pp::CompletionCallback open_callback =
          callback_factory_.NewRequiredCallback(&Plugin::NexeFileDidOpen);
      // Will always call the callback on success or failure.
      CHECK(
          nexe_downloader_.Open(program_url,
                                DOWNLOAD_TO_FILE,
                                open_callback,
                                &UpdateDownloadProgress));
      return;
    }
  }
  // Failed to select the program and/or the translator.
  ReportLoadError(error_info);
}

void Plugin::RequestNaClManifest(const nacl::string& url) {
  PLUGIN_PRINTF(("Plugin::RequestNaClManifest (url='%s')\n", url.c_str()));
  PLUGIN_PRINTF(("Plugin::RequestNaClManifest (plugin base url='%s')\n",
                 plugin_base_url().c_str()));
  // The full URL of the manifest file is relative to the base url.
  CHECK(url_util_ != NULL);
  pp::Var nmf_resolved_url =
      url_util_->ResolveRelativeToURL(plugin_base_url(), pp::Var(url));
  if (!nmf_resolved_url.is_string()) {
    ErrorInfo error_info;
    error_info.SetReport(
        ERROR_MANIFEST_RESOLVE_URL,
        nacl::string("could not resolve URL \"") + url.c_str() +
        "\" relative to \"" + plugin_base_url().c_str() + "\".");
    ReportLoadError(error_info);
    return;
  }
  PLUGIN_PRINTF(("Plugin::RequestNaClManifest (resolved url='%s')\n",
                 nmf_resolved_url.AsString().c_str()));
  set_manifest_base_url(nmf_resolved_url.AsString());
  set_manifest_url(url);
  // Inform JavaScript that a load is starting.
  set_nacl_ready_state(OPENED);
  EnqueueProgressEvent(kProgressEventLoadStart);
  bool is_data_uri = GetUrlScheme(nmf_resolved_url.AsString()) == SCHEME_DATA;
  HistogramEnumerateManifestIsDataURI(static_cast<int>(is_data_uri));
  if (is_data_uri) {
    pp::CompletionCallback open_callback =
        callback_factory_.NewRequiredCallback(&Plugin::NaClManifestBufferReady);
    // Will always call the callback on success or failure.
    CHECK(nexe_downloader_.Open(nmf_resolved_url.AsString(),
                                DOWNLOAD_TO_BUFFER,
                                open_callback,
                                NULL));
  } else {
    pp::CompletionCallback open_callback =
        callback_factory_.NewRequiredCallback(&Plugin::NaClManifestFileDidOpen);
    // Will always call the callback on success or failure.
    CHECK(nexe_downloader_.Open(nmf_resolved_url.AsString(),
                                DOWNLOAD_TO_FILE,
                                open_callback,
                                NULL));
  }
}


bool Plugin::SetManifestObject(const nacl::string& manifest_json,
                               ErrorInfo* error_info) {
  PLUGIN_PRINTF(("Plugin::SetManifestObject(): manifest_json='%s'.\n",
       manifest_json.c_str()));
  if (error_info == NULL)
    return false;
  manifest_.reset(
      new Manifest(url_util_, manifest_base_url(), GetSandboxISA()));
  if (!manifest_->Init(manifest_json, error_info)) {
    return false;
  }
  return true;
}

bool Plugin::SelectProgramURLFromManifest(nacl::string* result,
                                          ErrorInfo* error_info,
                                          bool* is_portable) {
  const nacl::string sandbox_isa(GetSandboxISA());
  PLUGIN_PRINTF(("Plugin::SelectProgramURLFromManifest(): sandbox='%s'.\n",
                 sandbox_isa.c_str()));
  if (result == NULL || error_info == NULL || manifest_ == NULL)
    return false;
  return manifest_->GetProgramURL(result, error_info, is_portable);
}

void Plugin::UrlDidOpenForStreamAsFile(int32_t pp_error,
                                       FileDownloader*& url_downloader,
                                       PP_CompletionCallback callback) {
  PLUGIN_PRINTF(("Plugin::UrlDidOpen (pp_error=%"NACL_PRId32
                 ", url_downloader=%p)\n", pp_error,
                 static_cast<void*>(url_downloader)));
  url_downloaders_.erase(url_downloader);
  nacl::scoped_ptr<FileDownloader> scoped_url_downloader(url_downloader);
  int32_t file_desc = scoped_url_downloader->GetPOSIXFileDescriptor();

  if (pp_error != PP_OK) {
    PP_RunCompletionCallback(&callback, pp_error);
  } else if (file_desc > NACL_NO_FILE_DESC) {
    url_fd_map_[url_downloader->url_to_open()] = file_desc;
    PP_RunCompletionCallback(&callback, PP_OK);
  } else {
    PP_RunCompletionCallback(&callback, PP_ERROR_FAILED);
  }
}

int32_t Plugin::GetPOSIXFileDesc(const nacl::string& url) {
  PLUGIN_PRINTF(("Plugin::GetFileDesc (url=%s)\n", url.c_str()));
  int32_t file_desc_ok_to_close = NACL_NO_FILE_DESC;
  std::map<nacl::string, int32_t>::iterator it = url_fd_map_.find(url);
  if (it != url_fd_map_.end())
    file_desc_ok_to_close = DUP(it->second);
  return file_desc_ok_to_close;
}


bool Plugin::StreamAsFile(const nacl::string& url,
                          PP_CompletionCallback callback) {
  PLUGIN_PRINTF(("Plugin::StreamAsFile (url='%s')\n", url.c_str()));
  FileDownloader* downloader = new FileDownloader();
  downloader->Initialize(this);
  url_downloaders_.insert(downloader);
  pp::CompletionCallback open_callback =
      callback_factory_.NewRequiredCallback(
          &Plugin::UrlDidOpenForStreamAsFile, downloader, callback);
  // Untrusted loads are always relative to the page's origin.
  CHECK(url_util_ != NULL);
  pp::Var resolved_url =
      url_util_->ResolveRelativeToURL(pp::Var(plugin_base_url()), url);
  if (!resolved_url.is_string()) {
    PLUGIN_PRINTF(("Plugin::StreamAsFile: "
                   "could not resolve url \"%s\" relative to plugin \"%s\".",
                   url.c_str(),
                   plugin_base_url().c_str()));
    return false;
  }
  // If true, will always call the callback on success or failure.
  return downloader->Open(url,
                          DOWNLOAD_TO_FILE,
                          open_callback,
                          &UpdateDownloadProgress);
}

#ifndef HACK_FOR_MACOS_HANG_REMOVED
// The following is needed to avoid a plugin startup hang in the
// MacOS "chrome_browser_tests under gyp" stage.
// TODO(sehr,mseaborn): remove this hack.
void (plugin::Plugin::*pmem)(int32_t,
                             plugin::FileDownloader*&,
                             pp::VarPrivate&);
void Plugin::XYZZY(const nacl::string& url,
                           pp::VarPrivate js_callback) {
  UNREFERENCED_PARAMETER(url);
  UNREFERENCED_PARAMETER(js_callback);
  pp::CompletionCallback open_callback =
      callback_factory_.NewRequiredCallback(pmem,
          reinterpret_cast<plugin::FileDownloader*>(NULL),
          js_callback);
  static_cast<void>(open_callback);
}
#endif  // HACK_FOR_MACOS_HANG_REMOVED


void Plugin::ReportLoadSuccess(LengthComputable length_computable,
                               uint64_t loaded_bytes,
                               uint64_t total_bytes) {
  // Set the readyState attribute to indicate loaded.
  set_nacl_ready_state(DONE);
  // Inform JavaScript that loading was successful and is complete.
  const nacl::string& url = nexe_downloader_.url_to_open();
  EnqueueProgressEvent(
      kProgressEventLoad, url, length_computable, loaded_bytes, total_bytes);
  EnqueueProgressEvent(
      kProgressEventLoadEnd, url, length_computable, loaded_bytes, total_bytes);

  // UMA
  HistogramEnumerateLoadStatus(ERROR_LOAD_SUCCESS);
}


// TODO(ncbray): report UMA stats
void Plugin::ReportLoadError(const ErrorInfo& error_info) {
  PLUGIN_PRINTF(("Plugin::ReportLoadError (error='%s')\n",
                 error_info.message().c_str()));
  // Set the readyState attribute to indicate we need to start over.
  set_nacl_ready_state(DONE);
  set_nexe_error_reported(true);
  // Report an error in lastError and on the JavaScript console.
  nacl::string message = nacl::string("NaCl module load failed: ") +
      error_info.message();
  set_last_error_string(message);
  browser_interface()->AddToConsole(this, message);
  ShutdownProxy();
  // Inform JavaScript that loading encountered an error and is complete.
  EnqueueProgressEvent(kProgressEventError);
  EnqueueProgressEvent(kProgressEventLoadEnd);

  // UMA
  HistogramEnumerateLoadStatus(error_info.error_code());
}


void Plugin::ReportLoadAbort() {
  PLUGIN_PRINTF(("Plugin::ReportLoadAbort\n"));
  // Set the readyState attribute to indicate we need to start over.
  set_nacl_ready_state(DONE);
  set_nexe_error_reported(true);
  // Report an error in lastError and on the JavaScript console.
  nacl::string error_string("NaCl module load failed: user aborted");
  set_last_error_string(error_string);
  browser_interface()->AddToConsole(this, error_string);
  ShutdownProxy();
  // Inform JavaScript that loading was aborted and is complete.
  EnqueueProgressEvent(kProgressEventAbort);
  EnqueueProgressEvent(kProgressEventLoadEnd);

  // UMA
  HistogramEnumerateLoadStatus(ERROR_LOAD_ABORTED);
}

void Plugin::UpdateDownloadProgress(
    PP_Instance pp_instance,
    PP_Resource pp_resource,
    int64_t /*bytes_sent*/,
    int64_t /*total_bytes_to_be_sent*/,
    int64_t bytes_received,
    int64_t total_bytes_to_be_received) {
  Instance* instance = pp::Module::Get()->InstanceForPPInstance(pp_instance);
  if (instance != NULL) {
    Plugin* plugin = static_cast<Plugin*>(instance);
    // Rate limit progress events to a maximum of 100 per second.
    int64_t time = NaClGetTimeOfDayMicroseconds();
    int64_t elapsed = time - plugin->time_of_last_progress_event_;
    const int64_t kTenMilliseconds = 10000;
    if (elapsed > kTenMilliseconds) {
      plugin->time_of_last_progress_event_ = time;

      // Find the URL loader that sent this notification.
      const FileDownloader* file_downloader =
          plugin->FindFileDownloader(pp_resource);
      // If not a streamed file, it must be the .nexe loader.
      if (file_downloader == NULL)
        file_downloader = &plugin->nexe_downloader_;
      nacl::string url = file_downloader->url_to_open();
      LengthComputable length_computable = (total_bytes_to_be_received >= 0) ?
          LENGTH_IS_COMPUTABLE : LENGTH_IS_NOT_COMPUTABLE;

      plugin->EnqueueProgressEvent(kProgressEventProgress,
                                   url,
                                   length_computable,
                                   bytes_received,
                                   total_bytes_to_be_received);
    }
  }
}

const FileDownloader* Plugin::FindFileDownloader(
    PP_Resource url_loader) const {
  const FileDownloader* file_downloader = NULL;
  if (url_loader == nexe_downloader_.url_loader()) {
    file_downloader = &nexe_downloader_;
  } else {
    std::set<FileDownloader*>::const_iterator it = url_downloaders_.begin();
    while (it != url_downloaders_.end()) {
      if (url_loader == (*it)->url_loader()) {
        file_downloader = (*it);
        break;
      }
      ++it;
    }
  }
  return file_downloader;
}

void Plugin::EnqueueProgressEvent(const char* event_type) {
  EnqueueProgressEvent(event_type,
                       NACL_NO_URL,
                       Plugin::LENGTH_IS_NOT_COMPUTABLE,
                       Plugin::kUnknownBytes,
                       Plugin::kUnknownBytes);
}

void Plugin::EnqueueProgressEvent(const char* event_type,
                                  const nacl::string& url,
                                  LengthComputable length_computable,
                                  uint64_t loaded_bytes,
                                  uint64_t total_bytes) {
  PLUGIN_PRINTF(("Plugin::EnqueueProgressEvent ("
                 "event_type='%s', url='%s', length_computable=%d, "
                 "loaded=%"NACL_PRIu64", total=%"NACL_PRIu64")\n",
                 event_type,
                 url.c_str(),
                 static_cast<int>(length_computable),
                 loaded_bytes,
                 total_bytes));

  progress_events_.push(new ProgressEvent(event_type,
                                          url,
                                          length_computable,
                                          loaded_bytes,
                                          total_bytes));
  // Note that using callback_factory_ in this way is not thread safe.
  // If/when EnqueueProgressEvent is callable from another thread, this
  // will need to change.
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&Plugin::DispatchProgressEvent);
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, callback, 0);
}

void Plugin::ReportSelLdrLoadStatus(int status) {
  HistogramEnumerateSelLdrLoadStatus(static_cast<NaClErrorCode>(status));
}

void Plugin::DispatchProgressEvent(int32_t result) {
  PLUGIN_PRINTF(("Plugin::DispatchProgressEvent (result=%"
                 NACL_PRId32")\n", result));
  if (result < 0) {
    return;
  }
  if (progress_events_.empty()) {
    PLUGIN_PRINTF(("Plugin::DispatchProgressEvent: no pending events\n"));
    return;
  }
  nacl::scoped_ptr<ProgressEvent> event(progress_events_.front());
  progress_events_.pop();
  PLUGIN_PRINTF(("Plugin::DispatchProgressEvent ("
                 "event_type='%s', url='%s', length_computable=%d, "
                 "loaded=%"NACL_PRIu64", total=%"NACL_PRIu64")\n",
                 event->event_type(),
                 event->url(),
                 static_cast<int>(event->length_computable()),
                 event->loaded_bytes(),
                 event->total_bytes()));

  static const char* kEventClosureJS =
      "(function(target, type, url,"
      "          lengthComputable, loadedBytes, totalBytes) {"
      "    var progress_event = new ProgressEvent(type, {"
      "        bubbles: false,"
      "        cancelable: true,"
      "        lengthComputable: lengthComputable,"
      "        loaded: loadedBytes,"
      "        total: totalBytes"
      "      });"
      "    progress_event.url = url;"
      "    target.dispatchEvent(progress_event);"
      "})";

  // Create a function object by evaluating the JavaScript text.
  // TODO(sehr, polina): We should probably cache the created function object to
  // avoid JavaScript reparsing.
  pp::VarPrivate exception;
  pp::VarPrivate function_object = ExecuteScript(kEventClosureJS, &exception);
  if (!exception.is_undefined() || !function_object.is_object()) {
    PLUGIN_PRINTF(("Plugin::DispatchProgressEvent:"
                   " Function object creation failed.\n"));
    return;
  }
  // Get the target of the event to be dispatched.
  pp::Var owner_element_object = GetOwnerElementObject();
  if (!owner_element_object.is_object()) {
    PLUGIN_PRINTF(("Plugin::DispatchProgressEvent:"
                   " Couldn't get owner element object.\n"));
    NACL_NOTREACHED();
    return;
  }

  pp::Var argv[6];
  static const uint32_t argc = NACL_ARRAY_SIZE(argv);
  argv[0] = owner_element_object;
  argv[1] = pp::Var(event->event_type());
  argv[2] = pp::Var(event->url());
  argv[3] = pp::Var(event->length_computable() == LENGTH_IS_COMPUTABLE);
  argv[4] = pp::Var(static_cast<double>(event->loaded_bytes()));
  argv[5] = pp::Var(static_cast<double>(event->total_bytes()));

  // Dispatch the event.
  const pp::Var default_method;
  function_object.Call(default_method, argc, argv, &exception);
  if (!exception.is_undefined()) {
    PLUGIN_PRINTF(("Plugin::DispatchProgressEvent:"
                   " event dispatch failed.\n"));
  }
}

UrlSchemeType Plugin::GetUrlScheme(const std::string& url) {
  CHECK(url_util_ != NULL);
  PP_URLComponents_Dev comps;
  pp::Var canonicalized =
      url_util_->Canonicalize(pp::Var(url), &comps);

  if (canonicalized.is_null() ||
      (comps.scheme.begin == 0 && comps.scheme.len == -1)) {
    // |url| was an invalid URL or has no scheme.
    return SCHEME_OTHER;
  }

  CHECK(comps.scheme.begin <
            static_cast<int>(canonicalized.AsString().size()));
  CHECK(comps.scheme.begin + comps.scheme.len <
            static_cast<int>(canonicalized.AsString().size()));

  std::string scheme = canonicalized.AsString().substr(comps.scheme.begin,
                                                       comps.scheme.len);
  if (scheme == kChromeExtensionUriScheme)
    return SCHEME_CHROME_EXTENSION;
  if (scheme == kDataUriScheme)
    return SCHEME_DATA;
  return SCHEME_OTHER;
}

}  // namespace plugin
