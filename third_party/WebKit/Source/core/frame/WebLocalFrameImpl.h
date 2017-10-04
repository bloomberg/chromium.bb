/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebLocalFrameImpl_h
#define WebLocalFrameImpl_h

#include "core/CoreExport.h"
#include "core/editing/Forward.h"
#include "core/exported/WebInputMethodControllerImpl.h"
#include "core/frame/ContentSettingsClient.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebFrameWidgetBase.h"
#include "platform/WebTaskRunner.h"
#include "platform/geometry/FloatRect.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebFileSystemType.h"
#include "public/web/WebLocalFrame.h"

#include <memory>

namespace blink {

class ChromePrintContext;
class IntSize;
class KURL;
class LocalFrameClient;
class ScrollableArea;
class SharedWorkerRepositoryClientImpl;
class TextCheckerClient;
class TextCheckerClientImpl;
class TextFinder;
class WebAssociatedURLLoader;
struct WebAssociatedURLLoaderOptions;
class WebAutofillClient;
class WebDevToolsAgentImpl;
class WebDevToolsFrontendImpl;
class WebFrameClient;
class WebNode;
class WebPerformance;
class WebPlugin;
class WebScriptExecutionCallback;
class WebSpellCheckPanelHostClient;
class WebView;
class WebViewImpl;
enum class WebFrameLoadType;
struct WebContentSecurityPolicyViolation;
struct WebPrintParams;

template <typename T>
class WebVector;

// Implementation of WebFrame, note that this is a reference counted object.
class CORE_EXPORT WebLocalFrameImpl final
    : public GarbageCollectedFinalized<WebLocalFrameImpl>,
      public WebLocalFrame {
 public:
  // WebFrame methods:
  // TODO(dcheng): Fix sorting here; a number of method have been moved to
  // WebLocalFrame but not correctly updated here.
  void Close() override;
  WebString AssignedName() const override;
  void SetName(const WebString&) override;
  WebVector<WebIconURL> IconURLs(int icon_types_mask) const override;
  void SetSharedWorkerRepositoryClient(
      WebSharedWorkerRepositoryClient*) override;
  WebSize GetScrollOffset() const override;
  void SetScrollOffset(const WebSize&) override;
  WebSize ContentsSize() const override;
  bool HasVisibleContent() const override;
  WebRect VisibleContentRect() const override;
  WebView* View() const override;
  WebDocument GetDocument() const override;
  WebPerformance Performance() const override;
  void DispatchUnloadEvent() override;
  void ExecuteScript(const WebScriptSource&) override;
  void ExecuteScriptInIsolatedWorld(int world_id,
                                    const WebScriptSource* sources,
                                    unsigned num_sources) override;
  void SetIsolatedWorldSecurityOrigin(int world_id,
                                      const WebSecurityOrigin&) override;
  void SetIsolatedWorldContentSecurityPolicy(int world_id,
                                             const WebString&) override;
  void SetIsolatedWorldHumanReadableName(int world_id,
                                         const WebString&) override;
  void AddMessageToConsole(const WebConsoleMessage&) override;
  void CollectGarbage() override;
  v8::Local<v8::Value> ExecuteScriptAndReturnValue(
      const WebScriptSource&) override;
  void RequestExecuteScriptAndReturnValue(const WebScriptSource&,
                                          bool user_gesture,
                                          WebScriptExecutionCallback*) override;
  void RequestExecuteV8Function(v8::Local<v8::Context>,
                                v8::Local<v8::Function>,
                                v8::Local<v8::Value> receiver,
                                int argc,
                                v8::Local<v8::Value> argv[],
                                WebScriptExecutionCallback*) override;
  void ExecuteScriptInIsolatedWorld(
      int world_id,
      const WebScriptSource* sources_in,
      unsigned num_sources,
      WebVector<v8::Local<v8::Value>>* results) override;
  void RequestExecuteScriptInIsolatedWorld(
      int world_id,
      const WebScriptSource* source_in,
      unsigned num_sources,
      bool user_gesture,
      ScriptExecutionType,
      WebScriptExecutionCallback*) override;
  v8::Local<v8::Value> CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>,
      v8::Local<v8::Value>,
      int argc,
      v8::Local<v8::Value> argv[]) override;
  v8::Local<v8::Context> MainWorldScriptContext() const override;
  v8::Local<v8::Object> GlobalProxy() const override;
  void Reload(WebFrameLoadType) override;
  void ReloadWithOverrideURL(const WebURL& override_url,
                             WebFrameLoadType) override;
  void ReloadImage(const WebNode&) override;
  void ReloadLoFiImages() override;
  void LoadRequest(const WebURLRequest&) override;
  void LoadHTMLString(const WebData& html,
                      const WebURL& base_url,
                      const WebURL& unreachable_url,
                      bool replace) override;
  void StopLoading() override;
  WebDocumentLoader* GetProvisionalDocumentLoader() const override;
  WebDocumentLoader* GetDocumentLoader() const override;
  void EnableViewSourceMode(bool enable) override;
  bool IsViewSourceModeEnabled() const override;
  void SetReferrerForRequest(WebURLRequest&, const WebURL& referrer) override;
  WebAssociatedURLLoader* CreateAssociatedURLLoader(
      const WebAssociatedURLLoaderOptions&) override;
  void SetMarkedText(const WebString&,
                     unsigned location,
                     unsigned length) override;
  void UnmarkText() override;
  bool HasMarkedText() const override;
  WebRange MarkedRange() const override;
  bool FirstRectForCharacterRange(unsigned location,
                                  unsigned length,
                                  WebRect&) const override;
  size_t CharacterIndexForPoint(const WebPoint&) const override;
  bool ExecuteCommand(const WebString&) override;
  bool ExecuteCommand(const WebString&, const WebString& value) override;
  bool IsCommandEnabled(const WebString&) const override;
  void SetTextCheckClient(WebTextCheckClient*) override;
  void SetSpellCheckPanelHostClient(WebSpellCheckPanelHostClient*) override;
  void EnableSpellChecking(bool) override;
  bool IsSpellCheckingEnabled() const override;
  void ReplaceMisspelledRange(const WebString&) override;
  void RemoveSpellingMarkers() override;
  void RemoveSpellingMarkersUnderWords(
      const WebVector<WebString>& words) override;
  void SetContentSettingsClient(WebContentSettingsClient*) override;
  bool HasSelection() const override;
  WebRange SelectionRange() const override;
  WebString SelectionAsText() const override;
  WebString SelectionAsMarkup() const override;
  bool SelectWordAroundCaret() override;
  void SelectRange(const WebPoint& base, const WebPoint& extent) override;
  void SelectRange(const WebRange&,
                   HandleVisibilityBehavior,
                   blink::mojom::SelectionMenuBehavior) override;
  WebString RangeAsText(const WebRange&) override;
  void MoveRangeSelectionExtent(const WebPoint&) override;
  void MoveRangeSelection(
      const WebPoint& base,
      const WebPoint& extent,
      WebFrame::TextGranularity = kCharacterGranularity) override;
  void MoveCaretSelection(const WebPoint&) override;
  bool SetEditableSelectionOffsets(int start, int end) override;
  bool SetCompositionFromExistingText(
      int composition_start,
      int composition_end,
      const WebVector<WebImeTextSpan>& ime_text_spans) override;
  void ExtendSelectionAndDelete(int before, int after) override;
  void DeleteSurroundingText(int before, int after) override;
  void DeleteSurroundingTextInCodePoints(int before, int after) override;
  void SetCaretVisible(bool) override;
  void DispatchBeforePrintEvent() override;
  int PrintBegin(const WebPrintParams&,
                 const WebNode& constrain_to_node) override;
  float PrintPage(int page_to_print, WebCanvas*) override;
  float GetPrintPageShrink(int page) override;
  void PrintEnd() override;
  void DispatchAfterPrintEvent() override;
  bool IsPrintScalingDisabledForPlugin(const WebNode&) override;
  bool GetPrintPresetOptionsForPlugin(const WebNode&,
                                      WebPrintPresetOptions*) override;
  bool HasCustomPageSizeStyle(int page_index) override;
  bool IsPageBoxVisible(int page_index) override;
  void PageSizeAndMarginsInPixels(int page_index,
                                  WebDoubleSize& page_size,
                                  int& margin_top,
                                  int& margin_right,
                                  int& margin_bottom,
                                  int& margin_left) override;
  WebString PageProperty(const WebString& property_name,
                         int page_index) override;
  void PrintPagesForTesting(WebCanvas*, const WebSize&) override;

