// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "remoting/host/disconnect_window_mac.h"

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/disconnect_window.h"

namespace remoting {

class DisconnectWindowMac : public remoting::DisconnectWindow {
 public:
  DisconnectWindowMac();
  virtual ~DisconnectWindowMac();

  virtual void Show(remoting::ChromotingHost* host,
                    const std::string& username) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  DisconnectWindowController* window_controller_;

  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowMac);
};

DisconnectWindowMac::DisconnectWindowMac()
    : window_controller_(nil) {
}

DisconnectWindowMac::~DisconnectWindowMac() {
  [window_controller_ close];
}

void DisconnectWindowMac::Show(remoting::ChromotingHost* host,
                               const std::string& username) {
  CHECK(window_controller_ == nil);
  NSString* nsUsername = base::SysUTF8ToNSString(username);
  window_controller_ =
      [[DisconnectWindowController alloc] initWithHost:host
                                              username:nsUsername];
  [window_controller_ showWindow:nil];
}

void DisconnectWindowMac::Hide() {
  // DisconnectWindowController is responsible for releasing itself in its
  // windowWillClose: method.
  [window_controller_ close];
  window_controller_ = nil;
}

remoting::DisconnectWindow* remoting::DisconnectWindow::Create() {
  return new DisconnectWindowMac;
}

}  // namespace remoting

@interface DisconnectWindowController()
@property (nonatomic, assign) remoting::ChromotingHost* host;
@property (nonatomic, copy) NSString* username;

- (BOOL)isRToL;

@end

@implementation DisconnectWindowController

@synthesize host = host_;
@synthesize username = username_;

- (id)initWithHost:(remoting::ChromotingHost*)host
          username:(NSString*)username {
  self = [super initWithWindowNibName:@"disconnect_window"];
  if (self) {
    host_ = host;
    username_ = [username copy];
  }
  return self;
}

- (void)dealloc {
  [username_ release];
  [super dealloc];
}

- (IBAction)stopSharing:(id)sender {
  if (self.host) {
    self.host->Shutdown(base::Closure());
    self.host = NULL;
  }
}

- (BOOL)isRToL {
  return host_->ui_strings().direction == remoting::UiStrings::RTL;
}

- (void)close {
  self.host = NULL;
  [super close];
}

- (void)windowDidLoad {
  string16 text = ReplaceStringPlaceholders(
      host_->ui_strings().disconnect_message,
      base::SysNSStringToUTF16(self.username),
      NULL);
  [connectedToField_ setStringValue:base::SysUTF16ToNSString(text)];

  [disconnectButton_ setTitle:base::SysUTF16ToNSString(
      host_->ui_strings().disconnect_button_text_plus_shortcut)];

  // Resize the window dynamically based on the content.
  CGFloat oldConnectedWidth = NSWidth([connectedToField_ bounds]);
  [connectedToField_ sizeToFit];
  NSRect connectedToFrame = [connectedToField_ frame];
  CGFloat newConnectedWidth = NSWidth(connectedToFrame);

  // Set a max width for the connected to text field.
  if (newConnectedWidth >
      remoting::DisconnectWindow::kMaximumConnectedNameWidthInPixels) {
    newConnectedWidth
        = remoting::DisconnectWindow::kMaximumConnectedNameWidthInPixels;
    connectedToFrame.size.width = newConnectedWidth;
    [connectedToField_ setFrame:connectedToFrame];
  }

  CGFloat oldDisconnectWidth = NSWidth([disconnectButton_ bounds]);
  [disconnectButton_ sizeToFit];
  NSRect disconnectFrame = [disconnectButton_ frame];
  CGFloat newDisconnectWidth = NSWidth(disconnectFrame);

  // Move the disconnect button appropriately.
  disconnectFrame.origin.x += newConnectedWidth - oldConnectedWidth;
  [disconnectButton_ setFrame:disconnectFrame];

  // Then resize the window appropriately
  NSWindow *window = [self window];
  NSRect windowFrame = [window frame];
  windowFrame.size.width += (newConnectedWidth - oldConnectedWidth +
                             newDisconnectWidth - oldDisconnectWidth);
  [window setFrame:windowFrame display:NO];

  if ([self isRToL]) {
    // Handle right to left case
    CGFloat buttonInset = NSWidth(windowFrame) - NSMaxX(disconnectFrame);
    CGFloat buttonTextSpacing
        = NSMinX(disconnectFrame) - NSMaxX(connectedToFrame);
    disconnectFrame.origin.x = buttonInset;
    connectedToFrame.origin.x = NSMaxX(disconnectFrame) + buttonTextSpacing;
    [connectedToField_ setFrame:connectedToFrame];
    [disconnectButton_ setFrame:disconnectFrame];
  }

  // Center the window at the bottom of the screen, above the dock (if present).
  NSRect desktopRect = [[NSScreen mainScreen] visibleFrame];
  NSRect windowRect = [[self window] frame];
  CGFloat x = (NSWidth(desktopRect) - NSWidth(windowRect)) / 2;
  CGFloat y = NSMinY(desktopRect);
  [[self window] setFrameOrigin:NSMakePoint(x, y)];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self stopSharing:self];
  [self autorelease];
}

