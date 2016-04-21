// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/Transferables.h"

#include "core/dom/DOMArrayBufferBase.h"
#include "core/dom/MessagePort.h"
#include "core/frame/ImageBitmap.h"

namespace blink {

DEFINE_TRACE(Transferables)
{
    visitor->trace(arrayBuffers);
    visitor->trace(imageBitmaps);
    visitor->trace(messagePorts);
}

} // namespace blink
