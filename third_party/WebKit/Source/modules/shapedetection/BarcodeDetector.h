// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BarcodeDetector_h
#define BarcodeDetector_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/shapedetection/ShapeDetector.h"
#include "public/platform/modules/shapedetection/barcodedetection.mojom-blink.h"

namespace blink {

class LocalFrame;

class MODULES_EXPORT BarcodeDetector final : public ShapeDetector,
                                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BarcodeDetector* create(Document&);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit BarcodeDetector(LocalFrame&);
  ~BarcodeDetector() override = default;

  ScriptPromise doDetect(ScriptPromiseResolver*,
                         mojo::ScopedSharedBufferHandle,
                         int imageWidth,
                         int imageHeight) override;
  void onDetectBarcodes(ScriptPromiseResolver*,
                        Vector<mojom::blink::BarcodeDetectionResultPtr>);
  void onBarcodeServiceConnectionError();

  mojom::blink::BarcodeDetectionPtr m_barcodeService;

  HeapHashSet<Member<ScriptPromiseResolver>> m_barcodeServiceRequests;
};

}  // namespace blink

#endif  // BarcodeDetector_h
