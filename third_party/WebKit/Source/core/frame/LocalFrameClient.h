/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
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

#ifndef LocalFrameClient_h
#define LocalFrameClient_h

#include <memory>

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/dom/IconURL.h"
#include "core/frame/FrameClient.h"
#include "core/frame/FrameTypes.h"
#include "core/html/LinkResource.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/NavigationPolicy.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/weborigin/Referrer.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebContentSecurityPolicyStruct.h"
#include "public/platform/WebEffectiveConnectionType.h"
#include "public/platform/WebFeaturePolicy.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/platform/WebLoadingBehaviorFlag.h"
#include "public/platform/WebSuddenTerminationDisablerType.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebTriggeringEventInfo.h"
#include "v8/include/v8.h"

namespace service_manager {
class InterfaceProvider;
}  // namespace service_manager

namespace blink {
namespace mojom {
enum class WebFeature : int32_t;
}  // namespace mojom

class Document;
class DocumentLoader;
class HTMLFormElement;
class HTMLFrameOwnerElement;
class HTMLMediaElement;
class HTMLPlugInElement;
class HistoryItem;
class KURL;
class LocalFrame;
class PluginView;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;
class SharedWorkerRepositoryClient;
class SubstituteData;
class TextCheckerClient;
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebCookieJar;
class WebFrame;
class WebLayerTreeView;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebMediaPlayerSource;
class WebRemotePlaybackClient;
class WebRTCPeerConnectionHandler;
class WebServiceWorkerProvider;
class WebSpellCheckPanelHostClient;
class WebTaskRunner;
class WebURLLoader;

class CORE_EXPORT LocalFrameClient : public FrameClient {
 public:
  ~LocalFrameClient() override {}

  virtual WebFrame* GetWebFrame() const { return nullptr; }

  virtual bool HasWebView() const = 0;  // mainly for assertions

  virtual void DispatchWillSendRequest(ResourceRequest&) = 0;
  virtual void DispatchDidReceiveResponse(const ResourceResponse&) = 0;
  virtual void DispatchDidLoadResourceFromMemoryCache(
      const ResourceRequest&,
      const ResourceResponse&) = 0;

  virtual void DispatchDidHandleOnloadEvents() = 0;
  virtual void DispatchDidReceiveServerRedirectForProvisionalLoad() = 0;
  virtual void DispatchDidNavigateWithinPage(HistoryItem*,
                                             HistoryCommitType,
                                             bool content_initiated) {}
  virtual void DispatchWillCommitProvisionalLoad() = 0;
  virtual void DispatchDidStartProvisionalLoad(DocumentLoader*,
                                               ResourceRequest&) = 0;
  virtual void DispatchDidReceiveTitle(const String&) = 0;
  virtual void DispatchDidChangeIcons(IconType) = 0;
  virtual void DispatchDidCommitLoad(HistoryItem*, HistoryCommitType) = 0;
  virtual void DispatchDidFailProvisionalLoad(const ResourceError&,
                                              HistoryCommitType) = 0;
  virtual void DispatchDidFailLoad(const ResourceError&, HistoryCommitType) = 0;
  virtual void DispatchDidFinishDocumentLoad() = 0;
  virtual void DispatchDidFinishLoad() = 0;
  virtual void DispatchDidChangeThemeColor() = 0;

  virtual NavigationPolicy DecidePolicyForNavigation(
      const ResourceRequest&,
      Document* origin_document,
      DocumentLoader*,
      NavigationType,
      NavigationPolicy,
      bool should_replace_current_entry,
      bool is_client_redirect,
      WebTriggeringEventInfo,
      HTMLFormElement*,
      ContentSecurityPolicyDisposition
          should_check_main_world_content_security_policy) = 0;

  virtual void DispatchWillSendSubmitEvent(HTMLFormElement*) = 0;
  virtual void DispatchWillSubmitForm(HTMLFormElement*) = 0;

  virtual void DidStartLoading(LoadStartType) = 0;
  virtual void ProgressEstimateChanged(double progress_estimate) = 0;
  virtual void DidStopLoading() = 0;

