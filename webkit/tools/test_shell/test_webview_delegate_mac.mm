// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#import <Cocoa/Cocoa.h>
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webmenurunner_mac.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/tools/test_shell/test_shell.h"

using webkit::npapi::WebPluginDelegateImpl;
using WebKit::WebCursorInfo;
using WebKit::WebNavigationPolicy;
using WebKit::WebPopupMenu;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebWidget;

namespace {

// Helper function for manufacturing input events to send to WebKit.
NSEvent* EventWithMenuAction(BOOL item_chosen, int window_num,
                             int item_height, int selected_index,
                             NSRect menu_bounds, NSRect view_bounds) {
  NSEvent* event = nil;
  double event_time = (double)(AbsoluteToDuration(UpTime())) / 1000.0;

  if (item_chosen) {
    // Construct a mouse up event to simulate the selection of an appropriate
    // menu item.
    NSPoint click_pos;
    click_pos.x = menu_bounds.size.width / 2;

    // This is going to be hard to calculate since the button is painted by
    // WebKit, the menu by Cocoa, and we have to translate the selected_item
    // index to a coordinate that WebKit's PopupMenu expects which uses a
    // different font *and* expects to draw the menu below the button like we do
    // on Windows.
    // The WebKit popup menu thinks it will draw just below the button, so
    // create the click at the offset based on the selected item's index and
    // account for the different coordinate system used by NSView.
    int item_offset = selected_index * item_height + item_height / 2;
    click_pos.y = view_bounds.size.height - item_offset;
    event = [NSEvent mouseEventWithType:NSLeftMouseUp
                               location:click_pos
                          modifierFlags:0
                              timestamp:event_time
                           windowNumber:window_num
                                context:nil
                            eventNumber:0
                             clickCount:1
                               pressure:1.0];
  } else {
    // Fake an ESC key event (keyCode = 0x1B, from webinputevent_mac.mm) and
    // forward that to WebKit.
    NSPoint key_pos;
    key_pos.x = 0;
    key_pos.y = 0;
    NSString* escape_str = [NSString stringWithFormat:@"%c", 0x1B];
    event = [NSEvent keyEventWithType:NSKeyDown
                             location:key_pos
                        modifierFlags:0
                            timestamp:event_time
                         windowNumber:window_num
                              context:nil
                           characters:@""
          charactersIgnoringModifiers:escape_str
                            isARepeat:NO
                              keyCode:0x1B];
  }

  return event;
}

}  // namespace

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
  double font_size = popup_menu_info_->itemFontSize;
  int selected_index = popup_menu_info_->selectedIndex;
  bool right_aligned = popup_menu_info_->rightAligned;
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
  menu_runner.reset([[WebMenuRunner alloc] initWithItems:items
                                                fontSize:font_size
                                            rightAligned:right_aligned]);

  [menu_runner runMenuInView:shell_->webViewWnd()
                  withBounds:position
                initialIndex:selected_index];

  // Get the selected item and forward to WebKit. WebKit expects an input event
  // (mouse down, keyboard activity) for this, so we calculate the proper
  // position based on the selected index and provided bounds.
  WebWidgetHost* popup = shell_->popupHost();
  int window_num = [shell_->mainWnd() windowNumber];
  NSEvent* event = EventWithMenuAction([menu_runner menuItemWasChosen],
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
  NSCursor* ns_cursor = WebCursor(cursor_info).GetNativeCursor();
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

webkit::npapi::WebPluginDelegate* TestWebViewDelegate::CreatePluginDelegate(
    const base::FilePath& path,
    const std::string& mime_type) {
  WebWidgetHost *host = GetWidgetHost();
  if (!host)
    return NULL;

  WebPluginDelegateImpl* delegate =
      WebPluginDelegateImpl::Create(path, mime_type);
  if (delegate)
    delegate->SetNoBufferContext();
  return delegate;
}

void TestWebViewDelegate::CreatedPluginWindow(
    gfx::PluginWindowHandle handle) {
}

void TestWebViewDelegate::WillDestroyPluginWindow(
    gfx::PluginWindowHandle handle) {
}

void TestWebViewDelegate::DidMovePlugin(
    const webkit::npapi::WebPluginGeometry& move) {
  // TODO(port): add me once plugins work.
}

// Public methods -------------------------------------------------------------

void TestWebViewDelegate::UpdateSelectionClipboard(bool is_empty_selection) {
  // No selection clipboard on mac, do nothing.
}

// Private methods ------------------------------------------------------------

void TestWebViewDelegate::ShowJavaScriptAlert(const base::string16& message) {
  NSString *text =
      [NSString stringWithUTF8String:UTF16ToUTF8(message).c_str()];
  NSAlert *alert = [NSAlert alertWithMessageText:@"JavaScript Alert"
                                   defaultButton:@"OK"
                                 alternateButton:nil
                                     otherButton:nil
                       informativeTextWithFormat:@"%@", text];
  [alert runModal];
}

void TestWebViewDelegate::SetPageTitle(const base::string16& title) {
  [[shell_->webViewHost()->view_handle() window]
      setTitle:[NSString stringWithUTF8String:UTF16ToUTF8(title).c_str()]];
}

void TestWebViewDelegate::SetAddressBarURL(const GURL& url) {
  const char* frameURL = url.spec().c_str();
  NSString *address = [NSString stringWithUTF8String:frameURL];
  [shell_->editWnd() setStringValue:address];
}
