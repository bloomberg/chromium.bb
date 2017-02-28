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

#include "core/loader/MixedContentChecker.h"

#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/NetworkUtils.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/WebMixedContent.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

// When a frame is local, use its full URL to represent the main resource. When
// the frame is remote, the full URL isn't accessible, so use the origin. This
// function is used, for example, to determine the URL to show in console
// messages about mixed content.
KURL mainResourceUrlForFrame(Frame* frame) {
  if (frame->isRemoteFrame()) {
    return KURL(KURL(),
                frame->securityContext()->getSecurityOrigin()->toString());
  }
  return toLocalFrame(frame)->document()->url();
}

const char* requestContextName(WebURLRequest::RequestContext context) {
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
  NOTREACHED();
  return "resource";
}

}  // namespace

static void measureStricterVersionOfIsMixedContent(Frame* frame,
                                                   const KURL& url) {
  // We're currently only checking for mixed content in `https://*` contexts.
  // What about other "secure" contexts the SchemeRegistry knows about? We'll
  // use this method to measure the occurrence of non-webby mixed content to
  // make sure we're not breaking the world without realizing it.
  SecurityOrigin* origin = frame->securityContext()->getSecurityOrigin();
  if (MixedContentChecker::isMixedContent(origin, url)) {
    if (origin->protocol() != "https") {
      UseCounter::count(
          frame,
          UseCounter::MixedContentInNonHTTPSFrameThatRestrictsMixedContent);
    }
  } else if (!SecurityOrigin::isSecure(url) &&
             SchemeRegistry::shouldTreatURLSchemeAsSecure(origin->protocol())) {
    UseCounter::count(
        frame,
        UseCounter::MixedContentInSecureFrameThatDoesNotRestrictMixedContent);
  }
}

bool requestIsSubframeSubresource(Frame* frame,
                                  WebURLRequest::FrameType frameType) {
  return (frame && frame != frame->tree().top() &&
          frameType != WebURLRequest::FrameTypeNested);
}

// static
bool MixedContentChecker::isMixedContent(SecurityOrigin* securityOrigin,
                                         const KURL& url) {
  if (!SchemeRegistry::shouldTreatURLSchemeAsRestrictingMixedContent(
          securityOrigin->protocol()))
    return false;

  // |url| is mixed content if its origin is not potentially trustworthy nor
  // secure. We do a quick check against `SecurityOrigin::isSecure` to catch
  // things like `about:blank`, which cannot be sanely passed into
  // `SecurityOrigin::create` (as their origin depends on their context).
  // blob: and filesystem: URLs never hit the network, and access is restricted
  // to same-origin contexts, so they are not blocked either.
  bool isAllowed = url.protocolIs("blob") || url.protocolIs("filesystem") ||
                   SecurityOrigin::isSecure(url) ||
                   SecurityOrigin::create(url)->isPotentiallyTrustworthy();
  // TODO(mkwst): Remove this once 'localhost' is no longer considered
  // potentially trustworthy.
  if (isAllowed && url.protocolIs("http") &&
      NetworkUtils::isLocalHostname(url.host(), nullptr))
    isAllowed = false;
  return !isAllowed;
}

// static
Frame* MixedContentChecker::inWhichFrameIsContentMixed(
    Frame* frame,
    WebURLRequest::FrameType frameType,
    const KURL& url) {
  // We only care about subresource loads; top-level navigations cannot be mixed
  // content. Neither can frameless requests.
  if (frameType == WebURLRequest::FrameTypeTopLevel || !frame)
    return nullptr;

  // Check the top frame first.
  if (Frame* top = frame->tree().top()) {
    measureStricterVersionOfIsMixedContent(top, url);
    if (isMixedContent(top->securityContext()->getSecurityOrigin(), url))
      return top;
  }

  measureStricterVersionOfIsMixedContent(frame, url);
  if (isMixedContent(frame->securityContext()->getSecurityOrigin(), url))
    return frame;

  // No mixed content, no problem.
  return nullptr;
}

