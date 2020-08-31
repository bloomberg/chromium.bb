// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_virtual_view.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessibility/view_ax_platform_node_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

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

}  // namespace

class AXVirtualViewTest : public ViewsTestBase {
 public:
  AXVirtualViewTest() = default;
  AXVirtualViewTest(const AXVirtualViewTest&) = delete;
  AXVirtualViewTest& operator=(const AXVirtualViewTest&) = delete;
  ~AXVirtualViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    ui::AXPlatformNode::NotifyAddAXModeFlags(ui::kAXModeComplete);

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));
    button_ = new TestButton;
    button_->SetSize(gfx::Size(20, 20));
    widget_->GetContentsView()->AddChildView(button_);
    virtual_label_ = new AXVirtualView;
    virtual_label_->GetCustomData().role = ax::mojom::Role::kStaticText;
    virtual_label_->GetCustomData().SetName("Label");
    button_->GetViewAccessibility().AddVirtualChildView(
        base::WrapUnique(virtual_label_));
    widget_->Show();

    ViewAccessibility::AccessibilityEventsCallback
        accessibility_events_callback = base::BindRepeating(
            [](std::vector<std::pair<const ui::AXPlatformNodeDelegate*,
                                     const ax::mojom::Event>>*
                   accessibility_events,
               const ui::AXPlatformNodeDelegate* delegate,
               const ax::mojom::Event event_type) {
              DCHECK(accessibility_events);
              accessibility_events->push_back({delegate, event_type});
            },
            &accessibility_events_);
    button_->GetViewAccessibility().set_accessibility_events_callback(
        std::move(accessibility_events_callback));
  }

  void TearDown() override {
    if (!widget_->IsClosed())
      widget_->Close();
    ViewsTestBase::TearDown();
  }

 protected:
  ViewAXPlatformNodeDelegate* GetButtonAccessibility() const {
    return static_cast<ViewAXPlatformNodeDelegate*>(
        &button_->GetViewAccessibility());
  }

  void ExpectReceivedAccessibilityEvents(
      const std::vector<std::pair<const ui::AXPlatformNodeDelegate*,
                                  const ax::mojom::Event>>& expected_events) {
    EXPECT_EQ(accessibility_events_.size(), expected_events.size());

    size_t i = 0;
    for (const auto& actual_event : accessibility_events_) {
      if (i >= expected_events.size())
        break;

      const auto& expected_event = expected_events[i];
      EXPECT_EQ(actual_event.first, expected_event.first);
      EXPECT_EQ(actual_event.second, expected_event.second);
      ++i;
    }

    accessibility_events_.clear();
  }

  Widget* widget_;
  Button* button_;
  // Weak, |button_| owns this.
  AXVirtualView* virtual_label_;

 private:
  std::vector<
      std::pair<const ui::AXPlatformNodeDelegate*, const ax::mojom::Event>>
      accessibility_events_;
};

TEST_F(AXVirtualViewTest, AccessibilityRoleAndName) {
  EXPECT_EQ(ax::mojom::Role::kButton, GetButtonAccessibility()->GetData().role);
  EXPECT_EQ(ax::mojom::Role::kStaticText, virtual_label_->GetData().role);
  EXPECT_EQ("Label", virtual_label_->GetData().GetStringAttribute(
                         ax::mojom::StringAttribute::kName));
}

// The focusable state of a virtual view should not depend on the focusable
// state of the real view ancestor, however the enabled state should.
TEST_F(AXVirtualViewTest, FocusableAndEnabledState) {
  virtual_label_->GetCustomData().AddState(ax::mojom::State::kFocusable);
  EXPECT_TRUE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kFocusable));
  EXPECT_TRUE(virtual_label_->GetData().HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            virtual_label_->GetData().GetRestriction());

  button_->SetFocusBehavior(View::FocusBehavior::NEVER);
  EXPECT_FALSE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kFocusable));
  EXPECT_TRUE(virtual_label_->GetData().HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            virtual_label_->GetData().GetRestriction());

  button_->SetEnabled(false);
  EXPECT_FALSE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kFocusable));
  EXPECT_TRUE(virtual_label_->GetData().HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kDisabled,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kDisabled,
            virtual_label_->GetData().GetRestriction());

  button_->SetEnabled(true);
  button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  virtual_label_->GetCustomData().RemoveState(ax::mojom::State::kFocusable);
  EXPECT_TRUE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kFocusable));
  EXPECT_FALSE(
      virtual_label_->GetData().HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            GetButtonAccessibility()->GetData().GetRestriction());
  EXPECT_EQ(ax::mojom::Restriction::kNone,
            virtual_label_->GetData().GetRestriction());
}

