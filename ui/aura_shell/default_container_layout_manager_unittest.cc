// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/default_container_layout_manager.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/desktop.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/workspace/workspace.h"
#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/aura_shell/workspace_controller.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/native_widget_aura.h"

namespace aura_shell {
namespace test {

namespace {

using views::Widget;
using aura_shell::internal::DefaultContainerLayoutManager;

class DefaultContainerLayoutManagerTest : public aura::test::AuraTestBase {
 public:
  DefaultContainerLayoutManagerTest() : layout_manager_(NULL) {}
  virtual ~DefaultContainerLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    aura::Desktop* desktop = aura::Desktop::GetInstance();
    container_.reset(
        CreateTestWindow(gfx::Rect(0, 0, 500, 400), desktop));
    workspace_controller_.reset(
        new aura_shell::internal::WorkspaceController(container_.get()));
    layout_manager_ = new DefaultContainerLayoutManager(
        workspace_controller_->workspace_manager());
    container_->SetLayoutManager(layout_manager_);

    desktop->SetHostSize(gfx::Size(500, 400));
  }

  aura::Window* CreateTestWindowWithType(const gfx::Rect& bounds,
                                         aura::Window* parent,
                                         aura::WindowType type) {
    aura::Window* window = new aura::Window(NULL);
    window->SetType(type);
    window->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    window->SetBounds(bounds);
    window->Show();
    window->SetParent(parent);
    return window;
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds,
                                 aura::Window* parent) {
    return CreateTestWindowWithType(bounds,
                                    parent,
                                    aura::WINDOW_TYPE_NORMAL);
  }

  aura::Window* container() { return container_.get(); }

  DefaultContainerLayoutManager* default_container_layout_manager() {
    return layout_manager_;
  }

 protected:
  aura_shell::internal::WorkspaceManager* workspace_manager() {
    return workspace_controller_->workspace_manager();
  }

 private:
  scoped_ptr<aura::Window> container_;
  scoped_ptr<aura_shell::internal::WorkspaceController> workspace_controller_;
  // LayoutManager is owned by |container|.
  aura_shell::internal::DefaultContainerLayoutManager* layout_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultContainerLayoutManagerTest);
};

// Utility functions to set and get show state on |window|.
void Maximize(aura::Window* window) {
  window->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
}

void Fullscreen(aura::Window* window) {
  window->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
}

void Restore(aura::Window* window) {
  window->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_NORMAL);
}

ui::WindowShowState GetShowState(aura::Window* window) {
  return static_cast<ui::WindowShowState>(
      window->GetIntProperty(aura::kShowStateKey));
}

}  // namespace

TEST_F(DefaultContainerLayoutManagerTest, SetBounds) {
  // Layout Manager moves the window to (0,0) to fit to draggable area.
  scoped_ptr<aura::Window> child(
      CreateTestWindow(gfx::Rect(0, -1000, 100, 100), container()));
  // Window is centered in workspace.
  EXPECT_EQ("200,0 100x100", child->bounds().ToString());

  // DCLM enforces the window height can't be taller than its owner's height.
  child->SetBounds(gfx::Rect(0, 0, 100, 500));
  EXPECT_EQ("200,0 100x400", child->bounds().ToString());

  // DCLM enforces the window width can't be wider than its owner's width.
  child->SetBounds(gfx::Rect(0, 0, 900, 500));
  EXPECT_EQ("0,0 500x400", child->bounds().ToString());

  // Y origin must always be the top of drag area.
  child->SetBounds(gfx::Rect(0, 500, 900, 500));
  EXPECT_EQ("0,0 500x400", child->bounds().ToString());
  child->SetBounds(gfx::Rect(0, -500, 900, 500));
  EXPECT_EQ("0,0 500x400", child->bounds().ToString());
}

TEST_F(DefaultContainerLayoutManagerTest, DragWindow) {
  scoped_ptr<aura::Window> child(
      CreateTestWindow(gfx::Rect(0, -1000, 50, 50), container()));
  gfx::Rect original_bounds = child->bounds();

  default_container_layout_manager()->PrepareForMoveOrResize(
      child.get(), NULL);
  // X origin must fit within viewport.
  child->SetBounds(gfx::Rect(-100, 500, 50, 50));
  EXPECT_EQ("0,0 50x50", child->GetTargetBounds().ToString());
  child->SetBounds(gfx::Rect(1000, 500, 50, 50));
  EXPECT_EQ("450,0 50x50", child->GetTargetBounds().ToString());
  default_container_layout_manager()->EndMove(child.get(), NULL);
  EXPECT_EQ(original_bounds.ToString(), child->GetTargetBounds().ToString());
}

