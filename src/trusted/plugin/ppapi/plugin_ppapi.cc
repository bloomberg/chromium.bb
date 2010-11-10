/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"

#include <string>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/dev/url_request_info_dev.h"
#include "ppapi/cpp/dev/url_response_info_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"

namespace {
const char* const kSrcAttribute = "src";  // The "src" attr of the <embed> tag.
// The "nacl" attribute of the <embed> tag.  The value is expected to be either
// a URL or URI pointing to the manifest file (which is expeceted to contain
// JSON matching ISAs with .nexe URLs).
const char* const kNaclManifestAttribute = "nacl";
// The "nexes" attr of the <embed> tag, and the key used to find the dicitonary
// of nexe URLs in the manifest file.
const char* const kNexesAttribute = "nexes";
}  // namespace

namespace plugin {

PluginPpapi* PluginPpapi::New(PP_Instance pp_instance) {
  PLUGIN_PRINTF(("PluginPpapi::New (pp_instance=%"NACL_PRId64")\n",
                 pp_instance));
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  if (!NaClHandlePassBrowserCtor()) {
    return NULL;
  }
#endif
  PluginPpapi* plugin = new(std::nothrow) PluginPpapi(pp_instance);
  if (plugin == NULL) {
    return NULL;
  }
  PLUGIN_PRINTF(("PluginPpapi::New (return %p)\n",
                 static_cast<void*>(plugin)));
  return plugin;
}


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
    const char* src_attr = LookupArgument(kSrcAttribute);
    PLUGIN_PRINTF(("PluginPpapi::Init (src_attr=%s)\n", src_attr));
    if (src_attr != NULL) {
      status = SetSrcPropertyImpl(src_attr);
    } else {
      // If there was no "src" attribute, then look for a "nacl" attribute
      // and try to load the ISA defined for this particular sandbox.
      const char* nacl_attr = LookupArgument(kNaclManifestAttribute);
      PLUGIN_PRINTF(("PluginPpapi::Init (nacl_attr=%s)\n", nacl_attr));
      if (nacl_attr != NULL) {
        // TODO(dspringer): if this is a data URI, then grab the JSON from it,
        // otherwise, issue a GET for the URL.
      } else {
        // If there was no "src" or "nacl" attributes, then look for a "nexes"
        // attribute and try to load the ISA defined for this particular
        // sandbox.
        // TODO(dspringer): deprecate this.
        const char* nexes_attr = LookupArgument(kNexesAttribute);
        PLUGIN_PRINTF(("PluginPpapi::Init (nexes_attr=%s)\n", nexes_attr));
        if (nexes_attr != NULL) {
          status = SetNexesPropertyImpl(nexes_attr);
        }
      }
    }
  }

  PLUGIN_PRINTF(("PluginPpapi::Init (return %d)\n", status));
  return status;
}


PluginPpapi::PluginPpapi(PP_Instance pp_instance)
    : pp::Instance(pp_instance),
      ppapi_proxy_(NULL) {
  PLUGIN_PRINTF(("PluginPpapi::PluginPpapi (this=%p, pp_instance=%"
                 NACL_PRId64")\n", static_cast<void*>(this), pp_instance));
  callback_factory_.Initialize(this);
}


