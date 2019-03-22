// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/views_util.h"

#include <memory>

#include "ash/assistant/ui/base/assistant_button.h"
#include "base/macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {
namespace assistant {
namespace util {

views::ImageButton* CreateImageButton(views::ButtonListener* listener,
                                      const gfx::VectorIcon& icon,
                                      int size_in_dip,
                                      int icon_size_in_dip,
                                      int accessible_name_id,
                                      SkColor icon_color) {
  auto* button = new AssistantButton(listener);
  button->SetAccessibleName(l10n_util::GetStringUTF16(accessible_name_id));
  button->SetImage(views::Button::STATE_NORMAL,
                   gfx::CreateVectorIcon(icon, icon_size_in_dip, icon_color));
  button->SetPreferredSize(gfx::Size(size_in_dip, size_in_dip));
  return button;
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
