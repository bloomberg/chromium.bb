// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DetectedBarcode_h
#define DetectedBarcode_h

#include "modules/ModulesExport.h"
#include "modules/imagecapture/Point2D.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DOMRect;

class MODULES_EXPORT DetectedBarcode final
    : public GarbageCollectedFinalized<DetectedBarcode>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DetectedBarcode* Create();
  static DetectedBarcode* Create(String, DOMRect*, HeapVector<Point2D>);

  const String& rawValue() const;
  DOMRect* boundingBox() const;
  const HeapVector<Point2D>& cornerPoints() const;
  void Trace(blink::Visitor*);

 private:
  DetectedBarcode(String, DOMRect*, HeapVector<Point2D>);

  const String raw_value_;
  const Member<DOMRect> bounding_box_;
  const HeapVector<Point2D> corner_points_;
};

}  // namespace blink

#endif  // DetectedBarcode_h
