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

#include "platform/loader/fetch/ResourceFetcher.h"

#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/mhtml/ArchiveResource.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/network/NetworkInstrumentation.h"
#include "platform/network/NetworkUtils.h"
#include "platform/probe/PlatformProbes.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"

using blink::WebURLRequest;

namespace blink {

namespace {

#define DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, name)                      \
  case Resource::k##name: {                                                 \
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
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Media)          \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Mock)           \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Raw)            \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Script)         \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, SVGDocument)    \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, TextTrack)      \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, XSLStyleSheet)  \
  }

void AddRedirectsToTimingInfo(Resource* resource, ResourceTimingInfo* info) {
  // Store redirect responses that were packed inside the final response.
  const auto& responses = resource->GetResponse().RedirectResponses();
  for (size_t i = 0; i < responses.size(); ++i) {
    const KURL& new_url = i + 1 < responses.size()
                              ? KURL(responses[i + 1].Url())
                              : resource->GetResourceRequest().Url();
    bool cross_origin =
        !SecurityOrigin::AreSameSchemeHostPort(responses[i].Url(), new_url);
    info->AddRedirect(responses[i], cross_origin);
  }
}

ResourceLoadPriority TypeToPriority(Resource::Type type) {
  switch (type) {
    case Resource::kMainResource:
    case Resource::kCSSStyleSheet:
    case Resource::kFont:
      // Also parser-blocking scripts (set explicitly in loadPriority)
      return kResourceLoadPriorityVeryHigh;
    case Resource::kXSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    case Resource::kRaw:
    case Resource::kImportResource:
    case Resource::kScript:
      // Also visible resources/images (set explicitly in loadPriority)
      return kResourceLoadPriorityHigh;
    case Resource::kManifest:
    case Resource::kMock:
      // Also late-body scripts discovered by the preload scanner (set
      // explicitly in loadPriority)
      return kResourceLoadPriorityMedium;
    case Resource::kImage:
    case Resource::kTextTrack:
    case Resource::kMedia:
    case Resource::kSVGDocument:
      // Also async scripts (set explicitly in loadPriority)
      return kResourceLoadPriorityLow;
    case Resource::kLinkPrefetch:
      return kResourceLoadPriorityVeryLow;
  }

  NOTREACHED();
  return kResourceLoadPriorityUnresolved;
}

bool ShouldResourceBeAddedToMemoryCache(const FetchParameters& params,
                                        Resource* resource) {
  if (!IsMainThread())
    return false;
  if (params.Options().data_buffering_policy == kDoNotBufferData)
    return false;

  // TODO(yhirano): Stop adding RawResources to MemoryCache completely.
  if (resource->GetType() == Resource::kMainResource)
    return false;
  if (IsRawResource(*resource) &&
      (params.IsSpeculativePreload() || params.IsLinkPreload())) {
    return false;
  }
  return true;
}

}  // namespace

ResourceLoadPriority ResourceFetcher::ComputeLoadPriority(
    Resource::Type type,
    const ResourceRequest& resource_request,
    ResourcePriority::VisibilityStatus visibility,
    FetchParameters::DeferOption defer_option,
    FetchParameters::SpeculativePreloadType speculative_preload_type,
    bool is_link_preload) {
  ResourceLoadPriority priority = TypeToPriority(type);

  // Visible resources (images in practice) get a boost to High priority.
  if (visibility == ResourcePriority::kVisible)
    priority = kResourceLoadPriorityHigh;

  // Resources before the first image are considered "early" in the document and
  // resources after the first image are "late" in the document.  Important to
  // note that this is based on when the preload scanner discovers a resource
  // for the most part so the main parser may not have reached the image element
  // yet.
  if (type == Resource::kImage && !is_link_preload)
    image_fetched_ = true;

  // A preloaded font should not take precedence over critical CSS or
  // parser-blocking scripts.
  if (type == Resource::kFont && is_link_preload)
    priority = kResourceLoadPriorityHigh;

  if (FetchParameters::kIdleLoad == defer_option) {
    priority = kResourceLoadPriorityVeryLow;
  } else if (type == Resource::kScript) {
    // Special handling for scripts.
    // Default/Parser-Blocking/Preload early in document: High (set in
    // typeToPriority)
    // Async/Defer: Low Priority (applies to both preload and parser-inserted)
    // Preload late in document: Medium
    if (FetchParameters::kLazyLoad == defer_option) {
      priority = kResourceLoadPriorityLow;
    } else if (speculative_preload_type ==
                   FetchParameters::SpeculativePreloadType::kInDocument &&
               image_fetched_) {
      // Speculative preload is used as a signal for scripts at the bottom of
      // the document.
      priority = kResourceLoadPriorityMedium;
    }
  } else if (FetchParameters::kLazyLoad == defer_option) {
    priority = kResourceLoadPriorityVeryLow;
  } else if (resource_request.GetRequestContext() ==
                 WebURLRequest::kRequestContextBeacon ||
             resource_request.GetRequestContext() ==
                 WebURLRequest::kRequestContextPing ||
             resource_request.GetRequestContext() ==
                 WebURLRequest::kRequestContextCSPReport) {
    priority = kResourceLoadPriorityVeryLow;
  }

  // A manually set priority acts as a floor. This is used to ensure that
  // synchronous requests are always given the highest possible priority, as
  // well as to ensure that there isn't priority churn if images move in and out
  // of the viewport, or are displayed more than once, both in and out of the
  // viewport.
  return std::max(priority, resource_request.Priority());
}

static void PopulateTimingInfo(ResourceTimingInfo* info, Resource* resource) {
  KURL initial_url = resource->GetResponse().RedirectResponses().IsEmpty()
                         ? resource->GetResourceRequest().Url()
                         : resource->GetResponse().RedirectResponses()[0].Url();
  info->SetInitialURL(initial_url);
  info->SetFinalResponse(resource->GetResponse());
}

