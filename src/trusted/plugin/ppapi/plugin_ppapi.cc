/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"

#include <algorithm>
#include <stdio.h>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/ppapi/async_receive.h"
#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"

#include "ppapi/c/dev/ppp_find_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/dev/ppp_scrollbar_dev.h"
#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/c/dev/ppp_widget_dev.h"
#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/find_dev.h"
#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/dev/scrollbar_dev.h"
#include "ppapi/cpp/dev/selection_dev.h"
#include "ppapi/cpp/dev/widget_client_dev.h"
#include "ppapi/cpp/dev/zoom_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"

namespace plugin {

namespace {

const char* const kTypeAttribute = "type";
// The "nacl" attribute of the <embed> tag.  The value is expected to be either
// a URL or URI pointing to the manifest file (which is expected to contain
// JSON matching ISAs with .nexe URLs).
const char* const kNaclManifestAttribute = "nacl";
// This is a pretty arbitrary limit on the byte size of the NaCl manfest file.
// Note that the resulting string object has to have at least one byte extra
// for the null termination character.
const ssize_t kNaclManifestMaxFileBytesPlusNull = 1024;
const ssize_t kNaclManifestMaxFileBytesNoNull =
    kNaclManifestMaxFileBytesPlusNull - 1;
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

// Derive a class from pp::Find_Dev to forward PPP_Find_Dev calls to
// the plugin.
class FindAdapter : public pp::Find_Dev {
 public:
  explicit FindAdapter(PluginPpapi* plugin)
    : pp::Find_Dev(plugin),
      plugin_(plugin) {
    ppapi_proxy::BrowserPpp* proxy = plugin_->ppapi_proxy();
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
    ppapi_proxy::BrowserPpp* proxy = plugin_->ppapi_proxy();
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
    ppapi_proxy::BrowserPpp* proxy = plugin_->ppapi_proxy();
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
    ppapi_proxy::BrowserPpp* proxy = plugin_->ppapi_proxy();
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
    ppapi_proxy::BrowserPpp* proxy = plugin_->ppapi_proxy();
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

const char* const PluginPpapi::kNaClMIMEType = "application/x-nacl";

bool PluginPpapi::SetAsyncCallback(void* obj, SrpcParams* params) {
  PluginPpapi* plugin =
      static_cast<PluginPpapi*>(reinterpret_cast<Plugin*>(obj));
  if (plugin->service_runtime_ == NULL) {
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
  nacl::DescWrapper* socket = plugin->service_runtime_->async_receive_desc();
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

  bool status = Plugin::Init(
      browser_interface,
      PPInstanceToInstanceIdentifier(static_cast<pp::Instance*>(this)),
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
    const char* nacl_attr = LookupArgument(kNaclManifestAttribute);
    PLUGIN_PRINTF(("PluginPpapi::Init (nacl_attr=%s)\n", nacl_attr));
    if (nacl_attr != NULL) {
      // Issue a GET for the "nacl" attribute.  The value of the attribute
      // can be a URI or a URL pointing to the manifest file. The manifest
      // file will be parsed to determine the nexe URL.
      // Sets nacl property to full manifest URL.
      RequestNaClManifest(nacl_attr);
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
    : pp::Instance(pp_instance),
      last_error_string_(""),
      ppapi_proxy_(NULL),
      replayDidChangeView(false) {
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
  UnrefScriptableHandle(&scriptable_handle_);
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
    return false;
  } else {
    return pp::PPBoolToBool(
        ppapi_proxy_->ppp_instance_interface()->HandleDocumentLoad(
            pp_instance(), url_loader.pp_resource()));
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
  if (pp_error != PP_OK || file_desc == NACL_NO_FILE_DESC) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else {
      ReportLoadError("could not load nexe url.");
    }
    return;
  }
  nacl::string error_string;
  if (!IsValidNexeOrigin(nexe_downloader_.url(),
                         NACL_NO_FILE_PATH,
                         &error_string)) {
    ReportLoadError(error_string);
    return;
  }
  int32_t file_desc_ok_to_close = DUP(file_desc);
  if (file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    ReportLoadError("could not duplicate loaded file handle.");
    return;
  }
  // Inform JavaScript that we successfully loaded the manifest file.
  DispatchProgressEvent("progress", false, kUnknownBytes, kUnknownBytes);
  nacl::scoped_ptr<nacl::DescWrapper>
      wrapper(wrapper_factory()->MakeFileDesc(file_desc_ok_to_close, O_RDONLY));
  bool was_successful = LoadNaClModule(wrapper.get(), &error_string);
  if (was_successful) {
    ReportLoadSuccess();
    // Set the __moduleReady attribute to indicate ready to start.
    set_nacl_module_ready(true);
  } else {
    ReportLoadError(error_string);
  }
}


bool PluginPpapi::StartProxiedExecution(NaClSrpcChannel* srpc_channel,
                                        nacl::string* error_string) {
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (srpc_channel=%p)\n",
                 reinterpret_cast<void*>(srpc_channel)));
  // Check that the .nexe exports the PPAPI intialization method.
  NaClSrpcService* client_service = srpc_channel->client;
  if (NaClSrpcServiceMethodIndex(client_service,
                                 "PPP_InitializeModule:iihs:ii") ==
      kNaClSrpcInvalidMethodIndex) {
    *error_string =
        "could not find PPP_InitializeModule() - toolchain version mismatch?";
    return false;
  }
  ppapi_proxy_ =
      new(std::nothrow) ppapi_proxy::BrowserPpp(srpc_channel, this);
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (ppapi_proxy=%p)\n",
                 reinterpret_cast<void*>(ppapi_proxy_)));
  if (ppapi_proxy_ == NULL) {
    *error_string = "could not allocate proxy memory.";
    return false;
  }
  pp::Module* module = pp::Module::Get();
  CHECK(module != NULL);  // We could not have gotten past init stage otherwise.
  int32_t pp_error =
      ppapi_proxy_->InitializeModule(module->pp_module(),
                                     module->get_browser_interface());
  if (pp_error != PP_OK) {
    *error_string = "could not initialize module.";
    return false;
  }
  const PPP_Instance* instance_interface =
      ppapi_proxy_->ppp_instance_interface();
  CHECK(instance_interface != NULL);  // Verified on module initialization.
  PP_Bool did_create = instance_interface->DidCreate(
      pp_instance(),
      argc(),
      const_cast<const char**>(argn()),
      const_cast<const char**>(argv()));
  if (did_create == PP_FALSE) {
    *error_string = "could not create instance.";
    return false;
  }

  ScriptableHandlePpapi* handle =
      static_cast<ScriptableHandlePpapi*>(scriptable_handle());
  PP_Var scriptable_proxy =
      instance_interface->GetInstanceObject(pp_instance());
  handle->set_scriptable_proxy(pp::Var(pp::Var::PassRef(), scriptable_proxy));

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


void PluginPpapi::NaClManifestFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  // The manifest file was successfully opened.  Set the __nacl property on the
  // plugin now, so that the full url is available to error handlers.
  set_nacl_manifest_url(nexe_downloader_.url());
  int32_t file_desc = nexe_downloader_.GetPOSIXFileDescriptor();
  PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen (file_desc=%"
                 NACL_PRId32")\n", file_desc));
  if (pp_error != PP_OK || file_desc == NACL_NO_FILE_DESC) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else {
      ReportLoadError("could not load manifest url.");
    }
    return;
  }
  // Duplicate the file descriptor in order to create a FILE stream with it
  // that can later be closed without closing the original descriptor.  The
  // browser will take care of the original descriptor.
  int dup_file_desc = DUP(file_desc);
  FILE* json_file = fdopen(dup_file_desc, "rb");
  PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen "
                 "(dup_file_desc=%"NACL_PRId32", json_file=%p)\n",
                 dup_file_desc, reinterpret_cast<void*>(json_file)));
  if (json_file == NULL) {
    CLOSE(dup_file_desc);
    ReportLoadError("could not open manifest file.");
    return;
  }
  nacl::scoped_array<char> json_buffer(
      new char[kNaclManifestMaxFileBytesPlusNull]);
  if (json_buffer == NULL) {
    fclose(json_file);
    ReportLoadError("could not allocate manifest memory.");
    return;
  }
  size_t read_byte_count = fread(json_buffer.get(),
                                 sizeof(char),
                                 kNaclManifestMaxFileBytesNoNull,
                                 json_file);
  bool read_error = (ferror(json_file) != 0);
  bool file_too_large = (feof(json_file) == 0);
  // Once the bytes are read, the FILE is no longer needed, so close it.  This
  // allows for early returns without leaking the |json_file| FILE object.
  fclose(json_file);
  if (read_error || file_too_large) {
    // No need to close |file_desc|, that is handled by |nexe_downloader_|.
    PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen failed: "
                   "read_error=%d file_too_large=%d "
                   "read_byte_count=%"NACL_PRIuS"\n",
                   read_error, file_too_large,
                   read_byte_count));
    ReportLoadError("could not read manifest file.");
    return;
  }
  json_buffer[read_byte_count] = '\0';  // Force null termination.
  nacl::string nexe_url;
  if (SelectNexeURLFromManifest(json_buffer.get(), &nexe_url)) {
    PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen (nexe_url=%s)\n",
                   nexe_url.c_str()));
    // Inform JavaScript that we found a nexe URL to load.
    DispatchProgressEvent("progress", false, kUnknownBytes, kUnknownBytes);
    pp::CompletionCallback open_callback =
        callback_factory_.NewCallback(&PluginPpapi::NexeFileDidOpen);
    // Will always call the callback on success or failure.
    nexe_downloader_.Open(nexe_url, open_callback);
    return;
  }
  ReportLoadError("could not select from manifest file.");
}


