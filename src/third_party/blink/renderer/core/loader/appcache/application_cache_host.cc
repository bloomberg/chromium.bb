/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"

#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-blink.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_application_cache_host.h"
#include "third_party/blink/renderer/core/events/application_cache_error_event.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/hosts_using_features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/inspector_application_cache_agent.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

// We provide a custom implementation of this class that calls out to the
// embedding application instead of using WebCore's built in appcache system.
// This file replaces webcore/appcache/ApplicationCacheHost.cpp in our build.

ApplicationCacheHost::ApplicationCacheHost(DocumentLoader* document_loader)
    : dom_application_cache_(nullptr),
      document_loader_(document_loader),
      defers_events_(true) {
  DCHECK(document_loader_);
}

ApplicationCacheHost::~ApplicationCacheHost() {
  // Verify that detachFromDocumentLoader() has been performed already.
  DCHECK(!host_);
}

void ApplicationCacheHost::WillStartLoading(ResourceRequest& request) {
  if (!IsApplicationCacheEnabled() || !host_)
    return;
  const base::UnguessableToken& host_id = host_->GetHostID();
  if (!host_id.is_empty())
    request.SetAppCacheHostID(host_id);
}

void ApplicationCacheHost::WillStartLoadingMainResource(DocumentLoader* loader,
                                                        const KURL& url,
                                                        const String& method) {
  if (!IsApplicationCacheEnabled())
    return;
  // We defer creating the outer host object to avoid spurious
  // creation/destruction around creating empty documents. At this point, we're
  // initiating a main resource load for the document, so its for real.

  DCHECK(document_loader_->GetFrame());
  LocalFrame& frame = *document_loader_->GetFrame();
  host_ = frame.Client()->CreateApplicationCacheHost(loader, this);
  if (!host_)
    return;

  const WebApplicationCacheHost* spawning_host = nullptr;
  Frame* spawning_frame = frame.Tree().Parent();
  if (!spawning_frame || !IsA<LocalFrame>(spawning_frame))
    spawning_frame = frame.Loader().Opener();
  if (!spawning_frame || !IsA<LocalFrame>(spawning_frame))
    spawning_frame = &frame;
  if (DocumentLoader* spawning_doc_loader =
          To<LocalFrame>(spawning_frame)->Loader().GetDocumentLoader()) {
    spawning_host =
        spawning_doc_loader->GetApplicationCacheHost()
            ? spawning_doc_loader->GetApplicationCacheHost()->host_.get()
            : nullptr;
  }

  host_->WillStartMainResourceRequest(url, method, spawning_host);

  // NOTE: The semantics of this method, and others in this interface, are
  // subtly different than the method names would suggest. For example, in this
  // method never returns an appcached response in the SubstituteData out
  // argument, instead we return the appcached response thru the usual resource
  // loading pipeline.
}

void ApplicationCacheHost::SelectCacheWithoutManifest() {
  if (host_)
    host_->SelectCacheWithoutManifest();
}

void ApplicationCacheHost::SelectCacheWithManifest(const KURL& manifest_url) {
  DCHECK(document_loader_);

  LocalFrame* frame = document_loader_->GetFrame();
  Document* document = frame->GetDocument();
  if (document->IsSandboxed(WebSandboxFlags::kOrigin)) {
    // Prevent sandboxes from establishing application caches.
    SelectCacheWithoutManifest();
    return;
  }
  if (document->IsSecureContext()) {
    UseCounter::Count(document,
                      WebFeature::kApplicationCacheManifestSelectSecureOrigin);
  } else {
    Deprecation::CountDeprecation(
        document, WebFeature::kApplicationCacheManifestSelectInsecureOrigin);
    Deprecation::CountDeprecationCrossOriginIframe(
        *document, WebFeature::kApplicationCacheManifestSelectInsecureOrigin);
    HostsUsingFeatures::CountAnyWorld(
        *document, HostsUsingFeatures::Feature::
                       kApplicationCacheManifestSelectInsecureHost);
  }
  if (host_ && !host_->SelectCacheWithManifest(manifest_url)) {
    // It's a foreign entry, restart the current navigation from the top of the
    // navigation algorithm. The navigation will not result in the same resource
    // being loaded, because "foreign" entries are never picked during
    // navigation. see ApplicationCacheGroup::selectCache()
    FrameLoadRequest request(document, ResourceRequest(document->Url()));
    request.SetClientRedirectReason(ClientNavigationReason::kReload);
    frame->Navigate(request, WebFrameLoadType::kReplaceCurrentItem);
  }
}

void ApplicationCacheHost::DidReceiveResponseForMainResource(
    const ResourceResponse& response) {
  if (host_) {
    WrappedResourceResponse wrapped(response);
    host_->DidReceiveResponseForMainResource(wrapped);
  }
}

void ApplicationCacheHost::SetApplicationCache(
    ApplicationCache* dom_application_cache) {
  DCHECK(!dom_application_cache_ || !dom_application_cache);
  dom_application_cache_ = dom_application_cache;
}

void ApplicationCacheHost::DetachFromDocumentLoader() {
  // Detach from the owning DocumentLoader and let go of
  // WebApplicationCacheHost.
  SetApplicationCache(nullptr);
  host_.reset();
  document_loader_ = nullptr;
}