  void DispatchMessageEventWithOriginCheck(
      const WebSecurityOrigin& intended_target_origin,
      const WebDOMEvent&) override;

  WebRect GetSelectionBoundsRectForTesting() const override;

  WebString GetLayerTreeAsTextForTesting(
      bool show_debug_info = false) const override;

  // WebLocalFrame methods:
  WebLocalFrameImpl* CreateLocalChild(WebTreeScopeType,
                                      WebFrameClient*,
                                      blink::InterfaceRegistry*) override;
  void SetAutofillClient(WebAutofillClient*) override;
  WebAutofillClient* AutofillClient() override;
  void SetDevToolsAgentClient(WebDevToolsAgentClient*) override;
  WebDevToolsAgent* DevToolsAgent() override;
  WebLocalFrameImpl* LocalRoot() override;
  WebFrame* FindFrameByName(const WebString& name) override;
  void SendPings(const WebURL& destination_url) override;
  bool DispatchBeforeUnloadEvent(bool) override;
  WebURLRequest RequestFromHistoryItem(const WebHistoryItem&,
                                       WebCachePolicy) const override;
  WebURLRequest RequestForReload(WebFrameLoadType,
                                 const WebURL&) const override;
  void Load(const WebURLRequest&,
            WebFrameLoadType = WebFrameLoadType::kStandard,
            const WebHistoryItem& = WebHistoryItem(),
            WebHistoryLoadType = kWebHistoryDifferentDocumentLoad,
            bool is_client_redirect = false) override;
  void LoadData(const WebData&,
                const WebString& mime_type,
                const WebString& text_encoding,
                const WebURL& base_url,
                const WebURL& unreachable_url,
                bool replace,
                WebFrameLoadType,
                const WebHistoryItem&,
                WebHistoryLoadType,
                bool is_client_redirect) override;
  FallbackContentResult MaybeRenderFallbackContent(
      const WebURLError&) const override;
  void ReportContentSecurityPolicyViolation(
      const blink::WebContentSecurityPolicyViolation&) override;
  bool IsLoading() const override;
  bool IsNavigationScheduledWithin(double interval) const override;
  void SetCommittedFirstRealLoad() override;
  void SetHasReceivedUserGesture() override;
  void BlinkFeatureUsageReport(const std::set<int>& features) override;
  void MixedContentFound(const WebURL& main_resource_url,
                         const WebURL& mixed_content_url,
                         WebURLRequest::RequestContext,
                         bool was_allowed,
                         bool had_redirect,
                         const WebSourceLocation&) override;
  void ClientDroppedNavigation() override;
  void SendOrientationChangeEvent() override;
  WebSandboxFlags EffectiveSandboxFlags() const override;
  void DidCallAddSearchProvider() override;
  void DidCallIsSearchProviderInstalled() override;
  void ReplaceSelection(const WebString&) override;
  void RequestFind(int identifier,
                   const WebString& search_text,
                   const WebFindOptions&) override;
  bool Find(int identifier,
            const WebString& search_text,
            const WebFindOptions&,
            bool wrap_within_frame,
            bool* active_now = nullptr) override;
  void StopFinding(StopFindAction) override;
  void IncreaseMatchCount(int count, int identifier) override;
  int FindMatchMarkersVersion() const override;
  WebFloatRect ActiveFindMatchRect() override;
  void FindMatchRects(WebVector<WebFloatRect>&) override;
  int SelectNearestFindMatch(const WebFloatPoint&,
                             WebRect* selection_rect) override;
  float DistanceToNearestFindMatch(const WebFloatPoint&) override;
  void SetTickmarks(const WebVector<WebRect>&) override;
  WebFrameWidgetBase* FrameWidget() const override;
  void CopyImageAt(const WebPoint&) override;
  void SaveImageAt(const WebPoint&) override;
  void SetEngagementLevel(mojom::EngagementLevel) override;
  void SetHasHighMediaEngagement(bool) override;
  void ClearActiveFindMatch() override;
  void UsageCountChromeLoadTimes(const WebString& metric) override;
  WebFrameScheduler* Scheduler() const override;
  SingleThreadTaskRunnerRefPtr GetTaskRunner(TaskType) override;
  WebInputMethodController* GetInputMethodController() override;

