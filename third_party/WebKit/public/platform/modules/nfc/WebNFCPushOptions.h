// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNFCPushOptions_h
#define WebNFCPushOptions_h

#include "public/platform/modules/nfc/WebNFCPushTarget.h"

namespace blink {

// Contains members of NFCPushOption dictionary as specified in the IDL.
struct WebNFCPushOptions {
    WebNFCPushOptions(const WebNFCPushTarget& target = WebNFCPushTarget::Any, double timeout = std::numeric_limits<double>::infinity(), bool ignoreRead = true)
        : target(target), timeout(timeout), ignoreRead(ignoreRead)
    {
    }

    WebNFCPushTarget target;
    double timeout; // in milliseconds
    bool ignoreRead;
};

} // namespace blink

#endif // WebNFCPushOptions_h
