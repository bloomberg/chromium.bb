/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
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

#include "core/exported/WebPagePopupImpl.h"

#include <memory>

#include "core/dom/AXObjectCacheBase.h"
#include "core/dom/ContextFeatures.h"
#include "core/events/MessageEvent.h"
#include "core/events/WebInputEventConversion.h"
#include "core/exported/WebSettingsImpl.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutView.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/PagePopupClient.h"
#include "core/page/PagePopupSupplement.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/LayoutTestSupport.h"
#include "platform/animation/CompositorAnimationHost.h"
#include "platform/bindings/ScriptForbiddenScope.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "public/platform/WebCursorInfo.h"
#include "public/platform/WebFloatRect.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWidgetClient.h"

namespace blink {

class PagePopupChromeClient final : public EmptyChromeClient {
 public:
  static PagePopupChromeClient* Create(WebPagePopupImpl* popup) {
    return new PagePopupChromeClient(popup);
  }

  void SetWindowRect(const IntRect& rect, LocalFrame&) override {
    popup_->SetWindowRect(rect);
  }

  bool IsPopup() override { return true; }

 private:
  explicit PagePopupChromeClient(WebPagePopupImpl* popup) : popup_(popup) {
    DCHECK(popup_->WidgetClient());
  }

  void CloseWindowSoon() override { popup_->ClosePopup(); }

  IntRect RootWindowRect() override { return popup_->WindowRectInScreen(); }

  IntRect ViewportToScreen(const IntRect& rect,
                           const PlatformFrameView* frame_view) const override {
    WebRect rect_in_screen(rect);
    WebRect window_rect = popup_->WindowRectInScreen();
    popup_->WidgetClient()->ConvertViewportToWindow(&rect_in_screen);
    rect_in_screen.x += window_rect.x;
    rect_in_screen.y += window_rect.y;
    return rect_in_screen;
  }

  float WindowToViewportScalar(const float scalar_value) const override {
    WebFloatRect viewport_rect(0, 0, scalar_value, 0);
    popup_->WidgetClient()->ConvertWindowToViewport(&viewport_rect);
    return viewport_rect.width;
  }

  void AddMessageToConsole(LocalFrame*,
                           MessageSource,
                           MessageLevel,
                           const String& message,
                           unsigned line_number,
                           const String&,
                           const String&) override {
#ifndef NDEBUG
    fprintf(stderr, "CONSOLE MESSSAGE:%u: %s\n", line_number,
            message.Utf8().data());
#endif
  }

  void InvalidateRect(const IntRect& paint_rect) override {
    if (!paint_rect.IsEmpty())
      popup_->WidgetClient()->DidInvalidateRect(paint_rect);
  }

  void ScheduleAnimation(const PlatformFrameView*) override {
    // Calling scheduleAnimation on m_webView so WebViewTestProxy will call
    // beginFrame.
    if (LayoutTestSupport::IsRunningLayoutTest())
      popup_->web_view_->MainFrameImpl()->FrameWidget()->ScheduleAnimation();

    if (popup_->layer_tree_view_) {
      popup_->layer_tree_view_->SetNeedsBeginFrame();
      return;
    }
    popup_->widget_client_->ScheduleAnimation();
  }

  void AttachCompositorAnimationTimeline(CompositorAnimationTimeline* timeline,
                                         LocalFrame*) override {
    if (popup_->animation_host_)
      popup_->animation_host_->AddTimeline(*timeline);
  }

  void DetachCompositorAnimationTimeline(CompositorAnimationTimeline* timeline,
                                         LocalFrame*) override {
    if (popup_->animation_host_)
      popup_->animation_host_->RemoveTimeline(*timeline);
  }

  WebScreenInfo GetScreenInfo() const override {
    return popup_->web_view_->Client()
               ? popup_->web_view_->Client()->GetScreenInfo()
               : WebScreenInfo();
  }

  WebViewImpl* GetWebView() const override { return popup_->web_view_; }

  IntSize MinimumWindowSize() const override { return IntSize(0, 0); }

  void SetCursor(const Cursor& cursor, LocalFrame* local_frame) override {
    popup_->widget_client_->DidChangeCursor(WebCursorInfo(cursor));
  }

