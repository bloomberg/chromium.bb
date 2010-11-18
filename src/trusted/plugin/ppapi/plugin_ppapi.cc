/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"

#include <stdio.h>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"

// TODO(sehr,polina): we should have a Failure helper class that can log errors
// to the JavaScript console, and it should be used in addition to
// PLUGIN_PRINTF in the event of a failure.

namespace {
const char* const kSrcAttribute = "src";  // The "src" attr of the <embed> tag.
// The "nacl" attribute of the <embed> tag.  The value is expected to be either
// a URL or URI pointing to the manifest file (which is expeceted to contain
// JSON matching ISAs with .nexe URLs).
const char* const kNaclManifestAttribute = "nacl";
// This is a pretty arbitrary limit on the byte size of the NaCl manfest file.
// Note that the resulting string object has to have at least one byte extra
// for the null termination character.
const ssize_t kNaclManifestMaxFileBytesPlusNull = 1024;
const ssize_t kNaclManifestMaxFileBytesNoNull =
    kNaclManifestMaxFileBytesPlusNull - 1;
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
    // Note: The order of attribute lookup is important.  This pattern looks
    // for a "nacl" attribute first, then a "src" attribute, and finally a
    // "nexes" attribute.
    const char* nacl_attr = LookupArgument(kNaclManifestAttribute);
    PLUGIN_PRINTF(("PluginPpapi::Init (nacl_attr=%s)\n", nacl_attr));
    if (nacl_attr != NULL) {
      // Issue a GET for the "nacl" attribute.  The value of the attribute
      // can be a data: URI, or a URL that must be fetched.  In either case,
      // the GET is started here, and once a valid .nexe file has been
      // determined, SetSrcPropertyImpl() is called to shut down the current
      // service runtime (sel_ldr) process and start the download of the new
      // .nexe.  If the download is successful, a new service runtime starts
      // and runs the new .nexe.
      status = RequestNaClManifest(nacl_attr);
    } else {
      const char* src_attr = LookupArgument(kSrcAttribute);
      PLUGIN_PRINTF(("PluginPpapi::Init (src_attr=%s)\n", src_attr));
      if (src_attr != NULL) {
        status = SetSrcPropertyImpl(src_attr);
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
  url_downloader_.Initialize(this);
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


bool PluginPpapi::HandleDocumentLoad(const pp::URLLoader& url_loader) {
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
      PLUGIN_PRINTF(("PluginPpapi::GetInstanceObject failed\n"));
      return *handle_var;  // make a copy
    }
    // Yuck.  This feels like another low-level interface usage.
    // TODO(sehr,polina): add a better interface to rebuild Vars from
    // low-level components we proxy.
    return pp::Var(pp::Var::PassRef(),
                   instance_interface->GetInstanceObject(pp_instance()));
  }
}


void PluginPpapi::NexeFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("PluginPpapi::RemoteFileDidOpen (pp_error=%"NACL_PRId32")\n",
                 pp_error));
  if (pp_error != PP_OK) {
    return;
  }
  int32_t file_desc = url_downloader_.GetOSFileDescriptor();
  PLUGIN_PRINTF(("PluginPpapi::RemoteFileDidOpen (file_desc=%"NACL_PRId32")\n",
                 file_desc));
  if (file_desc > NACL_NO_FILE_DESC) {
    LoadNaClModule(url_downloader_.url(), file_desc);
  }
}


bool PluginPpapi::RequestNaClModule(const nacl::string& url) {
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule (url='%s')\n", url.c_str()));
  pp::CompletionCallback open_callback =
      callback_factory_.NewCallback(&PluginPpapi::NexeFileDidOpen);
  // If an error occurs during Open(), |open_callback| will be called with the
  // appropriate error value.  If the URL loading started up OK, then
  // |open_callback| will be called later on as the URL loading progresses.
  return url_downloader_.Open(url, open_callback);
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
      PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution failed\n"));
    // For now we will leak the proxy, but no longer be allowed to access it.
    ppapi_proxy_ = NULL;
    return;
  }
  const PPP_Instance* instance_interface =
      reinterpret_cast<const PPP_Instance*>(
          ppapi_proxy_->GetInterface(PPP_INSTANCE_INTERFACE));
  if (instance_interface == NULL) {
      PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution failed\n"));
    // TODO(sehr): Shut down the proxy.  For now we will leak the proxy, but
    // no longer be allowed to access it.
    ppapi_proxy_ = NULL;
    return;
  }
  // Create an instance and initialize the instance's parameters.
  if (!instance_interface->DidCreate(pp_instance(),
                                     argc(),
                                     const_cast<const char**>(argn()),
                                     const_cast<const char**>(argv()))) {
    PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution failed\n"));
    return;
  }
}


void PluginPpapi::NaClManifestFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(
      ("PluginPpapi::NaClManifestFileDidOpen (pp_error=%"NACL_PRId32")\n",
       pp_error));
  if (pp_error != PP_OK) {
    return;
  }
  int32_t file_desc = url_downloader_.GetOSFileDescriptor();
  PLUGIN_PRINTF(
      ("PluginPpapi::NaClManifestFileDidOpen (file_desc=%"NACL_PRId32")\n",
       file_desc));
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
    PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen failed\n"));
    return;
  }
  nacl::scoped_array<char> json_buffer(
      new char[kNaclManifestMaxFileBytesPlusNull]);
  size_t read_byte_count = fread(json_buffer.get(),
                                 sizeof(char),
                                 kNaclManifestMaxFileBytesNoNull,
                                 json_file);
  bool read_error = (ferror(json_file) != 0);
  bool file_too_large = (feof(json_file) == 0);
  // Once the bytes are read, the FILE is no longer needed, so close it.  This
  // allows for early returns without leaking the |json_file| FILE object.
  fclose(json_file);
  if (read_error || (file_too_large == 0)) {
    // No need to close |file_desc|, that is handled by |url_downloader_|.
    PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen failed\n"));
    return;
  }
  json_buffer[read_byte_count] = '\0';  // Force null termination.
  nacl::string nexe_url;
  if (SelectNexeURLFromManifest(json_buffer.get(), &nexe_url)) {
    // Success, load the .nexe by setting the "src" attribute.
    PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen (nexe_url=%s)\n",
                   nexe_url.c_str()));
    SetSrcPropertyImpl(nexe_url);
    return;
  }
  PLUGIN_PRINTF(("PluginPpapi::NaClManifestFileDidOpen failed\n"));
}


bool PluginPpapi::RequestNaClManifest(const nacl::string& url) {
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClManifest (url='%s')\n", url.c_str()));
  pp::CompletionCallback open_callback =
      callback_factory_.NewCallback(&PluginPpapi::NaClManifestFileDidOpen);
  // If an error occurs during Open(), |open_callback| will be called with the
  // appropriate error value.  If the URL loading started up OK, then
  // |open_callback| will be called later on as the URL loading progresses.
  return url_downloader_.Open(url, open_callback);
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
