// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_TARGET_BUTTON_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_TARGET_BUTTON_H_

#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace sharesheet {

// SharesheetTargetButton is owned by |sharesheet_bubble_view|. It represents
// a single target (either app or action) in the |sharesheet_bubble_view|. The
// target is comprised of an image (made from |icon| for apps or from
// |vector_icon| for actions), a |display_name| and an optional
// |secondary_display_name| below it. When pressed this button launches the
// associated target.
class SharesheetTargetButton : public views::Button {
 public:
  METADATA_HEADER(SharesheetTargetButton);

  SharesheetTargetButton(PressedCallback callback,
                         const std::u16string& display_name,
                         const std::u16string& secondary_display_name,
                         const absl::optional<gfx::ImageSkia> icon,
                         const gfx::VectorIcon* vector_icon);
  SharesheetTargetButton(const SharesheetTargetButton&) = delete;
  SharesheetTargetButton& operator=(const SharesheetTargetButton&) = delete;

 private:
  void SetLabelProperties(views::Label* label);
};

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_TARGET_BUTTON_H_
