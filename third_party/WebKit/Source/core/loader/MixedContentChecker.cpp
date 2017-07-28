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
#include "core/frame/ContentSettingsClient.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "core/workers/WorkerContentSettingsClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerSettings.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/NetworkUtils.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/WebMixedContent.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebWorkerFetchContext.h"

namespace blink {

namespace {

// When a frame is local, use its full URL to represent the main resource. When
// the frame is remote, the full URL isn't accessible, so use the origin. This
// function is used, for example, to determine the URL to show in console
// messages about mixed content.
KURL MainResourceUrlForFrame(Frame* frame) {
  if (frame->IsRemoteFrame()) {
    return KURL(NullURL(),
                frame->GetSecurityContext()->GetSecurityOrigin()->ToString());
  }
  return ToLocalFrame(frame)->GetDocument()->Url();
}

const char* RequestContextName(WebURLRequest::RequestContext context) {
  switch (context) {
    case WebURLRequest::kRequestContextAudio:
      return "audio file";
    case WebURLRequest::kRequestContextBeacon:
      return "Beacon endpoint";
    case WebURLRequest::kRequestContextCSPReport:
      return "Content Security Policy reporting endpoint";
    case WebURLRequest::kRequestContextDownload:
      return "download";
    case WebURLRequest::kRequestContextEmbed:
      return "plugin resource";
    case WebURLRequest::kRequestContextEventSource:
      return "EventSource endpoint";
    case WebURLRequest::kRequestContextFavicon:
      return "favicon";
    case WebURLRequest::kRequestContextFetch:
      return "resource";
    case WebURLRequest::kRequestContextFont:
      return "font";
    case WebURLRequest::kRequestContextForm:
      return "form action";
    case WebURLRequest::kRequestContextFrame:
      return "frame";
    case WebURLRequest::kRequestContextHyperlink:
      return "resource";
    case WebURLRequest::kRequestContextIframe:
      return "frame";
    case WebURLRequest::kRequestContextImage:
      return "image";
    case WebURLRequest::kRequestContextImageSet:
      return "image";
    case WebURLRequest::kRequestContextImport:
      return "HTML Import";
    case WebURLRequest::kRequestContextInternal:
      return "resource";
    case WebURLRequest::kRequestContextLocation:
      return "resource";
    case WebURLRequest::kRequestContextManifest:
      return "manifest";
    case WebURLRequest::kRequestContextObject:
      return "plugin resource";
    case WebURLRequest::kRequestContextPing:
      return "hyperlink auditing endpoint";
    case WebURLRequest::kRequestContextPlugin:
      return "plugin data";
    case WebURLRequest::kRequestContextPrefetch:
      return "prefetch resource";
    case WebURLRequest::kRequestContextScript:
      return "script";
    case WebURLRequest::kRequestContextServiceWorker:
      return "Service Worker script";
    case WebURLRequest::kRequestContextSharedWorker:
      return "Shared Worker script";
    case WebURLRequest::kRequestContextStyle:
      return "stylesheet";
    case WebURLRequest::kRequestContextSubresource:
      return "resource";
    case WebURLRequest::kRequestContextTrack:
      return "Text Track";
    case WebURLRequest::kRequestContextUnspecified:
      return "resource";
    case WebURLRequest::kRequestContextVideo:
      return "video";
    case WebURLRequest::kRequestContextWorker:
      return "Worker script";
    case WebURLRequest::kRequestContextXMLHttpRequest:
      return "XMLHttpRequest endpoint";
    case WebURLRequest::kRequestContextXSLT:
      return "XSLT";
  }
  NOTREACHED();
  return "resource";
}

}  // namespace

static void MeasureStricterVersionOfIsMixedContent(Frame& frame,
                                                   const KURL& url,
                                                   const LocalFrame* source) {
  // We're currently only checking for mixed content in `https://*` contexts.
  // What about other "secure" contexts the SchemeRegistry knows about? We'll
  // use this method to measure the occurrence of non-webby mixed content to
  // make sure we're not breaking the world without realizing it.
  SecurityOrigin* origin = frame.GetSecurityContext()->GetSecurityOrigin();
  if (MixedContentChecker::IsMixedContent(origin, url)) {
    if (origin->Protocol() != "https") {
      UseCounter::Count(
          source,
          WebFeature::kMixedContentInNonHTTPSFrameThatRestrictsMixedContent);
    }
  } else if (!SecurityOrigin::IsSecure(url) &&
             SchemeRegistry::ShouldTreatURLSchemeAsSecure(origin->Protocol())) {
    UseCounter::Count(
        source,
        WebFeature::kMixedContentInSecureFrameThatDoesNotRestrictMixedContent);
  }
}

bool RequestIsSubframeSubresource(Frame* frame,
                                  WebURLRequest::FrameType frame_type) {
  return (frame && frame != frame->Tree().Top() &&
          frame_type != WebURLRequest::kFrameTypeNested);
}

// static
bool MixedContentChecker::IsMixedContent(SecurityOrigin* security_origin,
                                         const KURL& url) {
  if (!SchemeRegistry::ShouldTreatURLSchemeAsRestrictingMixedContent(
          security_origin->Protocol()))
    return false;

  // |url| is mixed content if its origin is not potentially trustworthy nor
  // secure. We do a quick check against `SecurityOrigin::isSecure` to catch
  // things like `about:blank`, which cannot be sanely passed into
  // `SecurityOrigin::create` (as their origin depends on their context).
  // blob: and filesystem: URLs never hit the network, and access is restricted
  // to same-origin contexts, so they are not blocked either.
  bool is_allowed = url.ProtocolIs("blob") || url.ProtocolIs("filesystem") ||
                    SecurityOrigin::IsSecure(url) ||
                    SecurityOrigin::Create(url)->IsPotentiallyTrustworthy();
  // TODO(mkwst): Remove this once 'localhost' is no longer considered
  // potentially trustworthy.
  if (is_allowed && url.ProtocolIs("http") &&
      NetworkUtils::IsLocalHostname(url.Host(), nullptr))
    is_allowed = false;
  return !is_allowed;
}

// static
Frame* MixedContentChecker::InWhichFrameIsContentMixed(
    Frame* frame,
    WebURLRequest::FrameType frame_type,
    const KURL& url,
    const LocalFrame* source) {
  // We only care about subresource loads; top-level navigations cannot be mixed
  // content. Neither can frameless requests.
  if (frame_type == WebURLRequest::kFrameTypeTopLevel || !frame)
    return nullptr;

  // Check the top frame first.
  Frame& top = frame->Tree().Top();
  MeasureStricterVersionOfIsMixedContent(top, url, source);
  if (IsMixedContent(top.GetSecurityContext()->GetSecurityOrigin(), url))
    return &top;

  MeasureStricterVersionOfIsMixedContent(*frame, url, source);
  if (IsMixedContent(frame->GetSecurityContext()->GetSecurityOrigin(), url))
    return frame;

  // No mixed content, no problem.
  return nullptr;
}

// static
void MixedContentChecker::LogToConsoleAboutFetch(
    ExecutionContext* execution_context,
    const KURL& main_resource_url,
    const KURL& url,
    WebURLRequest::RequestContext request_context,
    bool allowed,
    std::unique_ptr<SourceLocation> source_location) {
  String message = String::Format(
      "Mixed Content: The page at '%s' was loaded over HTTPS, but requested an "
      "insecure %s '%s'. %s",
      main_resource_url.ElidedString().Utf8().data(),
      RequestContextName(request_context), url.ElidedString().Utf8().data(),
      allowed ? "This content should also be served over HTTPS."
              : "This request has been blocked; the content must be served "
                "over HTTPS.");
  MessageLevel message_level =
      allowed ? kWarningMessageLevel : kErrorMessageLevel;
  if (source_location) {
    execution_context->AddConsoleMessage(
        ConsoleMessage::Create(kSecurityMessageSource, message_level, message,
                               std::move(source_location)));
  } else {
    execution_context->AddConsoleMessage(
        ConsoleMessage::Create(kSecurityMessageSource, message_level, message));
  }
}

// static
void MixedContentChecker::Count(Frame* frame,
                                WebURLRequest::RequestContext request_context,
                                const LocalFrame* source) {
  UseCounter::Count(source, WebFeature::kMixedContentPresent);

  // Roll blockable content up into a single counter, count unblocked types
  // individually so we can determine when they can be safely moved to the
  // blockable category:
  WebMixedContentContextType context_type =
      WebMixedContent::ContextTypeFromRequestContext(
          request_context,
          frame->GetSettings()->GetStrictMixedContentCheckingForPlugin());
  if (context_type == WebMixedContentContextType::kBlockable) {
    UseCounter::Count(source, WebFeature::kMixedContentBlockable);
    return;
  }

  WebFeature feature;
  switch (request_context) {
    case WebURLRequest::kRequestContextAudio:
      feature = WebFeature::kMixedContentAudio;
      break;
    case WebURLRequest::kRequestContextDownload:
      feature = WebFeature::kMixedContentDownload;
      break;
    case WebURLRequest::kRequestContextFavicon:
      feature = WebFeature::kMixedContentFavicon;
      break;
    case WebURLRequest::kRequestContextImage:
      feature = WebFeature::kMixedContentImage;
      break;
    case WebURLRequest::kRequestContextInternal:
      feature = WebFeature::kMixedContentInternal;
      break;
    case WebURLRequest::kRequestContextPlugin:
      feature = WebFeature::kMixedContentPlugin;
      break;
    case WebURLRequest::kRequestContextPrefetch:
      feature = WebFeature::kMixedContentPrefetch;
      break;
    case WebURLRequest::kRequestContextVideo:
      feature = WebFeature::kMixedContentVideo;
      break;

    default:
      NOTREACHED();
      return;
  }
  UseCounter::Count(source, feature);
}

// static
bool MixedContentChecker::ShouldBlockFetch(
    LocalFrame* frame,
    WebURLRequest::RequestContext request_context,
    WebURLRequest::FrameType frame_type,
    ResourceRequest::RedirectStatus redirect_status,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) {
  // Frame-level loads are checked by the browser if PlzNavigate is enabled. No
  // need to check them again here.
  if (frame->GetSettings()->GetBrowserSideNavigationEnabled() &&
      frame_type != WebURLRequest::kFrameTypeNone) {
    return false;
  }

  Frame* effective_frame = EffectiveFrameForFrameType(frame, frame_type);
  Frame* mixed_frame =
      InWhichFrameIsContentMixed(effective_frame, frame_type, url, frame);
  if (!mixed_frame)
    return false;

  MixedContentChecker::Count(mixed_frame, request_context, frame);
  if (ContentSecurityPolicy* policy =
          frame->GetSecurityContext()->GetContentSecurityPolicy())
    policy->ReportMixedContent(url, redirect_status);

  Settings* settings = mixed_frame->GetSettings();
  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  LocalFrameClient* client = frame->Client();
  ContentSettingsClient* content_settings_client =
      frame->GetContentSettingsClient();
  SecurityOrigin* security_origin =
      mixed_frame->GetSecurityContext()->GetSecurityOrigin();
  bool allowed = false;

  // If we're in strict mode, we'll automagically fail everything, and
  // intentionally skip the client checks in order to prevent degrading the
  // site's security UI.
  bool strict_mode =
      mixed_frame->GetSecurityContext()->GetInsecureRequestPolicy() &
          kBlockAllMixedContent ||
      settings->GetStrictMixedContentChecking();

  WebMixedContentContextType context_type =
      WebMixedContent::ContextTypeFromRequestContext(
          request_context, settings->GetStrictMixedContentCheckingForPlugin());

  // If we're loading the main resource of a subframe, we need to take a close
  // look at the loaded URL. If we're dealing with a CORS-enabled scheme, then
  // block mixed frames as active content. Otherwise, treat frames as passive
  // content.
  //
  // FIXME: Remove this temporary hack once we have a reasonable API for
  // launching external applications via URLs. http://crbug.com/318788 and
  // https://crbug.com/393481
  if (frame_type == WebURLRequest::kFrameTypeNested &&
      !SchemeRegistry::ShouldTreatURLSchemeAsCORSEnabled(url.Protocol()))
    context_type = WebMixedContentContextType::kOptionallyBlockable;

  switch (context_type) {
    case WebMixedContentContextType::kOptionallyBlockable:
      allowed = !strict_mode;
      if (allowed) {
        content_settings_client->PassiveInsecureContentFound(url);
        client->DidDisplayInsecureContent();
      }
      break;

    case WebMixedContentContextType::kBlockable: {
      // Strictly block subresources that are mixed with respect to their
      // subframes, unless all insecure content is allowed. This is to avoid the
      // following situation: https://a.com embeds https://b.com, which loads a
      // script over insecure HTTP. The user opts to allow the insecure content,
      // thinking that they are allowing an insecure script to run on
      // https://a.com and not realizing that they are in fact allowing an
      // insecure script on https://b.com.
      if (!settings->GetAllowRunningOfInsecureContent() &&
          RequestIsSubframeSubresource(effective_frame, frame_type) &&
          IsMixedContent(frame->GetSecurityContext()->GetSecurityOrigin(),
                         url)) {
        UseCounter::Count(frame,
                          WebFeature::kBlockableMixedContentInSubframeBlocked);
        allowed = false;
        break;
      }

      bool should_ask_embedder =
          !strict_mode && settings &&
          (!settings->GetStrictlyBlockBlockableMixedContent() ||
           settings->GetAllowRunningOfInsecureContent());
      allowed = should_ask_embedder &&
                content_settings_client->AllowRunningInsecureContent(
                    settings && settings->GetAllowRunningOfInsecureContent(),
                    security_origin, url);
      if (allowed) {
        client->DidRunInsecureContent(security_origin, url);
        UseCounter::Count(frame, WebFeature::kMixedContentBlockableAllowed);
      }
      break;
    }

    case WebMixedContentContextType::kShouldBeBlockable:
      allowed = !strict_mode;
      if (allowed)
        client->DidDisplayInsecureContent();
      break;
    case WebMixedContentContextType::kNotMixedContent:
      NOTREACHED();
      break;
  };

  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    LogToConsoleAboutFetch(frame->GetDocument(),
                           MainResourceUrlForFrame(mixed_frame), url,
                           request_context, allowed, nullptr);
  }
  return !allowed;
}

