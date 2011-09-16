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
                                                window:this
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
@property (nonatomic, assign) remoting::DisconnectWindowMac* disconnectWindow;
@property (nonatomic, copy) NSString* username;
@end

@implementation DisconnectWindowController

@synthesize host = host_;
@synthesize disconnectWindow = disconnectWindow_;
@synthesize username = username_;

- (void)close {
  self.host = NULL;
  [super close];
}

- (id)initWithHost:(remoting::ChromotingHost*)host
            window:(remoting::DisconnectWindowMac*)disconnectWindow
          username:(NSString*)username {
  self = [super initWithWindowNibName:@"disconnect_window"];
  if (self) {
    host_ = host;
    disconnectWindow_ = disconnectWindow;
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
    self.host->Shutdown(NULL);
    self.host = NULL;
  }
  self.disconnectWindow = NULL;
}

- (void)windowDidLoad {
  string16 text = ReplaceStringPlaceholders(
      host_->ui_strings().disconnect_message,
      base::SysNSStringToUTF16(self.username),
      NULL);
  [connectedToField_ setStringValue:base::SysUTF16ToNSString(text)];

  [disconnectButton_ setTitle:base::SysUTF16ToNSString(
      host_->ui_strings().disconnect_button_text_plus_shortcut)];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self stopSharing:self];
  [self autorelease];
}

@end
