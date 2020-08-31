// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_BOTTOM_STATUS_INDICATOR_H_
#define ASH_LOGIN_UI_BOTTOM_STATUS_INDICATOR_H_

#include "ash/style/ash_color_provider.h"
#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class BottomStatusIndicator : public views::View {
 public:
  enum class ContentType {
    kNone,
    kAdbSideLoadingEnabled,
  };

  BottomStatusIndicator();

  BottomStatusIndicator(const BottomStatusIndicator&) = delete;
  BottomStatusIndicator& operator=(const BottomStatusIndicator&) = delete;
  ~BottomStatusIndicator() override = default;

  void SetText(const base::string16& text, SkColor color);
  void SetIcon(const gfx::VectorIcon& vector_icon,
               AshColorProvider::ContentLayerType type);

  void set_content_type(ContentType content_type) {
    content_type_ = content_type;
  }
  ContentType content_type() const { return content_type_; }

 private:
  views::Label* label_ = nullptr;
  views::ImageView* icon_ = nullptr;

  ContentType content_type_ = ContentType::kNone;
};

}  // namespace ash
#endif  // ASH_LOGIN_UI_BOTTOM_STATUS_INDICATOR_H_
