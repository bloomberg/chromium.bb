/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef _MSC_VER
// Do not warn about use of std::copy with raw pointers.
#pragma warning(disable : 4996)
#endif

#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"

#include <stdio.h>
#include <algorithm>
#include <deque>
#include <vector>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/ppapi/async_receive.h"
#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/manifest.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

#include "native_client/src/third_party/ppapi/c/dev/ppp_find_dev.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_printing_dev.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_scrollbar_dev.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_selection_dev.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_widget_dev.h"
#include "native_client/src/third_party/ppapi/c/dev/ppp_zoom_dev.h"
#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "native_client/src/third_party/ppapi/c/ppp_instance.h"
#include "native_client/src/third_party/ppapi/c/private/ppb_uma_private.h"
#include "native_client/src/third_party/ppapi/cpp/dev/find_dev.h"
#include "native_client/src/third_party/ppapi/cpp/dev/printing_dev.h"
#include "native_client/src/third_party/ppapi/cpp/dev/scrollbar_dev.h"
#include "native_client/src/third_party/ppapi/cpp/dev/selection_dev.h"
#include "native_client/src/third_party/ppapi/cpp/dev/url_util_dev.h"
#include "native_client/src/third_party/ppapi/cpp/dev/widget_client_dev.h"
#include "native_client/src/third_party/ppapi/cpp/dev/zoom_dev.h"
#include "native_client/src/third_party/ppapi/cpp/image_data.h"
#include "native_client/src/third_party/ppapi/cpp/module.h"
#include "native_client/src/third_party/ppapi/cpp/rect.h"

using ppapi_proxy::BrowserPpp;

