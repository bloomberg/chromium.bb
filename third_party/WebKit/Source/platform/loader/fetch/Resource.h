/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

#ifndef Resource_h
#define Resource_h

#include <memory>
#include "platform/MemoryCoordinator.h"
#include "platform/PlatformExport.h"
#include "platform/SharedBuffer.h"
#include "platform/Timer.h"
#include "platform/WebTaskRunner.h"
#include "platform/instrumentation/tracing/web_process_memory_dump.h"
#include "platform/loader/fetch/CachedMetadataHandler.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/fetch/ResourceStatus.h"
#include "platform/loader/fetch/TextResourceDecoderOptions.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/HashCountedSet.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/TextEncoding.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/CORSStatus.h"
#include "public/platform/WebDataConsumerHandle.h"

namespace blink {

class FetchParameters;
class ResourceClient;
class ResourceFetcher;
class ResourceFinishObserver;
class ResourceTimingInfo;
class ResourceLoader;
class SecurityOrigin;

// A resource that is held in the cache. Classes who want to use this object
// should derive from ResourceClient, to get the function calls in case the
// requested data has arrived. This class also does the actual communication
// with the loader to obtain the resource from the network.
class PLATFORM_EXPORT Resource : public GarbageCollectedFinalized<Resource>,
                                 public MemoryCoordinatorClient {
  USING_GARBAGE_COLLECTED_MIXIN(Resource);
  WTF_MAKE_NONCOPYABLE(Resource);

 public:
  // |Type| enum values are used in UMAs, so do not change the values of
  // existing |Type|. When adding a new |Type|, append it at the end and update
  // |kLastResourceType|.
  enum Type : uint8_t {
    kMainResource,
    kImage,
    kCSSStyleSheet,
    kScript,
    kFont,
    kRaw,
    kSVGDocument,
    kXSLStyleSheet,
    kLinkPrefetch,
    kTextTrack,
    kImportResource,
    kMedia,  // Audio or video file requested by a HTML5 media element
    kManifest,
    kMock  // Only for testing
  };
  static const int kLastResourceType = kMock + 1;

  // Whether a resource client for a preload should mark the preload as
  // referenced.
  enum PreloadReferencePolicy {
    kMarkAsReferenced,
    kDontMarkAsReferenced,
  };

  // Used by reloadIfLoFiOrPlaceholderImage().
  enum ReloadLoFiOrPlaceholderPolicy {
    kReloadIfNeeded,
    kReloadAlways,
  };

  virtual ~Resource();

  DECLARE_VIRTUAL_TRACE();

  virtual WTF::TextEncoding Encoding() const { return WTF::TextEncoding(); }
  virtual void AppendData(const char*, size_t);
  virtual void FinishAsError(const ResourceError&);

  void SetNeedsSynchronousCacheHit(bool needs_synchronous_cache_hit) {
    needs_synchronous_cache_hit_ = needs_synchronous_cache_hit;
  }

  void SetLinkPreload(bool is_link_preload) { link_preload_ = is_link_preload; }
  bool IsLinkPreload() const { return link_preload_; }

  void SetPreloadDiscoveryTime(double preload_discovery_time) {
    preload_discovery_time_ = preload_discovery_time;
  }

  const ResourceError& GetResourceError() const { return error_; }

  void SetIdentifier(unsigned long identifier) { identifier_ = identifier; }
  unsigned long Identifier() const { return identifier_; }

  virtual bool ShouldIgnoreHTTPStatusCodeErrors() const { return false; }

  const ResourceRequest& GetResourceRequest() const {
    return resource_request_;
  }
  const ResourceRequest& LastResourceRequest() const;

  virtual void SetRevalidatingRequest(const ResourceRequest&);

  void SetFetcherSecurityOrigin(SecurityOrigin* origin) {
    fetcher_security_origin_ = origin;
  }

  // This url can have a fragment, but it can match resources that differ by the
  // fragment only.
  const KURL& Url() const { return GetResourceRequest().Url(); }
  Type GetType() const { return static_cast<Type>(type_); }
  const ResourceLoaderOptions& Options() const { return options_; }
  ResourceLoaderOptions& MutableOptions() { return options_; }

  void DidChangePriority(ResourceLoadPriority, int intra_priority_value);
  virtual ResourcePriority PriorityFromObservers() {
    return ResourcePriority();
  }

  // The reference policy indicates that the client should not affect whether
  // a preload is considered referenced or not. This allows for "passive"
  // resource clients that simply observe the resource.
  void AddClient(ResourceClient*, PreloadReferencePolicy = kMarkAsReferenced);
  void RemoveClient(ResourceClient*);

  void AddFinishObserver(ResourceFinishObserver*,
                         PreloadReferencePolicy = kMarkAsReferenced);
  void RemoveFinishObserver(ResourceFinishObserver*);

  enum PreloadResult : uint8_t {
    kPreloadNotReferenced,
    kPreloadReferenced,
  };
  PreloadResult GetPreloadResult() const { return preload_result_; }

  ResourceStatus GetStatus() const { return status_; }
  void SetStatus(ResourceStatus status) { status_ = status; }

  size_t size() const { return EncodedSize() + DecodedSize() + OverheadSize(); }

  // Returns the size of content (response body) before decoding. Adding a new
  // usage of this function is not recommended (See the TODO below).
  //
  // TODO(hiroshige): Now EncodedSize/DecodedSize states are inconsistent and
  // need to be refactored (crbug/643135).
  size_t EncodedSize() const { return encoded_size_; }

  // Returns the current memory usage for the encoded data. Adding a new usage
  // of this function is not recommended as the same reason as |EncodedSize()|.
  //
  // |EncodedSize()| and |EncodedSizeMemoryUsageForTesting()| can return
  // different values, e.g., when ImageResource purges encoded image data after
  // finishing loading.
  size_t EncodedSizeMemoryUsageForTesting() const {
    return encoded_size_memory_usage_;
  }

  size_t DecodedSize() const { return decoded_size_; }
  size_t OverheadSize() const { return overhead_size_; }

  bool IsLoaded() const { return status_ > ResourceStatus::kPending; }

  bool IsLoading() const { return status_ == ResourceStatus::kPending; }
  bool StillNeedsLoad() const { return status_ < ResourceStatus::kPending; }

  void SetLoader(ResourceLoader*);
  ResourceLoader* Loader() const { return loader_.Get(); }

  bool ShouldBlockLoadEvent() const;
  bool IsLoadEventBlockingResourceType() const;

  // Computes the status of an object after loading. Updates the expire date on
  // the cache entry file
  virtual void Finish(double finish_time);
  void Finish() { Finish(0.0); }

  virtual RefPtr<const SharedBuffer> ResourceBuffer() const { return data_; }
  void SetResourceBuffer(RefPtr<SharedBuffer>);

  virtual bool WillFollowRedirect(const ResourceRequest&,
                                  const ResourceResponse&);

  // Called when a redirect response was received but a decision has already
  // been made to not follow it.
  virtual void WillNotFollowRedirect() {}

  virtual void ResponseReceived(const ResourceResponse&,
                                std::unique_ptr<WebDataConsumerHandle>);
  void SetResponse(const ResourceResponse&);
  const ResourceResponse& GetResponse() const { return response_; }

  virtual void ReportResourceTimingToClients(const ResourceTimingInfo&) {}

  // Sets the serialized metadata retrieved from the platform's cache.
  virtual void SetSerializedCachedMetadata(const char*, size_t);

  // This may return nullptr when the resource isn't cacheable.
  CachedMetadataHandler* CacheHandler();

  AtomicString HttpContentType() const;

  bool WasCanceled() const { return error_.IsCancellation(); }
  bool ErrorOccurred() const {
    return status_ == ResourceStatus::kLoadError ||
           status_ == ResourceStatus::kDecodeError;
  }
  bool LoadFailedOrCanceled() const { return !error_.IsNull(); }

  DataBufferingPolicy GetDataBufferingPolicy() const {
    return options_.data_buffering_policy;
  }
  void SetDataBufferingPolicy(DataBufferingPolicy);

  // The IsPreloaded() flag is using a counter in order to make sure that even
  // when multiple ResourceFetchers are preloading the resource, it will remain
  // marked as preloaded until *all* of them have used it.
  bool IsUnusedPreload() const {
    return IsPreloaded() && GetPreloadResult() == kPreloadNotReferenced;
  }
  bool IsPreloaded() const { return preload_count_; }
  void IncreasePreloadCount() { ++preload_count_; }
  void DecreasePreloadCount() {
    DCHECK(preload_count_);
    --preload_count_;
  }

  bool CanReuseRedirectChain() const;
  bool MustRevalidateDueToCacheHeaders() const;
  virtual bool CanUseCacheValidator() const;
  bool IsCacheValidator() const { return is_revalidating_; }
  bool HasCacheControlNoStoreHeader() const;
  bool MustReloadDueToVaryHeader(const ResourceRequest& new_request) const;

  void SetIntegrityMetadata(const IntegrityMetadataSet& metadata) {
    integrity_metadata_ = metadata;
  }
  const IntegrityMetadataSet& IntegrityMetadata() const {
    return integrity_metadata_;
  }
  // The argument must never be |NotChecked|.
  void SetIntegrityDisposition(ResourceIntegrityDisposition);
  ResourceIntegrityDisposition IntegrityDisposition() const {
    return integrity_disposition_;
  }
  bool MustRefetchDueToIntegrityMetadata(const FetchParameters&) const;

  bool IsAlive() const { return is_alive_; }

  CORSStatus GetCORSStatus() const { return cors_status_; }

  bool IsSameOriginOrCORSSuccessful() const {
    return cors_status_ == CORSStatus::kSameOrigin ||
           cors_status_ == CORSStatus::kSuccessful ||
           cors_status_ == CORSStatus::kServiceWorkerSuccessful;
  }

  void SetCacheIdentifier(const String& cache_identifier) {
    cache_identifier_ = cache_identifier;
  }
  String CacheIdentifier() const { return cache_identifier_; }

  virtual void DidSendData(unsigned long long /* bytesSent */,
                           unsigned long long /* totalBytesToBeSent */) {}
  virtual void DidDownloadData(int) {}

  double LoadFinishTime() const { return load_finish_time_; }

  void SetEncodedDataLength(int64_t value) {
    response_.SetEncodedDataLength(value);
  }
  void SetEncodedBodyLength(int value) {
    response_.SetEncodedBodyLength(value);
  }
  void SetDecodedBodyLength(int value) {
    response_.SetDecodedBodyLength(value);
  }

  virtual bool CanReuse(const FetchParameters&) const;

  // If cache-aware loading is activated, this callback is called when the first
  // disk-cache-only request failed due to cache miss. After this callback,
  // cache-aware loading is deactivated and a reload with original request will
  // be triggered right away in ResourceLoader.
  virtual void WillReloadAfterDiskCacheMiss() {}

  // TODO(shaochuan): This is for saving back the actual ResourceRequest sent
  // in ResourceFetcher::StartLoad() for retry in cache-aware loading, remove
  // once ResourceRequest is not modified in StartLoad(). crbug.com/632580
  void SetResourceRequest(const ResourceRequest& resource_request) {
    resource_request_ = resource_request;
  }

  // Used by the MemoryCache to reduce the memory consumption of the entry.
  void Prune();

  virtual void OnMemoryDump(WebMemoryDumpLevelOfDetail,
                            WebProcessMemoryDump*) const;

  // If this Resource is ImageResource and has the Lo-Fi response headers or is
  // a placeholder, reload the full original image with the Lo-Fi state set to
  // off and optionally bypassing the cache.
  virtual void ReloadIfLoFiOrPlaceholderImage(ResourceFetcher*,
                                              ReloadLoFiOrPlaceholderPolicy) {}

  // Used to notify ImageResourceContent of the start of actual loading.
  // JavaScript calls or client/observer notifications are disallowed inside
  // NotifyStartLoad().
  virtual void NotifyStartLoad() {}

  static const char* ResourceTypeToString(
      Type,
      const AtomicString& fetch_initiator_name);

  class ProhibitAddRemoveClientInScope : public AutoReset<bool> {
   public:
    ProhibitAddRemoveClientInScope(Resource* resource)
        : AutoReset(&resource->is_add_remove_client_prohibited_, true) {}
  };

  class RevalidationStartForbiddenScope : public AutoReset<bool> {
   public:
    RevalidationStartForbiddenScope(Resource* resource)
        : AutoReset(&resource->is_revalidation_start_forbidden_, true) {}
  };

 protected:
  Resource(const ResourceRequest&, Type, const ResourceLoaderOptions&);

  virtual void CheckNotify();

  void MarkClientFinished(ResourceClient*);

  virtual bool HasClientsOrObservers() const {
    return !clients_.IsEmpty() || !clients_awaiting_callback_.IsEmpty() ||
           !finished_clients_.IsEmpty() || !finish_observers_.IsEmpty();
  }
  virtual void DestroyDecodedDataForFailedRevalidation() {}

  void SetEncodedSize(size_t);
  void SetDecodedSize(size_t);

  void FinishPendingClients();

  virtual void DidAddClient(ResourceClient*);
  void WillAddClientOrObserver(PreloadReferencePolicy);

  void DidRemoveClientOrObserver();
  virtual void AllClientsAndObserversRemoved();

  bool HasClient(ResourceClient* client) const {
    return clients_.Contains(client) ||
           clients_awaiting_callback_.Contains(client) ||
           finished_clients_.Contains(client);
  }

  struct RedirectPair {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    explicit RedirectPair(const ResourceRequest& request,
                          const ResourceResponse& redirect_response)
        : request_(request), redirect_response_(redirect_response) {}

    ResourceRequest request_;
    ResourceResponse redirect_response_;
  };
  const Vector<RedirectPair>& RedirectChain() const { return redirect_chain_; }

  virtual void DestroyDecodedDataIfPossible() {}

  // Returns the memory dump name used for tracing. See Resource::OnMemoryDump.
  String GetMemoryDumpName() const;

  const HeapHashCountedSet<WeakMember<ResourceClient>>& Clients() const {
    return clients_;
  }

  void SetCachePolicyBypassingCache();
  void SetPreviewsState(WebURLRequest::PreviewsState);
  void ClearRangeRequestHeader();

  SharedBuffer* Data() const { return data_.Get(); }
  void ClearData();

  void TriggerNotificationForFinishObservers();

  virtual void SetEncoding(const String&) {}

 private:
  // To allow access to SetCORSStatus
  friend class ResourceLoader;
  friend class SubresourceIntegrityTest;

  class CachedMetadataHandlerImpl;
  class ServiceWorkerResponseCachedMetadataHandler;

  void CancelTimerFired(TimerBase*);

  void RevalidationSucceeded(const ResourceResponse&);
  void RevalidationFailed();

  size_t CalculateOverheadSize() const;

  String ReasonNotDeletable() const;

  void SetCORSStatus(const CORSStatus cors_status) {
    cors_status_ = cors_status;
  }

  // MemoryCoordinatorClient overrides:
  void OnPurgeMemory() override;

  PreloadResult preload_result_;
  Type type_;
  ResourceStatus status_;
  CORSStatus cors_status_;

  Member<CachedMetadataHandlerImpl> cache_handler_;
  RefPtr<SecurityOrigin> fetcher_security_origin_;

  ResourceError error_;

  double load_finish_time_;

  unsigned long identifier_;

  unsigned preload_count_;
  double preload_discovery_time_;

  size_t encoded_size_;
  size_t encoded_size_memory_usage_;
  size_t decoded_size_;

  // Resource::CalculateOverheadSize() is affected by changes in
  // |m_resourceRequest.url()|, but |m_overheadSize| is not updated after
  // initial |m_resourceRequest| is given, to reduce MemoryCache manipulation
  // and thus potential bugs. crbug.com/594644
  const size_t overhead_size_;

  String cache_identifier_;

  bool needs_synchronous_cache_hit_;
  bool link_preload_;
  bool is_revalidating_;
  bool is_alive_;
  bool is_add_remove_client_prohibited_;
  bool is_revalidation_start_forbidden_ = false;

  ResourceIntegrityDisposition integrity_disposition_;
  IntegrityMetadataSet integrity_metadata_;

  // Ordered list of all redirects followed while fetching this resource.
  Vector<RedirectPair> redirect_chain_;

  HeapHashCountedSet<WeakMember<ResourceClient>> clients_;
  HeapHashCountedSet<WeakMember<ResourceClient>> clients_awaiting_callback_;
  HeapHashCountedSet<WeakMember<ResourceClient>> finished_clients_;
  HeapHashSet<WeakMember<ResourceFinishObserver>> finish_observers_;

  ResourceLoaderOptions options_;

  double response_timestamp_;

  TaskRunnerTimer<Resource> cancel_timer_;
  TaskHandle async_finish_pending_clients_task_;

  ResourceRequest resource_request_;
  Member<ResourceLoader> loader_;
  ResourceResponse response_;

  RefPtr<SharedBuffer> data_;
};

class ResourceFactory {
  STACK_ALLOCATED();

