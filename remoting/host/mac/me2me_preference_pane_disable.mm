// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/mac/me2me_preference_pane_disable.h"

@implementation Me2MePreferencePaneDisable

@synthesize delegate = delegate_;

- (id)init {
  self = [super initWithNibName:@"me2me_preference_pane_disable"
                         bundle:[NSBundle bundleForClass:[self class]]];
  return self;
}

- (void)dealloc {
  [delegate_ release];
  [super dealloc];
}

- (void)setEnabled:(BOOL)enabled {
  [disable_button_ setEnabled:enabled];
}

- (void)onDisable:(id)sender {
  [delegate_ onDisable:self];
}

@end
