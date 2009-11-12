// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#import <Cocoa/Cocoa.h>
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/webmenurunner_mac.h"
#include "webkit/tools/test_shell/test_shell.h"

using WebKit::WebCursorInfo;
using WebKit::WebNavigationPolicy;
using WebKit::WebPopupMenu;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebWidget;

// WebViewClient --------------------------------------------------------------

WebWidget* TestWebViewDelegate::createPopupMenu(
    const WebPopupMenuInfo& info) {
  WebWidget* webwidget = shell_->CreatePopupWidget();
  popup_menu_info_.reset(new WebPopupMenuInfo(info));
  return webwidget;
}

// WebWidgetClient ------------------------------------------------------------

void TestWebViewDelegate::show(WebNavigationPolicy policy) {
  if (!popup_menu_info_.get())
    return;
  if (this != shell_->popup_delegate())
    return;
  // Display a HTML select menu.

  std::vector<WebMenuItem> items;
  for (size_t i = 0; i < popup_menu_info_->items.size(); ++i)
    items.push_back(popup_menu_info_->items[i]);

  int item_height = popup_menu_info_->itemHeight;
  int selected_index = popup_menu_info_->selectedIndex;
  popup_menu_info_.reset();  // No longer needed.

  const WebRect& bounds = popup_bounds_;

  // Set up the menu position.
  NSView* web_view = shell_->webViewWnd();
  NSRect view_rect = [web_view bounds];
  int y_offset = bounds.y + bounds.height;
  NSRect position = NSMakeRect(bounds.x, view_rect.size.height - y_offset,
                               bounds.width, bounds.height);

  // Display the menu.
  scoped_nsobject<WebMenuRunner> menu_runner;
  menu_runner.reset([[WebMenuRunner alloc] initWithItems:items]);

  [menu_runner runMenuInView:shell_->webViewWnd()
                  withBounds:position
                initialIndex:selected_index];

  // Get the selected item and forward to WebKit. WebKit expects an input event
  // (mouse down, keyboard activity) for this, so we calculate the proper
  // position based on the selected index and provided bounds.
  WebWidgetHost* popup = shell_->popupHost();
  int window_num = [shell_->mainWnd() windowNumber];
  NSEvent* event =
      webkit_glue::EventWithMenuAction([menu_runner menuItemWasChosen],
                                       window_num, item_height,
                                       [menu_runner indexOfSelectedItem],
                                       position, view_rect);

  if ([menu_runner menuItemWasChosen]) {
    // Construct a mouse up event to simulate the selection of an appropriate
    // menu item.
    popup->MouseEvent(event);
  } else {
    // Fake an ESC key event (keyCode = 0x1B, from webinputevent_mac.mm) and
    // forward that to WebKit.
    popup->KeyEvent(event);
  }
}

void TestWebViewDelegate::closeWidgetSoon() {
  if (this == shell_->delegate()) {
    NSWindow *win = shell_->mainWnd();
    [win performSelector:@selector(performClose:) withObject:nil afterDelay:0];
  } else if (this == shell_->popup_delegate()) {
    shell_->ClosePopup();
  }
}

void TestWebViewDelegate::didChangeCursor(const WebCursorInfo& cursor_info) {
  NSCursor* ns_cursor = WebCursor(cursor_info).GetCursor();
  [ns_cursor set];
}

WebRect TestWebViewDelegate::windowRect() {
  if (WebWidgetHost* host = GetWidgetHost()) {
    NSView *view = host->view_handle();
    NSRect rect = [view frame];
    return gfx::Rect(NSRectToCGRect(rect));
  }
  return WebRect();
}

void TestWebViewDelegate::setWindowRect(const WebRect& rect) {
  if (this == shell_->delegate()) {
    set_fake_window_rect(rect);
  } else if (this == shell_->popup_delegate()) {
    popup_bounds_ = rect;  // The initial position of the popup.
  }
}

