// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NinePieceImagePainter_h
#define NinePieceImagePainter_h

#include "platform/heap/Heap.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace blink {

class ComputedStyle;
class GraphicsContext;
class LayoutBoxModelObject;
class LayoutRect;
class NinePieceImage;

class NinePieceImagePainter {
  STACK_ALLOCATED();

 public:
  NinePieceImagePainter() {}

  bool Paint(GraphicsContext&,
             const LayoutBoxModelObject&,
             const LayoutRect&,
             const ComputedStyle&,
             const NinePieceImage&,
             SkBlendMode) const;
};

}  // namespace blink

#endif  // NinePieceImagePainter_h
