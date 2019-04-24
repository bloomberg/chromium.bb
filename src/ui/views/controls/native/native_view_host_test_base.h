// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "ui/views/test/views_test_base.h"

namespace views {

class NativeViewHost;
class NativeViewHostWrapper;
class Widget;

namespace test {

// Base class for NativeViewHost tests on different platforms.
class NativeViewHostTestBase : public ViewsTestBase {
 public:
  NativeViewHostTestBase();
  ~NativeViewHostTestBase() override;

  // testing::Test:
  void TearDown() override;

  // Create the |toplevel_| widget.
  void CreateTopLevel();

  // Create a testing |host_| that tracks destructor calls.
  void CreateTestingHost();

  // The number of times a host created by CreateHost() has been destroyed.
  int host_destroyed_count() { return host_destroyed_count_; }
  void ResetHostDestroyedCount() { host_destroyed_count_ = 0; }

  // Create a child widget whose native parent is |native_parent_view|, uses
  // |contents_view|, and is attached to |host| which is added as a child to
  // |parent_view|. This effectively borrows the native content view from a
  // newly created child Widget, and attaches it to |host|.
  Widget* CreateChildForHost(gfx::NativeView native_parent_view,
                             View* parent_view,
                             View* contents_view,
                             NativeViewHost* host);

  Widget* toplevel() { return toplevel_.get(); }
  void DestroyTopLevel();

  NativeViewHost* host() { return host_.get(); }
  void DestroyHost();
  NativeViewHost* ReleaseHost();

  NativeViewHostWrapper* GetNativeWrapper();

 private:
  class NativeViewHostTesting;

  std::unique_ptr<Widget> toplevel_;
  std::unique_ptr<NativeViewHost> host_;
  int host_destroyed_count_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostTestBase);
};

}  // namespace test
}  // namespace views
