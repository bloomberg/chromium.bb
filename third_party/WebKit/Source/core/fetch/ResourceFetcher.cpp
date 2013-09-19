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

#include "bindings/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/DocumentResource.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/ResourceLoaderSet.h"
#include "core/fetch/ScriptResource.h"
#include "core/fetch/ShaderResource.h"
#include "core/fetch/TextTrackResource.h"
#include "core/fetch/XSLStyleSheetResource.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLImport.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/PingLoader.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/loader/appcache/ApplicationCacheHost.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/DOMWindow.h"
#include "core/page/Frame.h"
#include "core/page/Performance.h"
#include "core/page/ResourceTimingInfo.h"
#include "core/page/Settings.h"
#include "core/platform/HistogramSupport.h"
#include "core/platform/Logging.h"
#include "core/platform/chromium/TraceEvent.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "weborigin/SecurityOrigin.h"
#include "weborigin/SecurityPolicy.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

#define PRELOAD_DEBUG 0

namespace WebCore {

namespace {

enum ActionUponResourceRequest {
    LoadResource,
    RevalidateResource,
    UseResourceFromCache,
    NumberOfResourceRequestActions,
};

Resource* createResource(Resource::Type type, const ResourceRequest& request, const String& charset)
{
    switch (type) {
    case Resource::Image:
        return new ImageResource(request);
    case Resource::CSSStyleSheet:
        return new CSSStyleSheetResource(request, charset);
    case Resource::Script:
        return new ScriptResource(request, charset);
    case Resource::SVGDocument:
        return new DocumentResource(request, Resource::SVGDocument);
    case Resource::Font:
        return new FontResource(request);
    case Resource::Raw:
    case Resource::MainResource:
        return new RawResource(request, type);
    case Resource::XSLStyleSheet:
        return new XSLStyleSheetResource(request);
    case Resource::LinkPrefetch:
        return new Resource(request, Resource::LinkPrefetch);
    case Resource::LinkSubresource:
        return new Resource(request, Resource::LinkSubresource);
    case Resource::TextTrack:
        return new TextTrackResource(request);
    case Resource::Shader:
        return new ShaderResource(request);
    case Resource::ImportResource:
        return new RawResource(request, type);
    case Resource::NumberOfTypes:
        ASSERT_NOT_REACHED();
    }

    ASSERT_NOT_REACHED();
    return 0;
}

ResourceLoadPriority loadPriority(Resource::Type type, const FetchRequest& request)
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
    case Resource::Font:
    case Resource::ImportResource:
        return ResourceLoadPriorityMedium;
    case Resource::Image:
        return request.forPreload() ? ResourceLoadPriorityVeryLow : ResourceLoadPriorityLow;
    case Resource::XSLStyleSheet:
        return ResourceLoadPriorityHigh;
    case Resource::SVGDocument:
        return ResourceLoadPriorityLow;
    case Resource::LinkPrefetch:
        return ResourceLoadPriorityVeryLow;
    case Resource::LinkSubresource:
        return ResourceLoadPriorityLow;
    case Resource::TextTrack:
        return ResourceLoadPriorityLow;
    case Resource::Shader:
        return ResourceLoadPriorityMedium;
    case Resource::NumberOfTypes:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return ResourceLoadPriorityUnresolved;
}

Resource* resourceFromDataURIRequest(const ResourceRequest& request)
{
    const KURL& url = request.url();
    ASSERT(url.protocolIsData());

    WebKit::WebString mimetype;
    WebKit::WebString charset;
    RefPtr<SharedBuffer> data = PassRefPtr<SharedBuffer>(WebKit::Platform::current()->parseDataURL(url, mimetype, charset));
    if (!data)
        return 0;
    ResourceResponse response(url, mimetype, data->size(), charset, String());

    Resource* resource = createResource(Resource::Image, request, charset);
    resource->responseReceived(response);
    // FIXME: AppendData causes an unnecessary memcpy.
    if (data->size())
        resource->appendData(data->data(), data->size());
    resource->finish();
    return resource;
}

} // namespace

ResourceFetcher::ResourceFetcher(DocumentLoader* documentLoader)
    : m_document(0)
    , m_documentLoader(documentLoader)
    , m_requestCount(0)
    , m_garbageCollectDocumentResourcesTimer(this, &ResourceFetcher::garbageCollectDocumentResourcesTimerFired)
    , m_autoLoadImages(true)
    , m_imagesEnabled(true)
    , m_allowStaleResources(false)
{
}

ResourceFetcher::~ResourceFetcher()
{
    m_documentLoader = 0;
    m_document = 0;

    clearPreloads();

    // Make sure no requests still point to this ResourceFetcher
    ASSERT(!m_requestCount);
}

Resource* ResourceFetcher::cachedResource(const String& resourceURL) const
{
    KURL url = m_document->completeURL(resourceURL);
    return cachedResource(url);
}

Resource* ResourceFetcher::cachedResource(const KURL& resourceURL) const
{
    KURL url = MemoryCache::removeFragmentIdentifierIfNeeded(resourceURL);
    return m_documentResources.get(url).get();
}

Frame* ResourceFetcher::frame() const
{
    if (m_documentLoader)
        return m_documentLoader->frame();
    if (m_document && m_document->import())
        return m_document->import()->frame();
    return 0;
}

