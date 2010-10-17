// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_DRAG_DELEGATE_H__
#define WEBKIT_TOOLS_TEST_SHELL_DRAG_DELEGATE_H__

#include "app/win/drag_source.h"

namespace WebKit {
class WebView;
}

// A class that implements app::win::DragSource for the test shell webview
// delegate.
class TestDragDelegate : public app::win::DragSource {
 public:
  TestDragDelegate(HWND source_hwnd, WebKit::WebView* webview)
      : app::win::DragSource(),
        source_hwnd_(source_hwnd),
        webview_(webview) { }

 protected:
  // app::win::DragSource
  virtual void OnDragSourceCancel();
  virtual void OnDragSourceDrop();
  virtual void OnDragSourceMove();

 private:
  WebKit::WebView* webview_;

  // A HWND for the source we are associated with, used for translating
  // mouse coordinates from screen to client coordinates.
  HWND source_hwnd_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_DRAG_DELEGATE_H__
