// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/underlay_opengl_hosting_window.h"

#import <objc/runtime.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/scoped_nsobject.h"

@interface NSWindow (UndocumentedAPI)
// Normally, punching a hole in a window by painting a subview with a
// transparent color causes the shadow for that area to also not be present.
// That feature is "content has shadow", which means that shadows are effective
// even in the content area of the window. If, however, "content has shadow" is
// turned off, then the transparent area of the content casts a shadow. The one
// tricky part is that even if "content has shadow" is turned off, "the content"
// is defined as being the scanline from the leftmost opaque part to the
// rightmost opaque part.  Therefore, to force the entire window to have a
// shadow, make sure that for the entire content region, there is an opaque area
// on the right and left edge of the window.
- (void)_setContentHasShadow:(BOOL)shadow;
@end

@interface OpaqueView : NSView
@end

@implementation OpaqueView
- (void)drawRect:(NSRect)r {
  [[NSColor blackColor] set];
  NSRectFill(r);
}
@end

namespace {

NSComparisonResult OpaqueViewsOnTop(id view1, id view2, void* context) {
  BOOL view_1_is_opaque_view = [view1 isKindOfClass:[OpaqueView class]];
  BOOL view_2_is_opaque_view = [view2 isKindOfClass:[OpaqueView class]];
  if (view_1_is_opaque_view && view_2_is_opaque_view)
    return NSOrderedSame;
  if (view_1_is_opaque_view)
    return NSOrderedDescending;
  if (view_2_is_opaque_view)
    return NSOrderedAscending;
  return NSOrderedSame;
}

void RootDidAddSubview(id self, SEL _cmd, NSView* subview) {
  if (![[self window] isKindOfClass:[UnderlayOpenGLHostingWindow class]])
    return;

  // Make sure the opaques are on top.
  [self sortSubviewsUsingFunction:OpaqueViewsOnTop context:NULL];
}

}  // namespace

@implementation UnderlayOpenGLHostingWindow

+ (void)load {
  base::mac::ScopedNSAutoreleasePool pool;

  // On 10.8+ the background for textured windows are no longer drawn by
  // NSGrayFrame, and NSThemeFrame is used instead <http://crbug.com/114745>.
  Class borderViewClass = NSClassFromString(
      base::mac::IsOSMountainLionOrLater() ? @"NSThemeFrame" : @"NSGrayFrame");
  DCHECK(borderViewClass);
  if (!borderViewClass) return;

  // Install callback for added views.
  Method m = class_getInstanceMethod([NSView class], @selector(didAddSubview:));
  DCHECK(m);
  if (m) {
    BOOL didAdd = class_addMethod(borderViewClass,
                                  @selector(didAddSubview:),
                                  reinterpret_cast<IMP>(&RootDidAddSubview),
                                  method_getTypeEncoding(m));
    DCHECK(didAdd);
  }
}

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:windowStyle
                                 backing:bufferingType
                                   defer:deferCreation])) {
    // The invisible opaque area technique only works > 10.5. Fortunately, hole
    // punching is used only when IOSurfaces are used to transport, and that's
    // also only on > 10.5. Also, don't mess around with things if it's not a
    // proper window with a title bar and all.
    if (base::mac::IsOSSnowLeopardOrLater() &&
        windowStyle && NSTitledWindowMask) {
      [self setOpaque:NO];
      [self _setContentHasShadow:NO];

      NSView* rootView = [[self contentView] superview];
      const NSRect rootBounds = [rootView bounds];

      const CGFloat kEdgeInset = 16;
      const CGFloat kAlphaValueJustOpaqueEnough = 0.002;

      scoped_nsobject<NSView> leftOpaque([[OpaqueView alloc] initWithFrame:
          NSMakeRect(NSMinX(rootBounds), NSMinY(rootBounds) + kEdgeInset,
                     1, NSHeight(rootBounds) - 2 * kEdgeInset)]);
      [leftOpaque setAutoresizingMask:NSViewMaxXMargin | NSViewHeightSizable];
      [leftOpaque setAlphaValue:kAlphaValueJustOpaqueEnough];
      [rootView addSubview:leftOpaque];

      scoped_nsobject<NSView> rightOpaque([[OpaqueView alloc] initWithFrame:
          NSMakeRect(NSMaxX(rootBounds) - 1, NSMinY(rootBounds) + kEdgeInset,
                     1, NSHeight(rootBounds) - 2 * kEdgeInset)]);
      [rightOpaque setAutoresizingMask:NSViewMinXMargin | NSViewHeightSizable];
      [rightOpaque setAlphaValue:kAlphaValueJustOpaqueEnough];
      [rootView addSubview:rightOpaque];
    }
  }

  return self;
}

@end
