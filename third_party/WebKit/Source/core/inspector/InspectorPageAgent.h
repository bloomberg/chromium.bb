/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorPageAgent_h
#define InspectorPageAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Page.h"
#include "core/page/ChromeClient.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8-inspector.h"

namespace blink {

namespace probe {
class RecalculateStyle;
class UpdateLayout;
}

class Resource;
class Document;
class DocumentLoader;
class InspectedFrames;
class InspectorResourceContentLoader;
class KURL;
class LocalFrame;
class SharedBuffer;

using blink::protocol::Maybe;

class CORE_EXPORT InspectorPageAgent final
    : public InspectorBaseAgent<protocol::Page::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorPageAgent);

 public:
  class Client {
   public:
    virtual ~Client() {}
    virtual void PageLayoutInvalidated(bool resized) {}
    virtual void WaitForCreateWindow(LocalFrame*) {}
  };

  enum ResourceType {
    kDocumentResource,
    kStylesheetResource,
    kImageResource,
    kFontResource,
    kMediaResource,
    kScriptResource,
    kTextTrackResource,
    kXHRResource,
    kFetchResource,
    kEventSourceResource,
    kWebSocketResource,
    kManifestResource,
    kOtherResource
  };

  static InspectorPageAgent* Create(InspectedFrames*,
                                    Client*,
                                    InspectorResourceContentLoader*,
                                    v8_inspector::V8InspectorSession*);

  static HeapVector<Member<Document>> ImportsForFrame(LocalFrame*);
  static bool CachedResourceContent(Resource*,
                                    String* result,
                                    bool* base64_encoded);
  static bool SharedBufferContent(PassRefPtr<const SharedBuffer>,
                                  const String& mime_type,
                                  const String& text_encoding_name,
                                  String* result,
                                  bool* base64_encoded);

  static Resource* CachedResource(LocalFrame*, const KURL&);
  static String ResourceTypeJson(ResourceType);
  static ResourceType CachedResourceType(const Resource&);
  static String CachedResourceTypeJson(const Resource&);

  // Page API for frontend
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response addScriptToEvaluateOnLoad(const String& script_source,
                                               String* identifier) override;
  protocol::Response removeScriptToEvaluateOnLoad(
      const String& identifier) override;
  protocol::Response setAutoAttachToCreatedPages(bool) override;
  protocol::Response reload(Maybe<bool> bypass_cache,
                            Maybe<String> script_to_evaluate_on_load) override;
  protocol::Response navigate(const String& url,
                              Maybe<String> referrer,
                              Maybe<String> transitionType,
                              String* frame_id) override;
  protocol::Response stopLoading() override;
  protocol::Response getResourceTree(
      std::unique_ptr<protocol::Page::FrameResourceTree>* frame_tree) override;
  void getResourceContent(const String& frame_id,
                          const String& url,
                          std::unique_ptr<GetResourceContentCallback>) override;
  void searchInResource(const String& frame_id,
                        const String& url,
                        const String& query,
                        Maybe<bool> case_sensitive,
                        Maybe<bool> is_regex,
                        std::unique_ptr<SearchInResourceCallback>) override;
  protocol::Response setDocumentContent(const String& frame_id,
                                        const String& html) override;
  protocol::Response startScreencast(Maybe<String> format,
                                     Maybe<int> quality,
                                     Maybe<int> max_width,
                                     Maybe<int> max_height,
                                     Maybe<int> every_nth_frame) override;
  protocol::Response stopScreencast() override;
  protocol::Response getLayoutMetrics(
      std::unique_ptr<protocol::Page::LayoutViewport>*,
      std::unique_ptr<protocol::Page::VisualViewport>*,
      std::unique_ptr<protocol::DOM::Rect>*) override;
  protocol::Response createIsolatedWorld(
      const String& frame_id,
      Maybe<String> world_name,
      Maybe<bool> grant_universal_access) override;

  // InspectorInstrumentation API
  void DidClearDocumentOfWindowObject(LocalFrame*);
  void DomContentLoadedEventFired(LocalFrame*);
  void LoadEventFired(LocalFrame*);
  void DidCommitLoad(LocalFrame*, DocumentLoader*);
  void FrameAttachedToParent(LocalFrame*);
  void FrameDetachedFromParent(LocalFrame*);
  void FrameStartedLoading(LocalFrame*, FrameLoadType);
  void FrameStoppedLoading(LocalFrame*);
  void FrameScheduledNavigation(LocalFrame*, double delay);
  void FrameClearedScheduledNavigation(LocalFrame*);
  void WillRunJavaScriptDialog(const String& message, ChromeClient::DialogType);
  void DidRunJavaScriptDialog(bool result);
  void DidResizeMainFrame();
  void DidChangeViewport();
  void Will(const probe::UpdateLayout&);
  void Did(const probe::UpdateLayout&);
  void Will(const probe::RecalculateStyle&);
  void Did(const probe::RecalculateStyle&);
  void WindowCreated(LocalFrame*);

  // Inspector Controller API
  void Restore() override;
  bool ScreencastEnabled();

  DECLARE_VIRTUAL_TRACE();

 private:
  InspectorPageAgent(InspectedFrames*,
                     Client*,
                     InspectorResourceContentLoader*,
                     v8_inspector::V8InspectorSession*);

  void FinishReload();
  void GetResourceContentAfterResourcesContentLoaded(
      const String& frame_id,
      const String& url,
      std::unique_ptr<GetResourceContentCallback>);
  void SearchContentAfterResourcesContentLoaded(
      const String& frame_id,
      const String& url,
      const String& query,
      bool case_sensitive,
      bool is_regex,
      std::unique_ptr<SearchInResourceCallback>);

  static bool DataContent(const char* data,
                          unsigned size,
                          const String& text_encoding_name,
                          bool with_base64_encode,
                          String* result);

  void PageLayoutInvalidated(bool resized);

  std::unique_ptr<protocol::Page::Frame> BuildObjectForFrame(LocalFrame*);
  std::unique_ptr<protocol::Page::FrameResourceTree> BuildObjectForFrameTree(
      LocalFrame*);
  Member<InspectedFrames> inspected_frames_;
  v8_inspector::V8InspectorSession* v8_session_;
  Client* client_;
  long last_script_identifier_;
  String pending_script_to_evaluate_on_load_once_;
  String script_to_evaluate_on_load_once_;
  bool enabled_;
  bool reloading_;
  Member<InspectorResourceContentLoader> inspector_resource_content_loader_;
  int resource_content_loader_client_id_;
};

}  // namespace blink

#endif  // !defined(InspectorPagerAgent_h)
