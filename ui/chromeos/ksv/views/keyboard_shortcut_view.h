// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_KSV_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_
#define UI_CHROMEOS_KSV_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class TabbedPane;
class Widget;
}  // namespace views

namespace keyboard_shortcut_viewer {

class KeyboardShortcutItemView;
enum class ShortcutCategory;

// The UI container for Ash and Chrome keyboard shortcuts.
class KeyboardShortcutView : public views::WidgetDelegateView {
 public:
  ~KeyboardShortcutView() override;

  // Shows the Keyboard Shortcut Viewer window, or re-activates an existing one.
  static views::Widget* Show(gfx::NativeWindow context);

 private:
  friend class KeyboardShortcutViewTest;

  KeyboardShortcutView();

  void InitViews();

  static KeyboardShortcutView* GetInstanceForTests();
  int GetCategoryNumberForTests() const;
  int GetTabCountForTests() const;

  // views::WidgetDelegate:
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanResize() const override;
  // TODO(wutao): need to customize the frame view header based on UX specs.
  views::ClientView* CreateClientView(views::Widget* widget) override;

  // Owned by views hierarchy.
  views::TabbedPane* tabbed_pane_;

  // Views are owned by views hierarchy.
  std::map<ShortcutCategory, std::vector<KeyboardShortcutItemView*>>
      shortcut_views_by_category_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardShortcutView);
};

}  // namespace keyboard_shortcut_viewer

#endif  // UI_CHROMEOS_KSV_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_
