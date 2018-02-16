// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_KSV_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_
#define UI_CHROMEOS_KSV_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/chromeos/search_box/search_box_view_delegate.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class TabbedPane;
class Widget;
}  // namespace views

namespace keyboard_shortcut_viewer {

class KeyboardShortcutItemView;
class KSVSearchBoxView;
enum class ShortcutCategory;

// The UI container for Ash and Chrome keyboard shortcuts.
class KeyboardShortcutView : public views::WidgetDelegateView,
                             public search_box::SearchBoxViewDelegate {
 public:
  ~KeyboardShortcutView() override;

  // Shows the Keyboard Shortcut Viewer window, or re-activates an existing one.
  static views::Widget* Show(gfx::NativeWindow context);

  // views::View:
  void Layout() override;

  // search_box::SearchBoxViewDelegate:
  void QueryChanged(search_box::SearchBoxViewBase* sender) override;
  void BackButtonPressed() override;
  void ActiveChanged(search_box::SearchBoxViewBase* sender) override;

 private:
  friend class KeyboardShortcutViewTest;

  KeyboardShortcutView();

  void InitViews();

  void RequestFocusForActiveTab();

  static KeyboardShortcutView* GetInstanceForTesting();
  int GetCategoryNumberForTesting() const;
  int GetTabCountForTesting() const;
  const std::vector<KeyboardShortcutItemView*>& GetShortcutViewsForTesting() {
    return shortcut_views_;
  }

  // views::WidgetDelegate:
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanResize() const override;
  // TODO(wutao): need to customize the frame view header based on UX specs.
  views::ClientView* CreateClientView(views::Widget* widget) override;

  // Owned by views hierarchy.
  views::TabbedPane* tabbed_pane_;
  // SearchBoxViewBase is a WidgetDelegateView, which owns itself and cannot be
  // deleted from the views hierarchy automatically.
  std::unique_ptr<KSVSearchBoxView> search_box_view_;
  views::View* search_results_container_;

  // Contains all the shortcut item views from all categories. This list is used
  // for searching. The views are owned by the Views hierarchy.
  std::vector<KeyboardShortcutItemView*> shortcut_views_;

  // Two illustrations to indicate the two search states: start searching and no
  // result found. Since these two views need to be added and removed
  // frequently from the |search_results_container_|, they are not owned by view
  // hierarchy to avoid recreating them.
  std::unique_ptr<views::View> search_start_view_;
  std::unique_ptr<views::View> search_no_result_view_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardShortcutView);
};

}  // namespace keyboard_shortcut_viewer

#endif  // UI_CHROMEOS_KSV_VIEWS_KEYBOARD_SHORTCUT_VIEW_H_
