// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_BACK_BUTTON_H_
#define ASH_SHELF_BACK_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_control_button.h"
#include "base/macros.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// The back button shown on the shelf when tablet mode is enabled. Its opacity
// and visiblity are handled by its parent, ShelfView, to ensure the fade
// in/out of the icon matches the movement of ShelfView's items.
class ASH_EXPORT BackButton : public ShelfControlButton {
 public:
  BackButton();
  ~BackButton() override;

 protected:
  // views::ImageButton:
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // Generate and send a VKEY_BROWSER_BACK key event when the back button
  // is pressed.
  void GenerateAndSendBackEvent(const ui::EventType& original_event_type);

  DISALLOW_COPY_AND_ASSIGN(BackButton);
};

}  // namespace ash

#endif  // ASH_SHELF_BACK_BUTTON_H_
