// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shapedetection/detected_barcode.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"

namespace blink {

DetectedBarcode* DetectedBarcode::Create() {
  HeapVector<Member<Point2D>> empty_list;
  return MakeGarbageCollected<DetectedBarcode>(
      g_empty_string, DOMRectReadOnly::Create(0, 0, 0, 0),
      shape_detection::mojom::BarcodeFormat::UNKNOWN, empty_list);
}

DetectedBarcode* DetectedBarcode::Create(
    String raw_value,
    DOMRectReadOnly* bounding_box,
    shape_detection::mojom::BarcodeFormat format,
    HeapVector<Member<Point2D>> corner_points) {
  return MakeGarbageCollected<DetectedBarcode>(raw_value, bounding_box, format,
                                               corner_points);
}

// static
WebString DetectedBarcode::BarcodeFormatToString(
    const shape_detection::mojom::BarcodeFormat format) {
  switch (format) {
    case shape_detection::mojom::BarcodeFormat::AZTEC:
      return WebString::FromUTF8("aztec");
    case shape_detection::mojom::BarcodeFormat::CODE_128:
      return WebString::FromUTF8("code_128");
    case shape_detection::mojom::BarcodeFormat::CODE_39:
      return WebString::FromUTF8("code_39");
    case shape_detection::mojom::BarcodeFormat::CODE_93:
      return WebString::FromUTF8("code_93");
    case shape_detection::mojom::BarcodeFormat::CODABAR:
      return WebString::FromUTF8("codabar");
    case shape_detection::mojom::BarcodeFormat::DATA_MATRIX:
      return WebString::FromUTF8("data_matrix");
    case shape_detection::mojom::BarcodeFormat::EAN_13:
      return WebString::FromUTF8("ean_13");
    case shape_detection::mojom::BarcodeFormat::EAN_8:
      return WebString::FromUTF8("ean_8");
    case shape_detection::mojom::BarcodeFormat::ITF:
      return WebString::FromUTF8("itf");
    case shape_detection::mojom::BarcodeFormat::PDF417:
      return WebString::FromUTF8("pdf417");
    case shape_detection::mojom::BarcodeFormat::QR_CODE:
      return WebString::FromUTF8("qr_code");
    case shape_detection::mojom::BarcodeFormat::UNKNOWN:
      return WebString::FromUTF8("unknown");
    case shape_detection::mojom::BarcodeFormat::UPC_A:
      return WebString::FromUTF8("upc_a");
    case shape_detection::mojom::BarcodeFormat::UPC_E:
      return WebString::FromUTF8("upc_e");
    default:
      NOTREACHED() << "Invalid BarcodeFormat";
  }
  return WebString();
}

const String& DetectedBarcode::rawValue() const {
  return raw_value_;
}

DOMRectReadOnly* DetectedBarcode::boundingBox() const {
  return bounding_box_.Get();
}

const HeapVector<Member<Point2D>>& DetectedBarcode::cornerPoints() const {
  return corner_points_;
}

String DetectedBarcode::format() const {
  return DetectedBarcode::BarcodeFormatToString(format_);
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
