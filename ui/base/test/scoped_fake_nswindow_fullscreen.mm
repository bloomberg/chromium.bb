// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/scoped_fake_nswindow_fullscreen.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#import "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"

// This method exists on NSWindowDelegate on 10.7+.
// To build on 10.6, we just need to declare it somewhere. We'll test
// -[NSObject respondsToSelector] before calling it.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7
@protocol NSWindowDelegateLion
- (NSSize)window:(NSWindow*)window
    willUseFullScreenContentSize:(NSSize)proposedSize;
@end
#endif

// Donates a testing implementation of [NSWindow toggleFullScreen:].
@interface ToggleFullscreenDonorForWindow : NSObject
@end

namespace {

ui::test::ScopedFakeNSWindowFullscreen::Impl* g_fake_fullscreen_impl = nullptr;

}  // namespace

namespace ui {
namespace test {

class ScopedFakeNSWindowFullscreen::Impl {
 public:
  Impl()
      : toggle_fullscreen_swizzler_([NSWindow class],
                                    [ToggleFullscreenDonorForWindow class],
                                    @selector(toggleFullScreen:)) {}

  ~Impl() {
    // If there's a pending transition, it means there's a task in the queue to
    // complete it, referencing |this|.
    DCHECK(!is_in_transition_);
  }

  void ToggleFullscreenForWindow(NSWindow* window) {
    DCHECK(!is_in_transition_);
    if (window_ == nil) {
      StartEnterFullscreen(window);
    } else if (window_ == window) {
      StartExitFullscreen();
    } else {
      // Another window is fullscreen.
      NOTREACHED();
    }
  }

  void StartEnterFullscreen(NSWindow* window) {
    // If the window cannot go fullscreen, do nothing.
    if (!([window collectionBehavior] &
          NSWindowCollectionBehaviorFullScreenPrimary)) {
      return;
    }

    // This cannot be id<NSWindowDelegate> because on 10.6 it won't have
    // window:willUseFullScreenContentSize:.
    id delegate = [window delegate];

    // Nothing is currently fullscreen. Make this window fullscreen.
    window_ = window;
    is_in_transition_ = true;
    frame_before_fullscreen_ = [window frame];
    NSSize fullscreen_content_size =
        [window contentRectForFrameRect:[[window screen] frame]].size;
    if ([delegate respondsToSelector:@selector(window:
                                         willUseFullScreenContentSize:)]) {
      fullscreen_content_size = [delegate window:window
                    willUseFullScreenContentSize:fullscreen_content_size];
    }
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowWillEnterFullScreenNotification
                      object:window];
    base::MessageLoopForUI::current()->PostTask(
        FROM_HERE, base::Bind(&Impl::FinishEnterFullscreen,
                              base::Unretained(this), fullscreen_content_size));
  }

  void FinishEnterFullscreen(NSSize fullscreen_content_size) {
    // The frame should not have changed during the transition.
    DCHECK(NSEqualRects(frame_before_fullscreen_, [window_ frame]));
    // Style mask must be set first because -[NSWindow frame] may be different
    // depending on NSFullScreenWindowMask.
    [window_ setStyleMask:[window_ styleMask] | NSFullScreenWindowMask];
    // The origin doesn't matter, NSFullScreenWindowMask means the origin will
    // be adjusted.
    NSRect target_fullscreen_frame = [window_
        frameRectForContentRect:NSMakeRect(0, 0, fullscreen_content_size.width,
                                           fullscreen_content_size.height)];
    [window_ setFrame:target_fullscreen_frame display:YES animate:NO];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowDidEnterFullScreenNotification
                      object:window_];
    // Store the actual frame because we check against it when exiting.
    frame_during_fullscreen_ = [window_ frame];
    is_in_transition_ = false;
  }

  void StartExitFullscreen() {
    is_in_transition_ = true;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowWillExitFullScreenNotification
                      object:window_];

    base::MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        base::Bind(&Impl::FinishExitFullscreen, base::Unretained(this)));
  }

  void FinishExitFullscreen() {
    // The bounds may have changed during the transition. Check for this before
    // setting the style mask because -[NSWindow frame] may be different
    // depending on NSFullScreenWindowMask.
    bool no_frame_change_during_fullscreen =
        NSEqualRects(frame_during_fullscreen_, [window_ frame]);
    [window_ setStyleMask:[window_ styleMask] & ~NSFullScreenWindowMask];
    // Set the original frame after setting the style mask.
    if (no_frame_change_during_fullscreen)
      [window_ setFrame:frame_before_fullscreen_ display:YES animate:NO];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSWindowDidExitFullScreenNotification
                      object:window_];
    window_ = nil;
    is_in_transition_ = false;
  }

 private:
  base::mac::ScopedObjCClassSwizzler toggle_fullscreen_swizzler_;

  // The currently fullscreen window.
  NSWindow* window_ = nil;
  NSRect frame_before_fullscreen_;
  NSRect frame_during_fullscreen_;
  bool is_in_transition_ = false;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

ScopedFakeNSWindowFullscreen::ScopedFakeNSWindowFullscreen() {
  // -[NSWindow toggleFullScreen:] does not exist on 10.6, so do nothing.
  if (base::mac::IsOSSnowLeopard())
    return;

  DCHECK(!g_fake_fullscreen_impl);
  impl_.reset(new Impl);
  g_fake_fullscreen_impl = impl_.get();
}

ScopedFakeNSWindowFullscreen::~ScopedFakeNSWindowFullscreen() {
  g_fake_fullscreen_impl = nullptr;
}

}  // namespace test
}  // namespace ui

@implementation ToggleFullscreenDonorForWindow

- (void)toggleFullScreen:(id)sender {
  NSWindow* window = base::mac::ObjCCastStrict<NSWindow>(self);
  g_fake_fullscreen_impl->ToggleFullscreenForWindow(window);
}

@end