FetchContext& ResourceFetcher::context() const
{
    if (Frame* frame = this->frame())
        return frame->fetchContext();
    return FetchContext::nullInstance();
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
    if (Frame* f = frame()) {
        if (f->document()->pageDismissalEventBeingDispatched() != Document::NoDismissal) {
            KURL requestURL = request.resourceRequest().url();
            if (requestURL.isValid() && canRequest(Resource::Image, requestURL, request.options(), request.forPreload()))
                PingLoader::loadImage(f, requestURL);
            return 0;
        }
    }

    if (request.resourceRequest().url().protocolIsData())
        preCacheDataURIImage(request);

    request.setDefer(clientDefersImage(request.resourceRequest().url()) ? FetchRequest::DeferredByClient : FetchRequest::NoDefer);
    return static_cast<ImageResource*>(requestResource(Resource::Image, request).get());
}

void ResourceFetcher::preCacheDataURIImage(const FetchRequest& request)
{
    const KURL& url = request.resourceRequest().url();
    ASSERT(url.protocolIsData());

    if (Resource* existing = memoryCache()->resourceForURL(url))
        return;

    if (Resource* resource = resourceFromDataURIRequest(request.resourceRequest()))
        memoryCache()->add(resource);
}

ResourcePtr<FontResource> ResourceFetcher::fetchFont(FetchRequest& request)
{
    return static_cast<FontResource*>(requestResource(Resource::Font, request).get());
}

ResourcePtr<TextTrackResource> ResourceFetcher::fetchTextTrack(FetchRequest& request)
{
    return static_cast<TextTrackResource*>(requestResource(Resource::TextTrack, request).get());
}

ResourcePtr<ShaderResource> ResourceFetcher::fetchShader(FetchRequest& request)
{
    return static_cast<ShaderResource*>(requestResource(Resource::Shader, request).get());
}

ResourcePtr<RawResource> ResourceFetcher::fetchImport(FetchRequest& request)
{
    return static_cast<RawResource*>(requestResource(Resource::ImportResource, request).get());
}

ResourcePtr<CSSStyleSheetResource> ResourceFetcher::fetchCSSStyleSheet(FetchRequest& request)
{
    return static_cast<CSSStyleSheetResource*>(requestResource(Resource::CSSStyleSheet, request).get());
}

ResourcePtr<CSSStyleSheetResource> ResourceFetcher::fetchUserCSSStyleSheet(FetchRequest& request)
{
    KURL url = MemoryCache::removeFragmentIdentifierIfNeeded(request.resourceRequest().url());

    if (Resource* existing = memoryCache()->resourceForURL(url)) {
        if (existing->type() == Resource::CSSStyleSheet)
            return static_cast<CSSStyleSheetResource*>(existing);
        memoryCache()->remove(existing);
    }

    request.setOptions(ResourceLoaderOptions(DoNotSendCallbacks, SniffContent, BufferData, AllowStoredCredentials, ClientRequestedCredentials, AskClientForCrossOriginCredentials, SkipSecurityCheck, CheckContentSecurityPolicy, UseDefaultOriginRestrictionsForType, DocumentContext));
    return static_cast<CSSStyleSheetResource*>(requestResource(Resource::CSSStyleSheet, request).get());
}

ResourcePtr<ScriptResource> ResourceFetcher::fetchScript(FetchRequest& request)
{
    return static_cast<ScriptResource*>(requestResource(Resource::Script, request).get());
}

ResourcePtr<XSLStyleSheetResource> ResourceFetcher::fetchXSLStyleSheet(FetchRequest& request)
{
    return static_cast<XSLStyleSheetResource*>(requestResource(Resource::XSLStyleSheet, request).get());
}

ResourcePtr<DocumentResource> ResourceFetcher::fetchSVGDocument(FetchRequest& request)
{
    return static_cast<DocumentResource*>(requestResource(Resource::SVGDocument, request).get());
}

ResourcePtr<Resource> ResourceFetcher::fetchLinkResource(Resource::Type type, FetchRequest& request)
{
    ASSERT(frame());
    ASSERT(type == Resource::LinkPrefetch || type == Resource::LinkSubresource);
    return requestResource(type, request);
}

ResourcePtr<RawResource> ResourceFetcher::fetchRawResource(FetchRequest& request)
{
    return static_cast<RawResource*>(requestResource(Resource::Raw, request).get());
}

ResourcePtr<RawResource> ResourceFetcher::fetchMainResource(FetchRequest& request)
{
    return static_cast<RawResource*>(requestResource(Resource::MainResource, request).get());
}

bool ResourceFetcher::checkInsecureContent(Resource::Type type, const KURL& url, MixedContentBlockingTreatment treatment) const
{
    if (treatment == TreatAsDefaultForType) {
        switch (type) {
        case Resource::Script:
        case Resource::XSLStyleSheet:
        case Resource::SVGDocument:
        case Resource::CSSStyleSheet:
        case Resource::ImportResource:
            // These resource can inject script into the current document (Script,
            // XSL) or exfiltrate the content of the current document (CSS).
            treatment = TreatAsActiveContent;
            break;

        case Resource::TextTrack:
        case Resource::Shader:
        case Resource::Raw:
        case Resource::Image:
        case Resource::Font:
            // These resources can corrupt only the frame's pixels.
            treatment = TreatAsPassiveContent;
            break;

        case Resource::MainResource:
        case Resource::LinkPrefetch:
        case Resource::LinkSubresource:
            // These cannot affect the current document.
            treatment = TreatAsAlwaysAllowedContent;
            break;
        case Resource::NumberOfTypes:
            ASSERT_NOT_REACHED();
        }
    }
    if (treatment == TreatAsActiveContent) {
        if (Frame* f = frame()) {
            if (!f->loader()->mixedContentChecker()->canRunInsecureContent(m_document->securityOrigin(), url))
                return false;
        }
    } else if (treatment == TreatAsPassiveContent) {
        if (Frame* f = frame()) {
            Frame* top = f->tree()->top();
            if (!top->loader()->mixedContentChecker()->canDisplayInsecureContent(top->document()->securityOrigin(), url))
                return false;
        }
    } else {
        ASSERT(treatment == TreatAsAlwaysAllowedContent);
    }
    return true;
}

