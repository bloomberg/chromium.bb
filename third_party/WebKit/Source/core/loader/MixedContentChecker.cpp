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

    // Contexts that we should block, but don't currently.
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
bool MixedContentChecker::shouldBlockFetch(LocalFrame* frame, const ResourceRequest& resourceRequest, const KURL& url)
{
    // No frame, no mixed content:
    if (!frame)
        return false;

    // Check the top frame first.
    if (Frame* top = frame->tree().top()) {
        // FIXME: We need a way to access the top-level frame's SecurityOrigin when that frame
        // is in a different process from the current frame. Until that is done, we bail out
        // early and allow the load.
        if (!top->isLocalFrame())
            return false;

        LocalFrame* localTop = toLocalFrame(top);
        if (frame != localTop && shouldBlockFetch(localTop, resourceRequest, url))
            return true;
    }

    // We only care about subresource loads; top-level navigations cannot be mixed content.
    if (resourceRequest.frameType() == WebURLRequest::FrameTypeTopLevel)
        return false;

    // No mixed content, no problem.
    if (!isMixedContent(frame->document()->securityOrigin(), url))
        return false;

    Settings* settings = frame->settings();
    FrameLoaderClient* client = frame->loader().client();
    SecurityOrigin* securityOrigin = frame->document()->securityOrigin();
    bool allowed = false;

    switch (contextTypeFromContext(resourceRequest.requestContext())) {
    case ContextTypeOptionallyBlockable:
        allowed = client->allowDisplayingInsecureContent(settings && settings->allowDisplayOfInsecureContent(), securityOrigin, url);
        if (allowed)
            client->didDisplayInsecureContent();
        return !allowed;

    case ContextTypeBlockable:
        allowed = client->allowRunningInsecureContent(settings && settings->allowRunningOfInsecureContent(), securityOrigin, url);
        if (allowed)
            client->didRunInsecureContent(securityOrigin, url);
        return !allowed;

    case ContextTypeBlockableUnlessLax:
        if (RuntimeEnabledFeatures::laxMixedContentCheckingEnabled()) {
            allowed = client->allowDisplayingInsecureContent(settings && settings->allowDisplayOfInsecureContent(), securityOrigin, url);
            if (allowed)
                client->didDisplayInsecureContent();
        } else {
            allowed = client->allowRunningInsecureContent(settings && settings->allowRunningOfInsecureContent(), securityOrigin, url);
            if (allowed)
                client->didRunInsecureContent(securityOrigin, url);
        }
        return !allowed;

    case ContextTypeShouldBeBlockable:
        return false;
    };
    ASSERT_NOT_REACHED();
    return true;
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

} // namespace blink
