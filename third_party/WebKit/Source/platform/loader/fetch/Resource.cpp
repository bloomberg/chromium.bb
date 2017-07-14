/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
    rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "platform/loader/fetch/Resource.h"

#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <memory>

#include "build/build_config.h"
#include "platform/Histogram.h"
#include "platform/InstanceCounters.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/WebTaskRunner.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/CachedMetadata.h"
#include "platform/loader/fetch/CrossOriginAccessControl.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceClient.h"
#include "platform/loader/fetch/ResourceClientWalker.h"
#include "platform/loader/fetch/ResourceFinishObserver.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "platform/network/HTTPParsers.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

namespace {

void NotifyFinishObservers(
    HeapHashSet<WeakMember<ResourceFinishObserver>>* observers) {
  for (const auto& observer : *observers)
    observer->NotifyFinished();
}

}  // namespace

// These response headers are not copied from a revalidated response to the
// cached response headers. For compatibility, this list is based on Chromium's
// net/http/http_response_headers.cc.
const char* const kHeadersToIgnoreAfterRevalidation[] = {
    "allow",
    "connection",
    "etag",
    "expires",
    "keep-alive",
    "last-modified",
    "proxy-authenticate",
    "proxy-connection",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "www-authenticate",
    "x-frame-options",
    "x-xss-protection",
};

// Some header prefixes mean "Don't copy this header from a 304 response.".
// Rather than listing all the relevant headers, we can consolidate them into
// this list, also grabbed from Chromium's net/http/http_response_headers.cc.
const char* const kHeaderPrefixesToIgnoreAfterRevalidation[] = {
    "content-", "x-content-", "x-webkit-"};

static inline bool ShouldUpdateHeaderAfterRevalidation(
    const AtomicString& header) {
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(kHeadersToIgnoreAfterRevalidation);
       i++) {
    if (DeprecatedEqualIgnoringCase(header,
                                    kHeadersToIgnoreAfterRevalidation[i]))
      return false;
  }
  for (size_t i = 0;
       i < WTF_ARRAY_LENGTH(kHeaderPrefixesToIgnoreAfterRevalidation); i++) {
    if (header.StartsWithIgnoringASCIICase(
            kHeaderPrefixesToIgnoreAfterRevalidation[i]))
      return false;
  }
  return true;
}

class Resource::CachedMetadataHandlerImpl : public CachedMetadataHandler {
 public:
  static Resource::CachedMetadataHandlerImpl* Create(Resource* resource) {
    return new CachedMetadataHandlerImpl(resource);
  }
  ~CachedMetadataHandlerImpl() override {}
  DECLARE_VIRTUAL_TRACE();
  void SetCachedMetadata(uint32_t, const char*, size_t, CacheType) override;
  void ClearCachedMetadata(CacheType) override;
  RefPtr<CachedMetadata> GetCachedMetadata(uint32_t) const override;
  String Encoding() const override;
  // Sets the serialized metadata retrieved from the platform's cache.
  void SetSerializedCachedMetadata(const char*, size_t);

 protected:
  explicit CachedMetadataHandlerImpl(Resource*);
  virtual void SendToPlatform();
  const ResourceResponse& GetResponse() const {
    return resource_->GetResponse();
  }

  RefPtr<CachedMetadata> cached_metadata_;

 private:
  Member<Resource> resource_;
};

Resource::CachedMetadataHandlerImpl::CachedMetadataHandlerImpl(
    Resource* resource)
    : resource_(resource) {}

DEFINE_TRACE(Resource::CachedMetadataHandlerImpl) {
  visitor->Trace(resource_);
  CachedMetadataHandler::Trace(visitor);
}

void Resource::CachedMetadataHandlerImpl::SetCachedMetadata(
    uint32_t data_type_id,
    const char* data,
    size_t size,
    CachedMetadataHandler::CacheType cache_type) {
  // Currently, only one type of cached metadata per resource is supported. If
  // the need arises for multiple types of metadata per resource this could be
  // enhanced to store types of metadata in a map.
  DCHECK(!cached_metadata_);
  cached_metadata_ = CachedMetadata::Create(data_type_id, data, size);
  if (cache_type == CachedMetadataHandler::kSendToPlatform)
    SendToPlatform();
}

void Resource::CachedMetadataHandlerImpl::ClearCachedMetadata(
    CachedMetadataHandler::CacheType cache_type) {
  cached_metadata_.Clear();
  if (cache_type == CachedMetadataHandler::kSendToPlatform)
    SendToPlatform();
}

RefPtr<CachedMetadata> Resource::CachedMetadataHandlerImpl::GetCachedMetadata(
    uint32_t data_type_id) const {
  if (!cached_metadata_ || cached_metadata_->DataTypeID() != data_type_id)
    return nullptr;
  return cached_metadata_;
}

String Resource::CachedMetadataHandlerImpl::Encoding() const {
  return String(resource_->Encoding().GetName());
}

void Resource::CachedMetadataHandlerImpl::SetSerializedCachedMetadata(
    const char* data,
    size_t size) {
  // We only expect to receive cached metadata from the platform once. If this
  // triggers, it indicates an efficiency problem which is most likely
  // unexpected in code designed to improve performance.
  DCHECK(!cached_metadata_);
  cached_metadata_ = CachedMetadata::CreateFromSerializedData(data, size);
}

