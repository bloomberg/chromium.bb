// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/callback.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "remoting/host/disconnect_window.h"

namespace remoting {
struct UiStrings;
}

// Controller for the disconnect window which allows the host user to
// quickly disconnect a session.
@interface DisconnectWindowController : NSWindowController {
 @private
  const remoting::UiStrings* ui_strings_;
  base::Closure disconnect_callback_;
  string16 username_;
  IBOutlet NSTextField* connectedToField_;
  IBOutlet NSButton* disconnectButton_;
}

- (id)initWithUiStrings:(const remoting::UiStrings*)ui_strings
               callback:(const base::Closure&)disconnect_callback
               username:(const std::string&)username;
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
