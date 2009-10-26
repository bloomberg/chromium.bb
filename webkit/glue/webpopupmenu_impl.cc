// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "Cursor.h"
#include "FramelessScrollView.h"
#include "FrameView.h"
#include "IntRect.h"
#include "PlatformContextSkia.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "SkiaUtils.h"
#undef LOG

#include "skia/ext/platform_canvas.h"
#include "webkit/api/public/WebInputEvent.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebWidgetClient.h"
#include "webkit/api/src/WebInputEventConversion.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webpopupmenu_impl.h"

using namespace WebCore;

using WebKit::PlatformKeyboardEventBuilder;
using WebKit::PlatformMouseEventBuilder;
using WebKit::PlatformWheelEventBuilder;
using WebKit::WebCanvas;
using WebKit::WebCompositionCommand;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebNavigationPolicy;
using WebKit::WebPoint;
using WebKit::WebPopupMenu;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebTextDirection;
using WebKit::WebWidget;
using WebKit::WebWidgetClient;

// WebPopupMenu ---------------------------------------------------------------

// static
WebPopupMenu* WebPopupMenu::create(WebWidgetClient* client) {
  return new WebPopupMenuImpl(client);
}

// WebWidget ------------------------------------------------------------------

WebPopupMenuImpl::WebPopupMenuImpl(WebWidgetClient* client)
    : client_(client),
      widget_(NULL) {
  // set to impossible point so we always get the first mouse pos
  last_mouse_position_ = WebPoint(-1, -1);
}

WebPopupMenuImpl::~WebPopupMenuImpl() {
  if (widget_)
    widget_->setClient(NULL);
}

void WebPopupMenuImpl::Init(WebCore::FramelessScrollView* widget,
                            const WebRect& bounds) {
  widget_ = widget;
  widget_->setClient(this);

  if (client_) {
    client_->setWindowRect(bounds);
    client_->show(WebNavigationPolicy());  // Policy is ignored
  }
}

void WebPopupMenuImpl::MouseMove(const WebMouseEvent& event) {
  // don't send mouse move messages if the mouse hasn't moved.
  if (event.x != last_mouse_position_.x ||
      event.y != last_mouse_position_.y) {
    last_mouse_position_ = WebPoint(event.x, event.y);
    widget_->handleMouseMoveEvent(PlatformMouseEventBuilder(widget_, event));
  }
}

void WebPopupMenuImpl::MouseLeave(const WebMouseEvent& event) {
  widget_->handleMouseMoveEvent(PlatformMouseEventBuilder(widget_, event));
}

void WebPopupMenuImpl::MouseDown(const WebMouseEvent& event) {
  widget_->handleMouseDownEvent(PlatformMouseEventBuilder(widget_, event));
}

void WebPopupMenuImpl::MouseUp(const WebMouseEvent& event) {
  mouseCaptureLost();
  widget_->handleMouseReleaseEvent(PlatformMouseEventBuilder(widget_, event));
}

void WebPopupMenuImpl::MouseWheel(const WebMouseWheelEvent& event) {
  widget_->handleWheelEvent(PlatformWheelEventBuilder(widget_, event));
}

bool WebPopupMenuImpl::KeyEvent(const WebKeyboardEvent& event) {
  return widget_->handleKeyEvent(PlatformKeyboardEventBuilder(event));
}

// WebWidget -------------------------------------------------------------------

void WebPopupMenuImpl::close() {
  if (widget_)
    widget_->hide();

  client_ = NULL;

  deref();  // Balances ref() from WebWidget::Create
}

void WebPopupMenuImpl::resize(const WebSize& new_size) {
  if (size_ == new_size)
    return;
  size_ = new_size;

  if (widget_) {
    IntRect new_geometry(0, 0, size_.width, size_.height);
    widget_->setFrameRect(new_geometry);
  }

  if (client_) {
    WebRect damaged_rect(0, 0, size_.width, size_.height);
    client_->didInvalidateRect(damaged_rect);
  }
}

void WebPopupMenuImpl::layout() {
}

void WebPopupMenuImpl::paint(WebCanvas* canvas, const WebRect& rect) {
  if (!widget_)
    return;

  if (!rect.isEmpty()) {
#if WEBKIT_USING_CG
    GraphicsContext gc(canvas);
#elif WEBKIT_USING_SKIA
    PlatformContextSkia context(canvas);
    // PlatformGraphicsContext is actually a pointer to PlatformContextSkia.
    GraphicsContext gc(reinterpret_cast<PlatformGraphicsContext*>(&context));
#else
    notImplemented();
#endif
    widget_->paint(&gc, webkit_glue::WebRectToIntRect(rect));
  }
}

