/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/trusted/plugin/npapi/plugin_npapi.h"

#include <stdio.h>
#include <string.h>

#include <limits>
#include <set>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/shared/npruntime/npmodule.h"

#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/handle_pass/browser_handle.h"

#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/npapi/async_receive.h"
#include "native_client/src/trusted/plugin/npapi/browser_impl_npapi.h"
#include "native_client/src/trusted/plugin/npapi/closure.h"
#include "native_client/src/trusted/plugin/npapi/multimedia_socket.h"
#include "native_client/src/trusted/plugin/npapi/scriptable_impl_npapi.h"
#include "native_client/src/trusted/plugin/npapi/video.h"
#include "native_client/src/trusted/plugin/origin.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/stream_shm_buffer.h"
#include "native_client/src/trusted/plugin/utility.h"

namespace {

static bool identifiers_initialized = false;

void InitializeIdentifiers() {
  if (identifiers_initialized) {
    return;
  }
  plugin::PluginNpapi::kHrefIdent = NPN_GetStringIdentifier("href");
  plugin::PluginNpapi::kLengthIdent = NPN_GetStringIdentifier("length");
  plugin::PluginNpapi::kLocationIdent = NPN_GetStringIdentifier("location");
  identifiers_initialized = true;
}

// TODO(polina): is there a way to share this with PluginPPAPI?
bool UrlAsNaClDesc(void* obj, plugin::SrpcParams* params) {
  NaClSrpcArg** ins = params->ins();
  PLUGIN_PRINTF(("UrlAsNaClDesc (obj=%p, url=%s, callback=%p)\n",
                 obj, ins[0]->u.sval.str, ins[1]->u.oval));

  plugin::Plugin* plugin = reinterpret_cast<plugin::Plugin*>(obj);
  const char* url = ins[0]->u.sval.str;
  NPObject* callback_obj = reinterpret_cast<NPObject*>(ins[1]->u.oval);

  plugin::UrlAsNaClDescNotify* callback =
      new(std::nothrow) plugin::UrlAsNaClDescNotify(plugin, url, callback_obj);
  if (NULL == callback) {
    params->set_exception_string("out of memory in __urlAsNaClDesc");
    return false;
  }
  if (!callback->StartDownload()) {
    PLUGIN_PRINTF(("UrlAsNaClDesc (failed to load url to local file)\n"));
    params->set_exception_string("specified url could not be loaded");
    // callback is always deleted in URLNotify
    return false;
  }
  return true;
}

}  // namespace