TEST_F(AXVirtualViewTest, VirtualLabelIsChildOfButton) {
  EXPECT_EQ(1, GetButtonAccessibility()->GetChildCount());
  EXPECT_EQ(0, virtual_label_->GetChildCount());
  ASSERT_NE(nullptr, virtual_label_->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_->GetParent());
  ASSERT_NE(nullptr, GetButtonAccessibility()->ChildAtIndex(0));
  EXPECT_EQ(virtual_label_->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(0));
}

TEST_F(AXVirtualViewTest, RemoveFromParentView) {
  ASSERT_EQ(1, GetButtonAccessibility()->GetChildCount());
  std::unique_ptr<AXVirtualView> removed_label =
      virtual_label_->RemoveFromParentView();
  EXPECT_EQ(nullptr, removed_label->GetParent());
  EXPECT_TRUE(GetButtonAccessibility()->virtual_children().empty());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  removed_label->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1, removed_label->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_1->GetParent());
  std::unique_ptr<AXVirtualView> removed_child_1 =
      virtual_child_1->RemoveFromParentView();
  EXPECT_EQ(nullptr, removed_child_1->GetParent());
  EXPECT_EQ(0, removed_label->GetChildCount());
}

TEST_F(AXVirtualViewTest, AddingAndRemovingVirtualChildren) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());
  ExpectReceivedAccessibilityEvents({});

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  EXPECT_EQ(1, virtual_label_->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_1->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  EXPECT_EQ(2, virtual_label_->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_2->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  EXPECT_EQ(2, virtual_label_->GetChildCount());
  EXPECT_EQ(0, virtual_child_1->GetChildCount());
  EXPECT_EQ(1, virtual_child_2->GetChildCount());
  ASSERT_NE(nullptr, virtual_child_3->GetParent());
  EXPECT_EQ(virtual_child_2->GetNativeObject(), virtual_child_3->GetParent());
  ASSERT_NE(nullptr, virtual_child_2->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_2->ChildAtIndex(0));
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  virtual_child_2->RemoveChildView(virtual_child_3);
  EXPECT_EQ(0, virtual_child_2->GetChildCount());
  EXPECT_EQ(2, virtual_label_->GetChildCount());
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  virtual_label_->RemoveAllChildViews();
  EXPECT_EQ(0, virtual_label_->GetChildCount());
  // There should be two "kChildrenChanged" events because Two virtual child
  // views are removed in total.
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(GetButtonAccessibility(),
                      ax::mojom::Event::kChildrenChanged),
       std::make_pair(GetButtonAccessibility(),
                      ax::mojom::Event::kChildrenChanged)});
}

TEST_F(AXVirtualViewTest, ReorderingVirtualChildren) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2, virtual_label_->GetChildCount());

  virtual_label_->ReorderChildView(virtual_child_1, -1);
  ASSERT_EQ(2, virtual_label_->GetChildCount());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));

  virtual_label_->ReorderChildView(virtual_child_1, 0);
  ASSERT_EQ(2, virtual_label_->GetChildCount());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  ASSERT_NE(nullptr, virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));

  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0, virtual_label_->GetChildCount());
}

TEST_F(AXVirtualViewTest, ContainsVirtualChild) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  ASSERT_EQ(1, virtual_child_2->GetChildCount());

  EXPECT_TRUE(button_->GetViewAccessibility().Contains(virtual_label_));
  EXPECT_TRUE(virtual_label_->Contains(virtual_label_));
  EXPECT_TRUE(virtual_label_->Contains(virtual_child_1));
  EXPECT_TRUE(virtual_label_->Contains(virtual_child_2));
  EXPECT_TRUE(virtual_label_->Contains(virtual_child_3));
  EXPECT_TRUE(virtual_child_2->Contains(virtual_child_2));
  EXPECT_TRUE(virtual_child_2->Contains(virtual_child_3));

  EXPECT_FALSE(virtual_child_1->Contains(virtual_label_));
  EXPECT_FALSE(virtual_child_2->Contains(virtual_label_));
  EXPECT_FALSE(virtual_child_3->Contains(virtual_child_2));

  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0, virtual_label_->GetChildCount());
}

