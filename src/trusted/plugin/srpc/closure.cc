/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <string.h>

#include "native_client/src/include/nacl_string.h"
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
  dprintf(("StartDownload plugin_identifier_=%p, requested_url_=%s, this=%p\n",
           reinterpret_cast<void*>(plugin_identifier_),
           requested_url_.c_str(),
           reinterpret_cast<void*>(this)));
  NPError err = NPN_GetURLNotify(plugin_identifier_,
                                 requested_url_.c_str(),
                                 NULL,
                                 this);
  return (NPERR_NO_ERROR == err);
}

LoadNaClAppNotify::LoadNaClAppNotify(Plugin* plugin, nacl::string url)
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
                            nacl::StreamShmBuffer* shmbufp) {
  dprintf(("LoadNaClAppNotify Run %s, %p\n", url,
           static_cast<void*>(shmbufp)));
  if (NULL != shmbufp) {
    plugin()->Load(url, url, shmbufp);
  }
}


UrlAsNaClDescNotify::UrlAsNaClDescNotify(Plugin* plugin,
                                         nacl::string url,
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
    if (NULL == fname || NULL == stream) {
      dprintf(("fetch failed\n"));
      ScalarToNPVariant("URL get failed", &status);
      break;
    }

    dprintf(("fetched FQ URL %s\n", stream->url));
    nacl::string url_origin = nacl::UrlToOrigin(stream->url);
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
                              nacl::StreamShmBuffer* shmbufp) {
  // create a SharedMemory object, make it available via np_callback_
  NPVariant retval;
  NPVariant status;
  NPObject *nacl_desc = NULL;
  NPIdentifier callback_selector =
    (NPIdentifier)PortablePluginInterface::kOnfailIdent;

  dprintf(("UrlAsNaClDescNotify::Run(%s, %p )\n", url,
           static_cast<void*>(shmbufp)));

  VOID_TO_NPVARIANT(retval);
  VOID_TO_NPVARIANT(status);

  // execute body once; construct to use break statement to exit body early
  do {
    if (NULL == shmbufp) {
      dprintf(("bad buffer - stream handling failed\n"));
      ScalarToNPVariant("Same origin violation", &status);
      break;
    }

    dprintf(("fetched FQ URL %s\n", url));
    nacl::string url_origin = nacl::UrlToOrigin(url);
    if (url_origin != plugin()->origin()) {
      dprintf(("same origin policy forbids access: "
              " page from origin %s attempted to"
              " fetch page with origin %s\n",
              plugin()->origin().c_str(),
              url_origin.c_str()));
      ScalarToNPVariant("Same origin violation", &status);
      break;
    }

    int32_t size;
    NaClDesc *raw_desc = shmbufp->shm(&size);
    if (NULL == raw_desc) {
      dprintf((" extracting shm failed\n"));
      break;
    }
    nacl::DescWrapper *wrapped_shm =
        plugin()->wrapper_factory()->MakeGeneric(NaClDescRef(raw_desc));
    SharedMemoryInitializer init_info(plugin()->GetPortablePluginInterface(),
                                      wrapped_shm,
                                      plugin());
    ScriptableHandle<SharedMemory> *shared_memory =
        ScriptableHandle<SharedMemory>::New(&init_info);

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
                                 nacl::string url,
                                 int32_t notify_data,
                                 bool call_url_notify) :
  Closure(NULL, url),
  module_(module),
  npp_(npp),
  notify_data_(notify_data),
  call_url_notify_(call_url_notify) {
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
    if (NULL == fname || NULL == stream) {
      dprintf(("fetch failed\n"));
      break;
    }

    dprintf(("fetched FQ URL %s\n", stream->url));
    nacl::string url_origin = nacl::UrlToOrigin(stream->url);
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

  // The following two variables are passed when NPP_URLNotify is invoked.
  // Both default to the values to be passed when the requested NPN_GetURL*
  // fails.  We do not know the reason from the browser, so we use a default.
  NPReason notify_reason = NPRES_NETWORK_ERR;
  // On error, we return the requested URL.
  char* notify_url = const_cast<char*>(requested_url().c_str());

  // If successful, invoke NPP_StreamAsFile.
  if (NULL != ndiod) {
    module_->StreamAsFile(npp_,
                          ndiod->desc(),
                          const_cast<char*>(stream->url),
                          stream->end);
    ndiod->Delete();
    // We return success and the version of the URL that the browser returns.
    // The latter is typically the fully qualified URL for the request.
    notify_reason = NPRES_DONE;
    notify_url = const_cast<char*>(stream->url);
  }
  // If the user requested a notification, invoke NPP_URLNotify.
  if (call_url_notify_) {
    module_->URLNotify(npp_,
                       notify_url,
                       notify_reason,
                       reinterpret_cast<void*>(notify_data_));
  }
}

void NpGetUrlClosure::Run(const char* url, nacl::StreamShmBuffer* shmbufp) {
  // create a SharedMemory object, make it available via np_callback_
  nacl::DescWrapperFactory factory;
  dprintf(("NpGetUrlClosure::Run(%s, %p)\n", url,
           static_cast<void*>(shmbufp)));

  // execute body once; construct to use break statement to exit body early
  do {
    if (NULL == shmbufp || NULL == url) {
      dprintf(("bad buffer or URL - stream handling failed\n"));
      break;
    }

    dprintf(("fetched FQ URL %s\n", url));
    nacl::string url_origin = nacl::UrlToOrigin(url);
    if (url_origin != module_->origin()) {
      dprintf(("same origin policy forbids access: "
              " page from origin %s attempted to"
              " fetch page with origin %s\n",
              module_->origin().c_str(),
              url_origin.c_str()));
      break;
    }
  } while (0);

  // The following two variables are passed when NPP_URLNotify is invoked.
  // Both default to the values to be passed when the requested NPN_GetURL*
  // fails.  We do not know the reason from the browser, so we use a default.
  NPReason notify_reason = NPRES_NETWORK_ERR;
  // On error, we return the requested URL.
  char* notify_url = const_cast<char*>(requested_url().c_str());

  // If successful, invoke NPP_StreamAsFile.
  if (NULL != shmbufp) {
    int32_t size;
    NaClDesc *raw_desc = shmbufp->shm(&size);
    if (NULL == raw_desc) {
      dprintf((" extracting shm failed\n"));
      return;
    }
    nacl::DescWrapper *wrapped_shm =
        plugin()->wrapper_factory()->MakeGeneric(NaClDescRef(raw_desc));
    module_->StreamAsFile(npp_,
                          wrapped_shm->desc(),
                          const_cast<char*>(url),
                          size);
    wrapped_shm->Delete();
    // We return success and the version of the URL that the browser returns.
    // The latter is typically the fully qualified URL for the request.
    notify_reason = NPRES_DONE;
    notify_url = const_cast<char*>(url);
  }
  // If the user requested a notification, invoke NPP_URLNotify.
  if (call_url_notify_) {
    module_->URLNotify(npp_,
                       notify_url,
                       notify_reason,
                       reinterpret_cast<void*>(notify_data_));
  }
}

}  // namespace nacl_srpc
