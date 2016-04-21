// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransferablesForModules_h
#define TransferablesForModules_h

#include "bindings/core/v8/Transferables.h"
#include "modules/ModulesExport.h"
#include "modules/offscreencanvas/OffscreenCanvas.h"

namespace blink {

using OffscreenCanvasArray = HeapVector<Member<OffscreenCanvas>>;

class MODULES_EXPORT TransferablesForModules : public Transferables {
public:
    TransferablesForModules() { }

    OffscreenCanvasArray offscreenCanvases;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(offscreenCanvases);
        Transferables::trace(visitor);
    }
};

} // namespace blink

#endif // TransferablesForModules_h