@end


@interface DisconnectWindow()
- (BOOL)isRToL;
@end

@implementation DisconnectWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(unsigned int)aStyle
                  backing:(NSBackingStoreType)bufferingType
                  defer:(BOOL)flag {
  // Pass NSBorderlessWindowMask for the styleMask to remove the title bar.
  self = [super initWithContentRect:contentRect
                          styleMask:NSBorderlessWindowMask
                            backing:bufferingType
                              defer:flag];

  if (self) {
    // Set window to be clear and non-opaque so we can see through it.
    [self setBackgroundColor:[NSColor clearColor]];
    [self setOpaque:NO];
    [self setMovableByWindowBackground:YES];

    // Pull the window up to Status Level so that it always displays.
    [self setLevel:NSStatusWindowLevel];
  }
  return self;
}

- (BOOL)isRToL {
  DCHECK([[self windowController] respondsToSelector:@selector(isRToL)]);
  return [[self windowController] isRToL];
}

@end


@interface DisconnectView()
- (BOOL)isRToL;
@end

@implementation DisconnectView

- (BOOL)isRToL {
  DCHECK([[self window] isKindOfClass:[DisconnectWindow class]]);
  return [static_cast<DisconnectWindow*>([self window]) isRToL];
}

- (void)drawRect:(NSRect)rect {
  // All magic numbers taken from screen shots provided by UX.
  NSRect bounds = NSInsetRect([self bounds], 1, 1);

  NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                       xRadius:5
                                                       yRadius:5];
  NSColor *gray = [NSColor colorWithCalibratedWhite:0.91 alpha:1.0];
  [gray setFill];
  [path fill];
  [path setLineWidth:4];
  NSColor *green = [NSColor colorWithCalibratedRed:0.13
                                             green:0.69
                                              blue:0.11
                                             alpha:1.0];
  [green setStroke];
  [path stroke];


  // Draw drag handle on proper side
  const CGFloat kHeight = 21.0;
  const CGFloat kBaseInset = 12.0;
  const CGFloat kDragHandleWidth = 5.0;

  NSColor *dark = [NSColor colorWithCalibratedWhite:0.70 alpha:1.0];
  NSColor *light = [NSColor colorWithCalibratedWhite:0.97 alpha:1.0];

  // Turn off aliasing so it's nice and crisp.
  NSGraphicsContext *context = [NSGraphicsContext currentContext];
  BOOL alias = [context shouldAntialias];
  [context setShouldAntialias:NO];

  // Handle bidirectional locales properly.
  CGFloat inset = [self isRToL] ? NSMaxX(bounds) - kBaseInset - kDragHandleWidth
                                : kBaseInset;

  NSPoint top = NSMakePoint(inset, NSMidY(bounds) - kHeight / 2.0);
  NSPoint bottom = NSMakePoint(inset, top.y + kHeight);

  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [dark setStroke];
  [path stroke];

  top.x += 1;
  bottom.x += 1;
  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [light setStroke];
  [path stroke];

  top.x += 2;
  bottom.x += 2;
  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [dark setStroke];
  [path stroke];

  top.x += 1;
  bottom.x += 1;
  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [light setStroke];
  [path stroke];

  [context setShouldAntialias:alias];
}

@end
