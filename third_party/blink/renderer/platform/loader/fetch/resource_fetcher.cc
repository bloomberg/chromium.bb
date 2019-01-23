/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
    rights reserved.
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

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

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/stale_revalidation_resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/mhtml/archive_resource.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/network_instrumentation.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/probe/platform_probes.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

using blink::WebURLRequest;

namespace blink {

constexpr uint32_t ResourceFetcher::kKeepaliveInflightBytesQuota;

namespace {

constexpr base::TimeDelta kKeepaliveLoadersTimeout =
    base::TimeDelta::FromSeconds(30);

#define DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, name)                      \
  case ResourceType::k##name: {                                             \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                        \
        EnumerationHistogram, resource_histogram,                           \
        ("Blink.MemoryCache.RevalidationPolicy." prefix #name, kLoad + 1)); \
    resource_histogram.Count(policy);                                       \
    break;                                                                  \
  }

#define DEFINE_RESOURCE_HISTOGRAM(prefix)                    \
  switch (factory.GetType()) {                               \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, CSSStyleSheet)  \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Font)           \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Image)          \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, ImportResource) \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, LinkPrefetch)   \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, MainResource)   \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Manifest)       \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Audio)          \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Video)          \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Mock)           \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Raw)            \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Script)         \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, SVGDocument)    \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, TextTrack)      \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, XSLStyleSheet)  \
  }

ResourceLoadPriority TypeToPriority(ResourceType type) {
  switch (type) {
    case ResourceType::kMainResource:
    case ResourceType::kCSSStyleSheet:
    case ResourceType::kFont:
      // Also parser-blocking scripts (set explicitly in loadPriority)
      return ResourceLoadPriority::kVeryHigh;
    case ResourceType::kXSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
      FALLTHROUGH;
    case ResourceType::kRaw:
    case ResourceType::kImportResource:
    case ResourceType::kScript:
      // Also visible resources/images (set explicitly in loadPriority)
      return ResourceLoadPriority::kHigh;
    case ResourceType::kManifest:
    case ResourceType::kMock:
      // Also late-body scripts discovered by the preload scanner (set
      // explicitly in loadPriority)
      return ResourceLoadPriority::kMedium;
    case ResourceType::kImage:
    case ResourceType::kTextTrack:
    case ResourceType::kAudio:
    case ResourceType::kVideo:
    case ResourceType::kSVGDocument:
      // Also async scripts (set explicitly in loadPriority)
      return ResourceLoadPriority::kLow;
    case ResourceType::kLinkPrefetch:
      return ResourceLoadPriority::kVeryLow;
  }

  NOTREACHED();
  return ResourceLoadPriority::kUnresolved;
}

static bool IsCacheableHTTPMethod(const AtomicString& method) {
  // Per http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html#sec13.10,
  // these methods always invalidate the cache entry.
  return method != http_names::kPOST && method != http_names::kPUT &&
         method != "DELETE";
}

bool ShouldResourceBeAddedToMemoryCache(const FetchParameters& params,
                                        Resource* resource) {
  if (!IsMainThread())
    return false;
  if (params.Options().data_buffering_policy == kDoNotBufferData)
    return false;
  if (IsRawResource(*resource))
    return false;
  if (!IsCacheableHTTPMethod(params.GetResourceRequest().HttpMethod()))
    return false;
  return true;
}

static ResourceFetcher::ResourceFetcherSet& MainThreadFetchersSet() {
  DEFINE_STATIC_LOCAL(
      Persistent<ResourceFetcher::ResourceFetcherSet>, fetchers,
      (MakeGarbageCollected<ResourceFetcher::ResourceFetcherSet>()));
  return *fetchers;
}

ResourceLoadPriority AdjustPriorityWithPriorityHint(
    ResourceLoadPriority priority_so_far,
    ResourceType type,
    const ResourceRequest& resource_request,
    FetchParameters::DeferOption defer_option,
    bool is_link_preload) {
  mojom::FetchImportanceMode importance_mode =
      resource_request.GetFetchImportanceMode();

  ResourceLoadPriority new_priority = priority_so_far;

  switch (importance_mode) {
    case mojom::FetchImportanceMode::kImportanceAuto:
      break;
    case mojom::FetchImportanceMode::kImportanceHigh:
      // Boost priority of
      // - Late and async scripts
      // - Images
      // - Prefetch
      if ((type == ResourceType::kScript &&
           (FetchParameters::kLazyLoad == defer_option)) ||
          type == ResourceType::kImage || type == ResourceType::kLinkPrefetch) {
        new_priority = ResourceLoadPriority::kHigh;
      }

      DCHECK_LE(priority_so_far, new_priority);
      break;
    case mojom::FetchImportanceMode::kImportanceLow:
      // Demote priority of:
      // - Images
      //     Note: this will only have a real effect on in-viewport images since
      //     out-of-viewport images already have priority set to kLow
      // - Link preloads
      //     For this initial implementation we do a blanket demotion regardless
      //     of `as` value/type. TODO(domfarolino): maybe discuss a more
      //     granular approach with loading team
      if (type == ResourceType::kImage ||
          resource_request.GetRequestContext() ==
              mojom::RequestContextType::FETCH ||
          is_link_preload) {
        new_priority = ResourceLoadPriority::kLow;
      }

      DCHECK_LE(new_priority, priority_so_far);
      break;
  }

  return new_priority;
}

const base::Feature kStaleWhileRevalidateExperiment{
    "StaleWhileRevalidateExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

bool MatchesStaleWhileRevalidateControlList(const String& host) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<String>,
                                  stale_while_revalidate_control_hosts, ());
  if (stale_while_revalidate_control_hosts->IsNull()) {
    *stale_while_revalidate_control_hosts =
        GetFieldTrialParamValueByFeature(kStaleWhileRevalidateExperiment,
                                         "control_hosts")
            .c_str();
  }
  return !host.IsEmpty() &&
         stale_while_revalidate_control_hosts->Find(host) != kNotFound;
}

bool MatchesStaleWhileRevalidateAllowList(const String& host) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<String>,
                                  stale_while_revalidate_allow_hosts, ());
  if (stale_while_revalidate_allow_hosts->IsNull()) {
    *stale_while_revalidate_allow_hosts =
        GetFieldTrialParamValueByFeature(kStaleWhileRevalidateExperiment,
                                         "hosts")
            .c_str();
  }
  return !host.IsEmpty() &&
         stale_while_revalidate_allow_hosts->Find(host) != kNotFound;
}

}  // namespace

