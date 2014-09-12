/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/loader/MixedContentChecker.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

// static
bool MixedContentChecker::isMixedContent(SecurityOrigin* securityOrigin, const KURL& url)
{
    if (securityOrigin->protocol() != "https")
        return false; // We only care about HTTPS security origins.

    // We're in a secure context, so |url| is mixed content if it's insecure.
    return !SecurityOrigin::isSecure(url);
}

// static
MixedContentChecker::ContextType MixedContentChecker::contextTypeFromContext(WebURLRequest::RequestContext context)
{
    switch (context) {
    // "Optionally-blockable" mixed content
    case WebURLRequest::RequestContextAudio:
    case WebURLRequest::RequestContextFavicon:
    case WebURLRequest::RequestContextImage:
    case WebURLRequest::RequestContextVideo:
        return ContextTypeOptionallyBlockable;

    // "Blockable" mixed content
    case WebURLRequest::RequestContextBeacon:
    case WebURLRequest::RequestContextCSPReport:
    case WebURLRequest::RequestContextEmbed:
    case WebURLRequest::RequestContextFetch:
    case WebURLRequest::RequestContextFont:
    case WebURLRequest::RequestContextForm:
    case WebURLRequest::RequestContextFrame:
    case WebURLRequest::RequestContextHyperlink:
    case WebURLRequest::RequestContextIframe:
    case WebURLRequest::RequestContextImageSet:
    case WebURLRequest::RequestContextImport:
    case WebURLRequest::RequestContextLocation:
    case WebURLRequest::RequestContextManifest:
    case WebURLRequest::RequestContextObject:
    case WebURLRequest::RequestContextPing:
    case WebURLRequest::RequestContextScript:
    case WebURLRequest::RequestContextServiceWorker:
    case WebURLRequest::RequestContextSharedWorker:
    case WebURLRequest::RequestContextStyle:
    case WebURLRequest::RequestContextSubresource:
    case WebURLRequest::RequestContextTrack:
    case WebURLRequest::RequestContextWorker:
    case WebURLRequest::RequestContextXSLT:
        return ContextTypeBlockable;

    // "Blockable" mixed content whose behavior changed recently, and which is thus guarded behind the "lax" flag
    case WebURLRequest::RequestContextEventSource:
    case WebURLRequest::RequestContextXMLHttpRequest:
        return ContextTypeBlockableUnlessLax;

    // FIXME: Contexts that we should block, but don't currently. https://crbug.com/388650
    case WebURLRequest::RequestContextDownload:
    case WebURLRequest::RequestContextInternal:
    case WebURLRequest::RequestContextPlugin:
    case WebURLRequest::RequestContextPrefetch:
        return ContextTypeShouldBeBlockable;

    case WebURLRequest::RequestContextUnspecified:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return ContextTypeBlockable;
}

// static
const char* MixedContentChecker::typeNameFromContext(WebURLRequest::RequestContext context)
{
    switch (context) {
    case WebURLRequest::RequestContextAudio:
        return "audio file";
    case WebURLRequest::RequestContextBeacon:
        return "Beacon endpoint";
    case WebURLRequest::RequestContextCSPReport:
        return "Content Security Policy reporting endpoint";
    case WebURLRequest::RequestContextDownload:
        return "download";
    case WebURLRequest::RequestContextEmbed:
        return "plugin resource";
    case WebURLRequest::RequestContextEventSource:
        return "EventSource endpoint";
    case WebURLRequest::RequestContextFavicon:
        return "favicon";
    case WebURLRequest::RequestContextFetch:
        return "resource";
    case WebURLRequest::RequestContextFont:
        return "font";
    case WebURLRequest::RequestContextForm:
        return "form action";
    case WebURLRequest::RequestContextFrame:
        return "frame";
    case WebURLRequest::RequestContextHyperlink:
        return "resource";
    case WebURLRequest::RequestContextIframe:
        return "frame";
    case WebURLRequest::RequestContextImage:
        return "image";
    case WebURLRequest::RequestContextImageSet:
        return "image";
    case WebURLRequest::RequestContextImport:
        return "HTML Import";
    case WebURLRequest::RequestContextInternal:
        return "resource";
    case WebURLRequest::RequestContextLocation:
        return "resource";
    case WebURLRequest::RequestContextManifest:
        return "manifest";
    case WebURLRequest::RequestContextObject:
        return "plugin resource";
    case WebURLRequest::RequestContextPing:
        return "hyperlink auditing endpoint";
    case WebURLRequest::RequestContextPlugin:
        return "plugin data";
    case WebURLRequest::RequestContextPrefetch:
        return "prefetch resource";
    case WebURLRequest::RequestContextScript:
        return "script";
    case WebURLRequest::RequestContextServiceWorker:
        return "Service Worker script";
    case WebURLRequest::RequestContextSharedWorker:
        return "Shared Worker script";
    case WebURLRequest::RequestContextStyle:
        return "stylesheet";
    case WebURLRequest::RequestContextSubresource:
        return "resource";
    case WebURLRequest::RequestContextTrack:
        return "Text Track";
    case WebURLRequest::RequestContextUnspecified:
        return "resource";
    case WebURLRequest::RequestContextVideo:
        return "video";
    case WebURLRequest::RequestContextWorker:
        return "Worker script";
    case WebURLRequest::RequestContextXMLHttpRequest:
        return "XMLHttpRequest endpoint";
    case WebURLRequest::RequestContextXSLT:
        return "XSLT";
    }
    ASSERT_NOT_REACHED();
    return "resource";
}

// static
void MixedContentChecker::logToConsole(LocalFrame* frame, const KURL& url, WebURLRequest::RequestContext requestContext, bool allowed)
{
}

LocalFrame* MixedContentChecker::inWhichFrameIsThisContentMixed(LocalFrame* frame, WebURLRequest::RequestContext requestContext, WebURLRequest::FrameType frameType, const KURL& url)
{
    // No frame, no mixed content:
    if (!frame)
        return 0;

    // We only care about subresource loads; top-level navigations cannot be mixed content.
    if (frameType == WebURLRequest::FrameTypeTopLevel)
        return 0;

    // Check the top frame first.
    if (Frame* top = frame->tree().top()) {
        // FIXME: We need a way to access the top-level frame's SecurityOrigin when that frame
        // is in a different process from the current frame. Until that is done, we bail out
        // early and allow the load.
        if (!top->isLocalFrame())
            return 0;

        LocalFrame* localTop = toLocalFrame(top);
        if (frame != localTop && inWhichFrameIsThisContentMixed(localTop, requestContext, frameType, url))
            return localTop;
    }

    // Just count these for the moment, don't block them.
    if (Platform::current()->isReservedIPAddress(url) && !Platform::current()->isReservedIPAddress(KURL(ParsedURLString, frame->document()->securityOrigin()->toString())))
        UseCounter::count(frame->document(), contextTypeFromContext(requestContext) == ContextTypeBlockable ? UseCounter::MixedContentPrivateIPInPublicWebsiteActive : UseCounter::MixedContentPrivateIPInPublicWebsitePassive);

    // No mixed content, no problem.
    if (!isMixedContent(frame->document()->securityOrigin(), url))
        return 0;

    return frame;
}

// static
bool MixedContentChecker::shouldBlockFetch(LocalFrame* frame, WebURLRequest::RequestContext requestContext, WebURLRequest::FrameType frameType, const KURL& url)
{
    LocalFrame* effectiveFrame = inWhichFrameIsThisContentMixed(frame, requestContext, frameType, url);
    if (!effectiveFrame)
        return false;

    Settings* settings = effectiveFrame->settings();
    FrameLoaderClient* client = effectiveFrame->loader().client();
    SecurityOrigin* securityOrigin = effectiveFrame->document()->securityOrigin();
    bool allowed = false;

    ContextType contextType = contextTypeFromContext(requestContext);
    if (contextType == ContextTypeBlockableUnlessLax)
        contextType = RuntimeEnabledFeatures::laxMixedContentCheckingEnabled() ? ContextTypeOptionallyBlockable : ContextTypeBlockable;

    if (frameType == WebURLRequest::FrameTypeNested) {
        // For subframes, if we're dealing with a CORS-enabled scheme, then block mixed content. Otherwise,
        // treat frames as passive content.
        //
        // FIXME: Remove this temporary hack once we have a reasonable API for launching external applications
        // via URLs. http://crbug.com/318788 and https://crbug.com/393481
        contextType = SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(url.protocol()) ? ContextTypeBlockable : ContextTypeOptionallyBlockable;
    }

    switch (contextType) {
    case ContextTypeOptionallyBlockable:
        allowed = client->allowDisplayingInsecureContent(settings && settings->allowDisplayOfInsecureContent(), securityOrigin, url);
        if (allowed)
            client->didDisplayInsecureContent();
        break;

    case ContextTypeBlockable:
        allowed = client->allowRunningInsecureContent(settings && settings->allowRunningOfInsecureContent(), securityOrigin, url);
        if (allowed)
            client->didRunInsecureContent(securityOrigin, url);
        break;

    case ContextTypeShouldBeBlockable:
        return false;

    case ContextTypeBlockableUnlessLax:
        // We map this to either OptionallyBlockable or Blockable above.
        ASSERT_NOT_REACHED();
        return true;
    };

    String message = String::format(
        "Mixed Content: The page at '%s' was loaded over HTTPS, but requested an insecure %s '%s'. %s",
        frame->document()->url().elidedString().utf8().data(), typeNameFromContext(requestContext), url.elidedString().utf8().data(),
        allowed ? "This content should also be served over HTTPS." : "This request has been blocked; the content must be served over HTTPS.");
    MessageLevel messageLevel = allowed ? WarningMessageLevel : ErrorMessageLevel;
    frame->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, messageLevel, message));

    return !allowed;
}

