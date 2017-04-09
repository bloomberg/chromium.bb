// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimCanvas_h
#define SimCanvas_h

#include "platform/graphics/Color.h"
#include "platform/wtf/Vector.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace blink {

class SimCanvas : public SkCanvas {
 public:
  SimCanvas(int width, int height);

  enum CommandType {
    kRect,
    kText,
    kImage,
    kShape,
  };

  // TODO(esprehn): Ideally we'd put the text in here too, but SkTextBlob
  // has no way to get the text back out so we can't assert about drawn text.
  struct Command {
    CommandType type;
    RGBA32 color;
  };

  const Vector<Command>& Commands() const { return commands_; }

  // Rect
  void onDrawRect(const SkRect&, const SkPaint&) override;

  // Shape
  void onDrawOval(const SkRect&, const SkPaint&) override;
  void onDrawRRect(const SkRRect&, const SkPaint&) override;
  void onDrawPath(const SkPath&, const SkPaint&) override;

  // Image
  void onDrawImage(const SkImage*, SkScalar, SkScalar, const SkPaint*) override;
  void onDrawImageRect(const SkImage*,
                       const SkRect* src,
                       const SkRect& dst,
                       const SkPaint*,
                       SrcRectConstraint) override;

  // Text
  void onDrawText(const void* text,
                  size_t byte_length,
                  SkScalar x,
                  SkScalar y,
                  const SkPaint&) override;
  void onDrawPosText(const void* text,
                     size_t byte_length,
                     const SkPoint pos[],
                     const SkPaint&) override;
  void onDrawPosTextH(const void* text,
                      size_t byte_length,
                      const SkScalar xpos[],
                      SkScalar const_y,
                      const SkPaint&) override;
  void onDrawTextOnPath(const void* text,
                        size_t byte_length,
                        const SkPath&,
                        const SkMatrix*,
                        const SkPaint&) override;
  void onDrawTextBlob(const SkTextBlob*,
                      SkScalar x,
                      SkScalar y,
                      const SkPaint&) override;

 private:
  void AddCommand(CommandType, RGBA32 = 0);

  Vector<Command> commands_;
};

}  // namespace blink

#endif
