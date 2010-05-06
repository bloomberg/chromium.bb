/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/trusted/plugin/srpc/srpc.h"

#include <stdio.h>
#include <string.h>

#include <limits>
#include <set>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/shared/npruntime/npmodule.h"

#include "native_client/src/trusted/handle_pass/browser_handle.h"

#include "native_client/src/trusted/plugin/origin.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/closure.h"
#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"
#include "native_client/src/trusted/plugin/npinstance.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"
#include "native_client/src/trusted/plugin/srpc/video.h"

#include "native_client/src/trusted/service_runtime/gio_shm_unbounded.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"

using nacl::assert_cast;

std::set<const nacl_srpc::ScriptableHandleBase*>*
    nacl_srpc::ScriptableHandleBase::valid_handles = NULL;

namespace nacl {

static int32_t stringToInt32(char* src) {
  // TODO(adonovan): Why make a copy??
  char buf[32];
  strncpy(buf, src, sizeof buf);
  buf[sizeof buf - 1] = '\0';
  return strtol(buf,  // NOLINT(runtime/deprecated_fn)
                static_cast<char**>(NULL), 10);
}

StreamShmBuffer::StreamShmBuffer() {
  shmbufp_ = reinterpret_cast<NaClGioShmUnbounded *>(malloc(sizeof *shmbufp_));
  /* if NULL == shmbufp_, object is not initialized and no member fns run */
  if (NULL == shmbufp_) {
    dprintf(("StreamShmBuffer: malloc failed\n"));
    return;
  }
  if (!NaClGioShmUnboundedCtor(shmbufp_)) {
    dprintf(("StreamShmBuffer: NaClGioShmUnboundedCtor failed\n"));
    free(shmbufp_);
    shmbufp_ = NULL;
  }
}

StreamShmBuffer::~StreamShmBuffer() {
  if (NULL == shmbufp_)
    return;
  shmbufp_->base.vtbl->Dtor(&shmbufp_->base);
  free(shmbufp_);
}

int32_t StreamShmBuffer::write(int32_t offset, int32_t len, void* buf) {
  if (NULL == shmbufp_) {
    return 0;
  }
  shmbufp_->base.vtbl->Seek(&shmbufp_->base, offset, SEEK_SET);
  ssize_t rv = shmbufp_->base.vtbl->Write(&shmbufp_->base, buf, len);
  if (rv != len) {
    dprintf(("StreamShmBuffer::write returned %ld, not %ld\n",
             static_cast<long>(rv), static_cast<long>(len)));
  }
  return static_cast<int32_t>(rv);
}

int32_t StreamShmBuffer::read(int32_t offset, int32_t len, void* buf) {
  if (NULL == shmbufp_) {
    return 0;
  }
  shmbufp_->base.vtbl->Seek(&shmbufp_->base, offset, SEEK_SET);
  ssize_t rv = shmbufp_->base.vtbl->Read(&shmbufp_->base, buf, len);
  if (rv != len) {
    dprintf(("StreamShmBuffer::read returned %ld, not %ld\n",
             static_cast<long>(rv), static_cast<long>(len)));
  }
  return static_cast<int32_t>(rv);
}

NaClDesc* StreamShmBuffer::shm(int32_t* size) {
  size_t actual_size;
  NaClDesc *ndp;

  ndp = NaClGioShmUnboundedGetNaClDesc(shmbufp_, &actual_size);
  if (static_cast<size_t>(std::numeric_limits<int32_t>::max()) < actual_size) {
    // fail -- not representable
    *size = 0;
    return NULL;
  }
  *size = static_cast<int32_t>(actual_size);
  return ndp;
}

SRPC_Plugin::SRPC_Plugin(NPP npp, int argc, char* argn[], char* argv[])
    : npp_(npp),
      plugin_(NULL),
      module_(NULL),
      nacl_instance_(NULL) {
  dprintf(("SRPC_Plugin::SRPC_Plugin(%p, %d)\n",
           static_cast<void*>(this), argc));
  InitializeIdentifiers();
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  if (!NaClHandlePassBrowserCtor())
    return;
#endif
  // SRPC_Plugin initially gets exclusive ownership of plugin_.
  nacl_srpc::PortableHandleInitializer init_info(this);
  // Note to greppers: this is effectively new Plugin(); Plugin::Init().
  plugin_ = nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>::New(&init_info);
  if (NULL == plugin_) {
    dprintf(("SRPC_Plugin::SRPC_Plugin:"
             " ScriptableHandle::New returned null\n"));
    return;
  }
  // Remember the argn/argv pairs for NPAPI proxying
  npapi_argn_ = new(std::nothrow) char*[argc];
  npapi_argv_ = new(std::nothrow) char*[argc];
  npapi_argc_ = 0;
  // Set up the height and width attributes if passed (for Opera)
  for (int i = 0; i < argc; ++i) {
    if (!strncmp(argn[i], "height", 7)) {
      NPVariant variant;
      int32_t x = stringToInt32(argv[i]);
      INT32_TO_NPVARIANT(x, variant);
      nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>::SetProperty(plugin_,
          (NPIdentifier)PortablePluginInterface::kHeightIdent,
          &variant);
    } else if (!strncmp(argn[i], "width", 6)) {
      NPVariant variant;
      int32_t x = stringToInt32(argv[i]);
      INT32_TO_NPVARIANT(x, variant);
      nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>::SetProperty(plugin_,
        (NPIdentifier)PortablePluginInterface::kWidthIdent,
        &variant);
    } else if (!strncmp(argn[i], "update", 7)) {
      NPVariant variant;
      int32_t x = stringToInt32(argv[i]);
      INT32_TO_NPVARIANT(x, variant);
      nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>::SetProperty(plugin_,
        (NPIdentifier)PortablePluginInterface::kVideoUpdateModeIdent,
        &variant);
    } else {
      if (NULL != npapi_argn_ && NULL != npapi_argv_) {
        npapi_argn_[npapi_argc_] = strdup(argn[i]);
        npapi_argv_[npapi_argc_] = strdup(argv[i]);
        if (NULL == npapi_argn_[npapi_argc_] ||
            NULL == npapi_argv_[npapi_argc_]) {
          // Give up on passing arguments.
          free(npapi_argn_[npapi_argc_]);
          free(npapi_argv_[npapi_argc_]);
          continue;
        }
        ++npapi_argc_;
      }
    }
  }

  // Give the Plugin a chance to inspect its arg[nv] arrays, which
  // aren't set until after Init() has already returned.
  static_cast<nacl_srpc::Plugin*>(plugin_->get_handle())->PostInit();

  video_ = new(std::nothrow) VideoMap(this);
  if (NULL == video_) {
    // TODO(sehr): Move this setup to an Init method that can fail.
    plugin_->Unref();
    plugin_ = NULL;
  }

  dprintf(("SRPC_Plugin::SRPC_Plugin: done, plugin_ %p, video_ %p\n",
           static_cast<void*>(plugin_),
           static_cast<void*>(video_)));
}

void SRPC_Plugin::set_module(nacl::NPModule* module) {
  dprintf(("Setting module pointer to %p\n", static_cast<void*>(module)));
  module_ = module;
  if (NULL != module) {
    // Set the origins.
    nacl_srpc::Plugin* plugin =
        static_cast<nacl_srpc::Plugin*>(plugin_->get_handle());
    module->set_nacl_module_origin(plugin->nacl_module_origin());
    module->set_origin(plugin->origin());
    // Initialize the NaCl module's NPAPI interface.
    // This should only be done for the first instance in a given group.
    module->Initialize();
    // Create a new instance of that group.
    const char mime_type[] = "application/nacl-npapi-over-srpc";
    NPError err = module->New(const_cast<char*>(mime_type),
                              npp_,
                              argc(),
                              argn(),
                              argv());
    // Remember the scriptable version of the NaCl instance.
    err = module_->GetValue(npp_,
                            NPPVpluginScriptableNPObject,
                            reinterpret_cast<void*>(&nacl_instance_));
    // Send an initial NPP_SetWindow to the plugin.
    NPWindow window;
    window.height = plugin->height();
    window.width = plugin->width();
    module->SetWindow(npp_, &window);
  }
}

SRPC_Plugin::~SRPC_Plugin() {
  nacl_srpc::ScriptableHandle<nacl_srpc::Plugin>* plugin = plugin_;

#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  NaClHandlePassBrowserDtor();
#endif

  // TODO(nfullagar): please explain why plugin is needed here.  why
  // does plugin_ need to be set to NULL -- but the object not
  // Unref'd, while holding the video global lock?
  dprintf(("SRPC_Plugin::~SRPC_Plugin plugin_ is %p\n",
           static_cast<void* >(plugin_)));
  if (NULL != plugin_) {
    dprintf(("SRPC_Plugin::~SRPC_Plugin plugin_->get_handle() is %p\n",
             static_cast<void* >(plugin_->get_handle())));
  }
  // TODO(nfullagar): is it possible for video_ to be non-NULL while
  // plugin_ is NULL?  could these three blocks be fused?
  /* SCOPE */ {
    VideoScopedGlobalLock video_lock;
    dprintf(("SRPC_Plugin::~SRPC_Plugin(%p)\n", static_cast<void*>(this)));
    plugin_ = NULL;
    dprintf(("SRPC_Plugin::~SPRC_Plugin deleting video_\n"));
    if (NULL != video_) {
      delete video_;
      video_ = NULL;
    }
  }
  // Delete the NPModule for this plugin.
  if (NULL != module_) {
    delete module_;
    module_ = NULL;
  }
  // don't hold scoped global lock while calling NPN_ReleaseObject
  if (NULL != plugin) {
    // Destroying SRPC_Plugin releases ownership of the plugin.
    plugin->Unref();
  }
}

NPError SRPC_Plugin::Destroy(NPSavedData** save) {
  dprintf(("SRPC_Plugin::Destroy(%p, %p)\n", static_cast<void*>(this),
           static_cast<void*>(save)));
  delete this;
  return NPERR_NO_ERROR;
}

// SetWindow is called by the browser as part of the NPAPI interface for
// setting up a plugin that has the ability to draw into a window.  It is
// passed a semi-custom window descriptor (some is platform-neutral, some not)
// as documented in the NPAPI documentation.
NPError SRPC_Plugin::SetWindow(NPWindow* window) {
  NPError ret = NPERR_GENERIC_ERROR;
  dprintf(("SRPC_Plugin::SetWindow(%p, %p)\n", static_cast<void* >(this),
           static_cast<void*>(window)));
  if (NULL == module_) {
    if (video_ && video_->SetWindow(window)) {
        ret = NPERR_NO_ERROR;
    }
    return ret;
  } else {
    // Send NPP_SetWindow to NPModule.
    return module_->SetWindow(npp_, window);
  }
}

NPError SRPC_Plugin::GetValue(NPPVariable variable, void* value) {
  const char** stringp = static_cast<const char**>(value);

  dprintf(("SRPC_Plugin::GetValue(%p, %d)\n", static_cast<void*>(this),
           variable));

  switch (variable) {
    case NPPVpluginNameString:
      *stringp = "NativeClient Simple RPC + multimedia a/v interface";
      return NPERR_NO_ERROR;
    case NPPVpluginDescriptionString:
      *stringp = "NativeClient Simple RPC interaction w/ multimedia.";
      return NPERR_NO_ERROR;
    case NPPVpluginScriptableNPObject:
      *(static_cast<NPObject**>(value)) = GetScriptableInstance();
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

int16_t SRPC_Plugin::HandleEvent(void* param) {
  int16_t ret;
  dprintf(("SRPC_Plugin::HandleEvent(%p, %p)\n", static_cast<void*>(this),
           static_cast<void*>(param)));
  if (NULL == module_) {
    if (video_) {
      ret = video_->HandleEvent(param);
    } else {
      ret = 0;
    }
  } else {
    return module_->HandleEvent(npp_, param);
  }
  return ret;
}

NPObject* SRPC_Plugin::GetScriptableInstance() {
  dprintf(("SRPC_Plugin::GetScriptableInstance(%p)\n",
           static_cast<void*>(this)));

  // Anyone requesting access to the scriptable instance is given shared
  // ownership of plugin_.
  return NPN_RetainObject(plugin_);
}

NPError SRPC_Plugin::NewStream(NPMIMEType type,
                               NPStream* stream,
                               NPBool seekable,
                               uint16_t* stype) {
  dprintf(("SRPC_Plugin::NewStream(%p, %s, %p, %d)\n",
           static_cast<void*>(this), type, static_cast<void*>(stream),
           seekable));
#ifdef NACL_STANDALONE
  *stype = NP_ASFILEONLY;
#else
  // When running as a built-in plugin in Chrome we cannot access the
  // file system, therefore we use normal streams to get the data.
  *stype = NP_NORMAL;
#endif
  return NPERR_NO_ERROR;
}

int32_t SRPC_Plugin::WriteReady(NPStream* stream) {
  UNREFERENCED_PARAMETER(stream);
  return 32 * 1024;
}

int32_t SRPC_Plugin::Write(NPStream* stream,
                           int32_t offset,
                           int32_t len,
                           void* buf) {
  StreamShmBuffer* stream_buffer = NULL;
  if (NULL == stream->pdata) {
    stream_buffer = new(std::nothrow) StreamShmBuffer();
    stream->pdata = reinterpret_cast<void*>(stream_buffer);
  } else {
    stream_buffer = reinterpret_cast<StreamShmBuffer*>(stream->pdata);
  }
  if (NULL == stream_buffer) {
    return 0;
  }

  int32_t written = stream_buffer->write(offset, len, buf);
  if (NULL == stream->notifyData) {
    // Closures are handled in URLNotify (this is the only way to know
    // the stream is completed since not all streams have a valid "end" value
    // Here we handle only the default, src=...  streams (statically obtained)
    if (static_cast<int32_t>(stream->end) == offset + len) {
      // Stream downloaded - go ahead
      dprintf(("default run\n"));
      nacl_srpc::Plugin* real_plugin =
        static_cast<nacl_srpc::Plugin*>(plugin()->get_handle());
      real_plugin->Load(stream->url,
                        stream->url,
                        stream_buffer);
      delete(stream_buffer);
      stream->pdata = NULL;
    }
  } else {
    nacl_srpc::Closure* closure
      = static_cast<nacl_srpc::Closure*>(stream->notifyData);
    // NPStream is deleted before URLNotify is called, so we need to keep
    // the buffer
    closure->set_buffer(stream_buffer);
  }

  return written;
}

void SRPC_Plugin::StreamAsFile(NPStream* stream,
                               const char* fname) {
  dprintf(("SRPC_Plugin::StreamAsFile(%p, %p, %s)\n",
           static_cast<void*>(this), static_cast<void*>(stream), fname));
  nacl_srpc::Closure* closure
      = static_cast<nacl_srpc::Closure*>(stream->notifyData);

  if (NULL != closure) {
    closure->Run(stream, fname);
  } else {
    // default, src=... statically obtained
    dprintf(("default run\n"));
    nacl_srpc::Plugin* real_plugin =
        static_cast<nacl_srpc::Plugin*>(plugin_->get_handle());
    real_plugin->Load(stream->url, fname);
  }
}

NPError SRPC_Plugin::DestroyStream(NPStream* stream,
                                   NPReason reason) {
  dprintf(("SRPC_Plugin::DestroyStream(%p, %p, %d)\n",
           static_cast<void*>(this), static_cast<void*>(stream), reason));

  return NPERR_NO_ERROR;
}

void SRPC_Plugin::URLNotify(const char* url,
                            NPReason reason,
                            void* notifyData) {
  dprintf(("SRPC_Plugin::URLNotify(%p, %s, %d, %p)\n",
           static_cast<void*>(this), url, reason, notifyData));

  // TODO(sehr): use autoptr to avoid delete.
  nacl_srpc::Closure* closure
      = static_cast<nacl_srpc::Closure*>(notifyData);

  if (NPRES_DONE == reason) {
    // Note, we cannot access the HTTP status code so we might have
    // been returned a 404 error page.  This is reported in the ELF
    // validity checks.
    dprintf(("URLNotify: '%s', reason=NPRES_DONE (%d)\n", url, reason));
    StreamShmBuffer* stream_buffer = closure->buffer();
    if (NULL != stream_buffer) {
      // NPStream is not valid here since DestroyStream was called earlier
      closure->Run(url, stream_buffer);
      delete(stream_buffer);
      closure->set_buffer(NULL);
    }
  } else {
    dprintf(("Unable to open: '%s' reason=%d\n", url, reason));
    if (NULL != closure) {
      // BUG(adonovan): there's a bug here (issue #430).  The two arms
      // of if(reason==...)  above make inconsistent assumptions about
      // the nullity of "closure": the true branch assumes non-NULL,
      // the false branch assumes possibly- NULL.  But "reason" is
      // independent of "closure", so at best we don't call onfail();
      // at worst we invoke closure->Run() on a null pointer.

      // TODO(sehr): this loses the browser's reason for failure.
      closure->Run(static_cast<NPStream*>(NULL),
                   static_cast<const char*>(NULL));
    } else {
      plugin()->get_handle()->GetPortablePluginInterface()->RunOnfailHandler();
    }
  }
  delete closure;  // NB: delete NULL is okay
}

}  // namespace nacl
