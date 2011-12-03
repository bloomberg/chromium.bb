// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was forked off the Mac port.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/point.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/npapi/gtk_plugin_container_manager.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"

using WebKit::WebCursorInfo;
using WebKit::WebFrame;
using WebKit::WebNavigationPolicy;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebWidget;
using WebKit::WebView;

namespace {

enum SelectionClipboardType {
  TEXT_HTML,
  PLAIN_TEXT,
};

GdkAtom GetTextHtmlAtom() {
  GdkAtom kTextHtmlGdkAtom = gdk_atom_intern_static_string("text/html");
  return kTextHtmlGdkAtom;
}

void SelectionClipboardGetContents(GtkClipboard* clipboard,
    GtkSelectionData* selection_data, guint info, gpointer data) {
  // Ignore formats that we don't know about.
  if (info != TEXT_HTML && info != PLAIN_TEXT)
    return;

  WebView* webview = static_cast<WebView*>(data);
  WebFrame* frame = webview->focusedFrame();
  if (!frame)
    frame = webview->mainFrame();
  DCHECK(frame);

  std::string selection;
  if (TEXT_HTML == info) {
    selection = frame->selectionAsMarkup().utf8();
  } else {
    selection = frame->selectionAsText().utf8();
  }
  if (TEXT_HTML == info) {
    gtk_selection_data_set(selection_data,
                           GetTextHtmlAtom(),
                           8 /* bits per data unit, ie, char */,
                           reinterpret_cast<const guchar*>(selection.data()),
                           selection.length());
  } else {
    gtk_selection_data_set_text(selection_data, selection.data(),
        selection.length());
  }
}

}  // namespace

// WebViewClient --------------------------------------------------------------

WebWidget* TestWebViewDelegate::createPopupMenu(
    const WebPopupMenuInfo& info) {
  NOTREACHED();
  return NULL;
}

// WebWidgetClient ------------------------------------------------------------

void TestWebViewDelegate::show(WebNavigationPolicy policy) {
  WebWidgetHost* host = GetWidgetHost();
  GtkWidget* drawing_area = host->view_handle();
  GtkWidget* window =
      gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
  gtk_widget_show_all(window);
}

void TestWebViewDelegate::closeWidgetSoon() {
  if (this == shell_->delegate()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&gtk_widget_destroy, GTK_WIDGET(shell_->mainWnd())));
  } else if (this == shell_->popup_delegate()) {
    shell_->ClosePopup();
  }
}

void TestWebViewDelegate::didChangeCursor(const WebCursorInfo& cursor_info) {
  current_cursor_.InitFromCursorInfo(cursor_info);
  GdkCursorType cursor_type =
      static_cast<GdkCursorType>(current_cursor_.GetCursorType());
  GdkCursor* gdk_cursor;
  if (cursor_type == GDK_CURSOR_IS_PIXMAP) {
    // TODO(port): WebKit bug https://bugs.webkit.org/show_bug.cgi?id=16388 is
    // that calling gdk_window_set_cursor repeatedly is expensive.  We should
    // avoid it here where possible.
    gdk_cursor = current_cursor_.GetCustomCursor();
  } else {
    // Optimize the common case, where the cursor hasn't changed.
    // However, we can switch between different pixmaps, so only on the
    // non-pixmap branch.
    if (cursor_type_ == cursor_type)
      return;
    if (cursor_type == GDK_LAST_CURSOR)
      gdk_cursor = NULL;
    else
      gdk_cursor = gfx::GetCursor(cursor_type);
  }
  cursor_type_ = cursor_type;
  gdk_window_set_cursor(shell_->webViewWnd()->window, gdk_cursor);
}

WebRect TestWebViewDelegate::windowRect() {
  WebWidgetHost* host = GetWidgetHost();
  GtkWidget* drawing_area = host->view_handle();
  GtkWidget* vbox = gtk_widget_get_parent(drawing_area);
  GtkWidget* window = gtk_widget_get_parent(vbox);

  gint x, y;
  gtk_window_get_position(GTK_WINDOW(window), &x, &y);
  x += vbox->allocation.x + drawing_area->allocation.x;
  y += vbox->allocation.y + drawing_area->allocation.y;

  return WebRect(x, y,
                 drawing_area->allocation.width,
                 drawing_area->allocation.height);
}

