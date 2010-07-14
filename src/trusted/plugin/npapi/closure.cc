/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/trusted/plugin/npapi/closure.h"

#include <string.h>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/npruntime/npmodule.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/npapi/browser_impl_npapi.h"
#include "native_client/src/trusted/plugin/npapi/npapi_native.h"
#include "native_client/src/trusted/plugin/npapi/plugin_npapi.h"
#include "native_client/src/trusted/plugin/npapi/scriptable_impl_npapi.h"
#include "native_client/src/trusted/plugin/origin.h"
#include "native_client/src/trusted/plugin/srpc/desc_based_handle.h"
#include "native_client/src/trusted/plugin/srpc/scriptable_handle.h"
#include "native_client/src/trusted/plugin/srpc/shared_memory.h"
#include "native_client/src/trusted/plugin/srpc/stream_shm_buffer.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"

struct NPObject;

namespace plugin {

bool Closure::StartDownload() {
  PLUGIN_PRINTF(("StartDownload npp_=%p, requested_url_=%s, this=%p\n",
                 reinterpret_cast<void*>(npp_),
                 requested_url_.c_str(),
                 reinterpret_cast<void*>(this)));
  NPError err = NPN_GetURLNotify(npp_,
                                 requested_url_.c_str(),
                                 NULL,
                                 this);
  return (NPERR_NO_ERROR == err);
}

LoadNaClAppNotify::LoadNaClAppNotify(Plugin* plugin, nacl::string url)
    : Closure(plugin, url) {
  PLUGIN_PRINTF(("LoadNaClAppNotify ctor\n"));
}

LoadNaClAppNotify::~LoadNaClAppNotify() {
  PLUGIN_PRINTF(("LoadNaClAppNotify dtor\n"));
}

void LoadNaClAppNotify::RunFromFile(NPStream* stream,
                                    const nacl::string& fname) {
  PLUGIN_PRINTF(("LoadNaClAppNotify::RunFromFile(%p, %s)\n",
                 static_cast<void*>(stream),
                 fname.c_str()));
  plugin()->Load(stream->url, fname.c_str());
}

void LoadNaClAppNotify::RunFromBuffer(const nacl::string& url,
                                      StreamShmBuffer* shmbufp) {
  PLUGIN_PRINTF(("LoadNaClAppNotify RunFromBuffer(%s, %p)\n", url.c_str(),
                 static_cast<void*>(shmbufp)));
  if (NULL != shmbufp) {
    plugin()->Load(url.c_str(), url.c_str(), shmbufp);
  }
}

UrlAsNaClDescNotify::UrlAsNaClDescNotify(Plugin* plugin,
                                         nacl::string url,
                                         void* callback_obj) :
    Closure(plugin, url),
    np_callback_(reinterpret_cast<NPObject*>(callback_obj)) {
  PLUGIN_PRINTF(("UrlAsNaClDescNotify ctor\n"));
  NPN_RetainObject(np_callback_);
}

UrlAsNaClDescNotify::~UrlAsNaClDescNotify() {
  PLUGIN_PRINTF(("UrlAsNaClDescNotify dtor\n"));
  NPN_ReleaseObject(np_callback_);
  np_callback_ = NULL;
}

void UrlAsNaClDescNotify::RunFromFile(NPStream* stream,
                                      const nacl::string& fname) {
  // open file in DescWrapper, make available via np_callback_
  NPVariant retval;
  NPVariant status;
  ScriptableImplNpapi* nacl_desc = NULL;
  BrowserInterface* browser_interface = plugin()->browser_interface();
  NPIdentifier callback_selector = reinterpret_cast<NPIdentifier>(
      browser_interface->StringToIdentifier("onfail"));

  PLUGIN_PRINTF(("UrlAsNaClDescNotify::RunFromFile(%p, %s)\n",
                 static_cast<void*>(stream),
                 fname.c_str()));

  VOID_TO_NPVARIANT(retval);
  VOID_TO_NPVARIANT(status);

  // execute body once; construct to use break statement to exit body early
  do {
    if (NULL == stream) {
      PLUGIN_PRINTF(("fetch failed\n"));
      // If stream is null, the filename contains a message.
      nacl::stringstream msg;
      msg << "URL get failed: " << fname;
      ScalarToNPVariant(msg.str().c_str(), &status);
      break;
    }

    PLUGIN_PRINTF(("fetched FQ URL %s\n", stream->url));
    nacl::string url_origin = nacl::UrlToOrigin(stream->url);
    if (url_origin != plugin()->origin()) {
      PLUGIN_PRINTF(("same origin policy forbids access: "
                     " page from origin %s attempted to"
                     " fetch page with origin %s\n",
                     plugin()->origin().c_str(),
                     url_origin.c_str()));
      ScalarToNPVariant("Same origin violation", &status);
      break;
    }

    nacl::DescWrapperFactory* factory = plugin()->wrapper_factory();
    nacl::DescWrapper* ndiod =
        factory->OpenHostFile(const_cast<char*>(fname.c_str()),
                              NACL_ABI_O_RDONLY,
                              0);
    if (NULL == ndiod) {
      PLUGIN_PRINTF(("NaClHostDescOpen failed\n"));

      ScalarToNPVariant("NaClHostDescOpen failed", &status);
      break;
    }
    PLUGIN_PRINTF(("created ndiod %p\n", static_cast<void*>(ndiod)));

    nacl_desc = ScriptableImplNpapi::New(DescBasedHandle::New(plugin(), ndiod));
    // nacl_desc takes ownership of ndiod.
    callback_selector = reinterpret_cast<NPIdentifier>(
        browser_interface->StringToIdentifier("onload"));

    ScalarToNPVariant(static_cast<NPObject*>(nacl_desc), &status);
    // NPVariant takes ownership of NPObject nacl_desc
  } while (0);

  PLUGIN_PRINTF(("calling np_callback_ %p, nacl_desc %p, status %p\n",
                 static_cast<void*>(np_callback_),
                 static_cast<void*>(nacl_desc),
                 static_cast<void*>(&status)));
  NPP npp = InstanceIdentifierToNPP(plugin()->instance_id());
  NPN_Invoke(npp,
             np_callback_,
             callback_selector,
             &status,
             1,
             &retval);

  PLUGIN_PRINTF(("releasing status %p\n", static_cast<void*>(&status)));
  NPN_ReleaseVariantValue(&status);
  NPN_ReleaseVariantValue(&retval);
}

void UrlAsNaClDescNotify::RunFromBuffer(const nacl::string& url,
                                        StreamShmBuffer* shmbufp) {
  // create a SharedMemory object, make it available via np_callback_
  NPVariant retval;
  NPVariant status;
  ScriptableHandle* nacl_desc = NULL;
  BrowserInterface* browser_interface = plugin()->browser_interface();
  NPIdentifier callback_selector = reinterpret_cast<NPIdentifier>(
      browser_interface->StringToIdentifier("onfail"));

  PLUGIN_PRINTF(("UrlAsNaClDescNotify::RunFromBuffer(%s, %p)\n",
                 url.c_str(), static_cast<void*>(shmbufp)));

  VOID_TO_NPVARIANT(retval);
  VOID_TO_NPVARIANT(status);

  // execute body once; construct to use break statement to exit body early
  do {
    if (NULL == shmbufp) {
      PLUGIN_PRINTF(("bad buffer - stream handling failed\n"));
      ScalarToNPVariant("Same origin violation", &status);
      break;
    }

    PLUGIN_PRINTF(("fetched FQ URL %s\n", url.c_str()));
    nacl::string url_origin = nacl::UrlToOrigin(url);
    if (url_origin != plugin()->origin()) {
      PLUGIN_PRINTF(("same origin policy forbids access: "
                     " page from origin %s attempted to"
                     " fetch page with origin %s\n",
                     plugin()->origin().c_str(),
                     url_origin.c_str()));
      ScalarToNPVariant("Same origin violation", &status);
      break;
    }

    int32_t size;
    NaClDesc* raw_desc = shmbufp->shm(&size);
    if (NULL == raw_desc) {
      PLUGIN_PRINTF((" extracting shm failed\n"));
      break;
    }
    nacl::DescWrapper* wrapped_shm =
        plugin()->wrapper_factory()->MakeGeneric(NaClDescRef(raw_desc));
    ScriptableImplNpapi* shared_memory =
        ScriptableImplNpapi::New(SharedMemory::New(plugin(), wrapped_shm));

    callback_selector = reinterpret_cast<NPIdentifier>(
        browser_interface->StringToIdentifier("onload"));

    ScalarToNPVariant(static_cast<NPObject*>(shared_memory), &status);
    // NPVariant takes ownership of ScriptableHandle nacl_desc
  } while (0);

  PLUGIN_PRINTF(("calling np_callback_ %p, nacl_desc %p, status %p\n",
                  static_cast<void*>(np_callback_),
           static_cast<void*>(nacl_desc),
           static_cast<void*>(&status)));
  NPP npp = InstanceIdentifierToNPP(plugin()->instance_id());
  NPN_Invoke(npp,
             np_callback_,
             callback_selector,
             &status,
             1,
             &retval);

  PLUGIN_PRINTF(("releasing status %p\n", static_cast<void*>(&status)));
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
  PluginNpapi* plugin = reinterpret_cast<PluginNpapi*>(npp->pdata);
  set_plugin(plugin);
  PLUGIN_PRINTF(("NpGetUrlClosure ctor\n"));
}

NpGetUrlClosure::~NpGetUrlClosure() {
  PLUGIN_PRINTF(("NpGetUrlClosure dtor\n"));
  module_ = NULL;
  npp_ = NULL;
}

void NpGetUrlClosure::RunFromFile(NPStream* stream,
                                  const nacl::string& fname) {
  // open file in DescWrapper, make available via np_callback_
  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* ndiod = NULL;

  PLUGIN_PRINTF(("NpGetUrlClosure::RunFromFile(%p, %s)\n",
                  static_cast<void*>(stream), fname.c_str()));

  // execute body once; construct to use break statement to exit body early.
  do {
    if (NULL == stream) {
      // If stream is null, the filename contains a message.
      PLUGIN_PRINTF(("NpGetUrlClosure: fetch failed: %s\n", fname.c_str()));
      break;
    }

    PLUGIN_PRINTF(("fetched FQ URL %s\n", stream->url));
    nacl::string url_origin = nacl::UrlToOrigin(stream->url);
    if (url_origin != module_->origin()) {
      PLUGIN_PRINTF(("same origin policy forbids access: "
                     " page from origin %s attempted to"
                     " fetch page with origin %s\n",
                     module_->origin().c_str(),
                     url_origin.c_str()));
      break;
    }

    ndiod = factory.OpenHostFile(const_cast<char*>(fname.c_str()),
                                 NACL_ABI_O_RDONLY,
                                 0);
    if (NULL == ndiod) {
      PLUGIN_PRINTF(("NaClHostDescOpen failed\n"));
      break;
    }
    PLUGIN_PRINTF(("created ndiod %p\n", static_cast<void*>(ndiod)));
  } while (0);

  // The following two variables are passed when NPP_URLNotify is invoked.
  // Both default to the values to be passed when the requested NPN_GetURL*
  // fails.  We do not know the reason from the browser, so we use a default.
  NPReason notify_reason = NPRES_NETWORK_ERR;
  // On error, we return the requested URL.
  nacl::string notify_url = requested_url();

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
    notify_url = stream->url;
  }
  // If the user requested a notification, invoke URLNotify.
  if (call_url_notify_) {
    module_->URLNotify(npp_,
                       notify_url.c_str(),
                       notify_reason,
                       reinterpret_cast<void*>(notify_data_));
  }
}

