// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_DRAG_UTIL_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_DRAG_UTIL_H_

#include <vector>

namespace gfx {
class ImageSkia;
class Vector2d;
}  // namespace gfx

namespace ash {

class HoldingSpaceItemView;

namespace holding_space_util {

// Returns a drag image for the specified holding space item `views` and the
// offset with which to position it. The drag image consists of a stacked
// representation of the dragged items with the first item being stacked on top.
// Note that the drag image will paint at most two items with an overflow badge
// to represent the presence of additional drag items if necessary.
void CreateDragImage(const std::vector<const HoldingSpaceItemView*>& views,
                     gfx::ImageSkia* drag_image,
                     gfx::Vector2d* drag_offset);

}  // namespace holding_space_util
}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_DRAG_UTIL_H_
