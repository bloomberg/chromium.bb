// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasModules_h
#define OffscreenCanvasModules_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "modules/ModulesExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CanvasContextCreationAttributes;
class OffscreenCanvas;

class MODULES_EXPORT OffscreenCanvasModules {
  STATIC_ONLY(OffscreenCanvasModules);

 public:
  static void getContext(ExecutionContext*,
                         OffscreenCanvas&,
                         const String&,
                         const CanvasContextCreationAttributes&,
                         ExceptionState&,
                         OffscreenRenderingContext&);
};

}  // namespace blink

#endif  // OffscreenCanvasModules_h
