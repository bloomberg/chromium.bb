// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of TestWebViewDelegate, which serves
// as the WebViewDelegate for the TestShellWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#include <objidl.h>
#include <shlobj.h>
#include <shlwapi.h>

#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/tools/test_shell/drop_delegate.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"

using WebKit::WebCursorInfo;
using WebKit::WebNavigationPolicy;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebWidget;

// WebViewClient --------------------------------------------------------------

WebWidget* TestWebViewDelegate::createPopupMenu(
    const WebPopupMenuInfo& info) {
  NOTREACHED();
  return NULL;
}

// WebWidgetClient ------------------------------------------------------------

void TestWebViewDelegate::show(WebNavigationPolicy) {
  if (WebWidgetHost* host = GetWidgetHost()) {
    HWND root = GetAncestor(host->view_handle(), GA_ROOT);
    ShowWindow(root, SW_SHOW);
    UpdateWindow(root);
  }
}

void TestWebViewDelegate::closeWidgetSoon() {
  if (this == shell_->delegate()) {
    PostMessage(shell_->mainWnd(), WM_CLOSE, 0, 0);
  } else if (this == shell_->popup_delegate()) {
    shell_->ClosePopup();
  }
}

void TestWebViewDelegate::didChangeCursor(const WebCursorInfo& cursor_info) {
  if (WebWidgetHost* host = GetWidgetHost()) {
    current_cursor_.InitFromCursorInfo(cursor_info);
    HINSTANCE mod_handle = GetModuleHandle(NULL);
    host->SetCursor(current_cursor_.GetCursor(mod_handle));
  }
}

WebRect TestWebViewDelegate::windowRect() {
  if (WebWidgetHost* host = GetWidgetHost()) {
    RECT rect;
    ::GetWindowRect(host->view_handle(), &rect);
    return gfx::Rect(rect);
  }
  return WebRect();
}

void TestWebViewDelegate::setWindowRect(const WebRect& rect) {
  if (this == shell_->delegate()) {
    set_fake_window_rect(rect);
  } else if (this == shell_->popup_delegate()) {
    MoveWindow(shell_->popupWnd(),
               rect.x, rect.y, rect.width, rect.height, FALSE);
  }
}

WebRect TestWebViewDelegate::rootWindowRect() {
  if (using_fake_rect_) {
    return fake_window_rect();
  }
  if (WebWidgetHost* host = GetWidgetHost()) {
    RECT rect;
    HWND root_window = ::GetAncestor(host->view_handle(), GA_ROOT);
    ::GetWindowRect(root_window, &rect);
    return gfx::Rect(rect);
  }
  return WebRect();
}

WebRect TestWebViewDelegate::windowResizerRect() {
  // Not necessary on Windows.
  return WebRect();
}

void TestWebViewDelegate::runModal() {
  WebWidgetHost* host = GetWidgetHost();
  if (!host)
    return;

  show(WebKit::WebNavigationPolicyNewWindow);

  WindowList* wl = TestShell::windowList();
  for (WindowList::const_iterator i = wl->begin(); i != wl->end(); ++i) {
    if (*i != shell_->mainWnd())
      EnableWindow(*i, FALSE);
  }

  shell_->set_is_modal(true);
  MessageLoop::current()->Run();

  for (WindowList::const_iterator i = wl->begin(); i != wl->end(); ++i)
    EnableWindow(*i, TRUE);
}

// WebPluginPageDelegate ------------------------------------------------------

webkit::npapi::WebPluginDelegate* TestWebViewDelegate::CreatePluginDelegate(
    const FilePath& path,
    const std::string& mime_type) {
  HWND hwnd = shell_->webViewHost()->view_handle();
  if (!hwnd)
    return NULL;

  return webkit::npapi::WebPluginDelegateImpl::Create(path, mime_type);
}

void TestWebViewDelegate::CreatedPluginWindow(
    gfx::PluginWindowHandle handle) {
  // ignored
}

void TestWebViewDelegate::WillDestroyPluginWindow(
    gfx::PluginWindowHandle handle) {
  // ignored
}

void TestWebViewDelegate::DidMovePlugin(
    const webkit::npapi::WebPluginGeometry& move) {
  unsigned long flags = 0;

  if (move.rects_valid) {
    HRGN hrgn = ::CreateRectRgn(move.clip_rect.x(),
                                move.clip_rect.y(),
                                move.clip_rect.right(),
                                move.clip_rect.bottom());
    gfx::SubtractRectanglesFromRegion(hrgn, move.cutout_rects);

    // Note: System will own the hrgn after we call SetWindowRgn,
    // so we don't need to call DeleteObject(hrgn)
    ::SetWindowRgn(move.window, hrgn, FALSE);
  } else {
    flags |= (SWP_NOSIZE | SWP_NOMOVE);
  }

  if (move.visible)
    flags |= SWP_SHOWWINDOW;
  else
    flags |= SWP_HIDEWINDOW;

  ::SetWindowPos(move.window,
                 NULL,
                 move.window_rect.x(),
                 move.window_rect.y(),
                 move.window_rect.width(),
                 move.window_rect.height(),
                 flags);
}

// Public methods -------------------------------------------------------------

void TestWebViewDelegate::UpdateSelectionClipboard(bool is_empty_selection) {
  // No selection clipboard on windows, do nothing.
}

// Private methods ------------------------------------------------------------

void TestWebViewDelegate::ShowJavaScriptAlert(const string16& message) {
  MessageBox(NULL, message.c_str(), L"JavaScript Alert", MB_OK);
}

void TestWebViewDelegate::SetPageTitle(const string16& title) {
  // The Windows test shell, pre-refactoring, ignored this.  *shrug*
}

void TestWebViewDelegate::SetAddressBarURL(const GURL& url) {
  string16 url_string = UTF8ToUTF16(url.spec());
  SendMessage(shell_->editWnd(), WM_SETTEXT, 0,
              reinterpret_cast<LPARAM>(url_string.c_str()));
}
