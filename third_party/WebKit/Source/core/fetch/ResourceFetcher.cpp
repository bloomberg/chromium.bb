/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "core/fetch/ResourceFetcher.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "core/FetchInitiatorTypeNames.h"
#include "core/dom/Document.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/DocumentResource.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/ResourceLoaderSet.h"
#include "core/fetch/ScriptResource.h"
#include "core/fetch/SubstituteData.h"
#include "core/fetch/UniqueIdentifier.h"
#include "core/fetch/XSLStyleSheetResource.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/PingLoader.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "core/timing/ResourceTimingInfo.h"
#include "core/svg/graphics/SVGImageChromeClient.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/mhtml/ArchiveResource.h"
#include "platform/mhtml/ArchiveResourceCollection.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

#define PRELOAD_DEBUG 0

using blink::WebURLRequest;

namespace blink {

static Resource* createResource(Resource::Type type, const ResourceRequest& request, const String& charset)
{
    switch (type) {
    case Resource::Image:
        return new ImageResource(request);
    case Resource::CSSStyleSheet:
        return new CSSStyleSheetResource(request, charset);
    case Resource::Script:
        return ScriptResource::create(request, charset).leakPtr();
    case Resource::SVGDocument:
        return new DocumentResource(request, Resource::SVGDocument);
    case Resource::Font:
        return new FontResource(request);
    case Resource::MainResource:
    case Resource::Raw:
    case Resource::TextTrack:
    case Resource::Media:
        return new RawResource(request, type);
    case Resource::XSLStyleSheet:
        return new XSLStyleSheetResource(request, charset);
    case Resource::LinkPrefetch:
        return new Resource(request, Resource::LinkPrefetch);
    case Resource::LinkSubresource:
        return new Resource(request, Resource::LinkSubresource);
    case Resource::ImportResource:
        return new RawResource(request, type);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static ResourceLoadPriority loadPriority(Resource::Type type, const FetchRequest& request)
{
    if (request.priority() != ResourceLoadPriorityUnresolved)
        return request.priority();

    switch (type) {
    case Resource::MainResource:
        return ResourceLoadPriorityVeryHigh;
    case Resource::CSSStyleSheet:
        return ResourceLoadPriorityHigh;
    case Resource::Raw:
        return request.options().synchronousPolicy == RequestSynchronously ? ResourceLoadPriorityVeryHigh : ResourceLoadPriorityMedium;
    case Resource::Script:
        // Async scripts do not block the parser so they get the lowest priority and can be
        // loaded in parser order with images.
        if (FetchRequest::LazyLoad == request.defer())
            return ResourceLoadPriorityLow;
        return ResourceLoadPriorityMedium;
    case Resource::Font:
    case Resource::ImportResource:
        return ResourceLoadPriorityMedium;
    case Resource::Image:
        // Default images to VeryLow, and promote whatever is visible. This improves
        // speed-index by ~5% on average, ~14% at the 99th percentile.
        return ResourceLoadPriorityVeryLow;
    case Resource::Media:
        return ResourceLoadPriorityLow;
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
        return ResourceLoadPriorityHigh;
    case Resource::SVGDocument:
        return ResourceLoadPriorityLow;
    case Resource::LinkPrefetch:
        return ResourceLoadPriorityVeryLow;
    case Resource::LinkSubresource:
        return ResourceLoadPriorityLow;
    case Resource::TextTrack:
        return ResourceLoadPriorityLow;
    }
    ASSERT_NOT_REACHED();
    return ResourceLoadPriorityUnresolved;
}

static Resource* resourceFromDataURIRequest(const ResourceRequest& request, const ResourceLoaderOptions& resourceOptions, const String& cacheIdentifier)
{
    const KURL& url = request.url();
    ASSERT(url.protocolIsData());

    blink::WebString mimetype;
    blink::WebString charset;
    RefPtr<SharedBuffer> data = PassRefPtr<SharedBuffer>(blink::Platform::current()->parseDataURL(url, mimetype, charset));
    if (!data)
        return nullptr;
    ResourceResponse response(url, mimetype, data->size(), charset, String());

    Resource* resource = createResource(Resource::Image, request, charset);
    resource->setOptions(resourceOptions);
    // FIXME: We should provide a body stream here.
    resource->responseReceived(response, nullptr);
    if (data->size())
        resource->setResourceBuffer(data);
    resource->setCacheIdentifier(cacheIdentifier);
    resource->finish();
    return resource;
}

static void populateResourceTiming(ResourceTimingInfo* info, Resource* resource, bool clearLoadTimings)
{
    info->setInitialRequest(resource->resourceRequest());
    info->setFinalResponse(resource->response());
    if (clearLoadTimings) {
        info->clearLoadTimings();
        info->setLoadFinishTime(info->initialTime());
    } else {
        info->setLoadFinishTime(resource->loadFinishTime());
    }
}

static void reportResourceTiming(ResourceTimingInfo* info, Document* initiatorDocument, bool isMainResource)
{
    if (initiatorDocument && isMainResource)
        initiatorDocument = initiatorDocument->parentDocument();
    if (!initiatorDocument || !initiatorDocument->loader())
        return;
    if (LocalDOMWindow* initiatorWindow = initiatorDocument->domWindow())
        DOMWindowPerformance::performance(*initiatorWindow)->addResourceTiming(*info, initiatorDocument);
}

static WebURLRequest::RequestContext requestContextFromType(const ResourceFetcher* fetcher, Resource::Type type)
{
    switch (type) {
    case Resource::MainResource:
        if (fetcher->frame()->tree().parent())
            return WebURLRequest::RequestContextIframe;
        // FIXME: Change this to a context frame type (once we introduce them): http://fetch.spec.whatwg.org/#concept-request-context-frame-type
        return WebURLRequest::RequestContextHyperlink;
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::CSSStyleSheet:
        return WebURLRequest::RequestContextStyle;
    case Resource::Script:
        return WebURLRequest::RequestContextScript;
    case Resource::Font:
        return WebURLRequest::RequestContextFont;
    case Resource::Image:
        return WebURLRequest::RequestContextImage;
    case Resource::Raw:
        return WebURLRequest::RequestContextSubresource;
    case Resource::ImportResource:
        return WebURLRequest::RequestContextImport;
    case Resource::LinkPrefetch:
        return WebURLRequest::RequestContextPrefetch;
    case Resource::LinkSubresource:
        return WebURLRequest::RequestContextSubresource;
    case Resource::TextTrack:
        return WebURLRequest::RequestContextTrack;
    case Resource::SVGDocument:
        return WebURLRequest::RequestContextImage;
    case Resource::Media: // TODO: Split this.
        return WebURLRequest::RequestContextVideo;
    }
    ASSERT_NOT_REACHED();
    return WebURLRequest::RequestContextSubresource;
}

static ResourceRequestCachePolicy memoryCachePolicyToResourceRequestCachePolicy(
    const CachePolicy policy) {
    if (policy == CachePolicyVerify)
        return UseProtocolCachePolicy;
    if (policy == CachePolicyRevalidate)
        return ReloadIgnoringCacheData;
    if (policy == CachePolicyReload)
        return ReloadBypassingCache;
    if (policy == CachePolicyHistoryBuffer)
        return ReturnCacheDataElseLoad;
    return UseProtocolCachePolicy;
}

ResourceFetcher::ResourceFetcher(PassOwnPtrWillBeRawPtr<FetchContext> context)
    : m_context(context)
    , m_garbageCollectDocumentResourcesTimer(this, &ResourceFetcher::garbageCollectDocumentResourcesTimerFired)
    , m_resourceTimingReportTimer(this, &ResourceFetcher::resourceTimingReportTimerFired)
    , m_autoLoadImages(true)
    , m_imagesEnabled(true)
    , m_allowStaleResources(false)
{
}

ResourceFetcher::~ResourceFetcher()
{
    clearPreloads();

#if !ENABLE(OILPAN)
    // Make sure no requests still point to this ResourceFetcher
    // Oilpan: no object reference can be keeping this alive,
    // so property trivially holds.
    ASSERT(!m_loaders || m_loaders->isEmpty());
#endif
}

Resource* ResourceFetcher::cachedResource(const KURL& resourceURL) const
{
    KURL url = MemoryCache::removeFragmentIdentifierIfNeeded(resourceURL);
    return m_documentResources.get(url).get();
}

ResourcePtr<Resource> ResourceFetcher::fetchSynchronously(FetchRequest& request)
{
    ASSERT(document());
    request.mutableResourceRequest().setTimeoutInterval(10);
    ResourceLoaderOptions options(request.options());
    options.synchronousPolicy = RequestSynchronously;
    request.setOptions(options);
    return requestResource(Resource::Raw, request);
}

ResourcePtr<ImageResource> ResourceFetcher::fetchImage(FetchRequest& request)
{
    if (request.resourceRequest().requestContext() == WebURLRequest::RequestContextUnspecified)
        request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextImage);
    if (LocalFrame* f = frame()) {
        if (f->document()->pageDismissalEventBeingDispatched() != Document::NoDismissal) {
            KURL requestURL = request.resourceRequest().url();
            if (requestURL.isValid() && canRequest(Resource::Image, request.resourceRequest(), requestURL, request.options(), request.forPreload(), request.originRestriction()))
                PingLoader::loadImage(f, requestURL);
            return 0;
        }
    }

    if (request.resourceRequest().url().protocolIsData())
        preCacheDataURIImage(request);

    request.setDefer(clientDefersImage(request.resourceRequest().url()) ? FetchRequest::DeferredByClient : FetchRequest::NoDefer);
    ResourcePtr<Resource> resource = requestResource(Resource::Image, request);
    return resource && resource->type() == Resource::Image ? toImageResource(resource) : 0;
}

void ResourceFetcher::preCacheDataURIImage(const FetchRequest& request)
{
    const KURL& url = request.resourceRequest().url();
    ASSERT(url.protocolIsData());

    const String cacheIdentifier = getCacheIdentifier();
    if (memoryCache()->resourceForURL(url, cacheIdentifier))
        return;

    if (Resource* resource = resourceFromDataURIRequest(request.resourceRequest(), request.options(), cacheIdentifier)) {
        memoryCache()->add(resource);
        scheduleDocumentResourcesGC();
    }
}

ResourcePtr<FontResource> ResourceFetcher::fetchFont(FetchRequest& request)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextFont);
    return toFontResource(requestResource(Resource::Font, request));
}

