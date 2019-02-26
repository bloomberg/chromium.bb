// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_VIEWS_UTIL_H_
#define ASH_ASSISTANT_UTIL_VIEWS_UTIL_H_

#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class ButtonListener;
class ImageButton;
}  // namespace views

namespace ash {
namespace assistant {
namespace util {

// Creates an ImageButton with the default Assistant styles.
views::ImageButton* CreateImageButton(views::ButtonListener* listener,
                                      const gfx::VectorIcon& icon,
                                      int size_in_dip,
                                      int icon_size_in_dip,
                                      int accessible_name_id,
                                      SkColor icon_color = gfx::kGoogleGrey600);

}  // namespace util
}  // namespace assistant
}  // namespace ash
#endif  // ASH_ASSISTANT_UTIL_VIEWS_UTIL_H_
