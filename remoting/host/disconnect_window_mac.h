// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

namespace remoting {
class ChromotingHost;
class DisconnectWindowMac;
}

@interface DisconnectWindowController : NSWindowController {
 @private
  remoting::ChromotingHost* host_;
  remoting::DisconnectWindowMac* disconnectWindow_;
  NSString* username_;
  IBOutlet NSTextField* connectedToField_;
  IBOutlet NSButton* disconnectButton_;
}

- (IBAction)stopSharing:(id)sender;
- (id)initWithHost:(remoting::ChromotingHost*)host
            window:(remoting::DisconnectWindowMac*)window
          username:(NSString*)username;
@end
