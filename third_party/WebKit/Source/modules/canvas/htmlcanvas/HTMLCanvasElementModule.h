// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLCanvasElementModule_h
#define HTMLCanvasElementModule_h

#include "core/html/canvas/HTMLCanvasElement.h"
#include "modules/ModulesExport.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CanvasContextCreationAttributesModule;
class HTMLCanvasElement;
class OffscreenCanvas;

class MODULES_EXPORT HTMLCanvasElementModule {
  STATIC_ONLY(HTMLCanvasElementModule);

  friend class HTMLCanvasElementModuleTest;

 public:
  static void getContext(HTMLCanvasElement&,
                         const String&,
                         const CanvasContextCreationAttributesModule&,
                         ExceptionState&,
                         RenderingContext&);
  static OffscreenCanvas* transferControlToOffscreen(HTMLCanvasElement&,
                                                     ExceptionState&);

 private:
  static OffscreenCanvas* TransferControlToOffscreenInternal(HTMLCanvasElement&,
                                                             ExceptionState&);
};
}  // namespace blink

#endif
