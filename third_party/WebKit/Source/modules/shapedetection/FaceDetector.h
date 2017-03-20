// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FaceDetector_h
#define FaceDetector_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/shapedetection/ShapeDetector.h"
#include "services/shape_detection/public/interfaces/facedetection.mojom-blink.h"

namespace blink {

class FaceDetectorOptions;

class MODULES_EXPORT FaceDetector final : public ShapeDetector,
                                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static FaceDetector* create(const FaceDetectorOptions&);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit FaceDetector(const FaceDetectorOptions&);
  ~FaceDetector() override = default;

  ScriptPromise doDetect(ScriptPromiseResolver*,
                         mojo::ScopedSharedBufferHandle,
                         int imageWidth,
                         int imageHeight) override;
  void onDetectFaces(ScriptPromiseResolver*,
                     shape_detection::mojom::blink::FaceDetectionResultPtr);
  void onFaceServiceConnectionError();

  shape_detection::mojom::blink::FaceDetectionPtr m_faceService;

  HeapHashSet<Member<ScriptPromiseResolver>> m_faceServiceRequests;
};

}  // namespace blink

#endif  // FaceDetector_h
