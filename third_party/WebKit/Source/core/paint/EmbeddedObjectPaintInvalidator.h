// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EmbeddedObjectPaintInvalidator_h
#define EmbeddedObjectPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutEmbeddedObject;
struct PaintInvalidatorContext;

class EmbeddedObjectPaintInvalidator {
  STACK_ALLOCATED();

 public:
  EmbeddedObjectPaintInvalidator(const LayoutEmbeddedObject& embedded_object,
                                 const PaintInvalidatorContext& context)
      : embedded_object_(embedded_object), context_(context) {}

  PaintInvalidationReason InvalidatePaint();

 private:
  const LayoutEmbeddedObject& embedded_object_;
  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif
