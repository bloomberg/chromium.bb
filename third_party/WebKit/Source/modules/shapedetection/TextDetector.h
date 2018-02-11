// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextDetector_h
#define TextDetector_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/ModulesExport.h"
#include "modules/canvas/canvas2d/CanvasRenderingContext2D.h"
#include "modules/shapedetection/ShapeDetector.h"
#include "services/shape_detection/public/mojom/textdetection.mojom-blink.h"

namespace blink {

class ExecutionContext;

class MODULES_EXPORT TextDetector final : public ShapeDetector {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TextDetector* Create(ExecutionContext*);

  virtual void Trace(blink::Visitor*);

 private:
  explicit TextDetector(ExecutionContext*);
  ~TextDetector() override = default;

  ScriptPromise DoDetect(ScriptPromiseResolver*, SkBitmap) override;
  void OnDetectText(
      ScriptPromiseResolver*,
      Vector<shape_detection::mojom::blink::TextDetectionResultPtr>);
  void OnTextServiceConnectionError();

  shape_detection::mojom::blink::TextDetectionPtr text_service_;

  HeapHashSet<Member<ScriptPromiseResolver>> text_service_requests_;
};

}  // namespace blink

#endif  // TextDetector_h
