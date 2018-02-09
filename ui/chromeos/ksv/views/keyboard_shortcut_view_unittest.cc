// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/views/keyboard_shortcut_view.h"

#include <set>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/ksv/keyboard_shortcut_viewer_metadata.h"
#include "ui/chromeos/ksv/views/keyboard_shortcut_item_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace keyboard_shortcut_viewer {

class KeyboardShortcutViewTest : public views::ViewsTestBase {
 public:
  KeyboardShortcutViewTest() = default;
  ~KeyboardShortcutViewTest() override = default;

 protected:
  int GetCategoryNumber() const {
    DCHECK(GetView());
    return GetView()->GetCategoryNumberForTesting();
  }

  int GetTabCount() const {
    DCHECK(GetView());
    return GetView()->GetTabCountForTesting();
  }

  const std::vector<KeyboardShortcutItemView*>& GetShortcutViews() {
    DCHECK(GetView());
    return GetView()->GetShortcutViewsForTesting();
  }

 private:
  KeyboardShortcutView* GetView() const {
    return KeyboardShortcutView::GetInstanceForTesting();
  }

  DISALLOW_COPY_AND_ASSIGN(KeyboardShortcutViewTest);
};

// Shows and closes the widget for KeyboardShortcutViewer.
TEST_F(KeyboardShortcutViewTest, ShowAndClose) {
  // Showing the widget.
  views::Widget* widget = KeyboardShortcutView::Show(GetContext());
  EXPECT_TRUE(widget);

  // Cleaning up.
  widget->CloseNow();
}

// Test that the number of side tabs equals to the number of categories.
TEST_F(KeyboardShortcutViewTest, SideTabsCount) {
  // Showing the widget.
  views::Widget* widget = KeyboardShortcutView::Show(GetContext());

  int category_number = 0;
  ShortcutCategory current_category = ShortcutCategory::kUnknown;
  for (auto* item_view : GetShortcutViews()) {
    const ShortcutCategory category = item_view->category();
    if (current_category != category) {
      DCHECK(current_category < category);
      ++category_number;
      current_category = category;
    }
  }
  EXPECT_EQ(GetTabCount(), category_number);

  // Cleaning up.
  widget->CloseNow();
}

}  // namespace keyboard_shortcut_viewer