  virtual void DownloadURL(const ResourceRequest&,
                           const String& suggested_name) = 0;
  virtual void LoadErrorPage(int reason) = 0;

  virtual bool NavigateBackForward(int offset) const = 0;

  // Another page has accessed the initial empty document of this frame. It is
  // no longer safe to display a provisional URL, since a URL spoof is now
  // possible.
  virtual void DidAccessInitialDocument() {}

  // This frame has displayed inactive content (such as an image) from an
  // insecure source.  Inactive content cannot spread to other frames.
  virtual void DidDisplayInsecureContent() = 0;

  // This frame contains a form that submits to an insecure target url.
  virtual void DidContainInsecureFormAction() = 0;

  // The indicated security origin has run active content (such as a script)
  // from an insecure source.  Note that the insecure content can spread to
  // other frames in the same origin.
  virtual void DidRunInsecureContent(SecurityOrigin*, const KURL&) = 0;
  virtual void DidDetectXSS(const KURL&, bool did_block_entire_page) = 0;
  virtual void DidDispatchPingLoader(const KURL&) = 0;

  // The frame displayed content with certificate errors with given URL.
  virtual void DidDisplayContentWithCertificateErrors(const KURL&) = 0;
  // The frame ran content with certificate errors with the given URL.
  virtual void DidRunContentWithCertificateErrors(const KURL&) = 0;

  // Will be called when |PerformanceTiming| events are updated
  virtual void DidChangePerformanceTiming() {}

  // Will be called when a particular loading code path has been used. This
  // propogates renderer loading behavior to the browser process for histograms.
  virtual void DidObserveLoadingBehavior(WebLoadingBehaviorFlag) {}

  // Will be called when a new useCounter feature has been observed in a frame.
  // This propogates feature usage to the browser process for histograms.
  virtual void DidObserveNewFeatureUsage(mojom::WebFeature) {}

  // Transmits the change in the set of watched CSS selectors property that
  // match any element on the frame.
  virtual void SelectorMatchChanged(
      const Vector<String>& added_selectors,
      const Vector<String>& removed_selectors) = 0;

  virtual DocumentLoader* CreateDocumentLoader(LocalFrame*,
                                               const ResourceRequest&,
                                               const SubstituteData&,
                                               ClientRedirectPolicy) = 0;

  virtual String UserAgent() = 0;

  virtual String DoNotTrackValue() = 0;

  virtual void TransitionToCommittedForNewPage() = 0;

  virtual LocalFrame* CreateFrame(const AtomicString& name,
                                  HTMLFrameOwnerElement*) = 0;

  // Whether or not plugin creation should fail if the HTMLPlugInElement isn't
  // in the DOM after plugin initialization.
  enum DetachedPluginPolicy {
    kFailOnDetachedPlugin,
    kAllowDetachedPlugin,
  };
  virtual bool CanCreatePluginWithoutRenderer(
      const String& mime_type) const = 0;
  virtual PluginView* CreatePlugin(HTMLPlugInElement&,
                                   const KURL&,
                                   const Vector<String>&,
                                   const Vector<String>&,
                                   const String&,
                                   bool load_manually,
                                   DetachedPluginPolicy) = 0;

