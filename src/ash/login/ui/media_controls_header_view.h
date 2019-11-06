// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_MEDIA_CONTROLS_HEADER_VIEW_H_
#define ASH_LOGIN_UI_MEDIA_CONTROLS_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class ASH_EXPORT MediaControlsHeaderView : public views::View {
 public:
  MediaControlsHeaderView();
  ~MediaControlsHeaderView() override;

  void SetAppIcon(const gfx::ImageSkia& img);
  void SetAppName(const base::string16& name);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  const base::string16& app_name_for_testing() const;
  const views::ImageView* app_icon_for_testing() const;

 private:
  views::ImageView* app_icon_view_;
  views::Label* app_name_view_;

  DISALLOW_COPY_AND_ASSIGN(MediaControlsHeaderView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_MEDIA_CONTROLS_HEADER_VIEW_H_