  void ExtractSmartClipData(WebRect rect_in_viewport,
                            WebString& clip_text,
                            WebString& clip_html) override;

  void AdvanceFocusInForm(WebFocusType) override;

  void InitializeCoreFrame(Page&, FrameOwner*, const AtomicString& name);
  LocalFrame* GetFrame() const { return frame_.Get(); }

  void WillBeDetached();
  void WillDetachParent();

  static WebLocalFrameImpl* Create(WebTreeScopeType,
                                   WebFrameClient*,
                                   InterfaceRegistry*,
                                   WebFrame* opener);
  static WebLocalFrameImpl* CreateMainFrame(WebView*,
                                            WebFrameClient*,
                                            InterfaceRegistry*,
                                            WebFrame* opener,
                                            const WebString& name,
                                            WebSandboxFlags);
  static WebLocalFrameImpl* CreateProvisional(WebFrameClient*,
                                              InterfaceRegistry*,
                                              WebRemoteFrame*,
                                              WebSandboxFlags,
                                              WebParsedFeaturePolicy);

  ~WebLocalFrameImpl() override;

  LocalFrame* CreateChildFrame(const AtomicString& name,
                               HTMLFrameOwnerElement*);

  void DidChangeContentsSize(const IntSize&);

  void CreateFrameView();

