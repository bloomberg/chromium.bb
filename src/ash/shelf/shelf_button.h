// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUTTON_H_
#define ASH_SHELF_SHELF_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"

namespace ash {
class InkDropButtonListener;
class ShelfView;

// Button used for items on the shelf.
class ASH_EXPORT ShelfButton : public views::Button {
 public:
  explicit ShelfButton(ShelfView* shelf_view);
  ~ShelfButton() override;

  // views::View
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnFocus() override;
  void OnBlur() override;

 protected:
  ShelfView* shelf_view() { return shelf_view_; }

  // views::Button
  void NotifyClick(const ui::Event& event) override;
  bool ShouldEnterPushedState(const ui::Event& event) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;

 private:
  // The shelf view hosting this button.
  ShelfView* shelf_view_;

  InkDropButtonListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(ShelfButton);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUTTON_H_