TEST_F(DefaultContainerLayoutManagerTest, Popup) {
  scoped_ptr<aura::Window> popup(
      CreateTestWindowWithType(gfx::Rect(0, -1000, 100, 100),
                               container(),
                               aura::WINDOW_TYPE_POPUP));
  // A popup window can be placed outside of draggable area.
  EXPECT_EQ("0,-1000 100x100", popup->bounds().ToString());

  // A popup window can be moved to outside of draggable area.
  popup->SetBounds(gfx::Rect(-100, 0, 100, 100));
  EXPECT_EQ("-100,0 100x100", popup->bounds().ToString());

  // A popup window can be resized to the size bigger than draggable area.
  popup->SetBounds(gfx::Rect(0, 0, 1000, 1000));
  EXPECT_EQ("0,0 1000x1000", popup->bounds().ToString());
}

// Make sure a window with a transient parent isn't resized by the layout
// manager.
TEST_F(DefaultContainerLayoutManagerTest, IgnoreTransient) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::WINDOW_TYPE_NORMAL);
  window->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
  aura::Desktop::GetInstance()->AddTransientChild(window.get());
  window->SetBounds(gfx::Rect(0, 0, 200, 200));
  window->Show();
  window->SetParent(container());

  EXPECT_EQ("0,0 200x200", window->bounds().ToString());
}

TEST_F(DefaultContainerLayoutManagerTest, Fullscreen) {
  scoped_ptr<aura::Window> w(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100), container()));
  gfx::Rect fullscreen_bounds =
      workspace_manager()->FindBy(w.get())->bounds();
  gfx::Rect original_bounds = w->GetTargetBounds();

  // Restoreing the restored window.
  Restore(w.get());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetShowState(w.get()));
  EXPECT_EQ(original_bounds.ToString(), w->bounds().ToString());

  // Fullscreen
  Fullscreen(w.get());
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN, GetShowState(w.get()));
  EXPECT_EQ(fullscreen_bounds.ToString(), w->bounds().ToString());
  w->SetIntProperty(aura::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetShowState(w.get()));
  EXPECT_EQ(original_bounds.ToString(), w->bounds().ToString());

  Fullscreen(w.get());
  // Setting |ui::SHOW_STATE_FULLSCREEN| should have no additional effect.
  Fullscreen(w.get());
  EXPECT_EQ(fullscreen_bounds, w->bounds());
  Restore(w.get());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetShowState(w.get()));
  EXPECT_EQ(original_bounds.ToString(), w->bounds().ToString());

  // Calling SetBounds() in fullscreen mode should only update the
  // restore bounds not change the bounds of the window.
  gfx::Rect new_bounds(50, 50, 50, 50);
  Fullscreen(w.get());
  w->SetBounds(new_bounds);
  EXPECT_EQ(fullscreen_bounds.ToString(), w->bounds().ToString());
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN, GetShowState(w.get()));
  Restore(w.get());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetShowState(w.get()));
  EXPECT_EQ(50, w->bounds().height());
}