WebURLRequest::RequestContext ResourceFetcher::DetermineRequestContext(
    Resource::Type type,
    IsImageSet is_image_set,
    bool is_main_frame) {
  DCHECK((is_image_set == kImageNotImageSet) ||
         (type == Resource::kImage && is_image_set == kImageIsImageSet));
  switch (type) {
    case Resource::kMainResource:
      if (!is_main_frame)
        return WebURLRequest::kRequestContextIframe;
      // FIXME: Change this to a context frame type (once we introduce them):
      // http://fetch.spec.whatwg.org/#concept-request-context-frame-type
      return WebURLRequest::kRequestContextHyperlink;
    case Resource::kXSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    case Resource::kCSSStyleSheet:
      return WebURLRequest::kRequestContextStyle;
    case Resource::kScript:
      return WebURLRequest::kRequestContextScript;
    case Resource::kFont:
      return WebURLRequest::kRequestContextFont;
    case Resource::kImage:
      if (is_image_set == kImageIsImageSet)
        return WebURLRequest::kRequestContextImageSet;
      return WebURLRequest::kRequestContextImage;
    case Resource::kRaw:
      return WebURLRequest::kRequestContextSubresource;
    case Resource::kImportResource:
      return WebURLRequest::kRequestContextImport;
    case Resource::kLinkPrefetch:
      return WebURLRequest::kRequestContextPrefetch;
    case Resource::kTextTrack:
      return WebURLRequest::kRequestContextTrack;
    case Resource::kSVGDocument:
      return WebURLRequest::kRequestContextImage;
    case Resource::kMedia:  // TODO: Split this.
      return WebURLRequest::kRequestContextVideo;
    case Resource::kManifest:
      return WebURLRequest::kRequestContextManifest;
    case Resource::kMock:
      return WebURLRequest::kRequestContextSubresource;
  }
  NOTREACHED();
  return WebURLRequest::kRequestContextSubresource;
}

ResourceFetcher::ResourceFetcher(FetchContext* new_context,
                                 RefPtr<WebTaskRunner> task_runner)
    : context_(new_context),
      scheduler_(ResourceLoadScheduler::Create(&Context())),
      archive_(Context().IsMainFrame() ? nullptr : Context().Archive()),
      resource_timing_report_timer_(
          std::move(task_runner),
          this,
          &ResourceFetcher::ResourceTimingReportTimerFired),
      auto_load_images_(true),
      images_enabled_(true),
      allow_stale_resources_(false),
      image_fetched_(false) {}

ResourceFetcher::~ResourceFetcher() {}

Resource* ResourceFetcher::CachedResource(const KURL& resource_url) const {
  KURL url = MemoryCache::RemoveFragmentIdentifierIfNeeded(resource_url);
  const WeakMember<Resource>& resource = document_resources_.at(url);
  return resource.Get();
}

bool ResourceFetcher::IsControlledByServiceWorker() const {
  return Context().IsControlledByServiceWorker();
}

bool ResourceFetcher::ResourceNeedsLoad(Resource* resource,
                                        const FetchParameters& params,
                                        RevalidationPolicy policy) {
  // Defer a font load until it is actually needed unless this is a link
  // preload.
  if (resource->GetType() == Resource::kFont && !params.IsLinkPreload())
    return false;
  if (resource->GetType() == Resource::kImage &&
      ShouldDeferImageLoad(resource->Url()))
    return false;
  return policy != kUse || resource->StillNeedsLoad();
}

// Limit the number of URLs in m_validatedURLs to avoid memory bloat.
// http://crbug.com/52411
static const int kMaxValidatedURLsSize = 10000;

void ResourceFetcher::RequestLoadStarted(unsigned long identifier,
                                         Resource* resource,
                                         const FetchParameters& params,
                                         RevalidationPolicy policy,
                                         bool is_static_data) {
  if (policy == kUse && resource->GetStatus() == ResourceStatus::kCached &&
      !validated_urls_.Contains(resource->Url())) {
    // Loaded from MemoryCache.
    DidLoadResourceFromMemoryCache(identifier, resource,
                                   params.GetResourceRequest());
  }

  if (is_static_data)
    return;

  if (policy == kUse && !resource->StillNeedsLoad() &&
      !validated_urls_.Contains(params.GetResourceRequest().Url())) {
    // Resources loaded from memory cache should be reported the first time
    // they're used.
    RefPtr<ResourceTimingInfo> info = ResourceTimingInfo::Create(
        params.Options().initiator_info.name, MonotonicallyIncreasingTime(),
        resource->GetType() == Resource::kMainResource);
    PopulateTimingInfo(info.Get(), resource);
    info->ClearLoadTimings();
    info->SetLoadFinishTime(info->InitialTime());
    scheduled_resource_timing_reports_.push_back(std::move(info));
    if (!resource_timing_report_timer_.IsActive())
      resource_timing_report_timer_.StartOneShot(0, BLINK_FROM_HERE);
  }

  if (validated_urls_.size() >= kMaxValidatedURLsSize) {
    validated_urls_.clear();
  }
  validated_urls_.insert(params.GetResourceRequest().Url());
}

