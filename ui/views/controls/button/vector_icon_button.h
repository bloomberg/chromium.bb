// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_VECTOR_ICON_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_VECTOR_ICON_BUTTON_H_

#include "base/macros.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
struct VectorIcon;
enum class VectorIconId;
}

namespace views {

class VectorIconButtonDelegate;

// A button that has an image and no text, with the image defined by a vector
// icon identifier.
class VIEWS_EXPORT VectorIconButton : public views::ImageButton {
 public:
  explicit VectorIconButton(VectorIconButtonDelegate* delegate);
  ~VectorIconButton() override;

  // Sets the icon to display and provides a callback which should return the
  // text color from which to derive this icon's color. The one that takes an ID
  // is deprecated and should be removed when all vector icons are identified by
  // VectorIcon structs.
  void SetIcon(gfx::VectorIconId id);
  void SetIcon(const gfx::VectorIcon& icon);

  // views::ImageButton:
  void OnThemeChanged() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

 private:
  // Performs the work common to both SetIcon() variants.
  void OnSetIcon();

  // Called when something may have affected the button's images or colors.
  void UpdateImagesAndColors();

  VectorIconButtonDelegate* delegate_;
  // TODO(estade): remove |id_| in favor of |icon_| once all callers have been
  // updated.
  gfx::VectorIconId id_;
  const gfx::VectorIcon* icon_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VectorIconButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_VECTOR_ICON_BUTTON_H_
