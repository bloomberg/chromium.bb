// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_delegate.h"

#include <algorithm>

#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/app/resource.h"
#include "content/shell/browser/shell.h"
#import "ui/base/cocoa/underlay_opengl_hosting_window.h"
#include "url/gurl.h"

// Receives notification that the window is closing so that it can start the
// tear-down process. Is responsible for deleting itself when done.
@interface ContentShellWindowDelegate : NSObject <NSWindowDelegate> {
 @private
  content::Shell* _shell;
}
- (id)initWithShell:(content::Shell*)shell;
@end

@implementation ContentShellWindowDelegate

- (id)initWithShell:(content::Shell*)shell {
  if ((self = [super init])) {
    _shell = shell;
  }
  return self;
}

// Called when the window is about to close. Perform the self-destruction
// sequence by getting rid of the shell and removing it and the window from
// the various global lists. By returning YES, we allow the window to be
// removed from the screen.
- (BOOL)windowShouldClose:(id)sender {
  NSWindow* window = base::mac::ObjCCastStrict<NSWindow>(sender);
  [window autorelease];
  // Don't leave a dangling pointer if the window lives beyond
  // this method. See crbug.com/719830.
  [window setDelegate:nil];
  delete _shell;
  [self release];

  return YES;
}

- (void)performAction:(id)sender {
  _shell->ActionPerformed([sender tag]);
}

- (void)takeURLStringValueFrom:(id)sender {
  _shell->URLEntered(base::SysNSStringToUTF8([sender stringValue]));
}

@end

@interface CrShellWindow : UnderlayOpenGLHostingWindow {
 @private
  content::Shell* _shell;
}
- (void)setShell:(content::Shell*)shell;
- (void)showDevTools:(id)sender;
@end

@implementation CrShellWindow

- (void)setShell:(content::Shell*)shell {
  _shell = shell;
}

- (void)showDevTools:(id)sender {
  _shell->ShowDevTools();
}

@end

namespace {

NSString* kWindowTitle = @"Content Shell";

// Layout constants (in view coordinates)
const CGFloat kButtonWidth = 72;
const CGFloat kURLBarHeight = 24;

// The minimum size of the window's content (in view coordinates)
const CGFloat kMinimumWindowWidth = 400;
const CGFloat kMinimumWindowHeight = 300;

void MakeShellButton(NSRect* rect,
                     NSString* title,
                     NSView* parent,
                     int control,
                     NSView* target,
                     NSString* key,
                     NSUInteger modifier) {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:*rect]);
  [button setTitle:title];
  [button setBezelStyle:NSSmallSquareBezelStyle];
  [button setAutoresizingMask:(NSViewMaxXMargin | NSViewMinYMargin)];
  [button setTarget:target];
  [button setAction:@selector(performAction:)];
  [button setTag:control];
  [button setKeyEquivalent:key];
  [button setKeyEquivalentModifierMask:modifier];
  [parent addSubview:button];
  rect->origin.x += kButtonWidth;
}

}  // namespace

namespace content {

struct ShellPlatformDelegate::ShellData {
  gfx::NativeWindow window;
  NSTextField* url_edit_view = nullptr;
};

struct ShellPlatformDelegate::PlatformData {};

ShellPlatformDelegate::ShellPlatformDelegate() = default;
ShellPlatformDelegate::~ShellPlatformDelegate() = default;

void ShellPlatformDelegate::Initialize(const gfx::Size& default_window_size) {
  // |platform_| is unused on this platform.
}

void ShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  DCHECK(!shell->headless());

  DCHECK(!base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  int width = initial_size.width();
  int height = initial_size.height();

  if (!Shell::ShouldHideToolbar())
    height += kURLBarHeight;
  NSRect initial_window_bounds = NSMakeRect(0, 0, width, height);
  NSRect content_rect = initial_window_bounds;
  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                          NSMiniaturizableWindowMask | NSResizableWindowMask;
  CrShellWindow* window =
      [[CrShellWindow alloc] initWithContentRect:content_rect
                                       styleMask:style_mask
                                         backing:NSBackingStoreBuffered
                                           defer:NO];
  [window setShell:shell];
  [window setTitle:kWindowTitle];
  NSView* content = [window contentView];

  // If the window is allowed to get too small, it will wreck the view bindings.
  NSSize min_size = NSMakeSize(kMinimumWindowWidth, kMinimumWindowHeight);
  min_size = [content convertSize:min_size toView:nil];
  // Note that this takes window coordinates.
  [window setContentMinSize:min_size];

  // Set the shell window to participate in Lion Fullscreen mode. Set
  // Setting this flag has no effect on Snow Leopard or earlier.
  NSUInteger collectionBehavior = [window collectionBehavior];
  collectionBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
  [window setCollectionBehavior:collectionBehavior];

  // Rely on the window delegate to clean us up rather than immediately
  // releasing when the window gets closed. We use the delegate to do
  // everything from the autorelease pool so the shell isn't on the stack
  // during cleanup (ie, a window close from javascript).
  [window setReleasedWhenClosed:NO];

  // Create a window delegate to watch for when it's asked to go away. It will
  // clean itself up so we don't need to hold a reference.
  ContentShellWindowDelegate* delegate =
      [[ContentShellWindowDelegate alloc] initWithShell:shell];
  [window setDelegate:delegate];

  if (!Shell::ShouldHideToolbar()) {
    NSRect button_frame =
        NSMakeRect(0, NSMaxY(initial_window_bounds) - kURLBarHeight,
                   kButtonWidth, kURLBarHeight);

    MakeShellButton(&button_frame, @"Back", content, IDC_NAV_BACK,
                    (NSView*)delegate, @"[", NSCommandKeyMask);
    MakeShellButton(&button_frame, @"Forward", content, IDC_NAV_FORWARD,
                    (NSView*)delegate, @"]", NSCommandKeyMask);
    MakeShellButton(&button_frame, @"Reload", content, IDC_NAV_RELOAD,
                    (NSView*)delegate, @"r", NSCommandKeyMask);
    MakeShellButton(&button_frame, @"Stop", content, IDC_NAV_STOP,
                    (NSView*)delegate, @".", NSCommandKeyMask);

    button_frame.size.width =
        NSWidth(initial_window_bounds) - NSMinX(button_frame);
    base::scoped_nsobject<NSTextField> url_edit_view(
        [[NSTextField alloc] initWithFrame:button_frame]);
    [content addSubview:url_edit_view];
    [url_edit_view setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];
    [url_edit_view setTarget:delegate];
    [url_edit_view setAction:@selector(takeURLStringValueFrom:)];
    [[url_edit_view cell] setWraps:NO];
    [[url_edit_view cell] setScrollable:YES];
    shell_data.url_edit_view = url_edit_view.get();
  }

  // Show the new window.
  [window makeKeyAndOrderFront:nil];

  shell_data.window = window;
}

gfx::NativeWindow ShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  return shell_data.window;
}