  void SetEventListenerProperties(
      LocalFrame* frame,
      WebEventListenerClass event_class,
      WebEventListenerProperties properties) override {
    DCHECK(frame->IsMainFrame());
    WebWidgetClient* client = popup_->WidgetClient();
    if (WebLayerTreeView* layer_tree_view = popup_->layer_tree_view_) {
      layer_tree_view->SetEventListenerProperties(event_class, properties);
      if (event_class == WebEventListenerClass::kTouchStartOrMove) {
        client->HasTouchEventHandlers(
            properties != WebEventListenerProperties::kNothing ||
            EventListenerProperties(frame,
                                    WebEventListenerClass::kTouchEndOrCancel) !=
                WebEventListenerProperties::kNothing);
      } else if (event_class == WebEventListenerClass::kTouchEndOrCancel) {
        client->HasTouchEventHandlers(
            properties != WebEventListenerProperties::kNothing ||
            EventListenerProperties(frame,
                                    WebEventListenerClass::kTouchStartOrMove) !=
                WebEventListenerProperties::kNothing);
      }
    } else {
      client->HasTouchEventHandlers(true);
    }
  }
  WebEventListenerProperties EventListenerProperties(
      LocalFrame*,
      WebEventListenerClass event_class) const override {
    if (popup_->layer_tree_view_) {
      return popup_->layer_tree_view_->EventListenerProperties(event_class);
    }
    return WebEventListenerProperties::kNothing;
  }

  void SetHasScrollEventHandlers(LocalFrame* frame,
                                 bool has_event_handlers) override {
    DCHECK(frame->IsMainFrame());
    if (popup_->layer_tree_view_)
      popup_->layer_tree_view_->SetHaveScrollEventHandlers(has_event_handlers);
  }

  void SetTouchAction(LocalFrame* frame, TouchAction touch_action) override {
    DCHECK(frame);
    WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
    WebFrameWidget* widget = web_frame->LocalRoot()->FrameWidget();
    if (!widget)
      return;

    if (WebWidgetClient* client = ToWebFrameWidgetBase(widget)->Client())
      client->SetTouchAction(static_cast<WebTouchAction>(touch_action));
  }

  void AttachRootGraphicsLayer(GraphicsLayer* graphics_layer,
                               LocalFrame* local_root) override {
    popup_->SetRootGraphicsLayer(graphics_layer);
  }

  void SetToolTip(LocalFrame&,
                  const String& tooltip_text,
                  TextDirection dir) override {
    if (popup_->WidgetClient()) {
      popup_->WidgetClient()->SetToolTipText(tooltip_text,
                                             ToWebTextDirection(dir));
    }
  }

