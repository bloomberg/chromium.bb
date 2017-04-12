// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Transform3DRecorder_h
#define Transform3DRecorder_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class FloatPoint3D;
class GraphicsContext;
class TransformationMatrix;

class Transform3DRecorder {
  STACK_ALLOCATED();

 public:
  Transform3DRecorder(GraphicsContext&,
                      const DisplayItemClient&,
                      DisplayItem::Type,
                      const TransformationMatrix&,
                      const FloatPoint3D& transform_origin);
  ~Transform3DRecorder();

 private:
  GraphicsContext& context_;
  const DisplayItemClient& client_;
  DisplayItem::Type type_;
  bool skip_recording_for_identity_transform_;
};

}  // namespace blink

#endif  // Transform3DRecorder_h
