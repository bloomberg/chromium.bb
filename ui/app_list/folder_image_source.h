// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_FOLDER_IMAGE_SOURCE_H_
#define UI_APP_LIST_APP_LIST_FOLDER_IMAGE_SOURCE_H_

#include <vector>

#include "ui/gfx/image/canvas_image_source.h"

namespace gfx {
class Canvas;
class ImageSkia;
class Rect;
class Size;
}

namespace app_list {

// Generates the folder icon with the top 4 child item icons laid in 2x2 tile.
class FolderImageSource : public gfx::CanvasImageSource {
 public:
  typedef std::vector<gfx::ImageSkia> Icons;

  FolderImageSource(const Icons& icons, const gfx::Size& size);
  ~FolderImageSource() override;

  // Gets the size of a small app icon inside the folder icon.
  static gfx::Size ItemIconSize();

  // Calculates the top item icons' bounds inside |folder_icon_bounds|.
  // Returns the bounds of top item icons in sequence of top left, top right,
  // bottom left, bottom right.
  static std::vector<gfx::Rect> GetTopIconsBounds(
      const gfx::Rect& folder_icon_bounds);

 private:
  void DrawIcon(gfx::Canvas* canvas,
                const gfx::ImageSkia& icon,
                const gfx::Size icon_size,
                int x,
                int y);

  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override;

  Icons icons_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(FolderImageSource);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_