ResourcePtr<RawResource> ResourceFetcher::fetchImport(FetchRequest& request)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextImport);
    return toRawResource(requestResource(Resource::ImportResource, request));
}

ResourcePtr<CSSStyleSheetResource> ResourceFetcher::fetchCSSStyleSheet(FetchRequest& request)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextStyle);
    return toCSSStyleSheetResource(requestResource(Resource::CSSStyleSheet, request));
}

ResourcePtr<ScriptResource> ResourceFetcher::fetchScript(FetchRequest& request)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextScript);
    return toScriptResource(requestResource(Resource::Script, request));
}

ResourcePtr<XSLStyleSheetResource> ResourceFetcher::fetchXSLStyleSheet(FetchRequest& request)
{
    ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextXSLT);
    return toXSLStyleSheetResource(requestResource(Resource::XSLStyleSheet, request));
}

ResourcePtr<DocumentResource> ResourceFetcher::fetchSVGDocument(FetchRequest& request)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextImage);
    return toDocumentResource(requestResource(Resource::SVGDocument, request));
}

ResourcePtr<Resource> ResourceFetcher::fetchLinkResource(Resource::Type type, FetchRequest& request)
{
    ASSERT(frame());
    ASSERT(type == Resource::LinkPrefetch || type == Resource::LinkSubresource);
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(type == Resource::LinkPrefetch ? WebURLRequest::RequestContextPrefetch : WebURLRequest::RequestContextSubresource);
    return requestResource(type, request);
}

ResourcePtr<RawResource> ResourceFetcher::fetchRawResource(FetchRequest& request)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    ASSERT(request.resourceRequest().requestContext() != WebURLRequest::RequestContextUnspecified);
    return toRawResource(requestResource(Resource::Raw, request));
}

ResourcePtr<RawResource> ResourceFetcher::fetchMainResource(FetchRequest& request, const SubstituteData& substituteData)
{
    ASSERT(request.resourceRequest().frameType() != WebURLRequest::FrameTypeNone);
    ASSERT(request.resourceRequest().requestContext() == WebURLRequest::RequestContextForm || request.resourceRequest().requestContext() == WebURLRequest::RequestContextFrame || request.resourceRequest().requestContext() == WebURLRequest::RequestContextHyperlink || request.resourceRequest().requestContext() == WebURLRequest::RequestContextIframe || request.resourceRequest().requestContext() == WebURLRequest::RequestContextInternal || request.resourceRequest().requestContext() == WebURLRequest::RequestContextLocation);

    if (substituteData.isValid())
        preCacheSubstituteDataForMainResource(request, substituteData);
    return toRawResource(requestResource(Resource::MainResource, request));
}

ResourcePtr<RawResource> ResourceFetcher::fetchMedia(FetchRequest& request)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    ASSERT(request.resourceRequest().requestContext() == WebURLRequest::RequestContextAudio || request.resourceRequest().requestContext() == WebURLRequest::RequestContextVideo);
    return toRawResource(requestResource(Resource::Media, request));
}

ResourcePtr<RawResource> ResourceFetcher::fetchTextTrack(FetchRequest& request)
{
    ASSERT(request.resourceRequest().frameType() == WebURLRequest::FrameTypeNone);
    request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextTrack);
    return toRawResource(requestResource(Resource::TextTrack, request));
}

void ResourceFetcher::preCacheSubstituteDataForMainResource(const FetchRequest& request, const SubstituteData& substituteData)
{
    const String cacheIdentifier = getCacheIdentifier();
    const KURL& url = request.url();
    if (Resource* oldResource = memoryCache()->resourceForURL(url, cacheIdentifier))
        memoryCache()->remove(oldResource);

    ResourceResponse response(url, substituteData.mimeType(), substituteData.content()->size(), substituteData.textEncoding(), emptyString());
    ResourcePtr<Resource> resource = createResource(Resource::MainResource, request.resourceRequest(), substituteData.textEncoding());
    resource->setNeedsSynchronousCacheHit(substituteData.forceSynchronousLoad());
    resource->setOptions(request.options());
    resource->setDataBufferingPolicy(BufferData);
    resource->responseReceived(response, nullptr);
    if (substituteData.content()->size())
        resource->setResourceBuffer(substituteData.content());
    resource->setCacheIdentifier(cacheIdentifier);
    resource->finish();
    memoryCache()->add(resource.get());
}

