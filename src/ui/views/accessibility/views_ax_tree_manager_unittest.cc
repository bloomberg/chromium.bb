// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/views_ax_tree_manager.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace test {

namespace {

class TestButton : public Button {
 public:
  TestButton() : Button(nullptr) {}
  TestButton(const TestButton&) = delete;
  TestButton& operator=(const TestButton&) = delete;
  ~TestButton() override = default;
};

class ViewsAXTreeManagerTest : public ViewsTestBase {
 public:
  ViewsAXTreeManagerTest() = default;
  ~ViewsAXTreeManagerTest() override = default;
  ViewsAXTreeManagerTest(const ViewsAXTreeManagerTest&) = delete;
  ViewsAXTreeManagerTest& operator=(const ViewsAXTreeManagerTest&) = delete;

 protected:
  void SetUp() override;
  void TearDown() override;
  ui::AXNode* FindNode(const ax::mojom::Role role,
                       const std::string& name_or_value) const;
  void WaitFor(const ui::AXEventGenerator::Event event);

  Widget* widget() const { return widget_; }
  Button* button() const { return button_; }
  Label* label() const { return label_; }
  const ViewsAXTreeManager& manager() const { return *manager_; }

 private:
  ui::AXNode* FindNodeInSubtree(ui::AXNode* root,
                                const ax::mojom::Role role,
                                const std::string& name_or_value) const;
  void OnGeneratedEvent(Widget* widget,
                        ui::AXEventGenerator::Event event,
                        ui::AXNode::AXID node_id);

  Widget* widget_ = nullptr;
  Button* button_ = nullptr;
  Label* label_ = nullptr;
  std::unique_ptr<ViewsAXTreeManager> manager_;
  ui::AXEventGenerator::Event event_to_wait_for_;
  std::unique_ptr<base::RunLoop> loop_runner_;
};

void ViewsAXTreeManagerTest::SetUp() {
  ViewsTestBase::SetUp();

  widget_ = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  widget_->Init(std::move(params));

  button_ = new TestButton();
  button_->SetSize(gfx::Size(20, 20));

  label_ = new Label();
  button_->AddChildView(label_);

  widget_->GetContentsView()->AddChildView(button_);
  widget_->Show();

  manager_ = std::make_unique<ViewsAXTreeManager>(widget_);
  ASSERT_NE(nullptr, manager_.get());
  manager_->SetGeneratedEventCallbackForTesting(base::BindRepeating(
      &ViewsAXTreeManagerTest::OnGeneratedEvent, base::Unretained(this)));
  WaitFor(ui::AXEventGenerator::Event::LOAD_COMPLETE);
}

void ViewsAXTreeManagerTest::TearDown() {
  manager_->UnsetGeneratedEventCallbackForTesting();
  manager_.reset();
  if (!widget_->IsClosed())
    widget_->Close();
  ViewsTestBase::TearDown();
}

ui::AXNode* ViewsAXTreeManagerTest::FindNode(
    const ax::mojom::Role role,
    const std::string& name_or_value) const {
  ui::AXNode* root = manager_->GetRootAsAXNode();
  EXPECT_NE(nullptr, root);
  return FindNodeInSubtree(root, role, name_or_value);
}

void ViewsAXTreeManagerTest::WaitFor(const ui::AXEventGenerator::Event event) {
  ASSERT_FALSE(loop_runner_ && loop_runner_->IsRunningOnCurrentThread())
      << "Waiting for multiple events is currently not supported.";
  loop_runner_ = std::make_unique<base::RunLoop>();
  event_to_wait_for_ = event;
  loop_runner_->Run();
}

ui::AXNode* ViewsAXTreeManagerTest::FindNodeInSubtree(
    ui::AXNode* root,
    const ax::mojom::Role role,
    const std::string& name_or_value) const {
  EXPECT_NE(nullptr, root);
  const std::string& name =
      root->GetStringAttribute(ax::mojom::StringAttribute::kName);
  const std::string& value =
      root->GetStringAttribute(ax::mojom::StringAttribute::kValue);
  if (root->data().role == role &&
      (name == name_or_value || value == name_or_value)) {
    return root;
  }

  for (ui::AXNode* child : root->children()) {
    ui::AXNode* result = FindNodeInSubtree(child, role, name_or_value);
    if (result)
      return result;
  }
  return nullptr;
}

void ViewsAXTreeManagerTest::OnGeneratedEvent(Widget* widget,
                                              ui::AXEventGenerator::Event event,
                                              ui::AXNode::AXID node_id) {
  ASSERT_NE(nullptr, manager_.get())
      << "Should not be called after TearDown().";
  if (loop_runner_ && event == event_to_wait_for_)
    loop_runner_->Quit();
}

}  // namespace

TEST_F(ViewsAXTreeManagerTest, MirrorInitialTree) {
  ui::AXNodeData button_data;
  button()->GetViewAccessibility().GetAccessibleNodeData(&button_data);
  ui::AXNode* ax_button = FindNode(ax::mojom::Role::kButton, "");
  ASSERT_NE(nullptr, ax_button);
  EXPECT_EQ(button_data.role, ax_button->data().role);
  EXPECT_EQ(
      button_data.GetStringAttribute(ax::mojom::StringAttribute::kDescription),
      ax_button->data().GetStringAttribute(
          ax::mojom::StringAttribute::kDescription));
  EXPECT_EQ(
      button_data.GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb),
      ax_button->data().GetIntAttribute(
          ax::mojom::IntAttribute::kDefaultActionVerb));
  EXPECT_TRUE(ax_button->data().HasState(ax::mojom::State::kFocusable));
}

TEST_F(ViewsAXTreeManagerTest, PerformAction) {
  ui::AXNode* ax_button = FindNode(ax::mojom::Role::kButton, "");
  ASSERT_NE(nullptr, ax_button);
  ASSERT_FALSE(ax_button->data().HasIntAttribute(
      ax::mojom::IntAttribute::kCheckedState));
  button()->SetState(TestButton::STATE_PRESSED);
  button()->NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged,
                                     true);
  WaitFor(ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED);
}

}  // namespace test
}  // namespace views