void ResourceFetcher::DidLoadResourceFromMemoryCache(
    unsigned long identifier,
    Resource* resource,
    const ResourceRequest& original_resource_request) {
  ResourceRequest resource_request(resource->Url());
  resource_request.SetFrameType(original_resource_request.GetFrameType());
  resource_request.SetRequestContext(
      original_resource_request.GetRequestContext());
  Context().DispatchDidLoadResourceFromMemoryCache(identifier, resource_request,
                                                   resource->GetResponse());
  Context().DispatchWillSendRequest(identifier, resource_request,
                                    ResourceResponse() /* redirects */,
                                    resource->Options().initiator_info);
  Context().DispatchDidReceiveResponse(
      identifier, resource->GetResponse(), resource_request.GetFrameType(),
      resource_request.GetRequestContext(), resource,
      FetchContext::ResourceResponseType::kFromMemoryCache);

  if (resource->EncodedSize() > 0)
    Context().DispatchDidReceiveData(identifier, 0, resource->EncodedSize());

  Context().DispatchDidFinishLoading(
      identifier, 0, 0, resource->GetResponse().DecodedBodyLength());
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

  // TODO(japhet): We only send main resource data: urls through WebURLLoader
  // for the benefit of a service worker test
  // (RenderViewImplTest.ServiceWorkerNetworkProviderSetup), which is at a layer
  // where it isn't easy to mock out a network load. It uses data: urls to
  // emulate the behavior it wants to test, which would otherwise be reserved
  // for network loads.
  if (!archive_ && !substitute_data.IsValid() &&
      (factory.GetType() == Resource::kMainResource ||
       factory.GetType() == Resource::kRaw))
    return nullptr;

  const String cache_identifier = GetCacheIdentifier();
  if (Resource* old_resource =
          GetMemoryCache()->ResourceForURL(url, cache_identifier)) {
    // There's no reason to re-parse if we saved the data from the previous
    // parse.
    if (params.Options().data_buffering_policy != kDoNotBufferData)
      return old_resource;
    GetMemoryCache()->Remove(old_resource);
  }

  ResourceResponse response;
  RefPtr<SharedBuffer> data;
  if (substitute_data.IsValid()) {
    data = substitute_data.Content();
    response.SetURL(url);
    response.SetMimeType(substitute_data.MimeType());
    response.SetExpectedContentLength(data->size());
    response.SetTextEncodingName(substitute_data.TextEncoding());
  } else if (url.ProtocolIsData()) {
    data = NetworkUtils::ParseDataURLAndPopulateResponse(url, response);
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
    response.SetURL(url);
    response.SetMimeType(archive_resource->MimeType());
    response.SetExpectedContentLength(data->size());
    response.SetTextEncodingName(archive_resource->TextEncoding());
  }

  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  resource->SetNeedsSynchronousCacheHit(substitute_data.ForceSynchronousLoad());
  // FIXME: We should provide a body stream here.
  resource->SetStatus(ResourceStatus::kPending);
  resource->NotifyStartLoad();
  resource->ResponseReceived(response, nullptr);
  resource->SetDataBufferingPolicy(kBufferData);
  if (data->size())
    resource->SetResourceBuffer(data);
  resource->SetIdentifier(CreateUniqueIdentifier());
  resource->SetCacheIdentifier(cache_identifier);
  resource->Finish();

  if (ShouldResourceBeAddedToMemoryCache(params, resource) &&
      !substitute_data.IsValid()) {
    GetMemoryCache()->Add(resource);
  }

  return resource;
}

Resource* ResourceFetcher::ResourceForBlockedRequest(
    const FetchParameters& params,
    const ResourceFactory& factory,
    ResourceRequestBlockedReason blocked_reason) {
  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  resource->SetStatus(ResourceStatus::kPending);
  resource->NotifyStartLoad();
  resource->FinishAsError(ResourceError::CancelledDueToAccessCheckError(
      params.Url(), blocked_reason));
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
  if (it->value == resource) {
    resource->DecreasePreloadCount();
    preloads_.erase(it);
  }
}

ResourceFetcher::PrepareRequestResult ResourceFetcher::PrepareRequest(
    FetchParameters& params,
    const ResourceFactory& factory,
    const SubstituteData& substitute_data,
    unsigned long identifier,
    ResourceRequestBlockedReason& blocked_reason) {
  ResourceRequest& resource_request = params.MutableResourceRequest();

  DCHECK(params.Options().synchronous_policy == kRequestAsynchronously ||
         factory.GetType() == Resource::kRaw ||
         factory.GetType() == Resource::kXSLStyleSheet);

  params.OverrideContentType(factory.ContentType());

  SecurityViolationReportingPolicy reporting_policy =
      params.IsSpeculativePreload()
          ? SecurityViolationReportingPolicy::kSuppressReporting
          : SecurityViolationReportingPolicy::kReport;
  Context().PopulateResourceRequest(
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url()),
      factory.GetType(), params.GetClientHintsPreferences(),
      params.GetResourceWidth(), params.Options(), reporting_policy,
      resource_request);

  if (!params.Url().IsValid())
    return kAbort;

  resource_request.SetPriority(ComputeLoadPriority(
      factory.GetType(), params.GetResourceRequest(),
      ResourcePriority::kNotVisible, params.Defer(),
      params.GetSpeculativePreloadType(), params.IsLinkPreload()));
  InitializeResourceRequest(resource_request, factory.GetType(),
                            params.Defer());
  network_instrumentation::ResourcePrioritySet(identifier,
                                               resource_request.Priority());

  blocked_reason = Context().CanRequest(
      factory.GetType(), resource_request,
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url()),
      params.Options(),
      /* Don't send security violation reports for speculative preloads */
      reporting_policy, params.GetOriginRestriction());
  if (blocked_reason != ResourceRequestBlockedReason::kNone) {
    DCHECK(!substitute_data.ForceSynchronousLoad());
    return kBlock;
  }

  // For initial requests, call prepareRequest() here before revalidation
  // policy is determined.
  Context().PrepareRequest(resource_request,
                           FetchContext::RedirectType::kNotForRedirect);

  if (!params.Url().IsValid())
    return kAbort;

  RefPtr<SecurityOrigin> origin = params.Options().security_origin;
  params.MutableOptions().cors_flag =
      !origin || !origin->CanRequestNoSuborigin(params.Url());

  if (params.Options().cors_handling_by_resource_fetcher ==
      kEnableCORSHandlingByResourceFetcher) {
    bool allow_stored_credentials = false;
    switch (resource_request.GetFetchCredentialsMode()) {
      case WebURLRequest::kFetchCredentialsModeOmit:
        break;
      case WebURLRequest::kFetchCredentialsModeSameOrigin:
        allow_stored_credentials =
            !params.Options().cors_flag ||
            (origin &&
             origin->HasSuboriginAndShouldAllowCredentialsFor(params.Url()));
        break;
      case WebURLRequest::kFetchCredentialsModeInclude:
      case WebURLRequest::kFetchCredentialsModePassword:
        allow_stored_credentials = true;
        break;
    }
    resource_request.SetAllowStoredCredentials(allow_stored_credentials);
  }

  return kContinue;
}

