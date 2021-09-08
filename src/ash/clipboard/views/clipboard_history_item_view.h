// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_

#include "ash/clipboard/clipboard_history_util.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class MenuItemView;
}  // namespace views

namespace ash {
class ClipboardHistoryDeleteButton;
class ClipboardHistoryItem;
class ClipboardHistoryMainButton;
class ClipboardHistoryResourceManager;

// The base class for menu items of the clipboard history menu.
class ClipboardHistoryItemView : public views::View {
 public:
  static std::unique_ptr<ClipboardHistoryItemView>
  CreateFromClipboardHistoryItem(
      const ClipboardHistoryItem& item,
      const ClipboardHistoryResourceManager* resource_manager,
      views::MenuItemView* container);

  ClipboardHistoryItemView(const ClipboardHistoryItemView& rhs) = delete;
  ClipboardHistoryItemView& operator=(const ClipboardHistoryItemView& rhs) =
      delete;
  ~ClipboardHistoryItemView() override;

  // Advances the pseudo focus (backward if reverse is true). Returns whether
  // the view still keeps the pseudo focus.
  bool AdvancePseudoFocus(bool reverse);

  void HandleDeleteButtonPressEvent(const ui::Event& event);

  void HandleMainButtonPressEvent(const ui::Event& event);

  // Initializes the menu item.
  void Init();

  // Attempts to handle the gesture event redirected from `main_button_`.
  void MaybeHandleGestureEventFromMainButton(ui::GestureEvent* event);

  // Called when the selection state has changed.
  void OnSelectionChanged();

  // Returns whether the highlight background should show.
  bool ShouldHighlight() const;

  // Called when the mouse click on descendants (such as the main button or
  // the delete button) gets canceled.
  void OnMouseClickOnDescendantCanceled();

  ClipboardHistoryUtil::Action action() const { return action_; }

 protected:
  // Used by subclasses to draw contents, such as text or bitmaps.
  class ContentsView : public views::View, public views::ViewTargeterDelegate {
   public:
    explicit ContentsView(ClipboardHistoryItemView* container);
    ContentsView(const ContentsView& rhs) = delete;
    ContentsView& operator=(const ContentsView& rhs) = delete;
    ~ContentsView() override;

    // Install DeleteButton on the contents view.
    void InstallDeleteButton();

    void OnHostPseudoFocusUpdated();

    ClipboardHistoryDeleteButton* delete_button() { return delete_button_; }
    const ClipboardHistoryDeleteButton* delete_button() const {
      return delete_button_;
    }

   protected:
    virtual ClipboardHistoryDeleteButton* CreateDeleteButton() = 0;

    ClipboardHistoryItemView* container() { return container_; }

   private:
    // views::View:
    const char* GetClassName() const override;

    // views::ViewTargeterDelegate:
    bool DoesIntersectRect(const views::View* target,
                           const gfx::Rect& rect) const override;

    // Owned by the view hierarchy.
    ClipboardHistoryDeleteButton* delete_button_ = nullptr;

    // The parent of ContentsView.
    ClipboardHistoryItemView* const container_;
  };

  ClipboardHistoryItemView(const ClipboardHistoryItem* clipboard_history_item,
                           views::MenuItemView* container);

  // Maybe record histograms after the button is pressed.
  void MaybeRecordButtonPressedHistogram() const;

  // Creates the contents view.
  virtual std::unique_ptr<ContentsView> CreateContentsView() = 0;

  // Returns the name of the accessible node.
  virtual std::u16string GetAccessibleName() const = 0;

  const ClipboardHistoryItem* clipboard_history_item() const {
    return clipboard_history_item_;
  }

 private:
  // Indicates the child under pseudo focus, i.e. the view responding to the
  // user actions on the menu item (like clicking the mouse or triggering an
  // accelerator). Note that the child under pseudo focus does not have view
  // focus. It is where "pseudo" comes from.
  // The enumeration types are arranged in the forward focus traversal order.
  enum PseudoFocus {
    // No child is under pseudo focus.
    kEmpty = 0,

    // The main button has pseudo focus.
    kMainButton = 1,

    // The delete button has pseudo focus.
    kDeleteButton = 2,

    // Marks the end. It should not be assigned to `pseudo_focus_`.
    kMaxValue = 3
  };

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* data) override;

  // Activates the menu item with the specified action and event flags.
  void Activate(ClipboardHistoryUtil::Action action, int event_flags);

  // Calculates the action type when `main_button_` is clicked.
  ClipboardHistoryUtil::Action CalculateActionForMainButtonClick() const;

  bool ShouldShowDeleteButton() const;

  // Called when receiving pseudo focus for the first time.
  void InitiatePseudoFocus(bool reverse);

  // Updates `pseudo_focus_` and children visibility.
  void SetPseudoFocus(PseudoFocus new_pseudo_focus);

  // Owned by ClipboardHistoryMenuModelAdapter.
  const ClipboardHistoryItem* const clipboard_history_item_;

  views::MenuItemView* const container_;

  ContentsView* contents_view_ = nullptr;

  ClipboardHistoryMainButton* main_button_ = nullptr;

  PseudoFocus pseudo_focus_ = PseudoFocus::kEmpty;

  // Indicates whether the menu item is under the gesture long press.
  bool under_gesture_long_press_ = false;

  // Indicates the action to take. It is set when the menu item is activated
  // through `main_button_` or the delete button.
  ClipboardHistoryUtil::Action action_ = ClipboardHistoryUtil::Action::kEmpty;

  base::CallbackListSubscription subscription_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_ITEM_VIEW_H_
