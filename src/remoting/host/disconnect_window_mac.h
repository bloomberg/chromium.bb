// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

// Controller for the disconnect window which allows the host user to
// quickly disconnect a session.
@interface DisconnectWindowController : NSWindowController {
 @private
  base::Closure _disconnect_callback;
  base::string16 _username;
}

- (id)initWithCallback:(const base::Closure&)disconnect_callback
              username:(const std::string&)username
                window:(NSWindow*)window;
- (void)initializeWindow;
- (void)stopSharing:(id)sender;
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