ResourceFetcherInit::ResourceFetcherInit(
    const ResourceFetcherProperties& properties,
    FetchContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : ResourceFetcherInit(properties,
                          context,
                          std::move(task_runner),
                          *MakeGarbageCollected<NullConsoleLogger>()) {}

ResourceLoadPriority ResourceFetcher::ComputeLoadPriority(
    ResourceType type,
    const ResourceRequest& resource_request,
    ResourcePriority::VisibilityStatus visibility,
    FetchParameters::DeferOption defer_option,
    FetchParameters::SpeculativePreloadType speculative_preload_type,
    bool is_link_preload) {
  ResourceLoadPriority priority = TypeToPriority(type);

  // Visible resources (images in practice) get a boost to High priority.
  if (visibility == ResourcePriority::kVisible)
    priority = ResourceLoadPriority::kHigh;

  // Resources before the first image are considered "early" in the document and
  // resources after the first image are "late" in the document.  Important to
  // note that this is based on when the preload scanner discovers a resource
  // for the most part so the main parser may not have reached the image element
  // yet.
  if (type == ResourceType::kImage && !is_link_preload)
    image_fetched_ = true;

  // A preloaded font should not take precedence over critical CSS or
  // parser-blocking scripts.
  if (type == ResourceType::kFont && is_link_preload)
    priority = ResourceLoadPriority::kHigh;

  if (FetchParameters::kIdleLoad == defer_option) {
    priority = ResourceLoadPriority::kVeryLow;
  } else if (type == ResourceType::kScript) {
    // Special handling for scripts.
    // Default/Parser-Blocking/Preload early in document: High (set in
    // typeToPriority)
    // Async/Defer: Low Priority (applies to both preload and parser-inserted)
    // Preload late in document: Medium
    if (FetchParameters::kLazyLoad == defer_option) {
      priority = ResourceLoadPriority::kLow;
    } else if (speculative_preload_type ==
                   FetchParameters::SpeculativePreloadType::kInDocument &&
               image_fetched_) {
      // Speculative preload is used as a signal for scripts at the bottom of
      // the document.
      priority = ResourceLoadPriority::kMedium;
    }
  } else if (FetchParameters::kLazyLoad == defer_option) {
    priority = ResourceLoadPriority::kVeryLow;
  } else if (resource_request.GetRequestContext() ==
                 mojom::RequestContextType::BEACON ||
             resource_request.GetRequestContext() ==
                 mojom::RequestContextType::PING ||
             resource_request.GetRequestContext() ==
                 mojom::RequestContextType::CSP_REPORT) {
    priority = ResourceLoadPriority::kVeryLow;
  }

  priority = AdjustPriorityWithPriorityHint(priority, type, resource_request,
                                            defer_option, is_link_preload);

  // A manually set priority acts as a floor. This is used to ensure that
  // synchronous requests are always given the highest possible priority, as
  // well as to ensure that there isn't priority churn if images move in and out
  // of the viewport, or are displayed more than once, both in and out of the
  // viewport.
  return std::max(Context().ModifyPriorityForExperiments(priority),
                  resource_request.Priority());
}

mojom::RequestContextType ResourceFetcher::DetermineRequestContext(
    ResourceType type,
    IsImageSet is_image_set,
    bool is_main_frame) {
  DCHECK((is_image_set == kImageNotImageSet) ||
         (type == ResourceType::kImage && is_image_set == kImageIsImageSet));
  switch (type) {
    case ResourceType::kMainResource:
      if (!is_main_frame)
        return mojom::RequestContextType::IFRAME;
      // FIXME: Change this to a context frame type (once we introduce them):
      // http://fetch.spec.whatwg.org/#concept-request-context-frame-type
      return mojom::RequestContextType::HYPERLINK;
    case ResourceType::kXSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
      FALLTHROUGH;
    case ResourceType::kCSSStyleSheet:
      return mojom::RequestContextType::STYLE;
    case ResourceType::kScript:
      return mojom::RequestContextType::SCRIPT;
    case ResourceType::kFont:
      return mojom::RequestContextType::FONT;
    case ResourceType::kImage:
      if (is_image_set == kImageIsImageSet)
        return mojom::RequestContextType::IMAGE_SET;
      return mojom::RequestContextType::IMAGE;
    case ResourceType::kRaw:
      return mojom::RequestContextType::SUBRESOURCE;
    case ResourceType::kImportResource:
      return mojom::RequestContextType::IMPORT;
    case ResourceType::kLinkPrefetch:
      return mojom::RequestContextType::PREFETCH;
    case ResourceType::kTextTrack:
      return mojom::RequestContextType::TRACK;
    case ResourceType::kSVGDocument:
      return mojom::RequestContextType::IMAGE;
    case ResourceType::kAudio:
      return mojom::RequestContextType::AUDIO;
    case ResourceType::kVideo:
      return mojom::RequestContextType::VIDEO;
    case ResourceType::kManifest:
      return mojom::RequestContextType::MANIFEST;
    case ResourceType::kMock:
      return mojom::RequestContextType::SUBRESOURCE;
  }
  NOTREACHED();
  return mojom::RequestContextType::SUBRESOURCE;
}

// A delegating ResourceFetcherProperties subclass which can be from the
// original ResourceFetcherProperties.
class ResourceFetcher::DetachableProperties final
    : public ResourceFetcherProperties {
 public:
  explicit DetachableProperties(const ResourceFetcherProperties& properties)
      : properties_(properties) {}
  ~DetachableProperties() override = default;

  void Detach() {
    if (!properties_) {
      // Already detached.
      return;
    }

    fetch_client_settings_object_ =
        MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
            properties_->GetFetchClientSettingsObject());
    is_main_frame_ = properties_->IsMainFrame();
    paused_ = properties_->IsPaused();
    load_complete_ = properties_->IsLoadComplete();

    properties_ = nullptr;
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(properties_);
    visitor->Trace(fetch_client_settings_object_);
    ResourceFetcherProperties::Trace(visitor);
  }

  // ResourceFetcherProperties implementation
  // Add a test in resource_fetcher_test.cc when you change behaviors.
  const FetchClientSettingsObject& GetFetchClientSettingsObject()
      const override {
    return properties_ ? properties_->GetFetchClientSettingsObject()
                       : *fetch_client_settings_object_;
  }
  bool IsMainFrame() const override {
    return properties_ ? properties_->IsMainFrame() : is_main_frame_;
  }
  ControllerServiceWorkerMode GetControllerServiceWorkerMode() const override {
    return properties_ ? properties_->GetControllerServiceWorkerMode()
                       : ControllerServiceWorkerMode::kNoController;
  }
  int64_t ServiceWorkerId() const override {
    // When detached, GetControllerServiceWorkerMode returns kNoController, so
    // this function must not be called.
    DCHECK(properties_);
    return properties_->ServiceWorkerId();
  }
  bool IsPaused() const override {
    return properties_ ? properties_->IsPaused() : paused_;
  }
  bool IsDetached() const override { return !properties_; }
  bool IsLoadComplete() const override {
    return properties_ ? properties_->IsLoadComplete() : load_complete_;
  }
  bool ShouldBlockLoadingMainResource() const override {
    // Returns true when detached in order to preserve the existing behavior.
    return properties_ ? properties_->ShouldBlockLoadingMainResource() : true;
  }
  bool ShouldBlockLoadingSubResource() const override {
    // Returns true when detached in order to preserve the existing behavior.
    return properties_ ? properties_->ShouldBlockLoadingSubResource() : true;
  }

 private:
  // |properties_| is null if and only if detached.
  Member<const ResourceFetcherProperties> properties_;

  // The following members are used when detached.
  Member<const FetchClientSettingsObject> fetch_client_settings_object_;
  bool is_main_frame_ = false;
  bool paused_ = false;
  bool load_complete_ = false;
};

ResourceFetcher::ResourceFetcher(const ResourceFetcherInit& init)
    : properties_(
          *MakeGarbageCollected<DetachableProperties>(*init.properties)),
      context_(init.context),
      task_runner_(init.task_runner),
      console_logger_(init.console_logger),
      archive_(init.archive),
      resource_timing_report_timer_(
          task_runner_,
          this,
          &ResourceFetcher::ResourceTimingReportTimerFired),
      auto_load_images_(true),
      images_enabled_(true),
      allow_stale_resources_(false),
      image_fetched_(false),
      stale_while_revalidate_enabled_(false) {
  DCHECK(console_logger_);
  InstanceCounters::IncrementCounter(InstanceCounters::kResourceFetcherCounter);
  if (IsMainThread())
    MainThreadFetchersSet().insert(this);
  context_->Bind(this);
  scheduler_ = MakeGarbageCollected<ResourceLoadScheduler>(
      init.initial_throttling_policy, context_);
}

ResourceFetcher::~ResourceFetcher() {
  InstanceCounters::DecrementCounter(InstanceCounters::kResourceFetcherCounter);
}

const ResourceFetcherProperties& ResourceFetcher::GetProperties() const {
  return *properties_;
}

Resource* ResourceFetcher::CachedResource(const KURL& resource_url) const {
  if (resource_url.IsEmpty())
    return nullptr;
  KURL url = MemoryCache::RemoveFragmentIdentifierIfNeeded(resource_url);
  const WeakMember<Resource>& resource = cached_resources_map_.at(url);
  return resource.Get();
}

mojom::ControllerServiceWorkerMode
ResourceFetcher::IsControlledByServiceWorker() const {
  return properties_->GetControllerServiceWorkerMode();
}

bool ResourceFetcher::ResourceNeedsLoad(Resource* resource,
                                        const FetchParameters& params,
                                        RevalidationPolicy policy) {
  // Defer a font load until it is actually needed unless this is a link
  // preload.
  if (resource->GetType() == ResourceType::kFont && !params.IsLinkPreload())
    return false;

  // Defer loading images either when:
  // - images are disabled
  // - instructed to defer loading images from network
  if (resource->GetType() == ResourceType::kImage &&
      (ShouldDeferImageLoad(resource->Url()) ||
       params.GetImageRequestOptimization() ==
           FetchParameters::kDeferImageLoad)) {
    return false;
  }
  return policy != kUse || resource->StillNeedsLoad();
}

void ResourceFetcher::RequestLoadStarted(unsigned long identifier,
                                         Resource* resource,
                                         const FetchParameters& params,
                                         RevalidationPolicy policy,
                                         bool is_static_data) {
  KURL url = MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url());
  if (policy == kUse && resource->GetStatus() == ResourceStatus::kCached &&
      !cached_resources_map_.Contains(url)) {
    // Loaded from MemoryCache.
    DidLoadResourceFromMemoryCache(identifier, resource,
                                   params.GetResourceRequest());
  }

  if (is_static_data)
    return;

  if (policy == kUse && !resource->StillNeedsLoad() &&
      !cached_resources_map_.Contains(url)) {
    // Resources loaded from memory cache should be reported the first time
    // they're used.
    scoped_refptr<ResourceTimingInfo> info = ResourceTimingInfo::Create(
        params.Options().initiator_info.name, CurrentTimeTicks(),
        resource->GetType() == ResourceType::kMainResource);
    // TODO(yoav): GetInitialUrlForResourceTiming() is only needed until
    // Out-of-Blink CORS lands: https://crbug.com/736308
    info->SetInitialURL(
        resource->GetResourceRequest().GetInitialUrlForResourceTiming().IsNull()
            ? resource->GetResourceRequest().Url()
            : resource->GetResourceRequest().GetInitialUrlForResourceTiming());
    ResourceResponse final_response = resource->GetResponse();
    final_response.SetResourceLoadTiming(nullptr);
    info->SetFinalResponse(final_response);
    info->SetLoadFinishTime(info->InitialTime());
    scheduled_resource_timing_reports_.push_back(std::move(info));
    if (!resource_timing_report_timer_.IsActive())
      resource_timing_report_timer_.StartOneShot(TimeDelta(), FROM_HERE);
  }
}

