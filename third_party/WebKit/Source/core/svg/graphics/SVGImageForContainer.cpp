/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/svg/graphics/SVGImageForContainer.h"

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/wtf/PassRefPtr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

IntSize SVGImageForContainer::Size() const {
  FloatSize scaled_container_size(container_size_);
  scaled_container_size.Scale(zoom_);
  return RoundedIntSize(scaled_container_size);
}

void SVGImageForContainer::Draw(PaintCanvas* canvas,
                                const PaintFlags& flags,
                                const FloatRect& dst_rect,
                                const FloatRect& src_rect,
                                RespectImageOrientationEnum,
                                ImageClampingMode) {
  image_->DrawForContainer(canvas, flags, container_size_, zoom_, dst_rect,
                           src_rect, url_);
}

void SVGImageForContainer::DrawPattern(GraphicsContext& context,
                                       const FloatRect& src_rect,
                                       const FloatSize& scale,
                                       const FloatPoint& phase,
                                       SkBlendMode op,
                                       const FloatRect& dst_rect,
                                       const FloatSize& repeat_spacing) {
  image_->DrawPatternForContainer(context, container_size_, zoom_, src_rect,
                                  scale, phase, op, dst_rect, repeat_spacing,
                                  url_);
}

bool SVGImageForContainer::ApplyShader(PaintFlags& flags,
                                       const SkMatrix& local_matrix) {
  return image_->ApplyShaderForContainer(container_size_, zoom_, url_, flags,
                                         local_matrix);
}

void SVGImageForContainer::PopulateImageForCurrentFrame(
    PaintImageBuilder& builder) {
  image_->PopulatePaintRecordForCurrentFrameForContainer(builder, url_, Size());
}

}  // namespace blink