TEST_F(AXVirtualViewTest, GetIndexOfVirtualChild) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  ASSERT_EQ(1, virtual_child_2->GetChildCount());

  EXPECT_EQ(-1, virtual_label_->GetIndexOf(virtual_label_));
  EXPECT_EQ(0, virtual_label_->GetIndexOf(virtual_child_1));
  EXPECT_EQ(1, virtual_label_->GetIndexOf(virtual_child_2));
  EXPECT_EQ(-1, virtual_label_->GetIndexOf(virtual_child_3));
  EXPECT_EQ(0, virtual_child_2->GetIndexOf(virtual_child_3));

  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0, virtual_label_->GetChildCount());
}

// Verify that virtual views with invisible ancestors inherit the
// ax::mojom::State::kInvisible state.
TEST_F(AXVirtualViewTest, InvisibleVirtualViews) {
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kInvisible));
  EXPECT_FALSE(
      virtual_label_->GetData().HasState(ax::mojom::State::kInvisible));

  button_->SetVisible(false);
  EXPECT_TRUE(GetButtonAccessibility()->GetData().HasState(
      ax::mojom::State::kInvisible));
  EXPECT_TRUE(virtual_label_->GetData().HasState(ax::mojom::State::kInvisible));
  button_->SetVisible(true);
}

// Verify that ignored virtual views are removed from the accessible tree and
// that their contents are intact.
TEST_F(AXVirtualViewTest, IgnoredVirtualViews) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  // An ignored node should not be exposed.
  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  virtual_child_1->GetCustomData().AddState(ax::mojom::State::kIgnored);
  ASSERT_EQ(0, virtual_label_->GetChildCount());
  ASSERT_EQ(0, virtual_child_1->GetChildCount());

  // The contents of ignored nodes should be exposed.
  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_child_1->AddChildView(base::WrapUnique(virtual_child_2));
  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  AXVirtualView* virtual_child_4 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_4));
  ASSERT_EQ(1, virtual_label_->GetChildCount());
  ASSERT_EQ(1, virtual_child_1->GetChildCount());
  ASSERT_EQ(2, virtual_child_2->GetChildCount());
  ASSERT_EQ(0, virtual_child_3->GetChildCount());
  ASSERT_EQ(0, virtual_child_4->GetChildCount());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  EXPECT_EQ(virtual_child_2->GetNativeObject(), virtual_child_3->GetParent());
  EXPECT_EQ(virtual_child_2->GetNativeObject(), virtual_child_4->GetParent());
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_child_1->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_2->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_child_2->ChildAtIndex(1));

  // The contents of ignored nodes should be unignored accessibility subtrees.
  virtual_child_2->GetCustomData().role = ax::mojom::Role::kIgnored;
  ASSERT_EQ(2, virtual_label_->GetChildCount());
  ASSERT_EQ(2, virtual_child_1->GetChildCount());
  ASSERT_EQ(2, virtual_child_2->GetChildCount());
  ASSERT_EQ(0, virtual_child_3->GetChildCount());
  ASSERT_EQ(0, virtual_child_4->GetChildCount());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_3->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_4->GetParent());
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_1->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_child_1->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_2->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_child_2->ChildAtIndex(1));

  // Test for mixed ignored and unignored virtual children.
  AXVirtualView* virtual_child_5 = new AXVirtualView;
  virtual_child_1->AddChildView(base::WrapUnique(virtual_child_5));
  ASSERT_EQ(3, virtual_label_->GetChildCount());
  ASSERT_EQ(3, virtual_child_1->GetChildCount());
  ASSERT_EQ(2, virtual_child_2->GetChildCount());
  ASSERT_EQ(0, virtual_child_5->GetChildCount());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_1->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_2->GetParent());
  EXPECT_EQ(virtual_label_->GetNativeObject(), virtual_child_5->GetParent());
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_label_->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_label_->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_5->GetNativeObject(),
            virtual_label_->ChildAtIndex(2));

  // An ignored root node should not be exposed.
  virtual_label_->GetCustomData().AddState(ax::mojom::State::kIgnored);
  ASSERT_EQ(3, GetButtonAccessibility()->GetChildCount());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_child_1->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_child_2->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_child_3->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_child_4->GetParent());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_child_5->GetParent());
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(0));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(1));
  EXPECT_EQ(virtual_child_5->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(2));

  // Test for mixed ignored and unignored root nodes.
  AXVirtualView* virtual_label_2 = new AXVirtualView;
  virtual_label_2->GetCustomData().role = ax::mojom::Role::kStaticText;
  virtual_label_2->GetCustomData().SetName("Label");
  button_->GetViewAccessibility().AddVirtualChildView(
      base::WrapUnique(virtual_label_2));
  ASSERT_EQ(4, GetButtonAccessibility()->GetChildCount());
  ASSERT_EQ(0, virtual_label_2->GetChildCount());
  EXPECT_EQ(button_->GetNativeViewAccessible(), virtual_label_2->GetParent());
  EXPECT_EQ(virtual_label_2->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(3));

  // A focusable node should not be ignored.
  virtual_child_1->GetCustomData().AddState(ax::mojom::State::kFocusable);
  ASSERT_EQ(2, GetButtonAccessibility()->GetChildCount());
  ASSERT_EQ(1, virtual_label_->GetChildCount());
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(0));
  EXPECT_EQ(virtual_label_2->GetNativeObject(),
            GetButtonAccessibility()->ChildAtIndex(1));
}

