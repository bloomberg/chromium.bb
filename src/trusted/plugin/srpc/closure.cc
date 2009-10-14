/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
    plugin()->set_nacl_module_origin(nacl::UrlToOrigin(stream->url));
    plugin()->set_local_url(fname);
    plugin()->Load();
  }
}

void LoadNaClAppNotify::Run(const char *url,
                            const void* buffer,
                            int32_t size) {
  dprintf(("LoadNaClAppNotify Run %s, %p, %x\n", url, buffer, size));
  if (NULL != buffer) {
    plugin()->set_nacl_module_origin(nacl::UrlToOrigin(url));
    plugin()->set_local_url(url);
    plugin()->Load(buffer, size);
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
  // open file as NaClHostDesc, create NaClDesc object, make available
  // via np_callback_
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

    NaClHostDesc *nhd = static_cast<NaClHostDesc *>(malloc(sizeof *nhd));
    if (NULL == nhd) {
      dprintf(("no memory for nhd\n"));
      // TODO(bsy) failure callback

      ScalarToNPVariant("No memory for NaClHostDesc object", &status);
      break;
    }
    int oserr = NaClHostDescOpen(nhd, const_cast<char *>(fname),
      NACL_ABI_O_RDONLY, 0);
    if (0 != oserr) {
      dprintf(("NaClHostDescOpen failed, NaCl error %d\n", oserr));
      free(nhd);

      ScalarToNPVariant("NaClHostDescOpen failed", &status);
      break;
    }
    NaClDescIoDesc *ndiod = NaClDescIoDescMake(nhd);  // takes ownership of nhd
    if (NULL == ndiod) {
      dprintf(("no memory for ndiod\n"));
      NaClHostDescClose(nhd);
      free(nhd);

      ScalarToNPVariant("No memory for NaClDescIoDesc object", &status);
      break;
    }
    dprintf(("created ndiod %p\n",
             static_cast<void *>(ndiod)));
    DescHandleInitializer init_info(plugin()->GetPortablePluginInterface(),
                                    reinterpret_cast<NaClDesc *>(ndiod),
                                    plugin());

    nacl_desc = ScriptableHandle<DescBasedHandle>::New(&init_info);
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
