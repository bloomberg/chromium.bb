// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"

namespace content {

class BrowserAccessibilityManagerWinTest : public testing::Test {
 public:
  BrowserAccessibilityManagerWinTest() = default;
  ~BrowserAccessibilityManagerWinTest() override = default;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  void SetUp() override;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerWinTest);
};

void BrowserAccessibilityManagerWinTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<TestBrowserAccessibilityDelegate>();
}

TEST_F(BrowserAccessibilityManagerWinTest, DynamicallyAddedIFrame) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalUIAutomation);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> root_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root), test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  ui::AXPlatformNode* root_document_root_node =
      ui::AXPlatformNode::FromNativeViewAccessible(
          root_manager->GetRoot()->GetNativeViewAccessible());

  std::unique_ptr<ui::AXPlatformNodeDelegate> fragment_root =
      std::make_unique<ui::AXFragmentRootWin>(
          gfx::kMockAcceleratedWidget,
          static_cast<ui::AXPlatformNodeWin*>(root_document_root_node));

  EXPECT_EQ(fragment_root->GetChildCount(), 1);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());

  // Simulate the case where an iframe is created but the update to add the
  // element to the root frame's document has not yet come through.
  std::unique_ptr<TestBrowserAccessibilityDelegate> iframe_delegate =
      std::make_unique<TestBrowserAccessibilityDelegate>();
  iframe_delegate->is_root_frame_ = false;
  iframe_delegate->accelerated_widget_ = gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> iframe_manager(
      BrowserAccessibilityManager::Create(MakeAXTreeUpdate(root),
                                          iframe_delegate.get(),
                                          new BrowserAccessibilityFactory()));

  // The new frame is not a root frame, so the fragment root's lone child should
  // still be the same as before.
  EXPECT_EQ(fragment_root->GetChildCount(), 1);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());
}

}  // namespace content