  WebPagePopupImpl* popup_;
};

class PagePopupFeaturesClient : public ContextFeaturesClient {
  bool IsEnabled(Document*, ContextFeatures::FeatureType, bool) override;
};

bool PagePopupFeaturesClient::IsEnabled(Document*,
                                        ContextFeatures::FeatureType type,
                                        bool default_value) {
  if (type == ContextFeatures::kPagePopup)
    return true;
  return default_value;
}

// WebPagePopupImpl ----------------------------------------------------------

WebPagePopupImpl::WebPagePopupImpl(WebWidgetClient* client)
    : widget_client_(client),
      closing_(false),
      layer_tree_view_(nullptr),
      root_layer_(nullptr),
      root_graphics_layer_(nullptr),
      is_accelerated_compositing_active_(false) {
  DCHECK(client);
}

WebPagePopupImpl::~WebPagePopupImpl() {
  DCHECK(!page_);
}

bool WebPagePopupImpl::Initialize(WebViewImpl* web_view,
                                  PagePopupClient* popup_client) {
  DCHECK(web_view);
  DCHECK(popup_client);
  web_view_ = web_view;
  popup_client_ = popup_client;

  if (!widget_client_ || !InitializePage())
    return false;
  widget_client_->Show(WebNavigationPolicy());
  SetFocus(true);

  return true;
}

bool WebPagePopupImpl::InitializePage() {
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  chrome_client_ = PagePopupChromeClient::Create(this);
  page_clients.chrome_client = chrome_client_.Get();

  Settings& main_settings = web_view_->GetPage()->GetSettings();
  page_ = Page::Create(page_clients);
  page_->GetSettings().SetScriptEnabled(true);
  page_->GetSettings().SetAllowScriptsToCloseWindows(true);
  page_->GetSettings().SetDeviceSupportsTouch(
      main_settings.GetDeviceSupportsTouch());
  page_->GetSettings().SetMinimumFontSize(main_settings.GetMinimumFontSize());
  page_->GetSettings().SetMinimumLogicalFontSize(
      main_settings.GetMinimumLogicalFontSize());
  // FIXME: Should we support enabling a11y while a popup is shown?
  page_->GetSettings().SetAccessibilityEnabled(
      main_settings.GetAccessibilityEnabled());
  page_->GetSettings().SetScrollAnimatorEnabled(
      main_settings.GetScrollAnimatorEnabled());
  page_->GetSettings().SetAvailablePointerTypes(
      main_settings.GetAvailablePointerTypes());
  page_->GetSettings().SetPrimaryPointerType(
      main_settings.GetPrimaryPointerType());

  ProvideContextFeaturesTo(*page_, std::make_unique<PagePopupFeaturesClient>());
  DEFINE_STATIC_LOCAL(LocalFrameClient, empty_local_frame_client,
                      (EmptyLocalFrameClient::Create()));
  LocalFrame* frame =
      LocalFrame::Create(&empty_local_frame_client, *page_, nullptr);
  frame->SetPagePopupOwner(popup_client_->OwnerElement());
  frame->SetView(LocalFrameView::Create(*frame));
  frame->Init();
  frame->View()->SetParentVisible(true);
  frame->View()->SetSelfVisible(true);
  if (AXObjectCache* cache =
          popup_client_->OwnerElement().GetDocument().ExistingAXObjectCache())
    cache->ChildrenChanged(&popup_client_->OwnerElement());

  DCHECK(frame->DomWindow());
  PagePopupSupplement::Install(*frame, *this, popup_client_);
  DCHECK_EQ(popup_client_->OwnerElement().GetDocument().ExistingAXObjectCache(),
            frame->GetDocument()->ExistingAXObjectCache());

  InitializeLayerTreeView();

  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  popup_client_->WriteDocument(data.get());
  frame->SetPageZoomFactor(popup_client_->ZoomFactor());
  frame->ForceSynchronousDocumentInstall("text/html", data);
  return true;
}

void WebPagePopupImpl::PostMessageToPopup(const String& message) {
  if (!page_)
    return;
  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  if (LocalDOMWindow* window = ToLocalFrame(page_->MainFrame())->DomWindow())
    window->DispatchEvent(MessageEvent::Create(message));
}

void WebPagePopupImpl::DestroyPage() {
  if (!page_)
    return;

  page_->WillBeDestroyed();
  page_.Clear();
}

AXObject* WebPagePopupImpl::RootAXObject() {
  if (!page_ || !page_->MainFrame())
    return nullptr;
  Document* document = ToLocalFrame(page_->MainFrame())->GetDocument();
  if (!document)
    return nullptr;
  AXObjectCache* cache = document->GetOrCreateAXObjectCache();
  DCHECK(cache);
  return ToAXObjectCacheBase(cache)->GetOrCreate(document->GetLayoutView());
}

void WebPagePopupImpl::SetWindowRect(const IntRect& rect_in_screen) {
  WidgetClient()->SetWindowRect(rect_in_screen);
}

void WebPagePopupImpl::SetRootGraphicsLayer(GraphicsLayer* layer) {
  root_graphics_layer_ = layer;
  root_layer_ = layer ? layer->PlatformLayer() : nullptr;

  is_accelerated_compositing_active_ = !!layer;
  if (layer_tree_view_) {
    if (root_layer_) {
      layer_tree_view_->SetRootLayer(*root_layer_);
    } else {
      layer_tree_view_->ClearRootLayer();
    }
  }
}

void WebPagePopupImpl::InitializeLayerTreeView() {
  TRACE_EVENT0("blink", "WebPagePopupImpl::initializeLayerTreeView");
  layer_tree_view_ = widget_client_->InitializeLayerTreeView();
  if (layer_tree_view_) {
    layer_tree_view_->SetVisible(true);
    animation_host_ = std::make_unique<CompositorAnimationHost>(
        layer_tree_view_->CompositorAnimationHost());
    page_->LayerTreeViewInitialized(*layer_tree_view_, nullptr);
  } else {
    animation_host_ = nullptr;
  }
}

void WebPagePopupImpl::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  if (!page_)
    return;
  page_->Animator().SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}