// static
bool MixedContentChecker::ShouldBlockFetchOnWorker(
    WorkerOrWorkletGlobalScope* global_scope,
    WebWorkerFetchContext* worker_fetch_context,
    const ResourceRequest& request,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) {
  if (!MixedContentChecker::IsMixedContent(global_scope->GetSecurityOrigin(),
                                           url)) {
    return false;
  }

  UseCounter::Count(global_scope, WebFeature::kMixedContentPresent);
  UseCounter::Count(global_scope, WebFeature::kMixedContentBlockable);
  if (ContentSecurityPolicy* policy = global_scope->GetContentSecurityPolicy())
    policy->ReportMixedContent(url, request.GetRedirectStatus());

  // Blocks all mixed content request from worklets.
  // TODO(horo): Revise this when the spec is updated.
  // Worklets spec: https://www.w3.org/TR/worklets-1/#security-considerations
  // Spec issue: https://github.com/w3c/css-houdini-drafts/issues/92
  if (!global_scope->IsWorkerGlobalScope())
    return true;

  WorkerGlobalScope* worker_global_scope = ToWorkerGlobalScope(global_scope);
  WorkerSettings* settings = worker_global_scope->GetWorkerSettings();
  DCHECK(settings);
  bool allowed = false;
  if (!settings->GetAllowRunningOfInsecureContent() &&
      worker_fetch_context->IsOnSubframe()) {
    UseCounter::Count(global_scope,
                      WebFeature::kBlockableMixedContentInSubframeBlocked);
    allowed = false;
  } else {
    bool strict_mode = worker_global_scope->GetInsecureRequestPolicy() &
                           kBlockAllMixedContent ||
                       settings->GetStrictMixedContentChecking();
    bool should_ask_embedder =
        !strict_mode && (!settings->GetStrictlyBlockBlockableMixedContent() ||
                         settings->GetAllowRunningOfInsecureContent());
    allowed = should_ask_embedder &&
              WorkerContentSettingsClient::From(*global_scope)
                  ->AllowRunningInsecureContent(
                      settings->GetAllowRunningOfInsecureContent(),
                      global_scope->GetSecurityOrigin(), url);
    if (allowed) {
      worker_fetch_context->DidRunInsecureContent(
          WebSecurityOrigin(global_scope->GetSecurityOrigin()), url);
      UseCounter::Count(global_scope,
                        WebFeature::kMixedContentBlockableAllowed);
    }
  }

  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    LogToConsoleAboutFetch(global_scope, global_scope->Url(), url,
                           request.GetRequestContext(), allowed, nullptr);
  }
  return !allowed;
}