void PluginPpapi::RequestNaClManifest(const nacl::string& url) {
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClManifest (url='%s')\n", url.c_str()));
  // Inform JavaScript that a load is starting.
  DispatchProgressEvent("loadstart", false, kUnknownBytes, kUnknownBytes);
  pp::CompletionCallback open_callback =
      callback_factory_.NewCallback(&PluginPpapi::NaClManifestFileDidOpen);
  // Will always call the callback on success or failure.
  nexe_downloader_.Open(url, open_callback);
}


bool PluginPpapi::SelectNexeURLFromManifest(
    const nacl::string& nexe_manifest_json, nacl::string* result) {
  const nacl::string sandbox_isa(GetSandboxISA());
  PLUGIN_PRINTF(
      ("GetNexeURLFromManifest(): sandbox='%s' nexe_manifest_json='%s'.\n",
       sandbox_isa.c_str(), nexe_manifest_json.c_str()));
  if (result == NULL)
    return false;
  // Parse the JSON via the browser.
  pp::Var exception;
  pp::Var json_parser = GetWindowObject().GetProperty("JSON", &exception);
  if (!exception.is_undefined()) {
    return false;
  }
  pp::Var manifest_root =
      json_parser.Call("parse", nexe_manifest_json, &exception);
  if (!exception.is_undefined()) {
    return false;
  }
  if (!manifest_root.HasProperty(kNexesKey)) {
    return false;
  }
  pp::Var nexes_dict = manifest_root.GetProperty(kNexesKey);
  // Look for a key with the same name as the ISA string.
  if (!nexes_dict.HasProperty(sandbox_isa)) {
    return false;
  }
  pp::Var nexe_url = nexes_dict.GetProperty(sandbox_isa);
  if (!nexe_url.is_string()) {
    return false;
  }
  *result = nexe_url.AsString();
  return true;
}