namespace plugin {

NPIdentifier PluginNpapi::kHrefIdent;
NPIdentifier PluginNpapi::kLengthIdent;
NPIdentifier PluginNpapi::kLocationIdent;

PluginNpapi* PluginNpapi::New(NPP npp,
                              int argc,
                              char* argn[],
                              char* argv[]) {
  PLUGIN_PRINTF(("PluginNpapi::New (npp=%p, argc=%d)\n",
                 static_cast<void*>(npp), argc));
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  if (!NaClHandlePassBrowserCtor()) {
    return NULL;
  }
#endif
  InitializeIdentifiers();
  // TODO(sehr): use scoped_ptr for proper delete semantics.
  BrowserInterface* browser_interface =
      static_cast<BrowserInterface*>(new(std::nothrow) BrowserImplNpapi);
  if (browser_interface == NULL) {
    return NULL;
  }
  PluginNpapi* plugin = new(std::nothrow) PluginNpapi();
  InstanceIdentifier instance_id = NPPToInstanceIdentifier(npp);
  if (plugin == NULL ||
      !plugin->Init(browser_interface, instance_id, argc, argn, argv)) {
    PLUGIN_PRINTF(("PluginNpapi::New: Init failed\n"));
    return NULL;
  }
  // Add methods only implemented by the NPAPI plugin.
  plugin->AddMethodCall(UrlAsNaClDesc, "__urlAsNaClDesc", "so", "");
  plugin->AddMethodCall(SetAsyncCallback, "__setAsyncCallback", "o", "");
  // Set up the multimedia video support.
  plugin->video_ = new(std::nothrow) VideoMap(plugin);
  if (NULL == plugin->video_) {
    return false;
  }
  // Create the browser scriptable handle for plugin.
  ScriptableHandle* handle = browser_interface->NewScriptableHandle(plugin);
  PLUGIN_PRINTF(("PluginNpapi::New (scriptable_handle=%p)\n",
                 static_cast<void*>(handle)));
  if (NULL == handle) {
    return NULL;
  }
  plugin->set_scriptable_handle(handle);
  PLUGIN_PRINTF(("PluginNpapi::New (return %p)\n", static_cast<void*>(plugin)));
  return plugin;
}

PluginNpapi::~PluginNpapi() {
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  NaClHandlePassBrowserDtor();
#endif

  PLUGIN_PRINTF(("PluginNpapi::~PluginNpapi (this=%p)\n",
                 static_cast<void* >(this)));
  // Delete the NPModule for this plugin.
  if (NULL != module_) {
    delete module_;
  }
  /* SCOPE */ {
    VideoScopedGlobalLock video_lock;
    PLUGIN_PRINTF(("Plugin::~Plugin deleting video_\n"));
    if (NULL != video_) {
      delete video_;
      video_ = NULL;
    }
  }
}

NPError PluginNpapi::Destroy(NPSavedData** save) {
  PLUGIN_PRINTF(("PluginNpapi::Destroy (this=%p, save=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(save)));

  ShutDownSubprocess();

  // This should be done after terminating the sel_ldr subprocess so
  // that we can be sure we will not block forever when waiting for
  // the upcall thread to exit.
  delete module_;
  module_ = NULL;

  // This has the indirect effect of doing "delete this".
  PLUGIN_PRINTF(("PluginNpapi::Destroy (this=%p, scriptable_handle=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(scriptable_handle())));
  scriptable_handle()->Unref();
  return NPERR_NO_ERROR;
}

// SetWindow is called by the browser as part of the NPAPI interface for
// setting up a plugin that has the ability to draw into a window.  It is
// passed a semi-custom window descriptor (some is platform-neutral, some not)
// as documented in the NPAPI documentation.
NPError PluginNpapi::SetWindow(NPWindow* window) {
  NPError ret = NPERR_GENERIC_ERROR;
  PLUGIN_PRINTF(("PluginNpapi::SetWindow(%p, %p)\n", static_cast<void* >(this),
                 static_cast<void*>(window)));

// NOTE(gregoryd): Chrome does not allow us to call NPN_GetUrlNotify during
// initialization, but does call SetWindows afterwards, so we use this call
// to trigger the download if the src property hasn't been specified.
#if !defined(NACL_STANDALONE)
  // If the <embed src='...'> attr was defined, the browser would have
  // implicitly called GET on it, which calls LoadNaClModule() and
  // set_nacl_module_url().
  // In the absence of this attr, we use the "nexes" attribute if present.
  if (nacl_module_url() == NACL_NO_URL) {
    const char* nexes_attr = LookupArgument("nexes");
    if (nexes_attr != NULL) {
      SetNexesPropertyImpl(nexes_attr);
    }
  }
#endif
  if (NULL == module_) {
    if (video() && video()->SetWindow(window)) {
        ret = NPERR_NO_ERROR;
    }
    return ret;
  } else {
    // Send NPP_SetWindow to NPModule.
    NPP npp = InstanceIdentifierToNPP(instance_id());
    return module_->SetWindow(npp, window);
  }
}

NPError PluginNpapi::GetValue(NPPVariable variable, void* value) {
  const char** stringp = static_cast<const char**>(value);

  PLUGIN_PRINTF(("PluginNpapi::GetValue(%p, %d)\n", static_cast<void*>(this),
                 variable));

  switch (variable) {
    case NPPVpluginNameString:
      *stringp = "NativeClient Simple RPC + multimedia a/v interface";
      return NPERR_NO_ERROR;
    case NPPVpluginDescriptionString:
      *stringp = "NativeClient Simple RPC interaction w/ multimedia.";
      return NPERR_NO_ERROR;
    case NPPVpluginScriptableNPObject:
      // Anyone requesting access to the scriptable instance is given shared
      // ownership of the scriptable handle.
      *(static_cast<NPObject**>(value)) =
          static_cast<ScriptableImplNpapi*>(scriptable_handle()->AddRef());
      return NPERR_NO_ERROR;
    case NPPVpluginWindowBool:
    case NPPVpluginTransparentBool:
    case NPPVjavaClass:
    case NPPVpluginWindowSize:
    case NPPVpluginTimerInterval:
    case NPPVpluginScriptableInstance:
    case NPPVpluginScriptableIID:
    case NPPVjavascriptPushCallerBool:
    case NPPVpluginKeepLibraryInMemory:
    case NPPVpluginNativeAccessibleAtkPlugId:
    case NPPVpluginNeedsXEmbed:
    case NPPVformValue:
    case NPPVpluginUrlRequestsDisplayedBool:
    case NPPVpluginWantsAllNetworkStreams:
    case NPPVpluginCancelSrcStream:
#ifdef XP_MACOSX
    // Mac has several drawing, event, etc. models in NPAPI that are unique.
    case NPPVpluginDrawingModel:
    case NPPVpluginEventModel:
    case NPPVpluginCoreAnimationLayer:
#endif  // XP_MACOSX
    default:
      return NPERR_INVALID_PARAM;
  }
}

int16_t PluginNpapi::HandleEvent(void* param) {
  int16_t ret;
  PLUGIN_PRINTF(("PluginNpapi::HandleEvent(%p, %p)\n", static_cast<void*>(this),
                 static_cast<void*>(param)));
  if (NULL == module_) {
    if (video()) {
      ret = video()->HandleEvent(param);
    } else {
      ret = 0;
    }
  } else {
    NPP npp = InstanceIdentifierToNPP(instance_id());
    return module_->HandleEvent(npp, param);
  }
  return ret;
}

// Downloading resources can be caused implicitly (by the browser in response
// to src= in the embed/object tag) or explicitly (by calls to NPN_GetURL or
// NPN_GetURLNotify).  Implicit downloading is happening whenever
// notifyData==NULL, and always results in calling Load on the Plugin object.
// Explicit downloads place a pointer to a Closure object in notifyData.  How
// these closures are manipulated depends on which browser we are running
// within.  If we are in Chrome (NACL_STANDALONE is not defined):
// - NewStream creates a StreamShmBuffer object and attaches that to
//   stream->pdata and the buffer member of the closure (if there was one).
// - WriteReady and Write populate the buffer object.
// - DestroyStream signals the end of WriteReady/Write processing.  If the
//   reason is NPRES_DONE, then the closure's Run method is invoked and
//   the closure is deleted.  If there is no closure, Load is called.
// If we are not in Chrome (NACL_STANDALONE is defined):
// - NewStream returns NP_ASFILEONLY, which causes StreamAsFile to be invoked.
// - StreamAsFile indicates that the browser has fully downloaded the resource
//   and placed it in the local file system.  This causes the closure's Run
//   method to be invoked.  If there is no closure, Load is called.
// In both cases, URLNotify is used to report any errors.

NPError PluginNpapi::NewStream(NPMIMEType type,
                               NPStream* stream,
                               NPBool seekable,
                               uint16_t* stype) {
  PLUGIN_PRINTF(("PluginNpapi::NewStream(%p, %s, %p, %d)\n",
                 static_cast<void*>(this), type, static_cast<void*>(stream),
                 seekable));
#ifdef NACL_STANDALONE
  *stype = NP_ASFILEONLY;
#else
  // When running as a built-in plugin in Chrome we cannot access the
  // file system, therefore we use normal streams to get the data.
  *stype = NP_NORMAL;
  // Stream pdata should not be set until the stream is created.
  if (NULL != stream->pdata) {
    return NPERR_GENERIC_ERROR;
  }
  // StreamShmBuffer is used to download large files in chunks in Chrome.
  StreamShmBuffer* stream_buffer = new(std::nothrow) StreamShmBuffer();
  // Remember the stream buffer on the stream.
  stream->pdata = reinterpret_cast<void*>(stream_buffer);
  // Other than the default "src=" download, there should have been a
  // closure attached to the stream by NPN_GetURLNotify.
  Closure* closure = static_cast<Closure*>(stream->notifyData);
  if (NULL != closure) {
    closure->set_buffer(stream_buffer);
  }
#endif
  return NPERR_NO_ERROR;
}

int32_t PluginNpapi::WriteReady(NPStream* stream) {
  if (NULL == stream) {
    return -1;
  }
  return 32 * 1024;
}

int32_t PluginNpapi::Write(NPStream* stream,
                           int32_t offset,
                           int32_t len,
                           void* buf) {
  if (NULL == stream) {
    return -1;
  }
  StreamShmBuffer* stream_buffer =
      reinterpret_cast<StreamShmBuffer*>(stream->pdata);
  // Should have been set during call to NewStream.
  if (NULL == stream_buffer) {
    return -1;
  }
  return stream_buffer->write(offset, len, buf);
}

void PluginNpapi::StreamAsFile(NPStream* stream,
                               const char* fname) {
  PLUGIN_PRINTF(("PluginNpapi::StreamAsFile(%p, %p, %s)\n",
                 static_cast<void*>(this), static_cast<void*>(stream), fname));
  // The stream should be valid until the destroy call is complete.
  // Furthermore, a valid filename should have been passed.
  if (NULL == fname || NULL == stream) {
    PLUGIN_PRINTF(("StreamAsFile: FAILED: fname or stream was NULL.\n"));
    return;
  }
  // When StreamAsFile is called a file for the stream is presented to the
  // plugin.  This only happens outside of Chrome.  So this handler calls the
  // appropriate load method to transfer the requested resource to the sel_ldr
  // instance.
  if (NULL == stream->notifyData) {
    // If there was no closure, there was no explicit plugin call to
    // NPN_GetURL{Notify}.  Hence this resource was downloaded by default,
    // typically through src=... in the embed/object tag.
    PLUGIN_PRINTF(("StreamAsFile: default run\n"));
    LoadNaClModule(stream->url, fname);
  } else {
    // Otherwise, we invoke the Run on the closure that was set up by
    // the requestor.
    Closure* closure = static_cast<Closure*>(stream->notifyData);
    closure->RunFromFile(stream, fname);
  }
}

NPError PluginNpapi::DestroyStream(NPStream* stream,
                                   NPReason reason) {
  PLUGIN_PRINTF(("PluginNpapi::DestroyStream(%p, %p, %d)\n",
                 static_cast<void*>(this), static_cast<void*>(stream), reason));

  // DestroyStream is called whenever a request for a resource either succeeds
  // or fails.  If the request succeeded, we would already have called
  // StreamAsFile (for non-Chrome browsers, which already invoked the Run
  // method on the closure), or would have done all the Writes (for Chrome,
  // and we still need to invoke the Run method).

  // The stream should be valid until the destroy call is complete.
  if (NULL == stream || NULL == stream->url) {
    return NPERR_GENERIC_ERROR;
  }
  // Defer error handling to URLNotify.
  if (NPRES_DONE != reason) {
    return NPERR_NO_ERROR;
  }
  if (NULL == stream->notifyData) {
    // Here we handle only the default, src=...  streams (statically obtained)
    // Stream download completed so start the nexe load into the service
    // runtime.
    PLUGIN_PRINTF(("DestroyStream: default run\n"));
    StreamShmBuffer* stream_buffer =
        reinterpret_cast<StreamShmBuffer*>(stream->pdata);
    // We are running outside of Chrome, so StreamAsFile does the load.
    if (NULL == stream_buffer) {
      return NPERR_NO_ERROR;
    }
    // Note, we cannot access the HTTP status code, so we might have
    // been returned a 404 error page.  This is reported in the ELF
    // validity checks that Load precipitates.
    LoadNaClModule(stream->url, stream_buffer);
    delete(stream_buffer);
    stream->pdata = NULL;
  } else {
    // Otherwise there was a closure.
    Closure* closure = static_cast<Closure*>(stream->notifyData);
    StreamShmBuffer* stream_buffer = closure->buffer();
    if (NULL != stream_buffer) {
      // There was a buffer attached, so we are in Chrome. Invoke its Run.
      // If we are not in Chrome, Run was invoked by StreamAsFile.
      closure->RunFromBuffer(stream->url, stream_buffer);
      delete stream_buffer;
    }
    delete closure;
    stream->notifyData = NULL;
  }
  return NPERR_NO_ERROR;
}

void PluginNpapi::URLNotify(const char* url,
                            NPReason reason,
                            void* notifyData) {
  PLUGIN_PRINTF(("PluginNpapi::URLNotify(%p, %s, %d, %p)\n",
                 static_cast<void*>(this), url, reason, notifyData));

  // The url should always be non-NULL.
  if (NULL == url) {
    PLUGIN_PRINTF(("URLNotify: FAILED: url was NULL.\n"));
    return;
  }
  // If we succeeded, there is nothing to do.
  if (NPRES_DONE == reason) {
    return;
  }
  // If the request failed, we need to report the failure.
  PLUGIN_PRINTF(("URLNotify: Unable to open: '%s' reason=%d\n", url, reason));
  if (NULL == notifyData) {
    // The implicit download failed, run the embed/object's onfail= handler.
    RunOnfailHandler();
  } else {
    // Convert the reason to a string and abuse the closure's Run method
    // slightly by passing that the reason as the file name.
    Closure* closure = static_cast<Closure*>(notifyData);
    nacl::stringstream msg;
    msg << "reason: " << reason;
    closure->RunFromFile(static_cast<NPStream*>(NULL), msg.str());
    delete closure;
  }
}

void PluginNpapi::set_module(nacl::NPModule* module) {
  PLUGIN_PRINTF(("PluginNpapi::set_module(%p, %p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(module)));
  delete module_;
  module_ = module;
  if (NULL != module_) {
    // Set the origins.
    module_->set_nacl_module_origin(nacl_module_origin());
    module_->set_origin(origin());
    // Initialize the NaCl module's NPAPI interface.
    // This should only be done for the first instance in a given group.
    module_->Initialize();
    // Create a new instance of that group.
    const char mime_type[] = "application/nacl-npapi-over-srpc";
    NPP npp = InstanceIdentifierToNPP(instance_id());
    NPError err = module->New(const_cast<char*>(mime_type),
                              npp,
                              argc(),
                              argn(),
                              argv());
    // Remember the scriptable version of the NaCl instance.
    err = module_->GetValue(npp,
                            NPPVpluginScriptableNPObject,
                            reinterpret_cast<void*>(&nacl_instance_));
    // Send an initial NPP_SetWindow to the plugin.
    NPWindow window;
    window.height = height();
    window.width = width();
    module->SetWindow(npp, &window);
  }
}

bool PluginNpapi::InitializeModuleMultimedia(ScriptableHandle* raw_channel,
                                             ServiceRuntime* service_runtime) {
  PLUGIN_PRINTF(("PluginNpapi::InitializeModuleMultimedia\n"));
  video_->Enable();
  multimedia_channel_ = new(std::nothrow) MultimediaSocket(browser_interface(),
                                                           service_runtime);
  if (NULL == multimedia_channel_) {
    PLUGIN_PRINTF(("PluginNpapi::InitializeModuleMultimedia: "
                   "MultimediaSocket channel construction failed.\n"));
    return false;
  }

  // Initialize the multimedia system.
  if (!multimedia_channel_->InitializeModuleMultimedia(
           this, raw_channel->handle())) {
    PLUGIN_PRINTF(("PluginNpapi::InitializeModuleMultimedia: "
                   "InitializeModuleMultimedia failed.\n"));
    delete multimedia_channel_;
    multimedia_channel_ = NULL;
    return false;
  }
  return true;
}

void PluginNpapi::ShutdownMultimedia() {
  PLUGIN_PRINTF(("PluginNpapi::ShutdownMultimedia (this=%p)\n",
                 static_cast<void*>(this)));
  delete multimedia_channel_;
  multimedia_channel_ = NULL;
}

void PluginNpapi::StartProxiedExecution(NaClSrpcChannel* srpc_channel) {
  // Check that the .nexe exports the NPAPI initialization method.
  NaClSrpcService* client_service = srpc_channel->client;
  if (NaClSrpcServiceMethodIndex(client_service, "NP_Initialize:ih:i") ==
      kNaClSrpcInvalidMethodIndex) {
    return;
  }
  nacl::NPModule* npmodule = new(std::nothrow) nacl::NPModule(srpc_channel);
  if (NULL != npmodule) {
    set_module(npmodule);
  }
}

bool PluginNpapi::RequestNaClModule(const nacl::string& url) {
  // Load the new module if the origin of the page is valid.
  PLUGIN_PRINTF(("Plugin::SetProperty src = '%s'\n", url.c_str()));
  LoadNaClAppNotify* callback = new(std::nothrow) LoadNaClAppNotify(this, url);
  if ((NULL == callback) || (!callback->StartDownload())) {
    PLUGIN_PRINTF(("Failed to load URL to local file.\n"));
    // callback is always deleted in URLNotify
    return false;
  }
  return true;
}

bool PluginNpapi::SetAsyncCallback(void* obj, SrpcParams* params) {
  PluginNpapi* plugin =
    static_cast<PluginNpapi*>(reinterpret_cast<Plugin*>(obj));
  if (plugin->service_runtime_ == NULL) {
    params->set_exception_string("No subprocess running");
    return false;
  }
  if (plugin->receive_thread_running_) {
    params->set_exception_string("A callback has already been registered");
    return false;
  }
  ReceiveThreadArgs* args = new(std::nothrow) ReceiveThreadArgs;
  if (args == NULL) {
    params->set_exception_string("Memory allocation failed");
    return false;
  }
  args->plugin = InstanceIdentifierToNPP(plugin->instance_id());
  args->callback = reinterpret_cast<NPObject*>(params->ins()[0]->u.oval);
  NPN_RetainObject(args->callback);
  nacl::DescWrapper* socket = plugin->service_runtime_->async_receive_desc();
  NaClDescRef(socket->desc());
  args->socket = plugin->wrapper_factory()->MakeGeneric(socket->desc());

  // It would be nice if the thread interface did not require us to
  // specify a stack size.  This is fairly arbitrary.
  size_t stack_size = 128 << 10;
  NaClThreadCreateJoinable(&plugin->receive_thread_, AsyncReceiveThread, args,
                           stack_size);
  plugin->receive_thread_running_ = true;
  return true;
}

}  // namespace plugin
