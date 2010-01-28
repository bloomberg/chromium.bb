/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <string.h>
#include <string>

#include "native_client/src/shared/platform/nacl_host_desc.h"

#include "native_client/src/trusted/plugin/srpc/npapi_native.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"

#include "native_client/src/trusted/plugin/origin.h"
#include "native_client/src/trusted/plugin/srpc/closure.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/shared_memory.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"

struct NPObject;

namespace nacl_srpc {

bool Closure::StartDownload() {
  NPError err = NPN_GetURLNotify(
      plugin_->GetPortablePluginInterface()->GetPluginIdentifier(),
      url_.c_str(),
      NULL,
      this);
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

}  // namespace nacl_srpc