// static
void MixedContentChecker::logToConsoleAboutFetch(
    LocalFrame* frame,
    const KURL& mainResourceUrl,
    const KURL& url,
    WebURLRequest::RequestContext requestContext,
    bool allowed) {
  String message = String::format(
      "Mixed Content: The page at '%s' was loaded over HTTPS, but requested an "
      "insecure %s '%s'. %s",
      mainResourceUrl.elidedString().utf8().data(),
      requestContextName(requestContext), url.elidedString().utf8().data(),
      allowed ? "This content should also be served over HTTPS."
              : "This request has been blocked; the content must be served "
                "over HTTPS.");
  MessageLevel messageLevel = allowed ? WarningMessageLevel : ErrorMessageLevel;
  frame->document()->addConsoleMessage(
      ConsoleMessage::create(SecurityMessageSource, messageLevel, message));
}

// static
void MixedContentChecker::count(Frame* frame,
                                WebURLRequest::RequestContext requestContext) {
  UseCounter::count(frame, UseCounter::MixedContentPresent);

  // Roll blockable content up into a single counter, count unblocked types
  // individually so we can determine when they can be safely moved to the
  // blockable category:
  WebMixedContentContextType contextType =
      WebMixedContent::contextTypeFromRequestContext(
          requestContext,
          frame->settings()->getStrictMixedContentCheckingForPlugin());
  if (contextType == WebMixedContentContextType::Blockable) {
    UseCounter::count(frame, UseCounter::MixedContentBlockable);
    return;
  }

  UseCounter::Feature feature;
  switch (requestContext) {
    case WebURLRequest::RequestContextAudio:
      feature = UseCounter::MixedContentAudio;
      break;
    case WebURLRequest::RequestContextDownload:
      feature = UseCounter::MixedContentDownload;
      break;
    case WebURLRequest::RequestContextFavicon:
      feature = UseCounter::MixedContentFavicon;
      break;
    case WebURLRequest::RequestContextImage:
      feature = UseCounter::MixedContentImage;
      break;
    case WebURLRequest::RequestContextInternal:
      feature = UseCounter::MixedContentInternal;
      break;
    case WebURLRequest::RequestContextPlugin:
      feature = UseCounter::MixedContentPlugin;
      break;
    case WebURLRequest::RequestContextPrefetch:
      feature = UseCounter::MixedContentPrefetch;
      break;
    case WebURLRequest::RequestContextVideo:
      feature = UseCounter::MixedContentVideo;
      break;

    default:
      NOTREACHED();
      return;
  }
  UseCounter::count(frame, feature);
}

