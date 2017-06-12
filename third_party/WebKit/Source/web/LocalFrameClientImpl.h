/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LocalFrameClientImpl_h
#define LocalFrameClientImpl_h

#include "core/frame/LocalFrameClient.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebInsecureRequestPolicy.h"

#include <memory>

namespace blink {

class WebDevToolsAgentImpl;
class WebLocalFrameBase;

class LocalFrameClientImpl final : public LocalFrameClient {
 public:
  static LocalFrameClientImpl* Create(WebLocalFrameBase*);

  ~LocalFrameClientImpl() override;

  DECLARE_VIRTUAL_TRACE();

  WebLocalFrameBase* GetWebFrame() const { return web_frame_.Get(); }

  // LocalFrameClient ----------------------------------------------

  void DidCreateNewDocument() override;
  // Notifies the WebView delegate that the JS window object has been cleared,
  // giving it a chance to bind native objects to the window before script
  // parsing begins.
  void DispatchDidClearWindowObjectInMainWorld() override;
  void DocumentElementAvailable() override;
  void RunScriptsAtDocumentElementAvailable() override;
  void RunScriptsAtDocumentReady(bool document_is_empty) override;
  void RunScriptsAtDocumentIdle() override;

  void DidCreateScriptContext(v8::Local<v8::Context>, int world_id) override;
  void WillReleaseScriptContext(v8::Local<v8::Context>, int world_id) override;

  // Returns true if we should allow register V8 extensions to be added.
  bool AllowScriptExtensions() override;

  bool HasWebView() const override;
  bool InShadowTree() const override;
  Frame* Opener() const override;
  void SetOpener(Frame*) override;
  Frame* Parent() const override;
  Frame* Top() const override;
  Frame* NextSibling() const override;
  Frame* FirstChild() const override;
  void WillBeDetached() override;
  void Detached(FrameDetachType) override;
  void DispatchWillSendRequest(ResourceRequest&) override;
  void DispatchDidReceiveResponse(const ResourceResponse&) override;
  void DispatchDidLoadResourceFromMemoryCache(const ResourceRequest&,
                                              const ResourceResponse&) override;
  void DispatchDidHandleOnloadEvents() override;
  void DispatchDidReceiveServerRedirectForProvisionalLoad() override;
  void DispatchDidNavigateWithinPage(HistoryItem*,
                                     HistoryCommitType,
                                     bool content_initiated) override;
  void DispatchWillCommitProvisionalLoad() override;
  void DispatchDidStartProvisionalLoad(DocumentLoader*,
                                       ResourceRequest&) override;
  void DispatchDidReceiveTitle(const String&) override;
  void DispatchDidChangeIcons(IconType) override;
  void DispatchDidCommitLoad(HistoryItem*, HistoryCommitType) override;
  void DispatchDidFailProvisionalLoad(const ResourceError&,
                                      HistoryCommitType) override;
  void DispatchDidFailLoad(const ResourceError&, HistoryCommitType) override;
  void DispatchDidFinishDocumentLoad() override;
  void DispatchDidFinishLoad() override;