  static WebLocalFrameImpl* FromFrame(LocalFrame*);
  static WebLocalFrameImpl* FromFrame(LocalFrame&);
  static WebLocalFrameImpl* FromFrameOwnerElement(Element*);

  WebViewImpl* ViewImpl() const;

  LocalFrameView* GetFrameView() const {
    return GetFrame() ? GetFrame()->View() : 0;
  }

  WebDevToolsAgentImpl* DevToolsAgentImpl() const {
    return dev_tools_agent_.Get();
  }

  // When a Find operation ends, we want to set the selection to what was active
  // and set focus to the first focusable node we find (starting with the first
  // node in the matched range and going up the inheritance chain). If we find
  // nothing to focus we focus the first focusable node in the range. This
  // allows us to set focus to a link (when we find text inside a link), which
  // allows us to navigate by pressing Enter after closing the Find box.
  void SetFindEndstateFocusAndSelection();

  void DidFail(const ResourceError&, bool was_provisional, HistoryCommitType);
  void DidFinish();

  // Sets whether the WebLocalFrameImpl allows its document to be scrolled.
  // If the parameter is true, allow the document to be scrolled.
  // Otherwise, disallow scrolling.
  void SetCanHaveScrollbars(bool) override;

  WebFrameClient* Client() const { return client_; }
  void SetClient(WebFrameClient* client) { client_ = client; }

  ContentSettingsClient& GetContentSettingsClient() {
    return content_settings_client_;
  };

  SharedWorkerRepositoryClientImpl* SharedWorkerRepositoryClient() const {
    return shared_worker_repository_client_.get();
  }

  void SetInputEventsScaleForEmulation(float);

  TextCheckerClient& GetTextCheckerClient() const;
  WebTextCheckClient* TextCheckClient() const { return text_check_client_; }

  WebSpellCheckPanelHostClient* SpellCheckPanelHostClient() const override {
    return spell_check_panel_host_client_;
  }

  TextFinder* GetTextFinder() const;
  // Returns the text finder object if it already exists.
  // Otherwise creates it and then returns.
  TextFinder& EnsureTextFinder();

  // Returns a hit-tested VisiblePosition for the given point
  VisiblePosition VisiblePositionForViewportPoint(const WebPoint&);

  void SetFrameWidget(WebFrameWidgetBase*);

