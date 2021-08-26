// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_util.h"

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

bool IsUnhandledUnmodifiedEvent(const ui::KeyEvent& event) {
  if (event.handled() || event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (event.IsShiftDown() || event.IsControlDown() || event.IsAltDown())
    return false;

  return true;
}

bool IsUnhandledLeftRightKeyEvent(const ui::KeyEvent& event) {
  if (!IsUnhandledUnmodifiedEvent(event))
    return false;

  return event.key_code() == ui::VKEY_LEFT ||
         event.key_code() == ui::VKEY_RIGHT;
}

bool IsUnhandledUpDownKeyEvent(const ui::KeyEvent& event) {
  if (!IsUnhandledUnmodifiedEvent(event))
    return false;

  return event.key_code() == ui::VKEY_UP || event.key_code() == ui::VKEY_DOWN;
}

bool IsUnhandledArrowKeyEvent(const ui::KeyEvent& event) {
  if (!IsUnhandledUnmodifiedEvent(event))
    return false;

  return IsArrowKeyEvent(event);
}

bool IsArrowKeyEvent(const ui::KeyEvent& event) {
  return IsArrowKey(event.key_code());
}

bool IsArrowKey(const ui::KeyboardCode& key_code) {
  return key_code == ui::VKEY_DOWN || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_LEFT || key_code == ui::VKEY_UP;
}

bool IsFolderItem(AppListItem* item) {
  return item->GetItemType() == AppListFolderItem::kItemType;
}

bool LeftRightKeyEventShouldExitText(views::Textfield* textfield,
                                     const ui::KeyEvent& key_event) {
  DCHECK(IsUnhandledLeftRightKeyEvent(key_event));

  if (textfield->GetText().empty())
    return true;

  if (textfield->HasSelection())
    return false;

  if (textfield->GetCursorPosition() != 0 &&
      textfield->GetCursorPosition() != textfield->GetText().length()) {
    return false;
  }

  // For RTL language, the beginning position of the cursor will be at the right
  // side and it grows towards left as we are typing.
  const bool text_rtl =
      textfield->GetTextDirection() == base::i18n::RIGHT_TO_LEFT;
  const bool cursor_at_beginning = textfield->GetCursorPosition() == 0;
  const bool move_cursor_reverse =
      (text_rtl && key_event.key_code() == ui::VKEY_RIGHT) ||
      (!text_rtl && key_event.key_code() == ui::VKEY_LEFT);

  if ((cursor_at_beginning && !move_cursor_reverse) ||
      (!cursor_at_beginning && move_cursor_reverse)) {
    // Cursor is at either the beginning or the end of the textfield, and it
    // will move inward.
    return false;
  }

  return true;
}

bool ProcessLeftRightKeyTraversalForTextfield(views::Textfield* textfield,
                                              const ui::KeyEvent& key_event) {
  DCHECK(IsUnhandledLeftRightKeyEvent(key_event));

  if (!LeftRightKeyEventShouldExitText(textfield, key_event))
    return false;

  const bool move_focus_reverse = base::i18n::IsRTL()
                                      ? key_event.key_code() == ui::VKEY_RIGHT
                                      : key_event.key_code() == ui::VKEY_LEFT;

  // Move focus outside the textfield.
  textfield->GetFocusManager()->AdvanceFocus(move_focus_reverse);
  return true;
}

gfx::ImageSkia CreateIconWithCircleBackground(const gfx::ImageSkia& icon,
                                              SkColor background_color) {
  DCHECK_EQ(icon.width(), icon.height());
  // TODO(crbug.com/1185943): We should not be passing in hardcoded
  // `background_color`s here. Callers should be updated to use the appropriate
  // color from the NativeTheme or AshColorProvider.
  return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      icon.width() / 2, background_color, icon);
}

void PaintFocusBar(gfx::Canvas* canvas,
                   const gfx::Point content_origin,
                   const int height) {
  SkPath path;
  gfx::Rect focus_bar_bounds(content_origin.x() - kFocusBarThickness,
                             content_origin.y(), kFocusBarThickness * 2,
                             height);
  path.addRRect(SkRRect::MakeRectXY(RectToSkRect(focus_bar_bounds),
                                    kFocusBarThickness, kFocusBarThickness));
  canvas->ClipPath(path, true);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AppListColorProvider::Get()->GetFocusRingColor());
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kFocusBarThickness);
  gfx::Point top_point = content_origin;
  gfx::Point bottom_point = content_origin + gfx::Vector2d(0, height);
  canvas->DrawLine(top_point, bottom_point, flags);
}

}  // namespace ash
