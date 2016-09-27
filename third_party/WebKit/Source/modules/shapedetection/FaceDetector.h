// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FaceDetector_h
#define FaceDetector_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/ModulesExport.h"
#include "modules/shapedetection/Detector.h"

namespace blink {

class MODULES_EXPORT FaceDetector final : public Detector {
    DEFINE_WRAPPERTYPEINFO();

public:
    static FaceDetector* create();

    ScriptPromise detect(ScriptState*, const HTMLImageElement*);
};

} // namespace blink

#endif // FaceDetector_h