WebRect TestWebViewDelegate::rootWindowRect() {
  if (using_fake_rect_) {
    return fake_window_rect();
  }
  if (WebWidgetHost* host = GetWidgetHost()) {
    NSView *view = host->view_handle();
    NSRect rect = [[[view window] contentView] frame];
    return gfx::Rect(NSRectToCGRect(rect));
  }
  return WebRect();
}

@interface NSWindow(OSInternals)
- (NSRect)_growBoxRect;
@end

WebRect TestWebViewDelegate::windowResizerRect() {
  NSRect resize_rect = NSMakeRect(0, 0, 0, 0);
  WebWidgetHost* host = GetWidgetHost();
  // To match the WebKit screen shots, we need the resize area to overlap
  // the scroll arrows, so in layout test mode, we don't return a real rect.
  if (!(shell_->layout_test_mode()) && host) {
    NSView *view = host->view_handle();
    NSWindow* window = [view window];
    resize_rect = [window _growBoxRect];
    // The scrollbar assumes that the resizer goes all the way down to the
    // bottom corner, so we ignore any y offset to the rect itself and use the
    // entire bottom corner.
    resize_rect.origin.y = 0;
    // Convert to view coordinates from window coordinates.
    resize_rect = [view convertRect:resize_rect fromView:nil];
    // Flip the rect in view coordinates
    resize_rect.origin.y =
        [view frame].size.height - resize_rect.origin.y -
        resize_rect.size.height;
  }
  return gfx::Rect(NSRectToCGRect(resize_rect));
}

void TestWebViewDelegate::runModal() {
  NOTIMPLEMENTED();
}

// WebPluginPageDelegate ------------------------------------------------------

webkit_glue::WebPluginDelegate* TestWebViewDelegate::CreatePluginDelegate(
    const GURL& url,
    const std::string& mime_type,
    std::string* actual_mime_type) {
  WebWidgetHost *host = GetWidgetHost();
  if (!host)
    return NULL;
  gfx::NativeView view = host->view_handle();

  bool allow_wildcard = true;
  WebPluginInfo info;
  if (!NPAPI::PluginList::Singleton()->GetPluginInfo(
          url, mime_type,  allow_wildcard, &info, actual_mime_type)) {
    return NULL;
  }

  if (actual_mime_type && !actual_mime_type->empty())
    return WebPluginDelegateImpl::Create(info.path, *actual_mime_type, view);
  else
    return WebPluginDelegateImpl::Create(info.path, mime_type, view);
}

void TestWebViewDelegate::CreatedPluginWindow(
    gfx::PluginWindowHandle handle) {
}

void TestWebViewDelegate::WillDestroyPluginWindow(
    gfx::PluginWindowHandle handle) {
}

void TestWebViewDelegate::DidMovePlugin(
    const webkit_glue::WebPluginGeometry& move) {
  // TODO(port): add me once plugins work.
}

// Public methods -------------------------------------------------------------

void TestWebViewDelegate::UpdateSelectionClipboard(bool is_empty_selection) {
  // No selection clipboard on mac, do nothing.
}

// Private methods ------------------------------------------------------------

void TestWebViewDelegate::ShowJavaScriptAlert(const std::wstring& message) {
  NSString *text =
      [NSString stringWithUTF8String:WideToUTF8(message).c_str()];
  NSAlert *alert = [NSAlert alertWithMessageText:@"JavaScript Alert"
                                   defaultButton:@"OK"
                                 alternateButton:nil
                                     otherButton:nil
                       informativeTextWithFormat:text];
  [alert runModal];
}

void TestWebViewDelegate::SetPageTitle(const std::wstring& title) {
  [[shell_->webViewHost()->view_handle() window]
      setTitle:[NSString stringWithUTF8String:WideToUTF8(title).c_str()]];
}

void TestWebViewDelegate::SetAddressBarURL(const GURL& url) {
  const char* frameURL = url.spec().c_str();
  NSString *address = [NSString stringWithUTF8String:frameURL];
  [shell_->editWnd() setStringValue:address];
}
