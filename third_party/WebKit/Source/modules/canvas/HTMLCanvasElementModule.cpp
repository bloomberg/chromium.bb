// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/HTMLCanvasElementModule.h"

#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"

namespace blink {

void HTMLCanvasElementModule::getContext(HTMLCanvasElement& canvas, const String& type, const CanvasContextCreationAttributes& attributes, RenderingContext& result)
{
    CanvasRenderingContext* context = canvas.getCanvasRenderingContext(type, attributes);
    if (context) {
        context->setCanvasGetContextResult(result);
    }
}

}