  void DispatchDidChangeThemeColor() override;
  NavigationPolicy DecidePolicyForNavigation(
      const ResourceRequest&,
      Document* origin_document,
      DocumentLoader*,
      NavigationType,
      NavigationPolicy,
      bool should_replace_current_entry,
      bool is_client_redirect,
      HTMLFormElement*,
      ContentSecurityPolicyDisposition should_bypass_main_world_csp) override;
  void DispatchWillSendSubmitEvent(HTMLFormElement*) override;
  void DispatchWillSubmitForm(HTMLFormElement*) override;
  void DidStartLoading(LoadStartType) override;
  void DidStopLoading() override;
  void ProgressEstimateChanged(double progress_estimate) override;
  void LoadURLExternally(const ResourceRequest&,
                         NavigationPolicy,
                         const String& suggested_name,
                         bool should_replace_current_entry) override;
  void LoadErrorPage(int reason) override;
  bool NavigateBackForward(int offset) const override;
  void DidAccessInitialDocument() override;
  void DidDisplayInsecureContent() override;
  void DidContainInsecureFormAction() override;
  void DidRunInsecureContent(SecurityOrigin*,
                             const KURL& insecure_url) override;
  void DidDetectXSS(const KURL&, bool did_block_entire_page) override;
  void DidDispatchPingLoader(const KURL&) override;
  void DidDisplayContentWithCertificateErrors(const KURL&) override;
  void DidRunContentWithCertificateErrors(const KURL&) override;
  void DidChangePerformanceTiming() override;
  void DidObserveLoadingBehavior(WebLoadingBehaviorFlag) override;
  void SelectorMatchChanged(const Vector<String>& added_selectors,
                            const Vector<String>& removed_selectors) override;
  DocumentLoader* CreateDocumentLoader(LocalFrame*,
                                       const ResourceRequest&,
                                       const SubstituteData&,
                                       ClientRedirectPolicy) override;
  WTF::String UserAgent() override;
  WTF::String DoNotTrackValue() override;
  void TransitionToCommittedForNewPage() override;
  LocalFrame* CreateFrame(const FrameLoadRequest&,
                          const WTF::AtomicString& name,
                          HTMLFrameOwnerElement*) override;
  virtual bool CanCreatePluginWithoutRenderer(const String& mime_type) const;
  PluginView* CreatePlugin(HTMLPlugInElement&,
                           const KURL&,
                           const Vector<WTF::String>&,
                           const Vector<WTF::String>&,
                           const WTF::String&,
                           bool load_manually,
                           DetachedPluginPolicy) override;
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override;
  WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement&) override;
  void DidChangeScrollOffset() override;
  void DidUpdateCurrentHistoryItem() override;

  bool AllowContentInitiatedDataUrlNavigations(const KURL&) override;

  WebCookieJar* CookieJar() const override;
  void FrameFocused() const override;
  void DidChangeName(const String&) override;
  void DidEnforceInsecureRequestPolicy(WebInsecureRequestPolicy) override;
  void DidUpdateToUniqueOrigin() override;
  void DidChangeFramePolicy(Frame* child_frame,
                            SandboxFlags,
                            const WebParsedFeaturePolicy&) override;
  void DidSetFeaturePolicyHeader(
      const WebParsedFeaturePolicy& parsed_header) override;
  void DidAddContentSecurityPolicies(
      const blink::WebVector<WebContentSecurityPolicy>&) override;
  void DidChangeFrameOwnerProperties(HTMLFrameOwnerElement*) override;

  void DispatchWillStartUsingPeerConnectionHandler(
      WebRTCPeerConnectionHandler*) override;

  bool AllowWebGL(bool enabled_per_settings) override;

  void DispatchWillInsertBody() override;

  std::unique_ptr<WebServiceWorkerProvider> CreateServiceWorkerProvider()
      override;
  ContentSettingsClient& GetContentSettingsClient() override;

  SharedWorkerRepositoryClient* GetSharedWorkerRepositoryClient() override;

  std::unique_ptr<WebApplicationCacheHost> CreateApplicationCacheHost(
      WebApplicationCacheHostClient*) override;

  void DispatchDidChangeManifest() override;

  unsigned BackForwardLength() override;

  void SuddenTerminationDisablerChanged(bool present,
                                        SuddenTerminationDisablerType) override;

  BlameContext* GetFrameBlameContext() override;

  LinkResource* CreateServiceWorkerLinkResource(HTMLLinkElement*) override;

  WebEffectiveConnectionType GetEffectiveConnectionType() override;

  bool ShouldUseClientLoFiForRequest(const ResourceRequest&) override;

  KURL OverrideFlashEmbedWithHTML(const KURL&) override;

  void SetHasReceivedUserGesture(bool received_previously) override;

  void SetDevToolsFrameId(const String& devtools_frame_id) override;

  void AbortClientNavigation() override;

  TextCheckerClient& GetTextCheckerClient() const override;

  std::unique_ptr<WebURLLoader> CreateURLLoader() override;

  service_manager::InterfaceProvider* GetInterfaceProvider() override;

 private:
  explicit LocalFrameClientImpl(WebLocalFrameBase*);

  bool IsLocalFrameClientImpl() const override { return true; }
  WebDevToolsAgentImpl* DevToolsAgent();

  // The WebFrame that owns this object and manages its lifetime. Therefore,
  // the web frame object is guaranteed to exist.
  Member<WebLocalFrameBase> web_frame_;

  String user_agent_;
};

DEFINE_TYPE_CASTS(LocalFrameClientImpl,
                  LocalFrameClient,
                  client,
                  client->IsLocalFrameClientImpl(),
                  client.IsLocalFrameClientImpl());

}  // namespace blink

#endif