void WebPagePopupImpl::BeginFrame(double last_frame_time_monotonic) {
  if (!page_)
    return;
  // FIXME: This should use lastFrameTimeMonotonic but doing so
  // breaks tests.
  PageWidgetDelegate::Animate(*page_, CurrentTimeTicksInSeconds());
}

void WebPagePopupImpl::WillCloseLayerTreeView() {
  if (page_ && layer_tree_view_)
    page_->WillCloseLayerTreeView(*layer_tree_view_, nullptr);

  is_accelerated_compositing_active_ = false;
  layer_tree_view_ = nullptr;
  animation_host_ = nullptr;
}

void WebPagePopupImpl::UpdateLifecycle(LifecycleUpdate requested_update) {
  if (!page_)
    return;
  PageWidgetDelegate::UpdateLifecycle(
      *page_, *page_->DeprecatedLocalMainFrame(), requested_update);
}

void WebPagePopupImpl::UpdateAllLifecyclePhasesAndCompositeForTesting() {
  if (layer_tree_view_)
    layer_tree_view_->SynchronouslyCompositeNoRasterForTesting();
}

void WebPagePopupImpl::Paint(WebCanvas* canvas, const WebRect& rect) {
  if (!closing_) {
    PageWidgetDelegate::Paint(*page_, canvas, rect,
                              *page_->DeprecatedLocalMainFrame());
  }
}

void WebPagePopupImpl::Resize(const WebSize& new_size_in_viewport) {
  WebRect new_size(0, 0, new_size_in_viewport.width,
                   new_size_in_viewport.height);
  WidgetClient()->ConvertViewportToWindow(&new_size);

  WebRect window_rect = WindowRectInScreen();

  // TODO(bokan): We should only call into this if the bounds actually changed
  // but this reveals a bug in Aura. crbug.com/633140.
  window_rect.width = new_size.width;
  window_rect.height = new_size.height;
  SetWindowRect(window_rect);

  if (page_) {
    ToLocalFrame(page_->MainFrame())->View()->Resize(new_size_in_viewport);
    page_->GetVisualViewport().SetSize(new_size_in_viewport);
  }

  widget_client_->DidInvalidateRect(
      WebRect(0, 0, new_size.width, new_size.height));
}

WebInputEventResult WebPagePopupImpl::HandleKeyEvent(
    const WebKeyboardEvent& event) {
  if (closing_ || !page_->MainFrame() ||
      !ToLocalFrame(page_->MainFrame())->View())
    return WebInputEventResult::kNotHandled;
  return ToLocalFrame(page_->MainFrame())->GetEventHandler().KeyEvent(event);
}

WebInputEventResult WebPagePopupImpl::HandleCharEvent(
    const WebKeyboardEvent& event) {
  return HandleKeyEvent(event);
}

WebInputEventResult WebPagePopupImpl::HandleGestureEvent(
    const WebGestureEvent& event) {
  if (closing_ || !page_ || !page_->MainFrame() ||
      !ToLocalFrame(page_->MainFrame())->View())
    return WebInputEventResult::kNotHandled;
  if ((event.GetType() == WebInputEvent::kGestureTap ||
       event.GetType() == WebInputEvent::kGestureTapDown) &&
      !IsViewportPointInWindow(event.x, event.y)) {
    Cancel();
    return WebInputEventResult::kNotHandled;
  }
  LocalFrame& frame = *ToLocalFrame(page_->MainFrame());
  WebGestureEvent scaled_event = TransformWebGestureEvent(frame.View(), event);
  return frame.GetEventHandler().HandleGestureEvent(scaled_event);
}

void WebPagePopupImpl::HandleMouseDown(LocalFrame& main_frame,
                                       const WebMouseEvent& event) {
  if (IsViewportPointInWindow(event.PositionInWidget().x,
                              event.PositionInWidget().y))
    PageWidgetEventHandler::HandleMouseDown(main_frame, event);
  else
    Cancel();
}

WebInputEventResult WebPagePopupImpl::HandleMouseWheel(
    LocalFrame& main_frame,
    const WebMouseWheelEvent& event) {
  if (IsViewportPointInWindow(event.PositionInWidget().x,
                              event.PositionInWidget().y))
    return PageWidgetEventHandler::HandleMouseWheel(main_frame, event);
  Cancel();
  return WebInputEventResult::kNotHandled;
}

