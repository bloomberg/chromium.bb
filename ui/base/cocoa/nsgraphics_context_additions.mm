// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/nsgraphics_context_additions.h"

@implementation NSGraphicsContext (CrAdditions)

- (void)cr_setPatternPhase:(NSPoint)phase
                   forView:(NSView*)view {
  if ([view layer]) {
    NSPoint bottomLeft = NSZeroPoint;
    if ([view isFlipped])
      bottomLeft.y = NSMaxY([view bounds]);
    phase.y -= [view convertPoint:bottomLeft toView:nil].y;
  }
  [self setPatternPhase:phase];
}

@end