bool ResourceFetcher::canRequest(Resource::Type type, const KURL& url, const ResourceLoaderOptions& options, bool forPreload)
{
    if (document() && !document()->securityOrigin()->canDisplay(url)) {
        if (!forPreload)
            context().reportLocalLoadFailed(url);
        LOG(ResourceLoading, "ResourceFetcher::requestResource URL was not allowed by SecurityOrigin::canDisplay");
        return 0;
    }

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    bool shouldBypassMainWorldContentSecurityPolicy = (frame() && frame()->script()->shouldBypassMainWorldContentSecurityPolicy()) || (options.contentSecurityPolicyOption == DoNotCheckContentSecurityPolicy);

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
    case Resource::Shader:
    case Resource::ImportResource:
        // By default these types of resources can be loaded from any origin.
        // FIXME: Are we sure about Resource::Font?
        if (options.requestOriginPolicy == RestrictToSameOrigin && !m_document->securityOrigin()->canRequest(url)) {
            printAccessDeniedMessage(url);
            return false;
        }
        break;
    case Resource::SVGDocument:
    case Resource::XSLStyleSheet:
        if (!m_document->securityOrigin()->canRequest(url)) {
            printAccessDeniedMessage(url);
            return false;
        }
        break;
    case Resource::NumberOfTypes:
        ASSERT_NOT_REACHED();
    }

    switch (type) {
    case Resource::XSLStyleSheet:
        if (!shouldBypassMainWorldContentSecurityPolicy && !m_document->contentSecurityPolicy()->allowScriptFromSource(url))
            return false;
        break;
    case Resource::Script:
    case Resource::ImportResource:
        if (!shouldBypassMainWorldContentSecurityPolicy && !m_document->contentSecurityPolicy()->allowScriptFromSource(url))
            return false;

        if (frame()) {
            Settings* settings = frame()->settings();
            if (!frame()->loader()->client()->allowScriptFromSource(!settings || settings->isScriptEnabled(), url)) {
                frame()->loader()->client()->didNotAllowScript();
                return false;
            }
        }
        break;
    case Resource::Shader:
        // Since shaders are referenced from CSS Styles use the same rules here.
    case Resource::CSSStyleSheet:
        if (!shouldBypassMainWorldContentSecurityPolicy && !m_document->contentSecurityPolicy()->allowStyleFromSource(url))
            return false;
        break;
    case Resource::SVGDocument:
    case Resource::Image:
        if (!shouldBypassMainWorldContentSecurityPolicy && !m_document->contentSecurityPolicy()->allowImageFromSource(url))
            return false;
        break;
    case Resource::Font: {
        if (!shouldBypassMainWorldContentSecurityPolicy && !m_document->contentSecurityPolicy()->allowFontFromSource(url))
            return false;
        break;
    }
    case Resource::MainResource:
    case Resource::Raw:
    case Resource::LinkPrefetch:
    case Resource::LinkSubresource:
        break;
    case Resource::TextTrack:
        // Cues aren't called out in the CPS spec yet, but they only work with a media element
        // so use the media policy.
        if (!shouldBypassMainWorldContentSecurityPolicy && !m_document->contentSecurityPolicy()->allowMediaFromSource(url))
            return false;
        break;
    case Resource::NumberOfTypes:
        ASSERT_NOT_REACHED();
    }

    // Last of all, check for insecure content. We do this last so that when
    // folks block insecure content with a CSP policy, they don't get a warning.
    // They'll still get a warning in the console about CSP blocking the load.

    // FIXME: Should we consider forPreload here?
    if (!checkInsecureContent(type, url, options.mixedContentBlockingTreatment))
        return false;

    return true;
}

bool ResourceFetcher::canAccess(Resource* resource)
{
    // Redirects can change the response URL different from one of request.
    if (!canRequest(resource->type(), resource->response().url(), resource->options(), false))
        return false;

    String error;
    switch (resource->type()) {
    case Resource::Script:
    case Resource::ImportResource:
        if (resource->options().requestOriginPolicy == PotentiallyCrossOriginEnabled
            && !m_document->securityOrigin()->canRequest(resource->response().url())
            && !resource->passesAccessControlCheck(m_document->securityOrigin(), error)) {
            if (frame() && frame()->document())
                frame()->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Script from origin '" + SecurityOrigin::create(resource->response().url())->toString() + "' has been blocked from loading by Cross-Origin Resource Sharing policy: " + error);
            return false;
        }

        break;
    default:
        ASSERT_NOT_REACHED(); // FIXME: generalize to non-script resources
        return false;
    }

    return true;
}

bool ResourceFetcher::shouldLoadNewResource() const
{
    if (!frame())
        return false;
    if (!m_documentLoader)
        return true;
    return m_documentLoader == frame()->loader()->activeDocumentLoader();
}

