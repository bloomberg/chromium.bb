// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimCanvas_h
#define SimCanvas_h

#include "platform/graphics/Color.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "wtf/Vector.h"

namespace blink {

class SimCanvas : public PaintCanvas {
 public:
  SimCanvas(int width, int height);

  enum CommandType {
    Rect,
    Text,
    Image,
    Shape,
  };

  // TODO(esprehn): Ideally we'd put the text in here too, but SkTextBlob
  // has no way to get the text back out so we can't assert about drawn text.
  struct Command {
    CommandType type;
    RGBA32 color;
  };

  const Vector<Command>& commands() const { return m_commands; }

  // Rect
  void onDrawRect(const SkRect&, const PaintFlags&) override;

  // Shape
  void onDrawOval(const SkRect&, const PaintFlags&) override;
  void onDrawRRect(const SkRRect&, const PaintFlags&) override;
  void onDrawPath(const SkPath&, const PaintFlags&) override;

  // Image
  void onDrawImage(const SkImage*,
                   SkScalar,
                   SkScalar,
                   const PaintFlags*) override;
  void onDrawImageRect(const SkImage*,
                       const SkRect* src,
                       const SkRect& dst,
                       const PaintFlags*,
                       SkCanvas::SrcRectConstraint) override;

  // Text
  void onDrawText(const void* text,
                  size_t byteLength,
                  SkScalar x,
                  SkScalar y,
                  const PaintFlags&) override;
  void onDrawPosText(const void* text,
                     size_t byteLength,
                     const SkPoint pos[],
                     const PaintFlags&) override;
  void onDrawPosTextH(const void* text,
                      size_t byteLength,
                      const SkScalar xpos[],
                      SkScalar constY,
                      const PaintFlags&) override;

  void onDrawTextOnPath(const void* text,
                        size_t byteLength,
                        const SkPath&,
                        const SkMatrix*,
                        const PaintFlags&) override;
  void onDrawTextBlob(const SkTextBlob*,
                      SkScalar x,
                      SkScalar y,
                      const PaintFlags&) override;

 private:
  void addCommand(CommandType, RGBA32 = 0);

  Vector<Command> m_commands;
};

}  // namespace blink

#endif
