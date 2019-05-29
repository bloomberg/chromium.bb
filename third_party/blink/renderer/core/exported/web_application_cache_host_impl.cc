// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/web_application_cache_host_impl.h"

#include "third_party/blink/public/mojom/frame/document_interface_broker.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_application_cache_host_client.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"

namespace blink {

namespace {

const char kHttpGETMethod[] = "GET";

// Note: the order of the elements in this array must match those
// of the EventID enum in appcache_interfaces.h.
const char* const kEventNames[] = {"Checking",    "Error",    "NoUpdate",
                                   "Downloading", "Progress", "UpdateReady",
                                   "Cached",      "Obsolete"};

KURL ClearUrlRef(const WebURL& web_url) {
  KURL url(web_url);
  if (!url.HasFragmentIdentifier())
    return url;
  url.RemoveFragmentIdentifier();
  return url;
}

mojom::blink::DocumentInterfaceBroker* GetDocumentInterfaceBroker(
    WebLocalFrame* web_frame) {
  if (!web_frame)
    return nullptr;
  Frame* frame = WebFrame::ToCoreFrame(*web_frame);
  if (auto* local_frame = DynamicTo<LocalFrame>(frame))
    return local_frame->Client()->GetDocumentInterfaceBroker();
  return nullptr;
}

}  // namespace

WebApplicationCacheHostImpl::WebApplicationCacheHostImpl(
    WebLocalFrame* web_frame,
    WebApplicationCacheHostClient* client,
    const base::UnguessableToken& appcache_host_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : binding_(this),
      client_(client),
      status_(mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED),
      is_scheme_supported_(false),
      is_get_method_(false),
      is_new_master_entry_(MAYBE_NEW_ENTRY),
      was_select_cache_called_(false) {
  DCHECK(client);
  // PlzNavigate: The browser passes the ID to be used.
  if (!appcache_host_id.is_empty())
    host_id_ = appcache_host_id;
  else
    host_id_ = base::UnguessableToken::Create();

  mojom::blink::AppCacheFrontendPtr frontend_ptr;
  binding_.Bind(mojo::MakeRequest(&frontend_ptr, task_runner), task_runner);

  mojom::blink::DocumentInterfaceBroker* interface_broker =
      GetDocumentInterfaceBroker(web_frame);
  if (interface_broker) {
    interface_broker->RegisterAppCacheHost(
        mojo::MakeRequest(&backend_host_, std::move(task_runner)),
        std::move(frontend_ptr), host_id_);
    return;
  }

  DEFINE_STATIC_LOCAL(
      const mojom::blink::AppCacheBackendPtr, backend_ptr, ([] {
        mojom::blink::AppCacheBackendPtr result;
        Platform::Current()->GetInterfaceProvider()->GetInterface(
            mojo::MakeRequest(&result));
        return result;
      }()));

  // Once we have 'WebContextInterfaceBroker', we can call this function through
  // it like render frame.
  // Refer to the design document, 'https://bit.ly/2GT0rZv'.
  backend_ptr.get()->RegisterHost(
      mojo::MakeRequest(&backend_host_, std::move(task_runner)),
      std::move(frontend_ptr), host_id_);
}

WebApplicationCacheHostImpl::~WebApplicationCacheHostImpl() = default;

void WebApplicationCacheHostImpl::CacheSelected(
    mojom::blink::AppCacheInfoPtr info) {
  cache_info_ = *info;
  client_->DidChangeCacheAssociation();
  if (select_cache_for_shared_worker_completion_callback_)
    std::move(select_cache_for_shared_worker_completion_callback_).Run();
}

void WebApplicationCacheHostImpl::EventRaised(
    mojom::blink::AppCacheEventID event_id) {
  DCHECK_NE(event_id,
            mojom::blink::AppCacheEventID::
                APPCACHE_PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK_NE(event_id,
            mojom::blink::AppCacheEventID::
                APPCACHE_ERROR_EVENT);  // See OnErrorEventRaised.

  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char kFormatString[] = "Application Cache %s event";
  String message =
      String::Format(kFormatString, kEventNames[static_cast<int>(event_id)]);
  LogMessage(mojom::blink::ConsoleMessageLevel::kInfo, message);

  switch (event_id) {
    case mojom::blink::AppCacheEventID::APPCACHE_CHECKING_EVENT:
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;
      break;
    case mojom::blink::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT:
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_DOWNLOADING;
      break;
    case mojom::blink::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT:
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_UPDATE_READY;
      break;
    case mojom::blink::AppCacheEventID::APPCACHE_CACHED_EVENT:
    case mojom::blink::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT:
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_IDLE;
      break;
    case mojom::blink::AppCacheEventID::APPCACHE_OBSOLETE_EVENT:
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_OBSOLETE;
      break;
    default:
      NOTREACHED();
      break;
  }

  client_->NotifyEventListener(event_id);
}

void WebApplicationCacheHostImpl::ProgressEventRaised(const KURL& url,
                                                      int num_total,
                                                      int num_complete) {
  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char kFormatString[] = "Application Cache Progress event (%d of %d) %s";
  String message = String::Format(kFormatString, num_complete, num_total,
                                  url.GetString().Utf8().data());
  LogMessage(mojom::blink::ConsoleMessageLevel::kInfo, message);
  status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_DOWNLOADING;
  client_->NotifyProgressEventListener(url, num_total, num_complete);
}

void WebApplicationCacheHostImpl::ErrorEventRaised(
    mojom::blink::AppCacheErrorDetailsPtr details) {
  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char kFormatString[] = "Application Cache Error event: %s";
  String full_message =
      String::Format(kFormatString, details->message.Utf8().data());
  LogMessage(mojom::blink::ConsoleMessageLevel::kError, full_message);

  status_ = cache_info_.is_complete
                ? mojom::blink::AppCacheStatus::APPCACHE_STATUS_IDLE
                : mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
  if (details->is_cross_origin) {
    // Don't leak detailed information to script for cross-origin resources.
    DCHECK_EQ(mojom::blink::AppCacheErrorReason::APPCACHE_RESOURCE_ERROR,
              details->reason);
    client_->NotifyErrorEventListener(details->reason, details->url, 0,
                                      WebString());
  } else {
    client_->NotifyErrorEventListener(details->reason, details->url,
                                      details->status, details->message);
  }
}

void WebApplicationCacheHostImpl::WillStartMainResourceRequest(
    const WebURL& url,
    const String& method,
    const WebApplicationCacheHost* spawning_host) {
  original_main_resource_url_ = ClearUrlRef(url);

  is_get_method_ = (method == kHttpGETMethod);
  DCHECK(method == method.UpperASCII());

  const WebApplicationCacheHostImpl* spawning_host_impl =
      static_cast<const WebApplicationCacheHostImpl*>(spawning_host);
  if (spawning_host_impl && (spawning_host_impl != this) &&
      (spawning_host_impl->status_ !=
       mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED)) {
    backend_host_->SetSpawningHostId(spawning_host_impl->host_id());
  }
}

void WebApplicationCacheHostImpl::SelectCacheWithoutManifest() {
  if (was_select_cache_called_)
    return;
  was_select_cache_called_ = true;

  status_ =
      (document_response_.AppCacheID() == mojom::blink::kAppCacheNoCacheId)
          ? mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED
          : mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;
  is_new_master_entry_ = OLD_ENTRY;
  backend_host_->SelectCache(document_url_, document_response_.AppCacheID(),
                             KURL());
}

bool WebApplicationCacheHostImpl::SelectCacheWithManifest(
    const WebURL& manifest_url) {
  if (was_select_cache_called_)
    return true;
  was_select_cache_called_ = true;

  KURL manifest_kurl(ClearUrlRef(manifest_url));

  // 6.9.6 The application cache selection algorithm
  // Check for new 'master' entries.
  if (document_response_.AppCacheID() == mojom::blink::kAppCacheNoCacheId) {
    if (is_scheme_supported_ && is_get_method_ &&
        SecurityOrigin::AreSameSchemeHostPort(manifest_kurl, document_url_)) {
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;
      is_new_master_entry_ = NEW_ENTRY;
    } else {
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
      is_new_master_entry_ = OLD_ENTRY;
      manifest_kurl = KURL();
    }
    backend_host_->SelectCache(document_url_, mojom::blink::kAppCacheNoCacheId,
                               manifest_kurl);
    return true;
  }

  DCHECK_EQ(OLD_ENTRY, is_new_master_entry_);

  // 6.9.6 The application cache selection algorithm
  // Check for 'foreign' entries.
  KURL document_manifest_kurl(document_response_.AppCacheManifestURL());
  if (document_manifest_kurl != manifest_kurl) {
    backend_host_->MarkAsForeignEntry(document_url_,
                                      document_response_.AppCacheID());
    status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
    return false;  // the navigation will be restarted
  }

  status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;

  // It's a 'master' entry that's already in the cache.
  backend_host_->SelectCache(document_url_, document_response_.AppCacheID(),
                             manifest_kurl);
  return true;
}

void WebApplicationCacheHostImpl::DidReceiveResponseForMainResource(
    const WebURLResponse& response) {
  document_response_ = response;
  document_url_ = ClearUrlRef(document_response_.CurrentRequestUrl());
  if (document_url_ != original_main_resource_url_)
    is_get_method_ = true;  // A redirect was involved.
  original_main_resource_url_ = KURL();

  is_scheme_supported_ =
      Platform::Current()->IsURLSupportedForAppCache(document_url_);
  if ((document_response_.AppCacheID() != mojom::blink::kAppCacheNoCacheId) ||
      !is_scheme_supported_ || !is_get_method_)
    is_new_master_entry_ = OLD_ENTRY;
}

mojom::blink::AppCacheStatus WebApplicationCacheHostImpl::GetStatus() {
  return status_;
}

bool WebApplicationCacheHostImpl::StartUpdate() {
  bool result = false;
  backend_host_->StartUpdate(&result);
  if (!result)
    return false;
  if (status_ == mojom::blink::AppCacheStatus::APPCACHE_STATUS_IDLE ||
      status_ == mojom::blink::AppCacheStatus::APPCACHE_STATUS_UPDATE_READY) {
    status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;
  } else {
    status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
    backend_host_->GetStatus(&status_);
  }
  return true;
}

bool WebApplicationCacheHostImpl::SwapCache() {
  bool result = false;
  backend_host_->SwapCache(&result);
  if (!result)
    return false;
  backend_host_->GetStatus(&status_);
  return true;
}

void WebApplicationCacheHostImpl::GetAssociatedCacheInfo(
    WebApplicationCacheHost::CacheInfo* info) {
  info->manifest_url = cache_info_.manifest_url;
  if (!cache_info_.is_complete)
    return;
  info->creation_time = cache_info_.creation_time.ToDoubleT();
  info->update_time = cache_info_.last_update_time.ToDoubleT();
  info->response_sizes = cache_info_.response_sizes;
  info->padding_sizes = cache_info_.padding_sizes;
}

const base::UnguessableToken& WebApplicationCacheHostImpl::GetHostID() const {
  return host_id_;
}

void WebApplicationCacheHostImpl::GetResourceList(
    WebVector<ResourceInfo>* resources) {
  if (!cache_info_.is_complete)
    return;
  Vector<mojom::blink::AppCacheResourceInfoPtr> boxed_infos;
  backend_host_->GetResourceList(&boxed_infos);
  Vector<mojom::blink::AppCacheResourceInfo> resource_infos;
  for (auto& b : boxed_infos) {
    resource_infos.emplace_back(std::move(*b));
  }

  WebVector<ResourceInfo> web_resources(resource_infos.size());
  for (size_t i = 0; i < resource_infos.size(); ++i) {
    web_resources[i].response_size = resource_infos[i].response_size;
    web_resources[i].padding_size = resource_infos[i].padding_size;
    web_resources[i].is_master = resource_infos[i].is_master;
    web_resources[i].is_explicit = resource_infos[i].is_explicit;
    web_resources[i].is_manifest = resource_infos[i].is_manifest;
    web_resources[i].is_foreign = resource_infos[i].is_foreign;
    web_resources[i].is_fallback = resource_infos[i].is_fallback;
    web_resources[i].url = resource_infos[i].url;
  }
  resources->Swap(web_resources);
}

void WebApplicationCacheHostImpl::SelectCacheForSharedWorker(
    int64_t app_cache_id,
    base::OnceClosure completion_callback) {
  select_cache_for_shared_worker_completion_callback_ =
      std::move(completion_callback);
  backend_host_->SelectCacheForSharedWorker(app_cache_id);
}

}  // namespace blink