ResourcePtr<Resource> ResourceFetcher::requestResource(Resource::Type type, FetchRequest& request)
{
    ASSERT(request.options().synchronousPolicy == RequestAsynchronously || type == Resource::Raw);

    KURL url = request.resourceRequest().url();

    LOG(ResourceLoading, "ResourceFetcher::requestResource '%s', charset '%s', priority=%d, forPreload=%u", url.elidedString().latin1().data(), request.charset().latin1().data(), request.priority(), request.forPreload());

    // If only the fragment identifiers differ, it is the same resource.
    url = MemoryCache::removeFragmentIdentifierIfNeeded(url);

    if (!url.isValid())
        return 0;

    if (!canRequest(type, url, request.options(), request.forPreload()))
        return 0;

    if (Frame* f = frame())
        f->loader()->client()->dispatchWillRequestResource(&request);

    // See if we can use an existing resource from the cache.
    ResourcePtr<Resource> resource = memoryCache()->resourceForURL(url);

    const RevalidationPolicy policy = determineRevalidationPolicy(type, request.mutableResourceRequest(), request.forPreload(), resource.get(), request.defer());
    switch (policy) {
    case Reload:
        memoryCache()->remove(resource.get());
        // Fall through
    case Load:
        resource = loadResource(type, request, request.charset());
        HistogramSupport::histogramEnumeration(
            "WebCore.ResourceFetcher.ActionUponResourceRequest", LoadResource,
            NumberOfResourceRequestActions);
        break;
    case Revalidate:
        resource = revalidateResource(request, resource.get());
        HistogramSupport::histogramEnumeration(
            "WebCore.ResourceFetcher.ActionUponResourceRequest", RevalidateResource,
            NumberOfResourceRequestActions);
        break;
    case Use:
        resource->updateForAccess();
        notifyLoadedFromMemoryCache(resource.get());
        HistogramSupport::histogramEnumeration(
            "WebCore.ResourceFetcher.ActionUponResourceRequest", UseResourceFromCache,
            NumberOfResourceRequestActions);
        HistogramSupport::histogramEnumeration(
            "WebCore.ResourceFetcher.ResourceHasClientUponCacheHit", resource->hasClients(), 2);
        HistogramSupport::histogramEnumeration(
            "WebCore.ResourceFetcher.ResourceTypeUponCacheHit", resource->type(),
            Resource::NumberOfTypes);
        break;
    }

    if (!resource)
        return 0;

    if (policy != Use)
        resource->setIdentifier(createUniqueIdentifier());

    if (!request.forPreload() || policy != Use) {
        ResourceLoadPriority priority = loadPriority(type, request);
        if (priority != resource->resourceRequest().priority()) {
            resource->resourceRequest().setPriority(priority);
            resource->didChangePriority(priority);
        }
    }

    if ((policy != Use || resource->stillNeedsLoad()) && FetchRequest::NoDefer == request.defer()) {
        if (!shouldLoadNewResource()) {
            if (resource->inCache())
                memoryCache()->remove(resource.get());
            return 0;
        }

        if (!m_documentLoader || !m_documentLoader->scheduleArchiveLoad(resource.get(), request.resourceRequest()))
            resource->load(this, request.options());

        // For asynchronous loads that immediately fail, it's sufficient to return a
        // null Resource, as it indicates that something prevented the load from starting.
        // If there's a network error, that failure will happen asynchronously. However, if
        // a sync load receives a network error, it will have already happened by this point.
        // In that case, the requester should have access to the relevant ResourceError, so
        // we need to return a non-null Resource.
        if (resource->errorOccurred()) {
            if (resource->inCache())
                memoryCache()->remove(resource.get());
            return request.options().synchronousPolicy == RequestSynchronously ? resource : 0;
        }
    }

    // FIXME: Temporarily leave main resource caching disabled for chromium,
    // see https://bugs.webkit.org/show_bug.cgi?id=107962. Before caching main
    // resources, we should be sure to understand the implications for memory
    // use.
    //
    // Ensure main resources aren't preloaded, and other main resource loads
    // are removed from cache to prevent reuse.
    if (type == Resource::MainResource) {
        ASSERT(policy != Use);
        ASSERT(policy != Revalidate);
        memoryCache()->remove(resource.get());
        if (request.forPreload())
            return 0;
    }

    if (!request.resourceRequest().url().protocolIsData())
        m_validatedURLs.add(request.resourceRequest().url());

    ASSERT(resource->url() == url.string());
    m_documentResources.set(resource->url(), resource);
    return resource;
}

void ResourceFetcher::determineTargetType(ResourceRequest& request, Resource::Type type)
{
    ResourceRequest::TargetType targetType;

    switch (type) {
    case Resource::MainResource:
        if (frame()->tree()->parent())
            targetType = ResourceRequest::TargetIsSubframe;
        else
            targetType = ResourceRequest::TargetIsMainFrame;
        break;
    case Resource::CSSStyleSheet:
    case Resource::XSLStyleSheet:
        targetType = ResourceRequest::TargetIsStyleSheet;
        break;
    case Resource::Script:
        targetType = ResourceRequest::TargetIsScript;
        break;
    case Resource::Font:
        targetType = ResourceRequest::TargetIsFont;
        break;
    case Resource::Image:
        targetType = ResourceRequest::TargetIsImage;
        break;
    case Resource::Shader:
    case Resource::Raw:
    case Resource::ImportResource:
        targetType = ResourceRequest::TargetIsSubresource;
        break;
    case Resource::LinkPrefetch:
        targetType = ResourceRequest::TargetIsPrefetch;
        break;
    case Resource::LinkSubresource:
        targetType = ResourceRequest::TargetIsSubresource;
        break;
    case Resource::TextTrack:
        targetType = ResourceRequest::TargetIsTextTrack;
        break;
    case Resource::SVGDocument:
        targetType = ResourceRequest::TargetIsImage;
        break;
    default:
        ASSERT_NOT_REACHED();
        targetType = ResourceRequest::TargetIsSubresource;
        break;
    }
    request.setTargetType(targetType);
}