TEST_F(AXVirtualViewTest, OverrideFocus) {
  ViewAccessibility& button_accessibility = button_->GetViewAccessibility();
  ASSERT_NE(nullptr, button_accessibility.GetNativeObject());
  ASSERT_NE(nullptr, virtual_label_->GetNativeObject());
  ExpectReceivedAccessibilityEvents({});

  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  button_accessibility.OverrideFocus(virtual_label_);
  EXPECT_EQ(virtual_label_->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(virtual_label_, ax::mojom::Event::kFocus)});

  button_accessibility.OverrideFocus(nullptr);
  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(GetButtonAccessibility(), ax::mojom::Event::kFocus)});

  ASSERT_EQ(0, virtual_label_->GetChildCount());
  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  ASSERT_EQ(1, virtual_label_->GetChildCount());
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  ASSERT_EQ(2, virtual_label_->GetChildCount());
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  button_accessibility.OverrideFocus(virtual_child_1);
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(virtual_child_1, ax::mojom::Event::kFocus)});

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_3));
  ASSERT_EQ(1, virtual_child_2->GetChildCount());
  ExpectReceivedAccessibilityEvents({std::make_pair(
      GetButtonAccessibility(), ax::mojom::Event::kChildrenChanged)});

  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  button_accessibility.OverrideFocus(virtual_child_3);
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(virtual_child_3, ax::mojom::Event::kFocus)});

  // Test that calling GetFocus() while the owner view is not focused will
  // return nullptr.
  EXPECT_EQ(nullptr, virtual_label_->GetFocus());
  EXPECT_EQ(nullptr, virtual_child_1->GetFocus());
  EXPECT_EQ(nullptr, virtual_child_2->GetFocus());
  EXPECT_EQ(nullptr, virtual_child_3->GetFocus());

  button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  button_->RequestFocus();
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(GetButtonAccessibility(), ax::mojom::Event::kFocus),
       std::make_pair(GetButtonAccessibility(),
                      ax::mojom::Event::kChildrenChanged)});

  // Test that calling GetFocus() from any object in the tree will return the
  // same result.
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_label_->GetFocus());
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_child_1->GetFocus());
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_child_2->GetFocus());
  EXPECT_EQ(virtual_child_3->GetNativeObject(), virtual_child_3->GetFocus());

  virtual_label_->RemoveChildView(virtual_child_2);
  ASSERT_EQ(1, virtual_label_->GetChildCount());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(GetButtonAccessibility(), ax::mojom::Event::kFocus),
       std::make_pair(GetButtonAccessibility(),
                      ax::mojom::Event::kChildrenChanged)});

  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  EXPECT_EQ(button_accessibility.GetNativeObject(), virtual_label_->GetFocus());
  EXPECT_EQ(button_accessibility.GetNativeObject(),
            virtual_child_1->GetFocus());

  button_accessibility.OverrideFocus(virtual_child_1);
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(virtual_child_1, ax::mojom::Event::kFocus)});

  virtual_label_->RemoveAllChildViews();
  ASSERT_EQ(0, virtual_label_->GetChildCount());
  EXPECT_EQ(button_accessibility.GetNativeObject(),
            button_accessibility.GetFocusedDescendant());
  ExpectReceivedAccessibilityEvents(
      {std::make_pair(GetButtonAccessibility(), ax::mojom::Event::kFocus),
       std::make_pair(GetButtonAccessibility(),
                      ax::mojom::Event::kChildrenChanged)});
}

