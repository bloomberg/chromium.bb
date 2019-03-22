// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/window_element.h"

#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "ui/aura/window.h"
#include "ui/views/test/views_test_base.h"
#include "ui/wm/core/window_util.h"

namespace ui_devtools {
using ::testing::_;

class WindowElementTest : public views::ViewsTestBase {
 public:
  WindowElementTest() {}
  ~WindowElementTest() override {}

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    window_.reset(new aura::Window(nullptr, aura::client::WINDOW_TYPE_NORMAL));
    window_->Init(ui::LAYER_NOT_DRAWN);
    aura::Window* root_window = GetContext();
    DCHECK(root_window);
    root_window->AddChild(window_.get());
    delegate_.reset(new testing::NiceMock<MockUIElementDelegate>);
    // |OnUIElementAdded| is called on element creation.
    EXPECT_CALL(*delegate_, OnUIElementAdded(_, _)).Times(1);
    element_.reset(new WindowElement(window_.get(), delegate_.get(), nullptr));
  }

  void TearDown() override {
    // The root window needs to be empty by teardown.
    element_.reset();
    delegate_.reset();
    window_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  aura::Window* window() { return window_.get(); }
  WindowElement* element() { return element_.get(); }
  MockUIElementDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<WindowElement> element_;
  std::unique_ptr<MockUIElementDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(WindowElementTest);
};

TEST_F(WindowElementTest, SettingsBoundsOnWindowCallsDelegate) {
  EXPECT_CALL(*delegate(), OnUIElementBoundsChanged(element())).Times(1);
  window()->SetBounds(gfx::Rect(10, 20, 300, 400));
}

TEST_F(WindowElementTest, SettingsBoundsOnElementSetsOnWindow) {
  gfx::Rect old_bounds = window()->bounds();

  gfx::Rect new_bounds = gfx::Rect(10, 20, 300, 400);
  DCHECK(old_bounds != new_bounds);

  element()->SetBounds(new_bounds);
  EXPECT_EQ(window()->bounds(), new_bounds);
}

TEST_F(WindowElementTest, SettingVisibleOnElementSetsOnWindow) {
  DCHECK(!window()->IsVisible());

  element()->SetVisible(true);
  EXPECT_TRUE(window()->IsVisible());

  element()->SetVisible(false);
  EXPECT_FALSE(window()->IsVisible());
}

TEST_F(WindowElementTest, GetVisible) {
  bool visible;

  window()->Hide();
  element()->GetVisible(&visible);
  EXPECT_FALSE(visible);

  window()->Show();
  element()->GetVisible(&visible);
  EXPECT_TRUE(visible);
}

TEST_F(WindowElementTest, GetBounds) {
  gfx::Rect actual_bounds;

  gfx::Rect new_bounds = gfx::Rect(10, 20, 300, 400);
  window()->SetBounds(new_bounds);
  element()->GetBounds(&actual_bounds);
  EXPECT_EQ(actual_bounds, new_bounds);
}

TEST_F(WindowElementTest, GetAttributes) {
  std::string window_name("A window name");
  window()->SetName(window_name);

  std::unique_ptr<protocol::Array<std::string>> attrs =
      element()->GetAttributes();

  DCHECK_EQ(attrs->length(), 4U);

  EXPECT_EQ(attrs->get(0), "name");
  EXPECT_EQ(attrs->get(1), window_name);

  DCHECK(!wm::IsActiveWindow(window()));
  EXPECT_EQ(attrs->get(2), "active");
  EXPECT_EQ(attrs->get(3), "false");

  wm::ActivateWindow(window());
  attrs = element()->GetAttributes();
  DCHECK_EQ(attrs->length(), 4U);
  EXPECT_EQ(attrs->get(2), "active");
  EXPECT_EQ(attrs->get(3), "true");
}
}  // namespace ui_devtools