ResourceRequestCachePolicy ResourceFetcher::resourceRequestCachePolicy(const ResourceRequest& request, Resource::Type type)
{
    if (type == Resource::MainResource) {
        FrameLoadType frameLoadType = frame()->loader()->loadType();
        bool isReload = frameLoadType == FrameLoadTypeReload || frameLoadType == FrameLoadTypeReloadFromOrigin;
        if (request.httpMethod() == "POST" && frameLoadType == FrameLoadTypeBackForward)
            return ReturnCacheDataDontLoad;
        if (!m_documentLoader->overrideEncoding().isEmpty() || frameLoadType == FrameLoadTypeBackForward)
            return ReturnCacheDataElseLoad;
        if (isReload || frameLoadType == FrameLoadTypeSame || request.isConditional() || request.httpMethod() == "POST")
            return ReloadIgnoringCacheData;
        return UseProtocolCachePolicy;
    }

    if (request.isConditional())
        return ReloadIgnoringCacheData;

    if (m_documentLoader && m_documentLoader->isLoadingInAPISense()) {
        // For POST requests, we mutate the main resource's cache policy to avoid form resubmission.
        // This policy should not be inherited by subresources.
        ResourceRequestCachePolicy mainResourceCachePolicy = m_documentLoader->request().cachePolicy();
        if (mainResourceCachePolicy == ReturnCacheDataDontLoad)
            return ReturnCacheDataElseLoad;
        return mainResourceCachePolicy;
    }
    return UseProtocolCachePolicy;
}

void ResourceFetcher::addAdditionalRequestHeaders(ResourceRequest& request, Resource::Type type)
{
    if (!frame())
        return;

    if (request.cachePolicy() == UseProtocolCachePolicy)
        request.setCachePolicy(resourceRequestCachePolicy(request, type));
    if (request.targetType() == ResourceRequest::TargetIsUnspecified)
        determineTargetType(request, type);
    if (type == Resource::LinkPrefetch || type == Resource::LinkSubresource)
        request.setHTTPHeaderField("Purpose", "prefetch");

    context().addAdditionalRequestHeaders(*document(), request, type);
}

ResourcePtr<Resource> ResourceFetcher::revalidateResource(const FetchRequest& request, Resource* resource)
{
    ASSERT(resource);
    ASSERT(resource->inCache());
    ASSERT(resource->isLoaded());
    ASSERT(resource->canUseCacheValidator());
    ASSERT(!resource->resourceToRevalidate());

    ResourceRequest revalidatingRequest(resource->resourceRequest());
    addAdditionalRequestHeaders(revalidatingRequest, resource->type());

    const String& lastModified = resource->response().httpHeaderField("Last-Modified");
    const String& eTag = resource->response().httpHeaderField("ETag");
    if (!lastModified.isEmpty() || !eTag.isEmpty()) {
        ASSERT(context().cachePolicy(resource->type()) != CachePolicyReload);
        if (context().cachePolicy(resource->type()) == CachePolicyRevalidate)
            revalidatingRequest.setHTTPHeaderField("Cache-Control", "max-age=0");
        if (!lastModified.isEmpty())
            revalidatingRequest.setHTTPHeaderField("If-Modified-Since", lastModified);
        if (!eTag.isEmpty())
            revalidatingRequest.setHTTPHeaderField("If-None-Match", eTag);
    }

    ResourcePtr<Resource> newResource = createResource(resource->type(), revalidatingRequest, resource->encoding());

    LOG(ResourceLoading, "Resource %p created to revalidate %p", newResource.get(), resource);
    newResource->setResourceToRevalidate(resource);

    memoryCache()->remove(resource);
    memoryCache()->add(newResource.get());
    storeResourceTimingInitiatorInformation(newResource, request);
    TRACE_EVENT_ASYNC_BEGIN2("net", "Resource", newResource.get(), "url", newResource->url().string().ascii(), "priority", newResource->resourceRequest().priority());
    return newResource;
}

ResourcePtr<Resource> ResourceFetcher::loadResource(Resource::Type type, FetchRequest& request, const String& charset)
{
    ASSERT(!memoryCache()->resourceForURL(request.resourceRequest().url()));

    LOG(ResourceLoading, "Loading Resource for '%s'.", request.resourceRequest().url().elidedString().latin1().data());

    addAdditionalRequestHeaders(request.mutableResourceRequest(), type);
    ResourcePtr<Resource> resource = createResource(type, request.mutableResourceRequest(), charset);

    memoryCache()->add(resource.get());
    storeResourceTimingInitiatorInformation(resource, request);
    TRACE_EVENT_ASYNC_BEGIN2("net", "Resource", resource.get(), "url", resource->url().string().ascii(), "priority", resource->resourceRequest().priority());
    return resource;
}

void ResourceFetcher::storeResourceTimingInitiatorInformation(const ResourcePtr<Resource>& resource, const FetchRequest& request)
{
    if (request.options().requestInitiatorContext != DocumentContext)
        return;

    RefPtr<ResourceTimingInfo> info = ResourceTimingInfo::create(request.options().initiatorInfo.name, monotonicallyIncreasingTime());

    if (resource->type() == Resource::MainResource) {
        // <iframe>s should report the initial navigation requested by the parent document, but not subsequent navigations.
        if (frame()->ownerElement() && !frame()->ownerElement()->loadedNonEmptyDocument()) {
            info->setInitiatorType(frame()->ownerElement()->localName());
            m_resourceTimingInfoMap.add(resource.get(), info);
            frame()->ownerElement()->didLoadNonEmptyDocument();
        }
    } else {
        m_resourceTimingInfoMap.add(resource.get(), info);
    }
}

