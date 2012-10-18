// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

@protocol Me2MePreferencePaneConfirmPinHandler <NSObject>
- (void)applyConfiguration:(id)sender pin:(NSString*)pin;
@end

@interface Me2MePreferencePaneConfirmPin : NSViewController {
  IBOutlet NSTextField* email_;
  IBOutlet NSTextField* pin_;
  IBOutlet NSButton* apply_button_;
  id delegate_;
}

@property (retain) id delegate;

- (void)setEmail:(NSString*)email;
- (void)setButtonText:(NSString*)text;
- (void)setEnabled:(BOOL)enabled;
- (void)resetPin;
- (IBAction)onApply:(id)sender;

@end