void ResourceFetcher::DidLoadResourceFromMemoryCache(
    unsigned long identifier,
    Resource* resource,
    const ResourceRequest& original_request) {
  ResourceRequest request = original_request;

  Context().DispatchWillSendRequest(
      identifier, request, ResourceResponse() /* redirects */,
      resource->GetType(), resource->Options().initiator_info);
  Context().DispatchDidReceiveResponse(
      identifier, request, resource->GetResponse(), resource,
      FetchContext::ResourceResponseType::kFromMemoryCache);

  if (resource->EncodedSize() > 0) {
    Context().DispatchDidReceiveData(identifier, nullptr,
                                     resource->EncodedSize());
  }

  Context().DispatchDidFinishLoading(
      identifier, TimeTicks(), 0, resource->GetResponse().DecodedBodyLength(),
      false);
}

static std::unique_ptr<TracedValue> UrlForTraceEvent(const KURL& url) {
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  value->SetString("url", url.GetString());
  return value;
}

Resource* ResourceFetcher::ResourceForStaticData(
    const FetchParameters& params,
    const ResourceFactory& factory,
    const SubstituteData& substitute_data) {
  const KURL& url = params.GetResourceRequest().Url();
  DCHECK(url.ProtocolIsData() || substitute_data.IsValid() || archive_);

  if (!archive_ && !substitute_data.IsValid() &&
      factory.GetType() == ResourceType::kRaw) {
    return nullptr;
  }

  const String cache_identifier = GetCacheIdentifier();
  // Most off-main-thread resource fetches use Resource::kRaw and don't reach
  // this point, but off-main-thread module fetches might.
  if (IsMainThread()) {
    if (Resource* old_resource =
            GetMemoryCache()->ResourceForURL(url, cache_identifier)) {
      // There's no reason to re-parse if we saved the data from the previous
      // parse.
      if (params.Options().data_buffering_policy != kDoNotBufferData)
        return old_resource;
      GetMemoryCache()->Remove(old_resource);
    }
  }

  ResourceResponse response;
  scoped_refptr<SharedBuffer> data;
  if (substitute_data.IsValid()) {
    data = substitute_data.Content();
    response.SetCurrentRequestUrl(url);
    response.SetMimeType(substitute_data.MimeType());
    response.SetExpectedContentLength(data->size());
    response.SetTextEncodingName(substitute_data.TextEncoding());
  } else if (url.ProtocolIsData()) {
    data = network_utils::ParseDataURLAndPopulateResponse(url, response);
    if (!data)
      return nullptr;
    // |response| is modified by parseDataURLAndPopulateResponse() and is
    // ready to be used.
  } else {
    ArchiveResource* archive_resource =
        archive_->SubresourceForURL(params.Url());
    // The archive doesn't contain the resource, the request must be aborted.
    if (!archive_resource)
      return nullptr;
    data = archive_resource->Data();
    response.SetCurrentRequestUrl(url);
    response.SetMimeType(archive_resource->MimeType());
    response.SetExpectedContentLength(data->size());
    response.SetTextEncodingName(archive_resource->TextEncoding());
  }

  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  resource->NotifyStartLoad();
  // FIXME: We should provide a body stream here.
  resource->ResponseReceived(response, nullptr);
  resource->SetDataBufferingPolicy(kBufferData);
  if (data->size())
    resource->SetResourceBuffer(data);
  resource->SetIdentifier(CreateUniqueIdentifier());
  resource->SetCacheIdentifier(cache_identifier);
  resource->Finish(TimeTicks(), task_runner_.get());

  if (!substitute_data.IsValid())
    AddToMemoryCacheIfNeeded(params, resource);

  return resource;
}

Resource* ResourceFetcher::ResourceForBlockedRequest(
    const FetchParameters& params,
    const ResourceFactory& factory,
    ResourceRequestBlockedReason blocked_reason,
    ResourceClient* client) {
  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  if (client)
    client->SetResource(resource, task_runner_.get());
  resource->FinishAsError(ResourceError::CancelledDueToAccessCheckError(
                              params.Url(), blocked_reason),
                          task_runner_.get());
  return resource;
}

void ResourceFetcher::MakePreloadedResourceBlockOnloadIfNeeded(
    Resource* resource,
    const FetchParameters& params) {
  // TODO(yoav): Test that non-blocking resources (video/audio/track) continue
  // to not-block even after being preloaded and discovered.
  if (resource && resource->Loader() &&
      resource->IsLoadEventBlockingResourceType() &&
      resource->IsLinkPreload() && !params.IsLinkPreload() &&
      non_blocking_loaders_.Contains(resource->Loader())) {
    non_blocking_loaders_.erase(resource->Loader());
    loaders_.insert(resource->Loader());
  }
}

void ResourceFetcher::UpdateMemoryCacheStats(Resource* resource,
                                             RevalidationPolicy policy,
                                             const FetchParameters& params,
                                             const ResourceFactory& factory,
                                             bool is_static_data) const {
  if (is_static_data)
    return;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      BooleanHistogram, resource_histogram,
      ("Blink.ResourceFetcher.StaleWhileRevalidate"));
  resource_histogram.Count(params.IsStaleRevalidation());

  if (params.IsSpeculativePreload() || params.IsLinkPreload()) {
    DEFINE_RESOURCE_HISTOGRAM("Preload.");
  } else {
    DEFINE_RESOURCE_HISTOGRAM("");
  }

  // Aims to count Resource only referenced from MemoryCache (i.e. what would be
  // dead if MemoryCache holds weak references to Resource). Currently we check
  // references to Resource from ResourceClient and |m_preloads| only, because
  // they are major sources of references.
  if (resource && !resource->IsAlive() && !ContainsAsPreload(resource)) {
    DEFINE_RESOURCE_HISTOGRAM("Dead.");
  }
}

bool ResourceFetcher::ContainsAsPreload(Resource* resource) const {
  auto it = preloads_.find(PreloadKey(resource->Url(), resource->GetType()));
  return it != preloads_.end() && it->value == resource;
}

void ResourceFetcher::RemovePreload(Resource* resource) {
  auto it = preloads_.find(PreloadKey(resource->Url(), resource->GetType()));
  if (it == preloads_.end())
    return;
  if (it->value == resource)
    preloads_.erase(it);
}

base::Optional<ResourceRequestBlockedReason> ResourceFetcher::PrepareRequest(
    FetchParameters& params,
    const ResourceFactory& factory,
    const SubstituteData& substitute_data,
    unsigned long identifier,
    WebScopedVirtualTimePauser& virtual_time_pauser) {
  ResourceRequest& resource_request = params.MutableResourceRequest();
  ResourceType resource_type = factory.GetType();
  const ResourceLoaderOptions& options = params.Options();

  DCHECK(options.synchronous_policy == kRequestAsynchronously ||
         resource_type == ResourceType::kRaw ||
         resource_type == ResourceType::kXSLStyleSheet);

  params.OverrideContentType(factory.ContentType());

  // Don't send security violation reports for speculative preloads.
  SecurityViolationReportingPolicy reporting_policy =
      params.IsSpeculativePreload()
          ? SecurityViolationReportingPolicy::kSuppressReporting
          : SecurityViolationReportingPolicy::kReport;

  // Note that resource_request.GetRedirectStatus() may return kFollowedRedirect
  // here since e.g. ThreadableLoader may create a new Resource from
  // a ResourceRequest that originates from the ResourceRequest passed to
  // the redirect handling callback.

  // Before modifying the request for CSP, evaluate report-only headers. This
  // allows site owners to learn about requests that are being modified
  // (e.g. mixed content that is being upgraded by upgrade-insecure-requests).
  Context().CheckCSPForRequest(
      resource_request.GetRequestContext(),
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url()), options,
      reporting_policy, resource_request.GetRedirectStatus());

  // This may modify params.Url() (via the resource_request argument).
  Context().PopulateResourceRequest(
      resource_type, params.GetClientHintsPreferences(),
      params.GetResourceWidth(), resource_request);

  if (!params.Url().IsValid())
    return ResourceRequestBlockedReason::kOther;

  resource_request.SetPriority(ComputeLoadPriority(
      resource_type, params.GetResourceRequest(), ResourcePriority::kNotVisible,
      params.Defer(), params.GetSpeculativePreloadType(),
      params.IsLinkPreload()));
  if (resource_request.GetCacheMode() == mojom::FetchCacheMode::kDefault) {
    resource_request.SetCacheMode(Context().ResourceRequestCachePolicy(
        resource_request, resource_type, params.Defer()));
  }
  if (resource_request.GetRequestContext() ==
      mojom::RequestContextType::UNSPECIFIED) {
    resource_request.SetRequestContext(DetermineRequestContext(
        resource_type, kImageNotImageSet, properties_->IsMainFrame()));
  }
  if (resource_type == ResourceType::kLinkPrefetch)
    resource_request.SetHTTPHeaderField(http_names::kPurpose, "prefetch");

  bool resource_allows_stale_while_revalidate = false;
  bool host_matches_control =
      MatchesStaleWhileRevalidateControlList(params.Url().Host());
  bool host_matches_allow = false;
  if (!host_matches_control) {
    host_matches_allow =
        MatchesStaleWhileRevalidateAllowList(params.Url().Host());
  }
  if (host_matches_allow)
    resource_allows_stale_while_revalidate = host_matches_allow;

  // For the finch experiment indicate that the resource likely supports
  // stale while revalidate (based on a list of hosts). This allows us
  // to only log metrics for sites that would possibly benefit from
  // stale while revalidate being enabled.
  resource_request.SetStaleRevalidateCandidate(
      (host_matches_allow || host_matches_control) &&
      !params.IsStaleRevalidation());
  // Indicate whether the network stack can return a stale resource. If a
  // stale resource is returned a StaleRevalidation request will be scheduled.
  // Explicitly disallow stale responses for fetchers that don't have SWR
  // enabled (via origin trial), non-GET requests and resource requests that
  // are raw. We are explicitly excluding RawResources here to avoid
  // unintentional SWR, as bugs around RawResources tend to be complicated and
  // critical.
  resource_request.SetAllowStaleResponse(
      (resource_allows_stale_while_revalidate ||
       stale_while_revalidate_enabled_) &&
      resource_request.HttpMethod() == http_names::kGET &&
      !IsRawResource(resource_type) && !params.IsStaleRevalidation());

  Context().AddAdditionalRequestHeaders(
      resource_request, (resource_type == ResourceType::kMainResource)
                            ? kFetchMainResource
                            : kFetchSubresource);

  network_instrumentation::ResourcePrioritySet(identifier,
                                               resource_request.Priority());

  KURL url = MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url());
  base::Optional<ResourceRequestBlockedReason> blocked_reason =
      Context().CanRequest(resource_type, resource_request, url, options,
                           reporting_policy,
                           resource_request.GetRedirectStatus());

  if (Context().IsAdResource(url, resource_type,
                             resource_request.GetRequestContext())) {
    resource_request.SetIsAdResource();
  }

  if (blocked_reason)
    return blocked_reason;

  // For initial requests, call PrepareRequest() here before revalidation
  // policy is determined.
  Context().PrepareRequest(resource_request, virtual_time_pauser,
                           FetchContext::RedirectType::kNotForRedirect);

  if (!params.Url().IsValid())
    return ResourceRequestBlockedReason::kOther;

  if (!RuntimeEnabledFeatures::OutOfBlinkCorsEnabled() &&
      options.cors_handling_by_resource_fetcher ==
          kEnableCorsHandlingByResourceFetcher) {
    const scoped_refptr<const SecurityOrigin> origin =
        resource_request.RequestorOrigin();
    DCHECK(!options.cors_flag);
    params.MutableOptions().cors_flag = cors::CalculateCorsFlag(
        params.Url(), origin.get(), resource_request.GetFetchRequestMode());
    // TODO(yhirano): Reject requests for non CORS-enabled schemes.
    // See https://crrev.com/c/1298828.
    resource_request.SetAllowStoredCredentials(cors::CalculateCredentialsFlag(
        resource_request.GetFetchCredentialsMode(),
        cors::CalculateResponseTainting(
            params.Url(), resource_request.GetFetchRequestMode(), origin.get(),
            params.Options().cors_flag ? CorsFlag::Set : CorsFlag::Unset)));
  }

  if (RuntimeEnabledFeatures::OutOfBlinkCorsEnabled() &&
      resource_request.GetFetchCredentialsMode() ==
          network::mojom::FetchCredentialsMode::kOmit) {
    // See comments at network::ResourceRequest::fetch_credentials_mode.
    resource_request.SetAllowStoredCredentials(false);
  }

  return base::nullopt;
}