bool ResourceFetcher::canRequest(Resource::Type type, const ResourceRequest& resourceRequest, const KURL& url, const ResourceLoaderOptions& options, bool forPreload, FetchRequest::OriginRestriction originRestriction) const
{
    SecurityOrigin* securityOrigin = options.securityOrigin.get();
    if (!securityOrigin && document())
        securityOrigin = document()->securityOrigin();

    if (originRestriction != FetchRequest::NoOriginRestriction && securityOrigin && !securityOrigin->canDisplay(url)) {
        if (!forPreload)
            context().reportLocalLoadFailed(url);
        WTF_LOG(ResourceLoading, "ResourceFetcher::requestResource URL was not allowed by SecurityOrigin::canDisplay");
        return 0;
    }

    // Some types of resources can be loaded only from the same origin. Other
    // types of resources, like Images, Scripts, and CSS, can be loaded from
    // any URL.
    switch (type) {
    case Resource::MainResource:
    case Resource::Image:
    case Resource::CSSStyleSheet:
    case Resource::Script:
    case Resource::Font:
    case Resource::Raw:
    case Resource::LinkPrefetch:
    case Resource::LinkSubresource:
    case Resource::TextTrack:
    case Resource::ImportResource:
    case Resource::Media:
        // By default these types of resources can be loaded from any origin.
        // FIXME: Are we sure about Resource::Font?
        if (originRestriction == FetchRequest::RestrictToSameOrigin && !securityOrigin->canRequest(url)) {
            printAccessDeniedMessage(url);
            return false;
        }
        break;
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::SVGDocument:
        if (!securityOrigin->canRequest(url)) {
            printAccessDeniedMessage(url);
            return false;
        }
        break;
    }

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    bool shouldBypassMainWorldCSP = (frame() && frame()->script().shouldBypassMainWorldCSP()) || (options.contentSecurityPolicyOption == DoNotCheckContentSecurityPolicy);

    // Don't send CSP messages for preloads, we might never actually display those items.
    ContentSecurityPolicy::ReportingStatus cspReporting = forPreload ?
        ContentSecurityPolicy::SuppressReport : ContentSecurityPolicy::SendReport;

    // document() can be null, but not in any of the cases where csp is actually used below.
    // ImageResourceTest.MultipartImage crashes w/o the document() null check.
    // I believe it's the Resource::Raw case.
    const ContentSecurityPolicy* csp = document() ? document()->contentSecurityPolicy() : nullptr;

    // FIXME: This would be cleaner if moved this switch into an allowFromSource()
    // helper on this object which took a Resource::Type, then this block would
    // collapse to about 10 lines for handling Raw and Script special cases.
    switch (type) {
    case Resource::XSLStyleSheet:
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
        if (!shouldBypassMainWorldCSP && !csp->allowScriptFromSource(url, cspReporting))
            return false;
        break;
    case Resource::Script:
    case Resource::ImportResource:
        if (!shouldBypassMainWorldCSP && !csp->allowScriptFromSource(url, cspReporting))
            return false;

        if (frame()) {
            Settings* settings = frame()->settings();
            if (!frame()->loader().client()->allowScriptFromSource(!settings || settings->scriptEnabled(), url)) {
                frame()->loader().client()->didNotAllowScript();
                return false;
            }
        }
        break;
    case Resource::CSSStyleSheet:
        if (!shouldBypassMainWorldCSP && !csp->allowStyleFromSource(url, cspReporting))
            return false;
        break;
    case Resource::SVGDocument:
    case Resource::Image:
        if (!shouldBypassMainWorldCSP && !csp->allowImageFromSource(url, cspReporting))
            return false;
        break;
    case Resource::Font: {
        if (!shouldBypassMainWorldCSP && !csp->allowFontFromSource(url, cspReporting))
            return false;
        break;
    }
    case Resource::MainResource:
    case Resource::Raw:
    case Resource::LinkPrefetch:
    case Resource::LinkSubresource:
        break;
    case Resource::Media:
    case Resource::TextTrack:
        if (!shouldBypassMainWorldCSP && !csp->allowMediaFromSource(url, cspReporting))
            return false;

        if (frame()) {
            if (!frame()->loader().client()->allowMedia(url))
                return false;
        }
        break;
    }

    // SVG Images have unique security rules that prevent all subresource requests
    // except for data urls.
    if (type != Resource::MainResource) {
        if (frame() && frame()->chromeClient().isSVGImageChromeClient() && !url.protocolIsData())
            return false;
    }

    // FIXME: Once we use RequestContext for CSP (http://crbug.com/390497), remove this extra check.
    if (resourceRequest.requestContext() == WebURLRequest::RequestContextManifest) {
        if (!shouldBypassMainWorldCSP && !csp->allowManifestFromSource(url, cspReporting))
            return false;
    }

    // Measure the number of legacy URL schemes ('ftp://') and the number of embedded-credential
    // ('http://user:password@...') resources embedded as subresources. in the hopes that we can
    // block them at some point in the future.
    if (resourceRequest.frameType() != WebURLRequest::FrameTypeTopLevel) {
        if (SchemeRegistry::shouldTreatURLSchemeAsLegacy(url.protocol()) && !SchemeRegistry::shouldTreatURLSchemeAsLegacy(frame()->document()->securityOrigin()->protocol()))
            UseCounter::count(frame()->document(), UseCounter::LegacyProtocolEmbeddedAsSubresource);
        if (!url.user().isEmpty() || !url.pass().isEmpty())
            UseCounter::count(frame()->document(), UseCounter::RequestedSubresourceWithEmbeddedCredentials);
    }

    // Last of all, check for mixed content. We do this last so that when
    // folks block mixed content with a CSP policy, they don't get a warning.
    // They'll still get a warning in the console about CSP blocking the load.

    // If we're loading the main resource of a subframe, ensure that we check
    // against the parent of the active frame, rather than the frame itself.
    LocalFrame* effectiveFrame = frame();
    if (resourceRequest.frameType() == WebURLRequest::FrameTypeNested) {
        // FIXME: Deal with RemoteFrames.
        if (frame()->tree().parent()->isLocalFrame())
            effectiveFrame = toLocalFrame(frame()->tree().parent());
    }

    MixedContentChecker::ReportingStatus mixedContentReporting = forPreload ?
        MixedContentChecker::SuppressReport : MixedContentChecker::SendReport;
    return !MixedContentChecker::shouldBlockFetch(effectiveFrame, resourceRequest, url, mixedContentReporting);
}

bool ResourceFetcher::canAccessResource(Resource* resource, SecurityOrigin* sourceOrigin, const KURL& url) const
{
    // Redirects can change the response URL different from one of request.
    if (!canRequest(resource->type(), resource->resourceRequest(), url, resource->options(), resource->isUnusedPreload(), FetchRequest::UseDefaultOriginRestrictionForType))
        return false;

    if (!sourceOrigin && document())
        sourceOrigin = document()->securityOrigin();

    if (sourceOrigin->canRequest(url))
        return true;

    String errorDescription;
    if (!resource->passesAccessControlCheck(document(), sourceOrigin, errorDescription)) {
        if (resource->type() == Resource::Font)
            toFontResource(resource)->setCORSFailed();
        if (frame() && frame()->document()) {
            String resourceType = Resource::resourceTypeToString(resource->type(), resource->options().initiatorInfo);
            frame()->document()->addConsoleMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, resourceType + " from origin '" + SecurityOrigin::create(url)->toString() + "' has been blocked from loading by Cross-Origin Resource Sharing policy: " + errorDescription));
        }
        return false;
    }
    return true;
}

bool ResourceFetcher::isControlledByServiceWorker() const
{
    LocalFrame* localFrame = frame();
    if (!localFrame)
        return false;
    ASSERT(documentLoader() || localFrame->loader().documentLoader());
    if (documentLoader())
        return localFrame->loader().client()->isControlledByServiceWorker(*documentLoader());
    // documentLoader() is null while loading resources from the imported HTML.
    // In such cases whether the request is controlled by ServiceWorker or not
    // is determined by the document loader of the frame.
    return localFrame->loader().client()->isControlledByServiceWorker(*localFrame->loader().documentLoader());
}

int64_t ResourceFetcher::serviceWorkerID() const
{
    LocalFrame* localFrame = frame();
    if (!localFrame)
        return -1;
    ASSERT(documentLoader() || localFrame->loader().documentLoader());
    if (documentLoader())
        return localFrame->loader().client()->serviceWorkerID(*documentLoader());
    // documentLoader() is null while loading resources from the imported HTML.
    // In such cases a service worker ID could be retrieved from the document
    // loader of the frame.
    return localFrame->loader().client()->serviceWorkerID(*localFrame->loader().documentLoader());
}

bool ResourceFetcher::shouldLoadNewResource(Resource::Type type) const
{
    if (!frame())
        return false;
    if (!documentLoader())
        return true;
    if (type == Resource::MainResource)
        return documentLoader() == frame()->loader().provisionalDocumentLoader();
    return documentLoader() == frame()->loader().documentLoader();
}

