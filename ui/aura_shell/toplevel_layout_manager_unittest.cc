// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/toplevel_layout_manager.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/desktop.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/base/ui_base_types.h"
#include "ui/aura/window.h"

namespace aura_shell {

namespace {

class ToplevelLayoutManagerTest : public aura::test::AuraTestBase {
 public:
  ToplevelLayoutManagerTest() : layout_manager_(NULL) {}
  virtual ~ToplevelLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    aura::Desktop::GetInstance()->screen()->set_work_area_insets(
        gfx::Insets(1, 2, 3, 4));
    aura::Desktop::GetInstance()->SetHostSize(gfx::Size(500, 400));
    container_.reset(new aura::Window(NULL));
    container_->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    container_->SetBounds(gfx::Rect(0, 0, 500, 500));
    layout_manager_ = new internal::ToplevelLayoutManager();
    container_->SetLayoutManager(layout_manager_);
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    aura::Window* window = new aura::Window(NULL);
    window->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    window->SetBounds(bounds);
    window->Show();
    window->SetParent(container_.get());
    return window;
  }

 private:
  // Owned by |container_|.
  internal::ToplevelLayoutManager* layout_manager_;

  scoped_ptr<aura::Window> container_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelLayoutManagerTest);
};

}  // namespace

// Tests normal->maximize->normal.
TEST_F(ToplevelLayoutManagerTest, Maximize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(gfx::Screen::GetMonitorWorkAreaNearestWindow(window.get()),
            window->bounds());
  window->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds, window->bounds());
}

// Tests normal->fullscreen->normal.
TEST_F(ToplevelLayoutManagerTest, Fullscreen) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(gfx::Screen::GetMonitorAreaNearestWindow(window.get()),
            window->bounds());
  window->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds, window->bounds());
}

}  // namespace aura_shell
