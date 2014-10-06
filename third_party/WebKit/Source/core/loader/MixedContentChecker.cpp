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
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {
} // namespace

MixedContentChecker::MixedContentChecker(LocalFrame* frame)
    : m_frame(frame)
{
}

FrameLoaderClient* MixedContentChecker::client() const
{
    return m_frame->loader().client();
}

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
    String message = String::format(
        "Mixed Content: The page at '%s' was loaded over HTTPS, but requested an insecure %s '%s'. %s",
        frame->document()->url().elidedString().utf8().data(), typeNameFromContext(requestContext), url.elidedString().utf8().data(),
        allowed ? "This content should also be served over HTTPS." : "This request has been blocked; the content must be served over HTTPS.");
    MessageLevel messageLevel = allowed ? WarningMessageLevel : ErrorMessageLevel;
    frame->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, messageLevel, message));
}

LocalFrame* MixedContentChecker::inWhichFrameIsThisContentMixed(LocalFrame* frame, WebURLRequest::RequestContext requestContext, WebURLRequest::FrameType frameType, const KURL& url)
{
    // No frame, no mixed content:
    if (!frame)
        return nullptr;

    // We only care about subresource loads; top-level navigations cannot be mixed content.
    if (frameType == WebURLRequest::FrameTypeTopLevel)
        return nullptr;

    // Check the top frame first.
    if (Frame* top = frame->tree().top()) {
        // FIXME: We need a way to access the top-level frame's SecurityOrigin when that frame
        // is in a different process from the current frame. Until that is done, we bail out
        // early and allow the load.
        if (!top->isLocalFrame())
            return nullptr;

        LocalFrame* localTop = toLocalFrame(top);
        if (frame != localTop && inWhichFrameIsThisContentMixed(localTop, requestContext, frameType, url))
            return localTop;
    }

    // Just count these for the moment, don't block them.
    if (Platform::current()->isReservedIPAddress(url) && !Platform::current()->isReservedIPAddress(KURL(ParsedURLString, frame->document()->securityOrigin()->toString())))
        UseCounter::count(frame->document(), contextTypeFromContext(requestContext) == ContextTypeBlockable ? UseCounter::MixedContentPrivateIPInPublicWebsiteActive : UseCounter::MixedContentPrivateIPInPublicWebsitePassive);

    // No mixed content, no problem.
    if (!isMixedContent(frame->document()->securityOrigin(), url))
        return nullptr;

    return frame;
}

// static
bool MixedContentChecker::shouldBlockFetch(LocalFrame* frame, WebURLRequest::RequestContext requestContext, WebURLRequest::FrameType frameType, const KURL& url)
{
    LocalFrame* effectiveFrame = inWhichFrameIsThisContentMixed(frame, requestContext, frameType, url);
    if (!effectiveFrame)
        return false;

    // We grab the settings and client from the frame in which the content was mixed, as it might be
    // configured to allow mixed content in a different way than the frame in which the content
    // loads (e.g. if Frame A is allowed to frame an insecure Frame B, we defer to Frame A's settings
    // when evaluating Frame B's subresource loads). Yes, this is confusing.
    Settings* settings = effectiveFrame->settings();
    FrameLoaderClient* client = effectiveFrame->loader().client();
    SecurityOrigin* securityOrigin = effectiveFrame->document()->securityOrigin();
    bool allowed = false;

    ContextType contextType = contextTypeFromContext(requestContext);
    if (contextType == ContextTypeBlockableUnlessLax)
        contextType = RuntimeEnabledFeatures::laxMixedContentCheckingEnabled() ? ContextTypeOptionallyBlockable : ContextTypeBlockable;

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

    // While we use the |effectiveFrame| to grab the settings object, we log the console error in
    // |frame|, where the violation actually happened.
    String message = String::format(
        "Mixed Content: The page at '%s' was loaded over HTTPS, but requested an insecure %s '%s'. %s",
        frame->document()->url().elidedString().utf8().data(), typeNameFromContext(requestContext), url.elidedString().utf8().data(),
        allowed ? "This content should also be served over HTTPS." : "This request has been blocked; the content must be served over HTTPS.");
    MessageLevel messageLevel = allowed ? WarningMessageLevel : ErrorMessageLevel;
    frame->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, messageLevel, message));

    return !allowed;
}

bool MixedContentChecker::canDisplayInsecureContentInternal(SecurityOrigin* securityOrigin, const KURL& url, const MixedContentType type) const
{
    // Check the top frame if it differs from MixedContentChecker's m_frame.
    if (!m_frame->tree().top()->isLocalFrame()) {
        // FIXME: We need a way to access the top-level frame's MixedContentChecker when that frame
        // is in a different process from the current frame. Until that is done, we always allow
        // loads in remote frames.
        return false;
    }
    Frame* top = m_frame->tree().top();
    if (top != m_frame && !toLocalFrame(top)->loader().mixedContentChecker()->canDisplayInsecureContent(toLocalFrame(top)->document()->securityOrigin(), url))
        return false;

    // Just count these for the moment, don't block them.
    if (Platform::current()->isReservedIPAddress(url) && !Platform::current()->isReservedIPAddress(KURL(ParsedURLString, securityOrigin->toString())))
        UseCounter::count(m_frame->document(), UseCounter::MixedContentPrivateIPInPublicWebsitePassive);

    // Then check the current frame:
    if (!isMixedContent(securityOrigin, url))
        return true;

    Settings* settings = m_frame->settings();
    bool allowed = client()->allowDisplayingInsecureContent(settings && settings->allowDisplayOfInsecureContent(), securityOrigin, url);
    logWarning(allowed, url, type);

    if (allowed)
        client()->didDisplayInsecureContent();

    return allowed;
}