Resource* ResourceFetcher::RequestResource(
    FetchParameters& params,
    const ResourceFactory& factory,
    ResourceClient* client,
    const SubstituteData& substitute_data,
    unsigned long identifier) {
  if (!identifier)
    identifier = CreateUniqueIdentifier();
  ResourceRequest& resource_request = params.MutableResourceRequest();
  network_instrumentation::ScopedResourceLoadTracker
      scoped_resource_load_tracker(identifier, resource_request);
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_THREAD_SAFE(
      "Blink.Fetch.RequestResourceTime");
  TRACE_EVENT1("blink", "ResourceFetcher::requestResource", "url",
               UrlForTraceEvent(params.Url()));

  // TODO(crbug.com/123004): Remove once we have enough stats on data URIs that
  // contain fragments ('#' characters).
  //
  // TODO(crbug.com/796173): This call happens before commit for iframes that
  // have data URI sources, which causes UKM to miss the metric recording.
  if (context_) {
    const KURL& url = params.Url();
    if (url.HasFragmentIdentifier() && url.ProtocolIsData()) {
      context_->CountUsage(mojom::WebFeature::kDataUriHasOctothorpe);
    }
  }

  // |resource_request|'s origin can be null here, corresponding to the "client"
  // value in the spec. In that case client's origin is used.
  if (!resource_request.RequestorOrigin()) {
    resource_request.SetRequestorOrigin(
        properties_->GetFetchClientSettingsObject().GetSecurityOrigin());
  }

  WebScopedVirtualTimePauser pauser;
  base::Optional<ResourceRequestBlockedReason> blocked_reason =
      PrepareRequest(params, factory, substitute_data, identifier, pauser);
  if (blocked_reason) {
    return ResourceForBlockedRequest(params, factory, blocked_reason.value(),
                                     client);
  }

  ResourceType resource_type = factory.GetType();

  if (!params.IsSpeculativePreload()) {
    // Only log if it's not for speculative preload.
    Context().RecordLoadingActivity(resource_request, resource_type,
                                    params.Options().initiator_info.name);
  }

  Resource* resource = nullptr;
  RevalidationPolicy policy = kLoad;

  bool is_data_url = resource_request.Url().ProtocolIsData();
  bool is_static_data = is_data_url || substitute_data.IsValid() || archive_;
  bool is_stale_revalidation = params.IsStaleRevalidation();
  if (!is_stale_revalidation && is_static_data) {
    resource = ResourceForStaticData(params, factory, substitute_data);
    if (resource) {
      policy =
          DetermineRevalidationPolicy(resource_type, params, *resource, true);
    } else if (!is_data_url && archive_) {
      // Abort the request if the archive doesn't contain the resource, except
      // in the case of data URLs which might have resources such as fonts that
      // need to be decoded only on demand. These data URLs are allowed to be
      // processed using the normal ResourceFetcher machinery.
      return ResourceForBlockedRequest(
          params, factory, ResourceRequestBlockedReason::kOther, client);
    }
  }

  if (!is_stale_revalidation && !resource) {
    resource = MatchPreload(params, resource_type);
    if (resource) {
      policy = kUse;
      // If |params| is for a blocking resource and a preloaded resource is
      // found, we may need to make it block the onload event.
      MakePreloadedResourceBlockOnloadIfNeeded(resource, params);
    } else if (IsMainThread()) {
      resource =
          GetMemoryCache()->ResourceForURL(params.Url(), GetCacheIdentifier());
      if (resource) {
        policy = DetermineRevalidationPolicy(resource_type, params, *resource,
                                             is_static_data);
      }
    }
  }

  UpdateMemoryCacheStats(resource, policy, params, factory, is_static_data);

  switch (policy) {
    case kReload:
      GetMemoryCache()->Remove(resource);
      FALLTHROUGH;
    case kLoad:
      resource = CreateResourceForLoading(params, factory);
      break;
    case kRevalidate:
      InitializeRevalidation(resource_request, resource);
      break;
    case kUse:
      bool used_stale = false;
      if (resource_request.AllowsStaleResponse() &&
          resource->ShouldRevalidateStaleResponse()) {
        used_stale = true;
        ScheduleStaleRevalidate(resource);
      }
      if (resource_request.IsStaleRevalidateCandidate()) {
        context_->DidObserveLoadingBehavior(
            used_stale
                ? WebLoadingBehaviorFlag::
                      kStaleWhileRevalidateResourceCandidateStaleCacheLoad
                : WebLoadingBehaviorFlag::
                      kStaleWhileRevalidateResourceCandidateCacheLoad);
      }

      if (resource->IsLinkPreload() && !params.IsLinkPreload())
        resource->SetLinkPreload(false);
      break;
  }
  DCHECK(resource);
  // TODO(yoav): turn to a DCHECK. See https://crbug.com/690632
  CHECK_EQ(resource->GetType(), resource_type);

  if (policy != kUse) {
    resource->SetIdentifier(identifier);
    resource->VirtualTimePauser() = std::move(pauser);
  }

  if (client)
    client->SetResource(resource, task_runner_.get());

  // TODO(yoav): It is not clear why preloads are exempt from this check. Can we
  // remove the exemption?
  if (!params.IsSpeculativePreload() || policy != kUse) {
    // When issuing another request for a resource that is already in-flight
    // make sure to not demote the priority of the in-flight request. If the new
    // request isn't at the same priority as the in-flight request, only allow
    // promotions. This can happen when a visible image's priority is increased
    // and then another reference to the image is parsed (which would be at a
    // lower priority).
    if (resource_request.Priority() > resource->GetResourceRequest().Priority())
      resource->DidChangePriority(resource_request.Priority(), 0);
    // TODO(yoav): I'd expect the stated scenario to not go here, as its policy
    // would be Use.
  }

  // If only the fragment identifiers differ, it is the same resource.
  DCHECK(EqualIgnoringFragmentIdentifier(resource->Url(), params.Url()));
  RequestLoadStarted(identifier, resource, params, policy, is_static_data);
  if (!is_stale_revalidation) {
    cached_resources_map_.Set(
        MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url()), resource);
  }
  document_resources_.insert(resource);

  // Returns with an existing resource if the resource does not need to start
  // loading immediately. If revalidation policy was determined as |Revalidate|,
  // the resource was already initialized for the revalidation here, but won't
  // start loading.
  if (ResourceNeedsLoad(resource, params, policy)) {
    if (!StartLoad(resource)) {
      resource->FinishAsError(ResourceError::CancelledError(params.Url()),
                              task_runner_.get());
    }
  }

  if (policy != kUse)
    InsertAsPreloadIfNecessary(resource, params, resource_type);

  if (resource->Identifier() == identifier &&
      (resource->StillNeedsLoad() || resource->IsLoading())) {
    scoped_resource_load_tracker.ResourceLoadContinuesBeyondScope();
  }

  return resource;
}