void Resource::CachedMetadataHandlerImpl::SendToPlatform() {
  if (cached_metadata_) {
    const Vector<char>& serialized_data = cached_metadata_->SerializedData();
    Platform::Current()->CacheMetadata(
        GetResponse().Url(), GetResponse().ResponseTime(),
        serialized_data.data(), serialized_data.size());
  } else {
    Platform::Current()->CacheMetadata(
        GetResponse().Url(), GetResponse().ResponseTime(), nullptr, 0);
  }
}

class Resource::ServiceWorkerResponseCachedMetadataHandler
    : public Resource::CachedMetadataHandlerImpl {
 public:
  static Resource::CachedMetadataHandlerImpl* Create(
      Resource* resource,
      SecurityOrigin* security_origin) {
    return new ServiceWorkerResponseCachedMetadataHandler(resource,
                                                          security_origin);
  }
  ~ServiceWorkerResponseCachedMetadataHandler() override {}
  DECLARE_VIRTUAL_TRACE();

 protected:
  void SendToPlatform() override;

 private:
  explicit ServiceWorkerResponseCachedMetadataHandler(Resource*,
                                                      SecurityOrigin*);
  String cache_storage_cache_name_;
  RefPtr<SecurityOrigin> security_origin_;
};

Resource::ServiceWorkerResponseCachedMetadataHandler::
    ServiceWorkerResponseCachedMetadataHandler(Resource* resource,
                                               SecurityOrigin* security_origin)
    : CachedMetadataHandlerImpl(resource), security_origin_(security_origin) {}

DEFINE_TRACE(Resource::ServiceWorkerResponseCachedMetadataHandler) {
  CachedMetadataHandlerImpl::Trace(visitor);
}

void Resource::ServiceWorkerResponseCachedMetadataHandler::SendToPlatform() {
  // We don't support sending the metadata to the platform when the response was
  // directly fetched via a ServiceWorker (eg:
  // FetchEvent.respondWith(fetch(FetchEvent.request))) to prevent an attacker's
  // Service Worker from poisoning the metadata cache of HTTPCache.
  if (GetResponse().CacheStorageCacheName().IsNull())
    return;

  if (cached_metadata_) {
    const Vector<char>& serialized_data = cached_metadata_->SerializedData();
    Platform::Current()->CacheMetadataInCacheStorage(
        GetResponse().Url(), GetResponse().ResponseTime(),
        serialized_data.data(), serialized_data.size(),
        WebSecurityOrigin(security_origin_),
        GetResponse().CacheStorageCacheName());
  } else {
    Platform::Current()->CacheMetadataInCacheStorage(
        GetResponse().Url(), GetResponse().ResponseTime(), nullptr, 0,
        WebSecurityOrigin(security_origin_),
        GetResponse().CacheStorageCacheName());
  }
}

Resource::Resource(const ResourceRequest& request,
                   Type type,
                   const ResourceLoaderOptions& options)
    : type_(type),
      status_(ResourceStatus::kNotStarted),
      load_finish_time_(0),
      identifier_(0),
      preload_discovery_time_(0.0),
      encoded_size_(0),
      encoded_size_memory_usage_(0),
      decoded_size_(0),
      overhead_size_(CalculateOverheadSize()),
      cache_identifier_(MemoryCache::DefaultCacheIdentifier()),
      needs_synchronous_cache_hit_(false),
      link_preload_(false),
      is_revalidating_(false),
      is_alive_(false),
      is_add_remove_client_prohibited_(false),
      integrity_disposition_(ResourceIntegrityDisposition::kNotChecked),
      options_(options),
      response_timestamp_(CurrentTime()),
      cancel_timer_(
          // We use MainThread() for main-thread cases to avoid syscall cost
          // when checking main_thread_->isCurrentThread() in currentThread().
          IsMainThread() ? Platform::Current()
                               ->MainThread()
                               ->Scheduler()
                               ->LoadingTaskRunner()
                         : Platform::Current()
                               ->CurrentThread()
                               ->Scheduler()
                               ->LoadingTaskRunner(),
          this,
          &Resource::CancelTimerFired),
      resource_request_(request) {
  InstanceCounters::IncrementCounter(InstanceCounters::kResourceCounter);

  // Currently we support the metadata caching only for HTTP family.
  if (GetResourceRequest().Url().ProtocolIsInHTTPFamily())
    cache_handler_ = CachedMetadataHandlerImpl::Create(this);
  if (IsMainThread())
    MemoryCoordinator::Instance().RegisterClient(this);
}

Resource::~Resource() {
  InstanceCounters::DecrementCounter(InstanceCounters::kResourceCounter);
}

DEFINE_TRACE(Resource) {
  visitor->Trace(loader_);
  visitor->Trace(cache_handler_);
  visitor->Trace(clients_);
  visitor->Trace(clients_awaiting_callback_);
  visitor->Trace(finished_clients_);
  visitor->Trace(finish_observers_);
  MemoryCoordinatorClient::Trace(visitor);
}