namespace plugin {

namespace {

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

// URL schemes that we treat in special ways.
const char* const kChromeExtensionUriScheme = "chrome-extension";
const char* const kDataUriScheme = "data";

// The key used to find the dictionary nexe URLs in the manifest file.
const char* const kNexesKey = "nexes";

bool UrlAsNaClDesc(void* obj, SrpcParams* params) {
  // TODO(sehr,polina): this API should take a selector specify which of
  // UMP, CORS, or traditional origin policy should be used.
  NaClSrpcArg** ins = params->ins();
  PLUGIN_PRINTF(("UrlAsNaClDesc (obj=%p, url=%s, callback=%p)\n",
                 obj, ins[0]->arrays.str, ins[1]->arrays.oval));

  PluginPpapi* plugin =
      static_cast<PluginPpapi*>(reinterpret_cast<Plugin*>(obj));
  const char* url = ins[0]->arrays.str;
  // TODO(sehr,polina): Ensure that origin checks are performed here.
  plugin->UrlAsNaClDesc(url, *reinterpret_cast<pp::Var*>(ins[1]->arrays.oval));
  return true;
}

bool GetLastError(void* obj, SrpcParams* params) {
  NaClSrpcArg** outs = params->outs();
  PLUGIN_PRINTF(("GetLastError (obj=%p)\n", obj));

  PluginPpapi* plugin =
      static_cast<PluginPpapi*>(reinterpret_cast<Plugin*>(obj));
  outs[0]->arrays.str = strdup(plugin->last_error_string().c_str());
  return true;
}

const int64_t kTimeSmallMin = 1;         // in ms
const int64_t kTimeSmallMax = 20000;     // in ms
const uint32_t kTimeSmallBuckets = 100;

const int64_t kSizeKBMin = 1;
const int64_t kSizeKBMax = 512*1024;     // very large .nexe
const uint32_t kSizeKBBuckets = 100;

const PPB_UMA_Private* GetUMAInterface() {
  pp::Module *module = pp::Module::Get();
  CHECK(module);
  return reinterpret_cast<const PPB_UMA_Private*>(
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
  explicit FindAdapter(PluginPpapi* plugin)
    : pp::Find_Dev(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_find_ = reinterpret_cast<const PPP_Find_Dev*>(
        proxy->GetPluginInterface(PPP_FIND_DEV_INTERFACE));
  }

  bool StartFind(const std::string& text, bool case_sensitive) {
    if (ppp_find_ != NULL) {
      PP_Bool pp_success =
          ppp_find_->StartFind(plugin_->pp_instance(),
                               text.c_str(),
                               pp::BoolToPPBool(case_sensitive));
      return pp_success == PP_TRUE;
    }
    return false;
  }

  void SelectFindResult(bool forward) {
    if (ppp_find_ != NULL) {
      ppp_find_->SelectFindResult(plugin_->pp_instance(),
                                  pp::BoolToPPBool(forward));
    }
  }

  void StopFind() {
    if (ppp_find_ != NULL)
      ppp_find_->StopFind(plugin_->pp_instance());
  }

 private:
  PluginPpapi* plugin_;
  const PPP_Find_Dev* ppp_find_;

  NACL_DISALLOW_COPY_AND_ASSIGN(FindAdapter);
};


// Derive a class from pp::Printing_Dev to forward PPP_Printing_Dev calls to
// the plugin.
class PrintingAdapter : public pp::Printing_Dev {
 public:
  explicit PrintingAdapter(PluginPpapi* plugin)
    : pp::Printing_Dev(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_printing_ = reinterpret_cast<const PPP_Printing_Dev*>(
        proxy->GetPluginInterface(PPP_PRINTING_DEV_INTERFACE));
  }

  PP_PrintOutputFormat_Dev*
      QuerySupportedPrintOutputFormats(uint32_t* format_count) {
    if (ppp_printing_ != NULL) {
      return ppp_printing_->QuerySupportedFormats(plugin_->pp_instance(),
                                                  format_count);
    }
    *format_count = 0;
    return NULL;
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

 private:
  PluginPpapi* plugin_;
  const PPP_Printing_Dev* ppp_printing_;

  NACL_DISALLOW_COPY_AND_ASSIGN(PrintingAdapter);
};


// Derive a class from pp::Selection_Dev to forward PPP_Selection_Dev calls to
// the plugin.
class SelectionAdapter : public pp::Selection_Dev {
 public:
  explicit SelectionAdapter(PluginPpapi* plugin)
    : pp::Selection_Dev(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_selection_ = reinterpret_cast<const PPP_Selection_Dev*>(
        proxy->GetPluginInterface(PPP_SELECTION_DEV_INTERFACE));
  }

  pp::Var GetSelectedText(bool html) {
    if (ppp_selection_ != NULL) {
      PP_Var var = ppp_selection_->GetSelectedText(plugin_->pp_instance(),
                                                   pp::BoolToPPBool(html));
      return pp::Var(pp::Var::PassRef(), var);
    }
    return pp::Var();
  }

 private:
  PluginPpapi* plugin_;
  const PPP_Selection_Dev* ppp_selection_;

  NACL_DISALLOW_COPY_AND_ASSIGN(SelectionAdapter);
};


// Derive a class from pp::WidgetClient_Dev to forward PPP_Widget_Dev
// and PPP_Scrollbar_Dev calls to the plugin.
class WidgetClientAdapter : public pp::WidgetClient_Dev {
 public:
  explicit WidgetClientAdapter(PluginPpapi* plugin)
    : pp::WidgetClient_Dev(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_widget_ = reinterpret_cast<const PPP_Widget_Dev*>(
        proxy->GetPluginInterface(PPP_WIDGET_DEV_INTERFACE));
    ppp_scrollbar_ = reinterpret_cast<const PPP_Scrollbar_Dev*>(
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

 private:
  PluginPpapi* plugin_;
  const PPP_Widget_Dev* ppp_widget_;
  const PPP_Scrollbar_Dev* ppp_scrollbar_;

  NACL_DISALLOW_COPY_AND_ASSIGN(WidgetClientAdapter);
};


// Derive a class from pp::Zoom_Dev to forward PPP_Zoom_Dev calls to
// the plugin.
class ZoomAdapter : public pp::Zoom_Dev {
 public:
  explicit ZoomAdapter(PluginPpapi* plugin)
    : pp::Zoom_Dev(plugin),
      plugin_(plugin) {
    BrowserPpp* proxy = plugin_->ppapi_proxy();
    CHECK(proxy != NULL);
    ppp_zoom_ = reinterpret_cast<const PPP_Zoom_Dev*>(
        proxy->GetPluginInterface(PPP_ZOOM_DEV_INTERFACE));
  }

  void Zoom(double factor, bool text_only) {
    if (ppp_zoom_ != NULL) {
      ppp_zoom_->Zoom(plugin_->pp_instance(),
                      factor,
                      pp::BoolToPPBool(text_only));
    }
  }

 private:
  PluginPpapi* plugin_;
  const PPP_Zoom_Dev* ppp_zoom_;

  NACL_DISALLOW_COPY_AND_ASSIGN(ZoomAdapter);
};

}  // namespace

class ProgressEvent {
 public:
  ProgressEvent(const char* event_type,
                Plugin::LengthComputable length_computable,
                uint64_t loaded_bytes,
                uint64_t total_bytes) :
    event_type_(event_type),
    length_computable_(length_computable),
    loaded_bytes_(loaded_bytes),
    total_bytes_(total_bytes) { }
  const char* event_type() const { return event_type_; }
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
  Plugin::LengthComputable length_computable_;
  uint64_t loaded_bytes_;
  uint64_t total_bytes_;
};

const char* const PluginPpapi::kNaClMIMEType = "application/x-nacl";

bool PluginPpapi::IsForeignMIMEType() const {
  return
      !mime_type().empty() &&
      mime_type() != kNaClMIMEType;
}


bool PluginPpapi::SetAsyncCallback(void* obj, SrpcParams* params) {
  PluginPpapi* plugin =
      static_cast<PluginPpapi*>(reinterpret_cast<Plugin*>(obj));
  if (plugin->main_service_runtime() == NULL) {
    params->set_exception_string("No subprocess running");
    return false;
  }
  if (plugin->receive_thread_running_) {
    params->set_exception_string("A callback has already been registered");
    return false;
  }
  AsyncNaClToJSThreadArgs* args = new(std::nothrow) AsyncNaClToJSThreadArgs;
  if (args == NULL) {
    params->set_exception_string("Memory allocation failed");
    return false;
  }
  args->callback = *reinterpret_cast<pp::Var*>(params->ins()[0]->arrays.oval);
  nacl::DescWrapper* socket =
      plugin->main_service_runtime()->async_receive_desc();
  NaClDescRef(socket->desc());
  // The MakeGeneric() call is necessary because the new DescWrapper
  // has a separate lifetime from the one returned by
  // async_receive_desc().  The new DescWrapper exists for the
  // lifetime of the child thread.
  args->socket.reset(plugin->wrapper_factory()->MakeGeneric(socket->desc()));

  // It would be nice if the thread interface did not require us to
  // specify a stack size.  This is fairly arbitrary.
  size_t stack_size = 128 << 10;
  NaClThreadCreateJoinable(&plugin->receive_thread_, AsyncNaClToJSThread, args,
                           stack_size);
  plugin->receive_thread_running_ = true;
  return true;
}

PluginPpapi* PluginPpapi::New(PP_Instance pp_instance) {
  PLUGIN_PRINTF(("PluginPpapi::New (pp_instance=%"NACL_PRId32")\n",
                 pp_instance));
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  if (!NaClHandlePassBrowserCtor()) {
    return NULL;
  }
#endif
  PluginPpapi* plugin = new(std::nothrow) PluginPpapi(pp_instance);
  PLUGIN_PRINTF(("PluginPpapi::New (plugin=%p)\n", static_cast<void*>(plugin)));
  if (plugin == NULL) {
    return NULL;
  }
  return plugin;
}

// All failures of this function will show up as "Missing Plugin-in", so
// there is no need to log to JS console that there was an initialization
// failure. Note that module loading functions will log their own errors.
bool PluginPpapi::Init(uint32_t argc, const char* argn[], const char* argv[]) {
  PLUGIN_PRINTF(("PluginPpapi::Init (argc=%"NACL_PRIu32")\n", argc));
  HistogramEnumerateOsArch(GetSandboxISA());
  init_time_ = NaClGetTimeOfDayMicroseconds();

  BrowserInterface* browser_interface =
      static_cast<BrowserInterface*>(new(std::nothrow) BrowserInterfacePpapi);
  if (browser_interface == NULL) {
    return false;
  }
  ScriptableHandle* handle = browser_interface->NewScriptableHandle(this);
  if (handle == NULL) {
    return false;
  }
  set_scriptable_handle(handle);
  PLUGIN_PRINTF(("PluginPpapi::Init (scriptable_handle=%p)\n",
                 static_cast<void*>(scriptable_handle())));
  url_util_ = pp::URLUtil_Dev::Get();
  if (url_util_ == NULL) {
    return false;
  }
  PLUGIN_PRINTF(("PluginPpapi::Init (url_util_=%p)\n",
                 static_cast<const void*>(url_util_)));

  bool status = Plugin::Init(
      browser_interface,
      PPInstanceToInstanceIdentifier(static_cast<pp::InstancePrivate*>(this)),
      static_cast<int>(argc),
      // TODO(polina): Can we change the args on our end to be const to
      // avoid these ugly casts? This will also require changes to npapi code.
      const_cast<char**>(argn),
      const_cast<char**>(argv));
  if (status) {
    const char* type_attr = LookupArgument(kTypeAttribute);
    if (type_attr != NULL) {
      mime_type_ = nacl::string(type_attr);
      std::transform(mime_type_.begin(), mime_type_.end(), mime_type_.begin(),
                     tolower);
    }

    const char* manifest_url = LookupArgument(kSrcManifestAttribute);
    // If the MIME type is foreign, then 'src' will be the URL for the content
    // and 'nacl' will be the URL for the manifest.
    if (IsForeignMIMEType())
        manifest_url = LookupArgument(kNaClManifestAttribute);
    // Use the document URL as the base for resolving relative URLs to find the
    // manifest.  This takes into account the setting of <base> tags that
    // precede the embed/object.
    CHECK(url_util_ != NULL);
    pp::Var base_var =
        url_util_->GetDocumentURL(
            *InstanceIdentifierToPPInstance(instance_id()));
    if (!base_var.is_string()) {
      PLUGIN_PRINTF(("PluginPpapi::Init (unable to find document url)\n"));
      return false;
    }
    set_plugin_base_url(base_var.AsString());
    if (manifest_url == NULL) {
      // TODO(sehr,polina): this should be a hard error when scripting
      // the src property is no longer allowed.
      PLUGIN_PRINTF(("PluginPpapi::Init:"
                     " WARNING: no 'src' property, so no manifest loaded.\n"));
      if (NULL != LookupArgument(kNaClManifestAttribute)) {
        PLUGIN_PRINTF(("PluginPpapi::Init:"
                       " WARNING: 'nacl' property is incorrect. Use 'src'.\n"));
      }
    } else {
      // Issue a GET for the manifest_url.  The manifest file will be parsed to
      // determine the nexe URL.
      // Sets src property to full manifest URL.
      RequestNaClManifest(manifest_url);
    }
  }

  if (ExperimentalJavaScriptApisAreEnabled()) {
    AddMethodCall(plugin::UrlAsNaClDesc, "__urlAsNaClDesc", "so", "");
    AddMethodCall(SetAsyncCallback, "__setAsyncCallback", "o", "");
  }

  // Export a property to allow us to get the last error description.
  AddPropertyGet(GetLastError, "lastError", "s");

  PLUGIN_PRINTF(("PluginPpapi::Init (status=%d)\n", status));
  return status;
}


PluginPpapi::PluginPpapi(PP_Instance pp_instance)
    : pp::InstancePrivate(pp_instance),
      last_error_string_(""),
      ppapi_proxy_(NULL),
      replayDidChangeView(false),
      replayHandleDocumentLoad(false),
      nexe_size_(0) {
  PLUGIN_PRINTF(("PluginPpapi::PluginPpapi (this=%p, pp_instance=%"
                 NACL_PRId32")\n", static_cast<void*>(this), pp_instance));
  NaClSrpcModuleInit();
  nexe_downloader_.Initialize(this);
  callback_factory_.Initialize(this);
}


PluginPpapi::~PluginPpapi() {
  PLUGIN_PRINTF(("PluginPpapi::~PluginPpapi (this=%p, scriptable_handle=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(scriptable_handle())));

#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  NaClHandlePassBrowserDtor();
#endif

  url_downloaders_.erase(url_downloaders_.begin(), url_downloaders_.end());

  ShutdownProxy();
  ScriptableHandle* scriptable_handle_ = scriptable_handle();
  ScriptableHandle::Unref(&scriptable_handle_);
  NaClSrpcModuleFini();
}


void PluginPpapi::DidChangeView(const pp::Rect& position,
                                const pp::Rect& clip) {
  PLUGIN_PRINTF(("PluginPpapi::DidChangeView (this=%p)\n",
                 static_cast<void*>(this)));

  if (ppapi_proxy_ == NULL) {
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


void PluginPpapi::DidChangeFocus(bool has_focus) {
  PLUGIN_PRINTF(("PluginPpapi::DidChangeFocus (this=%p)\n",
                 static_cast<void*>(this)));
  if (ppapi_proxy_ == NULL) {
    return;
  } else {
    ppapi_proxy_->ppp_instance_interface()->DidChangeFocus(
        pp_instance(), pp::BoolToPPBool(has_focus));
  }
}


bool PluginPpapi::HandleInputEvent(const PP_InputEvent& event) {
  PLUGIN_PRINTF(("PluginPpapi::HandleInputEvent (this=%p)\n",
                 static_cast<void*>(this)));
  if (ppapi_proxy_ == NULL) {
    return false;  // event is not handled here.
  } else {
    return pp::PPBoolToBool(
      ppapi_proxy_->ppp_instance_interface()->HandleInputEvent(
          pp_instance(), &event));
  }
}


bool PluginPpapi::HandleDocumentLoad(const pp::URLLoader& url_loader) {
  PLUGIN_PRINTF(("PluginPpapi::HandleDocumentLoad (this=%p)\n",
                 static_cast<void*>(this)));
  if (ppapi_proxy_ == NULL) {
    // Store this event and replay it when the proxy becomes available.
    replayHandleDocumentLoad = true;
    replayHandleDocumentLoadURLLoader = url_loader;
    // Return true so that the browser keeps servicing this loader so we can
    // perform requests on it later.
    return true;
  } else {
    return pp::PPBoolToBool(
        ppapi_proxy_->ppp_instance_interface()->HandleDocumentLoad(
            pp_instance(), url_loader.pp_resource()));
  }
}


void PluginPpapi::HandleMessage(const pp::Var& message) {
  PLUGIN_PRINTF(("PluginPpapi::HandleMessage (this=%p)\n",
                 static_cast<void*>(this)));
  if (ppapi_proxy_ != NULL &&
      ppapi_proxy_->ppp_messaging_interface() != NULL) {
    ppapi_proxy_->ppp_messaging_interface()->HandleMessage(
        pp_instance(), message.pp_var());
  }
}


pp::Var PluginPpapi::GetInstanceObject() {
  PLUGIN_PRINTF(("PluginPpapi::GetInstanceObject (this=%p)\n",
                 static_cast<void*>(this)));
  // The browser will unref when it discards the var for this object.
  ScriptableHandlePpapi* handle =
      static_cast<ScriptableHandlePpapi*>(scriptable_handle()->AddRef());
  pp::Var* handle_var = handle->var();
  PLUGIN_PRINTF(("PluginPpapi::GetInstanceObject (handle=%p, handle_var=%p)\n",
                 static_cast<void*>(handle), static_cast<void*>(handle_var)));
  return *handle_var;  // make a copy
}


void PluginPpapi::NexeFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PluginPpapi::NexeFileDidOpen (pp_error=%"NACL_PRId32")\n",
                 pp_error));
  int32_t file_desc = nexe_downloader_.GetPOSIXFileDescriptor();
  PLUGIN_PRINTF(("PluginPpapi::NexeFileDidOpen (file_desc=%"NACL_PRId32")\n",
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
  if (!IsValidNexeOrigin(nexe_downloader_.url(), &error_info)) {
    ReportLoadError(error_info);
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
  HistogramTimeSmall("NaCl.Perf.DownloadTime.Nexe",
                     nexe_downloader_.TimeSinceOpenMilliseconds());

  // Inform JavaScript that we successfully loaded the manifest file.
  EnqueueProgressEvent("progress",
                       LENGTH_IS_COMPUTABLE,
                       nexe_bytes_read,
                       nexe_bytes_read);
  nacl::scoped_ptr<nacl::DescWrapper>
      wrapper(wrapper_factory()->MakeFileDesc(file_desc_ok_to_close, O_RDONLY));
  bool was_successful = LoadNaClModule(wrapper.get(), &error_info);
  if (was_successful) {
    ReportLoadSuccess(LENGTH_IS_COMPUTABLE, nexe_bytes_read, nexe_bytes_read);
  } else {
    ReportLoadError(error_info);
  }
}


bool PluginPpapi::StartProxiedExecution(NaClSrpcChannel* srpc_channel,
                                        ErrorInfo* error_info) {
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (srpc_channel=%p)\n",
                 reinterpret_cast<void*>(srpc_channel)));

  // TODO(elijahtaylor) nexe_size_ should never be less than zero, revisit when
  // dynamic loading is more mature.
  // http://code.google.com/p/nativeclient/issues/detail?id=1534
  if (init_time_ > 0 && nexe_size_ > 0) {
    float dt = static_cast<float>(
        (NaClGetTimeOfDayMicroseconds() - init_time_) / NACL_MICROS_PER_MILLI);
    float size_in_MB = static_cast<float>(nexe_size_) / 1024.f / 1024.f;
    HistogramTimeSmall("NaCl.Perf.StartupTime.PerMB",
                       static_cast<int64_t>(dt / size_in_MB));
    HistogramTimeSmall("NaCl.Perf.StartupTime.Total",
                       static_cast<int64_t>(dt));
  }

  // Check that the .nexe exports the PPAPI intialization method.
  NaClSrpcService* client_service = srpc_channel->client;
  if (NaClSrpcServiceMethodIndex(client_service,
                                 "PPP_InitializeModule:iihs:ii") ==
      kNaClSrpcInvalidMethodIndex) {
    error_info->SetReport(
        ERROR_START_PROXY,
        "could not find PPP_InitializeModule() - toolchain version mismatch?");
    PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (%s)\n",
                   error_info->message().c_str()));
    return false;
  }
  nacl::scoped_ptr<BrowserPpp> ppapi_proxy(
      new(std::nothrow) BrowserPpp(srpc_channel, this));
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (ppapi_proxy=%p)\n",
                 reinterpret_cast<void*>(ppapi_proxy.get())));
  if (ppapi_proxy.get() == NULL) {
    error_info->SetReport(ERROR_START_PROXY,
                          "could not allocate proxy memory.");
    return false;
  }
  pp::Module* module = pp::Module::Get();
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (module=%p)\n",
                 reinterpret_cast<void*>(module)));
  CHECK(module != NULL);  // We could not have gotten past init stage otherwise.
  int32_t pp_error =
      ppapi_proxy->InitializeModule(module->pp_module(),
                                     module->get_browser_interface());
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {
    error_info->SetReport(ERROR_START_PROXY,
                          "could not initialize module.");
    return false;
  }
  const PPP_Instance* instance_interface =
      ppapi_proxy->ppp_instance_interface();
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (ppp_instance=%p)\n",
                 reinterpret_cast<const void*>(instance_interface)));
  CHECK(instance_interface != NULL);  // Verified on module initialization.
  PP_Bool did_create = instance_interface->DidCreate(
      pp_instance(),
      argc(),
      const_cast<const char**>(argn()),
      const_cast<const char**>(argv()));
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (did_create=%d)\n",
                 did_create));
  if (did_create == PP_FALSE) {
    error_info->SetReport(ERROR_START_PROXY,
                          "could not create instance.");
    return false;
  }

  ppapi_proxy_ = ppapi_proxy.release();

