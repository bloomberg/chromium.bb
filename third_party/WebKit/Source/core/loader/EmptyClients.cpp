/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/EmptyClients.h"

#include <memory>
#include "core/frame/ContentSettingsClient.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/VisualViewport.h"
#include "core/html/forms/ColorChooser.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/html/forms/FileChooser.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/loader/DocumentLoader.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebApplicationCacheHost.h"
#include "public/platform/WebMediaPlayer.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"

namespace blink {

void FillWithEmptyClients(Page::PageClients& page_clients) {
  DEFINE_STATIC_LOCAL(ChromeClient, dummy_chrome_client,
                      (EmptyChromeClient::Create()));
  page_clients.chrome_client = &dummy_chrome_client;

  DEFINE_STATIC_LOCAL(EmptyContextMenuClient, dummy_context_menu_client, ());
  page_clients.context_menu_client = &dummy_context_menu_client;

  DEFINE_STATIC_LOCAL(EmptyEditorClient, dummy_editor_client, ());
  page_clients.editor_client = &dummy_editor_client;
}

class EmptyPopupMenu : public PopupMenu {
 public:
  void Show() override {}
  void Hide() override {}
  void UpdateFromElement(UpdateReason) override {}
  void DisconnectClient() override {}
};

class EmptyFrameScheduler : public WebFrameScheduler {
 public:
  EmptyFrameScheduler() { DCHECK(IsMainThread()); }

  scoped_refptr<WebTaskRunner> GetTaskRunner(TaskType type) override {
    return Platform::Current()->MainThread()->GetWebTaskRunner();
  }

  void AddThrottlingObserver(ObserverType, Observer*) override {}
  void RemoveThrottlingObserver(ObserverType, Observer*) override {}
  void SetFrameVisible(bool) override {}
  bool IsFrameVisible() const override { return false; }
  void SetPageVisible(bool) override {}
  bool IsPageVisible() const override { return false; }
  void SetPaused(bool) override {}
  void SetCrossOrigin(bool) override {}
  bool IsCrossOrigin() const override { return false; }
  WebFrameScheduler::FrameType GetFrameType() const override {
    return WebFrameScheduler::FrameType::kSubframe;
  }
  WebViewScheduler* GetWebViewScheduler() const override { return nullptr; }
  WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser() {
    return WebScopedVirtualTimePauser();
  }
  void DidStartProvisionalLoad(bool is_main_frame) override {}
  void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                bool is_reload,
                                bool is_main_frame) override {}
  void OnFirstMeaningfulPaint() override {}
  std::unique_ptr<ActiveConnectionHandle> OnActiveConnectionCreated() override {
    return nullptr;
  }
  bool IsExemptFromBudgetBasedThrottling() const override { return false; }
};

PopupMenu* EmptyChromeClient::OpenPopupMenu(LocalFrame&, HTMLSelectElement&) {
  return new EmptyPopupMenu();
}

ColorChooser* EmptyChromeClient::OpenColorChooser(LocalFrame*,
                                                  ColorChooserClient*,
                                                  const Color&) {
  return nullptr;
}

DateTimeChooser* EmptyChromeClient::OpenDateTimeChooser(
    DateTimeChooserClient*,
    const DateTimeChooserParameters&) {
  return nullptr;
}

void EmptyChromeClient::OpenTextDataListChooser(HTMLInputElement&) {}

void EmptyChromeClient::OpenFileChooser(LocalFrame*,
                                        scoped_refptr<FileChooser>) {}

void EmptyChromeClient::AttachRootGraphicsLayer(GraphicsLayer* layer,
                                                LocalFrame* local_root) {
  Page* page = local_root ? local_root->GetPage() : nullptr;
  if (!page)
    return;
  page->GetVisualViewport().AttachLayerTree(layer);
}

String EmptyChromeClient::AcceptLanguages() {
  return String();
}

std::unique_ptr<WebFrameScheduler> EmptyChromeClient::CreateFrameScheduler(
    BlameContext* blame_context,
    WebFrameScheduler::FrameType frame_type) {
  return std::make_unique<EmptyFrameScheduler>();
}

NavigationPolicy EmptyLocalFrameClient::DecidePolicyForNavigation(
    const ResourceRequest&,
    Document* origin_document,
    DocumentLoader*,
    NavigationType,
    NavigationPolicy,
    bool,
    bool,
    WebTriggeringEventInfo,
    HTMLFormElement*,
    ContentSecurityPolicyDisposition) {
  return kNavigationPolicyIgnore;
}

void EmptyLocalFrameClient::DispatchWillSendSubmitEvent(HTMLFormElement*) {}

void EmptyLocalFrameClient::DispatchWillSubmitForm(HTMLFormElement*) {}

DocumentLoader* EmptyLocalFrameClient::CreateDocumentLoader(
    LocalFrame* frame,
    const ResourceRequest& request,
    const SubstituteData& substitute_data,
    ClientRedirectPolicy client_redirect_policy,
    const base::UnguessableToken& devtools_navigation_token) {
  DCHECK(frame);

  return DocumentLoader::Create(frame, request, substitute_data,
                                client_redirect_policy,
                                devtools_navigation_token);
}

LocalFrame* EmptyLocalFrameClient::CreateFrame(const AtomicString&,
                                               HTMLFrameOwnerElement*) {
  return nullptr;
}

PluginView* EmptyLocalFrameClient::CreatePlugin(HTMLPlugInElement&,
                                                const KURL&,
                                                const Vector<String>&,
                                                const Vector<String>&,
                                                const String&,
                                                bool,
                                                DetachedPluginPolicy) {
  return nullptr;
}

std::unique_ptr<WebMediaPlayer> EmptyLocalFrameClient::CreateWebMediaPlayer(
    HTMLMediaElement&,
    const WebMediaPlayerSource&,
    WebMediaPlayerClient*,
    WebLayerTreeView*) {
  return nullptr;
}

WebRemotePlaybackClient* EmptyLocalFrameClient::CreateWebRemotePlaybackClient(
    HTMLMediaElement&) {
  return nullptr;
}

WebTextCheckClient* EmptyLocalFrameClient::GetTextCheckerClient() const {
  return nullptr;
}

std::unique_ptr<WebServiceWorkerProvider>
EmptyLocalFrameClient::CreateServiceWorkerProvider() {
  return nullptr;
}

ContentSettingsClient& EmptyLocalFrameClient::GetContentSettingsClient() {
  return content_settings_client_;
}

std::unique_ptr<WebApplicationCacheHost>
EmptyLocalFrameClient::CreateApplicationCacheHost(
    WebApplicationCacheHostClient*) {
  return nullptr;
}

EmptyRemoteFrameClient::EmptyRemoteFrameClient() = default;

bool EmptyContextMenuClient::ShowContextMenu(const ContextMenu*,
                                             WebMenuSourceType source_type) {
  return false;
}

}  // namespace blink