void Resource::SetLoader(ResourceLoader* loader) {
  CHECK(!loader_);
  DCHECK(StillNeedsLoad());
  loader_ = loader;
  status_ = ResourceStatus::kPending;
}

void Resource::CheckNotify() {
  if (IsLoading())
    return;

  TriggerNotificationForFinishObservers();

  ResourceClientWalker<ResourceClient> w(clients_);
  while (ResourceClient* c = w.Next()) {
    MarkClientFinished(c);
    c->NotifyFinished(this);
  }
}

void Resource::MarkClientFinished(ResourceClient* client) {
  if (clients_.Contains(client)) {
    finished_clients_.insert(client);
    clients_.erase(client);
  }
}

void Resource::AppendData(const char* data, size_t length) {
  TRACE_EVENT0("blink", "Resource::appendData");
  DCHECK(!is_revalidating_);
  DCHECK(!ErrorOccurred());
  if (options_.data_buffering_policy == kDoNotBufferData)
    return;
  if (data_)
    data_->Append(data, length);
  else
    data_ = SharedBuffer::Create(data, length);
  SetEncodedSize(data_->size());
}

void Resource::SetResourceBuffer(RefPtr<SharedBuffer> resource_buffer) {
  DCHECK(!is_revalidating_);
  DCHECK(!ErrorOccurred());
  DCHECK_EQ(options_.data_buffering_policy, kBufferData);
  data_ = std::move(resource_buffer);
  SetEncodedSize(data_->size());
}

void Resource::ClearData() {
  data_.Clear();
  encoded_size_memory_usage_ = 0;
}

void Resource::TriggerNotificationForFinishObservers() {
  if (finish_observers_.IsEmpty())
    return;

  auto new_collections = new HeapHashSet<WeakMember<ResourceFinishObserver>>(
      std::move(finish_observers_));
  finish_observers_.clear();

  Platform::Current()
      ->CurrentThread()
      ->Scheduler()
      ->LoadingTaskRunner()
      ->PostTask(BLINK_FROM_HERE, WTF::Bind(&NotifyFinishObservers,
                                            WrapPersistent(new_collections)));

  DidRemoveClientOrObserver();
}

void Resource::SetDataBufferingPolicy(
    DataBufferingPolicy data_buffering_policy) {
  options_.data_buffering_policy = data_buffering_policy;
  ClearData();
  SetEncodedSize(0);
}

void Resource::FinishAsError(const ResourceError& error) {
  DCHECK(!error.IsNull());
  error_ = error;
  is_revalidating_ = false;

  if ((error_.IsCancellation() || !is_unused_preload_) && IsMainThread())
    GetMemoryCache()->Remove(this);

  if (!ErrorOccurred())
    SetStatus(ResourceStatus::kLoadError);
  DCHECK(ErrorOccurred());
  ClearData();
  loader_ = nullptr;
  CheckNotify();
}

void Resource::Finish(double load_finish_time) {
  DCHECK(!is_revalidating_);
  load_finish_time_ = load_finish_time;
  if (!ErrorOccurred())
    status_ = ResourceStatus::kCached;
  loader_ = nullptr;
  CheckNotify();
}

AtomicString Resource::HttpContentType() const {
  return GetResponse().HttpContentType();
}

void Resource::SetIntegrityDisposition(
    ResourceIntegrityDisposition disposition) {
  DCHECK_NE(disposition, ResourceIntegrityDisposition::kNotChecked);
  DCHECK(type_ == Resource::kScript || type_ == Resource::kCSSStyleSheet);
  integrity_disposition_ = disposition;
}

bool Resource::MustRefetchDueToIntegrityMetadata(
    const FetchParameters& params) const {
  if (params.IntegrityMetadata().IsEmpty())
    return false;

  return !IntegrityMetadata::SetsEqual(integrity_metadata_,
                                       params.IntegrityMetadata());
}

static double CurrentAge(const ResourceResponse& response,
                         double response_timestamp) {
  // RFC2616 13.2.3
  // No compensation for latency as that is not terribly important in practice
  double date_value = response.Date();
  double apparent_age = std::isfinite(date_value)
                            ? std::max(0., response_timestamp - date_value)
                            : 0;
  double age_value = response.Age();
  double corrected_received_age = std::isfinite(age_value)
                                      ? std::max(apparent_age, age_value)
                                      : apparent_age;
  double resident_time = CurrentTime() - response_timestamp;
  return corrected_received_age + resident_time;
}

static double FreshnessLifetime(const ResourceResponse& response,
                                double response_timestamp) {
#if !defined(OS_ANDROID)
  // On desktop, local files should be reloaded in case they change.
  if (response.Url().IsLocalFile())
    return 0;
#endif

  // Cache other non-http / non-filesystem resources liberally.
  if (!response.Url().ProtocolIsInHTTPFamily() &&
      !response.Url().ProtocolIs("filesystem"))
    return std::numeric_limits<double>::max();

  // RFC2616 13.2.4
  double max_age_value = response.CacheControlMaxAge();
  if (std::isfinite(max_age_value))
    return max_age_value;
  double expires_value = response.Expires();
  double date_value = response.Date();
  double creation_time =
      std::isfinite(date_value) ? date_value : response_timestamp;
  if (std::isfinite(expires_value))
    return expires_value - creation_time;
  double last_modified_value = response.LastModified();
  if (std::isfinite(last_modified_value))
    return (creation_time - last_modified_value) * 0.1;
  // If no cache headers are present, the specification leaves the decision to
  // the UA. Other browsers seem to opt for 0.
  return 0;
}

