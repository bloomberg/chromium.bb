// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasModules_h
#define OffscreenCanvasModules_h

#include "bindings/core/v8/ExceptionState.h"
#include "modules/ModulesExport.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CanvasContextCreationAttributes;
class OffscreenCanvas;
class OffscreenCanvasRenderingContext2D;

class MODULES_EXPORT OffscreenCanvasModules {
    STATIC_ONLY(OffscreenCanvasModules)
public:
    static OffscreenCanvasRenderingContext2D* getContext(OffscreenCanvas&, const String&, const CanvasContextCreationAttributes&, ExceptionState&);
};

} // namespace blink

#endif // OffscreenCanvasModules_h