// static
void MixedContentChecker::LogToConsoleAboutWebSocket(
    LocalFrame* frame,
    const KURL& main_resource_url,
    const KURL& url,
    bool allowed) {
  String message = String::Format(
      "Mixed Content: The page at '%s' was loaded over HTTPS, but attempted to "
      "connect to the insecure WebSocket endpoint '%s'. %s",
      main_resource_url.ElidedString().Utf8().data(),
      url.ElidedString().Utf8().data(),
      allowed ? "This endpoint should be available via WSS. Insecure access is "
                "deprecated."
              : "This request has been blocked; this endpoint must be "
                "available over WSS.");
  MessageLevel message_level =
      allowed ? kWarningMessageLevel : kErrorMessageLevel;
  frame->GetDocument()->AddConsoleMessage(
      ConsoleMessage::Create(kSecurityMessageSource, message_level, message));
}

// static
bool MixedContentChecker::ShouldBlockWebSocket(
    LocalFrame* frame,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) {
  Frame* mixed_frame = InWhichFrameIsContentMixed(
      frame, WebURLRequest::kFrameTypeNone, url, frame);
  if (!mixed_frame)
    return false;

  UseCounter::Count(frame, WebFeature::kMixedContentPresent);
  UseCounter::Count(frame, WebFeature::kMixedContentWebSocket);
  if (ContentSecurityPolicy* policy =
          frame->GetSecurityContext()->GetContentSecurityPolicy()) {
    policy->ReportMixedContent(url,
                               ResourceRequest::RedirectStatus::kNoRedirect);
  }

  Settings* settings = mixed_frame->GetSettings();
  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  ContentSettingsClient* content_settings_client =
      frame->GetContentSettingsClient();
  LocalFrameClient* client = frame->Client();
  SecurityOrigin* security_origin =
      mixed_frame->GetSecurityContext()->GetSecurityOrigin();
  bool allowed = false;

  // If we're in strict mode, we'll automagically fail everything, and
  // intentionally skip the client checks in order to prevent degrading the
  // site's security UI.
  bool strict_mode =
      mixed_frame->GetSecurityContext()->GetInsecureRequestPolicy() &
          kBlockAllMixedContent ||
      settings->GetStrictMixedContentChecking();
  if (!strict_mode) {
    bool allowed_per_settings =
        settings && settings->GetAllowRunningOfInsecureContent();
    allowed = content_settings_client->AllowRunningInsecureContent(
        allowed_per_settings, security_origin, url);
  }

  if (allowed)
    client->DidRunInsecureContent(security_origin, url);

  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    LogToConsoleAboutWebSocket(frame, MainResourceUrlForFrame(mixed_frame), url,
                               allowed);
  }
  return !allowed;
}