void ResourceFetcher::ResourceTimingReportTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &resource_timing_report_timer_);
  Vector<scoped_refptr<ResourceTimingInfo>> timing_reports;
  timing_reports.swap(scheduled_resource_timing_reports_);
  for (const auto& timing_info : timing_reports)
    Context().AddResourceTiming(*timing_info);
}

void ResourceFetcher::InitializeRevalidation(
    ResourceRequest& revalidating_request,
    Resource* resource) {
  DCHECK(resource);
  DCHECK(GetMemoryCache()->Contains(resource));
  DCHECK(resource->IsLoaded());
  DCHECK(resource->CanUseCacheValidator());
  DCHECK(!resource->IsCacheValidator());
  DCHECK_EQ(properties_->GetControllerServiceWorkerMode(),
            mojom::ControllerServiceWorkerMode::kNoController);
  // RawResource doesn't support revalidation.
  CHECK(!IsRawResource(*resource));

  revalidating_request.SetIsRevalidating(true);

  const AtomicString& last_modified =
      resource->GetResponse().HttpHeaderField(http_names::kLastModified);
  const AtomicString& e_tag =
      resource->GetResponse().HttpHeaderField(http_names::kETag);
  if (!last_modified.IsEmpty() || !e_tag.IsEmpty()) {
    DCHECK_NE(mojom::FetchCacheMode::kBypassCache,
              revalidating_request.GetCacheMode());
    if (revalidating_request.GetCacheMode() ==
        mojom::FetchCacheMode::kValidateCache) {
      revalidating_request.SetHTTPHeaderField(http_names::kCacheControl,
                                              "max-age=0");
    }
  }
  if (!last_modified.IsEmpty()) {
    revalidating_request.SetHTTPHeaderField(http_names::kIfModifiedSince,
                                            last_modified);
  }
  if (!e_tag.IsEmpty())
    revalidating_request.SetHTTPHeaderField(http_names::kIfNoneMatch, e_tag);

  resource->SetRevalidatingRequest(revalidating_request);
}

void ResourceFetcher::AddToMemoryCacheIfNeeded(const FetchParameters& params,
                                               Resource* resource) {
  if (!ShouldResourceBeAddedToMemoryCache(params, resource))
    return;

  GetMemoryCache()->Add(resource);
}

Resource* ResourceFetcher::CreateResourceForLoading(
    const FetchParameters& params,
    const ResourceFactory& factory) {
  const String cache_identifier = GetCacheIdentifier();
  DCHECK(!IsMainThread() || params.IsStaleRevalidation() ||
         !GetMemoryCache()->ResourceForURL(params.GetResourceRequest().Url(),
                                           cache_identifier));

  RESOURCE_LOADING_DVLOG(1) << "Loading Resource for "
                            << params.GetResourceRequest().Url().ElidedString();

  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  resource->SetLinkPreload(params.IsLinkPreload());
  resource->SetCacheIdentifier(cache_identifier);

  AddToMemoryCacheIfNeeded(params, resource);
  return resource;
}

void ResourceFetcher::StorePerformanceTimingInitiatorInformation(
    Resource* resource) {
  const AtomicString& fetch_initiator = resource->Options().initiator_info.name;
  if (fetch_initiator == fetch_initiator_type_names::kInternal)
    return;

  if (resource->GetType() == ResourceType::kMainResource)
    return;

  scoped_refptr<ResourceTimingInfo> info = ResourceTimingInfo::Create(
      fetch_initiator, CurrentTimeTicks(), false /* is_main_resource */);

  if (resource->IsCacheValidator()) {
    const AtomicString& timing_allow_origin =
        resource->GetResponse().HttpHeaderField(http_names::kTimingAllowOrigin);
    if (!timing_allow_origin.IsEmpty())
      info->SetOriginalTimingAllowOrigin(timing_allow_origin);
  }

  resource_timing_info_map_.insert(resource, std::move(info));
}

void ResourceFetcher::RecordResourceTimingOnRedirect(
    Resource* resource,
    const ResourceResponse& redirect_response,
    const KURL& new_url) {
  ResourceTimingInfoMap::iterator it = resource_timing_info_map_.find(resource);
  if (it != resource_timing_info_map_.end())
    it->value->AddRedirect(redirect_response, new_url);
}

static bool IsDownloadOrStreamRequest(const ResourceRequest& request) {
  // Never use cache entries for DownloadToBlob / UseStreamOnResponse requests.
  // The data will be delivered through other paths.
  return request.DownloadToBlob() || request.UseStreamOnResponse();
}

Resource* ResourceFetcher::MatchPreload(const FetchParameters& params,
                                        ResourceType type) {
  auto it = preloads_.find(PreloadKey(params.Url(), type));
  if (it == preloads_.end())
    return nullptr;

  Resource* resource = it->value;

  if (resource->MustRefetchDueToIntegrityMetadata(params)) {
    if (!params.IsSpeculativePreload() && !params.IsLinkPreload())
      PrintPreloadWarning(resource, Resource::MatchStatus::kIntegrityMismatch);
    return nullptr;
  }

  if (params.IsSpeculativePreload())
    return resource;
  if (params.IsLinkPreload()) {
    resource->SetLinkPreload(true);
    return resource;
  }

  const ResourceRequest& request = params.GetResourceRequest();
  if (request.DownloadToBlob()) {
    PrintPreloadWarning(resource, Resource::MatchStatus::kBlobRequest);
    return nullptr;
  }

  if (IsImageResourceDisallowedToBeReused(*resource)) {
    PrintPreloadWarning(resource, Resource::MatchStatus::kImageLoadingDisabled);
    return nullptr;
  }

  const Resource::MatchStatus match_status = resource->CanReuse(params);
  if (match_status != Resource::MatchStatus::kOk) {
    PrintPreloadWarning(resource, match_status);
    return nullptr;
  }

  if (!resource->MatchPreload(params, task_runner_.get())) {
    PrintPreloadWarning(resource, Resource::MatchStatus::kUnknownFailure);
    return nullptr;
  }

  preloads_.erase(it);
  matched_preloads_.push_back(resource);
  return resource;
}

void ResourceFetcher::PrintPreloadWarning(Resource* resource,
                                          Resource::MatchStatus status) {
  if (!resource->IsLinkPreload())
    return;

  StringBuilder builder;
  builder.Append("A preload for '");
  builder.Append(resource->Url());
  builder.Append("' is found, but is not used ");

  switch (status) {
    case Resource::MatchStatus::kOk:
      NOTREACHED();
      break;
    case Resource::MatchStatus::kUnknownFailure:
      builder.Append("due to an unknown reason.");
      break;
    case Resource::MatchStatus::kIntegrityMismatch:
      builder.Append("due to an integrity mismatch.");
      break;
    case Resource::MatchStatus::kBlobRequest:
      builder.Append("because the new request loads the content as a blob.");
      break;
    case Resource::MatchStatus::kImageLoadingDisabled:
      builder.Append("because image loading is disabled.");
      break;
    case Resource::MatchStatus::kSynchronousFlagDoesNotMatch:
      builder.Append("because the new request is synchronous.");
      break;
    case Resource::MatchStatus::kRequestModeDoesNotMatch:
      builder.Append("because the request mode does not match. ");
      builder.Append("Consider taking a look at crossorigin attribute.");
      break;
    case Resource::MatchStatus::kRequestCredentialsModeDoesNotMatch:
      builder.Append("because the request credentials mode does not match. ");
      builder.Append("Consider taking a look at crossorigin attribute.");
      break;
    case Resource::MatchStatus::kKeepaliveSet:
      builder.Append("because the keepalive flag is set.");
      break;
    case Resource::MatchStatus::kRequestMethodDoesNotMatch:
      builder.Append("because the request HTTP method does not match.");
      break;
    case Resource::MatchStatus::kRequestHeadersDoNotMatch:
      builder.Append("because the request headers do not match.");
      break;
    case Resource::MatchStatus::kImagePlaceholder:
      builder.Append("due to different image placeholder policies.");
      break;
  }
  console_logger_->AddWarningMessage(ConsoleLogger::Source::kOther,
                                     builder.ToString());
}

void ResourceFetcher::InsertAsPreloadIfNecessary(Resource* resource,
                                                 const FetchParameters& params,
                                                 ResourceType type) {
  if (!params.IsSpeculativePreload() && !params.IsLinkPreload())
    return;
  DCHECK(!params.IsStaleRevalidation());
  // CSP web tests verify that preloads are subject to access checks by
  // seeing if they are in the `preload started` list. Therefore do not add
  // them to the list if the load is immediately denied.
  if (resource->LoadFailedOrCanceled() &&
      resource->GetResourceError().IsAccessCheck()) {
    return;
  }
  PreloadKey key(params.Url(), type);
  if (preloads_.find(key) != preloads_.end())
    return;

  preloads_.insert(key, resource);
  resource->MarkAsPreload();
  if (preloaded_urls_for_test_)
    preloaded_urls_for_test_->insert(resource->Url().GetString());
}

