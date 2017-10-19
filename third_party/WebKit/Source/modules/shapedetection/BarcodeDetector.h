// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BarcodeDetector_h
#define BarcodeDetector_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/ModulesExport.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/shapedetection/ShapeDetector.h"
#include "platform/bindings/ScriptWrappable.h"
#include "services/shape_detection/public/interfaces/barcodedetection.mojom-blink.h"

namespace blink {

class ExecutionContext;

class MODULES_EXPORT BarcodeDetector final : public ShapeDetector,
                                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BarcodeDetector* Create(ExecutionContext*);

  virtual void Trace(blink::Visitor*);

 private:
  explicit BarcodeDetector(ExecutionContext*);
  ~BarcodeDetector() override = default;

  ScriptPromise DoDetect(ScriptPromiseResolver*,
                         skia::mojom::blink::BitmapPtr) override;
  void OnDetectBarcodes(
      ScriptPromiseResolver*,
      Vector<shape_detection::mojom::blink::BarcodeDetectionResultPtr>);
  void OnBarcodeServiceConnectionError();

  shape_detection::mojom::blink::BarcodeDetectionPtr barcode_service_;

  HeapHashSet<Member<ScriptPromiseResolver>> barcode_service_requests_;
};

}  // namespace blink

#endif  // BarcodeDetector_h