bool MixedContentChecker::IsMixedFormAction(
    LocalFrame* frame,
    const KURL& url,
    SecurityViolationReportingPolicy reporting_policy) {
  // For whatever reason, some folks handle forms via JavaScript, and submit to
  // `javascript:void(0)` rather than calling `preventDefault()`. We
  // special-case `javascript:` URLs here, as they don't introduce MixedContent
  // for form submissions.
  if (url.ProtocolIs("javascript"))
    return false;

  Frame* mixed_frame = InWhichFrameIsContentMixed(
      frame, WebURLRequest::kFrameTypeNone, url, frame);
  if (!mixed_frame)
    return false;

  UseCounter::Count(frame, WebFeature::kMixedContentPresent);

  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  frame->Client()->DidContainInsecureFormAction();

  if (reporting_policy == SecurityViolationReportingPolicy::kReport) {
    String message = String::Format(
        "Mixed Content: The page at '%s' was loaded over a secure connection, "
        "but contains a form which targets an insecure endpoint '%s'. This "
        "endpoint should be made available over a secure connection.",
        MainResourceUrlForFrame(mixed_frame).ElidedString().Utf8().data(),
        url.ElidedString().Utf8().data());
    frame->GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kWarningMessageLevel, message));
  }

  return true;
}

void MixedContentChecker::CheckMixedPrivatePublic(
    LocalFrame* frame,
    const AtomicString& resource_ip_address) {
  if (!frame || !frame->GetDocument() || !frame->GetDocument()->Loader())
    return;

  // Just count these for the moment, don't block them.
  if (NetworkUtils::IsReservedIPAddress(resource_ip_address) &&
      frame->GetDocument()->AddressSpace() == kWebAddressSpacePublic) {
    UseCounter::Count(frame->GetDocument(),
                      WebFeature::kMixedContentPrivateHostnameInPublicHostname);
    // We can simplify the IP checks here, as we've already verified that
    // |resourceIPAddress| is a reserved IP address, which means it's also a
    // valid IP address in a normalized form.
    if (resource_ip_address.StartsWith("127.0.0.") ||
        resource_ip_address == "[::1]") {
      UseCounter::Count(frame->GetDocument(),
                        frame->GetDocument()->IsSecureContext()
                            ? WebFeature::kLoopbackEmbeddedInSecureContext
                            : WebFeature::kLoopbackEmbeddedInNonSecureContext);
    }
  }
}