Resource* ResourceFetcher::RequestResource(
    FetchParameters& params,
    const ResourceFactory& factory,
    const SubstituteData& substitute_data) {
  unsigned long identifier = CreateUniqueIdentifier();
  ResourceRequest& resource_request = params.MutableResourceRequest();
  network_instrumentation::ScopedResourceLoadTracker
      scoped_resource_load_tracker(identifier, resource_request);
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.Fetch.RequestResourceTime");
  // TODO(dproy): Remove this. http://crbug.com/659666
  TRACE_EVENT1("blink", "ResourceFetcher::requestResource", "url",
               UrlForTraceEvent(params.Url()));

  Resource* resource = nullptr;
  ResourceRequestBlockedReason blocked_reason =
      ResourceRequestBlockedReason::kNone;

  PrepareRequestResult result = PrepareRequest(params, factory, substitute_data,
                                               identifier, blocked_reason);
  if (result == kAbort)
    return nullptr;
  if (result == kBlock)
    return ResourceForBlockedRequest(params, factory, blocked_reason);

  if (!params.IsSpeculativePreload()) {
    // Only log if it's not for speculative preload.
    Context().RecordLoadingActivity(identifier, resource_request,
                                    factory.GetType(),
                                    params.Options().initiator_info.name);
  }

  // An URL with the "cid" scheme can only be handled by an MHTML Archive.
  // Abort the request when there is none.
  if (!archive_ && resource_request.Url().ProtocolIs(kContentIdScheme))
    return nullptr;

  bool is_data_url = resource_request.Url().ProtocolIsData();
  bool is_static_data = is_data_url || substitute_data.IsValid() || archive_;
  if (is_static_data) {
    resource = ResourceForStaticData(params, factory, substitute_data);
    // Abort the request if the archive doesn't contain the resource, except in
    // the case of data URLs which might have resources such as fonts that need
    // to be decoded only on demand. These data URLs are allowed to be
    // processed using the normal ResourceFetcher machinery.
    if (!resource && !is_data_url && archive_)
      return nullptr;
  }
  RevalidationPolicy policy = kLoad;
  bool preload_found = false;
  if (!resource) {
    resource = MatchPreload(params, factory.GetType());
    if (resource) {
      preload_found = true;
      policy = kUse;
      // If |param| is for a blocking resource and a preloaded resource is
      // found, we may need to make it block the onload event.
      MakePreloadedResourceBlockOnloadIfNeeded(resource, params);
    }
  }
  if (!preload_found) {
    if (!resource && IsMainThread()) {
      resource =
          GetMemoryCache()->ResourceForURL(params.Url(), GetCacheIdentifier());
    }

    policy = DetermineRevalidationPolicy(factory.GetType(), params, resource,
                                         is_static_data);
    TRACE_EVENT_INSTANT1(
        "blink", "ResourceFetcher::determineRevalidationPolicy",
        TRACE_EVENT_SCOPE_THREAD, "revalidationPolicy", policy);
  }

  UpdateMemoryCacheStats(resource, policy, params, factory, is_static_data);

  switch (policy) {
    case kReload:
      GetMemoryCache()->Remove(resource);
    // Fall through
    case kLoad:
      resource = CreateResourceForLoading(params, factory);
      break;
    case kRevalidate:
      InitializeRevalidation(resource_request, resource);
      break;
    case kUse:
      if (resource->IsLinkPreload() && !params.IsLinkPreload())
        resource->SetLinkPreload(false);
      break;
  }
  if (!resource)
    return nullptr;

  // TODO(yoav): turn to a DCHECK. See https://crbug.com/690632
  CHECK_EQ(resource->GetType(), factory.GetType());

  if (policy != kUse)
    resource->SetIdentifier(identifier);

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
  document_resources_.Set(
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url()), resource);

  // Returns with an existing resource if the resource does not need to start
  // loading immediately. If revalidation policy was determined as |Revalidate|,
  // the resource was already initialized for the revalidation here, but won't
  // start loading.
  if (!ResourceNeedsLoad(resource, params, policy)) {
    if (policy != kUse)
      InsertAsPreloadIfNecessary(resource, params, factory.GetType());
    return resource;
  }

  if (!StartLoad(resource))
    return nullptr;

  if (policy != kUse)
    InsertAsPreloadIfNecessary(resource, params, factory.GetType());
  scoped_resource_load_tracker.ResourceLoadContinuesBeyondScope();

  DCHECK(!resource->ErrorOccurred() ||
         params.Options().synchronous_policy == kRequestSynchronously);
  return resource;
}

void ResourceFetcher::ResourceTimingReportTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &resource_timing_report_timer_);
  Vector<RefPtr<ResourceTimingInfo>> timing_reports;
  timing_reports.swap(scheduled_resource_timing_reports_);
  for (const auto& timing_info : timing_reports)
    Context().AddResourceTiming(*timing_info);
}

WebURLRequest::RequestContext ResourceFetcher::DetermineRequestContext(
    Resource::Type type,
    IsImageSet is_image_set) const {
  return DetermineRequestContext(type, is_image_set, Context().IsMainFrame());
}

void ResourceFetcher::InitializeResourceRequest(
    ResourceRequest& request,
    Resource::Type type,
    FetchParameters::DeferOption defer) {
  if (request.GetCachePolicy() == WebCachePolicy::kUseProtocolCachePolicy) {
    request.SetCachePolicy(
        Context().ResourceRequestCachePolicy(request, type, defer));
  }
  if (request.GetRequestContext() == WebURLRequest::kRequestContextUnspecified)
    request.SetRequestContext(DetermineRequestContext(type, kImageNotImageSet));
  if (type == Resource::kLinkPrefetch)
    request.SetHTTPHeaderField(HTTPNames::Purpose, "prefetch");

  Context().AddAdditionalRequestHeaders(
      request, (type == Resource::kMainResource) ? kFetchMainResource
                                                 : kFetchSubresource);
}

void ResourceFetcher::InitializeRevalidation(
    ResourceRequest& revalidating_request,
    Resource* resource) {
  DCHECK(resource);
  DCHECK(GetMemoryCache()->Contains(resource));
  DCHECK(resource->IsLoaded());
  DCHECK(resource->CanUseCacheValidator());
  DCHECK(!resource->IsCacheValidator());
  DCHECK(!Context().IsControlledByServiceWorker());

  const AtomicString& last_modified =
      resource->GetResponse().HttpHeaderField(HTTPNames::Last_Modified);
  const AtomicString& e_tag =
      resource->GetResponse().HttpHeaderField(HTTPNames::ETag);
  if (!last_modified.IsEmpty() || !e_tag.IsEmpty()) {
    DCHECK_NE(WebCachePolicy::kBypassingCache,
              revalidating_request.GetCachePolicy());
    if (revalidating_request.GetCachePolicy() ==
        WebCachePolicy::kValidatingCacheData) {
      revalidating_request.SetHTTPHeaderField(HTTPNames::Cache_Control,
                                              "max-age=0");
    }
  }
  if (!last_modified.IsEmpty()) {
    revalidating_request.SetHTTPHeaderField(HTTPNames::If_Modified_Since,
                                            last_modified);
  }
  if (!e_tag.IsEmpty())
    revalidating_request.SetHTTPHeaderField(HTTPNames::If_None_Match, e_tag);

  resource->SetRevalidatingRequest(revalidating_request);
}

