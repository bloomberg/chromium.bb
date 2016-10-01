// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/FaceDetector.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"

namespace blink {

FaceDetector* FaceDetector::create() {
  return new FaceDetector();
}

ScriptPromise FaceDetector::detect(ScriptState* scriptState,
                                   const HTMLImageElement* image) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();
  resolver->reject(DOMException::create(NotSupportedError, "Not implemented"));
  return promise;
}

}  // namespace blink
