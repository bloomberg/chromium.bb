// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#import <AppKit/NSApplication.h>
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <Foundation/NSAutoreleasePool.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"

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
  compositor_->ScheduleFullRedraw();
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
  ~AppKitHost() override {}
 private:
  DISALLOW_COPY_AND_ASSIGN(AppKitHost);
};

// TestCompositorHostMac provides a window surface and a coordinated compositor
// for use in the compositor unit tests.
class TestCompositorHostMac : public TestCompositorHost,
                              public AppKitHost {
 public:
  TestCompositorHostMac(const gfx::Rect& bounds,
                        ui::ContextFactory* context_factory,
                        ui::ContextFactoryPrivate* context_factory_private);
  ~TestCompositorHostMac() override;

 private:
  // TestCompositorHost:
  void Show() override;
  ui::Compositor* GetCompositor() override;

  gfx::Rect bounds_;

  ui::Compositor compositor_;

  // Owned.  Released when window is closed.
  NSWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorHostMac);
};

TestCompositorHostMac::TestCompositorHostMac(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private)
    : bounds_(bounds),
      compositor_(context_factory_private->AllocateFrameSinkId(),
                  context_factory,
                  context_factory_private,
                  base::ThreadTaskRunnerHandle::Get()),
      window_(nil) {}

TestCompositorHostMac::~TestCompositorHostMac() {
  // Release reference to |compositor_|.  Important because the |compositor_|
  // holds |this| as its delegate, so that reference must be removed here.
  [[window_ contentView] setCompositor:NULL];
  {
    base::scoped_nsobject<NSView> new_view(
        [[NSView alloc] initWithFrame:NSZeroRect]);
    [window_ setContentView:new_view.get()];
  }

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
  compositor_.SetAcceleratedWidget(view);
  compositor_.SetScaleAndSize(1.0f, bounds_.size());
  [view setCompositor:&compositor_];
  [window_ setContentView:view];
  [window_ orderFront:nil];
}

ui::Compositor* TestCompositorHostMac::GetCompositor() {
  return &compositor_;
}

// static
TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private) {
  return new TestCompositorHostMac(bounds, context_factory,
                                   context_factory_private);
}

}  // namespace ui
