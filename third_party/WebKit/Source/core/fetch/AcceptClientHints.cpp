// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/fetch/AcceptClientHints.h"

#include "core/frame/LocalFrame.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/HTTPParsers.h"

namespace blink {

void handleAcceptClientHintsHeader(const String& headerValue, LocalFrame* frame)
{
    ASSERT(frame);
    if (RuntimeEnabledFeatures::clientHintsEnabled()) {
        CommaDelimitedHeaderSet acceptCH;
        parseCommaDelimitedHeader(headerValue, acceptCH);
        if (acceptCH.contains("dpr"))
            frame->setShouldSendDPRHint();
        if (acceptCH.contains("rw"))
            frame->setShouldSendRWHint();
    }
}

}