TEST_F(AXVirtualViewTest, Navigation) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  EXPECT_EQ(1, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_2));
  EXPECT_EQ(2, virtual_label_->GetChildCount());

  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_3));

  AXVirtualView* virtual_child_4 = new AXVirtualView;
  virtual_child_2->AddChildView(base::WrapUnique(virtual_child_4));

  EXPECT_EQ(nullptr, virtual_label_->GetNextSibling());
  EXPECT_EQ(nullptr, virtual_label_->GetPreviousSibling());
  EXPECT_EQ(0, virtual_label_->GetIndexInParent());

  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_child_1->GetNextSibling());
  EXPECT_EQ(nullptr, virtual_child_1->GetPreviousSibling());
  EXPECT_EQ(0, virtual_child_1->GetIndexInParent());

  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_child_2->GetNextSibling());
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_child_2->GetPreviousSibling());
  EXPECT_EQ(1, virtual_child_2->GetIndexInParent());

  EXPECT_EQ(nullptr, virtual_child_3->GetNextSibling());
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_child_3->GetPreviousSibling());
  EXPECT_EQ(2, virtual_child_3->GetIndexInParent());

  EXPECT_EQ(nullptr, virtual_child_4->GetNextSibling());
  EXPECT_EQ(nullptr, virtual_child_4->GetPreviousSibling());
  EXPECT_EQ(0, virtual_child_4->GetIndexInParent());
}

TEST_F(AXVirtualViewTest, HitTesting) {
  ASSERT_EQ(0, virtual_label_->GetChildCount());

  const gfx::Vector2d offset_from_origin =
      button_->GetBoundsInScreen().OffsetFromOrigin();

  // Test that hit testing is recursive.
  AXVirtualView* virtual_child_1 = new AXVirtualView;
  virtual_child_1->GetCustomData().relative_bounds.bounds =
      gfx::RectF(0, 0, 10, 10);
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_1));
  AXVirtualView* virtual_child_2 = new AXVirtualView;
  virtual_child_2->GetCustomData().relative_bounds.bounds =
      gfx::RectF(5, 5, 5, 5);
  virtual_child_1->AddChildView(base::WrapUnique(virtual_child_2));
  gfx::Point point_1 = gfx::Point(2, 2) + offset_from_origin;
  EXPECT_EQ(virtual_child_1->GetNativeObject(),
            virtual_child_1->HitTestSync(point_1.x(), point_1.y()));
  gfx::Point point_2 = gfx::Point(7, 7) + offset_from_origin;
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->HitTestSync(point_2.x(), point_2.y()));

  // Test that hit testing follows the z-order.
  AXVirtualView* virtual_child_3 = new AXVirtualView;
  virtual_child_3->GetCustomData().relative_bounds.bounds =
      gfx::RectF(5, 5, 10, 10);
  virtual_label_->AddChildView(base::WrapUnique(virtual_child_3));
  AXVirtualView* virtual_child_4 = new AXVirtualView;
  virtual_child_4->GetCustomData().relative_bounds.bounds =
      gfx::RectF(10, 10, 10, 10);
  virtual_child_3->AddChildView(base::WrapUnique(virtual_child_4));
  EXPECT_EQ(virtual_child_3->GetNativeObject(),
            virtual_label_->HitTestSync(point_2.x(), point_2.y()));
  gfx::Point point_3 = gfx::Point(12, 12) + offset_from_origin;
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_label_->HitTestSync(point_3.x(), point_3.y()));

  // Test that hit testing skips ignored nodes but not their descendants.
  virtual_child_3->GetCustomData().AddState(ax::mojom::State::kIgnored);
  EXPECT_EQ(virtual_child_2->GetNativeObject(),
            virtual_label_->HitTestSync(point_2.x(), point_2.y()));
  EXPECT_EQ(virtual_child_4->GetNativeObject(),
            virtual_label_->HitTestSync(point_3.x(), point_3.y()));
}

// Test for GetTargetForNativeAccessibilityEvent().
#if defined(OS_WIN)
TEST_F(AXVirtualViewTest, GetTargetForEvents) {
  EXPECT_EQ(button_, virtual_label_->GetOwnerView());
  EXPECT_NE(nullptr, HWNDForView(virtual_label_->GetOwnerView()));
  EXPECT_EQ(HWNDForView(button_),
            virtual_label_->GetTargetForNativeAccessibilityEvent());
}
#endif

}  // namespace test
}  // namespace views