 public:
  virtual Resource* Create(const ResourceRequest&,
                           const ResourceLoaderOptions&,
                           const TextResourceDecoderOptions&) const = 0;

  Resource::Type GetType() const { return type_; }
  TextResourceDecoderOptions::ContentType ContentType() const {
    return content_type_;
  }

 protected:
  explicit ResourceFactory(Resource::Type type,
                           TextResourceDecoderOptions::ContentType content_type)
      : type_(type), content_type_(content_type) {}

  Resource::Type type_;
  TextResourceDecoderOptions::ContentType content_type_;
};

class NonTextResourceFactory : public ResourceFactory {
 protected:
  explicit NonTextResourceFactory(Resource::Type type)
      : ResourceFactory(type, TextResourceDecoderOptions::kPlainTextContent) {}

  virtual Resource* Create(const ResourceRequest&,
                           const ResourceLoaderOptions&) const = 0;

  Resource* Create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options,
                   const TextResourceDecoderOptions&) const final {
    return Create(request, options);
  }
};

#define DEFINE_RESOURCE_TYPE_CASTS(typeName)                      \
  DEFINE_TYPE_CASTS(typeName##Resource, Resource, resource,       \
                    resource->GetType() == Resource::k##typeName, \
                    resource.GetType() == Resource::k##typeName);

}  // namespace blink

#endif
