// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushError.h"

namespace WebCore {

String PushError::errorString(blink::WebPushError::ErrorType type)
{
    switch (type) {
    case blink::WebPushError::ErrorTypeAbort:
        return "AbortError";
    case blink::WebPushError::ErrorTypeUnknown:
        return "UnknownError";
    }
    ASSERT_NOT_REACHED();
    return String();
}

} // namespace WebCore
