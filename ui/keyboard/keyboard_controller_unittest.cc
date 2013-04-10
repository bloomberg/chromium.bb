// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_proxy.h"

namespace keyboard {
namespace {

class KeyboardControllerTest : public testing::Test {
 public:
  KeyboardControllerTest() {}
  virtual ~KeyboardControllerTest() {}

  virtual void SetUp() OVERRIDE {
    aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
    aura_test_helper_->SetUp();
  }

  virtual void TearDown() OVERRIDE {
    aura_test_helper_->TearDown();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  scoped_ptr<aura::test::AuraTestHelper> aura_test_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerTest);
};

class TestKeyboardControllerProxy : public KeyboardControllerProxy {
 public:
  TestKeyboardControllerProxy() : window_(new aura::Window(NULL)) {
    window_->Init(ui::LAYER_NOT_DRAWN);
    window_->set_owned_by_parent(false);
  }
  virtual ~TestKeyboardControllerProxy() {}
  virtual aura::Window* GetKeyboardWindow() OVERRIDE { return window_.get(); }

 private:
  scoped_ptr<aura::Window> window_;
  DISALLOW_COPY_AND_ASSIGN(TestKeyboardControllerProxy);
};

}  // namespace

TEST_F(KeyboardControllerTest, KeyboardSize) {
  KeyboardControllerProxy* proxy = new TestKeyboardControllerProxy();
  KeyboardController controller(proxy);

  scoped_ptr<aura::Window> container(controller.GetContainerWindow());
  gfx::Rect bounds(0, 0, 100, 100);
  container->SetBounds(bounds);

  const gfx::Rect& before_bounds = proxy->GetKeyboardWindow()->bounds();
  gfx::Rect new_bounds(
      before_bounds.x(), before_bounds.y(),
      before_bounds.width() / 2, before_bounds.height() / 2);

  // The KeyboardController's LayoutManager shouldn't let this happen
  proxy->GetKeyboardWindow()->SetBounds(new_bounds);
  ASSERT_EQ(before_bounds, proxy->GetKeyboardWindow()->bounds());
}

}  // namespace keyboard
