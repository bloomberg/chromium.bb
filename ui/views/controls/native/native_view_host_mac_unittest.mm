// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/native/native_view_host_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

class NativeViewHostMacTest : public test::NativeViewHostTestBase {
 public:
  NativeViewHostMacTest() {}

  NativeViewHostMac* native_host() {
    return static_cast<NativeViewHostMac*>(GetNativeWrapper());
  }

  void CreateHost() {
    CreateTopLevel();
    CreateTestingHost();
    native_view_.reset([[NSView alloc] initWithFrame:NSZeroRect]);

    // Verify the expectation that the NativeViewHostWrapper is only created
    // after the NativeViewHost is added to a widget.
    EXPECT_FALSE(native_host());
    toplevel()->GetRootView()->AddChildView(host());
    EXPECT_TRUE(native_host());

    host()->Attach(native_view_);
  }

 protected:
  base::scoped_nsobject<NSView> native_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeViewHostMacTest);
};

// Test destroying the top level widget before destroying the NativeViewHost.
// On Mac, also ensure that the native view is removed from its superview when
// the Widget containing its host is destroyed.
TEST_F(NativeViewHostMacTest, DestroyWidget) {
  ResetHostDestroyedCount();
  CreateHost();
  ReleaseHost();
  EXPECT_EQ(0, host_destroyed_count());
  EXPECT_TRUE([native_view_ superview]);
  DestroyTopLevel();
  EXPECT_FALSE([native_view_ superview]);
  EXPECT_EQ(1, host_destroyed_count());
}

// Ensure the native view receives the correct bounds when it is attached. On
// Mac, the bounds of the native view is relative to the NSWindow it is in, not
// the screen, and the coordinates have to be flipped.
TEST_F(NativeViewHostMacTest, Attach) {
  CreateHost();
  EXPECT_TRUE([native_view_ superview]);
  EXPECT_TRUE([native_view_ window]);

  host()->Detach();

  [native_view_ setFrame:NSZeroRect];
  toplevel()->SetBounds(gfx::Rect(64, 48, 100, 200));
  host()->SetBounds(10, 10, 80, 60);

  EXPECT_FALSE([native_view_ superview]);
  EXPECT_FALSE([native_view_ window]);
  EXPECT_TRUE(NSEqualRects(NSZeroRect, [native_view_ frame]));

  host()->Attach(native_view_);
  EXPECT_TRUE([native_view_ superview]);
  EXPECT_TRUE([native_view_ window]);

  // Expect the top-left to be 10 pixels below the titlebar.
  int bottom = toplevel()->GetClientAreaBoundsInScreen().height() - 10 - 60;
  EXPECT_TRUE(NSEqualRects(NSMakeRect(10, bottom, 80, 60),
                           [native_view_ frame]));
}

// Ensure the native view is hidden along with its host, and when detaching, or
// when attaching to a host that is already hidden.
TEST_F(NativeViewHostMacTest, NativeViewHidden) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(0, 0, 100, 100));
  host()->SetBounds(10, 10, 80, 60);

  EXPECT_FALSE([native_view_ isHidden]);
  host()->SetVisible(false);
  EXPECT_TRUE([native_view_ isHidden]);
  host()->SetVisible(true);
  EXPECT_FALSE([native_view_ isHidden]);

  host()->Detach();
  EXPECT_TRUE([native_view_ isHidden]);  // Hidden when detached.
  [native_view_ setHidden:NO];

  host()->SetVisible(false);
  EXPECT_FALSE([native_view_ isHidden]);  // Stays visible.
  host()->Attach(native_view_);
  EXPECT_TRUE([native_view_ isHidden]);  // Hidden when attached.

  host()->Detach();
  [native_view_ setHidden:YES];
  host()->SetVisible(true);
  EXPECT_TRUE([native_view_ isHidden]);  // Stays hidden.
  host()->Attach(native_view_);
  EXPECT_FALSE([native_view_ isHidden]);  // Made visible when attached.

  EXPECT_TRUE([native_view_ superview]);
  toplevel()->GetRootView()->RemoveChildView(host());
  EXPECT_TRUE([native_view_ isHidden]);  // Hidden when removed from Widget.
  EXPECT_FALSE([native_view_ superview]);

  toplevel()->GetRootView()->AddChildView(host());
  EXPECT_FALSE([native_view_ isHidden]);  // And visible when added.
  EXPECT_TRUE([native_view_ superview]);
}

}  // namespace views