static bool CanUseResponse(const ResourceResponse& response,
                           double response_timestamp) {
  if (response.IsNull())
    return false;

  // FIXME: Why isn't must-revalidate considered a reason we can't use the
  // response?
  if (response.CacheControlContainsNoCache() ||
      response.CacheControlContainsNoStore())
    return false;

  if (response.HttpStatusCode() == 303) {
    // Must not be cached.
    return false;
  }

  if (response.HttpStatusCode() == 302 || response.HttpStatusCode() == 307) {
    // Default to not cacheable unless explicitly allowed.
    bool has_max_age = std::isfinite(response.CacheControlMaxAge());
    bool has_expires = std::isfinite(response.Expires());
    // TODO: consider catching Cache-Control "private" and "public" here.
    if (!has_max_age && !has_expires)
      return false;
  }

  return CurrentAge(response, response_timestamp) <=
         FreshnessLifetime(response, response_timestamp);
}

const ResourceRequest& Resource::LastResourceRequest() const {
  if (!redirect_chain_.size())
    return GetResourceRequest();
  return redirect_chain_.back().request_;
}

void Resource::SetRevalidatingRequest(const ResourceRequest& request) {
  SECURITY_CHECK(redirect_chain_.IsEmpty());
  SECURITY_CHECK(!is_unused_preload_);
  DCHECK(!request.IsNull());
  CHECK(!is_revalidation_start_forbidden_);
  is_revalidating_ = true;
  resource_request_ = request;
  status_ = ResourceStatus::kNotStarted;
}

bool Resource::WillFollowRedirect(const ResourceRequest& new_request,
                                  const ResourceResponse& redirect_response) {
  if (is_revalidating_)
    RevalidationFailed();
  redirect_chain_.push_back(RedirectPair(new_request, redirect_response));
  return true;
}

void Resource::SetResponse(const ResourceResponse& response) {
  response_ = response;
  if (this->GetResponse().WasFetchedViaServiceWorker()) {
    cache_handler_ = ServiceWorkerResponseCachedMetadataHandler::Create(
        this, fetcher_security_origin_.Get());
  }
}

void Resource::ResponseReceived(const ResourceResponse& response,
                                std::unique_ptr<WebDataConsumerHandle>) {
  response_timestamp_ = CurrentTime();
  if (preload_discovery_time_) {
    int time_since_discovery = static_cast<int>(
        1000 * (MonotonicallyIncreasingTime() - preload_discovery_time_));
    DEFINE_STATIC_LOCAL(CustomCountHistogram,
                        preload_discovery_to_first_byte_histogram,
                        ("PreloadScanner.TTFB", 0, 10000, 50));
    preload_discovery_to_first_byte_histogram.Count(time_since_discovery);
  }

  if (is_revalidating_) {
    if (response.HttpStatusCode() == 304) {
      RevalidationSucceeded(response);
      return;
    }
    RevalidationFailed();
  }
  SetResponse(response);
  String encoding = response.TextEncodingName();
  if (!encoding.IsNull())
    SetEncoding(encoding);
}

void Resource::SetSerializedCachedMetadata(const char* data, size_t size) {
  DCHECK(!is_revalidating_);
  DCHECK(!GetResponse().IsNull());
  if (cache_handler_)
    cache_handler_->SetSerializedCachedMetadata(data, size);
}

CachedMetadataHandler* Resource::CacheHandler() {
  return cache_handler_.Get();
}

String Resource::ReasonNotDeletable() const {
  StringBuilder builder;
  if (HasClientsOrObservers()) {
    builder.Append("hasClients(");
    builder.AppendNumber(clients_.size());
    if (!clients_awaiting_callback_.IsEmpty()) {
      builder.Append(", AwaitingCallback=");
      builder.AppendNumber(clients_awaiting_callback_.size());
    }
    if (!finished_clients_.IsEmpty()) {
      builder.Append(", Finished=");
      builder.AppendNumber(finished_clients_.size());
    }
    builder.Append(')');
  }
  if (loader_) {
    if (!builder.IsEmpty())
      builder.Append(' ');
    builder.Append("loader_");
  }
  if (IsMainThread() && GetMemoryCache()->Contains(this)) {
    if (!builder.IsEmpty())
      builder.Append(' ');
    builder.Append("in_memory_cache");
  }
  return builder.ToString();
}

void Resource::DidAddClient(ResourceClient* c) {
  if (IsLoaded()) {
    c->NotifyFinished(this);
    if (clients_.Contains(c)) {
      finished_clients_.insert(c);
      clients_.erase(c);
    }
  }
}

