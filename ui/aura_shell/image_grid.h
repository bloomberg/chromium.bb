// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_IMAGE_GRID_H_
#define UI_AURA_SHELL_IMAGE_GRID_H_
#pragma once

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_delegate.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace aura_shell {
namespace internal {

// An ImageGrid is a 3x3 array of ui::Layers, each containing an image.
//
// As the grid is resized, its images fill the requested space:
// - corner images are not scaled
// - top and bottom images are scaled horizontally
// - left and right images are scaled vertically
// - the center image is scaled in both directions
//
// If one of the non-center images is smaller than the largest images in its
// row or column, it will be aligned with the outside of the grid.  For
// example, given 4x4 top-left and top-right images and a 1x2 top images:
//
//   +--------+---------------------+--------+
//   |        |         top         |        |
//   | top-   +---------------------+  top-  +
//   | left   |                     | right  |
//   +----+---+                     +---+----+
//   |    |                             |    |
//   ...
//
// This may seem odd at first, but it lets ImageGrid be used to draw shadows
// with curved corners that extend inwards beyond a window's borders.  In the
// below example, the top-left corner image is overlaid on top of the window's
// top-left corner:
//
//   +---------+-----------------------
//   |    ..xxx|XXXXXXXXXXXXXXXXXX
//   |  .xXXXXX|XXXXXXXXXXXXXXXXXX_____
//   | .xXX    |                    ^ window's top edge
//   | .xXX    |
//   +---------+
//   | xXX|
//   | xXX|< window's left edge
//   | xXX|
//   ...
//
class AURA_SHELL_EXPORT ImageGrid {
 public:
  // Helper class for use by tests.
  class TestAPI {
   public:
    TestAPI(ImageGrid* grid) : grid_(grid) {}
    ui::Layer* top_left_layer() const { return grid_->top_left_layer_.get(); }
    ui::Layer* top_layer() const { return grid_->top_layer_.get(); }
    ui::Layer* top_right_layer() const { return grid_->top_right_layer_.get(); }
    ui::Layer* left_layer() const { return grid_->left_layer_.get(); }
    ui::Layer* center_layer() const { return grid_->center_layer_.get(); }
    ui::Layer* right_layer() const { return grid_->right_layer_.get(); }
    ui::Layer* bottom_left_layer() const {
      return grid_->bottom_left_layer_.get();
    }
    ui::Layer* bottom_layer() const { return grid_->bottom_layer_.get(); }
    ui::Layer* bottom_right_layer() const {
      return grid_->bottom_right_layer_.get();
    }

    // Returns |layer|'s bounds after applying the layer's current transform.
    gfx::Rect GetTransformedLayerBounds(const ui::Layer& layer);

   private:
    ImageGrid* grid_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  ImageGrid();
  ~ImageGrid();

  ui::Layer* layer() { return layer_.get(); }
  int top_image_height() const { return top_image_height_; }
  int bottom_image_height() const { return bottom_image_height_; }
  int left_image_width() const { return left_image_width_; }
  int right_image_width() const { return right_image_width_; }

  // Initializes the grid to display the passed-in images (any of which can be
  // NULL).  Ownership of the images remains with the caller.
  void Init(const gfx::Image* top_left_image,
            const gfx::Image* top_image,
            const gfx::Image* top_right_image,
            const gfx::Image* left_image,
            const gfx::Image* center_image,
            const gfx::Image* right_image,
            const gfx::Image* bottom_left_image,
            const gfx::Image* bottom_image,
            const gfx::Image* bottom_right_image);

  void SetSize(const gfx::Size& size);

 private:
  // Delegate responsible for painting a specific image on a layer.
  class ImagePainter : public ui::LayerDelegate {
   public:
    ImagePainter(const gfx::Image* image) : image_(image) {}
    virtual ~ImagePainter() {}

    // ui::LayerDelegate implementation:
    virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE;

   private:
    const gfx::Image* image_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(ImagePainter);
  };

  // Returns the dimensions of |image| if non-NULL or gfx::Size(0, 0) otherwise.
  static gfx::Size GetImageSize(const gfx::Image* image);

  // Initializes |layer_ptr| and |painter_ptr| to display |image|.
  // Also adds the passed-in layer to |layer_|.
  void InitImage(const gfx::Image* image,
                 scoped_ptr<ui::Layer>* layer_ptr,
                 scoped_ptr<ImagePainter>* painter_ptr);

  // Layer that contains all of the image layers.
  scoped_ptr<ui::Layer> layer_;

  // The grid's dimensions.
  gfx::Size size_;

  // Heights and widths of the images displayed by |top_layer_|,
  // |bottom_layer_|, |left_layer_|, and |right_layer_|.
  int top_image_height_;
  int bottom_image_height_;
  int left_image_width_;
  int right_image_width_;

  // Heights of the tallest images in the top and bottom rows and the widest
  // images in the left and right columns.
  int top_row_height_;
  int bottom_row_height_;
  int left_column_width_;
  int right_column_width_;

  // Layers used to display the various images.  Children of |layer_|.
  // Positions for which no images were supplied are NULL.
  scoped_ptr<ui::Layer> top_left_layer_;
  scoped_ptr<ui::Layer> top_layer_;
  scoped_ptr<ui::Layer> top_right_layer_;
  scoped_ptr<ui::Layer> left_layer_;
  scoped_ptr<ui::Layer> center_layer_;
  scoped_ptr<ui::Layer> right_layer_;
  scoped_ptr<ui::Layer> bottom_left_layer_;
  scoped_ptr<ui::Layer> bottom_layer_;
  scoped_ptr<ui::Layer> bottom_right_layer_;

  // Delegates responsible for painting the above layers.
  // Positions for which no images were supplied are NULL.
  scoped_ptr<ImagePainter> top_left_painter_;
  scoped_ptr<ImagePainter> top_painter_;
  scoped_ptr<ImagePainter> top_right_painter_;
  scoped_ptr<ImagePainter> left_painter_;
  scoped_ptr<ImagePainter> center_painter_;
  scoped_ptr<ImagePainter> right_painter_;
  scoped_ptr<ImagePainter> bottom_left_painter_;
  scoped_ptr<ImagePainter> bottom_painter_;
  scoped_ptr<ImagePainter> bottom_right_painter_;

  DISALLOW_COPY_AND_ASSIGN(ImageGrid);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_IMAGE_GRID_H_