TEST_F(DefaultContainerLayoutManagerTest, Maximized) {
  scoped_ptr<aura::Window> w(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100), container()));
  gfx::Rect original_bounds = w->GetTargetBounds();
  gfx::Rect fullscreen_bounds =
      workspace_manager()->FindBy(w.get())->bounds();
  gfx::Rect work_area_bounds =
      workspace_manager()->FindBy(w.get())->GetWorkAreaBounds();

  // Maximized
  Maximize(w.get());
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, GetShowState(w.get()));
  EXPECT_EQ(work_area_bounds.ToString(), w->bounds().ToString());
  Restore(w.get());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetShowState(w.get()));
  EXPECT_EQ(original_bounds.ToString(), w->bounds().ToString());

  // Maximize twice
  Maximize(w.get());
  Maximize(w.get());
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, GetShowState(w.get()));
  EXPECT_EQ(work_area_bounds.ToString(), w->bounds().ToString());
  Restore(w.get());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetShowState(w.get()));
  EXPECT_EQ(original_bounds.ToString(), w->bounds().ToString());

  // Maximized -> Fullscreen -> Maximized -> Normal
  Maximize(w.get());
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, GetShowState(w.get()));
  EXPECT_EQ(work_area_bounds.ToString(), w->bounds().ToString());
  Fullscreen(w.get());
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN, GetShowState(w.get()));
  EXPECT_EQ(fullscreen_bounds.ToString(), w->bounds().ToString());
  Maximize(w.get());
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, GetShowState(w.get()));
  EXPECT_EQ(work_area_bounds.ToString(), w->bounds().ToString());
  Restore(w.get());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetShowState(w.get()));
  EXPECT_EQ(original_bounds.ToString(), w->bounds().ToString());

  // Calling SetBounds() in maximized mode mode should only update the
  // restore bounds not change the bounds of the window.
  gfx::Rect new_bounds(50, 50, 50, 50);
  Maximize(w.get());
  w->SetBounds(new_bounds);
  EXPECT_EQ(work_area_bounds.ToString(), w->bounds().ToString());
  Restore(w.get());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetShowState(w.get()));
  EXPECT_EQ(50, w->bounds().height());
}

// Tests that fullscreen windows get resized after desktop is resized.
TEST_F(DefaultContainerLayoutManagerTest, FullscreenAfterDesktopResize) {
  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(300, 400),
                                               container()));
  gfx::Rect window_bounds = w1->GetTargetBounds();
  gfx::Rect fullscreen_bounds =
      workspace_manager()->FindBy(w1.get())->bounds();

  w1->Show();
  EXPECT_EQ(window_bounds.ToString(), w1->bounds().ToString());

  Fullscreen(w1.get());
  EXPECT_EQ(fullscreen_bounds.ToString(), w1->bounds().ToString());

  // Resize the desktop.
  aura::Desktop* desktop = aura::Desktop::GetInstance();
  gfx::Size new_desktop_size = desktop->GetHostSize();
  new_desktop_size.Enlarge(100, 200);
  desktop->OnHostResized(new_desktop_size);

  gfx::Rect new_fullscreen_bounds =
      workspace_manager()->FindBy(w1.get())->bounds();
  EXPECT_NE(fullscreen_bounds.size().ToString(),
            new_fullscreen_bounds.size().ToString());

  EXPECT_EQ(new_fullscreen_bounds.ToString(),
            w1->GetTargetBounds().ToString());

  Restore(w1.get());

  // The following test does not pass due to crbug.com/102413.
  // TODO(oshima): Re-enable this once the bug is fixed.
  // EXPECT_EQ(window_bounds.size().ToString(),
  //           w1->GetTargetBounds().size().ToString());
}

// Tests that maximized windows get resized after desktop is resized.
TEST_F(DefaultContainerLayoutManagerTest, MaximizeAfterDesktopResize) {
  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(300, 400),
                                               container()));
  gfx::Rect window_bounds = w1->GetTargetBounds();
  gfx::Rect work_area_bounds =
      workspace_manager()->FindBy(w1.get())->GetWorkAreaBounds();

  w1->Show();
  EXPECT_EQ(window_bounds.ToString(), w1->bounds().ToString());

  Maximize(w1.get());
  EXPECT_EQ(work_area_bounds.ToString(), w1->bounds().ToString());

  // Resize the desktop.
  aura::Desktop* desktop = aura::Desktop::GetInstance();
  gfx::Size new_desktop_size = desktop->GetHostSize();
  new_desktop_size.Enlarge(100, 200);
  desktop->OnHostResized(new_desktop_size);

  gfx::Rect new_work_area_bounds =
      workspace_manager()->FindBy(w1.get())->bounds();
  EXPECT_NE(work_area_bounds.size().ToString(),
            new_work_area_bounds.size().ToString());

  EXPECT_EQ(new_work_area_bounds.ToString(),
            w1->GetTargetBounds().ToString());

  Restore(w1.get());
  // The following test does not pass due to crbug.com/102413.
  // TODO(oshima): Re-enable this once the bug is fixed.
  // EXPECT_EQ(window_bounds.size().ToString(),
  //           w1->GetTargetBounds().size().ToString());
}

}  // namespace test
}  // namespace aura_shell