Resource* ResourceFetcher::CreateResourceForLoading(
    FetchParameters& params,
    const ResourceFactory& factory) {
  const String cache_identifier = GetCacheIdentifier();
  DCHECK(!IsMainThread() ||
         !GetMemoryCache()->ResourceForURL(params.GetResourceRequest().Url(),
                                           cache_identifier));

  RESOURCE_LOADING_DVLOG(1) << "Loading Resource for "
                            << params.GetResourceRequest().Url().ElidedString();

  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  resource->SetLinkPreload(params.IsLinkPreload());
  if (params.IsSpeculativePreload()) {
    resource->SetPreloadDiscoveryTime(params.PreloadDiscoveryTime());
  }
  resource->SetCacheIdentifier(cache_identifier);

  if (ShouldResourceBeAddedToMemoryCache(params, resource))
    GetMemoryCache()->Add(resource);
  return resource;
}

void ResourceFetcher::StorePerformanceTimingInitiatorInformation(
    Resource* resource) {
  const AtomicString& fetch_initiator = resource->Options().initiator_info.name;
  if (fetch_initiator == FetchInitiatorTypeNames::internal)
    return;

  bool is_main_resource = resource->GetType() == Resource::kMainResource;

  // The request can already be fetched in a previous navigation. Thus
  // startTime must be set accordingly.
  double start_time = resource->GetResourceRequest().NavigationStartTime()
                          ? resource->GetResourceRequest().NavigationStartTime()
                          : MonotonicallyIncreasingTime();

  // This buffer is created and populated for providing transferSize
  // and redirect timing opt-in information.
  if (is_main_resource) {
    DCHECK(!navigation_timing_info_);
    navigation_timing_info_ = ResourceTimingInfo::Create(
        fetch_initiator, start_time, is_main_resource);
  }

  RefPtr<ResourceTimingInfo> info =
      ResourceTimingInfo::Create(fetch_initiator, start_time, is_main_resource);

  if (resource->IsCacheValidator()) {
    const AtomicString& timing_allow_origin =
        resource->GetResponse().HttpHeaderField(HTTPNames::Timing_Allow_Origin);
    if (!timing_allow_origin.IsEmpty())
      info->SetOriginalTimingAllowOrigin(timing_allow_origin);
  }

  if (!is_main_resource ||
      Context().UpdateTimingInfoForIFrameNavigation(info.Get())) {
    resource_timing_info_map_.insert(resource, std::move(info));
  }
}

void ResourceFetcher::RecordResourceTimingOnRedirect(
    Resource* resource,
    const ResourceResponse& redirect_response,
    bool cross_origin) {
  ResourceTimingInfoMap::iterator it = resource_timing_info_map_.find(resource);
  if (it != resource_timing_info_map_.end()) {
    it->value->AddRedirect(redirect_response, cross_origin);
  }

  if (resource->GetType() == Resource::kMainResource) {
    DCHECK(navigation_timing_info_);
    navigation_timing_info_->AddRedirect(redirect_response, cross_origin);
  }
}

Resource* ResourceFetcher::MatchPreload(const FetchParameters& params,
                                        Resource::Type type) {
  auto it = preloads_.find(PreloadKey(params.Url(), type));
  if (it == preloads_.end())
    return nullptr;

  Resource* resource = it->value;

  if (resource->MustRefetchDueToIntegrityMetadata(params))
    return nullptr;

  if (params.IsSpeculativePreload())
    return resource;
  if (params.IsLinkPreload()) {
    resource->SetLinkPreload(true);
    return resource;
  }

  if (!IsReusableAlsoForPreloading(params, resource, false))
    return nullptr;

  resource->DecreasePreloadCount();
  preloads_.erase(it);
  matched_preloads_.push_back(resource);
  return resource;
}

void ResourceFetcher::InsertAsPreloadIfNecessary(Resource* resource,
                                                 const FetchParameters& params,
                                                 Resource::Type type) {
  if (!params.IsSpeculativePreload() && !params.IsLinkPreload())
    return;
  // CSP layout tests verify that preloads are subject to access checks by
  // seeing if they are in the `preload started` list. Therefore do not add
  // them to the list if the load is immediately denied.
  if (resource->GetResourceError().IsAccessCheck())
    return;
  PreloadKey key(params.Url(), type);
  if (preloads_.find(key) == preloads_.end()) {
    preloads_.insert(key, resource);
    resource->IncreasePreloadCount();
    if (preloaded_urls_for_test_)
      preloaded_urls_for_test_->insert(resource->Url().GetString());
  }
}

bool ResourceFetcher::IsReusableAlsoForPreloading(const FetchParameters& params,
                                                  Resource* existing_resource,
                                                  bool is_static_data) const {
  const ResourceRequest& request = params.GetResourceRequest();

  // Do not load from cache if images are not enabled. There are two general
  // cases:
  //
  // 1. Images are disabled. Don't ever load images, even if the image is cached
  // or it is a data: url. In this case, we "Reload" the image, then defer it
  // with resourceNeedsLoad() so that it never actually goes to the network.
  //
  // 2. Images are enabled, but not loaded automatically. In this case, we will
  // Use cached resources or data: urls, but will similarly fall back to a
  // deferred network load if we don't have the data available without a network
  // request. We check allowImage() here, which is affected by m_imagesEnabled
  // but not m_autoLoadImages, in order to allow for this differing behavior.
  //
  // TODO(japhet): Can we get rid of one of these settings?
  if (existing_resource->GetType() == Resource::kImage &&
      !Context().AllowImage(images_enabled_, existing_resource->Url())) {
    return false;
  }

  // Never use cache entries for downloadToFile / useStreamOnResponse requests.
  // The data will be delivered through other paths.
  if (request.DownloadToFile() || request.UseStreamOnResponse())
    return false;

  // Never reuse opaque responses from a service worker for requests that are
  // not no-cors. https://crbug.com/625575
  // TODO(yhirano): Remove this.
  if (existing_resource->GetResponse().WasFetchedViaServiceWorker() &&
      existing_resource->GetResponse().ServiceWorkerResponseType() ==
          kWebServiceWorkerResponseTypeOpaque &&
      request.GetFetchRequestMode() != WebURLRequest::kFetchRequestModeNoCORS) {
    return false;
  }

  if (is_static_data)
    return true;

  return existing_resource->CanReuse(params);
}

