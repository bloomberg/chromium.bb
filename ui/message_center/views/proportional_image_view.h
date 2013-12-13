// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_PROPORTIONAL_IMAGE_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_PROPORTIONAL_IMAGE_VIEW_H_

#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

namespace message_center {

// ProportionalImageViews center their images to preserve their proportion.
class ProportionalImageView : public views::View {
 public:
  ProportionalImageView(const gfx::ImageSkia& image);
  virtual ~ProportionalImageView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  gfx::Size GetImageSizeForWidth(int width);

  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(ProportionalImageView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_PROPORTIONAL_IMAGE_VIEW_H_