bool ResourceFetcher::resourceNeedsLoad(Resource* resource, const FetchRequest& request, RevalidationPolicy policy)
{
    if (FetchRequest::DeferredByClient == request.defer())
        return false;
    if (policy != Use)
        return true;
    if (resource->stillNeedsLoad())
        return true;
    return request.options().synchronousPolicy == RequestSynchronously && resource->isLoading();
}

// Limit the number of URLs in m_validatedURLs to avoid memory bloat.
// http://crbug.com/52411
static const int kMaxValidatedURLsSize = 10000;

void ResourceFetcher::requestLoadStarted(Resource* resource, const FetchRequest& request, ResourceLoadStartType type)
{
    if (type == ResourceLoadingFromCache)
        notifyLoadedFromMemoryCache(resource);

    if (request.resourceRequest().url().protocolIsData() || (documentLoader() && documentLoader()->substituteData().isValid()))
        return;

    if (type == ResourceLoadingFromCache && !resource->stillNeedsLoad() && !m_validatedURLs.contains(request.resourceRequest().url())) {
        // Resources loaded from memory cache should be reported the first time they're used.
        RefPtr<ResourceTimingInfo> info = ResourceTimingInfo::create(request.options().initiatorInfo.name, monotonicallyIncreasingTime());
        populateResourceTiming(info.get(), resource, true);
        m_scheduledResourceTimingReports.add(info, resource->type() == Resource::MainResource);
        if (!m_resourceTimingReportTimer.isActive())
            m_resourceTimingReportTimer.startOneShot(0, FROM_HERE);
    }

    if (m_validatedURLs.size() >= kMaxValidatedURLsSize) {
        m_validatedURLs.clear();
    }
    m_validatedURLs.add(request.resourceRequest().url());
}

ResourcePtr<Resource> ResourceFetcher::requestResource(Resource::Type type, FetchRequest& request)
{
    ASSERT(request.options().synchronousPolicy == RequestAsynchronously || type == Resource::Raw);

    TRACE_EVENT0("blink", "ResourceFetcher::requestResource");

    upgradeInsecureRequest(request);
    addClientHintsIfNeccessary(request);

    KURL url = request.resourceRequest().url();

    WTF_LOG(ResourceLoading, "ResourceFetcher::requestResource '%s', charset '%s', priority=%d, forPreload=%u, type=%s", url.elidedString().latin1().data(), request.charset().latin1().data(), request.priority(), request.forPreload(), ResourceTypeName(type));

    // If only the fragment identifiers differ, it is the same resource.
    url = MemoryCache::removeFragmentIdentifierIfNeeded(url);

    if (!url.isValid())
        return nullptr;

    if (!canRequest(type, request.resourceRequest(), url, request.options(), request.forPreload(), request.originRestriction()))
        return nullptr;

    if (LocalFrame* f = frame())
        f->loader().client()->dispatchWillRequestResource(&request);

    if (!request.forPreload()) {
        V8DOMActivityLogger* activityLogger = nullptr;
        if (request.options().initiatorInfo.name == FetchInitiatorTypeNames::xmlhttprequest)
            activityLogger = V8DOMActivityLogger::currentActivityLogger();
        else
            activityLogger = V8DOMActivityLogger::currentActivityLoggerIfIsolatedWorld();

        if (activityLogger) {
            Vector<String> argv;
            argv.append(Resource::resourceTypeToString(type, request.options().initiatorInfo));
            argv.append(url);
            activityLogger->logEvent("blinkRequestResource", argv.size(), argv.data());
        }
    }

    // See if we can use an existing resource from the cache.
    ResourcePtr<Resource> resource = memoryCache()->resourceForURL(url, getCacheIdentifier());

    const RevalidationPolicy policy = determineRevalidationPolicy(type, request, resource.get());
    switch (policy) {
    case Reload:
        memoryCache()->remove(resource.get());
        // Fall through
    case Load:
        resource = createResourceForLoading(type, request, request.charset());
        break;
    case Revalidate:
        resource = createResourceForRevalidation(request, resource.get());
        break;
    case Use:
        memoryCache()->updateForAccess(resource.get());
        break;
    }

    if (!resource)
        return nullptr;

    if (!resource->hasClients())
        m_deadStatsRecorder.update(policy);

    if (policy != Use)
        resource->setIdentifier(createUniqueIdentifier());

    if (!request.forPreload() || policy != Use) {
        ResourceLoadPriority priority = loadPriority(type, request);
        // When issuing another request for a resource that is already in-flight make
        // sure to not demote the priority of the in-flight request. If the new request
        // isn't at the same priority as the in-flight request, only allow promotions.
        // This can happen when a visible image's priority is increased and then another
        // reference to the image is parsed (which would be at a lower priority).
        if (priority > resource->resourceRequest().priority()) {
            resource->mutableResourceRequest().setPriority(priority);
            resource->didChangePriority(priority, 0);
        }
    }

    if (resourceNeedsLoad(resource.get(), request, policy)) {
        if (!shouldLoadNewResource(type)) {
            if (memoryCache()->contains(resource.get()))
                memoryCache()->remove(resource.get());
            return nullptr;
        }

        if (!scheduleArchiveLoad(resource.get(), request.resourceRequest()))
            resource->load(this, request.options());

        // For asynchronous loads that immediately fail, it's sufficient to return a
        // null Resource, as it indicates that something prevented the load from starting.
        // If there's a network error, that failure will happen asynchronously. However, if
        // a sync load receives a network error, it will have already happened by this point.
        // In that case, the requester should have access to the relevant ResourceError, so
        // we need to return a non-null Resource.
        if (resource->errorOccurred()) {
            if (memoryCache()->contains(resource.get()))
                memoryCache()->remove(resource.get());
            return request.options().synchronousPolicy == RequestSynchronously ? resource : nullptr;
        }
    }

    // FIXME: Temporarily leave main resource caching disabled for chromium,
    // see https://bugs.webkit.org/show_bug.cgi?id=107962. Before caching main
    // resources, we should be sure to understand the implications for memory
    // use.
    // Remove main resource from cache to prevent reuse.
    if (type == Resource::MainResource) {
        ASSERT(policy != Use || documentLoader()->substituteData().isValid());
        ASSERT(policy != Revalidate);
        memoryCache()->remove(resource.get());
    }

    requestLoadStarted(resource.get(), request, policy == Use ? ResourceLoadingFromCache : ResourceLoadingFromNetwork);

    ASSERT(resource->url() == url.string());
    m_documentResources.set(resource->url(), resource);
    return resource;
}

void ResourceFetcher::resourceTimingReportTimerFired(Timer<ResourceFetcher>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_resourceTimingReportTimer);
    HashMap<RefPtr<ResourceTimingInfo>, bool> timingReports;
    timingReports.swap(m_scheduledResourceTimingReports);
    for (const auto& timingInfo : timingReports)
        reportResourceTiming(timingInfo.key.get(), document(), timingInfo.value);
}

void ResourceFetcher::determineRequestContext(ResourceRequest& request, Resource::Type type)
{
    WebURLRequest::RequestContext requestContext = requestContextFromType(this, type);
    request.setRequestContext(requestContext);
}