static bool TypeNeedsSynchronousCacheHit(Resource::Type type) {
  // Some resources types default to return data synchronously. For most of
  // these, it's because there are layout tests that expect data to return
  // synchronously in case of cache hit. In the case of fonts, there was a
  // performance regression.
  // FIXME: Get to the point where we don't need to special-case sync/async
  // behavior for different resource types.
  if (type == Resource::kImage)
    return true;
  if (type == Resource::kCSSStyleSheet)
    return true;
  if (type == Resource::kScript)
    return true;
  if (type == Resource::kFont)
    return true;
  return false;
}

void Resource::WillAddClientOrObserver() {
  if (!HasClientsOrObservers()) {
    is_alive_ = true;
  }
}

void Resource::AddClient(ResourceClient* client) {
  CHECK(!is_add_remove_client_prohibited_);

  WillAddClientOrObserver();

  if (is_revalidating_) {
    clients_.insert(client);
    return;
  }

  // If an error has occurred or we have existing data to send to the new client
  // and the resource type supprts it, send it asynchronously.
  if ((ErrorOccurred() || !GetResponse().IsNull()) &&
      !TypeNeedsSynchronousCacheHit(GetType()) &&
      !needs_synchronous_cache_hit_) {
    clients_awaiting_callback_.insert(client);
    if (!async_finish_pending_clients_task_.IsActive()) {
      async_finish_pending_clients_task_ =
          Platform::Current()
              ->CurrentThread()
              ->Scheduler()
              ->LoadingTaskRunner()
              ->PostCancellableTask(BLINK_FROM_HERE,
                                    WTF::Bind(&Resource::FinishPendingClients,
                                              WrapWeakPersistent(this)));
    }
    return;
  }

  clients_.insert(client);
  DidAddClient(client);
  return;
}

void Resource::RemoveClient(ResourceClient* client) {
  CHECK(!is_add_remove_client_prohibited_);

  // This code may be called in a pre-finalizer, where weak members in the
  // HashCountedSet are already swept out.

  if (finished_clients_.Contains(client))
    finished_clients_.erase(client);
  else if (clients_awaiting_callback_.Contains(client))
    clients_awaiting_callback_.erase(client);
  else
    clients_.erase(client);

  if (clients_awaiting_callback_.IsEmpty() &&
      async_finish_pending_clients_task_.IsActive()) {
    async_finish_pending_clients_task_.Cancel();
  }

  DidRemoveClientOrObserver();
}

void Resource::AddFinishObserver(ResourceFinishObserver* client) {
  CHECK(!is_add_remove_client_prohibited_);
  DCHECK(!finish_observers_.Contains(client));

  WillAddClientOrObserver();
  finish_observers_.insert(client);
  if (IsLoaded())
    TriggerNotificationForFinishObservers();
}

void Resource::RemoveFinishObserver(ResourceFinishObserver* client) {
  CHECK(!is_add_remove_client_prohibited_);

  finish_observers_.erase(client);
  DidRemoveClientOrObserver();
}

void Resource::DidRemoveClientOrObserver() {
  if (!HasClientsOrObservers() && is_alive_) {
    is_alive_ = false;
    AllClientsAndObserversRemoved();

    // RFC2616 14.9.2:
    // "no-store: ... MUST make a best-effort attempt to remove the information
    // from volatile storage as promptly as possible"
    // "... History buffers MAY store such responses as part of their normal
    // operation."
    // We allow non-secure content to be reused in history, but we do not allow
    // secure content to be reused.
    if (HasCacheControlNoStoreHeader() && Url().ProtocolIs("https") &&
        IsMainThread())
      GetMemoryCache()->Remove(this);
  }
}

void Resource::AllClientsAndObserversRemoved() {
  if (!loader_)
    return;
  if (!cancel_timer_.IsActive())
    cancel_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void Resource::CancelTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &cancel_timer_);
  if (!HasClientsOrObservers() && loader_)
    loader_->Cancel();
}

void Resource::SetDecodedSize(size_t decoded_size) {
  if (decoded_size == decoded_size_)
    return;
  size_t old_size = size();
  decoded_size_ = decoded_size;
  if (IsMainThread())
    GetMemoryCache()->Update(this, old_size, size());
}

void Resource::SetEncodedSize(size_t encoded_size) {
  if (encoded_size == encoded_size_ &&
      encoded_size == encoded_size_memory_usage_)
    return;
  size_t old_size = size();
  encoded_size_ = encoded_size;
  encoded_size_memory_usage_ = encoded_size;
  if (IsMainThread())
    GetMemoryCache()->Update(this, old_size, size());
}

