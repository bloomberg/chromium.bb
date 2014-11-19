// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "platform/audio/Spatializer.h"

#include "platform/audio/StereoPanner.h"

namespace blink {

Spatializer* Spatializer::create(PanningModel model, float sampleRate)
{
    Spatializer* panner;

    switch (model) {
    case PanningModelEqualPower:
        panner = new StereoPanner(sampleRate);
        break;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return panner;
}

} // namespace blink

#endif // ENABLE(WEB_AUDIO)
