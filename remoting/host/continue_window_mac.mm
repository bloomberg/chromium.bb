// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/scoped_nsobject.h"
#include "base/sys_string_conversions.h"
#include "remoting/host/chromoting_host.h"

// As this is a plugin, there needs to be a way to find its bundle
// so that resources are able to be found. This class exists solely so that
// there is a way to get the bundle that this code file is in using
// [NSBundle bundleForClass:[ContinueWindowMacClassToLocateMyBundle class]]
// It is really only a name.
@interface ContinueWindowMacClassToLocateMyBundle : NSObject
@end

@implementation ContinueWindowMacClassToLocateMyBundle
@end

namespace remoting {

class ContinueWindowMac : public remoting::ContinueWindow {
 public:
  ContinueWindowMac() : modal_session_(NULL) {}
  virtual ~ContinueWindowMac() {}

  virtual void Show(remoting::ChromotingHost* host) OVERRIDE;
  virtual void Hide() OVERRIDE;

 private:
  NSModalSession modal_session_;

  DISALLOW_COPY_AND_ASSIGN(ContinueWindowMac);
};

void ContinueWindowMac::Show(remoting::ChromotingHost* host) {
  base::mac::ScopedNSAutoreleasePool pool;

  // Generate window shade
  NSArray* screens = [NSScreen screens];
  NSMutableArray* windows = [NSMutableArray arrayWithCapacity:[screens count]];
  for (NSScreen *screen in screens) {
    NSWindow* window =
      [[[NSWindow alloc] initWithContentRect:[screen frame]
                                   styleMask:NSBorderlessWindowMask
                                   backing:NSBackingStoreBuffered
                                   defer:NO
                                   screen:screen] autorelease];
    [window setReleasedWhenClosed:NO];
    [window setAlphaValue:0.8];
    [window setOpaque:NO];
    [window setBackgroundColor:[NSColor blackColor]];
    // Raise the window shade above just about everything else.
    // Leave the dock and menu bar exposed so the user has some basic level
    // of control (like they can quit Chromium).
    [window setLevel:NSModalPanelWindowLevel - 1];
    [window orderFront:nil];
    [windows addObject:window];
  }

  // Put up alert
  const UiStrings& strings = host->ui_strings();
  NSString* message = base::SysUTF16ToNSString(strings.continue_prompt);
  NSString* continue_button = base::SysUTF16ToNSString(
      strings.continue_button_text);
  NSString* cancel_button = base::SysUTF16ToNSString(
      strings.stop_sharing_button_text);
  scoped_nsobject<NSAlert> continue_alert([[NSAlert alloc] init]);
  [continue_alert setMessageText:message];
  [continue_alert addButtonWithTitle:continue_button];
  [continue_alert addButtonWithTitle:cancel_button];

  // See ContinueWindowMacClassToLocateMyBundle class above for details
  // on this.
  NSBundle *bundle =
      [NSBundle bundleForClass:[ContinueWindowMacClassToLocateMyBundle class]];
  NSString *imagePath = [bundle pathForResource:@"chromoting128" ofType:@"png"];
  scoped_nsobject<NSImage> image(
      [[NSImage alloc] initByReferencingFile:imagePath]);
  [continue_alert setIcon:image];
  [continue_alert layout];

  NSWindow* continue_window = [continue_alert window];
  [continue_window center];
  [continue_window orderWindow:NSWindowAbove
                    relativeTo:[[windows lastObject] windowNumber]];
  [continue_window makeKeyWindow];
  NSApplication* application = [NSApplication sharedApplication];
  modal_session_ = [application beginModalSessionForWindow:continue_window];
  NSInteger answer = 0;
  do {
    answer = [application runModalSession:modal_session_];
  } while (answer == NSRunContinuesResponse);
  [application endModalSession:modal_session_];
  modal_session_ = NULL;

  [continue_window close];

  // Remove window shade.
  for (NSWindow* window in windows) {
    [window close];
  }

  if (answer == NSAlertFirstButtonReturn) {
    host->PauseSession(false);
  } else {
    host->Shutdown(NULL);
  }
}

void ContinueWindowMac::Hide() {
  if (modal_session_) {
    NSApplication* application = [NSApplication sharedApplication];
    [application stopModalWithCode:NSAlertFirstButtonReturn];
  }
}

ContinueWindow* ContinueWindow::Create() {
  return new ContinueWindowMac();
}

}  // namespace remoting
