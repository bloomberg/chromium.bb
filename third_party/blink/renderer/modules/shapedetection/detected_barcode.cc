// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/detected_barcode.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/modules/imagecapture/point_2d.h"

namespace blink {

// static
String DetectedBarcode::BarcodeFormatToString(
    const shape_detection::mojom::BarcodeFormat format) {
  switch (format) {
    case shape_detection::mojom::BarcodeFormat::AZTEC:
      return "aztec";
    case shape_detection::mojom::BarcodeFormat::CODE_128:
      return "code_128";
    case shape_detection::mojom::BarcodeFormat::CODE_39:
      return "code_39";
    case shape_detection::mojom::BarcodeFormat::CODE_93:
      return "code_93";
    case shape_detection::mojom::BarcodeFormat::CODABAR:
      return "codabar";
    case shape_detection::mojom::BarcodeFormat::DATA_MATRIX:
      return "data_matrix";
    case shape_detection::mojom::BarcodeFormat::EAN_13:
      return "ean_13";
    case shape_detection::mojom::BarcodeFormat::EAN_8:
      return "ean_8";
    case shape_detection::mojom::BarcodeFormat::ITF:
      return "itf";
    case shape_detection::mojom::BarcodeFormat::PDF417:
      return "pdf417";
    case shape_detection::mojom::BarcodeFormat::QR_CODE:
      return "qr_code";
    case shape_detection::mojom::BarcodeFormat::UNKNOWN:
      return "unknown";
    case shape_detection::mojom::BarcodeFormat::UPC_A:
      return "upc_a";
    case shape_detection::mojom::BarcodeFormat::UPC_E:
      return "upc_e";
  }
}

DetectedBarcode::DetectedBarcode(String raw_value,
                                 DOMRectReadOnly* bounding_box,
                                 shape_detection::mojom::BarcodeFormat format,
                                 HeapVector<Member<Point2D>> corner_points)
    : raw_value_(raw_value),
      bounding_box_(bounding_box),
      format_(format),
      corner_points_(corner_points) {}

ScriptValue DetectedBarcode::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddString("rawValue", rawValue());
  result.Add("boundingBox", boundingBox()->toJSONForBinding(script_state));
  result.AddString("format", format());
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

void DetectedBarcode::Trace(blink::Visitor* visitor) {
  visitor->Trace(bounding_box_);
  visitor->Trace(corner_points_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
