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
class ImageResourceObserver;
class Node;
class LayoutRect;
class NinePieceImage;
class Document;

class NinePieceImagePainter {
  STACK_ALLOCATED();

 public:
  static bool Paint(GraphicsContext&,
                    const ImageResourceObserver&,
                    const Document&,
                    Node*,
                    const LayoutRect&,
                    const ComputedStyle&,
                    const NinePieceImage&,
                    SkBlendMode = SkBlendMode::kSrcOver);

 private:
  NinePieceImagePainter() {}
};

}  // namespace blink

#endif  // NinePieceImagePainter_h