// static
bool MixedContentChecker::shouldBlockWebSocket(LocalFrame* frame, const KURL& url)
{
    // FIXME: Should we have a RequestContext for WebSockets? Probably not, but this feels hacky otherwise.
    LocalFrame* effectiveFrame = inWhichFrameIsThisContentMixed(frame, WebURLRequest::RequestContextXMLHttpRequest, WebURLRequest::FrameTypeNone, url);
    if (!effectiveFrame)
        return false;

    Settings* settings = effectiveFrame->settings();
    FrameLoaderClient* client = effectiveFrame->loader().client();
    SecurityOrigin* securityOrigin = effectiveFrame->document()->securityOrigin();
    bool allowed = false;

    if (RuntimeEnabledFeatures::laxMixedContentCheckingEnabled()) {
        allowed = client->allowDisplayingInsecureContent(settings && (settings->allowRunningOfInsecureContent() || settings->allowConnectingInsecureWebSocket()), securityOrigin, url);
        if (allowed)
            client->didDisplayInsecureContent();
    } else {
        allowed = client->allowRunningInsecureContent(settings && (settings->allowRunningOfInsecureContent() || settings->allowConnectingInsecureWebSocket()), securityOrigin, url);
        if (allowed)
            client->didRunInsecureContent(securityOrigin, url);
    }

    String message = String::format(
        "Mixed Content: The page at '%s' was loaded over HTTPS, but requested the WebSocket endpoint '%s'. %s",
        effectiveFrame->document()->url().elidedString().utf8().data(), url.elidedString().utf8().data(),
        allowed ? "This endpoint should also be secure ('wss:')." : "This request has been blocked; the endpoint must be secure ('wss:').");
    MessageLevel messageLevel = allowed ? WarningMessageLevel : ErrorMessageLevel;
    effectiveFrame->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, messageLevel, message));

    return !allowed;
}

