// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUSBTransferInfo_h
#define WebUSBTransferInfo_h

#include "public/platform/WebVector.h"

namespace blink {

struct WebUSBTransferInfo {
    enum class Status {
        Ok,
        Stall,
        Babble,
    };

    WebUSBTransferInfo()
        : status(Status::Ok)
        , bytesWritten(0)
    {
    }

    Status status;

    // Data received, if this is an inbound transfer.
    WebVector<uint8_t> data;

    // Number of bytes written if this is an outbound transfer.
    uint32_t bytesWritten;
};

} // namespace blink

#endif // WebUSBTransferInfo_h