ResourceRequestCachePolicy ResourceFetcher::resourceRequestCachePolicy(const ResourceRequest& request, Resource::Type type)
{
    if (type == Resource::MainResource) {
        FrameLoadType frameLoadType = frame()->loader().loadType();
        if (request.httpMethod() == "POST" && frameLoadType == FrameLoadTypeBackForward)
            return ReturnCacheDataDontLoad;
        if (!frame()->host()->overrideEncoding().isEmpty() || frameLoadType == FrameLoadTypeBackForward)
            return ReturnCacheDataElseLoad;
        if (frameLoadType == FrameLoadTypeReloadFromOrigin)
            return ReloadBypassingCache;
        if (frameLoadType == FrameLoadTypeReload || frameLoadType == FrameLoadTypeSame || request.isConditional() || request.httpMethod() == "POST")
            return ReloadIgnoringCacheData;
        Frame* parent = frame()->tree().parent();
        if (parent && parent->isLocalFrame())
            return toLocalFrame(parent)->document()->fetcher()->resourceRequestCachePolicy(request, type);
        return UseProtocolCachePolicy;
    }

    if (request.isConditional())
        return ReloadIgnoringCacheData;

    if (documentLoader() && document() && !document()->loadEventFinished()) {
        // For POST requests, we mutate the main resource's cache policy to avoid form resubmission.
        // This policy should not be inherited by subresources.
        ResourceRequestCachePolicy mainResourceCachePolicy = documentLoader()->request().cachePolicy();
        if (documentLoader()->request().httpMethod() == "POST") {
            if (mainResourceCachePolicy == ReturnCacheDataDontLoad)
                return ReturnCacheDataElseLoad;
            return UseProtocolCachePolicy;
        }
        return memoryCachePolicyToResourceRequestCachePolicy(context().cachePolicy(document()));
    }
    return UseProtocolCachePolicy;
}

void ResourceFetcher::addAdditionalRequestHeaders(ResourceRequest& request, Resource::Type type)
{
    if (!frame())
        return;

    if (request.cachePolicy() == UseProtocolCachePolicy)
        request.setCachePolicy(resourceRequestCachePolicy(request, type));
    if (request.requestContext() == WebURLRequest::RequestContextUnspecified)
        determineRequestContext(request, type);
    if (type == Resource::LinkPrefetch || type == Resource::LinkSubresource)
        request.setHTTPHeaderField("Purpose", "prefetch");
    if (frame()->document())
        request.setOriginatesFromReservedIPRange(frame()->document()->isHostedInReservedIPRange());

    context().addAdditionalRequestHeaders(document(), request, (type == Resource::MainResource) ? FetchMainResource : FetchSubresource);
}

void ResourceFetcher::upgradeInsecureRequest(FetchRequest& fetchRequest)
{
    if (!document() || !RuntimeEnabledFeatures::experimentalContentSecurityPolicyFeaturesEnabled())
        return;

    KURL url = fetchRequest.resourceRequest().url();

    // Tack a 'Prefer' header to outgoing navigational requests, as described in
    // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
    if (fetchRequest.resourceRequest().frameType() != WebURLRequest::FrameTypeNone && !SecurityOrigin::isSecure(url))
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("Prefer", "return=secure-representation");

    if (document()->insecureRequestsPolicy() == SecurityContext::InsecureRequestsUpgrade && url.protocolIs("http")) {
        // We always upgrade subresource requests and nested frames, we always upgrade form
        // submissions, and we always upgrade requests whose host matches the host of the
        // containing document's security origin.
        //
        // FIXME: We need to check the document that set the policy, not the current document.
        const ResourceRequest& request = fetchRequest.resourceRequest();
        if (request.frameType() == WebURLRequest::FrameTypeNone
            || request.frameType() == WebURLRequest::FrameTypeNested
            || request.requestContext() == WebURLRequest::RequestContextForm
            || url.host() == document()->securityOrigin()->host())
        {
            url.setProtocol("https");
            if (url.port() == 80)
                url.setPort(443);
            fetchRequest.mutableResourceRequest().setURL(url);
        }
    }
}

void ResourceFetcher::addClientHintsIfNeccessary(FetchRequest& fetchRequest)
{
    if (!RuntimeEnabledFeatures::clientHintsEnabled() || !document() || !frame())
        return;

    if (frame()->shouldSendDPRHint())
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("DPR", AtomicString(String::number(document()->devicePixelRatio())));

    // FIXME: Send the RW hint based on the actual resource width, when we have it.
    if (frame()->shouldSendRWHint() && frame()->view())
        fetchRequest.mutableResourceRequest().addHTTPHeaderField("RW", AtomicString(String::number(frame()->view()->viewportWidth())));
}

ResourcePtr<Resource> ResourceFetcher::createResourceForRevalidation(const FetchRequest& request, Resource* resource)
{
    ASSERT(resource);
    ASSERT(memoryCache()->contains(resource));
    ASSERT(resource->isLoaded());
    ASSERT(resource->canUseCacheValidator());
    ASSERT(!resource->resourceToRevalidate());
    ASSERT(!isControlledByServiceWorker());

    ResourceRequest revalidatingRequest(resource->resourceRequest());
    revalidatingRequest.clearHTTPReferrer();
    addAdditionalRequestHeaders(revalidatingRequest, resource->type());

    const AtomicString& lastModified = resource->response().httpHeaderField("Last-Modified");
    const AtomicString& eTag = resource->response().httpHeaderField("ETag");
    if (!lastModified.isEmpty() || !eTag.isEmpty()) {
        ASSERT(context().cachePolicy(document()) != CachePolicyReload);
        if (context().cachePolicy(document()) == CachePolicyRevalidate)
            revalidatingRequest.setHTTPHeaderField("Cache-Control", "max-age=0");
    }
    if (!lastModified.isEmpty())
        revalidatingRequest.setHTTPHeaderField("If-Modified-Since", lastModified);
    if (!eTag.isEmpty())
        revalidatingRequest.setHTTPHeaderField("If-None-Match", eTag);

    ResourcePtr<Resource> newResource = createResource(resource->type(), revalidatingRequest, resource->encoding());
    WTF_LOG(ResourceLoading, "Resource %p created to revalidate %p", newResource.get(), resource);

    newResource->setResourceToRevalidate(resource);
    newResource->setCacheIdentifier(resource->cacheIdentifier());

    memoryCache()->remove(resource);
    memoryCache()->add(newResource.get());
    return newResource;
}

ResourcePtr<Resource> ResourceFetcher::createResourceForLoading(Resource::Type type, FetchRequest& request, const String& charset)
{
    const String cacheIdentifier = getCacheIdentifier();
    ASSERT(!memoryCache()->resourceForURL(request.resourceRequest().url(), cacheIdentifier));

    WTF_LOG(ResourceLoading, "Loading Resource for '%s'.", request.resourceRequest().url().elidedString().latin1().data());

    addAdditionalRequestHeaders(request.mutableResourceRequest(), type);
    ResourcePtr<Resource> resource = createResource(type, request.resourceRequest(), charset);
    resource->setCacheIdentifier(cacheIdentifier);

    memoryCache()->add(resource.get());
    return resource;
}

void ResourceFetcher::storeResourceTimingInitiatorInformation(Resource* resource)
{
    if (resource->options().requestInitiatorContext != DocumentContext)
        return;

    RefPtr<ResourceTimingInfo> info = ResourceTimingInfo::create(resource->options().initiatorInfo.name, monotonicallyIncreasingTime());

    if (resource->isCacheValidator()) {
        const AtomicString& timingAllowOrigin = resource->resourceToRevalidate()->response().httpHeaderField("Timing-Allow-Origin");
        if (!timingAllowOrigin.isEmpty())
            info->setOriginalTimingAllowOrigin(timingAllowOrigin);
    }

    if (resource->type() == Resource::MainResource) {
        // <iframe>s should report the initial navigation requested by the parent document, but not subsequent navigations.
        // FIXME: Resource timing is broken when the parent is a remote frame.
        if (frame()->deprecatedLocalOwner() && !frame()->deprecatedLocalOwner()->loadedNonEmptyDocument()) {
            info->setInitiatorType(frame()->deprecatedLocalOwner()->localName());
            m_resourceTimingInfoMap.add(resource, info);
            frame()->deprecatedLocalOwner()->didLoadNonEmptyDocument();
        }
    } else {
        m_resourceTimingInfoMap.add(resource, info);
    }
}