bool WebPagePopupImpl::IsViewportPointInWindow(int x, int y) {
  WebRect point_in_window(x, y, 0, 0);
  WidgetClient()->ConvertViewportToWindow(&point_in_window);
  WebRect window_rect = WindowRectInScreen();
  return IntRect(0, 0, window_rect.width, window_rect.height)
      .Contains(IntPoint(point_in_window.x, point_in_window.y));
}

WebInputEventResult WebPagePopupImpl::DispatchBufferedTouchEvents() {
  if (closing_)
    return WebInputEventResult::kNotHandled;
  return page_->DeprecatedLocalMainFrame()
      ->GetEventHandler()
      .DispatchBufferedTouchEvents();
}

WebInputEventResult WebPagePopupImpl::HandleInputEvent(
    const WebCoalescedInputEvent& event) {
  if (closing_)
    return WebInputEventResult::kNotHandled;
  DCHECK(!WebInputEvent::IsTouchEventType(event.Event().GetType()));
  return PageWidgetDelegate::HandleInputEvent(
      *this, event, page_->DeprecatedLocalMainFrame());
}

void WebPagePopupImpl::SetFocus(bool enable) {
  if (!page_)
    return;
  if (enable)
    page_->GetFocusController().SetActive(true);
  page_->GetFocusController().SetFocused(enable);
}

void WebPagePopupImpl::Close() {
  closing_ = true;
  // In case closePopup() was not called.
  if (page_)
    Cancel();
  widget_client_ = nullptr;
  Release();
}

void WebPagePopupImpl::ClosePopup() {
  {
    // This function can be called in EventDispatchForbiddenScope for the main
    // document, and the following operations dispatch some events.  It's safe
    // because web authors can't listen the events.
    EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;

    if (page_) {
      ToLocalFrame(page_->MainFrame())->Loader().StopAllLoaders();
      PagePopupSupplement::Uninstall(*ToLocalFrame(page_->MainFrame()));
    }
    bool close_already_called = closing_;
    closing_ = true;

    DestroyPage();

    // m_widgetClient might be 0 because this widget might be already closed.
    if (widget_client_ && !close_already_called) {
      // closeWidgetSoon() will call this->close() later.
      widget_client_->CloseWidgetSoon();
    }
  }
  popup_client_->DidClosePopup();
  web_view_->CleanupPagePopup();
}

LocalDOMWindow* WebPagePopupImpl::Window() {
  return page_->DeprecatedLocalMainFrame()->DomWindow();
}

void WebPagePopupImpl::LayoutAndPaintAsync(
    WebLayoutAndPaintAsyncCallback* callback) {
  layer_tree_view_->LayoutAndPaintAsync(callback);
}

void WebPagePopupImpl::CompositeAndReadbackAsync(
    WebCompositeAndReadbackAsyncCallback* callback) {
  DCHECK(IsAcceleratedCompositingActive());
  layer_tree_view_->CompositeAndReadbackAsync(callback);
}

WebPoint WebPagePopupImpl::PositionRelativeToOwner() {
  WebRect root_window_rect = web_view_->Client()->RootWindowRect();
  WebRect window_rect = WindowRectInScreen();
  return WebPoint(window_rect.x - root_window_rect.x,
                  window_rect.y - root_window_rect.y);
}

void WebPagePopupImpl::Cancel() {
  if (popup_client_)
    popup_client_->ClosePopup();
}

WebRect WebPagePopupImpl::WindowRectInScreen() const {
  return WidgetClient()->WindowRect();
}

// WebPagePopup ----------------------------------------------------------------

WebPagePopup* WebPagePopup::Create(WebWidgetClient* client) {
  CHECK(client);
  // A WebPagePopupImpl instance usually has two references.
  //  - One owned by the instance itself. It represents the visible widget.
  //  - One owned by a WebViewImpl. It's released when the WebViewImpl ask the
  //    WebPagePopupImpl to close.
  // We need them because the closing operation is asynchronous and the widget
  // can be closed while the WebViewImpl is unaware of it.
  auto popup = base::AdoptRef(new WebPagePopupImpl(client));
  popup->AddRef();
  return popup.get();
}

}  // namespace blink
