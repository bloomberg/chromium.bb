// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_TEXTFIELD_H_
#define ASH_WM_DESKS_DESKS_TEXTFIELD_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_highlightable_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

// Defines a textfield styled so when it's not focused, it looks like a normal
// label. It can be highlighted and activated by the
// `OverviewHighlightController`. It provides an API to elide long names.
// TODO(minch): Unify this to ash/style.
class ASH_EXPORT DesksTextfield : public views::Textfield,
                                  public OverviewHighlightableView {
 public:
  METADATA_HEADER(DesksTextfield);

  DesksTextfield();
  DesksTextfield(const DesksTextfield&) = delete;
  DesksTextfield& operator=(const DesksTextfield&) = delete;
  ~DesksTextfield() override;

  // The max number of characters (UTF-16) allowed for the textfield.
  static constexpr size_t kMaxLength = 300;

  // Use the potential max size of `this` to calculate elision, not its current
  // size to avoid eliding names that don't need to be.
  virtual void SetTextAndElideIfNeeded(const std::u16string& text);

  // If this view has focus, make the view's border visible and change
  // background to its active color. If it doesn't have focus, hide the view's
  // border and change background to its default color.
  void UpdateViewAppearance();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void SetBorder(std::unique_ptr<views::Border> b) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;

  // OverviewHighlightableView:
  views::View* GetView() override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;
  void MaybeSwapHighlightedView(bool right) override;
  void OnViewHighlighted() override;
  void OnViewUnhighlighted() override;

 protected:
  // Full text without being elided.
  std::u16string full_text_;

 private:
  void UpdateFocusRingState();

  // Returns the background color for this view based on whether it has focus
  // and if the mouse is entering/exiting the view.
  SkColor GetBackgroundColor() const;
};

BEGIN_VIEW_BUILDER(/* no export */, DesksTextfield, views::Textfield)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTextfield)

#endif  // ASH_WM_DESKS_DESKS_TEXTFIELD_H_