bool ResourceFetcher::IsImageResourceDisallowedToBeReused(
    const Resource& existing_resource) const {
  // When images are disabled, don't ever load images, even if the image is
  // cached or it is a data: url. In this case:
  // - remove the image from the memory cache, and
  // - create a new resource but defer loading (this is done by
  //   ResourceNeedsLoad()).
  //
  // This condition must be placed before the condition on |is_static_data| to
  // prevent loading a data: URL.
  //
  // TODO(japhet): Can we get rid of one of these settings?

  if (existing_resource.GetType() != ResourceType::kImage)
    return false;

  return !Context().AllowImage(images_enabled_, existing_resource.Url());
}

ResourceFetcher::RevalidationPolicy
ResourceFetcher::DetermineRevalidationPolicy(
    ResourceType type,
    const FetchParameters& fetch_params,
    const Resource& existing_resource,
    bool is_static_data) const {
  RevalidationPolicy policy = DetermineRevalidationPolicyInternal(
      type, fetch_params, existing_resource, is_static_data);

  TRACE_EVENT_INSTANT1("blink", "ResourceFetcher::DetermineRevalidationPolicy",
                       TRACE_EVENT_SCOPE_THREAD, "revalidationPolicy", policy);

  return policy;
}

ResourceFetcher::RevalidationPolicy
ResourceFetcher::DetermineRevalidationPolicyInternal(
    ResourceType type,
    const FetchParameters& fetch_params,
    const Resource& existing_resource,
    bool is_static_data) const {
  const ResourceRequest& request = fetch_params.GetResourceRequest();

  if (IsDownloadOrStreamRequest(request))
    return kReload;

  if (IsImageResourceDisallowedToBeReused(existing_resource))
    return kReload;

  // If the existing resource is loading and the associated fetcher is not equal
  // to |this|, we must not use the resource. Otherwise, CSP violation may
  // happen in redirect handling.
  if (existing_resource.Loader() &&
      existing_resource.Loader()->Fetcher() != this) {
    return kReload;
  }

  // It's hard to share a not-yet-referenced preloads via MemoryCache correctly.
  // A not-yet-matched preloads made by a foreign ResourceFetcher and stored in
  // the memory cache could be used without this block.
  if ((fetch_params.IsLinkPreload() || fetch_params.IsSpeculativePreload()) &&
      existing_resource.IsUnusedPreload()) {
    return kReload;
  }

  // Checks if the resource has an explicit policy about integrity metadata.
  //
  // This is necessary because ScriptResource and CSSStyleSheetResource objects
  // do not keep the raw data around after the source is accessed once, so if
  // the resource is accessed from the MemoryCache for a second time, there is
  // no way to redo an integrity check.
  //
  // Thus, Blink implements a scheme where it caches the integrity information
  // for those resources after the first time it is checked, and if there is
  // another request for that resource, with the same integrity metadata, Blink
  // skips the integrity calculation. However, if the integrity metadata is a
  // mismatch, the MemoryCache must be skipped here, and a new request for the
  // resource must be made to get the raw data. This is expected to be an
  // uncommon case, however, as it implies two same-origin requests to the same
  // resource, but with different integrity metadata.
  if (existing_resource.MustRefetchDueToIntegrityMetadata(fetch_params)) {
    return kReload;
  }

  // If the same URL has been loaded as a different type, we need to reload.
  if (existing_resource.GetType() != type) {
    // FIXME: If existingResource is a Preload and the new type is LinkPrefetch
    // We really should discard the new prefetch since the preload has more
    // specific type information! crbug.com/379893
    // fast/dom/HTMLLinkElement/link-and-subresource-test hits this case.
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::DetermineRevalidationPolicy "
                                 "reloading due to type mismatch.";
    return kReload;
  }

  // If resource was populated from a SubstituteData load or data: url, use it.
  // This doesn't necessarily mean that |resource| was just created by using
  // ResourceForStaticData().
  if (is_static_data)
    return kUse;

  if (existing_resource.CanReuse(fetch_params) != Resource::MatchStatus::kOk) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::DetermineRevalidationPolicy "
                                 "reloading due to Resource::CanReuse() "
                                 "returning false.";
    return kReload;
  }

  // Don't reload resources while pasting.
  if (allow_stale_resources_)
    return kUse;

  // FORCE_CACHE uses the cache no matter what.
  if (request.GetCacheMode() == mojom::FetchCacheMode::kForceCache)
    return kUse;

  // Don't reuse resources with Cache-control: no-store.
  if (existing_resource.HasCacheControlNoStoreHeader()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::DetermineRevalidationPolicy "
                                 "reloading due to Cache-control: no-store.";
    return kReload;
  }

  // During the initial load, avoid loading the same resource multiple times for
  // a single document, even if the cache policies would tell us to. We also
  // group loads of the same resource together. Raw resources are exempted, as
  // XHRs fall into this category and may have user-set Cache-Control: headers
  // or other factors that require separate requests.
  if (type != ResourceType::kRaw) {
    if (!properties_->IsLoadComplete() &&
        cached_resources_map_.Contains(
            MemoryCache::RemoveFragmentIdentifierIfNeeded(
                existing_resource.Url())))
      return kUse;
    if (existing_resource.IsLoading())
      return kUse;
  }

  // RELOAD always reloads
  if (request.GetCacheMode() == mojom::FetchCacheMode::kBypassCache) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::DetermineRevalidationPolicy "
                                 "reloading due to "
                                 "FetchCacheMode::kBypassCache";
    return kReload;
  }

  // We'll try to reload the resource if it failed last time.
  if (existing_resource.ErrorOccurred()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::DetermineRevalidationPolicy "
                                 "reloading due to resource being in the error "
                                 "state";
    return kReload;
  }

  // List of available images logic allows images to be re-used without cache
  // validation. We restrict this only to images from memory cache which are the
  // same as the version in the current document.
  if (type == ResourceType::kImage &&
      &existing_resource == CachedResource(request.Url())) {
    return kUse;
  }

  if (existing_resource.MustReloadDueToVaryHeader(request))
    return kReload;

  // If any of the redirects in the chain to loading the resource were not
  // cacheable, we cannot reuse our cached resource.
  if (!existing_resource.CanReuseRedirectChain()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::DetermineRevalidationPolicy "
                                 "reloading due to an uncacheable redirect";
    return kReload;
  }

  // Check if the cache headers requires us to revalidate (cache expiration for
  // example).
  if (request.GetCacheMode() == mojom::FetchCacheMode::kValidateCache ||
      existing_resource.MustRevalidateDueToCacheHeaders(
          request.AllowsStaleResponse()) ||
      request.CacheControlContainsNoCache()) {
    // Revalidation is harmful for non-matched preloads because it may lead to
    // sharing one preloaded resource among multiple ResourceFetchers.
    if (existing_resource.IsUnusedPreload())
      return kReload;

    // See if the resource has usable ETag or Last-modified headers. If the page
    // is controlled by the ServiceWorker, we choose the Reload policy because
    // the revalidation headers should not be exposed to the
    // ServiceWorker.(crbug.com/429570)
    //
    // TODO(falken): If the controller has no fetch event handler, we probably
    // can treat it as not being controlled in the S13nSW case. In the
    // non-S13nSW, we don't know what controller the request will ultimately go
    // to (due to skipWaiting) so be conservative.
    if (existing_resource.CanUseCacheValidator() &&
        properties_->GetControllerServiceWorkerMode() ==
            mojom::ControllerServiceWorkerMode::kNoController) {
      // If the resource is already a cache validator but not started yet, the
      // |Use| policy should be applied to subsequent requests.
      if (existing_resource.IsCacheValidator()) {
        DCHECK(existing_resource.StillNeedsLoad());
        return kUse;
      }
      return kRevalidate;
    }

    // No, must reload.
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::DetermineRevalidationPolicy "
                                 "reloading due to missing cache validators.";
    return kReload;
  }

  return kUse;
}

void ResourceFetcher::SetAutoLoadImages(bool enable) {
  if (enable == auto_load_images_)
    return;

  auto_load_images_ = enable;

  if (!auto_load_images_)
    return;

  ReloadImagesIfNotDeferred();
}

void ResourceFetcher::SetImagesEnabled(bool enable) {
  if (enable == images_enabled_)
    return;

  images_enabled_ = enable;

  if (!images_enabled_)
    return;

  ReloadImagesIfNotDeferred();
}

bool ResourceFetcher::ShouldDeferImageLoad(const KURL& url) const {
  return !Context().AllowImage(images_enabled_, url) || !auto_load_images_;
}

void ResourceFetcher::ReloadImagesIfNotDeferred() {
  for (Resource* resource : document_resources_) {
    if (resource->GetType() == ResourceType::kImage &&
        resource->StillNeedsLoad() && !ShouldDeferImageLoad(resource->Url()))
      StartLoad(resource);
  }
}

FetchContext& ResourceFetcher::Context() const {
  return *context_;
}

