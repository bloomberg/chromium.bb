// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimCanvas.h"

#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"

namespace blink {

static int s_depth = 0;

class DrawScope {
 public:
  DrawScope() { ++s_depth; }
  ~DrawScope() { --s_depth; }
};

SimCanvas::SimCanvas(int width, int height) : SkCanvas(width, height) {}

void SimCanvas::addCommand(CommandType type, RGBA32 color) {
  if (s_depth > 1)
    return;
  Command command = {type, color};
  m_commands.push_back(command);
}

void SimCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint) {
  DrawScope scope;
  addCommand(CommandType::Rect, paint.getColor());
  SkCanvas::onDrawRect(rect, paint);
}

void SimCanvas::onDrawOval(const SkRect& oval, const SkPaint& paint) {
  DrawScope scope;
  addCommand(CommandType::Shape, paint.getColor());
  SkCanvas::onDrawOval(oval, paint);
}

void SimCanvas::onDrawRRect(const SkRRect& rrect, const SkPaint& paint) {
  DrawScope scope;
  addCommand(CommandType::Shape, paint.getColor());
  SkCanvas::onDrawRRect(rrect, paint);
}

void SimCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) {
  DrawScope scope;
  addCommand(CommandType::Shape, paint.getColor());
  SkCanvas::onDrawPath(path, paint);
}

void SimCanvas::onDrawImage(const SkImage* image,
                            SkScalar left,
                            SkScalar top,
                            const SkPaint* paint) {
  DrawScope scope;
  addCommand(CommandType::Image);
  SkCanvas::onDrawImage(image, left, top, paint);
}

void SimCanvas::onDrawImageRect(const SkImage* image,
                                const SkRect* src,
                                const SkRect& dst,
                                const SkPaint* paint,
                                SrcRectConstraint constraint) {
  DrawScope scope;
  addCommand(CommandType::Image);
  SkCanvas::onDrawImageRect(image, src, dst, paint, constraint);
}

void SimCanvas::onDrawText(const void* text,
                           size_t byteLength,
                           SkScalar x,
                           SkScalar y,
                           const SkPaint& paint) {
  DrawScope scope;
  addCommand(CommandType::Text, paint.getColor());
  SkCanvas::onDrawText(text, byteLength, x, y, paint);
}

void SimCanvas::onDrawPosText(const void* text,
                              size_t byteLength,
                              const SkPoint pos[],
                              const SkPaint& paint) {
  DrawScope scope;
  addCommand(CommandType::Text, paint.getColor());
  SkCanvas::onDrawPosText(text, byteLength, pos, paint);
}

void SimCanvas::onDrawPosTextH(const void* text,
                               size_t byteLength,
                               const SkScalar xpos[],
                               SkScalar constY,
                               const SkPaint& paint) {
  DrawScope scope;
  addCommand(CommandType::Text, paint.getColor());
  SkCanvas::onDrawPosTextH(text, byteLength, xpos, constY, paint);
}

void SimCanvas::onDrawTextOnPath(const void* text,
                                 size_t byteLength,
                                 const SkPath& path,
                                 const SkMatrix* matrix,
                                 const SkPaint& paint) {
  DrawScope scope;
  addCommand(CommandType::Text, paint.getColor());
  SkCanvas::onDrawTextOnPath(text, byteLength, path, matrix, paint);
}

void SimCanvas::onDrawTextBlob(const SkTextBlob* blob,
                               SkScalar x,
                               SkScalar y,
                               const SkPaint& paint) {
  DrawScope scope;
  addCommand(CommandType::Text, paint.getColor());
  SkCanvas::onDrawTextBlob(blob, x, y, paint);
}

}  // namespace blink
