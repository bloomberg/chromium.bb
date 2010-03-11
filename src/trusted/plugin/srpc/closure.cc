/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <string.h>
#include <string>

#include "native_client/src/shared/npruntime/npmodule.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"

#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/srpc/npapi_native.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

#include "native_client/src/trusted/plugin/origin.h"
#include "native_client/src/trusted/plugin/srpc/closure.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/shared_memory.h"
#include "native_client/src/trusted/plugin/srpc/srpc.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"

struct NPObject;

namespace nacl_srpc {

bool Closure::StartDownload() {
  dprintf(("StartDownload plugin_identifier_=%p, url_=%s, this=%p\n",
           reinterpret_cast<void*>(plugin_identifier_),
           url_.c_str(),
           reinterpret_cast<void*>(this)));
  NPError err = NPN_GetURLNotify(plugin_identifier_, url_.c_str(), NULL, this);
  return (NPERR_NO_ERROR == err);
}

LoadNaClAppNotify::LoadNaClAppNotify(Plugin* plugin, std::string url)
    : Closure(plugin, url) {
  dprintf(("LoadNaClAppNotify ctor\n"));
}

LoadNaClAppNotify::~LoadNaClAppNotify() {
  dprintf(("LoadNaClAppNotify dtor\n"));
}

void LoadNaClAppNotify::Run(NPStream* stream, const char* fname) {
  dprintf(("LoadNaClAppNotify Run %p, %s\n",
           static_cast<void*>(stream),
           fname));
  if (NULL != fname) {
    plugin()->Load(stream->url, fname);
  }
}

void LoadNaClAppNotify::Run(const char *url,
                            const void* buffer,
                            int32_t size) {
  dprintf(("LoadNaClAppNotify Run %s, %p, %x\n", url, buffer, size));
  if (NULL != buffer) {
    plugin()->Load(url, url, buffer, size);
  }
}


UrlAsNaClDescNotify::UrlAsNaClDescNotify(Plugin* plugin,
                                         std::string url,
                                         void* callback_obj) :
    Closure(plugin, url),
    np_callback_(reinterpret_cast<NPObject*>(callback_obj)) {
  dprintf(("UrlAsNaClDescNotify ctor\n"));
  NPN_RetainObject(np_callback_);
}

UrlAsNaClDescNotify::~UrlAsNaClDescNotify() {
  dprintf(("UrlAsNaClDescNotify dtor\n"));
  NPN_ReleaseObject(np_callback_);
  np_callback_ = NULL;
}

void UrlAsNaClDescNotify::Run(NPStream *stream, const char *fname) {
  // open file in DescWrapper, make available via np_callback_
  NPVariant retval;
  NPVariant status;
  NPObject *nacl_desc = NULL;
  NPIdentifier callback_selector =
      (NPIdentifier)PortablePluginInterface::kOnfailIdent;

  dprintf(("UrlAsNaClDescNotify::Run(%p, %s)\n",
           static_cast<void *>(stream),
           fname));

  VOID_TO_NPVARIANT(retval);
  VOID_TO_NPVARIANT(status);

  // execute body once; construct to use break statement to exit body early
  do {
    if (NULL == fname) {
      dprintf(("fetch failed\n"));
      ScalarToNPVariant("Same origin violation", &status);
      break;
    }

    dprintf(("fetched FQ URL %s\n", stream->url));
    std::string url_origin = nacl::UrlToOrigin(stream->url);
    if (url_origin != plugin()->origin()) {
      dprintf(("same origin policy forbids access: "
        " page from origin %s attempted to"
        " fetch page with origin %s\n",
        plugin()->origin().c_str(),
        url_origin.c_str()));
      ScalarToNPVariant("Same origin violation", &status);
      break;
    }

    nacl::DescWrapper* ndiod =
        plugin()->wrapper_factory()->OpenHostFile(const_cast<char *>(fname),
                                                  NACL_ABI_O_RDONLY,
                                                  0);
    if (NULL == ndiod) {
      dprintf(("NaClHostDescOpen failed\n"));

      ScalarToNPVariant("NaClHostDescOpen failed", &status);
      break;
    }
    dprintf(("created ndiod %p\n",
             static_cast<void *>(ndiod)));
    DescHandleInitializer init_info(plugin()->GetPortablePluginInterface(),
                                    ndiod,
                                    plugin());

    nacl_desc = ScriptableHandle<DescBasedHandle>::New(&init_info);
    // nacl_desc takes ownership of ndiod.
    callback_selector = (NPIdentifier)PortablePluginInterface::kOnloadIdent;

    ScalarToNPVariant(static_cast<NPObject*>(nacl_desc), &status);
    // NPVariant takes ownership of NPObject nacl_desc
  } while (0);

  dprintf(("calling np_callback_ %p, nacl_desc %p, status %p\n",
           static_cast<void *>(np_callback_),
           static_cast<void *>(nacl_desc),
           static_cast<void *>(&status)));
  NPN_Invoke(plugin()->GetPortablePluginInterface()->GetPluginIdentifier(),
             np_callback_,
             callback_selector,
             &status,
             1,
             &retval);

  dprintf(("releasing status %p\n", static_cast<void *>(&status)));
  NPN_ReleaseVariantValue(&status);
  NPN_ReleaseVariantValue(&retval);
}

void UrlAsNaClDescNotify::Run(const char *url,
                              const void* buffer,
                              int32_t size) {
  // create a SharedMemory object, make it available via np_callback_
  NPVariant retval;
  NPVariant status;
  NPObject *nacl_desc = NULL;
  NPIdentifier callback_selector =
    (NPIdentifier)PortablePluginInterface::kOnfailIdent;

  dprintf(("UrlAsNaClDescNotify::Run(%s, %p, %x )\n", url, buffer, size));

  VOID_TO_NPVARIANT(retval);
  VOID_TO_NPVARIANT(status);

  // execute body once; construct to use break statement to exit body early
  do {
    if (NULL == buffer) {
      dprintf(("bad buffer - stream handling failed\n"));
      ScalarToNPVariant("Same origin violation", &status);
      break;
    }

    dprintf(("fetched FQ URL %s\n", url));
    std::string url_origin = nacl::UrlToOrigin(url);
    if (url_origin != plugin()->origin()) {
      dprintf(("same origin policy forbids access: "
              " page from origin %s attempted to"
              " fetch page with origin %s\n",
              plugin()->origin().c_str(),
              url_origin.c_str()));
      ScalarToNPVariant("Same origin violation", &status);
      break;
    }

    // Create SharedMemory and copy the data
    // TODO(gregoryd): can we create SharedMemory based on an existing buffer
    // (both here and in ServiceRuntimeInterface::Start)
    SharedMemoryInitializer init_info(plugin()->GetPortablePluginInterface(),
                                      plugin(),
                                      size);
    ScriptableHandle<SharedMemory> *shared_memory =
        ScriptableHandle<SharedMemory>::New(&init_info);

    SharedMemory *real_shared_memory =
      static_cast<SharedMemory*>(shared_memory->get_handle());
    // TODO(gregoryd): another option is to export a Write() function
    // from SharedMemory
    memcpy(real_shared_memory->buffer(), buffer, size);

    callback_selector = (NPIdentifier)PortablePluginInterface::kOnloadIdent;

    ScalarToNPVariant(static_cast<NPObject*>(shared_memory), &status);
    // NPVariant takes ownership of NPObject nacl_desc
  } while (0);

  dprintf(("calling np_callback_ %p, nacl_desc %p, status %p\n",
          static_cast<void *>(np_callback_),
          static_cast<void *>(nacl_desc),
          static_cast<void *>(&status)));
  NPN_Invoke(plugin()->GetPortablePluginInterface()->GetPluginIdentifier(),
             np_callback_,
             callback_selector,
             &status,
             1,
             &retval);

  dprintf(("releasing status %p\n", static_cast<void *>(&status)));
  NPN_ReleaseVariantValue(&status);
  NPN_ReleaseVariantValue(&retval);
}

NpGetUrlClosure::NpGetUrlClosure(NPP npp,
                                 nacl::NPModule* module,
                                 std::string url) :
  Closure(NULL, url), module_(module), npp_(npp) {
  nacl::SRPC_Plugin* srpc = reinterpret_cast<nacl::SRPC_Plugin*>(npp->pdata);
  Plugin* plugin = static_cast<Plugin*>(srpc->plugin()->get_handle());
  set_plugin(plugin);
  dprintf(("NpGetUrlClosure ctor\n"));
}

NpGetUrlClosure::~NpGetUrlClosure() {
  dprintf(("NpGetUrlClosure dtor\n"));
  module_ = NULL;
  npp_ = NULL;
}

void NpGetUrlClosure::Run(NPStream* stream, const char* fname) {
  // open file in DescWrapper, make available via np_callback_
  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* ndiod = NULL;

  dprintf(("NpGetUrlClosure::Run(%p, %s)\n",
           static_cast<void *>(stream),
           fname));

  // execute body once; construct to use break statement to exit body early
  do {
    if (NULL == fname) {
      dprintf(("fetch failed\n"));
      break;
    }

    dprintf(("fetched FQ URL %s\n", stream->url));
    std::string url_origin = nacl::UrlToOrigin(stream->url);
    if (url_origin != module_->origin()) {
      dprintf(("same origin policy forbids access: "
        " page from origin %s attempted to"
        " fetch page with origin %s\n",
        module_->origin().c_str(),
        url_origin.c_str()));
      break;
    }

    ndiod = factory.OpenHostFile(const_cast<char *>(fname),
                                 NACL_ABI_O_RDONLY,
                                 0);
    if (NULL == ndiod) {
      dprintf(("NaClHostDescOpen failed\n"));
      break;
    }
    dprintf(("created ndiod %p\n", static_cast<void *>(ndiod)));
  } while (0);

  if (NULL == ndiod || NULL == stream) {
    NaClDesc* invalid =
        const_cast<NaClDesc*>(
            reinterpret_cast<const NaClDesc*>(NaClDescInvalidMake()));
    module_->StreamAsFile(npp_, invalid, const_cast<char*>(stream->url), 0);
  } else {
    module_->StreamAsFile(npp_,
                          ndiod->desc(),
                          const_cast<char*>(stream->url),
                          stream->end);
    ndiod->Delete();
  }
}

void NpGetUrlClosure::Run(const char* url, const void* buffer, int32_t size) {
  // create a SharedMemory object, make it available via np_callback_
  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* ndshm = NULL;
  dprintf(("NpGetUrlClosure::Run(%s, %p, %x )\n", url, buffer, size));

  // execute body once; construct to use break statement to exit body early
  do {
    if (NULL == buffer || NULL == url) {
      dprintf(("bad buffer or URL - stream handling failed\n"));
      break;
    }

    dprintf(("fetched FQ URL %s\n", url));
    std::string url_origin = nacl::UrlToOrigin(url);
    if (url_origin != module_->origin()) {
      dprintf(("same origin policy forbids access: "
              " page from origin %s attempted to"
              " fetch page with origin %s\n",
              module_->origin().c_str(),
              url_origin.c_str()));
      break;
    }

    // Create SharedMemory and copy the data
    ndshm = factory.MakeShm(static_cast<size_t>(size));
    if (NULL == ndshm) {
      dprintf(("NaClHostDescOpen failed\n"));
      break;
    }
    dprintf(("created ndshm %p\n", static_cast<void *>(ndshm)));
    void* map_addr = NULL;
    size_t map_size = static_cast<size_t>(size);
    static const size_t kNaClAbiSizeTMax =
        static_cast<size_t>(~static_cast<nacl_abi_size_t>(0));
    if (kNaClAbiSizeTMax < map_size) {
      // There's no point in mapping a file that's larger than could be
      // accessed by NaCl module with 4GB of range.
      // TODO(sehr,bsy): this should probably be even tighter.
      break;
    }
    if (0 > ndshm->Map(&map_addr, &map_size)) {
      ndshm->Delete();
      ndshm = NULL;
      break;
    }
    memcpy(map_addr, buffer, size);
    ndshm->Unmap(map_addr, static_cast<size_t>(size));
    dprintf(("copied the data\n"));
  } while (0);

  if (NULL == ndshm) {
    NaClDesc* invalid =
        const_cast<NaClDesc*>(
            reinterpret_cast<const NaClDesc*>(NaClDescInvalidMake()));
    module_->StreamAsFile(npp_, invalid, const_cast<char*>(url), 0);
  } else {
    module_->StreamAsFile(npp_, ndshm->desc(), const_cast<char*>(url), size);
    ndshm->Delete();
  }
}

}  // namespace nacl_srpc
