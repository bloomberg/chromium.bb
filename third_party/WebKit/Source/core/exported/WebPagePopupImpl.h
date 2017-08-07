/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebPagePopupImpl_h
#define WebPagePopupImpl_h

#include "core/CoreExport.h"
#include "core/page/PagePopup.h"
#include "core/page/PageWidgetDelegate.h"
#include "platform/wtf/RefCounted.h"
#include "public/web/WebPagePopup.h"

// To avoid conflicts with the CreateWindow macro from the Windows SDK...
#undef PostMessage

namespace blink {

class CompositorAnimationHost;
class GraphicsLayer;
class Page;
class PagePopupChromeClient;
class PagePopupClient;
class WebLayerTreeView;
class WebLayer;
class WebViewImpl;
class LocalDOMWindow;

class CORE_EXPORT WebPagePopupImpl final
    : public NON_EXPORTED_BASE(WebPagePopup),
      public NON_EXPORTED_BASE(PageWidgetEventHandler),
      public NON_EXPORTED_BASE(PagePopup),
      public NON_EXPORTED_BASE(RefCounted<WebPagePopupImpl>) {
  WTF_MAKE_NONCOPYABLE(WebPagePopupImpl);
  USING_FAST_MALLOC(WebPagePopupImpl);

 public:
  ~WebPagePopupImpl() override;
  bool Initialize(WebViewImpl*, PagePopupClient*);
  void ClosePopup();
  WebWidgetClient* WidgetClient() const { return widget_client_; }
  bool HasSamePopupClient(WebPagePopupImpl* other) {
    return other && popup_client_ == other->popup_client_;
  }
  LocalDOMWindow* Window();
  void LayoutAndPaintAsync(WebLayoutAndPaintAsyncCallback*) override;
  void CompositeAndReadbackAsync(
      WebCompositeAndReadbackAsyncCallback*) override;
  WebPoint PositionRelativeToOwner() override;
  void PostMessage(const String& message) override;
  void Cancel();

  // PageWidgetEventHandler functions.
  WebInputEventResult HandleKeyEvent(const WebKeyboardEvent&) override;

 private:
  // WebWidget functions
  void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) final;
  void BeginFrame(double last_frame_time_monotonic) override;
  void UpdateAllLifecyclePhases() override;
  void WillCloseLayerTreeView() override;
  void Paint(WebCanvas*, const WebRect&) override;
  void Resize(const WebSize&) override;
  void Close() override;
  WebInputEventResult HandleInputEvent(const WebCoalescedInputEvent&) override;
  void SetFocus(bool) override;
  bool IsPagePopup() const override { return true; }
  bool IsAcceleratedCompositingActive() const override {
    return is_accelerated_compositing_active_;
  }

  // PageWidgetEventHandler functions
  WebInputEventResult HandleCharEvent(const WebKeyboardEvent&) override;
  WebInputEventResult HandleGestureEvent(const WebGestureEvent&) override;
  void HandleMouseDown(LocalFrame& main_frame, const WebMouseEvent&) override;
  WebInputEventResult HandleMouseWheel(LocalFrame& main_frame,
                                       const WebMouseWheelEvent&) override;

  bool IsViewportPointInWindow(int x, int y);

  // PagePopup function
  AXObject* RootAXObject() override;
  void SetWindowRect(const IntRect&) override;

  explicit WebPagePopupImpl(WebWidgetClient*);
  bool InitializePage();
  void DestroyPage();
  void InitializeLayerTreeView();
  void SetRootGraphicsLayer(GraphicsLayer*);

  WebRect WindowRectInScreen() const;

  WebWidgetClient* widget_client_;
  WebViewImpl* web_view_;
  Persistent<Page> page_;
  Persistent<PagePopupChromeClient> chrome_client_;
  PagePopupClient* popup_client_;
  bool closing_;

  WebLayerTreeView* layer_tree_view_;
  WebLayer* root_layer_;
  GraphicsLayer* root_graphics_layer_;
  std::unique_ptr<CompositorAnimationHost> animation_host_;
  bool is_accelerated_compositing_active_;

  friend class WebPagePopup;
  friend class PagePopupChromeClient;
};

DEFINE_TYPE_CASTS(WebPagePopupImpl,
                  WebWidget,
                  widget,
                  widget->IsPagePopup(),
                  widget.IsPagePopup());
// WebPagePopupImpl is the only implementation of PagePopup, so no
// further checking required.
DEFINE_TYPE_CASTS(WebPagePopupImpl, PagePopup, popup, true, true);

}  // namespace blink
#endif  // WebPagePopupImpl_h