ResourceFetcher::RevalidationPolicy ResourceFetcher::determineRevalidationPolicy(Resource::Type type, ResourceRequest& request, bool forPreload, Resource* existingResource, FetchRequest::DeferOption defer) const
{
    if (!existingResource)
        return Load;

    // We already have a preload going for this URL.
    if (forPreload && existingResource->isPreloaded())
        return Use;

    // If the same URL has been loaded as a different type, we need to reload.
    if (existingResource->type() != type) {
        LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to type mismatch.");
        return Reload;
    }

    // Do not load from cache if images are not enabled. The load for this image will be blocked
    // in ImageResource::load.
    if (FetchRequest::DeferredByClient == defer)
        return Reload;

    // Always use data uris.
    // FIXME: Extend this to non-images.
    if (type == Resource::Image && request.url().protocolIsData())
        return Use;

    if (!existingResource->canReuse(request))
        return Reload;

    // Certain requests (e.g., XHRs) might have manually set headers that require revalidation.
    // FIXME: In theory, this should be a Revalidate case. In practice, the MemoryCache revalidation path assumes a whole bunch
    // of things about how revalidation works that manual headers violate, so punt to Reload instead.
    if (request.isConditional())
        return Reload;

    // Don't reload resources while pasting.
    if (m_allowStaleResources)
        return Use;

    // Alwaus use preloads.
    if (existingResource->isPreloaded())
        return Use;

    // CachePolicyHistoryBuffer uses the cache no matter what.
    if (context().cachePolicy(type) == CachePolicyHistoryBuffer)
        return Use;

    // Don't reuse resources with Cache-control: no-store.
    if (existingResource->response().cacheControlContainsNoStore()) {
        LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to Cache-control: no-store.");
        return Reload;
    }

    // If credentials were sent with the previous request and won't be
    // with this one, or vice versa, re-fetch the resource.
    //
    // This helps with the case where the server sends back
    // "Access-Control-Allow-Origin: *" all the time, but some of the
    // client's requests are made without CORS and some with.
    if (existingResource->resourceRequest().allowCookies() != request.allowCookies()) {
        LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to difference in credentials settings.");
        return Reload;
    }

    // During the initial load, avoid loading the same resource multiple times for a single document, even if the cache policies would tell us to.
    if (document() && !document()->loadEventFinished() && m_validatedURLs.contains(existingResource->url()))
        return Use;

    // CachePolicyReload always reloads
    if (context().cachePolicy(type) == CachePolicyReload) {
        LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to CachePolicyReload.");
        return Reload;
    }

    // We'll try to reload the resource if it failed last time.
    if (existingResource->errorOccurred()) {
        LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicye reloading due to resource being in the error state");
        return Reload;
    }

    // For resources that are not yet loaded we ignore the cache policy.
    if (existingResource->isLoading())
        return Use;

    // Check if the cache headers requires us to revalidate (cache expiration for example).
    if (existingResource->mustRevalidateDueToCacheHeaders(context().cachePolicy(type))) {
        // See if the resource has usable ETag or Last-modified headers.
        if (existingResource->canUseCacheValidator())
            return Revalidate;

        // No, must reload.
        LOG(ResourceLoading, "ResourceFetcher::determineRevalidationPolicy reloading due to missing cache validators.");
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
    if (!m_document || m_document->url().isNull())
        message = "Unsafe attempt to load URL " + url.elidedString() + '.';
    else
        message = "Unsafe attempt to load URL " + url.elidedString() + " from frame with URL " + m_document->url().elidedString() + ". Domains, protocols and ports must match.\n";

    frame()->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message);
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
    return frame() && !frame()->loader()->client()->allowImage(m_imagesEnabled, url);
}

bool ResourceFetcher::shouldDeferImageLoad(const KURL& url) const
{
    return clientDefersImage(url) || !m_autoLoadImages;
}