bool MixedContentChecker::canRunInsecureContentInternal(SecurityOrigin* securityOrigin, const KURL& url, const MixedContentType type) const
{
    // Check the top frame if it differs from MixedContentChecker's m_frame.
    if (!m_frame->tree().top()->isLocalFrame()) {
        // FIXME: We need a way to access the top-level frame's MixedContentChecker when that frame
        // is in a different process from the current frame. Until that is done, we always allow
        // loads in remote frames.
        return true;
    }
    Frame* top = m_frame->tree().top();
    if (top != m_frame && !toLocalFrame(top)->loader().mixedContentChecker()->canRunInsecureContent(toLocalFrame(top)->document()->securityOrigin(), url))
        return false;

    // Just count these for the moment, don't block them.
    if (Platform::current()->isReservedIPAddress(url) && !Platform::current()->isReservedIPAddress(KURL(ParsedURLString, securityOrigin->toString())))
        UseCounter::count(m_frame->document(), UseCounter::MixedContentPrivateIPInPublicWebsiteActive);

    // Then check the current frame:
    if (!isMixedContent(securityOrigin, url))
        return true;

    Settings* settings = m_frame->settings();
    bool allowedPerSettings = settings && (settings->allowRunningOfInsecureContent() || ((type == WebSocket) && settings->allowConnectingInsecureWebSocket()));
    bool allowed = client()->allowRunningInsecureContent(allowedPerSettings, securityOrigin, url);
    logWarning(allowed, url, type);

    if (allowed)
        client()->didRunInsecureContent(securityOrigin, url);

    return allowed;
}

bool MixedContentChecker::canFrameInsecureContent(SecurityOrigin* securityOrigin, const KURL& url) const
{
    // If we're dealing with a CORS-enabled scheme, then block mixed frames as active content. Otherwise,
    // treat frames as passive content.
    //
    // FIXME: Remove this temporary hack once we have a reasonable API for launching external applications
    // via URLs. http://crbug.com/318788 and https://crbug.com/393481
    if (SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(url.protocol()))
        return canRunInsecureContentInternal(securityOrigin, url, MixedContentChecker::Execution);
    return canDisplayInsecureContentInternal(securityOrigin, url, MixedContentChecker::Display);
}

bool MixedContentChecker::canConnectInsecureWebSocket(SecurityOrigin* securityOrigin, const KURL& url) const
{
    if (RuntimeEnabledFeatures::laxMixedContentCheckingEnabled())
        return canDisplayInsecureContentInternal(securityOrigin, url, MixedContentChecker::WebSocket);
    return canRunInsecureContentInternal(securityOrigin, url, MixedContentChecker::WebSocket);
}

bool MixedContentChecker::canSubmitToInsecureForm(SecurityOrigin* securityOrigin, const KURL& url) const
{
    // For whatever reason, some folks handle forms via JavaScript, and submit to `javascript:void(0)`
    // rather than calling `preventDefault()`. We special-case `javascript:` URLs here, as they don't
    // introduce MixedContent for form submissions.
    if (url.protocolIs("javascript"))
        return true;

    // If lax mixed content checking is enabled (noooo!), skip this check entirely.
    if (RuntimeEnabledFeatures::laxMixedContentCheckingEnabled())
        return true;
    return canDisplayInsecureContentInternal(securityOrigin, url, MixedContentChecker::Submission);
}

void MixedContentChecker::logWarning(bool allowed, const KURL& target, const MixedContentType type) const
{
    StringBuilder message;
    message.append((allowed ? "" : "[blocked] "));
    message.append("The page at '" + m_frame->document()->url().elidedString() + "' was loaded over HTTPS, but ");
    switch (type) {
    case Display:
        message.append("displayed insecure content from '" + target.elidedString() + "': this content should also be loaded over HTTPS.\n");
        break;
    case Execution:
    case WebSocket:
        message.append("ran insecure content from '" + target.elidedString() + "': this content should also be loaded over HTTPS.\n");
        break;
    case Submission:
        message.append("is submitting data to an insecure location at '" + target.elidedString() + "': this content should also be submitted over HTTPS.\n");
        break;
    }
    MessageLevel messageLevel = allowed ? WarningMessageLevel : ErrorMessageLevel;
    m_frame->document()->addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, messageLevel, message.toString()));
}

void MixedContentChecker::checkMixedPrivatePublic(LocalFrame* frame, const AtomicString& resourceIPAddress)
{
    if (!frame || !frame->document() || !frame->document()->loader())
        return;

    KURL documentIP(ParsedURLString, "http://" + frame->document()->loader()->response().remoteIPAddress());
    KURL resourceIP(ParsedURLString, "http://" + resourceIPAddress);

    // Just count these for the moment, don't block them.
    //
    // FIXME: Once we know how we want to check this, adjust the platform APIs to avoid the KURL construction.
    if (Platform::current()->isReservedIPAddress(resourceIP) && !Platform::current()->isReservedIPAddress(documentIP))
        UseCounter::count(frame->document(), UseCounter::MixedContentPrivateHostnameInPublicHostname);
}

void MixedContentChecker::trace(Visitor* visitor)
{
    visitor->trace(m_frame);
}

} // namespace blink
