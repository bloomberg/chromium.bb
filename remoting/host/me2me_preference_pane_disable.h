// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

// This view-controller holds just a "Disable" button.  It is shown when the
// pref-pane is launched manually, or when the user has just confirmed their
// PIN and applied a new configuration.
@interface Me2MePreferencePaneDisable : NSViewController {
  IBOutlet NSButton* disable_button_;
  id delegate_;
}

@property (retain) id delegate;

- (void)setEnabled:(BOOL)enabled;
- (IBAction)onDisable:(id)sender;

@end