// static
bool MixedContentChecker::checkFormAction(LocalFrame* frame, const KURL& url)
{
    // For whatever reason, some folks handle forms via JavaScript, and submit to `javascript:void(0)`
    // rather than calling `preventDefault()`. We special-case `javascript:` URLs here, as they don't
    // introduce MixedContent for form submissions.
    if (url.protocolIs("javascript"))
        return false;

    // If lax mixed content checking is enabled (noooo!), skip this check entirely.
    if (RuntimeEnabledFeatures::laxMixedContentCheckingEnabled())
        return false;

    LocalFrame* effectiveFrame = inWhichFrameIsThisContentMixed(frame, WebURLRequest::RequestContextForm, WebURLRequest::FrameTypeNone, url);
    if (!effectiveFrame)
        return false;

    // No "allowed" check here; we're not yet exposing anything which would block form submission.
    FrameLoaderClient* client = effectiveFrame->loader().client();
    client->didDisplayInsecureContent();

    String message = String::format(
        "Mixed Content: The page at '%s' was loaded over HTTPS, but contains a form whose 'action' attribute is '%s'. This form should not submit data to insecure endpoints.",
        effectiveFrame->document()->url().elidedString().utf8().data(), url.elidedString().utf8().data());
    effectiveFrame->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, WarningMessageLevel, message));
    return true;
}

} // namespace blink
