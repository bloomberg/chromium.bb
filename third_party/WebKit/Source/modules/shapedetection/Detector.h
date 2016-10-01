// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Detector_h
#define Detector_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"

namespace blink {

class DetectedObject;
class HTMLImageElement;

class MODULES_EXPORT Detector : public GarbageCollected<Detector>,
                                public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Detector* create();
  virtual ScriptPromise detect(ScriptState*, const HTMLImageElement*) = 0;
  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // Detector_h
