// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/appcache/web_application_cache_host_impl.h"

#include <stddef.h>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/id_map.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/common/appcache_interfaces.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_frame_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"

using blink::WebApplicationCacheHost;
using blink::WebApplicationCacheHostClient;
using blink::WebString;
using blink::WebURL;
using blink::WebURLResponse;
using blink::WebVector;

namespace content {

namespace {

// Note: the order of the elements in this array must match those
// of the EventID enum in appcache_interfaces.h.
const char* const kEventNames[] = {
  "Checking", "Error", "NoUpdate", "Downloading", "Progress",
  "UpdateReady", "Cached", "Obsolete"
};

using HostsMap = base::IDMap<WebApplicationCacheHostImpl*>;

HostsMap* all_hosts() {
  static HostsMap* map = new HostsMap;
  return map;
}

GURL ClearUrlRef(const GURL& url) {
  if (!url.has_ref())
    return url;
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

}  // anon namespace

WebApplicationCacheHostImpl* WebApplicationCacheHostImpl::FromId(int id) {
  return all_hosts()->Lookup(id);
}

WebApplicationCacheHostImpl::WebApplicationCacheHostImpl(
    WebApplicationCacheHostClient* client,
    int appcache_host_id,
    int render_frame_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : binding_(this),
      client_(client),
      status_(blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED),
      is_scheme_supported_(false),
      is_get_method_(false),
      is_new_master_entry_(MAYBE_NEW_ENTRY),
      was_select_cache_called_(false) {
  DCHECK(client);
  // PlzNavigate: The browser passes the ID to be used.
  if (appcache_host_id != blink::mojom::kAppCacheNoHostId) {
    all_hosts()->AddWithID(this, appcache_host_id);
    host_id_ = appcache_host_id;
  } else {
    host_id_ = all_hosts()->Add(this);
  }
  DCHECK(host_id_ != blink::mojom::kAppCacheNoHostId);

  static const base::NoDestructor<blink::mojom::AppCacheBackendPtr> backend_ptr(
      [] {
        blink::mojom::AppCacheBackendPtr result;
        RenderThread::Get()->GetConnector()->BindInterface(
            mojom::kBrowserServiceName, mojo::MakeRequest(&result));
        return result;
      }());
  backend_ = backend_ptr->get();

  blink::mojom::AppCacheFrontendPtr frontend_ptr;
  binding_.Bind(mojo::MakeRequest(&frontend_ptr, task_runner), task_runner);
  backend_->RegisterHost(
      mojo::MakeRequest(&backend_host_, std::move(task_runner)),
      std::move(frontend_ptr), host_id_, render_frame_id);
}

WebApplicationCacheHostImpl::~WebApplicationCacheHostImpl() {
  all_hosts()->Remove(host_id_);
}

void WebApplicationCacheHostImpl::CacheSelected(
    blink::mojom::AppCacheInfoPtr info) {
  cache_info_ = *info;
  client_->DidChangeCacheAssociation();
}

void WebApplicationCacheHostImpl::EventRaised(
    blink::mojom::AppCacheEventID event_id) {
  DCHECK_NE(event_id,
            blink::mojom::AppCacheEventID::
                APPCACHE_PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK_NE(event_id,
            blink::mojom::AppCacheEventID::
                APPCACHE_ERROR_EVENT);  // See OnErrorEventRaised.

  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char kFormatString[] = "Application Cache %s event";
  std::string message = base::StringPrintf(
      kFormatString, kEventNames[static_cast<int>(event_id)]);
  LogMessage(blink::mojom::ConsoleMessageLevel::kInfo, message);

  switch (event_id) {
    case blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT:
      status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_CHECKING;
      break;
    case blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT:
      status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_DOWNLOADING;
      break;
    case blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT:
      status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_UPDATE_READY;
      break;
    case blink::mojom::AppCacheEventID::APPCACHE_CACHED_EVENT:
    case blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT:
      status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_IDLE;
      break;
    case blink::mojom::AppCacheEventID::APPCACHE_OBSOLETE_EVENT:
      status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_OBSOLETE;
      break;
    default:
      NOTREACHED();
      break;
  }

  client_->NotifyEventListener(event_id);
}

void WebApplicationCacheHostImpl::ProgressEventRaised(const GURL& url,
                                                      int num_total,
                                                      int num_complete) {
  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char kFormatString[] = "Application Cache Progress event (%d of %d) %s";
  std::string message = base::StringPrintf(kFormatString, num_complete,
                                           num_total, url.spec().c_str());
  LogMessage(blink::mojom::ConsoleMessageLevel::kInfo, message);
  status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_DOWNLOADING;
  client_->NotifyProgressEventListener(url, num_total, num_complete);
}

void WebApplicationCacheHostImpl::ErrorEventRaised(
    blink::mojom::AppCacheErrorDetailsPtr details) {
  // Emit logging output prior to calling out to script as we can get
  // deleted within the script event handler.
  const char kFormatString[] = "Application Cache Error event: %s";
  std::string full_message =
      base::StringPrintf(kFormatString, details->message.c_str());
  LogMessage(blink::mojom::ConsoleMessageLevel::kError, full_message);

  status_ = cache_info_.is_complete
                ? blink::mojom::AppCacheStatus::APPCACHE_STATUS_IDLE
                : blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
  if (details->is_cross_origin) {
    // Don't leak detailed information to script for cross-origin resources.
    DCHECK_EQ(blink::mojom::AppCacheErrorReason::APPCACHE_RESOURCE_ERROR,
              details->reason);
    client_->NotifyErrorEventListener(details->reason, details->url, 0,
                                      WebString());
  } else {
    client_->NotifyErrorEventListener(details->reason, details->url,
                                      details->status,
                                      WebString::FromUTF8(details->message));
  }
}

void WebApplicationCacheHostImpl::WillStartMainResourceRequest(
    const WebURL& url,
    const WebString& method_webstring,
    const WebApplicationCacheHost* spawning_host) {
  original_main_resource_url_ = ClearUrlRef(url);

  std::string method = method_webstring.Utf8();
  is_get_method_ = (method == kHttpGETMethod);
  DCHECK(method == base::ToUpperASCII(method));

  const WebApplicationCacheHostImpl* spawning_host_impl =
      static_cast<const WebApplicationCacheHostImpl*>(spawning_host);
  if (spawning_host_impl && (spawning_host_impl != this) &&
      (spawning_host_impl->status_ !=
       blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED)) {
    backend_host_->SetSpawningHostId(spawning_host_impl->host_id());
  }
}

void WebApplicationCacheHostImpl::SelectCacheWithoutManifest() {
  if (was_select_cache_called_)
    return;
  was_select_cache_called_ = true;

  status_ =
      (document_response_.AppCacheID() == blink::mojom::kAppCacheNoCacheId)
          ? blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED
          : blink::mojom::AppCacheStatus::APPCACHE_STATUS_CHECKING;
  is_new_master_entry_ = OLD_ENTRY;
  backend_host_->SelectCache(document_url_, document_response_.AppCacheID(),
                             GURL());
}

bool WebApplicationCacheHostImpl::SelectCacheWithManifest(
    const WebURL& manifest_url) {
  if (was_select_cache_called_)
    return true;
  was_select_cache_called_ = true;

  GURL manifest_gurl(ClearUrlRef(manifest_url));

  // 6.9.6 The application cache selection algorithm
  // Check for new 'master' entries.
  if (document_response_.AppCacheID() == blink::mojom::kAppCacheNoCacheId) {
    if (is_scheme_supported_ && is_get_method_ &&
        (manifest_gurl.GetOrigin() == document_url_.GetOrigin())) {
      status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_CHECKING;
      is_new_master_entry_ = NEW_ENTRY;
    } else {
      status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
      is_new_master_entry_ = OLD_ENTRY;
      manifest_gurl = GURL();
    }
    backend_host_->SelectCache(document_url_, blink::mojom::kAppCacheNoCacheId,
                               manifest_gurl);
    return true;
  }

  DCHECK_EQ(OLD_ENTRY, is_new_master_entry_);

  // 6.9.6 The application cache selection algorithm
  // Check for 'foreign' entries.
  GURL document_manifest_gurl(document_response_.AppCacheManifestURL());
  if (document_manifest_gurl != manifest_gurl) {
    backend_host_->MarkAsForeignEntry(document_url_,
                                      document_response_.AppCacheID());
    status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
    return false;  // the navigation will be restarted
  }

  status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_CHECKING;

  // Its a 'master' entry thats already in the cache.
  backend_host_->SelectCache(document_url_, document_response_.AppCacheID(),
                             manifest_gurl);
  return true;
}

void WebApplicationCacheHostImpl::DidReceiveResponseForMainResource(
    const WebURLResponse& response) {
  document_response_ = response;
  document_url_ = ClearUrlRef(document_response_.CurrentRequestUrl());
  if (document_url_ != original_main_resource_url_)
    is_get_method_ = true;  // A redirect was involved.
  original_main_resource_url_ = GURL();

  is_scheme_supported_ =  IsSchemeSupportedForAppCache(document_url_);
  if ((document_response_.AppCacheID() != blink::mojom::kAppCacheNoCacheId) ||
      !is_scheme_supported_ || !is_get_method_)
    is_new_master_entry_ = OLD_ENTRY;
}

blink::mojom::AppCacheStatus WebApplicationCacheHostImpl::GetStatus() {
  return status_;
}

bool WebApplicationCacheHostImpl::StartUpdate() {
  bool result = false;
  backend_host_->StartUpdate(&result);
  if (!result)
    return false;
  if (status_ == blink::mojom::AppCacheStatus::APPCACHE_STATUS_IDLE ||
      status_ == blink::mojom::AppCacheStatus::APPCACHE_STATUS_UPDATE_READY) {
    status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_CHECKING;
  } else {
    status_ = blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
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

int WebApplicationCacheHostImpl::GetHostID() const {
  return host_id_;
}

void WebApplicationCacheHostImpl::GetResourceList(
    WebVector<ResourceInfo>* resources) {
  if (!cache_info_.is_complete)
    return;
  std::vector<blink::mojom::AppCacheResourceInfoPtr> boxed_infos;
  backend_host_->GetResourceList(&boxed_infos);
  std::vector<blink::mojom::AppCacheResourceInfo> resource_infos;
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
    long long app_cache_id) {
  backend_host_->SelectCacheForSharedWorker(app_cache_id);
}

}  // namespace content