  // DevTools front-end bindings.
  void SetDevToolsFrontend(WebDevToolsFrontendImpl* frontend) {
    web_dev_tools_frontend_ = frontend;
  }
  WebDevToolsFrontendImpl* DevToolsFrontend() {
    return web_dev_tools_frontend_;
  }

  WebNode ContextMenuNode() const { return context_menu_node_.Get(); }
  void SetContextMenuNode(Node* node) { context_menu_node_ = node; }
  void ClearContextMenuNode() { context_menu_node_.Clear(); }

  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      SingleThreadTaskRunnerRefPtr) override;

  WebFrameWidgetBase* LocalRootFrameWidget();

  // Sets the local core frame and registers destruction observers.
  void SetCoreFrame(LocalFrame*);

  DECLARE_VIRTUAL_TRACE();

 private:
  WebLocalFrameImpl(WebTreeScopeType,
                    WebFrameClient*,
                    blink::InterfaceRegistry*);
  WebLocalFrameImpl(WebRemoteFrame*,
                    WebFrameClient*,
                    blink::InterfaceRegistry*);

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebLocalFrameImpl.
  bool IsWebLocalFrame() const override;
  WebLocalFrame* ToWebLocalFrame() override;
  bool IsWebRemoteFrame() const override;
  WebRemoteFrame* ToWebRemoteFrame() override;

  void LoadJavaScriptURL(const KURL&);

  HitTestResult HitTestResultForVisualViewportPos(const IntPoint&);

  WebPlugin* FocusedPluginIfInputMethodSupported();
  ScrollableArea* LayoutViewportScrollableArea() const;

  // Returns true if the frame is focused.
  bool IsFocused() const;

  // A helper for DispatchBeforePrintEvent() and DispatchAfterPrintEvent().
  void DispatchPrintEventRecursively(const AtomicString& event_type);

  Member<LocalFrameClient> local_frame_client_;

  // The embedder retains a reference to the WebCore LocalFrame while it is
  // active in the DOM. This reference is released when the frame is removed
  // from the DOM or the entire page is closed.  FIXME: These will need to
  // change to WebFrame when we introduce WebFrameProxy.
  Member<LocalFrame> frame_;

  Member<WebDevToolsAgentImpl> dev_tools_agent_;

  // This is set if the frame is the root of a local frame tree, and requires a
  // widget for layout.
  Member<WebFrameWidgetBase> frame_widget_;

  WebFrameClient* client_;
  WebAutofillClient* autofill_client_;
  ContentSettingsClient content_settings_client_;
  std::unique_ptr<SharedWorkerRepositoryClientImpl>
      shared_worker_repository_client_;

  // Will be initialized after first call to ensureTextFinder().
  Member<TextFinder> text_finder_;

  // Valid between calls to BeginPrint() and EndPrint(). Containts the print
  // information. Is used by PrintPage().
  Member<ChromePrintContext> print_context_;

  // Stores the additional input events scale when device metrics
  // emulation is enabled.
  float input_events_scale_factor_for_emulation_;

  // Borrowed pointers to Mojo objects.
  blink::InterfaceRegistry* interface_registry_;

  WebDevToolsFrontendImpl* web_dev_tools_frontend_;

  Member<Node> context_menu_node_;

  WebInputMethodControllerImpl input_method_controller_;

  // Stores the TextCheckerClient to bridge SpellChecker and WebTextCheckClient.
  Member<TextCheckerClientImpl> text_checker_client_;
  WebTextCheckClient* text_check_client_;

  WebSpellCheckPanelHostClient* spell_check_panel_host_client_;

  // Oilpan: WebLocalFrameImpl must remain alive until close() is called.
  // Accomplish that by keeping a self-referential Persistent<>. It is
  // cleared upon close().
  SelfKeepAlive<WebLocalFrameImpl> self_keep_alive_;

#if DCHECK_IS_ON()
  // True if DispatchBeforePrintEvent() was called, and
  // DispatchAfterPrintEvent() is not called yet.
  bool is_in_printing_ = false;
#endif
};

DEFINE_TYPE_CASTS(WebLocalFrameImpl,
                  WebFrame,
                  frame,
                  frame->IsWebLocalFrame(),
                  frame.IsWebLocalFrame());

}  // namespace blink

#endif