Frame* MixedContentChecker::EffectiveFrameForFrameType(
    LocalFrame* frame,
    WebURLRequest::FrameType frame_type) {
  // If we're loading the main resource of a subframe, ensure that we check
  // against the parent of the active frame, rather than the frame itself.
  if (frame_type != WebURLRequest::kFrameTypeNested)
    return frame;

  Frame* parent_frame = frame->Tree().Parent();
  DCHECK(parent_frame);
  return parent_frame;
}

void MixedContentChecker::HandleCertificateError(
    LocalFrame* frame,
    const ResourceResponse& response,
    WebURLRequest::FrameType frame_type,
    WebURLRequest::RequestContext request_context) {
  Frame* effective_frame = EffectiveFrameForFrameType(frame, frame_type);
  if (frame_type == WebURLRequest::kFrameTypeTopLevel || !effective_frame)
    return;

  // Use the current local frame's client; the embedder doesn't distinguish
  // mixed content signals from different frames on the same page.
  LocalFrameClient* client = frame->Client();
  bool strict_mixed_content_checking_for_plugin =
      effective_frame->GetSettings() &&
      effective_frame->GetSettings()->GetStrictMixedContentCheckingForPlugin();
  WebMixedContentContextType context_type =
      WebMixedContent::ContextTypeFromRequestContext(
          request_context, strict_mixed_content_checking_for_plugin);
  if (context_type == WebMixedContentContextType::kBlockable) {
    client->DidRunContentWithCertificateErrors(response.Url());
  } else {
    // contextTypeFromRequestContext() never returns NotMixedContent (it
    // computes the type of mixed content, given that the content is mixed).
    DCHECK_NE(context_type, WebMixedContentContextType::kNotMixedContent);
    client->DidDisplayContentWithCertificateErrors(response.Url());
  }
}