PluginPpapi::~PluginPpapi() {
  PLUGIN_PRINTF(("PluginPpapi::~PluginPpapi (this=%p, scriptable_handle=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(scriptable_handle())));

#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  NaClHandlePassBrowserDtor();
#endif

  delete ppapi_proxy_;
  ScriptableHandle* scriptable_handle_ = scriptable_handle();
  UnrefScriptableHandle(&scriptable_handle_);
}


void PluginPpapi::DidChangeView(const pp::Rect& position,
                                const pp::Rect& clip) {
  PLUGIN_PRINTF(("PluginPpapi::DidChangeView (this=%p)\n",
                 static_cast<void*>(this)));
  if (ppapi_proxy_ == NULL) {
    return;
  } else {
    // TODO(polina): cache the instance_interface on the plugin.
    const PPP_Instance* instance_interface =
        reinterpret_cast<const PPP_Instance*>(
            ppapi_proxy_->GetInterface(PPP_INSTANCE_INTERFACE));
    if (instance_interface == NULL) {
      // TODO(polina): report an error here.
      return;
    }
    instance_interface->DidChangeView(pp_instance(),
                                      &(position.pp_rect()),
                                      &(clip.pp_rect()));
  }
}


void PluginPpapi::DidChangeFocus(bool has_focus) {
  PLUGIN_PRINTF(("PluginPpapi::DidChangeFocus (this=%p)\n",
                 static_cast<void*>(this)));
  if (ppapi_proxy_ == NULL) {
    return;
  } else {
    // TODO(polina): cache the instance_interface on the plugin.
    const PPP_Instance* instance_interface =
        reinterpret_cast<const PPP_Instance*>(
            ppapi_proxy_->GetInterface(PPP_INSTANCE_INTERFACE));
    if (instance_interface == NULL) {
      // TODO(polina): report an error here.
      return;
    }
    instance_interface->DidChangeFocus(pp_instance(),
                                       pp::BoolToPPBool(has_focus));
  }
}


bool PluginPpapi::HandleInputEvent(const PP_InputEvent& event) {
  PLUGIN_PRINTF(("PluginPpapi::HandleInputEvent (this=%p)\n",
                 static_cast<void*>(this)));
  if (ppapi_proxy_ == NULL) {
    return false;  // event is not handled here.
  } else {
    // TODO(polina): cache the instance_interface on the plugin.
    const PPP_Instance* instance_interface =
        reinterpret_cast<const PPP_Instance*>(
            ppapi_proxy_->GetInterface(PPP_INSTANCE_INTERFACE));
    if (instance_interface == NULL) {
      // TODO(polina): report an error here.
      return false;  // event is not handled here.
    }
    return pp::PPBoolToBool(
        instance_interface->HandleInputEvent(pp_instance(), &event));
  }
}


bool PluginPpapi::HandleDocumentLoad(const pp::URLLoader_Dev& url_loader) {
  PLUGIN_PRINTF(("PluginPpapi::HandleDocumentLoad (this=%p)\n",
                 static_cast<void*>(this)));
  if (ppapi_proxy_ == NULL) {
    return false;
  } else {
    // TODO(polina): cache the instance_interface on the plugin.
    const PPP_Instance* instance_interface =
        reinterpret_cast<const PPP_Instance*>(
            ppapi_proxy_->GetInterface(PPP_INSTANCE_INTERFACE));
    if (instance_interface == NULL) {
      // TODO(polina): report an error here.
      return false;
    }
    return pp::PPBoolToBool(
        instance_interface->HandleDocumentLoad(pp_instance(),
                                               url_loader.pp_resource()));
  }
}


pp::Var PluginPpapi::GetInstanceObject() {
  PLUGIN_PRINTF(("PluginPpapi::GetInstanceObject (this=%p)\n",
                 static_cast<void*>(this)));
  ScriptableHandlePpapi* handle =
      static_cast<ScriptableHandlePpapi*>(scriptable_handle()->AddRef());
  if (ppapi_proxy_ == NULL) {
    pp::Var* handle_var = handle->var();
    PLUGIN_PRINTF(("PluginPpapi::GetInstanceObject (handle=%p, "
                   "handle_var=%p)\n",
                  static_cast<void*>(handle), static_cast<void*>(handle_var)));
    return *handle_var;  // make a copy
  } else {
    // TODO(sehr): cache the instance_interface on the plugin.
    const PPP_Instance* instance_interface =
        reinterpret_cast<const PPP_Instance*>(
            ppapi_proxy_->GetInterface(PPP_INSTANCE_INTERFACE));
    if (instance_interface == NULL) {
      pp::Var* handle_var = handle->var();
      // TODO(sehr): report an error here.
      return *handle_var;  // make a copy
    }
    // Yuck.  This feels like another low-level interface usage.
    // TODO(sehr,polina): add a better interface to rebuild Vars from
    // low-level components we proxy.
    return pp::Var(pp::Var::PassRef(),
                   instance_interface->GetInstanceObject(pp_instance()));
  }
}


void PluginPpapi::NexeURLLoadStartNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("PluginPpapi::NexeURLLoadStartNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {  // Url loading failed.
    return;
  }

  // Process the response, validating the headers to confirm successful loading.
  pp::URLResponseInfo_Dev url_response(nexe_loader_.GetResponseInfo());
  if (url_response.is_null()) {
    PLUGIN_PRINTF((
        "PluginPpapi::NexeURLLoadStartNotify (url_response=NULL)\n"));
    return;
  }
  int32_t status_code = url_response.GetStatusCode();
  PLUGIN_PRINTF(("PluginPpapi::NexeURLLoadStartNotify (status_code=%"
                 NACL_PRId32")\n", status_code));
  if (status_code != NACL_HTTP_STATUS_OK) {
    return;
  }

  // Finish streaming the body asynchronously providing a callback.
  pp::CompletionCallback onload_callback =
      callback_factory_.NewCallback(&PluginPpapi::NexeURLLoadFinishNotify);
  pp_error = nexe_loader_.FinishStreamingToFile(onload_callback);
  bool async_notify_ok = (pp_error == PP_ERROR_WOULDBLOCK);
  PLUGIN_PRINTF(("PluginPpapi::NexeURLLoadStartNotify (async_notify_ok=%d)\n",
                 async_notify_ok));
  if (!async_notify_ok) {
    onload_callback.Run(pp_error);  // Call manually to free allocated memory.
  }
}


void PluginPpapi::NexeURLLoadFinishNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("PluginPpapi::NexeURLLoadFinishNotify (pp_error=%"
                 NACL_PRId32")\n", pp_error));
  if (pp_error != PP_OK) {  // Streaming failed.
    return;
  }

  pp::URLResponseInfo_Dev url_response(nexe_loader_.GetResponseInfo());
  CHECK(url_response.GetStatusCode() == NACL_HTTP_STATUS_OK);

  // Record the full url from the response.
  pp::Var full_url = url_response.GetURL();
  PLUGIN_PRINTF(("PluginPpapi::NexeURLLoadFinishNotify (full_url=%s)\n",
                 full_url.DebugString().c_str()));
  if (!full_url.is_string()) {
    return;
  }
  set_nacl_module_url(full_url.AsString());

  // The file is now fully downloaded.
  pp::FileRef_Dev file(url_response.GetBody());
  if (file.is_null()) {
    PLUGIN_PRINTF(("PluginPpapi::NexeURLLoadFinishNotify (file=NULL)\n"));
    return;
  }

  // Open the file asynchronously providing a callback.
  pp::CompletionCallback onopen_callback =
      callback_factory_.NewCallback(&PluginPpapi::NexeFileOpenNotify);
  pp_error = nexe_reader_.Open(file, PP_FILEOPENFLAG_READ, onopen_callback);
  bool async_notify_ok = (pp_error == PP_ERROR_WOULDBLOCK);
  PLUGIN_PRINTF(("PluginPpapi::NexeURLLoadFinishNotify (async_notify_ok=%d)\n",
                 async_notify_ok));
  if (!async_notify_ok) {
    onopen_callback.Run(pp_error);  // Call manually to free allocated memory.
  }
}


