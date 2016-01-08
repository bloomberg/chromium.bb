// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebMemoryPressureListener.h"

#include "core/page/Page.h"
#include "platform/MemoryPurgeController.h"

namespace blink {

void WebMemoryPressureListener::onMemoryPressure(WebMemoryPressureLevel pressureLevel)
{
    Page::onMemoryPressure();
    MemoryPurgeController::onMemoryPressure(pressureLevel);
}

} // namespace blink