void Resource::FinishPendingClients() {
  // We're going to notify clients one by one. It is simple if the client does
  // nothing. However there are a couple other things that can happen.
  //
  // 1. Clients can be added during the loop. Make sure they are not processed.
  // 2. Clients can be removed during the loop. Make sure they are always
  //    available to be removed. Also don't call removed clients or add them
  //    back.
  //
  // Handle case (1) by saving a list of clients to notify. A separate list also
  // ensure a client is either in cliens_ or clients_awaiting_callback_.
  HeapVector<Member<ResourceClient>> clients_to_notify;
  CopyToVector(clients_awaiting_callback_, clients_to_notify);

  for (const auto& client : clients_to_notify) {
    // Handle case (2) to skip removed clients.
    if (!clients_awaiting_callback_.erase(client))
      continue;
    clients_.insert(client);

    // When revalidation starts after waiting clients are scheduled and
    // before they are added here. In such cases, we just add the clients
    // to |clients_| without DidAddClient(), as in Resource::AddClient().
    if (!is_revalidating_)
      DidAddClient(client);
  }

  // It is still possible for the above loop to finish a new client
  // synchronously. If there's no client waiting we should deschedule.
  bool scheduled = async_finish_pending_clients_task_.IsActive();
  if (scheduled && clients_awaiting_callback_.IsEmpty())
    async_finish_pending_clients_task_.Cancel();

  // Prevent the case when there are clients waiting but no callback scheduled.
  DCHECK(clients_awaiting_callback_.IsEmpty() || scheduled);
}

bool Resource::CanReuse(const FetchParameters& params) const {
  const ResourceRequest& new_request = params.GetResourceRequest();
  const ResourceLoaderOptions& new_options = params.Options();

  // Certain requests (e.g., XHRs) might have manually set headers that require
  // revalidation. In theory, this should be a Revalidate case. In practice, the
  // MemoryCache revalidation path assumes a whole bunch of things about how
  // revalidation works that manual headers violate, so punt to Reload instead.
  //
  // Similarly, a request with manually added revalidation headers can lead to a
  // 304 response for a request that wasn't flagged as a revalidation attempt.
  // Normally, successful revalidation will maintain the original response's
  // status code, but for a manual revalidation the response code remains 304.
  // In this case, the Resource likely has insufficient context to provide a
  // useful cache hit or revalidation. See http://crbug.com/643659
  if (new_request.IsConditional() || response_.HttpStatusCode() == 304)
    return false;

  // Answers the question "can a separate request with different options be
  // re-used" (e.g. preload request). The safe (but possibly slow) answer is
  // always false.
  //
  // Data buffering policy differences are believed to be safe for re-use.
  //
  // TODO: Check content_security_policy_option.
  //
  // initiator_info is purely informational and should be benign for re-use.
  //
  // request_initiator_context is benign (indicates document vs. worker).

  // Reuse only if both the existing Resource and the new request are
  // asynchronous. Particularly,
  // 1. Sync and async Resource/requests shouldn't be mixed (crbug.com/652172),
  // 2. Sync existing Resources shouldn't be revalidated, and
  // 3. Sync new requests shouldn't revalidate existing Resources.
  //
  // 2. and 3. are because SyncResourceHandler handles redirects without
  // calling WillFollowRedirect, and causes response URL mismatch
  // (crbug.com/618967) and bypassing redirect restriction around revalidation
  // (crbug.com/613971 for 2. and crbug.com/614989 for 3.).
  if (new_options.synchronous_policy == kRequestSynchronously ||
      options_.synchronous_policy == kRequestSynchronously)
    return false;

  if (resource_request_.GetKeepalive() || new_request.GetKeepalive()) {
    return false;
  }

  // securityOrigin has more complicated checks which callers are responsible
  // for.

  // TODO(yhirano): Clean up this condition. This is generated to keep the old
  // behavior across refactoring.
  //
  // TODO(tyoshino): Consider returning false when the credentials mode
  // differs.

  bool new_is_with_fetcher_cors_suppressed =
      new_options.cors_handling_by_resource_fetcher ==
      kDisableCORSHandlingByResourceFetcher;
  bool existing_was_with_fetcher_cors_suppressed =
      options_.cors_handling_by_resource_fetcher ==
      kDisableCORSHandlingByResourceFetcher;

  auto new_mode = new_request.GetFetchRequestMode();
  auto existing_mode = resource_request_.GetFetchRequestMode();

  if (new_is_with_fetcher_cors_suppressed) {
    if (existing_was_with_fetcher_cors_suppressed)
      return true;

    return existing_mode != WebURLRequest::kFetchRequestModeCORS;
  }

  if (existing_was_with_fetcher_cors_suppressed)
    return new_mode != WebURLRequest::kFetchRequestModeCORS;

  return existing_mode == new_mode &&
         new_request.GetFetchCredentialsMode() ==
             resource_request_.GetFetchCredentialsMode();
}

void Resource::Prune() {
  DestroyDecodedDataIfPossible();
}

void Resource::OnPurgeMemory() {
  Prune();
  if (!cache_handler_)
    return;
  cache_handler_->ClearCachedMetadata(CachedMetadataHandler::kCacheLocally);
}