void TestWebViewDelegate::setWindowRect(const WebRect& rect) {
  if (this == shell_->delegate()) {
    set_fake_window_rect(rect);
  } else if (this == shell_->popup_delegate()) {
    WebWidgetHost* host = GetWidgetHost();
    GtkWidget* drawing_area = host->view_handle();
    GtkWidget* window =
        gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
    gtk_window_resize(GTK_WINDOW(window), rect.width, rect.height);
    gtk_window_move(GTK_WINDOW(window), rect.x, rect.y);
  }
}

WebRect TestWebViewDelegate::rootWindowRect() {
  if (using_fake_rect_) {
    return fake_window_rect();
  }
  if (WebWidgetHost* host = GetWidgetHost()) {
    // We are being asked for the x/y and width/height of the entire browser
    // window.  This means the x/y is the distance from the corner of the
    // screen, and the width/height is the size of the entire browser window.
    // For example, this is used to implement window.screenX and window.screenY.
    GtkWidget* drawing_area = host->view_handle();
    GtkWidget* window =
        gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
    gint x, y, width, height;
    gtk_window_get_position(GTK_WINDOW(window), &x, &y);
    gtk_window_get_size(GTK_WINDOW(window), &width, &height);
    return WebRect(x, y, width, height);
  }
  return WebRect();
}

WebRect TestWebViewDelegate::windowResizerRect() {
  // Not necessary on Linux.
  return WebRect();
}

void TestWebViewDelegate::runModal() {
  NOTIMPLEMENTED();
}

// WebPluginPageDelegate ------------------------------------------------------

webkit::npapi::WebPluginDelegate* TestWebViewDelegate::CreatePluginDelegate(
    const FilePath& path,
    const std::string& mime_type) {
  // TODO(evanm): we probably shouldn't be doing this mapping to X ids at
  // this level.
  GdkNativeWindow plugin_parent =
      GDK_WINDOW_XWINDOW(shell_->webViewHost()->view_handle()->window);

  return webkit::npapi::WebPluginDelegateImpl::Create(
      path, mime_type, plugin_parent);
}

void TestWebViewDelegate::CreatedPluginWindow(
    gfx::PluginWindowHandle id) {
  shell_->webViewHost()->CreatePluginContainer(id);
}

void TestWebViewDelegate::WillDestroyPluginWindow(
    gfx::PluginWindowHandle id) {
  shell_->webViewHost()->DestroyPluginContainer(id);
}

void TestWebViewDelegate::DidMovePlugin(
    const webkit::npapi::WebPluginGeometry& move) {
  WebWidgetHost* host = GetWidgetHost();
  webkit::npapi::GtkPluginContainerManager* plugin_container_manager =
      static_cast<WebViewHost*>(host)->plugin_container_manager();
  plugin_container_manager->MovePluginContainer(move);
}

// Public methods -------------------------------------------------------------

void TestWebViewDelegate::UpdateSelectionClipboard(bool is_empty_selection) {
  if (is_empty_selection)
    return;

  GtkClipboard* clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  // Put data on the X clipboard.  This doesn't actually grab the text from
  // the HTML, it just registers a callback for when someone tries to paste.
  GtkTargetList* target_list = gtk_target_list_new(NULL, 0);
  gtk_target_list_add(target_list, GetTextHtmlAtom(), 0, TEXT_HTML);
  gtk_target_list_add_text_targets(target_list, PLAIN_TEXT);

  gint num_targets = 0;
  GtkTargetEntry* targets = gtk_target_table_new_from_list(target_list,
                                                           &num_targets);
  gtk_clipboard_set_with_data(clipboard, targets, num_targets,
                              SelectionClipboardGetContents, NULL,
                              shell_->webView());
  gtk_target_list_unref(target_list);
  gtk_target_table_free(targets, num_targets);
}

// Private methods ------------------------------------------------------------

void TestWebViewDelegate::ShowJavaScriptAlert(const std::wstring& message) {
  GtkWidget* dialog = gtk_message_dialog_new(
      shell_->mainWnd(), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
      GTK_BUTTONS_OK, "%s", WideToUTF8(message).c_str());
  gtk_window_set_title(GTK_WINDOW(dialog), "JavaScript Alert");
  gtk_dialog_run(GTK_DIALOG(dialog));  // Runs a nested message loop.
  gtk_widget_destroy(dialog);
}

void TestWebViewDelegate::SetPageTitle(const string16& title) {
  gtk_window_set_title(GTK_WINDOW(shell_->mainWnd()),
                       ("Test Shell - " + UTF16ToUTF8(title)).c_str());
}

void TestWebViewDelegate::SetAddressBarURL(const GURL& url) {
  gtk_entry_set_text(GTK_ENTRY(shell_->editWnd()), url.spec().c_str());
}
