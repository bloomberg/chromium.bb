// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimCanvas.h"

#include "platform/graphics/paint/PaintFlags.h"
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

SimCanvas::SimCanvas(int width, int height) : PaintCanvas(width, height) {}

void SimCanvas::addCommand(CommandType type, RGBA32 color) {
  if (s_depth > 1)
    return;
  Command command = {type, color};
  m_commands.push_back(command);
}

void SimCanvas::onDrawRect(const SkRect& rect, const PaintFlags& paint) {
  DrawScope scope;
  addCommand(CommandType::Rect, paint.getColor());
  PaintCanvas::onDrawRect(rect, paint);
}

void SimCanvas::onDrawOval(const SkRect& oval, const PaintFlags& paint) {
  DrawScope scope;
  addCommand(CommandType::Shape, paint.getColor());
  PaintCanvas::onDrawOval(oval, paint);
}

void SimCanvas::onDrawRRect(const SkRRect& rrect, const PaintFlags& paint) {
  DrawScope scope;
  addCommand(CommandType::Shape, paint.getColor());
  PaintCanvas::onDrawRRect(rrect, paint);
}

void SimCanvas::onDrawPath(const SkPath& path, const PaintFlags& paint) {
  DrawScope scope;
  addCommand(CommandType::Shape, paint.getColor());
  PaintCanvas::onDrawPath(path, paint);
}

void SimCanvas::onDrawImage(const SkImage* image,
                            SkScalar left,
                            SkScalar top,
                            const PaintFlags* paint) {
  DrawScope scope;
  addCommand(CommandType::Image);
  PaintCanvas::onDrawImage(image, left, top, paint);
}

void SimCanvas::onDrawImageRect(const SkImage* image,
                                const SkRect* src,
                                const SkRect& dst,
                                const PaintFlags* paint,
                                SkCanvas::SrcRectConstraint constraint) {
  DrawScope scope;
  addCommand(CommandType::Image);
  PaintCanvas::onDrawImageRect(image, src, dst, paint, constraint);
}

void SimCanvas::onDrawText(const void* text,
                           size_t byteLength,
                           SkScalar x,
                           SkScalar y,
                           const PaintFlags& paint) {
  DrawScope scope;
  addCommand(CommandType::Text, paint.getColor());
  PaintCanvas::onDrawText(text, byteLength, x, y, paint);
}

void SimCanvas::onDrawPosText(const void* text,
                              size_t byteLength,
                              const SkPoint pos[],
                              const PaintFlags& paint) {
  DrawScope scope;
  addCommand(CommandType::Text, paint.getColor());
  PaintCanvas::onDrawPosText(text, byteLength, pos, paint);
}

void SimCanvas::onDrawPosTextH(const void* text,
                               size_t byteLength,
                               const SkScalar xpos[],
                               SkScalar constY,
                               const PaintFlags& paint) {
  DrawScope scope;
  addCommand(CommandType::Text, paint.getColor());
  PaintCanvas::onDrawPosTextH(text, byteLength, xpos, constY, paint);
}

void SimCanvas::onDrawTextOnPath(const void* text,
                                 size_t byteLength,
                                 const SkPath& path,
                                 const SkMatrix* matrix,
                                 const PaintFlags& paint) {
  DrawScope scope;
  addCommand(CommandType::Text, paint.getColor());
  PaintCanvas::onDrawTextOnPath(text, byteLength, path, matrix, paint);
}

void SimCanvas::onDrawTextBlob(const SkTextBlob* blob,
                               SkScalar x,
                               SkScalar y,
                               const PaintFlags& paint) {
  DrawScope scope;
  addCommand(CommandType::Text, paint.getColor());
  PaintCanvas::onDrawTextBlob(blob, x, y, paint);
}

}  // namespace blink