ResourceFetcher::RevalidationPolicy
ResourceFetcher::DetermineRevalidationPolicy(
    Resource::Type type,
    const FetchParameters& fetch_params,
    Resource* existing_resource,
    bool is_static_data) const {
  const ResourceRequest& request = fetch_params.GetResourceRequest();

  if (!existing_resource)
    return kLoad;

  // If the existing resource is loading and the associated fetcher is not equal
  // to |this|, we must not use the resource. Otherwise, CSP violation may
  // happen in redirect handling.
  if (existing_resource->Loader() &&
      existing_resource->Loader()->Fetcher() != this) {
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
  if (existing_resource->MustRefetchDueToIntegrityMetadata(fetch_params)) {
    return kReload;
  }

  // If the same URL has been loaded as a different type, we need to reload.
  if (existing_resource->GetType() != type) {
    // FIXME: If existingResource is a Preload and the new type is LinkPrefetch
    // We really should discard the new prefetch since the preload has more
    // specific type information! crbug.com/379893
    // fast/dom/HTMLLinkElement/link-and-subresource-test hits this case.
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to type mismatch.";
    return kReload;
  }

  // If |existing_resource| is not reusable as a preloaded resource, it should
  // not be reusable as a normal resource as well.
  if (!IsReusableAlsoForPreloading(fetch_params, existing_resource,
                                   is_static_data)) {
    return kReload;
  }

  // If resource was populated from a SubstituteData load or data: url, use it.
  if (is_static_data)
    return kUse;

  // Don't reload resources while pasting.
  if (allow_stale_resources_)
    return kUse;

  // WebCachePolicy::ReturnCacheDataElseLoad uses the cache no matter what.
  if (request.GetCachePolicy() == WebCachePolicy::kReturnCacheDataElseLoad)
    return kUse;

  // Don't reuse resources with Cache-control: no-store.
  if (existing_resource->HasCacheControlNoStoreHeader()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to Cache-control: no-store.";
    return kReload;
  }

  // If credentials were sent with the previous request and won't be with this
  // one, or vice versa, re-fetch the resource.
  //
  // This helps with the case where the server sends back
  // "Access-Control-Allow-Origin: *" all the time, but some of the client's
  // requests are made without CORS and some with.
  if (existing_resource->GetResourceRequest().AllowStoredCredentials() !=
      request.AllowStoredCredentials()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to difference in credentials "
                                 "settings.";
    return kReload;
  }

  // During the initial load, avoid loading the same resource multiple times for
  // a single document, even if the cache policies would tell us to. We also
  // group loads of the same resource together. Raw resources are exempted, as
  // XHRs fall into this category and may have user-set Cache-Control: headers
  // or other factors that require separate requests.
  if (type != Resource::kRaw) {
    if (!Context().IsLoadComplete() &&
        validated_urls_.Contains(existing_resource->Url()))
      return kUse;
    if (existing_resource->IsLoading())
      return kUse;
  }

  // WebCachePolicy::BypassingCache always reloads
  if (request.GetCachePolicy() == WebCachePolicy::kBypassingCache) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to "
                                 "WebCachePolicy::BypassingCache.";
    return kReload;
  }

  // We'll try to reload the resource if it failed last time.
  if (existing_resource->ErrorOccurred()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to resource being in the error "
                                 "state";
    return kReload;
  }

  // List of available images logic allows images to be re-used without cache
  // validation. We restrict this only to images from memory cache which are the
  // same as the version in the current document.
  if (type == Resource::kImage &&
      existing_resource == CachedResource(request.Url())) {
    return kUse;
  }

  if (existing_resource->MustReloadDueToVaryHeader(request))
    return kReload;

  // If any of the redirects in the chain to loading the resource were not
  // cacheable, we cannot reuse our cached resource.
  if (!existing_resource->CanReuseRedirectChain()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to an uncacheable redirect";
    return kReload;
  }

  // Check if the cache headers requires us to revalidate (cache expiration for
  // example).
  if (request.GetCachePolicy() == WebCachePolicy::kValidatingCacheData ||
      existing_resource->MustRevalidateDueToCacheHeaders() ||
      request.CacheControlContainsNoCache()) {
    // See if the resource has usable ETag or Last-modified headers. If the page
    // is controlled by the ServiceWorker, we choose the Reload policy because
    // the revalidation headers should not be exposed to the
    // ServiceWorker.(crbug.com/429570)
    if (existing_resource->CanUseCacheValidator() &&
        !Context().IsControlledByServiceWorker()) {
      // If the resource is already a cache validator but not started yet, the
      // |Use| policy should be applied to subsequent requests.
      if (existing_resource->IsCacheValidator()) {
        DCHECK(existing_resource->StillNeedsLoad());
        return kUse;
      }
      return kRevalidate;
    }

    // No, must reload.
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
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
  for (Resource* resource : document_resources_.Values()) {
    if (resource->GetType() == Resource::kImage && resource->StillNeedsLoad() &&
        !ShouldDeferImageLoad(resource->Url()))
      StartLoad(resource);
  }
}

void ResourceFetcher::ClearContext() {
  scheduler_->Shutdown();
  ClearPreloads(ResourceFetcher::kClearAllPreloads);
  context_ = Context().Detach();
}

int ResourceFetcher::BlockingRequestCount() const {
  return loaders_.size();
}

int ResourceFetcher::NonblockingRequestCount() const {
  return non_blocking_loaders_.size();
}

void ResourceFetcher::EnableIsPreloadedForTest() {
  if (preloaded_urls_for_test_)
    return;
  preloaded_urls_for_test_ = WTF::WrapUnique(new HashSet<String>);

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
  LogPreloadStats(policy);

  Vector<PreloadKey> keys_to_be_removed;
  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    if (policy == kClearAllPreloads || !resource->IsLinkPreload()) {
      resource->DecreasePreloadCount();
      if (resource->GetPreloadResult() == Resource::kPreloadNotReferenced)
        GetMemoryCache()->Remove(resource);
      keys_to_be_removed.push_back(pair.key);
    }
  }
  preloads_.RemoveAll(keys_to_be_removed);

  matched_preloads_.clear();
}

