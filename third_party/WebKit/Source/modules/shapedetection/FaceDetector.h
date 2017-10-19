// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FaceDetector_h
#define FaceDetector_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/ModulesExport.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/shapedetection/ShapeDetector.h"
#include "platform/bindings/ScriptWrappable.h"
#include "services/shape_detection/public/interfaces/facedetection.mojom-blink.h"

namespace blink {

class ExecutionContext;
class FaceDetectorOptions;

class MODULES_EXPORT FaceDetector final : public ShapeDetector,
                                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static FaceDetector* Create(ExecutionContext*, const FaceDetectorOptions&);

  virtual void Trace(blink::Visitor*);

 private:
  FaceDetector(ExecutionContext*, const FaceDetectorOptions&);
  ~FaceDetector() override = default;

  ScriptPromise DoDetect(ScriptPromiseResolver*,
                         skia::mojom::blink::BitmapPtr) override;
  void OnDetectFaces(
      ScriptPromiseResolver*,
      Vector<shape_detection::mojom::blink::FaceDetectionResultPtr>);
  void OnFaceServiceConnectionError();

  shape_detection::mojom::blink::FaceDetectionPtr face_service_;

  HeapHashSet<Member<ScriptPromiseResolver>> face_service_requests_;
};

}  // namespace blink

#endif  // FaceDetector_h