void PluginPpapi::NexeFileOpenNotify(int32_t pp_error) {
  PLUGIN_PRINTF(("PluginPpapi::NexeFileOpenNotify (pp_error=%"NACL_PRId32")\n",
                 pp_error));
  if (pp_error != PP_OK) {  // File opening failed.
    return;
  }

  int32_t file_desc = nexe_reader_.GetOSFileDescriptor();
  PLUGIN_PRINTF(("PluginPpapi::NexeFileOpenNotify (file_desc=%"NACL_PRId32")\n",
                 file_desc));
  if (file_desc > NACL_NO_FILE_DESC) {
    LoadNaClModule(nacl_module_url(), file_desc);
  }
}


bool PluginPpapi::RequestNaClModule(const nacl::string& url) {
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule (url='%s')\n", url.c_str()));

  // Reset the url loader and file reader.
  // Note that we have the only refernce to the underlying objects, so
  // this will implicitly close any pending IO and destroy them.
  nexe_loader_ = pp::URLLoader_Dev(*this);
  nexe_reader_ = pp::FileIO_Dev();

  // Prepare the url request using a relative url.
  pp::URLRequestInfo_Dev url_request;
  url_request.SetURL(url);
  url_request.SetStreamToFile(true);

  // Request asynchronous download of the url providing an on-load callback.
  pp::CompletionCallback onload_callback =
      callback_factory_.NewCallback(&PluginPpapi::NexeURLLoadStartNotify);
  int32_t pp_error = nexe_loader_.Open(url_request, onload_callback);
  bool async_notify_ok = (pp_error == PP_ERROR_WOULDBLOCK);
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule (async_notify_ok=%d)\n",
                 async_notify_ok));
  if (!async_notify_ok) {
    onload_callback.Run(pp_error);  // Call manually to free allocated memory.
    return false;
  }
  return true;
}


