// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_INFO_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_INFO_VIEW_H_

#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class Transform;
}

namespace views {
class Label;
}  // namespace views

namespace ash {

class GlanceableInfoView;

class ASH_EXPORT AmbientInfoView : public views::View {
 public:
  METADATA_HEADER(AmbientInfoView);

  explicit AmbientInfoView(AmbientViewDelegate* delegate);
  AmbientInfoView(const AmbientInfoView&) = delete;
  AmbientInfoView& operator=(AmbientInfoView&) = delete;
  ~AmbientInfoView() override;

  // views::View
  void OnThemeChanged() override;

  void UpdateImageDetails(const std::u16string& details,
                          const std::u16string& related_details);

  void SetTextTransform(const gfx::Transform& transform);

 private:
  void InitLayout();

  // Owned by |AmbientController| and should always outlive |this|.
  AmbientViewDelegate* delegate_ = nullptr;

  GlanceableInfoView* glanceable_info_view_ = nullptr;
  views::Label* details_label_ = nullptr;
  views::Label* related_details_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_INFO_VIEW_H_