void Resource::OnMemoryDump(WebMemoryDumpLevelOfDetail level_of_detail,
                            WebProcessMemoryDump* memory_dump) const {
  static const size_t kMaxURLReportLength = 128;
  static const int kMaxResourceClientToShowInMemoryInfra = 10;

  const String dump_name = GetMemoryDumpName();
  WebMemoryAllocatorDump* dump =
      memory_dump->CreateMemoryAllocatorDump(dump_name);
  dump->AddScalar("encoded_size", "bytes", encoded_size_memory_usage_);
  if (HasClientsOrObservers())
    dump->AddScalar("live_size", "bytes", encoded_size_memory_usage_);
  else
    dump->AddScalar("dead_size", "bytes", encoded_size_memory_usage_);

  if (data_)
    data_->OnMemoryDump(dump_name, memory_dump);

  if (level_of_detail == WebMemoryDumpLevelOfDetail::kDetailed) {
    String url_to_report = Url().GetString();
    if (url_to_report.length() > kMaxURLReportLength) {
      url_to_report.Truncate(kMaxURLReportLength);
      url_to_report = url_to_report + "...";
    }
    dump->AddString("url", "", url_to_report);

    dump->AddString("reason_not_deletable", "", ReasonNotDeletable());

    Vector<String> client_names;
    ResourceClientWalker<ResourceClient> walker(clients_);
    while (ResourceClient* client = walker.Next())
      client_names.push_back(client->DebugName());
    ResourceClientWalker<ResourceClient> walker2(clients_awaiting_callback_);
    while (ResourceClient* client = walker2.Next())
      client_names.push_back("(awaiting) " + client->DebugName());
    ResourceClientWalker<ResourceClient> walker3(finished_clients_);
    while (ResourceClient* client = walker3.Next())
      client_names.push_back("(finished) " + client->DebugName());
    std::sort(client_names.begin(), client_names.end(),
              WTF::CodePointCompareLessThan);

    StringBuilder builder;
    for (size_t i = 0;
         i < client_names.size() && i < kMaxResourceClientToShowInMemoryInfra;
         ++i) {
      if (i > 0)
        builder.Append(" / ");
      builder.Append(client_names[i]);
    }
    if (client_names.size() > kMaxResourceClientToShowInMemoryInfra) {
      builder.Append(" / and ");
      builder.AppendNumber(client_names.size() -
                           kMaxResourceClientToShowInMemoryInfra);
      builder.Append(" more");
    }
    dump->AddString("ResourceClient", "", builder.ToString());
  }

  const String overhead_name = dump_name + "/metadata";
  WebMemoryAllocatorDump* overhead_dump =
      memory_dump->CreateMemoryAllocatorDump(overhead_name);
  overhead_dump->AddScalar("size", "bytes", OverheadSize());
  memory_dump->AddSuballocation(
      overhead_dump->Guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
}

String Resource::GetMemoryDumpName() const {
  return String::Format(
      "web_cache/%s_resources/%ld",
      ResourceTypeToString(GetType(), Options().initiator_info.name),
      identifier_);
}

void Resource::SetCachePolicyBypassingCache() {
  resource_request_.SetCachePolicy(WebCachePolicy::kBypassingCache);
}

void Resource::SetPreviewsState(WebURLRequest::PreviewsState previews_state) {
  resource_request_.SetPreviewsState(previews_state);
}

void Resource::ClearRangeRequestHeader() {
  resource_request_.ClearHTTPHeaderField("range");
}

void Resource::RevalidationSucceeded(
    const ResourceResponse& validating_response) {
  SECURITY_CHECK(redirect_chain_.IsEmpty());
  SECURITY_CHECK(EqualIgnoringFragmentIdentifier(validating_response.Url(),
                                                 GetResponse().Url()));
  response_.SetResourceLoadTiming(validating_response.GetResourceLoadTiming());

  // RFC2616 10.3.5
  // Update cached headers from the 304 response
  const HTTPHeaderMap& new_headers = validating_response.HttpHeaderFields();
  for (const auto& header : new_headers) {
    // Entity headers should not be sent by servers when generating a 304
    // response; misconfigured servers send them anyway. We shouldn't allow such
    // headers to update the original request. We'll base this on the list
    // defined by RFC2616 7.1, with a few additions for extension headers we
    // care about.
    if (!ShouldUpdateHeaderAfterRevalidation(header.key))
      continue;
    response_.SetHTTPHeaderField(header.key, header.value);
  }

  is_revalidating_ = false;
}

void Resource::RevalidationFailed() {
  SECURITY_CHECK(redirect_chain_.IsEmpty());
  ClearData();
  cache_handler_.Clear();
  DestroyDecodedDataForFailedRevalidation();
  is_revalidating_ = false;
}

void Resource::MarkAsPreload() {
  DCHECK(!is_unused_preload_);
  is_unused_preload_ = true;
}

void Resource::MatchPreload() {
  DCHECK(is_unused_preload_);
  is_unused_preload_ = false;

  if (preload_discovery_time_) {
    int time_since_discovery = static_cast<int>(
        1000 * (MonotonicallyIncreasingTime() - preload_discovery_time_));
    DEFINE_STATIC_LOCAL(CustomCountHistogram, preload_discovery_histogram,
                        ("PreloadScanner.ReferenceTime", 0, 10000, 50));
    preload_discovery_histogram.Count(time_since_discovery);
  }
}

bool Resource::CanReuseRedirectChain() const {
  for (auto& redirect : redirect_chain_) {
    if (!CanUseResponse(redirect.redirect_response_, response_timestamp_))
      return false;
    if (redirect.request_.CacheControlContainsNoCache() ||
        redirect.request_.CacheControlContainsNoStore())
      return false;
  }
  return true;
}

bool Resource::HasCacheControlNoStoreHeader() const {
  return GetResponse().CacheControlContainsNoStore() ||
         GetResourceRequest().CacheControlContainsNoStore();
}

bool Resource::MustReloadDueToVaryHeader(
    const ResourceRequest& new_request) const {
  const AtomicString& vary = GetResponse().HttpHeaderField(HTTPNames::Vary);
  if (vary.IsNull())
    return false;
  if (vary == "*")
    return true;

  CommaDelimitedHeaderSet vary_headers;
  ParseCommaDelimitedHeader(vary, vary_headers);
  for (const String& header : vary_headers) {
    AtomicString atomic_header(header);
    if (GetResourceRequest().HttpHeaderField(atomic_header) !=
        new_request.HttpHeaderField(atomic_header)) {
      return true;
    }
  }
  return false;
}

bool Resource::MustRevalidateDueToCacheHeaders() const {
  return !CanUseResponse(GetResponse(), response_timestamp_) ||
         GetResourceRequest().CacheControlContainsNoCache() ||
         GetResourceRequest().CacheControlContainsNoStore();
}

bool Resource::CanUseCacheValidator() const {
  if (IsLoading() || ErrorOccurred())
    return false;

  if (HasCacheControlNoStoreHeader())
    return false;

  // Do not revalidate Resource with redirects. https://crbug.com/613971
  if (!RedirectChain().IsEmpty())
    return false;

  return GetResponse().HasCacheValidatorFields() ||
         GetResourceRequest().HasCacheValidatorFields();
}

size_t Resource::CalculateOverheadSize() const {
  static const int kAverageClientsHashMapSize = 384;
  return sizeof(Resource) + GetResponse().MemoryUsage() +
         kAverageClientsHashMapSize +
         GetResourceRequest().Url().GetString().length() * 2;
}

void Resource::DidChangePriority(ResourceLoadPriority load_priority,
                                 int intra_priority_value) {
  resource_request_.SetPriority(load_priority, intra_priority_value);
  if (loader_)
    loader_->DidChangePriority(load_priority, intra_priority_value);
}

// TODO(toyoshim): Consider to generate automatically. https://crbug.com/675515.
static const char* InitiatorTypeNameToString(
    const AtomicString& initiator_type_name) {
  if (initiator_type_name == FetchInitiatorTypeNames::css)
    return "CSS resource";
  if (initiator_type_name == FetchInitiatorTypeNames::document)
    return "Document";
  if (initiator_type_name == FetchInitiatorTypeNames::icon)
    return "Icon";
  if (initiator_type_name == FetchInitiatorTypeNames::internal)
    return "Internal resource";
  if (initiator_type_name == FetchInitiatorTypeNames::link)
    return "Link element resource";
  if (initiator_type_name == FetchInitiatorTypeNames::processinginstruction)
    return "Processing instruction";
  if (initiator_type_name == FetchInitiatorTypeNames::texttrack)
    return "Text track";
  if (initiator_type_name == FetchInitiatorTypeNames::xml)
    return "XML resource";
  if (initiator_type_name == FetchInitiatorTypeNames::xmlhttprequest)
    return "XMLHttpRequest";

  static_assert(
      FetchInitiatorTypeNames::FetchInitiatorTypeNamesCount == 12,
      "New FetchInitiatorTypeNames should be handled correctly here.");

  return "Resource";
}

const char* Resource::ResourceTypeToString(
    Type type,
    const AtomicString& fetch_initiator_name) {
  switch (type) {
    case Resource::kMainResource:
      return "Main resource";
    case Resource::kImage:
      return "Image";
    case Resource::kCSSStyleSheet:
      return "CSS stylesheet";
    case Resource::kScript:
      return "Script";
    case Resource::kFont:
      return "Font";
    case Resource::kRaw:
      return InitiatorTypeNameToString(fetch_initiator_name);
    case Resource::kSVGDocument:
      return "SVG document";
    case Resource::kXSLStyleSheet:
      return "XSL stylesheet";
    case Resource::kLinkPrefetch:
      return "Link prefetch resource";
    case Resource::kTextTrack:
      return "Text track";
    case Resource::kImportResource:
      return "Imported resource";
    case Resource::kMedia:
      return "Media";
    case Resource::kManifest:
      return "Manifest";
    case Resource::kMock:
      return "Mock";
  }
  NOTREACHED();
  return InitiatorTypeNameToString(fetch_initiator_name);
}

bool Resource::ShouldBlockLoadEvent() const {
  return !link_preload_ && IsLoadEventBlockingResourceType();
}

bool Resource::IsLoadEventBlockingResourceType() const {
  switch (type_) {
    case Resource::kMainResource:
    case Resource::kImage:
    case Resource::kCSSStyleSheet:
    case Resource::kScript:
    case Resource::kFont:
    case Resource::kSVGDocument:
    case Resource::kXSLStyleSheet:
    case Resource::kImportResource:
      return true;
    case Resource::kRaw:
    case Resource::kLinkPrefetch:
    case Resource::kTextTrack:
    case Resource::kMedia:
    case Resource::kManifest:
    case Resource::kMock:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace blink