void PluginPpapi::StartProxiedExecution(NaClSrpcChannel* srpc_channel) {
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (%p)\n",
                 reinterpret_cast<void*>(srpc_channel)));
  // Check that the .nexe exports the PPAPI intialization method.
  NaClSrpcService* client_service = srpc_channel->client;
  if (NaClSrpcServiceMethodIndex(client_service,
                                 "PPP_InitializeModule:ilhs:ii") ==
      kNaClSrpcInvalidMethodIndex) {
    return;
  }
  ppapi_proxy_ =
      new(std::nothrow) ppapi_proxy::BrowserPpp(srpc_channel);
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (ppapi_proxy_ = %p)\n",
                 reinterpret_cast<void*>(ppapi_proxy_)));
  if (ppapi_proxy_ == NULL) {
    return;
  }
  pp::Module* module = pp::Module::Get();
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (module = %p)\n",
                 reinterpret_cast<void*>(module)));
  if (module == NULL) {
    return;
  }
  int32_t init_retval =
      ppapi_proxy_->InitializeModule(module->pp_module(),
                                     module->get_browser_interface(),
                                     pp_instance());
  if (init_retval != PP_OK) {
    // TODO(sehr): we should report an error here and shut down the proxy.
    // For now we will leak the proxy, but no longer be allowed to access it.
    ppapi_proxy_ = NULL;
    return;
  }
  const PPP_Instance* instance_interface =
      reinterpret_cast<const PPP_Instance*>(
          ppapi_proxy_->GetInterface(PPP_INSTANCE_INTERFACE));
  if (instance_interface == NULL) {
    // TODO(sehr): we should report an error here and shut down the proxy.
    // For now we will leak the proxy, but no longer be allowed to access it.
    ppapi_proxy_ = NULL;
    return;
  }
  // Create an instance and initialize the instance's parameters.
  if (!instance_interface->DidCreate(pp_instance(),
                                     argc(),
                                     const_cast<const char**>(argn()),
                                     const_cast<const char**>(argv()))) {
    // TODO(sehr): we should report an error here.
    return;
  }
}

bool PluginPpapi::SelectNexeURLFromManifest(
    const nacl::string& nexe_manifest_json, nacl::string* result) {
  const nacl::string sandbox_isa(GetSandboxISA());
  PLUGIN_PRINTF(
      ("GetNexeURLFromManifest(): sandbox='%s' nexe_manifest_json='%s'.\n",
       sandbox_isa.c_str(), nexe_manifest_json.c_str()));
  if (result == NULL)
    return false;
  // Eval the JSON via the browser.
  pp::Var window_object = GetWindowObject();
  if (!window_object.is_object()) {
    return false;
  }
  nacl::string eval_string("(");
  eval_string += nexe_manifest_json;
  eval_string += ")";
  pp::Var manifest_root = window_object.Call("eval", eval_string);
  if (!manifest_root.is_object()) {
    return false;
  }
  // Look for the 'nexes' key.
  if (!manifest_root.HasProperty(kNexesAttribute)) {
    return false;
  }
  pp::Var nexes_dict = manifest_root.GetProperty(kNexesAttribute);
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
}  // namespace plugin
