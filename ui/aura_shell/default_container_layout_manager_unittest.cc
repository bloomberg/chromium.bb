// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/default_container_layout_manager.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/desktop.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/base/view_prop.h"
#include "views/widget/native_widget_aura.h"

namespace aura_shell {
namespace test {

namespace {

using views::Widget;
using aura_shell::internal::DefaultContainerLayoutManager;

class DefaultContainerLayoutManagerTest : public aura::test::AuraTestBase {
 public:
  DefaultContainerLayoutManagerTest() {}
  virtual ~DefaultContainerLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    aura::Desktop* desktop = aura::Desktop::GetInstance();
    container_.reset(
        CreateTestWindow(gfx::Rect(0, 0, 500, 400), desktop));
    workspace_manager_.reset(new WorkspaceManager(container_.get()));

    desktop->SetHostSize(gfx::Size(500, 400));
    default_container_layout_manager_ = new DefaultContainerLayoutManager(
        container_.get(), workspace_manager_.get());
    // draggable area is 0,0 500x400.
    container_->SetLayoutManager(default_container_layout_manager_);
  }

  aura::Window* CreateTestWindowWithType(const gfx::Rect& bounds,
                                         aura::Window* parent,
                                         Widget::InitParams::Type type) {
    aura::Window* window = new aura::Window(NULL);
    props_.push_back(new ui::ViewProp(
        window, views::NativeWidgetAura::kWindowTypeKey,
        reinterpret_cast<void*>(type)));
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
                                    Widget::InitParams::TYPE_WINDOW);
  }

  aura::Window* container() { return container_.get(); }

  DefaultContainerLayoutManager* default_container_layout_manager() {
    return default_container_layout_manager_;
  }


 private:
  scoped_ptr<aura::Window> container_;
  ScopedVector<ui::ViewProp> props_;
  scoped_ptr<aura_shell::WorkspaceManager> workspace_manager_;
  DefaultContainerLayoutManager* default_container_layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(DefaultContainerLayoutManagerTest);
};

}  // namespace

#if !defined(OS_WIN)
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
#endif

#if !defined(OS_WIN)
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
#endif

TEST_F(DefaultContainerLayoutManagerTest, Popup) {
  scoped_ptr<aura::Window> popup(
      CreateTestWindowWithType(gfx::Rect(0, -1000, 100, 100),
                               container(),
                               Widget::InitParams::TYPE_POPUP));
  // A popup window can be placed outside of draggable area.
  EXPECT_EQ("0,-1000 100x100", popup->bounds().ToString());

  // A popup window can be moved to outside of draggable area.
  popup->SetBounds(gfx::Rect(-100, 0, 100, 100));
  EXPECT_EQ("-100,0 100x100", popup->bounds().ToString());

  // A popup window can be resized to the size bigger than draggable area.
  popup->SetBounds(gfx::Rect(0, 0, 1000, 1000));
  EXPECT_EQ("0,0 1000x1000", popup->bounds().ToString());
}

}  // namespace test
}  // namespace aura_shell