bool WebPopupMenuImpl::handleInputEvent(const WebInputEvent& input_event) {
  if (!widget_)
    return false;

  // TODO (jcampan): WebKit seems to always return false on mouse events
  // methods. For now we'll assume it has processed them (as we are only
  // interested in whether keyboard events are processed).
  switch (input_event.type) {
    case WebInputEvent::MouseMove:
      MouseMove(*static_cast<const WebMouseEvent*>(&input_event));
      return true;

    case WebInputEvent::MouseLeave:
      MouseLeave(*static_cast<const WebMouseEvent*>(&input_event));
      return true;

    case WebInputEvent::MouseWheel:
      MouseWheel(*static_cast<const WebMouseWheelEvent*>(&input_event));
      return true;

    case WebInputEvent::MouseDown:
      MouseDown(*static_cast<const WebMouseEvent*>(&input_event));
      return true;

    case WebInputEvent::MouseUp:
      MouseUp(*static_cast<const WebMouseEvent*>(&input_event));
      return true;

    // In Windows, RawKeyDown only has information about the physical key, but
    // for "selection", we need the information about the character the key
    // translated into. For English, the physical key value and the character
    // value are the same, hence, "selection" works for English. But for other
    // languages, such as Hebrew, the character value is different from the
    // physical key value. Thus, without accepting Char event type which
    // contains the key's character value, the "selection" won't work for
    // non-English languages, such as Hebrew.
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
    case WebInputEvent::Char:
      return KeyEvent(*static_cast<const WebKeyboardEvent*>(&input_event));

    default:
      break;
  }
  return false;
}

void WebPopupMenuImpl::mouseCaptureLost() {
}

void WebPopupMenuImpl::setFocus(bool enable) {
}

bool WebPopupMenuImpl::handleCompositionEvent(
    WebCompositionCommand command,
    int cursor_position,
    int target_start,
    int target_end,
    const WebString& ime_string) {
  return false;
}

bool WebPopupMenuImpl::queryCompositionStatus(bool* enabled,
                                              WebRect* caret_rect) {
  return false;
}

void WebPopupMenuImpl::setTextDirection(WebTextDirection direction) {
}

//-----------------------------------------------------------------------------
// WebCore::HostWindow

void WebPopupMenuImpl::repaint(const WebCore::IntRect& paint_rect,
                            bool content_changed,
                            bool immediate,
                            bool repaint_content_only) {
  // Ignore spurious calls.
  if (!content_changed || paint_rect.isEmpty())
    return;
  if (client_)
    client_->didInvalidateRect(webkit_glue::IntRectToWebRect(paint_rect));
}

void WebPopupMenuImpl::scroll(const WebCore::IntSize& scroll_delta,
                           const WebCore::IntRect& scroll_rect,
                           const WebCore::IntRect& clip_rect) {
  if (client_) {
    int dx = scroll_delta.width();
    int dy = scroll_delta.height();
    client_->didScrollRect(dx, dy, webkit_glue::IntRectToWebRect(clip_rect));
  }
}

WebCore::IntPoint WebPopupMenuImpl::screenToWindow(
    const WebCore::IntPoint& point) const {
  notImplemented();
  return WebCore::IntPoint();
}

WebCore::IntRect WebPopupMenuImpl::windowToScreen(
    const WebCore::IntRect& rect) const {
  notImplemented();
  return WebCore::IntRect();
}

void WebPopupMenuImpl::scrollRectIntoView(
    const WebCore::IntRect&, const WebCore::ScrollView*) const {
  // Nothing to be done here since we do not have the concept of a container
  // that implements its own scrolling.
}

void WebPopupMenuImpl::scrollbarsModeDidChange() const {
  // Nothing to be done since we have no concept of different scrollbar modes.
}

//-----------------------------------------------------------------------------
// WebCore::FramelessScrollViewClient

void WebPopupMenuImpl::popupClosed(WebCore::FramelessScrollView* widget) {
  ASSERT(widget == widget_);
  if (widget_) {
    widget_->setClient(NULL);
    widget_ = NULL;
  }
  client_->closeWidgetSoon();
}
