// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/checkbox.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/views_test_base.h"

namespace views {

class CheckboxTest : public ViewsTestBase {
 public:
  CheckboxTest() = default;

  CheckboxTest(const CheckboxTest&) = delete;
  CheckboxTest& operator=(const CheckboxTest&) = delete;

  ~CheckboxTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that the Checkbox can query the hover state
    // correctly.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();

    checkbox_ = widget_->SetContentsView(std::make_unique<Checkbox>());
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  Checkbox* checkbox() { return checkbox_; }

 private:
  std::unique_ptr<Widget> widget_;
  raw_ptr<Checkbox> checkbox_ = nullptr;
};

TEST_F(CheckboxTest, AccessibilityTest) {
  const std::u16string label_text = u"Some label";
  StyledLabel label;
  label.SetText(label_text);
  checkbox()->SetAssociatedLabel(&label);

  ui::AXNodeData ax_data;
  checkbox()->GetAccessibleNodeData(&ax_data);

  EXPECT_EQ(ax_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            label_text);
  EXPECT_EQ(ax_data.role, ax::mojom::Role::kCheckBox);
}

}  // namespace views