// static
bool MixedContentChecker::shouldBlockFetch(
    LocalFrame* frame,
    WebURLRequest::RequestContext requestContext,
    WebURLRequest::FrameType frameType,
    ResourceRequest::RedirectStatus redirectStatus,
    const KURL& url,
    SecurityViolationReportingPolicy reportingPolicy) {
  // Frame-level loads are checked by the browser if PlzNavigate is enabled. No
  // need to check them again here.
  if (frame->settings()->getBrowserSideNavigationEnabled() &&
      frameType != WebURLRequest::FrameTypeNone) {
    return false;
  }

  Frame* effectiveFrame = effectiveFrameForFrameType(frame, frameType);
  Frame* mixedFrame =
      inWhichFrameIsContentMixed(effectiveFrame, frameType, url);
  if (!mixedFrame)
    return false;

  MixedContentChecker::count(mixedFrame, requestContext);
  if (ContentSecurityPolicy* policy =
          frame->securityContext()->contentSecurityPolicy())
    policy->reportMixedContent(url, redirectStatus);

  Settings* settings = mixedFrame->settings();
  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  LocalFrameClient* client = frame->loader().client();
  SecurityOrigin* securityOrigin =
      mixedFrame->securityContext()->getSecurityOrigin();
  bool allowed = false;

  // If we're in strict mode, we'll automagically fail everything, and
  // intentionally skip the client checks in order to prevent degrading the
  // site's security UI.
  bool strictMode = mixedFrame->securityContext()->getInsecureRequestPolicy() &
                        kBlockAllMixedContent ||
                    settings->getStrictMixedContentChecking();

  WebMixedContentContextType contextType =
      WebMixedContent::contextTypeFromRequestContext(
          requestContext, settings->getStrictMixedContentCheckingForPlugin());

  // If we're loading the main resource of a subframe, we need to take a close
  // look at the loaded URL. If we're dealing with a CORS-enabled scheme, then
  // block mixed frames as active content. Otherwise, treat frames as passive
  // content.
  //
  // FIXME: Remove this temporary hack once we have a reasonable API for
  // launching external applications via URLs. http://crbug.com/318788 and
  // https://crbug.com/393481
  if (frameType == WebURLRequest::FrameTypeNested &&
      !SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(url.protocol()))
    contextType = WebMixedContentContextType::OptionallyBlockable;

  switch (contextType) {
    case WebMixedContentContextType::OptionallyBlockable:
      allowed = !strictMode;
      if (allowed) {
        client->passiveInsecureContentFound(url);
        client->didDisplayInsecureContent();
      }
      break;

    case WebMixedContentContextType::Blockable: {
      // Strictly block subresources that are mixed with respect to their
      // subframes, unless all insecure content is allowed. This is to avoid the
      // following situation: https://a.com embeds https://b.com, which loads a
      // script over insecure HTTP. The user opts to allow the insecure content,
      // thinking that they are allowing an insecure script to run on
      // https://a.com and not realizing that they are in fact allowing an
      // insecure script on https://b.com.
      if (!settings->getAllowRunningOfInsecureContent() &&
          requestIsSubframeSubresource(effectiveFrame, frameType) &&
          isMixedContent(frame->securityContext()->getSecurityOrigin(), url)) {
        UseCounter::count(mixedFrame,
                          UseCounter::BlockableMixedContentInSubframeBlocked);
        allowed = false;
        break;
      }

      bool shouldAskEmbedder =
          !strictMode && settings &&
          (!settings->getStrictlyBlockBlockableMixedContent() ||
           settings->getAllowRunningOfInsecureContent());
      allowed = shouldAskEmbedder &&
                client->allowRunningInsecureContent(
                    settings && settings->getAllowRunningOfInsecureContent(),
                    securityOrigin, url);
      if (allowed) {
        client->didRunInsecureContent(securityOrigin, url);
        UseCounter::count(mixedFrame, UseCounter::MixedContentBlockableAllowed);
      }
      break;
    }

    case WebMixedContentContextType::ShouldBeBlockable:
      allowed = !strictMode;
      if (allowed)
        client->didDisplayInsecureContent();
      break;
    case WebMixedContentContextType::NotMixedContent:
      NOTREACHED();
      break;
  };

  if (reportingPolicy == SecurityViolationReportingPolicy::Report) {
    logToConsoleAboutFetch(frame, mainResourceUrlForFrame(mixedFrame), url,
                           requestContext, allowed);
  }
  return !allowed;
}

// static
void MixedContentChecker::logToConsoleAboutWebSocket(
    LocalFrame* frame,
    const KURL& mainResourceUrl,
    const KURL& url,
    bool allowed) {
  String message = String::format(
      "Mixed Content: The page at '%s' was loaded over HTTPS, but attempted to "
      "connect to the insecure WebSocket endpoint '%s'. %s",
      mainResourceUrl.elidedString().utf8().data(),
      url.elidedString().utf8().data(),
      allowed ? "This endpoint should be available via WSS. Insecure access is "
                "deprecated."
              : "This request has been blocked; this endpoint must be "
                "available over WSS.");
  MessageLevel messageLevel = allowed ? WarningMessageLevel : ErrorMessageLevel;
  frame->document()->addConsoleMessage(
      ConsoleMessage::create(SecurityMessageSource, messageLevel, message));
}