// TODO(dmichael):  Remove the scripting proxy code entirely.
#ifndef PPAPI_INSTANCE_REMOVE_SCRIPTING
  ScriptableHandlePpapi* handle =
      static_cast<ScriptableHandlePpapi*>(scriptable_handle());
  PP_Var scriptable_proxy =
      instance_interface->GetInstanceObject(pp_instance());
  handle->set_scriptable_proxy(pp::Var(pp::Var::PassRef(), scriptable_proxy));
#endif

  // Create PPP* interface adapters to forward calls to .nexe.
  find_adapter_.reset(new(std::nothrow) FindAdapter(this));
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
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (success=true)\n"));
  return true;
}


void PluginPpapi::ShutdownProxy() {
  PLUGIN_PRINTF(("PluginPpapi::ShutdownProxy (ppapi_proxy=%p)\n",
                reinterpret_cast<void*>(ppapi_proxy_)));
  if (ppapi_proxy_ != NULL) {
    ppapi_proxy_->ShutdownModule();
    delete ppapi_proxy_;
    ppapi_proxy_ = NULL;
  }
}

void PluginPpapi::NaClManifestBufferReady(int32_t pp_error) {
  PLUGIN_PRINTF(("PluginPpapi::NaClManifestBufferReady (pp_error=%"
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

void PluginPpapi::NaClManifestFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  HistogramTimeSmall("NaCl.Perf.DownloadTime.Manifest",
                     nexe_downloader_.TimeSinceOpenMilliseconds());
  ErrorInfo error_info;
  // The manifest file was successfully opened.  Set the src property on the
  // plugin now, so that the full url is available to error handlers.
  set_manifest_url(nexe_downloader_.url());
  int32_t file_desc = nexe_downloader_.GetPOSIXFileDescriptor();
  PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen (file_desc=%"
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
  PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen "
                 "(dup_file_desc=%"NACL_PRId32", json_file=%p)\n",
                 dup_file_desc, reinterpret_cast<void*>(json_file)));
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
      PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen failed: "
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

void PluginPpapi::ProcessNaClManifest(const nacl::string& manifest_json) {
  HistogramSizeKB("NaCl.Perf.Size.Manifest",
                  static_cast<int32_t>(manifest_json.length() / 1024));

  nacl::string nexe_url;
  ErrorInfo error_info;
  if (!SetManifestObject(manifest_json, &error_info)) {
    ReportLoadError(error_info);
    return;
  }
  if (!SelectNexeURLFromManifest(&nexe_url, &error_info)) {
    ReportLoadError(error_info);
    return;
  }
  set_nacl_ready_state(LOADING);
  // Inform JavaScript that we found a nexe URL to load.
  EnqueueProgressEvent("progress",
                       LENGTH_IS_NOT_COMPUTABLE,
                       kUnknownBytes,
                       kUnknownBytes);
  pp::CompletionCallback open_callback =
      callback_factory_.NewCallback(&PluginPpapi::NexeFileDidOpen);
  // Will always call the callback on success or failure.
  nexe_downloader_.Open(nexe_url, DOWNLOAD_TO_FILE, open_callback);
}

void PluginPpapi::RequestNaClManifest(const nacl::string& url) {
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClManifest (url='%s')\n", url.c_str()));
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClManifest (plugin base url='%s')\n",
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
  set_manifest_base_url(nmf_resolved_url.AsString());
  set_manifest_url(url);
  // Inform JavaScript that a load is starting.
  set_nacl_ready_state(OPENED);
  EnqueueProgressEvent("loadstart",
                       LENGTH_IS_NOT_COMPUTABLE,
                       kUnknownBytes,
                       kUnknownBytes);
  bool is_data_uri = GetUrlScheme(nmf_resolved_url.AsString()) == SCHEME_DATA;
  HistogramEnumerateManifestIsDataURI(static_cast<int>(is_data_uri));
  if (is_data_uri) {
    pp::CompletionCallback open_callback =
        callback_factory_.NewCallback(&PluginPpapi::NaClManifestBufferReady);
    // Will always call the callback on success or failure.
    nexe_downloader_.Open(nmf_resolved_url.AsString(),
                          DOWNLOAD_TO_BUFFER,
                          open_callback);
  } else {
    pp::CompletionCallback open_callback =
        callback_factory_.NewCallback(&PluginPpapi::NaClManifestFileDidOpen);
    // Will always call the callback on success or failure.
    nexe_downloader_.Open(nmf_resolved_url.AsString(),
                          DOWNLOAD_TO_FILE,
                          open_callback);
  }
}


bool PluginPpapi::SetManifestObject(const nacl::string& manifest_json,
                                    ErrorInfo* error_info) {
  PLUGIN_PRINTF(("PluginPpapi::SetManifestObject(): manifest_json='%s'.\n",
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

bool PluginPpapi::SelectNexeURLFromManifest(nacl::string* result,
                                            ErrorInfo* error_info) {
  const nacl::string sandbox_isa(GetSandboxISA());
  PLUGIN_PRINTF(("PluginPpapi::SelectNexeURLFromManifest(): sandbox='%s'.\n",
       sandbox_isa.c_str()));
  if (result == NULL || error_info == NULL || manifest_ == NULL)
    return false;
  return manifest_->GetNexeURL(result, error_info);
}


void PluginPpapi::UrlDidOpenForUrlAsNaClDesc(int32_t pp_error,
                                             FileDownloader*& url_downloader,
                                             pp::VarPrivate& js_callback) {
  PLUGIN_PRINTF(("PluginPpapi::UrlDidOpenForUrlAsNaClDesc "
                 "(pp_error=%"NACL_PRId32", url_downloader=%p)\n",
                 pp_error, reinterpret_cast<void*>(url_downloader)));
  url_downloaders_.erase(url_downloader);
  nacl::scoped_ptr<FileDownloader> scoped_url_downloader(url_downloader);

  if (pp_error != PP_OK) {
    js_callback.Call(pp::Var("onfail"), pp::Var("URL get failed"));
    return;
  }
  int32_t file_desc = scoped_url_downloader->GetPOSIXFileDescriptor();
  int32_t file_desc_ok_to_close = DUP(file_desc);
  if (file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    js_callback.Call(pp::Var("onfail"), pp::Var("posix desc or dup failed"));
    return;
  }
  nacl::scoped_ptr<nacl::DescWrapper> scoped_desc_wrapper(
      wrapper_factory()->MakeFileDesc(
          file_desc_ok_to_close, NACL_ABI_O_RDONLY));
  if (scoped_desc_wrapper.get() == NULL) {
    // MakeFileDesc() already closed the file descriptor.
    js_callback.Call(pp::Var("onfail"), pp::Var("nacl desc wrapper failed"));
    return;
  }

  ScriptableHandlePpapi* handle = ScriptableHandlePpapi::New(
      DescBasedHandle::New(plugin(), scoped_desc_wrapper.get()));
  if (handle == NULL) {
    js_callback.Call(pp::Var("onfail"), pp::Var("scriptable handle failed"));
    return;
  }
  // We succeeded, so do not unref the wrapper!
  (void)(scoped_desc_wrapper.release());

  js_callback.Call(pp::Var("onload"), pp::VarPrivate(this, handle));
}


void PluginPpapi::UrlDidOpenForStreamAsFile(int32_t pp_error,
                                            FileDownloader*& url_downloader,
                                            PP_CompletionCallback callback) {
  PLUGIN_PRINTF(("PluginPpapi::UrlDidOpen (pp_error=%"NACL_PRId32
                 ", url_downloader=%p)\n", pp_error,
                 reinterpret_cast<void*>(url_downloader)));
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


int32_t PluginPpapi::GetPOSIXFileDesc(const nacl::string& url) {
  PLUGIN_PRINTF(("PluginPpapi::GetFileDesc (url=%s)\n", url.c_str()));
  int32_t file_desc_ok_to_close = NACL_NO_FILE_DESC;
  std::map<nacl::string, int32_t>::iterator it = url_fd_map_.find(url);
  if (it != url_fd_map_.end())
    file_desc_ok_to_close = DUP(it->second);
  return file_desc_ok_to_close;
}

// TODO(polina): reduce code duplication between UrlAsNaClDesc and StreamAsFile.

void PluginPpapi::UrlAsNaClDesc(const nacl::string& url,
                                pp::VarPrivate js_callback) {
  PLUGIN_PRINTF(("PluginPpapi::UrlAsNaClDesc (url='%s')\n", url.c_str()));
  FileDownloader* downloader = new FileDownloader();
  downloader->Initialize(this);
  url_downloaders_.insert(downloader);
  pp::CompletionCallback open_callback =
      callback_factory_.NewCallback(&PluginPpapi::UrlDidOpenForUrlAsNaClDesc,
                                    downloader,
                                    js_callback);
  // Untrusted loads are always relative to the page's origin.
  CHECK(url_util_ != NULL);
  pp::Var resolved_url =
      url_util_->ResolveRelativeToURL(pp::Var(plugin_base_url()), url);
  if (!resolved_url.is_string()) {
    PLUGIN_PRINTF(("PluginPpapi::UrlAsNaClDesc: "
                   "could not resolve url \"%s\" relative to base \"%s\".",
                   url.c_str(),
                   plugin_base_url().c_str()));
    return;
  }
  // Will always call the callback on success or failure.
  downloader->Open(url, DOWNLOAD_TO_FILE, open_callback);
}


bool PluginPpapi::StreamAsFile(const nacl::string& url,
                               PP_CompletionCallback callback) {
  PLUGIN_PRINTF(("PluginPpapi::StreamAsFile (url='%s')\n", url.c_str()));
  FileDownloader* downloader = new FileDownloader();
  downloader->Initialize(this);
  url_downloaders_.insert(downloader);
  pp::CompletionCallback open_callback =
      callback_factory_.NewCallback(&PluginPpapi::UrlDidOpenForStreamAsFile,
                                    downloader,
                                    callback);
  // Untrusted loads are always relative to the page's origin.
  CHECK(url_util_ != NULL);
  pp::Var resolved_url =
      url_util_->ResolveRelativeToURL(pp::Var(plugin_base_url()), url);
  if (!resolved_url.is_string()) {
    PLUGIN_PRINTF(("PluginPpapi::StreamAsFile: "
                   "could not resolve url \"%s\" relative to page \"%s\".",
                   url.c_str(),
                   origin().c_str()));
    return false;
  }
  // Will always call the callback on success or failure.
  return downloader->Open(url, DOWNLOAD_TO_FILE, open_callback);
}


void PluginPpapi::ReportLoadSuccess(LengthComputable length_computable,
                                    uint64_t loaded_bytes,
                                    uint64_t total_bytes) {
  // Set the readyState attribute to indicate loaded.
  set_nacl_ready_state(DONE);
  // Inform JavaScript that loading was successful and is complete.
  // Note that these events will be dispatched, as will failure events, when
  // this code is reached from __launchExecutableFromFd also.
  // TODO(sehr,polina): Remove comment when experimental APIs are removed.
  EnqueueProgressEvent("load", length_computable, loaded_bytes, total_bytes);
  EnqueueProgressEvent("loadend", length_computable, loaded_bytes, total_bytes);

  // UMA
  HistogramEnumerateLoadStatus(ERROR_LOAD_SUCCESS);
}


// TODO(ncbray): report UMA stats
void PluginPpapi::ReportLoadError(const ErrorInfo& error_info) {
  PLUGIN_PRINTF(("PluginPpapi::ReportLoadError (error='%s')\n",
                 error_info.message().c_str()));
  // Set the readyState attribute to indicate we need to start over.
  set_nacl_ready_state(DONE);
  // Report an error in lastError and on the JavaScript console.
  nacl::string message = nacl::string("NaCl module load failed: ") +
      error_info.message();
  set_last_error_string(message);
  browser_interface()->AddToConsole(instance_id(), message);
  ShutdownProxy();
  // Inform JavaScript that loading encountered an error and is complete.
  EnqueueProgressEvent("error",
                       LENGTH_IS_NOT_COMPUTABLE,
                       kUnknownBytes,
                       kUnknownBytes);
  EnqueueProgressEvent("loadend",
                       LENGTH_IS_NOT_COMPUTABLE,
                       kUnknownBytes,
                       kUnknownBytes);

  // UMA
  HistogramEnumerateLoadStatus(error_info.error_code());
}


void PluginPpapi::ReportLoadAbort() {
  PLUGIN_PRINTF(("PluginPpapi::ReportLoadAbort\n"));
  // Set the readyState attribute to indicate we need to start over.
  set_nacl_ready_state(DONE);
  // Report an error in lastError and on the JavaScript console.
  nacl::string error_string("NaCl module load failed: user aborted");
  set_last_error_string(error_string);
  browser_interface()->AddToConsole(instance_id(), error_string);
  ShutdownProxy();
  // Inform JavaScript that loading was aborted and is complete.
  EnqueueProgressEvent("abort",
                       LENGTH_IS_NOT_COMPUTABLE,
                       kUnknownBytes,
                       kUnknownBytes);
  EnqueueProgressEvent("loadend",
                       LENGTH_IS_NOT_COMPUTABLE,
                       kUnknownBytes,
                       kUnknownBytes);

  // UMA
  HistogramEnumerateLoadStatus(ERROR_LOAD_ABORTED);
}


void PluginPpapi::EnqueueProgressEvent(const char* event_type,
                                       LengthComputable length_computable,
                                       uint64_t loaded_bytes,
                                       uint64_t total_bytes) {
  PLUGIN_PRINTF(("PluginPpapi::EnqueueProgressEvent ("
                 "event_type='%s', length_computable=%d, "
                 "loaded=%"NACL_PRIu64", total=%"NACL_PRIu64")\n",
                 event_type,
                 static_cast<int>(length_computable),
                 loaded_bytes,
                 total_bytes));

  progress_events_.push(new ProgressEvent(event_type,
                                          length_computable,
                                          loaded_bytes,
                                          total_bytes));
  // Note that using callback_factory_ in this way is not thread safe.
  // If/when EnqueueProgressEvent is callable from another thread, this
  // will need to change.
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&PluginPpapi::DispatchProgressEvent);
  pp::Core* core = pp::Module::Get()->core();
  core->CallOnMainThread(0, callback, 0);
}

void PluginPpapi::ReportSelLdrLoadStatus(int status) {
  HistogramEnumerateSelLdrLoadStatus(static_cast<NaClErrorCode>(status));
}

void PluginPpapi::DispatchProgressEvent(int32_t result) {
  PLUGIN_PRINTF(("PluginPpapi::DispatchProgressEvent (result=%"
                 NACL_PRId32")\n", result));
  if (result < 0) {
    return;
  }
  if (progress_events_.empty()) {
    PLUGIN_PRINTF(("PluginPpapi::DispatchProgressEvent: no pending events\n"));
    return;
  }
  nacl::scoped_ptr<ProgressEvent> event(progress_events_.front());
  progress_events_.pop();
  PLUGIN_PRINTF(("PluginPpapi::DispatchProgressEvent ("
                 "event_type='%s', length_computable=%d, "
                 "loaded=%"NACL_PRIu64", total=%"NACL_PRIu64")\n",
                 event->event_type(),
                 static_cast<int>(event->length_computable()),
                 event->loaded_bytes(),
                 event->total_bytes()));

  static const char* kEventClosureJS =
      "(function(target, type, lengthComputable, loadedBytes, totalBytes) {"
      "    var progress_event = document.createEvent('ProgressEvent');"
      "    progress_event.initProgressEvent(type, false, true,"
      "                                     lengthComputable,"
      "                                     loadedBytes,"
      "                                     totalBytes);"
      "    target.dispatchEvent(progress_event);"
      "})";

  // Create a function object by evaluating the JavaScript text.
  // TODO(sehr, polina): We should probably cache the created function object to
  // avoid JavaScript reparsing.
  pp::VarPrivate exception;
  pp::VarPrivate function_object = ExecuteScript(kEventClosureJS, &exception);
  if (!exception.is_undefined() || !function_object.is_object()) {
    PLUGIN_PRINTF(("PluginPpapi::DispatchProgressEvent:"
                   " Function object creation failed.\n"));
    return;
  }
  // Get the target of the event to be dispatched.
  pp::Var owner_element_object = GetOwnerElementObject();
  if (!owner_element_object.is_object()) {
    PLUGIN_PRINTF(("PluginPpapi::DispatchProgressEvent:"
                   " Couldn't get owner element object.\n"));
    NACL_NOTREACHED();
    return;
  }

  pp::Var argv[5];
  static const uint32_t argc = NACL_ARRAY_SIZE(argv);
  argv[0] = owner_element_object;
  argv[1] = pp::Var(event->event_type());
  argv[2] = pp::Var(event->length_computable() == LENGTH_IS_COMPUTABLE);
  argv[3] = pp::Var(static_cast<double>(event->loaded_bytes()));
  argv[4] = pp::Var(static_cast<double>(event->total_bytes()));

  // Dispatch the event.
  const pp::Var default_method;
  function_object.Call(default_method, argc, argv, &exception);
  if (!exception.is_undefined()) {
    PLUGIN_PRINTF(("PluginPpapi::DispatchProgressEvent:"
                   " event dispatch failed.\n"));
  }
}

UrlSchemeType PluginPpapi::GetUrlScheme(const std::string& url) {
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