// static
void MixedContentChecker::MixedContentFound(
    LocalFrame* frame,
    const KURL& main_resource_url,
    const KURL& mixed_content_url,
    WebURLRequest::RequestContext request_context,
    bool was_allowed,
    bool had_redirect,
    std::unique_ptr<SourceLocation> source_location) {
  // Logs to the frame console.
  LogToConsoleAboutFetch(frame->GetDocument(), main_resource_url,
                         mixed_content_url, request_context, was_allowed,
                         std::move(source_location));
  // Reports to the CSP policy.
  ContentSecurityPolicy* policy =
      frame->GetSecurityContext()->GetContentSecurityPolicy();
  if (policy) {
    policy->ReportMixedContent(
        mixed_content_url,
        had_redirect ? ResourceRequest::RedirectStatus::kFollowedRedirect
                     : ResourceRequest::RedirectStatus::kNoRedirect);
  }
}

WebMixedContentContextType MixedContentChecker::ContextTypeForInspector(
    LocalFrame* frame,
    const ResourceRequest& request) {
  Frame* effective_frame =
      EffectiveFrameForFrameType(frame, request.GetFrameType());

  Frame* mixed_frame = InWhichFrameIsContentMixed(
      effective_frame, request.GetFrameType(), request.Url(), frame);
  if (!mixed_frame)
    return WebMixedContentContextType::kNotMixedContent;

  // See comment in shouldBlockFetch() about loading the main resource of a
  // subframe.
  if (request.GetFrameType() == WebURLRequest::kFrameTypeNested &&
      !SchemeRegistry::ShouldTreatURLSchemeAsCORSEnabled(
          request.Url().Protocol())) {
    return WebMixedContentContextType::kOptionallyBlockable;
  }

  bool strict_mixed_content_checking_for_plugin =
      mixed_frame->GetSettings() &&
      mixed_frame->GetSettings()->GetStrictMixedContentCheckingForPlugin();
  return WebMixedContent::ContextTypeFromRequestContext(
      request.GetRequestContext(), strict_mixed_content_checking_for_plugin);
}

}  // namespace blink
