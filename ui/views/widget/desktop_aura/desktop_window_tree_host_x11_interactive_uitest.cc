// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <X11/Xlib.h>

// Get rid of X11 macros which conflict with gtest.
#undef Bool
#undef None

#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gl/gl_surface.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/x11_property_change_waiter.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

namespace views {

namespace {

// Blocks till |window| gets activated.
class ActivationWaiter : public X11PropertyChangeWaiter {
 public:
  explicit ActivationWaiter(XID window)
      : X11PropertyChangeWaiter(ui::GetX11RootWindow(), "_NET_ACTIVE_WINDOW"),
        window_(window) {
  }

  virtual ~ActivationWaiter() {
  }

 private:
  // X11PropertyChangeWaiter:
  virtual bool ShouldKeepOnWaiting(const ui::PlatformEvent& event) OVERRIDE {
    XID xid = 0;
    ui::GetXIDProperty(ui::GetX11RootWindow(), "_NET_ACTIVE_WINDOW", &xid);
    return xid != window_;
  }

  XID window_;

  DISALLOW_COPY_AND_ASSIGN(ActivationWaiter);
};

// Creates a widget of size 100x100.
scoped_ptr<Widget> CreateWidget() {
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.remove_standard_frame = true;
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget->Init(params);
  return widget.Pass();
}

}  // namespace

class DesktopWindowTreeHostX11Test : public ViewsTestBase {
 public:
  DesktopWindowTreeHostX11Test() {
  }
  virtual ~DesktopWindowTreeHostX11Test() {
  }

  static void SetUpTestCase() {
    gfx::GLSurface::InitializeOneOffForTests();
    ui::RegisterPathProvider();
    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
  }

  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();

    // Make X11 synchronous for our display connection. This does not force the
    // window manager to behave synchronously.
    XSynchronize(gfx::GetXDisplay(), True);
  }

  virtual void TearDown() OVERRIDE {
    XSynchronize(gfx::GetXDisplay(), False);
    ViewsTestBase::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostX11Test);
};

// Test that calling Widget::Deactivate() sets the widget as inactive wrt to
// Chrome even if it not possible to deactivate the window wrt to the x server.
// This behavior is required by several interactive_ui_tests.
TEST_F(DesktopWindowTreeHostX11Test, Deactivate) {
  scoped_ptr<Widget> widget(CreateWidget());

  ActivationWaiter waiter(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  widget->Show();
  widget->Activate();
  waiter.Wait();

  widget->Deactivate();
  // Regardless of whether |widget|'s X11 window eventually gets deactivated,
  // |widget|'s "active" state should change.
  EXPECT_FALSE(widget->IsActive());

  // |widget|'s X11 window should still be active. Reactivating |widget| should
  // update the widget's "active" state.
  // Note: Activating a widget whose X11 window is not active does not
  // synchronously update the widget's "active" state.
  widget->Activate();
  EXPECT_TRUE(widget->IsActive());
}

}  // namespace views