void PluginPpapi::UrlDidOpenForUrlAsNaClDesc(int32_t pp_error,
                                             FileDownloader*& url_downloader,
                                             pp::Var& js_callback) {
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

  js_callback.Call(pp::Var("onload"), pp::Var(this, handle));
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

void PluginPpapi::UrlAsNaClDesc(const nacl::string& url, pp::Var js_callback) {
  PLUGIN_PRINTF(("PluginPpapi::UrlAsNaClDesc (url='%s')\n", url.c_str()));
  FileDownloader* downloader = new FileDownloader();
  downloader->Initialize(this);
  url_downloaders_.insert(downloader);
  pp::CompletionCallback open_callback =
      callback_factory_.NewCallback(&PluginPpapi::UrlDidOpenForUrlAsNaClDesc,
                                    downloader,
                                    js_callback);
  // Will always call the callback on success or failure.
  downloader->Open(url, open_callback);
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
  // Will always call the callback on success or failure.
  return downloader->Open(url, open_callback);
}


void PluginPpapi::ReportLoadSuccess() {
  // Inform JavaScript that loading was successful and is complete.
  // Note that these events will be dispatched, as will failure events, when
  // this code is reached from __launchExecutableFromFd also.
  // TODO(sehr,polina): Remove comment when experimental APIs are removed.
  DispatchProgressEvent("load", false, kUnknownBytes, kUnknownBytes);
  DispatchProgressEvent("loadend", false, kUnknownBytes, kUnknownBytes);
}


void PluginPpapi::ReportLoadError(const nacl::string& error) {
  PLUGIN_PRINTF(("PluginPpapi::ReportLoadError (error='%s')\n",
                 error.c_str()));
  nacl::string prefix("NaCl module load failed: ");
  set_last_error_string(prefix + error);
  browser_interface()->AddToConsole(instance_id(), prefix + error);
  ShutdownProxy();
  // Inform JavaScript that loading encountered an error and is complete.
  DispatchProgressEvent("error", false, kUnknownBytes, kUnknownBytes);
  DispatchProgressEvent("loadend", false, kUnknownBytes, kUnknownBytes);
}


void PluginPpapi::ReportLoadAbort() {
  PLUGIN_PRINTF(("PluginPpapi::ReportLoadAbort\n"));
  nacl::string error_string("NaCl module load failed: user aborted");
  set_last_error_string(error_string);
  browser_interface()->AddToConsole(instance_id(), error_string);
  ShutdownProxy();
  // Inform JavaScript that loading was aborted and is complete.
  DispatchProgressEvent("abort", false, kUnknownBytes, kUnknownBytes);
  DispatchProgressEvent("loadend", false, kUnknownBytes, kUnknownBytes);
}


void PluginPpapi::DispatchProgressEvent(const char* event_type,
                                        bool length_computable,
                                        uint64_t loaded_bytes,
                                        uint64_t total_bytes) {
  PLUGIN_PRINTF(("PluginPpapi::DispatchEvent ("
                 "event_type='%s', length_computable=%d, "
                 "loaded=%"NACL_PRIu64", total=%"NACL_PRIu64")\n",
                 event_type, length_computable, loaded_bytes, total_bytes));

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
  pp::Var window_object = GetWindowObject();
  if (!window_object.is_object()) {
    PLUGIN_PRINTF(("Couldn't get window object."));
    return;
  }
  pp::Var exception;
  pp::Var function_object = window_object.Call("eval",
                                               kEventClosureJS,
                                               &exception);
  if (!exception.is_undefined() || !function_object.is_object()) {
    PLUGIN_PRINTF(("Function object creation failed.\n"));
    return;
  }
  // Get the target of the event to be dispatched.
  pp::Var owner_element_object = GetOwnerElementObject();
  if (!owner_element_object.is_object()) {
    PLUGIN_PRINTF(("Couldn't get owner element object.\n"));
    NACL_NOTREACHED();
    return;
  }

  pp::Var argv[5];
  static const uint32_t argc = NACL_ARRAY_SIZE(argv);
  argv[0] = owner_element_object;
  argv[1] = pp::Var(event_type);
  argv[2] = pp::Var(length_computable);
  argv[3] = pp::Var(static_cast<double>(loaded_bytes));
  argv[4] = pp::Var(static_cast<double>(total_bytes));

  // Dispatch the event.
  const pp::Var default_method;
  function_object.Call(default_method, argc, argv, &exception);
  if (!exception.is_undefined()) {
    PLUGIN_PRINTF(("Event dispatch failed.\n"));
  }
}

}  // namespace plugin