void ResourceFetcher::WarnUnusedPreloads() {
  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    if (resource && resource->IsLinkPreload() &&
        resource->GetPreloadResult() == Resource::kPreloadNotReferenced) {
      Context().AddConsoleMessage(
          "The resource " + resource->Url().GetString() +
              " was preloaded using link preload but not used within a few "
              "seconds from the window's load event. Please make sure it "
              "wasn't preloaded for nothing.",
          FetchContext::kLogWarningMessage);
    }
  }
}

ArchiveResource* ResourceFetcher::CreateArchive(Resource* resource) {
  // Only the top-frame can load MHTML.
  if (!Context().IsMainFrame()) {
    Context().AddConsoleMessage(
        "Attempted to load a multipart archive into an subframe: " +
            resource->Url().GetString(),
        FetchContext::kLogErrorMessage);
    return nullptr;
  }

  archive_ = MHTMLArchive::Create(resource->Url(), resource->ResourceBuffer());
  if (!archive_) {
    // Log if attempting to load an invalid archive resource.
    Context().AddConsoleMessage(
        "Malformed multipart archive: " + resource->Url().GetString(),
        FetchContext::kLogErrorMessage);
    return nullptr;
  }

  return archive_->MainResource();
}

ResourceTimingInfo* ResourceFetcher::GetNavigationTimingInfo() {
  return navigation_timing_info_.Get();
}

void ResourceFetcher::HandleLoadCompletion(Resource* resource) {
  Context().DidLoadResource(resource);

  resource->ReloadIfLoFiOrPlaceholderImage(this, Resource::kReloadIfNeeded);
}

void ResourceFetcher::HandleLoaderFinish(Resource* resource,
                                         double finish_time,
                                         LoaderFinishType type) {
  DCHECK(resource);

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

  if (resource->GetType() == Resource::kMainResource) {
    DCHECK(navigation_timing_info_);
    // Store redirect responses that were packed inside the final response.
    AddRedirectsToTimingInfo(resource, navigation_timing_info_.Get());
    if (resource->GetResponse().IsHTTP()) {
      PopulateTimingInfo(navigation_timing_info_.Get(), resource);
      navigation_timing_info_->AddFinalTransferSize(
          encoded_data_length == -1 ? 0 : encoded_data_length);
    }
  }
  if (RefPtr<ResourceTimingInfo> info =
          resource_timing_info_map_.Take(resource)) {
    // Store redirect responses that were packed inside the final response.
    AddRedirectsToTimingInfo(resource, info.Get());

    if (resource->GetResponse().IsHTTP() &&
        resource->GetResponse().HttpStatusCode() < 400) {
      PopulateTimingInfo(info.Get(), resource);
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
  }

  Context().DispatchDidFinishLoading(
      resource->Identifier(), finish_time, encoded_data_length,
      resource->GetResponse().DecodedBodyLength());

  if (type == kDidFinishLoading)
    resource->Finish(finish_time);

  HandleLoadCompletion(resource);
}

void ResourceFetcher::HandleLoaderError(Resource* resource,
                                        const ResourceError& error) {
  DCHECK(resource);

  RemoveResourceLoader(resource->Loader());

  resource_timing_info_map_.Take(resource);

  bool is_internal_request = resource->Options().initiator_info.name ==
                             FetchInitiatorTypeNames::internal;

  Context().DispatchDidFail(resource->Identifier(), error,
                            resource->GetResponse().EncodedDataLength(),
                            is_internal_request);

  if (error.IsCancellation())
    RemovePreload(resource);
  resource->FinishAsError(error);

  HandleLoadCompletion(resource);
}

void ResourceFetcher::MoveResourceLoaderToNonBlocking(ResourceLoader* loader) {
  DCHECK(loader);
  // TODO(yoav): Convert CHECK to DCHECK if no crash reports come in.
  CHECK(loaders_.Contains(loader));
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
    ScriptForbiddenIfMainThreadScope script_forbidden_scope;

    if (!Context().ShouldLoadNewResource(resource->GetType()) &&
        IsMainThread()) {
      GetMemoryCache()->Remove(resource);
      return false;
    }

    ResourceResponse response;

    blink::probe::PlatformSendRequest probe(&Context(), resource->Identifier(),
                                            request, response,
                                            resource->Options().initiator_info);

    Context().DispatchWillSendRequest(resource->Identifier(), request, response,
                                      resource->Options().initiator_info);

    // Resource requests from suborigins should not be intercepted by the
    // service worker of the physical origin. This has the effect that, for now,
    // suborigins do not work with service workers. See
    // https://w3c.github.io/webappsec-suborigins/.
    SecurityOrigin* source_origin = Context().GetSecurityOrigin();
    if (source_origin && source_origin->HasSuborigin())
      request.SetServiceWorkerMode(WebURLRequest::ServiceWorkerMode::kNone);

    // TODO(shaochuan): Saving modified ResourceRequest back to |resource|,
    // remove once dispatchWillSendRequest() takes const ResourceRequest.
    // crbug.com/632580
    resource->SetResourceRequest(request);

    loader = ResourceLoader::Create(this, scheduler_, resource);
    if (resource->ShouldBlockLoadEvent())
      loaders_.insert(loader);
    else
      non_blocking_loaders_.insert(loader);

    StorePerformanceTimingInitiatorInformation(resource);
    resource->SetFetcherSecurityOrigin(source_origin);

    // NotifyStartLoad() shouldn't cause AddClient/RemoveClient().
    Resource::ProhibitAddRemoveClientInScope
        prohibit_add_remove_client_in_scope(resource);

    resource->NotifyStartLoad();
  }

  loader->Start();
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
}