void ResourceFetcher::ClearContext() {
  scheduler_->Shutdown();
  ClearPreloads(ResourceFetcher::kClearAllPreloads);

  {
    // This block used to be
    //  context_ = Context().Detach();
    // While we are splitting FetchContext to multiple classes we need to call
    // "detach" for multiple objects in a coordinated manner. See
    // https://crbug.com/914739 for the progress.
    // TODO(yhirano): Remove the cross-class dependency.
    auto* context = context_.Get();
    context_ = Context().Detach();
    context->Unbind();
    properties_->Detach();
    context_->Bind(this);
  }

  console_logger_ = MakeGarbageCollected<NullConsoleLogger>();

  // Make sure the only requests still going are keepalive requests.
  // Callers of ClearContext() should be calling StopFetching() prior
  // to this, but it's possible for additional requests to start during
  // StopFetching() (e.g., fallback fonts that only trigger when the
  // first choice font failed to load).
  StopFetching();

  if (!loaders_.IsEmpty() || !non_blocking_loaders_.IsEmpty()) {
    // There are some keepalive requests.
    // The use of WrapPersistent creates a reference cycle intentionally,
    // to keep the ResourceFetcher and ResourceLoaders alive until the requests
    // complete or the timer fires.
    keepalive_loaders_task_handle_ = PostDelayedCancellableTask(
        *task_runner_, FROM_HERE,
        WTF::Bind(&ResourceFetcher::StopFetchingIncludingKeepaliveLoaders,
                  WrapPersistent(this)),
        kKeepaliveLoadersTimeout);
  }
}

int ResourceFetcher::BlockingRequestCount() const {
  return loaders_.size();
}

int ResourceFetcher::NonblockingRequestCount() const {
  return non_blocking_loaders_.size();
}

int ResourceFetcher::ActiveRequestCount() const {
  return loaders_.size() + non_blocking_loaders_.size();
}

void ResourceFetcher::EnableIsPreloadedForTest() {
  if (preloaded_urls_for_test_)
    return;
  preloaded_urls_for_test_ = std::make_unique<HashSet<String>>();

  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    preloaded_urls_for_test_->insert(resource->Url().GetString());
  }
}

bool ResourceFetcher::IsPreloadedForTest(const KURL& url) const {
  DCHECK(preloaded_urls_for_test_);
  return preloaded_urls_for_test_->Contains(url.GetString());
}

void ResourceFetcher::ClearPreloads(ClearPreloadsPolicy policy) {
  Vector<PreloadKey> keys_to_be_removed;
  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    if (policy == kClearAllPreloads || !resource->IsLinkPreload()) {
      GetMemoryCache()->Remove(resource);
      keys_to_be_removed.push_back(pair.key);
    }
  }
  preloads_.RemoveAll(keys_to_be_removed);

  matched_preloads_.clear();
}

Vector<KURL> ResourceFetcher::GetUrlsOfUnusedPreloads() {
  Vector<KURL> urls;
  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    if (resource && resource->IsLinkPreload() && resource->IsUnusedPreload())
      urls.push_back(resource->Url());
  }
  return urls;
}

ArchiveResource* ResourceFetcher::CreateArchive(
    const KURL& url,
    scoped_refptr<const SharedBuffer> buffer) {
  // Only the top-frame can load MHTML.
  if (!properties_->IsMainFrame()) {
    console_logger_->AddErrorMessage(
        ConsoleLogger::Source::kScript,
        "Attempted to load a multipart archive into an subframe: " +
            url.GetString());
    return nullptr;
  }

  archive_ = MHTMLArchive::Create(url, buffer);
  if (!archive_) {
    // Log if attempting to load an invalid archive resource.
    console_logger_->AddErrorMessage(
        ConsoleLogger::Source::kScript,
        "Malformed multipart archive: " + url.GetString());
    return nullptr;
  }

  return archive_->MainResource();
}

void ResourceFetcher::HandleLoadCompletion(Resource* resource) {
  Context().DidLoadResource(resource);

  resource->ReloadIfLoFiOrPlaceholderImage(this, Resource::kReloadIfNeeded);
}

void ResourceFetcher::HandleLoaderFinish(
    Resource* resource,
    TimeTicks finish_time,
    LoaderFinishType type,
    uint32_t inflight_keepalive_bytes,
    bool should_report_corb_blocking,
    const std::vector<network::cors::PreflightTimingInfo>&
        cors_preflight_timing_info) {
  DCHECK(resource);

  DCHECK_LE(inflight_keepalive_bytes, inflight_keepalive_bytes_);
  inflight_keepalive_bytes_ -= inflight_keepalive_bytes;

  ResourceLoader* loader = resource->Loader();
  if (type == kDidFinishFirstPartInMultipart) {
    // When loading a multipart resource, make the loader non-block when
    // finishing loading the first part.
    MoveResourceLoaderToNonBlocking(loader);
  } else {
    RemoveResourceLoader(loader);
    DCHECK(!non_blocking_loaders_.Contains(loader));
  }
  DCHECK(!loaders_.Contains(loader));

  const int64_t encoded_data_length =
      resource->GetResponse().EncodedDataLength();

  if (scoped_refptr<ResourceTimingInfo> info =
          resource_timing_info_map_.Take(resource)) {
    if (resource->GetResponse().IsHTTP() &&
        resource->GetResponse().HttpStatusCode() < 400) {
      info->SetInitialURL(resource->GetResourceRequest()
                                  .GetInitialUrlForResourceTiming()
                                  .IsNull()
                              ? resource->GetResourceRequest().Url()
                              : resource->GetResourceRequest()
                                    .GetInitialUrlForResourceTiming());
      info->SetFinalResponse(resource->GetResponse());
      info->SetLoadFinishTime(finish_time);
      // encodedDataLength == -1 means "not available".
      // TODO(ricea): Find cases where it is not available but the
      // PerformanceResourceTiming spec requires it to be available and fix
      // them.
      info->AddFinalTransferSize(
          encoded_data_length == -1 ? 0 : encoded_data_length);

      if (resource->Options().request_initiator_context == kDocumentContext)
        Context().AddResourceTiming(*info);
      resource->ReportResourceTimingToClients(*info);
    }

    // Store additional timing info if CORS preflights are performed.
    for (const auto& timing_info : cors_preflight_timing_info) {
      // InitiatorType and InitialURL should be the same with each of the
      // original request.
      scoped_refptr<ResourceTimingInfo> preflight_info =
          ResourceTimingInfo::Create(info->InitiatorType(),
                                     timing_info.start_time, false);
      preflight_info->SetInitialURL(info->InitialURL());
      preflight_info->SetLoadFinishTime(timing_info.finish_time);
      preflight_info->AddFinalTransferSize(timing_info.transfer_size);

      // Set a provisional response to provide possible other information.
      ResourceResponse response(info->InitialURL());
      response.SetAlpnNegotiatedProtocol(
          WebString::FromUTF8(timing_info.alpn_negotiated_protocol));
      response.SetConnectionInfo(timing_info.connection_info);
      response.SetHTTPHeaderField(
          http_names::kTimingAllowOrigin,
          WebString::FromUTF8(timing_info.timing_allow_origin));
      response.SetEncodedDataLength(timing_info.transfer_size);
      preflight_info->SetFinalResponse(response);

      Context().AddResourceTiming(*preflight_info);
    }
  }

  resource->VirtualTimePauser().UnpauseVirtualTime();
  Context().DispatchDidFinishLoading(
      resource->Identifier(), finish_time, encoded_data_length,
      resource->GetResponse().DecodedBodyLength(), should_report_corb_blocking);

  if (type == kDidFinishLoading) {
    resource->Finish(finish_time, task_runner_.get());

    // Since this resource came from the network stack we only schedule a stale
    // while revalidate request if the network asked us to. If we called
    // ShouldRevalidateStaleResponse here then the resource would be checking
    // the freshness based on current time. It is possible that the resource
    // is fresh at the time of the network stack handling but not at the time
    // handling here and we should not be forcing a revalidation in that case.
    // eg. network stack returning a resource with max-age=0.
    bool used_stale = false;
    if (resource->GetResourceRequest().AllowsStaleResponse() &&
        resource->StaleRevalidationRequested()) {
      used_stale = true;
      ScheduleStaleRevalidate(resource);
    }

    if (resource->GetResourceRequest().IsStaleRevalidateCandidate()) {
      WebLoadingBehaviorFlag behavior = WebLoadingBehaviorFlag::
          kStaleWhileRevalidateResourceCandidateCacheLoad;
      if (used_stale) {
        behavior = WebLoadingBehaviorFlag::
            kStaleWhileRevalidateResourceCandidateStaleCacheLoad;
      } else if (resource->NetworkAccessed()) {
        behavior = WebLoadingBehaviorFlag::
            kStaleWhileRevalidateResourceCandidateNetworkLoad;
      }
      context_->DidObserveLoadingBehavior(behavior);
    }
  }

  HandleLoadCompletion(resource);
}

void ResourceFetcher::HandleLoaderError(Resource* resource,
                                        const ResourceError& error,
                                        uint32_t inflight_keepalive_bytes) {
  DCHECK(resource);

  DCHECK_LE(inflight_keepalive_bytes, inflight_keepalive_bytes_);
  inflight_keepalive_bytes_ -= inflight_keepalive_bytes;

  RemoveResourceLoader(resource->Loader());

  resource_timing_info_map_.Take(resource);

  bool is_internal_request = resource->Options().initiator_info.name ==
                             fetch_initiator_type_names::kInternal;

  resource->VirtualTimePauser().UnpauseVirtualTime();
  Context().DispatchDidFail(
      resource->LastResourceRequest().Url(), resource->Identifier(), error,
      resource->GetResponse().EncodedDataLength(), is_internal_request);

  if (error.IsCancellation())
    RemovePreload(resource);
  resource->FinishAsError(error, task_runner_.get());

  HandleLoadCompletion(resource);
}

