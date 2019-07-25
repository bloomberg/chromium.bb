// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/detected_text.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/modules/imagecapture/point_2d.h"

namespace blink {

DetectedText::DetectedText(String raw_value,
                           DOMRectReadOnly* bounding_box,
                           HeapVector<Member<Point2D>> corner_points)
    : raw_value_(raw_value),
      bounding_box_(bounding_box),
      corner_points_(corner_points) {}

ScriptValue DetectedText::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddString("rawValue", rawValue());
  result.Add("boundingBox", boundingBox()->toJSONForBinding(script_state));
  Vector<ScriptValue> corner_points;
  for (const auto& corner_point : corner_points_) {
    V8ObjectBuilder builder(script_state);
    builder.AddNumber("x", corner_point->x());
    builder.AddNumber("y", corner_point->y());
    corner_points.push_back(builder.GetScriptValue());
  }
  result.Add("cornerPoints", corner_points);
  return result.GetScriptValue();
}

void DetectedText::Trace(blink::Visitor* visitor) {
  visitor->Trace(bounding_box_);
  visitor->Trace(corner_points_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
