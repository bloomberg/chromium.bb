// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#import <AppKit/NSApplication.h>
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <Foundation/NSAutoreleasePool.h>

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/rect.h"

// AcceleratedTestView provides an NSView class that delegates drawing to a
// ui::Compositor delegate, setting up the NSOpenGLContext as required.
@interface AcceleratedTestView : NSView {
  ui::Compositor* compositor_;
}
// Designated initializer.
-(id)init;
-(void)setCompositor:(ui::Compositor*)compositor;
@end

@implementation AcceleratedTestView
-(id)init {
  // The frame will be resized when reparented into the window's view hierarchy.
  self = [super initWithFrame:NSZeroRect];
  return self;
}

-(void)setCompositor:(ui::Compositor*)compositor {
  compositor_ = compositor;
}

- (void)drawRect:(NSRect)rect {
  DCHECK(compositor_) << "Drawing with no compositor set.";
  compositor_->Draw();
}
@end

namespace ui {

// Tests that use Objective-C memory semantics need to have a top-level
// NSAutoreleasePool set up and initialized prior to execution and drained upon
// exit.  The tests will leak otherwise.
class FoundationHost {
 protected:
  FoundationHost() {
    pool_ = [[NSAutoreleasePool alloc] init];
  }
  virtual ~FoundationHost() {
    [pool_ drain];
  }

 private:
  NSAutoreleasePool* pool_;
  DISALLOW_COPY_AND_ASSIGN(FoundationHost);
};

// Tests that use the AppKit framework need to have the NSApplication
// initialized prior to doing anything with display objects such as windows,
// views, or controls.
class AppKitHost : public FoundationHost {
 protected:
  AppKitHost() {
    [NSApplication sharedApplication];
  }
  virtual ~AppKitHost() {
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(AppKitHost);
};

// TestCompositorHostMac provides a window surface and a coordinated compositor
// for use in the compositor unit tests.
class TestCompositorHostMac : public TestCompositorHost,
                              public AppKitHost {
 public:
  TestCompositorHostMac(const gfx::Rect& bounds,
                        ui::ContextFactory* context_factory);
  virtual ~TestCompositorHostMac();

 private:
  // TestCompositorHost:
  virtual void Show() OVERRIDE;
  virtual ui::Compositor* GetCompositor() OVERRIDE;

  gfx::Rect bounds_;

  ui::ContextFactory* context_factory_;

  scoped_ptr<ui::Compositor> compositor_;

  // Owned.  Released when window is closed.
  NSWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostMac);
};

TestCompositorHostMac::TestCompositorHostMac(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory)
    : bounds_(bounds), context_factory_(context_factory), window_(nil) {
}

TestCompositorHostMac::~TestCompositorHostMac() {
  // Release reference to |compositor_|.  Important because the |compositor_|
  // holds |this| as its delegate, so that reference must be removed here.
  [[window_ contentView] setCompositor:NULL];
  [window_ setContentView:nil];

  [window_ orderOut:nil];
  [window_ close];
}

void TestCompositorHostMac::Show() {
  DCHECK(!window_);
  window_ = [[NSWindow alloc]
                initWithContentRect:NSMakeRect(bounds_.x(),
                                               bounds_.y(),
                                               bounds_.width(),
                                               bounds_.height())
                          styleMask:NSBorderlessWindowMask
                            backing:NSBackingStoreBuffered
                              defer:NO];
  base::scoped_nsobject<AcceleratedTestView> view(
      [[AcceleratedTestView alloc] init]);
  compositor_.reset(new ui::Compositor(view, context_factory_));
  compositor_->SetScaleAndSize(1.0f, bounds_.size());
  [view setCompositor:compositor_.get()];
  [window_ setContentView:view];
  [window_ orderFront:nil];
}

ui::Compositor* TestCompositorHostMac::GetCompositor() {
  return compositor_.get();
}

// static
TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory) {
  return new TestCompositorHostMac(bounds, context_factory);
}

}  // namespace ui
