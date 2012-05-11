// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#include "remoting/host/disconnect_window.h"

namespace remoting {
class ChromotingHost;
}

// Controller for the disconnect window which allows the host user to
// quickly disconnect a session.
@interface DisconnectWindowController : NSWindowController {
 @private
  remoting::ChromotingHost* host_;
  base::Closure disconnect_callback_;
  NSString* username_;
  IBOutlet NSTextField* connectedToField_;
  IBOutlet NSButton* disconnectButton_;
}

- (id)initWithHost:(remoting::ChromotingHost*)host
          callback:(const base::Closure&)disconnect_callback
          username:(NSString*)username;
- (IBAction)stopSharing:(id)sender;
@end

// A floating window with a custom border. The custom border and background
// content is defined by DisconnectView. Declared here so that it can be
// instantiated via a xib.
@interface DisconnectWindow : NSWindow
@end

// The custom background/border for the DisconnectWindow. Declared here so that
// it can be instantiated via a xib.
@interface DisconnectView : NSView
@end