void ResourceFetcher::StopFetching() {
  // TODO(toyoshim): May want to suspend scheduler while canceling loaders so
  // that the cancellations below do not awake unnecessary scheduling.

  HeapVector<Member<ResourceLoader>> loaders_to_cancel;
  for (const auto& loader : non_blocking_loaders_) {
    if (!loader->GetKeepalive())
      loaders_to_cancel.push_back(loader);
  }
  for (const auto& loader : loaders_) {
    if (!loader->GetKeepalive())
      loaders_to_cancel.push_back(loader);
  }

  for (const auto& loader : loaders_to_cancel) {
    if (loaders_.Contains(loader) || non_blocking_loaders_.Contains(loader))
      loader->Cancel();
  }
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
  for (const auto& document_resource : document_resources_) {
    Resource* resource = document_resource.value.Get();
    if (!resource || resource->GetType() != Resource::kImage ||
        !resource->IsLoading())
      continue;

    ResourcePriority resource_priority = resource->PriorityFromObservers();
    ResourceLoadPriority resource_load_priority =
        ComputeLoadPriority(Resource::kImage, resource->GetResourceRequest(),
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
  for (const auto& document_resource : document_resources_) {
    Resource* resource = document_resource.value.Get();
    if (resource)
      resource->ReloadIfLoFiOrPlaceholderImage(this, Resource::kReloadAlways);
  }
}

void ResourceFetcher::LogPreloadStats(ClearPreloadsPolicy policy) {
  unsigned scripts = 0;
  unsigned script_misses = 0;
  unsigned stylesheets = 0;
  unsigned stylesheet_misses = 0;
  unsigned images = 0;
  unsigned image_misses = 0;
  unsigned fonts = 0;
  unsigned font_misses = 0;
  unsigned medias = 0;
  unsigned media_misses = 0;
  unsigned text_tracks = 0;
  unsigned text_track_misses = 0;
  unsigned imports = 0;
  unsigned import_misses = 0;
  unsigned raws = 0;
  unsigned raw_misses = 0;
  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    // Do not double count link rel preloads. These do not get cleared if the
    // ClearPreloadsPolicy is only clearing speculative markup preloads.
    if (resource->IsLinkPreload() &&
        policy == kClearSpeculativeMarkupPreloads) {
      continue;
    }
    int miss_count =
        resource->GetPreloadResult() == Resource::kPreloadNotReferenced ? 1 : 0;
    switch (resource->GetType()) {
      case Resource::kImage:
        images++;
        image_misses += miss_count;
        break;
      case Resource::kScript:
        scripts++;
        script_misses += miss_count;
        break;
      case Resource::kCSSStyleSheet:
        stylesheets++;
        stylesheet_misses += miss_count;
        break;
      case Resource::kFont:
        fonts++;
        font_misses += miss_count;
        break;
      case Resource::kMedia:
        medias++;
        media_misses += miss_count;
        break;
      case Resource::kTextTrack:
        text_tracks++;
        text_track_misses += miss_count;
        break;
      case Resource::kImportResource:
        imports++;
        import_misses += miss_count;
        break;
      case Resource::kRaw:
        raws++;
        raw_misses += miss_count;
        break;
      case Resource::kMock:
        // Do not count Resource::Mock because this type is only for testing.
        break;
      default:
        NOTREACHED();
    }
  }
  DEFINE_STATIC_LOCAL(CustomCountHistogram, image_preloads,
                      ("PreloadScanner.Counts2.Image", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, image_preload_misses,
                      ("PreloadScanner.Counts2.Miss.Image", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, script_preloads,
                      ("PreloadScanner.Counts2.Script", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, script_preload_misses,
                      ("PreloadScanner.Counts2.Miss.Script", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, stylesheet_preloads,
                      ("PreloadScanner.Counts2.CSSStyleSheet", 0, 100, 25));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, stylesheet_preload_misses,
      ("PreloadScanner.Counts2.Miss.CSSStyleSheet", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, font_preloads,
                      ("PreloadScanner.Counts2.Font", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, font_preload_misses,
                      ("PreloadScanner.Counts2.Miss.Font", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, media_preloads,
                      ("PreloadScanner.Counts2.Media", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, media_preload_misses,
                      ("PreloadScanner.Counts2.Miss.Media", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, text_track_preloads,
                      ("PreloadScanner.Counts2.TextTrack", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, text_track_preload_misses,
                      ("PreloadScanner.Counts2.Miss.TextTrack", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, import_preloads,
                      ("PreloadScanner.Counts2.Import", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, import_preload_misses,
                      ("PreloadScanner.Counts2.Miss.Import", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, raw_preloads,
                      ("PreloadScanner.Counts2.Raw", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, raw_preload_misses,
                      ("PreloadScanner.Counts2.Miss.Raw", 0, 100, 25));
  if (images)
    image_preloads.Count(images);
  if (image_misses)
    image_preload_misses.Count(image_misses);
  if (scripts)
    script_preloads.Count(scripts);
  if (script_misses)
    script_preload_misses.Count(script_misses);
  if (stylesheets)
    stylesheet_preloads.Count(stylesheets);
  if (stylesheet_misses)
    stylesheet_preload_misses.Count(stylesheet_misses);
  if (fonts)
    font_preloads.Count(fonts);
  if (font_misses)
    font_preload_misses.Count(font_misses);
  if (medias)
    media_preloads.Count(medias);
  if (media_misses)
    media_preload_misses.Count(media_misses);
  if (text_tracks)
    text_track_preloads.Count(text_tracks);
  if (text_track_misses)
    text_track_preload_misses.Count(text_track_misses);
  if (imports)
    import_preloads.Count(imports);
  if (import_misses)
    import_preload_misses.Count(import_misses);
  if (raws)
    raw_preloads.Count(raws);
  if (raw_misses)
    raw_preload_misses.Count(raw_misses);
}

String ResourceFetcher::GetCacheIdentifier() const {
  if (Context().IsControlledByServiceWorker())
    return String::Number(Context().ServiceWorkerID());
  return MemoryCache::DefaultCacheIdentifier();
}

void ResourceFetcher::EmulateLoadStartedForInspector(
    Resource* resource,
    const KURL& url,
    WebURLRequest::RequestContext request_context,
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
                       params.GetOriginRestriction());
  RequestLoadStarted(resource->Identifier(), resource, params, kUse);
}

DEFINE_TRACE(ResourceFetcher) {
  visitor->Trace(context_);
  visitor->Trace(scheduler_);
  visitor->Trace(archive_);
  visitor->Trace(loaders_);
  visitor->Trace(non_blocking_loaders_);
  visitor->Trace(document_resources_);
  visitor->Trace(preloads_);
  visitor->Trace(matched_preloads_);
  visitor->Trace(resource_timing_info_map_);
}

}  // namespace blink
