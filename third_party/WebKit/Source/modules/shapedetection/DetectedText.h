// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DetectedText_h
#define DetectedText_h

#include "modules/ModulesExport.h"
#include "modules/imagecapture/Point2D.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class DOMRect;

class MODULES_EXPORT DetectedText final
    : public GarbageCollectedFinalized<DetectedText>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DetectedText* Create();
  static DetectedText* Create(String, DOMRect*, HeapVector<Point2D>);

  const String& rawValue() const;
  DOMRect* boundingBox() const;
  const HeapVector<Point2D>& cornerPoints() const;
  void Trace(blink::Visitor*);

 private:
  DetectedText(String, DOMRect*, HeapVector<Point2D>);

  const String raw_value_;
  const Member<DOMRect> bounding_box_;
  const HeapVector<Point2D> corner_points_;
};

}  // namespace blink

#endif  // DetectedText_h