void ApplicationCacheHost::NotifyApplicationCache(
    mojom::AppCacheEventID id,
    int progress_total,
    int progress_done,
    mojom::AppCacheErrorReason error_reason,
    const String& error_url,
    int error_status,
    const String& error_message) {
  if (id != mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT) {
    probe::UpdateApplicationCacheStatus(document_loader_->GetFrame());
  }

  if (defers_events_) {
    // Event dispatching is deferred until document.onload has fired.
    deferred_events_.push_back(DeferredEvent(id, progress_total, progress_done,
                                             error_reason, error_url,
                                             error_status, error_message));
    return;
  }
  DispatchDOMEvent(id, progress_total, progress_done, error_reason, error_url,
                   error_status, error_message);
}

ApplicationCacheHost::CacheInfo ApplicationCacheHost::ApplicationCacheInfo() {
  if (!host_)
    return CacheInfo(NullURL(), 0, 0, 0, 0);

  WebApplicationCacheHost::CacheInfo web_info;
  host_->GetAssociatedCacheInfo(&web_info);
  return CacheInfo(web_info.manifest_url, web_info.creation_time,
                   web_info.update_time, web_info.response_sizes,
                   web_info.padding_sizes);
}

const base::UnguessableToken& ApplicationCacheHost::GetHostID() const {
  if (!host_)
    return base::UnguessableToken::Null();
  return host_->GetHostID();
}

void ApplicationCacheHost::FillResourceList(ResourceInfoList* resources) {
  if (!host_)
    return;

  WebVector<WebApplicationCacheHost::ResourceInfo> web_resources;
  host_->GetResourceList(&web_resources);
  for (size_t i = 0; i < web_resources.size(); ++i) {
    resources->push_back(ResourceInfo(
        web_resources[i].url, web_resources[i].is_master,
        web_resources[i].is_manifest, web_resources[i].is_fallback,
        web_resources[i].is_foreign, web_resources[i].is_explicit,
        web_resources[i].response_size, web_resources[i].padding_size));
  }
}

void ApplicationCacheHost::StopDeferringEvents() {
  for (unsigned i = 0; i < deferred_events_.size(); ++i) {
    const DeferredEvent& deferred = deferred_events_[i];
    DispatchDOMEvent(deferred.event_id, deferred.progress_total,
                     deferred.progress_done, deferred.error_reason,
                     deferred.error_url, deferred.error_status,
                     deferred.error_message);
  }
  deferred_events_.clear();
  defers_events_ = false;
}

void ApplicationCacheHost::DispatchDOMEvent(
    mojom::AppCacheEventID id,
    int progress_total,
    int progress_done,
    mojom::AppCacheErrorReason error_reason,
    const String& error_url,
    int error_status,
    const String& error_message) {
  // Don't dispatch an event if the window is detached.
  if (!dom_application_cache_ || !dom_application_cache_->DomWindow())
    return;

  const AtomicString& event_type = ApplicationCache::ToEventType(id);
  if (event_type.IsEmpty())
    return;
  Event* event = nullptr;
  if (id == mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT) {
    event =
        ProgressEvent::Create(event_type, true, progress_done, progress_total);
  } else if (id == mojom::AppCacheEventID::APPCACHE_ERROR_EVENT) {
    event = MakeGarbageCollected<ApplicationCacheErrorEvent>(
        error_reason, error_url, error_status, error_message);
  } else {
    event = Event::Create(event_type);
  }
  dom_application_cache_->DispatchEvent(*event);
}

mojom::AppCacheStatus ApplicationCacheHost::GetStatus() const {
  return host_ ? host_->GetStatus()
               : mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
}

bool ApplicationCacheHost::Update() {
  return host_ ? host_->StartUpdate() : false;
}

bool ApplicationCacheHost::SwapCache() {
  bool success = host_ ? host_->SwapCache() : false;
  if (success) {
    probe::UpdateApplicationCacheStatus(document_loader_->GetFrame());
  }
  return success;
}

void ApplicationCacheHost::Abort() {
  if (host_)
    host_->Abort();
}

bool ApplicationCacheHost::IsApplicationCacheEnabled() {
  DCHECK(document_loader_->GetFrame());
  return document_loader_->GetFrame()->GetSettings() &&
         document_loader_->GetFrame()
             ->GetSettings()
             ->GetOfflineWebApplicationCacheEnabled();
}

void ApplicationCacheHost::DidChangeCacheAssociation() {
  // FIXME: Prod the inspector to update its notion of what cache the page is
  // using.
}

void ApplicationCacheHost::NotifyEventListener(
    mojom::AppCacheEventID event_id) {
  NotifyApplicationCache(event_id, 0, 0,
                         mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                         String(), 0, String());
}

void ApplicationCacheHost::NotifyProgressEventListener(const WebURL&,
                                                       int progress_total,
                                                       int progress_done) {
  NotifyApplicationCache(mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT,
                         progress_total, progress_done,
                         mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR,
                         String(), 0, String());
}

void ApplicationCacheHost::NotifyErrorEventListener(
    mojom::AppCacheErrorReason reason,
    const WebURL& url,
    int status,
    const WebString& message) {
  NotifyApplicationCache(mojom::AppCacheEventID::APPCACHE_ERROR_EVENT, 0, 0,
                         reason, url.GetString(), status, message);
}

void ApplicationCacheHost::Trace(blink::Visitor* visitor) {
  visitor->Trace(dom_application_cache_);
  visitor->Trace(document_loader_);
}

}  // namespace blink