ResourceFetcher::RevalidationPolicy ResourceFetcher::determineRevalidationPolicy(Resource::Type type, const FetchRequest& fetchRequest, Resource* existingResource) const
{
    const ResourceRequest& request = fetchRequest.resourceRequest();

    if (!existingResource)
        return Load;

    // We already have a preload going for this URL.
    if (fetchRequest.forPreload() && existingResource->isPreloaded())
        return Use;

    // If the same URL has been loaded as a different type, we need to reload.
    if (existingResource->type() != type) {
        // FIXME: If existingResource is a Preload and the new type is LinkPrefetch
        // We really should discard the new prefetch since the preload has more
        // specific type information! crbug.com/379893
        // fast/dom/HTMLLinkElement/link-and-subresource-test hits this case.
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to type mismatch.");
        return Reload;
    }

    // Do not load from cache if images are not enabled. The load for this image will be blocked
    // in ImageResource::load.
    if (FetchRequest::DeferredByClient == fetchRequest.defer())
        return Reload;

    // Always use data uris.
    // FIXME: Extend this to non-images.
    if (type == Resource::Image && request.url().protocolIsData())
        return Use;

    // If a main resource was populated from a SubstituteData load, use it.
    if (type == Resource::MainResource && documentLoader()->substituteData().isValid())
        return Use;

    if (!existingResource->canReuse(request))
        return Reload;

    // Never use cache entries for downloadToFile requests. The caller expects the resource in a file.
    if (request.downloadToFile())
        return Reload;

    // Certain requests (e.g., XHRs) might have manually set headers that require revalidation.
    // FIXME: In theory, this should be a Revalidate case. In practice, the MemoryCache revalidation path assumes a whole bunch
    // of things about how revalidation works that manual headers violate, so punt to Reload instead.
    if (request.isConditional())
        return Reload;

    // Don't reload resources while pasting.
    if (m_allowStaleResources)
        return Use;

    if (!fetchRequest.options().canReuseRequest(existingResource->options()))
        return Reload;

    // Always use preloads.
    if (existingResource->isPreloaded())
        return Use;

    // CachePolicyHistoryBuffer uses the cache no matter what.
    CachePolicy cachePolicy = context().cachePolicy(document());
    if (cachePolicy == CachePolicyHistoryBuffer)
        return Use;

    // Don't reuse resources with Cache-control: no-store.
    if (existingResource->hasCacheControlNoStoreHeader()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to Cache-control: no-store.");
        return Reload;
    }

    // If credentials were sent with the previous request and won't be
    // with this one, or vice versa, re-fetch the resource.
    //
    // This helps with the case where the server sends back
    // "Access-Control-Allow-Origin: *" all the time, but some of the
    // client's requests are made without CORS and some with.
    if (existingResource->resourceRequest().allowStoredCredentials() != request.allowStoredCredentials()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to difference in credentials settings.");
        return Reload;
    }

    // During the initial load, avoid loading the same resource multiple times for a single document,
    // even if the cache policies would tell us to.
    // We also group loads of the same resource together.
    // Raw resources are exempted, as XHRs fall into this category and may have user-set Cache-Control:
    // headers or other factors that require separate requests.
    if (type != Resource::Raw) {
        if (document() && !document()->loadEventFinished() && m_validatedURLs.contains(existingResource->url()))
            return Use;
        if (existingResource->isLoading())
            return Use;
    }

    // CachePolicyReload always reloads
    if (cachePolicy == CachePolicyReload) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to CachePolicyReload.");
        return Reload;
    }

    // We'll try to reload the resource if it failed last time.
    if (existingResource->errorOccurred()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicye reloading due to resource being in the error state");
        return Reload;
    }

    // List of available images logic allows images to be re-used without cache validation. We restrict this only to images
    // from memory cache which are the same as the version in the current document.
    if (type == Resource::Image && existingResource == cachedResource(request.url()))
        return Use;

    // If any of the redirects in the chain to loading the resource were not cacheable, we cannot reuse our cached resource.
    if (!existingResource->canReuseRedirectChain()) {
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to an uncacheable redirect");
        return Reload;
    }

    // Check if the cache headers requires us to revalidate (cache expiration for example).
    if (cachePolicy == CachePolicyRevalidate || existingResource->mustRevalidateDueToCacheHeaders()
        || request.cacheControlContainsNoCache()) {
        // See if the resource has usable ETag or Last-modified headers.
        // If the page is controlled by the ServiceWorker, we choose the Reload policy because the revalidation headers should not be exposed to the ServiceWorker.(crbug.com/429570)
        if (existingResource->canUseCacheValidator() && !isControlledByServiceWorker())
            return Revalidate;

        // No, must reload.
        WTF_LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to missing cache validators.");
        return Reload;
    }

    return Use;
}

void ResourceFetcher::printAccessDeniedMessage(const KURL& url) const
{
    if (url.isNull())
        return;

    if (!frame())
        return;

    String message;
    if (!document() || document()->url().isNull())
        message = "Unsafe attempt to load URL " + url.elidedString() + '.';
    else
        message = "Unsafe attempt to load URL " + url.elidedString() + " from frame with URL " + document()->url().elidedString() + ". Domains, protocols and ports must match.\n";

    frame()->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, message));
}

void ResourceFetcher::setAutoLoadImages(bool enable)
{
    if (enable == m_autoLoadImages)
        return;

    m_autoLoadImages = enable;

    if (!m_autoLoadImages)
        return;

    reloadImagesIfNotDeferred();
}

void ResourceFetcher::setImagesEnabled(bool enable)
{
    if (enable == m_imagesEnabled)
        return;

    m_imagesEnabled = enable;

    if (!m_imagesEnabled)
        return;

    reloadImagesIfNotDeferred();
}

bool ResourceFetcher::clientDefersImage(const KURL& url) const
{
    return frame() && !frame()->loader().client()->allowImage(m_imagesEnabled, url);
}

bool ResourceFetcher::shouldDeferImageLoad(const KURL& url) const
{
    return clientDefersImage(url) || !m_autoLoadImages;
}

void ResourceFetcher::reloadImagesIfNotDeferred()
{
    for (const auto& documentResource : m_documentResources) {
        Resource* resource = documentResource.value.get();
        if (resource->type() == Resource::Image && resource->stillNeedsLoad() && !clientDefersImage(resource->url()))
            const_cast<Resource*>(resource)->load(this, defaultResourceOptions());
    }
}

void ResourceFetcher::redirectReceived(Resource* resource, const ResourceResponse& redirectResponse)
{
    ResourceTimingInfoMap::iterator it = m_resourceTimingInfoMap.find(resource);
    if (it != m_resourceTimingInfoMap.end())
        it->value->addRedirect(redirectResponse);
}

void ResourceFetcher::didLoadResource()
{
    scheduleDocumentResourcesGC();
    if (frame())
        frame()->loader().loadDone();
}

void ResourceFetcher::scheduleDocumentResourcesGC()
{
    if (!m_garbageCollectDocumentResourcesTimer.isActive())
        m_garbageCollectDocumentResourcesTimer.startOneShot(0, FROM_HERE);
}

// Garbage collecting m_documentResources is a workaround for the
// ResourcePtrs on the RHS being strong references. Ideally this
// would be a weak map, however ResourcePtrs perform additional
// bookkeeping on Resources, so instead pseudo-GC them -- when the
// reference count reaches 1, m_documentResources is the only reference, so
// remove it from the map.
void ResourceFetcher::garbageCollectDocumentResourcesTimerFired(Timer<ResourceFetcher>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_garbageCollectDocumentResourcesTimer);
    garbageCollectDocumentResources();
}

void ResourceFetcher::garbageCollectDocumentResources()
{
    typedef Vector<String, 10> StringVector;
    StringVector resourcesToDelete;

    for (const auto& documentResource : m_documentResources) {
        if (documentResource.value->hasOneHandle())
            resourcesToDelete.append(documentResource.key);
    }

    m_documentResources.removeAll(resourcesToDelete);
}