void NpGetUrlClosure::RunFromBuffer(const nacl::string& url,
                                    StreamShmBuffer* shmbufp) {
  // create a SharedMemory object, make it available via np_callback_
  nacl::DescWrapperFactory factory;
  PLUGIN_PRINTF(("NpGetUrlClosure::RunFromBuffer(%s, %p)\n", url.c_str(),
                 static_cast<void*>(shmbufp)));

  // The following two variables are passed when NPP_URLNotify is invoked.
  // Both default to the values to be passed when the requested NPN_GetURL*
  // fails.  We do not know the reason from the browser, so we use a default.
  NPReason notify_reason = NPRES_NETWORK_ERR;
  // On error, we return the requested URL.
  nacl::string notify_url = requested_url();

  // execute body once; construct to use break statement to exit body early
  do {
    if (NULL == shmbufp) {
      PLUGIN_PRINTF(("bad buffer or URL - stream handling failed\n"));
      break;
    }

    PLUGIN_PRINTF(("fetched FQ URL %s\n", url.c_str()));
    nacl::string url_origin = nacl::UrlToOrigin(url);
    if (url_origin != module_->origin()) {
      PLUGIN_PRINTF(("same origin policy forbids access: "
                     " page from origin %s attempted to"
                     " fetch page with origin %s\n",
                     module_->origin().c_str(),
                     url_origin.c_str()));
      break;
    }

    int32_t size;
    NaClDesc* raw_desc = shmbufp->shm(&size);
    if (NULL == raw_desc) {
      PLUGIN_PRINTF((" extracting shm failed\n"));
      return;
    }
    nacl::DescWrapper* wrapped_shm =
        plugin()->wrapper_factory()->MakeGeneric(NaClDescRef(raw_desc));
    module_->StreamAsFile(npp_,
                          wrapped_shm->desc(),
                          const_cast<char*>(url.c_str()),
                          size);
    wrapped_shm->Delete();
    // We return success and the version of the URL that the browser returns.
    // The latter is typically the fully qualified URL for the request.
    notify_reason = NPRES_DONE;
    notify_url = url;
  } while (0);

  // If the user requested a notification, invoke NPP_URLNotify.
  if (call_url_notify_) {
    module_->URLNotify(npp_,
                       notify_url.c_str(),
                       notify_reason,
                       reinterpret_cast<void*>(notify_data_));
  }
}

}  // namespace plugin