  virtual std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      WebLayerTreeView*) = 0;
  virtual WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement&) = 0;

  virtual void DidCreateNewDocument() = 0;
  virtual void DispatchDidClearWindowObjectInMainWorld() = 0;
  virtual void DocumentElementAvailable() = 0;
  virtual void RunScriptsAtDocumentElementAvailable() = 0;
  virtual void RunScriptsAtDocumentReady(bool document_is_empty) = 0;
  virtual void RunScriptsAtDocumentIdle() = 0;

  virtual void DidCreateScriptContext(v8::Local<v8::Context>, int world_id) = 0;
  virtual void WillReleaseScriptContext(v8::Local<v8::Context>,
                                        int world_id) = 0;
  virtual bool AllowScriptExtensions() = 0;

  virtual void DidChangeScrollOffset() {}
  virtual void DidUpdateCurrentHistoryItem() {}

  // Called when a content-initiated, main frame navigation to a data URL is
  // about to occur.
  virtual bool AllowContentInitiatedDataUrlNavigations(const KURL&) {
    return false;
  }

  virtual WebCookieJar* CookieJar() const = 0;

  virtual void DidChangeName(const String&) {}

  virtual void DidEnforceInsecureRequestPolicy(WebInsecureRequestPolicy) {}

  virtual void DidUpdateToUniqueOrigin() {}

  virtual void DidChangeFramePolicy(Frame* child_frame,
                                    SandboxFlags,
                                    const WebParsedFeaturePolicy&) {}

  virtual void DidSetFeaturePolicyHeader(
      const WebParsedFeaturePolicy& parsed_header) {}

  // Called when a set of new Content Security Policies is added to the frame's
  // document. This can be triggered by handling of HTTP headers, handling of
  // <meta> element, or by inheriting CSP from the parent (in case of
  // about:blank).
  virtual void DidAddContentSecurityPolicies(
      const blink::WebVector<WebContentSecurityPolicy>&) {}

  virtual void DidChangeFrameOwnerProperties(HTMLFrameOwnerElement*) {}

  virtual void DispatchWillStartUsingPeerConnectionHandler(
      WebRTCPeerConnectionHandler*) {}

  virtual bool AllowWebGL(bool enabled_per_settings) {
    return enabled_per_settings;
  }

  // If an HTML document is being loaded, informs the embedder that the document
  // will have its <body> attached soon.
  virtual void DispatchWillInsertBody() {}

  virtual std::unique_ptr<WebServiceWorkerProvider>
  CreateServiceWorkerProvider() = 0;

  virtual ContentSettingsClient& GetContentSettingsClient() = 0;

  virtual SharedWorkerRepositoryClient* GetSharedWorkerRepositoryClient() {
    return 0;
  }

  virtual std::unique_ptr<WebApplicationCacheHost> CreateApplicationCacheHost(
      WebApplicationCacheHostClient*) = 0;

  virtual void DispatchDidChangeManifest() {}

  virtual unsigned BackForwardLength() { return 0; }

  virtual bool IsLocalFrameClientImpl() const { return false; }

  virtual void SuddenTerminationDisablerChanged(
      bool present,
      WebSuddenTerminationDisablerType) {}

  // Effective connection type when this frame was loaded.
  virtual WebEffectiveConnectionType GetEffectiveConnectionType() {
    return WebEffectiveConnectionType::kTypeUnknown;
  }

  // Returns whether or not Client Lo-Fi is enabled for the frame
  // (and so image requests may be replaced with a placeholder).
  virtual bool IsClientLoFiActiveForFrame() { return false; }

  // Returns whether or not the requested image should be replaced with a
  // placeholder as part of the Client Lo-Fi previews feature.
  virtual bool ShouldUseClientLoFiForRequest(const ResourceRequest&) {
    return false;
  }

  // Overwrites the given URL to use an HTML5 embed if possible. An empty URL is
  // returned if the URL is not overriden.
  virtual KURL OverrideFlashEmbedWithHTML(const KURL&) { return KURL(); }

  virtual BlameContext* GetFrameBlameContext() { return nullptr; }

  virtual service_manager::InterfaceProvider* GetInterfaceProvider() {
    return nullptr;
  }

  virtual void SetHasReceivedUserGesture(bool received_previously) {}

  virtual void SetDevToolsFrameId(const String& devtools_frame_id) {}

  virtual void AbortClientNavigation() {}

  virtual WebSpellCheckPanelHostClient* SpellCheckPanelHostClient() const = 0;

  virtual TextCheckerClient& GetTextCheckerClient() const = 0;

  virtual std::unique_ptr<WebURLLoader> CreateURLLoader(const ResourceRequest&,
                                                        WebTaskRunner*) = 0;

  virtual void AnnotatedRegionsChanged() = 0;

  virtual void DidBlockFramebust(const KURL&) {}
};

}  // namespace blink

#endif  // LocalFrameClient_h