void ResourceFetcher::notifyLoadedFromMemoryCache(Resource* resource)
{
    if (!frame() || !frame()->page() || resource->status() != Resource::Cached || m_validatedURLs.contains(resource->url()))
        return;

    ResourceRequest request(resource->url());
    unsigned long identifier = createUniqueIdentifier();
    context().dispatchDidLoadResourceFromMemoryCache(request, resource->response());
    // FIXME: If willSendRequest changes the request, we don't respect it.
    willSendRequest(identifier, request, ResourceResponse(), resource->options().initiatorInfo);
    InspectorInstrumentation::markResourceAsCached(frame(), identifier);
    context().sendRemainingDelegateMessages(documentLoader(), identifier, resource->response(), resource->encodedSize());
}

int ResourceFetcher::requestCount() const
{
    return m_loaders ? m_loaders->size() : 0;
}

void ResourceFetcher::preload(Resource::Type type, FetchRequest& request, const String& charset)
{
    // Ensure main resources aren't preloaded, since the cache can't actually reuse the preload.
    if (type == Resource::MainResource)
        return;

    ASSERT(type == Resource::Script || type == Resource::CSSStyleSheet || type == Resource::Image);

    String encoding;
    if (type == Resource::Script || type == Resource::CSSStyleSheet) {
        encoding = charset.isEmpty() ? document()->charset().string() : charset;

        // RequestContext for Resource::Image is set in fetchImage below.
        determineRequestContext(request.mutableResourceRequest(), type);
    }

    request.setCharset(encoding);
    request.setForPreload(true);

    ResourcePtr<Resource> resource;
    // Loading images involves several special cases, so use dedicated fetch method instead.
    if (type == Resource::Image)
        resource = fetchImage(request);
    else
        resource = requestResource(type, request);
    if (!resource || (m_preloads && m_preloads->contains(resource.get())))
        return;
    TRACE_EVENT_ASYNC_STEP_INTO0("blink.net", "Resource", resource.get(), "Preload");
    resource->increasePreloadCount();

    if (!m_preloads)
        m_preloads = adoptPtr(new ListHashSet<Resource*>);
    m_preloads->add(resource.get());

#if PRELOAD_DEBUG
    printf("PRELOADING %s\n",  resource->url().string().latin1().data());
#endif
}

bool ResourceFetcher::isPreloaded(const String& urlString) const
{
    const KURL& url = document()->completeURL(urlString);

    if (m_preloads) {
        for (Resource* resource : *m_preloads) {
            if (resource->url() == url)
                return true;
        }
    }

    return false;
}

void ResourceFetcher::clearPreloads()
{
#if PRELOAD_DEBUG
    printPreloadStats();
#endif
    if (!m_preloads)
        return;

    for (Resource* resource : *m_preloads) {
        resource->decreasePreloadCount();
        bool deleted = resource->deleteIfPossible();
        if (!deleted && resource->preloadResult() == Resource::PreloadNotReferenced)
            memoryCache()->remove(resource);
    }
    m_preloads.clear();
}

void ResourceFetcher::addAllArchiveResources(MHTMLArchive* archive)
{
    ASSERT(archive);
    if (!m_archiveResourceCollection)
        m_archiveResourceCollection = ArchiveResourceCollection::create();
    m_archiveResourceCollection->addAllResources(archive);
}

bool ResourceFetcher::scheduleArchiveLoad(Resource* resource, const ResourceRequest& request)
{
    if (!m_archiveResourceCollection)
        return false;

    ArchiveResource* archiveResource = m_archiveResourceCollection->archiveResourceForURL(request.url());
    if (!archiveResource) {
        resource->error(Resource::LoadError);
        return true;
    }

    resource->setLoading(true);
    resource->responseReceived(archiveResource->response(), nullptr);
    SharedBuffer* data = archiveResource->data();
    if (data)
        resource->appendData(data->data(), data->size());
    resource->finish();
    return true;
}

void ResourceFetcher::didFinishLoading(Resource* resource, double finishTime, int64_t encodedDataLength)
{
    TRACE_EVENT_ASYNC_END0("blink.net", "Resource", resource);
    RefPtrWillBeRawPtr<Document> protectDocument(document());
    RefPtr<DocumentLoader> protectDocumentLoader(documentLoader());
    willTerminateResourceLoader(resource->loader());

    if (resource && resource->response().isHTTP() && resource->response().httpStatusCode() < 400 && document()) {
        ResourceTimingInfoMap::iterator it = m_resourceTimingInfoMap.find(resource);
        if (it != m_resourceTimingInfoMap.end()) {
            RefPtr<ResourceTimingInfo> info = it->value;
            m_resourceTimingInfoMap.remove(it);
            populateResourceTiming(info.get(), resource, false);
            reportResourceTiming(info.get(), document(), resource->type() == Resource::MainResource);
        }
    }
    context().dispatchDidFinishLoading(documentLoader(), resource->identifier(), finishTime, encodedDataLength);
}

void ResourceFetcher::didChangeLoadingPriority(const Resource* resource, ResourceLoadPriority loadPriority, int intraPriorityValue)
{
    TRACE_EVENT_ASYNC_STEP_INTO1("blink.net", "Resource", resource, "ChangePriority", "priority", loadPriority);
    context().dispatchDidChangeResourcePriority(resource->identifier(), loadPriority, intraPriorityValue);
}

void ResourceFetcher::didFailLoading(const Resource* resource, const ResourceError& error)
{
    TRACE_EVENT_ASYNC_END0("blink.net", "Resource", resource);
    willTerminateResourceLoader(resource->loader());
    bool isInternalRequest = resource->options().initiatorInfo.name == FetchInitiatorTypeNames::internal;
    context().dispatchDidFail(documentLoader(), resource->identifier(), error, isInternalRequest);
}

void ResourceFetcher::willSendRequest(unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    context().dispatchWillSendRequest(documentLoader(), identifier, request, redirectResponse, initiatorInfo);
}

void ResourceFetcher::didReceiveResponse(const Resource* resource, const ResourceResponse& response)
{
    MixedContentChecker::checkMixedPrivatePublic(frame(), response.remoteIPAddress());

    // If the response is fetched via ServiceWorker, the original URL of the response could be different from the URL of the request.
    // We check the URL not to load the resources which are forbidden by the page CSP. This behavior is not specified in the CSP specification yet.
    if (response.wasFetchedViaServiceWorker()) {
        const KURL& originalURL = response.originalURLViaServiceWorker();
        if (!canRequest(resource->type(), resource->resourceRequest(), originalURL, resource->options(), false, FetchRequest::UseDefaultOriginRestrictionForType)) {
            resource->loader()->cancel();
            bool isInternalRequest = resource->options().initiatorInfo.name == FetchInitiatorTypeNames::internal;
            context().dispatchDidFail(documentLoader(), resource->identifier(), ResourceError(errorDomainBlinkInternal, 0, originalURL.string(), "Unsafe attempt to load URL " + originalURL.elidedString() + " fetched by a ServiceWorker."), isInternalRequest);
            return;
        }
    }
    context().dispatchDidReceiveResponse(documentLoader(), resource->identifier(), response, resource->loader());
}

void ResourceFetcher::didReceiveData(const Resource* resource, const char* data, int dataLength, int encodedDataLength)
{
    context().dispatchDidReceiveData(documentLoader(), resource->identifier(), data, dataLength, encodedDataLength);
}

void ResourceFetcher::didDownloadData(const Resource* resource, int dataLength, int encodedDataLength)
{
    context().dispatchDidDownloadData(documentLoader(), resource->identifier(), dataLength, encodedDataLength);
}

void ResourceFetcher::acceptDataFromThreadedReceiver(unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    context().dispatchDidReceiveData(documentLoader(), identifier, data, dataLength, encodedDataLength);
}