// static
bool MixedContentChecker::shouldBlockWebSocket(
    LocalFrame* frame,
    const KURL& url,
    SecurityViolationReportingPolicy reportingPolicy) {
  Frame* mixedFrame =
      inWhichFrameIsContentMixed(frame, WebURLRequest::FrameTypeNone, url);
  if (!mixedFrame)
    return false;

  UseCounter::count(mixedFrame, UseCounter::MixedContentPresent);
  UseCounter::count(mixedFrame, UseCounter::MixedContentWebSocket);
  if (ContentSecurityPolicy* policy =
          frame->securityContext()->contentSecurityPolicy()) {
    policy->reportMixedContent(url,
                               ResourceRequest::RedirectStatus::NoRedirect);
  }

  Settings* settings = mixedFrame->settings();
  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  LocalFrameClient* client = frame->loader().client();
  SecurityOrigin* securityOrigin =
      mixedFrame->securityContext()->getSecurityOrigin();
  bool allowed = false;

  // If we're in strict mode, we'll automagically fail everything, and
  // intentionally skip the client checks in order to prevent degrading the
  // site's security UI.
  bool strictMode = mixedFrame->securityContext()->getInsecureRequestPolicy() &
                        kBlockAllMixedContent ||
                    settings->getStrictMixedContentChecking();
  if (!strictMode) {
    bool allowedPerSettings =
        settings && settings->getAllowRunningOfInsecureContent();
    allowed = client->allowRunningInsecureContent(allowedPerSettings,
                                                  securityOrigin, url);
  }

  if (allowed)
    client->didRunInsecureContent(securityOrigin, url);

  if (reportingPolicy == SecurityViolationReportingPolicy::Report) {
    logToConsoleAboutWebSocket(frame, mainResourceUrlForFrame(mixedFrame), url,
                               allowed);
  }
  return !allowed;
}

bool MixedContentChecker::isMixedFormAction(
    LocalFrame* frame,
    const KURL& url,
    SecurityViolationReportingPolicy reportingPolicy) {
  // For whatever reason, some folks handle forms via JavaScript, and submit to
  // `javascript:void(0)` rather than calling `preventDefault()`. We
  // special-case `javascript:` URLs here, as they don't introduce MixedContent
  // for form submissions.
  if (url.protocolIs("javascript"))
    return false;

  Frame* mixedFrame =
      inWhichFrameIsContentMixed(frame, WebURLRequest::FrameTypeNone, url);
  if (!mixedFrame)
    return false;

  UseCounter::count(mixedFrame, UseCounter::MixedContentPresent);

  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  frame->loader().client()->didDisplayInsecureContent();

  if (reportingPolicy == SecurityViolationReportingPolicy::Report) {
    String message = String::format(
        "Mixed Content: The page at '%s' was loaded over a secure connection, "
        "but contains a form which targets an insecure endpoint '%s'. This "
        "endpoint should be made available over a secure connection.",
        mainResourceUrlForFrame(mixedFrame).elidedString().utf8().data(),
        url.elidedString().utf8().data());
    frame->document()->addConsoleMessage(ConsoleMessage::create(
        SecurityMessageSource, WarningMessageLevel, message));
  }

  return true;
}

void MixedContentChecker::checkMixedPrivatePublic(
    LocalFrame* frame,
    const AtomicString& resourceIPAddress) {
  if (!frame || !frame->document() || !frame->document()->loader())
    return;

  // Just count these for the moment, don't block them.
  if (NetworkUtils::isReservedIPAddress(resourceIPAddress) &&
      frame->document()->addressSpace() == WebAddressSpacePublic) {
    UseCounter::count(frame->document(),
                      UseCounter::MixedContentPrivateHostnameInPublicHostname);
    // We can simplify the IP checks here, as we've already verified that
    // |resourceIPAddress| is a reserved IP address, which means it's also a
    // valid IP address in a normalized form.
    if (resourceIPAddress.startsWith("127.0.0.") ||
        resourceIPAddress == "[::1]") {
      UseCounter::count(frame->document(),
                        frame->document()->isSecureContext()
                            ? UseCounter::LoopbackEmbeddedInSecureContext
                            : UseCounter::LoopbackEmbeddedInNonSecureContext);
    }
  }
}

