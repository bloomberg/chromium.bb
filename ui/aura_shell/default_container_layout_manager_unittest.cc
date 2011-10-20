// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/default_container_layout_manager.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/desktop.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/window.h"

namespace aura_shell {
namespace test {

namespace {

class DefaultContainerLayoutManagerTest : public aura::test::AuraTestBase {
 public:
  DefaultContainerLayoutManagerTest() {}
  virtual ~DefaultContainerLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    aura::Desktop* desktop = aura::Desktop::GetInstance();
    container_.reset(
        CreateTestWindow(gfx::Rect(0, 0, 500, 400), desktop));
    // draggable area is 0,0 500x400.
    container_->SetLayoutManager(
        new aura_shell::internal::DefaultContainerLayoutManager(
            container_.get()));
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds,
                                 aura::Window* parent) {
    aura::Window* window = new aura::Window(NULL);
    window->Init();
    window->SetBounds(bounds);
    window->Show();
    window->SetParent(parent);
    return window;
  }

  aura::Window* container() { return container_.get(); }

 private:
  scoped_ptr<aura::Window> container_;

  DISALLOW_COPY_AND_ASSIGN(DefaultContainerLayoutManagerTest);
};

}  // namespace

TEST_F(DefaultContainerLayoutManagerTest, SetBounds) {
  // Layout Manager moves the window to (0,0) to fit to draggable area.
  scoped_ptr<aura::Window> child(
      CreateTestWindow(gfx::Rect(0, -1000, 100, 100), container()));
  EXPECT_EQ("0,0 100x100", child->bounds().ToString());

  // DCLM enforces the window height can't be taller than its owner's height.
  child->SetBounds(gfx::Rect(0, 0, 100, 500));
  EXPECT_EQ("0,0 100x400", child->bounds().ToString());

  // DCLM enforces the window width can't be wider than its owner's width.
  child->SetBounds(gfx::Rect(0, 0, 900, 500));
  EXPECT_EQ("0,0 500x400", child->bounds().ToString());

  // Y origin must always be the top of drag area.
  child->SetBounds(gfx::Rect(0, 500, 900, 500));
  EXPECT_EQ("0,0 500x400", child->bounds().ToString());
  child->SetBounds(gfx::Rect(0, -500, 900, 500));
  EXPECT_EQ("0,0 500x400", child->bounds().ToString());

  // X origin can be anywhere.
  child->SetBounds(gfx::Rect(-100, 500, 900, 500));
  EXPECT_EQ("-100,0 500x400", child->bounds().ToString());
  child->SetBounds(gfx::Rect(1000, 500, 900, 500));
  EXPECT_EQ("1000,0 500x400", child->bounds().ToString());
}

}  // namespace test
}  // namespace aura_shell
