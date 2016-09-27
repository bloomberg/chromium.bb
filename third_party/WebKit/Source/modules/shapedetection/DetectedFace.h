// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DetectedFace_h
#define DetectedFace_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "modules/shapedetection/DetectedObject.h"

namespace blink {

class MODULES_EXPORT DetectedFace final : public DetectedObject {
    DEFINE_WRAPPERTYPEINFO();

public:
    static DetectedFace* create();
};

} // namespace blink

#endif // DetectedFace_h