Frame* MixedContentChecker::effectiveFrameForFrameType(
    LocalFrame* frame,
    WebURLRequest::FrameType frameType) {
  // If we're loading the main resource of a subframe, ensure that we check
  // against the parent of the active frame, rather than the frame itself.
  if (frameType != WebURLRequest::FrameTypeNested)
    return frame;

  Frame* parentFrame = frame->tree().parent();
  DCHECK(parentFrame);
  return parentFrame;
}

void MixedContentChecker::handleCertificateError(
    LocalFrame* frame,
    const ResourceResponse& response,
    WebURLRequest::FrameType frameType,
    WebURLRequest::RequestContext requestContext) {
  Frame* effectiveFrame = effectiveFrameForFrameType(frame, frameType);
  if (frameType == WebURLRequest::FrameTypeTopLevel || !effectiveFrame)
    return;

  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  LocalFrameClient* client = frame->loader().client();
  bool strictMixedContentCheckingForPlugin =
      effectiveFrame->settings() &&
      effectiveFrame->settings()->getStrictMixedContentCheckingForPlugin();
  WebMixedContentContextType contextType =
      WebMixedContent::contextTypeFromRequestContext(
          requestContext, strictMixedContentCheckingForPlugin);
  if (contextType == WebMixedContentContextType::Blockable) {
    client->didRunContentWithCertificateErrors(response.url());
  } else {
    // contextTypeFromRequestContext() never returns NotMixedContent (it
    // computes the type of mixed content, given that the content is mixed).
    DCHECK_NE(contextType, WebMixedContentContextType::NotMixedContent);
    client->didDisplayContentWithCertificateErrors(response.url());
  }
}

// static
void MixedContentChecker::mixedContentFound(
    LocalFrame* frame,
    const KURL& mainResourceUrl,
    const KURL& mixedContentUrl,
    WebURLRequest::RequestContext requestContext,
    bool wasAllowed,
    bool hadRedirect) {
  // Logs to the frame console.
  logToConsoleAboutFetch(frame, mainResourceUrl, mixedContentUrl,
                         requestContext, wasAllowed);
  // Reports to the CSP policy.
  ContentSecurityPolicy* policy =
      frame->securityContext()->contentSecurityPolicy();
  if (policy) {
    policy->reportMixedContent(
        mixedContentUrl, hadRedirect
                             ? ResourceRequest::RedirectStatus::FollowedRedirect
                             : ResourceRequest::RedirectStatus::NoRedirect);
  }
}

WebMixedContentContextType MixedContentChecker::contextTypeForInspector(
    LocalFrame* frame,
    const ResourceRequest& request) {
  Frame* effectiveFrame =
      effectiveFrameForFrameType(frame, request.frameType());

  Frame* mixedFrame = inWhichFrameIsContentMixed(
      effectiveFrame, request.frameType(), request.url());
  if (!mixedFrame)
    return WebMixedContentContextType::NotMixedContent;

  // See comment in shouldBlockFetch() about loading the main resource of a
  // subframe.
  if (request.frameType() == WebURLRequest::FrameTypeNested &&
      !SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(
          request.url().protocol())) {
    return WebMixedContentContextType::OptionallyBlockable;
  }

  bool strictMixedContentCheckingForPlugin =
      mixedFrame->settings() &&
      mixedFrame->settings()->getStrictMixedContentCheckingForPlugin();
  return WebMixedContent::contextTypeFromRequestContext(
      request.requestContext(), strictMixedContentCheckingForPlugin);
}

}  // namespace blink
