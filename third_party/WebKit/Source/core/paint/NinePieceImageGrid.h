// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NinePieceImageGrid_h
#define NinePieceImageGrid_h

#include "core/CoreExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Image.h"
#include "platform/heap/Heap.h"

namespace blink {

class IntRectOutsets;
class NinePieceImage;

enum NinePiece {
  kMinPiece = 0,
  kTopLeftPiece = kMinPiece,
  kBottomLeftPiece,
  kLeftPiece,
  kTopRightPiece,
  kBottomRightPiece,
  kRightPiece,
  kTopPiece,
  kBottomPiece,
  kMiddlePiece,
  kMaxPiece
};

inline NinePiece& operator++(NinePiece& piece) {
  piece = static_cast<NinePiece>(static_cast<int>(piece) + 1);
  return piece;
}

// The NinePieceImageGrid class is responsible for computing drawing information
// for the nine piece image.
//
// http://dev.w3.org/csswg/css-backgrounds-3/#border-image-process
//
// Given an image, a set of slices and a border area:
//
//       |         |
//   +---+---------+---+          +------------------+
//   | 1 |    7    | 4 |          |      border      |
// --+---+---------+---+---       |  +------------+  |
//   |   |         |   |          |  |            |  |
//   | 3 |    9    | 6 |          |  |    css     |  |
//   |   |  image  |   |          |  |    box     |  |
//   |   |         |   |          |  |            |  |
// --+---+---------+---+---       |  |            |  |
//   | 2 |    8    | 5 |          |  +------------+  |
//   +---+---------+---+          |                  |
//       |         |              +------------------+
//
// it generates drawing information for the nine border pieces.
class CORE_EXPORT NinePieceImageGrid {
  STACK_ALLOCATED();

 public:
  NinePieceImageGrid(const NinePieceImage&,
                     IntSize image_size,
                     IntRect border_image_area,
                     const IntRectOutsets& border_widths);

  struct CORE_EXPORT NinePieceDrawInfo {
    STACK_ALLOCATED();
    bool is_drawable;
    bool is_corner_piece;
    FloatRect destination;
    FloatRect source;

    // tileScale and tileRule are only useful for non-corners, i.e. edge and
    // center pieces.
    FloatSize tile_scale;
    struct {
      Image::TileRule horizontal;
      Image::TileRule vertical;
    } tile_rule;
  };
  NinePieceDrawInfo GetNinePieceDrawInfo(NinePiece, float) const;

  struct Edge {
    DISALLOW_NEW();
    bool IsDrawable() const { return slice > 0 && width > 0; }
    float Scale() const { return IsDrawable() ? (float)width / slice : 1; }
    int slice;
    int width;
  };

 private:
  void SetDrawInfoCorner(NinePieceDrawInfo&, NinePiece) const;
  void SetDrawInfoEdge(NinePieceDrawInfo&, NinePiece) const;
  void SetDrawInfoMiddle(NinePieceDrawInfo&) const;

  IntRect border_image_area_;
  IntSize image_size_;
  Image::TileRule horizontal_tile_rule_;
  Image::TileRule vertical_tile_rule_;
  bool fill_;

  Edge top_;
  Edge right_;
  Edge bottom_;
  Edge left_;
};

}  // namespace blink

#endif  // NinePieceImageGrid_h