void ShellPlatformDelegate::CleanUp(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  shell_data_map_.erase(shell);
}

void ShellPlatformDelegate::SetContents(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  NSView* web_view = shell->web_contents()->GetNativeView().GetNativeNSView();
  [web_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

  NSView* content = [shell_data.window.GetNativeNSWindow() contentView];
  [content addSubview:web_view];

  NSRect frame = [content bounds];
  if (!Shell::ShouldHideToolbar())
    frame.size.height -= kURLBarHeight;
  [web_view setFrame:frame];
  [web_view setNeedsDisplay:YES];
}

void ShellPlatformDelegate::ResizeWebContent(Shell* shell,
                                             const gfx::Size& content_size) {
  DCHECK(!shell->headless());

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  int toolbar_height = Shell::ShouldHideToolbar() ? 0 : kURLBarHeight;
  NSRect frame = NSMakeRect(0, 0, content_size.width(),
                            content_size.height() + toolbar_height);
  [shell_data.window.GetNativeNSWindow().contentView setFrame:frame];
}

void ShellPlatformDelegate::EnableUIControl(Shell* shell,
                                            UIControl control,
                                            bool is_enabled) {
  DCHECK(!shell->headless());

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  int id;
  switch (control) {
    case BACK_BUTTON:
      id = IDC_NAV_BACK;
      break;
    case FORWARD_BUTTON:
      id = IDC_NAV_FORWARD;
      break;
    case STOP_BUTTON:
      id = IDC_NAV_STOP;
      break;
    default:
      NOTREACHED() << "Unknown UI control";
      return;
  }
  [[[shell_data.window.GetNativeNSWindow() contentView] viewWithTag:id]
      setEnabled:is_enabled];
}

void ShellPlatformDelegate::SetAddressBarURL(Shell* shell, const GURL& url) {
  DCHECK(!shell->headless());

  if (Shell::ShouldHideToolbar())
    return;

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  NSString* url_string = base::SysUTF8ToNSString(url.spec());
  [shell_data.url_edit_view setStringValue:url_string];
}

void ShellPlatformDelegate::SetIsLoading(Shell* shell, bool loading) {}

void ShellPlatformDelegate::SetTitle(Shell* shell,
                                     const base::string16& title) {
  DCHECK(!shell->headless());

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  NSString* title_string = base::SysUTF16ToNSString(title);
  [shell_data.window.GetNativeNSWindow() setTitle:title_string];
}

void ShellPlatformDelegate::RenderViewReady(Shell* shell) {
  DCHECK(!shell->headless());
}

bool ShellPlatformDelegate::DestroyShell(Shell* shell) {
  DCHECK(!shell->headless());

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  [shell_data.window.GetNativeNSWindow() performClose:nil];
  return true;  // The performClose() will do the destruction of Shell.
}

void ShellPlatformDelegate::ActivateContents(Shell* shell,
                                             WebContents* top_contents) {
  DCHECK(!shell->headless());

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  // This focuses the main frame RenderWidgetHost in the window, but does not
  // make the window itself active. The WebContentsDelegate (this class) is
  // responsible for doing both.
  top_contents->Focus();
  // This makes the window the active window for the application, and when the
  // app is active, the window will be also. That makes all RenderWidgetHosts
  // for the window active (which is separate from focused on mac).
  [shell_data.window.GetNativeNSWindow() makeKeyAndOrderFront:nil];
  // This makes the application active so that we can actually move focus
  // between windows and the renderer can receive focus/blur events.
  [NSApp activateIgnoringOtherApps:YES];
}

bool ShellPlatformDelegate::HandleKeyboardEvent(
    Shell* shell,
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  DCHECK(!shell->headless());

  if (event.skip_in_browser || Shell::ShouldHideToolbar())
    return false;

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  // The event handling to get this strictly right is a tangle; cheat here a bit
  // by just letting the menus have a chance at it.
  if ([event.os_event type] == NSKeyDown) {
    if (([event.os_event modifierFlags] & NSCommandKeyMask) &&
        [[event.os_event characters] isEqual:@"l"]) {
      [shell_data.window.GetNativeNSWindow()
          makeFirstResponder:shell_data.url_edit_view];
      return true;
    }

    [[NSApp mainMenu] performKeyEquivalent:event.os_event];
    return true;
  }
  return false;
}

}  // namespace content
