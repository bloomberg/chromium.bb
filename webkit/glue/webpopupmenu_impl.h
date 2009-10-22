// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBPOPUPMENU_IMPL_H_
#define WEBKIT_GLUE_WEBPOPUPMENU_IMPL_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebPopupMenu.h"
#include "webkit/api/public/WebSize.h"

#include "FramelessScrollViewClient.h"

namespace WebCore {
class Frame;
class FramelessScrollView;
class KeyboardEvent;
class Page;
class PlatformKeyboardEvent;
class Range;
class Widget;
}

namespace WebKit {
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
struct WebRect;
}

struct MenuItem;

class WebPopupMenuImpl : public WebKit::WebPopupMenu,
                         public WebCore::FramelessScrollViewClient,
                         public base::RefCounted<WebPopupMenuImpl> {
 public:
  // WebWidget
  virtual void close();
  virtual WebKit::WebSize size() { return size_; }
  virtual void resize(const WebKit::WebSize& new_size);
  virtual void layout();
  virtual void paint(WebKit::WebCanvas* canvas,
                     const WebKit::WebRect& rect);
  virtual bool handleInputEvent(const WebKit::WebInputEvent& input_event);
  virtual void mouseCaptureLost();
  virtual void setFocus(bool enable);
  virtual bool handleCompositionEvent(WebKit::WebCompositionCommand command,
                                      int cursor_position,
                                      int target_start,
                                      int target_end,
                                      const WebKit::WebString& text);
  virtual bool queryCompositionStatus(bool* enabled,
                                      WebKit::WebRect* caret_rect);
  virtual void setTextDirection(WebKit::WebTextDirection direction);

  // WebPopupMenuImpl
  void Init(WebCore::FramelessScrollView* widget,
            const WebKit::WebRect& bounds);

  WebKit::WebWidgetClient* client() {
    return client_;
  }

  void MouseMove(const WebKit::WebMouseEvent& mouse_event);
  void MouseLeave(const WebKit::WebMouseEvent& mouse_event);
  void MouseDown(const WebKit::WebMouseEvent& mouse_event);
  void MouseUp(const WebKit::WebMouseEvent& mouse_event);
  void MouseDoubleClick(const WebKit::WebMouseEvent& mouse_event);
  void MouseWheel(const WebKit::WebMouseWheelEvent& wheel_event);
  bool KeyEvent(const WebKit::WebKeyboardEvent& key_event);

 protected:
  friend class WebKit::WebPopupMenu;  // For WebPopupMenu::create
  friend class base::RefCounted<WebPopupMenuImpl>;

  WebPopupMenuImpl(WebKit::WebWidgetClient* client);
  ~WebPopupMenuImpl();

  // WebCore::HostWindow methods:
  virtual void repaint(const WebCore::IntRect&,
                       bool content_changed,
                       bool immediate = false,
                       bool repaint_content_only = false);
  virtual void scroll(const WebCore::IntSize& scroll_delta,
                      const WebCore::IntRect& scroll_rect,
                      const WebCore::IntRect& clip_rect);
  virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&) const;
  virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&) const;
  virtual PlatformPageClient platformPageClient() const { return NULL; }
  virtual void scrollRectIntoView(const WebCore::IntRect&,
                                  const WebCore::ScrollView*) const;
  virtual void scrollbarsModeDidChange() const;

  // WebCore::FramelessScrollViewClient methods:
  virtual void popupClosed(WebCore::FramelessScrollView* popup_view);

  WebKit::WebWidgetClient* client_;
  WebKit::WebSize size_;

  WebKit::WebPoint last_mouse_position_;

  // This is a non-owning ref.  The popup will notify us via popupClosed()
  // before it is destroyed.
  WebCore::FramelessScrollView* widget_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebPopupMenuImpl);
};

#endif  // WEBKIT_GLUE_WEBPOPUPMENU_IMPL_H_
