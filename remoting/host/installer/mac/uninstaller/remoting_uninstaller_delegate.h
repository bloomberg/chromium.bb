// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

@interface CRDAppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet NSWindow *window;

- (IBAction)uninstall:(NSButton *)sender;
- (IBAction)cancel:(id)sender;

- (IBAction)handleMenuClose:(NSMenuItem *)sender;

@end