void ResourceFetcher::MoveResourceLoaderToNonBlocking(ResourceLoader* loader) {
  DCHECK(loader);
  DCHECK(loaders_.Contains(loader));
  non_blocking_loaders_.insert(loader);
  loaders_.erase(loader);
}

bool ResourceFetcher::StartLoad(Resource* resource) {
  DCHECK(resource);
  DCHECK(resource->StillNeedsLoad());

  ResourceRequest request(resource->GetResourceRequest());
  ResourceLoader* loader = nullptr;

  {
    // Forbids JavaScript/revalidation until start()
    // to prevent unintended state transitions.
    Resource::RevalidationStartForbiddenScope
        revalidation_start_forbidden_scope(resource);
    ScriptForbiddenScope script_forbidden_scope;

    if (!Context().ShouldLoadNewResource(resource->GetType()) &&
        IsMainThread()) {
      GetMemoryCache()->Remove(resource);
      return false;
    }

    ResourceResponse response;

    blink::probe::PlatformSendRequest probe(&Context(), resource->Identifier(),
                                            request, response,
                                            resource->Options().initiator_info);

    resource->VirtualTimePauser().PauseVirtualTime();
    Context().DispatchWillSendRequest(resource->Identifier(), request, response,
                                      resource->GetType(),
                                      resource->Options().initiator_info);

    // TODO(shaochuan): Saving modified ResourceRequest back to |resource|,
    // remove once dispatchWillSendRequest() takes const ResourceRequest.
    // crbug.com/632580
    resource->SetResourceRequest(request);

    using QuotaType = decltype(inflight_keepalive_bytes_);
    QuotaType size = 0;
    if (request.GetKeepalive() && request.HttpBody()) {
      auto original_size = request.HttpBody()->SizeInBytes();
      DCHECK_LE(inflight_keepalive_bytes_, kKeepaliveInflightBytesQuota);
      if (original_size > std::numeric_limits<QuotaType>::max())
        return false;
      size = static_cast<QuotaType>(original_size);
      if (kKeepaliveInflightBytesQuota - inflight_keepalive_bytes_ < size)
        return false;

      inflight_keepalive_bytes_ += size;
    }

    loader = ResourceLoader::Create(this, scheduler_, resource, size);
    if (resource->ShouldBlockLoadEvent())
      loaders_.insert(loader);
    else
      non_blocking_loaders_.insert(loader);

    StorePerformanceTimingInitiatorInformation(resource);
  }

  loader->Start();

  {
    Resource::RevalidationStartForbiddenScope
        revalidation_start_forbidden_scope(resource);
    ScriptForbiddenScope script_forbidden_scope;

    // NotifyStartLoad() shouldn't cause AddClient/RemoveClient().
    Resource::ProhibitAddRemoveClientInScope
        prohibit_add_remove_client_in_scope(resource);
    if (!resource->IsLoaded())
      resource->NotifyStartLoad();
  }
  return true;
}

void ResourceFetcher::RemoveResourceLoader(ResourceLoader* loader) {
  DCHECK(loader);
  if (loaders_.Contains(loader))
    loaders_.erase(loader);
  else if (non_blocking_loaders_.Contains(loader))
    non_blocking_loaders_.erase(loader);
  else
    NOTREACHED();

  if (loaders_.IsEmpty() && non_blocking_loaders_.IsEmpty())
    keepalive_loaders_task_handle_.Cancel();
}

void ResourceFetcher::StopFetching() {
  StopFetchingInternal(StopFetchingTarget::kExcludingKeepaliveLoaders);
}

void ResourceFetcher::SetDefersLoading(bool defers) {
  for (const auto& loader : non_blocking_loaders_)
    loader->SetDefersLoading(defers);
  for (const auto& loader : loaders_)
    loader->SetDefersLoading(defers);
}

void ResourceFetcher::UpdateAllImageResourcePriorities() {
  TRACE_EVENT0(
      "blink",
      "ResourceLoadPriorityOptimizer::updateAllImageResourcePriorities");
  for (Resource* resource : document_resources_) {
    if (!resource || resource->GetType() != ResourceType::kImage ||
        !resource->IsLoading())
      continue;

    ResourcePriority resource_priority = resource->PriorityFromObservers();
    ResourceLoadPriority resource_load_priority = ComputeLoadPriority(
        ResourceType::kImage, resource->GetResourceRequest(),
        resource_priority.visibility);
    if (resource_load_priority == resource->GetResourceRequest().Priority())
      continue;

    resource->DidChangePriority(resource_load_priority,
                                resource_priority.intra_priority_value);
    network_instrumentation::ResourcePrioritySet(resource->Identifier(),
                                                 resource_load_priority);
    Context().DispatchDidChangeResourcePriority(
        resource->Identifier(), resource_load_priority,
        resource_priority.intra_priority_value);
  }
}

void ResourceFetcher::ReloadLoFiImages() {
  for (Resource* resource : document_resources_) {
    if (resource)
      resource->ReloadIfLoFiOrPlaceholderImage(this, Resource::kReloadAlways);
  }
}

String ResourceFetcher::GetCacheIdentifier() const {
  if (properties_->GetControllerServiceWorkerMode() !=
      mojom::ControllerServiceWorkerMode::kNoController)
    return String::Number(properties_->ServiceWorkerId());
  return MemoryCache::DefaultCacheIdentifier();
}

void ResourceFetcher::OnNetworkQuiet() {
  Context().DispatchNetworkQuiet();
  scheduler_->OnNetworkQuiet();
}

void ResourceFetcher::EmulateLoadStartedForInspector(
    Resource* resource,
    const KURL& url,
    mojom::RequestContextType request_context,
    const AtomicString& initiator_name) {
  if (CachedResource(url))
    return;
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(request_context);
  ResourceLoaderOptions options = resource->Options();
  options.initiator_info.name = initiator_name;
  FetchParameters params(resource_request, options);
  Context().CanRequest(resource->GetType(), resource->LastResourceRequest(),
                       resource->LastResourceRequest().Url(), params.Options(),
                       SecurityViolationReportingPolicy::kReport,
                       resource->LastResourceRequest().GetRedirectStatus());
  RequestLoadStarted(resource->Identifier(), resource, params, kUse);
}

void ResourceFetcher::PrepareForLeakDetection() {
  // Stop loaders including keepalive ones that may persist after page
  // navigation and thus affect instance counters of leak detection.
  StopFetchingIncludingKeepaliveLoaders();
}

void ResourceFetcher::SetStaleWhileRevalidateEnabled(bool enabled) {
  stale_while_revalidate_enabled_ = enabled;
}

void ResourceFetcher::StopFetchingInternal(StopFetchingTarget target) {
  // TODO(toyoshim): May want to suspend scheduler while canceling loaders so
  // that the cancellations below do not awake unnecessary scheduling.

  HeapVector<Member<ResourceLoader>> loaders_to_cancel;
  for (const auto& loader : non_blocking_loaders_) {
    if (target == StopFetchingTarget::kIncludingKeepaliveLoaders ||
        !loader->ShouldBeKeptAliveWhenDetached()) {
      loaders_to_cancel.push_back(loader);
    }
  }
  for (const auto& loader : loaders_) {
    if (target == StopFetchingTarget::kIncludingKeepaliveLoaders ||
        !loader->ShouldBeKeptAliveWhenDetached()) {
      loaders_to_cancel.push_back(loader);
    }
  }

  for (const auto& loader : loaders_to_cancel) {
    if (loaders_.Contains(loader) || non_blocking_loaders_.Contains(loader))
      loader->Cancel();
  }
}

void ResourceFetcher::StopFetchingIncludingKeepaliveLoaders() {
  StopFetchingInternal(StopFetchingTarget::kIncludingKeepaliveLoaders);
}

void ResourceFetcher::ScheduleStaleRevalidate(Resource* stale_resource) {
  if (stale_resource->StaleRevalidationStarted())
    return;
  stale_resource->SetStaleRevalidationStarted();
  task_runner_->PostTask(
      FROM_HERE,
      WTF::Bind(&ResourceFetcher::RevalidateStaleResource,
                WrapWeakPersistent(this), WrapPersistent(stale_resource)));
}

void ResourceFetcher::RevalidateStaleResource(Resource* stale_resource) {
  // Creating FetchParams from Resource::GetResourceRequest doesn't create
  // the exact same request as the original one, while for revalidation
  // purpose this is probably fine.
  // TODO(dtapuska): revisit this when we have a better way to re-dispatch
  // requests.
  FetchParameters params(stale_resource->GetResourceRequest());
  params.SetStaleRevalidation(true);
  RawResource::Fetch(
      params, this,
      MakeGarbageCollected<StaleRevalidationResourceClient>(stale_resource));
}

void ResourceFetcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  visitor->Trace(properties_);
  visitor->Trace(console_logger_);
  visitor->Trace(scheduler_);
  visitor->Trace(archive_);
  visitor->Trace(loaders_);
  visitor->Trace(non_blocking_loaders_);
  visitor->Trace(cached_resources_map_);
  visitor->Trace(document_resources_);
  visitor->Trace(preloads_);
  visitor->Trace(matched_preloads_);
  visitor->Trace(resource_timing_info_map_);
}

// static
const ResourceFetcher::ResourceFetcherSet&
ResourceFetcher::MainThreadFetchers() {
  return MainThreadFetchersSet();
}

}  // namespace blink