void ResourceFetcher::subresourceLoaderFinishedLoadingOnePart(ResourceLoader* loader)
{
    if (!m_nonBlockingLoaders)
        m_nonBlockingLoaders = ResourceLoaderSet::create();
    m_nonBlockingLoaders->add(loader);
    m_loaders->remove(loader);
    if (LocalFrame* frame = this->frame())
        frame->loader().loadDone();
}

void ResourceFetcher::didInitializeResourceLoader(ResourceLoader* loader)
{
    if (loader->cachedResource()->shouldBlockLoadEvent()) {
        if (!m_loaders)
            m_loaders = ResourceLoaderSet::create();
        m_loaders->add(loader);
    } else {
        if (!m_nonBlockingLoaders)
            m_nonBlockingLoaders = ResourceLoaderSet::create();
        m_nonBlockingLoaders->add(loader);
    }
}

void ResourceFetcher::willTerminateResourceLoader(ResourceLoader* loader)
{
    if (m_loaders && m_loaders->contains(loader))
        m_loaders->remove(loader);
    else if (m_nonBlockingLoaders && m_nonBlockingLoaders->contains(loader))
        m_nonBlockingLoaders->remove(loader);
    else
        ASSERT_NOT_REACHED();
}

void ResourceFetcher::willStartLoadingResource(Resource* resource, ResourceRequest& request)
{
    if (documentLoader())
        documentLoader()->applicationCacheHost()->willStartLoadingResource(request);

    storeResourceTimingInitiatorInformation(resource);
    TRACE_EVENT_ASYNC_BEGIN2("blink.net", "Resource", resource, "url", resource->url().string().ascii(), "priority", resource->resourceRequest().priority());
}

void ResourceFetcher::stopFetching()
{
    if (m_nonBlockingLoaders)
        m_nonBlockingLoaders->cancelAll();
    if (m_loaders)
        m_loaders->cancelAll();
}

bool ResourceFetcher::isFetching() const
{
    return m_loaders && !m_loaders->isEmpty();
}

void ResourceFetcher::setDefersLoading(bool defers)
{
    if (m_loaders)
        m_loaders->setAllDefersLoading(defers);
    if (m_nonBlockingLoaders)
        m_nonBlockingLoaders->setAllDefersLoading(defers);
}

bool ResourceFetcher::defersLoading() const
{
    if (LocalFrame* frame = this->frame())
        return frame->page()->defersLoading();
    return false;
}

bool ResourceFetcher::isLoadedBy(ResourceLoaderHost* possibleOwner) const
{
    return this == possibleOwner;
}

bool ResourceFetcher::canAccessRedirect(Resource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse, ResourceLoaderOptions& options)
{
    if (!canRequest(resource->type(), request, request.url(), options, resource->isUnusedPreload(), FetchRequest::UseDefaultOriginRestrictionForType))
        return false;
    if (options.corsEnabled == IsCORSEnabled) {
        SecurityOrigin* sourceOrigin = options.securityOrigin.get();
        if (!sourceOrigin && document())
            sourceOrigin = document()->securityOrigin();

        String errorMessage;
        if (!CrossOriginAccessControl::handleRedirect(document(), resource, sourceOrigin, request, redirectResponse, options, errorMessage)) {
            if (resource->type() == Resource::Font)
                toFontResource(resource)->setCORSFailed();
            if (frame() && frame()->document())
                frame()->document()->addConsoleMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, errorMessage));
            return false;
        }
    }
    if (resource->type() == Resource::Image && shouldDeferImageLoad(request.url()))
        return false;
    return true;
}

#if !ENABLE(OILPAN)
void ResourceFetcher::refResourceLoaderHost()
{
    ref();
}

void ResourceFetcher::derefResourceLoaderHost()
{
    deref();
}
#endif

#if PRELOAD_DEBUG
void ResourceFetcher::printPreloadStats()
{
    if (!m_preloads)
        return;

    unsigned scripts = 0;
    unsigned scriptMisses = 0;
    unsigned stylesheets = 0;
    unsigned stylesheetMisses = 0;
    unsigned images = 0;
    unsigned imageMisses = 0;
    for (Resource* resource : *m_preloads) {
        if (resource->preloadResult() == Resource::PreloadNotReferenced)
            printf("!! UNREFERENCED PRELOAD %s\n", resource->url().string().latin1().data());
        else if (resource->preloadResult() == Resource::PreloadReferencedWhileComplete)
            printf("HIT COMPLETE PRELOAD %s\n", resource->url().string().latin1().data());
        else if (resource->preloadResult() == Resource::PreloadReferencedWhileLoading)
            printf("HIT LOADING PRELOAD %s\n", resource->url().string().latin1().data());

        if (resource->type() == Resource::Script) {
            scripts++;
            if (resource->preloadResult() < Resource::PreloadReferencedWhileLoading)
                scriptMisses++;
        } else if (resource->type() == Resource::CSSStyleSheet) {
            stylesheets++;
            if (resource->preloadResult() < Resource::PreloadReferencedWhileLoading)
                stylesheetMisses++;
        } else {
            images++;
            if (resource->preloadResult() < Resource::PreloadReferencedWhileLoading)
                imageMisses++;
        }

        if (resource->errorOccurred())
            memoryCache()->remove(resource);

        resource->decreasePreloadCount();
    }
    m_preloads.clear();

    if (scripts)
        printf("SCRIPTS: %d (%d hits, hit rate %d%%)\n", scripts, scripts - scriptMisses, (scripts - scriptMisses) * 100 / scripts);
    if (stylesheets)
        printf("STYLESHEETS: %d (%d hits, hit rate %d%%)\n", stylesheets, stylesheets - stylesheetMisses, (stylesheets - stylesheetMisses) * 100 / stylesheets);
    if (images)
        printf("IMAGES:  %d (%d hits, hit rate %d%%)\n", images, images - imageMisses, (images - imageMisses) * 100 / images);
}
#endif

const ResourceLoaderOptions& ResourceFetcher::defaultResourceOptions()
{
    DEFINE_STATIC_LOCAL(ResourceLoaderOptions, options, (BufferData, AllowStoredCredentials, ClientRequestedCredentials, CheckContentSecurityPolicy, DocumentContext));
    return options;
}

String ResourceFetcher::getCacheIdentifier() const
{
    if (isControlledByServiceWorker())
        return String::number(serviceWorkerID());
    return MemoryCache::defaultCacheIdentifier();
}

ResourceFetcher::DeadResourceStatsRecorder::DeadResourceStatsRecorder()
    : m_useCount(0)
    , m_revalidateCount(0)
    , m_loadCount(0)
{
}

ResourceFetcher::DeadResourceStatsRecorder::~DeadResourceStatsRecorder()
{
    blink::Platform::current()->histogramCustomCounts(
        "WebCore.ResourceFetcher.HitCount", m_useCount, 0, 1000, 50);
    blink::Platform::current()->histogramCustomCounts(
        "WebCore.ResourceFetcher.RevalidateCount", m_revalidateCount, 0, 1000, 50);
    blink::Platform::current()->histogramCustomCounts(
        "WebCore.ResourceFetcher.LoadCount", m_loadCount, 0, 1000, 50);
}

void ResourceFetcher::DeadResourceStatsRecorder::update(RevalidationPolicy policy)
{
    switch (policy) {
    case Reload:
    case Load:
        ++m_loadCount;
        return;
    case Revalidate:
        ++m_revalidateCount;
        return;
    case Use:
        ++m_useCount;
        return;
    }
}

DEFINE_TRACE(ResourceFetcher)
{
    visitor->trace(m_context);
    visitor->trace(m_archiveResourceCollection);
    visitor->trace(m_loaders);
    visitor->trace(m_nonBlockingLoaders);
    ResourceLoaderHost::trace(visitor);
}

ResourceFetcher* ResourceFetcher::toResourceFetcher(ResourceLoaderHost* host)
{
    ASSERT(host->objectType() == ResourceLoaderHost::ResourceFetcherType);
    return static_cast<ResourceFetcher*>(host);
}

}