void ResourceFetcher::reloadImagesIfNotDeferred()
{
    DocumentResourceMap::iterator end = m_documentResources.end();
    for (DocumentResourceMap::iterator it = m_documentResources.begin(); it != end; ++it) {
        Resource* resource = it->value.get();
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

void ResourceFetcher::didLoadResource(Resource* resource)
{
    RefPtr<DocumentLoader> protectDocumentLoader(m_documentLoader);
    RefPtr<Document> protectDocument(m_document);

    if (resource && resource->response().isHTTP() && ((!resource->errorOccurred() && !resource->wasCanceled()) || resource->response().httpStatusCode() == 304) && document()) {
        ResourceTimingInfoMap::iterator it = m_resourceTimingInfoMap.find(resource);
        if (it != m_resourceTimingInfoMap.end()) {
            Document* initiatorDocument = document();
            if (resource->type() == Resource::MainResource)
                initiatorDocument = document()->parentDocument();
            ASSERT(initiatorDocument);
            RefPtr<ResourceTimingInfo> info = it->value;
            m_resourceTimingInfoMap.remove(it);
            info->setInitialRequest(resource->resourceRequest());
            info->setFinalResponse(resource->response());
            info->setLoadFinishTime(resource->loadFinishTime());
            if (DOMWindow* initiatorWindow = initiatorDocument->domWindow())
                initiatorWindow->performance()->addResourceTiming(*info, initiatorDocument);
        }
    }

    if (frame())
        frame()->loader()->loadDone();
    performPostLoadActions();

    if (!m_garbageCollectDocumentResourcesTimer.isActive())
        m_garbageCollectDocumentResourcesTimer.startOneShot(0);
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

    for (DocumentResourceMap::iterator it = m_documentResources.begin(); it != m_documentResources.end(); ++it) {
        if (it->value->hasOneHandle())
            resourcesToDelete.append(it->key);
    }

    for (StringVector::const_iterator it = resourcesToDelete.begin(); it != resourcesToDelete.end(); ++it)
        m_documentResources.remove(*it);
}

void ResourceFetcher::performPostLoadActions()
{
    checkForPendingPreloads();
}

void ResourceFetcher::notifyLoadedFromMemoryCache(Resource* resource)
{
    if (!frame() || !frame()->page() || resource->status() != Resource::Cached || m_validatedURLs.contains(resource->url()))
        return;
    if (!resource->shouldSendResourceLoadCallbacks())
        return;

    ResourceRequest request(resource->url());
    unsigned long identifier = createUniqueIdentifier();
    context().dispatchDidLoadResourceFromMemoryCache(request, resource->response());
    // FIXME: If willSendRequest changes the request, we don't respect it.
    willSendRequest(identifier, request, ResourceResponse(), resource->options());
    InspectorInstrumentation::markResourceAsCached(frame()->page(), identifier);
    context().sendRemainingDelegateMessages(m_documentLoader, identifier, resource->response(), 0, resource->encodedSize(), 0, ResourceError());
}

void ResourceFetcher::incrementRequestCount(const Resource* res)
{
    if (res->ignoreForRequestCount())
        return;

    ++m_requestCount;
}

void ResourceFetcher::decrementRequestCount(const Resource* res)
{
    if (res->ignoreForRequestCount())
        return;

    --m_requestCount;
    ASSERT(m_requestCount > -1);
}

void ResourceFetcher::preload(Resource::Type type, FetchRequest& request, const String& charset)
{
    bool delaySubresourceLoad = true;
    delaySubresourceLoad = false;
    if (delaySubresourceLoad) {
        bool hasRendering = m_document->body() && m_document->body()->renderer();
        bool canBlockParser = type == Resource::Script || type == Resource::CSSStyleSheet;
        if (!hasRendering && !canBlockParser) {
            // Don't preload subresources that can't block the parser before we have something to draw.
            // This helps prevent preloads from delaying first display when bandwidth is limited.
            PendingPreload pendingPreload = { type, request, charset };
            m_pendingPreloads.append(pendingPreload);
            return;
        }
    }
    requestPreload(type, request, charset);
}

void ResourceFetcher::checkForPendingPreloads()
{
    if (m_pendingPreloads.isEmpty() || !m_document->body() || !m_document->body()->renderer())
        return;
    while (!m_pendingPreloads.isEmpty()) {
        PendingPreload preload = m_pendingPreloads.takeFirst();
        // Don't request preload if the resource already loaded normally (this will result in double load if the page is being reloaded with cached results ignored).
        if (!cachedResource(preload.m_request.resourceRequest().url()))
            requestPreload(preload.m_type, preload.m_request, preload.m_charset);
    }
    m_pendingPreloads.clear();
}

void ResourceFetcher::requestPreload(Resource::Type type, FetchRequest& request, const String& charset)
{
    String encoding;
    if (type == Resource::Script || type == Resource::CSSStyleSheet)
        encoding = charset.isEmpty() ? m_document->charset() : charset;

    request.setCharset(encoding);
    request.setForPreload(true);

    ResourcePtr<Resource> resource = requestResource(type, request);
    if (!resource || (m_preloads && m_preloads->contains(resource.get())))
        return;
    TRACE_EVENT_ASYNC_STEP0("net", "Resource", resource.get(), "Preload");
    resource->increasePreloadCount();

    if (!m_preloads)
        m_preloads = adoptPtr(new ListHashSet<Resource*>);
    m_preloads->add(resource.get());

#if PRELOAD_DEBUG
    printf("PRELOADING %s\n",  resource->url().latin1().data());
#endif
}

bool ResourceFetcher::isPreloaded(const String& urlString) const
{
    const KURL& url = m_document->completeURL(urlString);

    if (m_preloads) {
        ListHashSet<Resource*>::iterator end = m_preloads->end();
        for (ListHashSet<Resource*>::iterator it = m_preloads->begin(); it != end; ++it) {
            Resource* resource = *it;
            if (resource->url() == url)
                return true;
        }
    }

    Deque<PendingPreload>::const_iterator dequeEnd = m_pendingPreloads.end();
    for (Deque<PendingPreload>::const_iterator it = m_pendingPreloads.begin(); it != dequeEnd; ++it) {
        PendingPreload pendingPreload = *it;
        if (pendingPreload.m_request.resourceRequest().url() == url)
            return true;
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

    ListHashSet<Resource*>::iterator end = m_preloads->end();
    for (ListHashSet<Resource*>::iterator it = m_preloads->begin(); it != end; ++it) {
        Resource* res = *it;
        res->decreasePreloadCount();
        bool deleted = res->deleteIfPossible();
        if (!deleted && res->preloadResult() == Resource::PreloadNotReferenced)
            memoryCache()->remove(res);
    }
    m_preloads.clear();
}

void ResourceFetcher::clearPendingPreloads()
{
    m_pendingPreloads.clear();
}

void ResourceFetcher::didFinishLoading(const Resource* resource, double finishTime, const ResourceLoaderOptions& options)
{
    TRACE_EVENT_ASYNC_END0("net", "Resource", resource);
    if (options.sendLoadCallbacks != SendCallbacks)
        return;
    context().dispatchDidFinishLoading(m_documentLoader, resource->identifier(), finishTime);
}

void ResourceFetcher::didChangeLoadingPriority(const Resource* resource, ResourceLoadPriority loadPriority)
{
    TRACE_EVENT_ASYNC_STEP1("net", "Resource", resource, "ChangePriority", "priority", loadPriority);
    context().dispatchDidChangeResourcePriority(resource->identifier(), loadPriority);
}

void ResourceFetcher::didFailLoading(const Resource* resource, const ResourceError& error, const ResourceLoaderOptions& options)
{
    TRACE_EVENT_ASYNC_END0("net", "Resource", resource);
    if (options.sendLoadCallbacks != SendCallbacks)
        return;
    context().dispatchDidFail(m_documentLoader, resource->identifier(), error);
}

void ResourceFetcher::willSendRequest(unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse, const ResourceLoaderOptions& options)
{
    if (options.sendLoadCallbacks == SendCallbacks)
        context().dispatchWillSendRequest(m_documentLoader, identifier, request, redirectResponse, options.initiatorInfo);
    else
        InspectorInstrumentation::willSendRequest(frame(), identifier, m_documentLoader, request, redirectResponse, options.initiatorInfo);
}

void ResourceFetcher::didReceiveResponse(const Resource* resource, const ResourceResponse& response, const ResourceLoaderOptions& options)
{
    if (options.sendLoadCallbacks != SendCallbacks)
        return;
    context().dispatchDidReceiveResponse(m_documentLoader, resource->identifier(), response, resource->loader());
}

void ResourceFetcher::didReceiveData(const Resource* resource, const char* data, int dataLength, int encodedDataLength, const ResourceLoaderOptions& options)
{
    // FIXME: use frame of master document for imported documents.
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willReceiveResourceData(frame(), resource->identifier(), encodedDataLength);
    if (options.sendLoadCallbacks != SendCallbacks)
        return;
    context().dispatchDidReceiveData(m_documentLoader, resource->identifier(), data, dataLength, encodedDataLength);
    InspectorInstrumentation::didReceiveResourceData(cookie);
}

void ResourceFetcher::subresourceLoaderFinishedLoadingOnePart(ResourceLoader* loader)
{
    if (m_multipartLoaders)
        m_multipartLoaders->add(loader);
    if (m_loaders)
        m_loaders->remove(loader);
    if (Frame* frame = this->frame())
        return frame->loader()->checkLoadComplete(m_documentLoader);
}

void ResourceFetcher::didInitializeResourceLoader(ResourceLoader* loader)
{
    if (!m_document)
        return;
    if (!m_loaders)
        m_loaders = adoptPtr(new ResourceLoaderSet());
    ASSERT(!m_loaders->contains(loader));
    m_loaders->add(loader);
}

void ResourceFetcher::willTerminateResourceLoader(ResourceLoader* loader)
{
    if (!m_loaders || !m_loaders->contains(loader))
        return;
    m_loaders->remove(loader);
    if (Frame* frame = this->frame())
        frame->loader()->checkLoadComplete(m_documentLoader);
}

void ResourceFetcher::willStartLoadingResource(ResourceRequest& request)
{
    if (m_documentLoader)
        m_documentLoader->applicationCacheHost()->willStartLoadingResource(request);
}

void ResourceFetcher::stopFetching()
{
    if (m_multipartLoaders)
        m_multipartLoaders->cancelAll();
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
}

bool ResourceFetcher::defersLoading() const
{
    if (Frame* frame = this->frame())
        return frame->page()->defersLoading();
    return false;
}

bool ResourceFetcher::isLoadedBy(ResourceLoaderHost* possibleOwner) const
{
    return this == possibleOwner;
}

bool ResourceFetcher::shouldRequest(Resource* resource, const ResourceRequest& request, const ResourceLoaderOptions& options)
{
    if (!canRequest(resource->type(), request.url(), options))
        return false;
    if (resource->type() == Resource::Image && shouldDeferImageLoad(request.url()))
        return false;
    return true;
}

void ResourceFetcher::refResourceLoaderHost()
{
    ref();
}

void ResourceFetcher::derefResourceLoaderHost()
{
    deref();
}

#if PRELOAD_DEBUG
void ResourceFetcher::printPreloadStats()
{
    unsigned scripts = 0;
    unsigned scriptMisses = 0;
    unsigned stylesheets = 0;
    unsigned stylesheetMisses = 0;
    unsigned images = 0;
    unsigned imageMisses = 0;
    ListHashSet<Resource*>::iterator end = m_preloads.end();
    for (ListHashSet<Resource*>::iterator it = m_preloads.begin(); it != end; ++it) {
        Resource* res = *it;
        if (res->preloadResult() == Resource::PreloadNotReferenced)
            printf("!! UNREFERENCED PRELOAD %s\n", res->url().latin1().data());
        else if (res->preloadResult() == Resource::PreloadReferencedWhileComplete)
            printf("HIT COMPLETE PRELOAD %s\n", res->url().latin1().data());
        else if (res->preloadResult() == Resource::PreloadReferencedWhileLoading)
            printf("HIT LOADING PRELOAD %s\n", res->url().latin1().data());

        if (res->type() == Resource::Script) {
            scripts++;
            if (res->preloadResult() < Resource::PreloadReferencedWhileLoading)
                scriptMisses++;
        } else if (res->type() == Resource::CSSStyleSheet) {
            stylesheets++;
            if (res->preloadResult() < Resource::PreloadReferencedWhileLoading)
                stylesheetMisses++;
        } else {
            images++;
            if (res->preloadResult() < Resource::PreloadReferencedWhileLoading)
                imageMisses++;
        }

        if (res->errorOccurred())
            memoryCache()->remove(res);

        res->decreasePreloadCount();
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
    DEFINE_STATIC_LOCAL(ResourceLoaderOptions, options, (SendCallbacks, SniffContent, BufferData, AllowStoredCredentials, ClientRequestedCredentials, AskClientForCrossOriginCredentials, DoSecurityCheck, CheckContentSecurityPolicy, UseDefaultOriginRestrictionsForType, DocumentContext));
    return options;
}

}
