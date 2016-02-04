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
    {
    }

    // Individual packet statuses. This vector has only one element if this is
    // not an isochronous transfer.
    WebVector<Status> status;

    // Data received, if this is an inbound transfer.
    WebVector<uint8_t> data;

    // Requested length of each packet if this is an inbound isochronous
    // transfer.
    WebVector<uint32_t> packetLength;

    // Number of bytes written if this is an outbound transfer. This vector has
    // only one element if this is not an isochronous transfer otherwise it is
    // the number of bytes transferred in each isochronous packet (inbound or
    // outbound).
    WebVector<uint32_t> bytesTransferred;
};

} // namespace blink

#endif // WebUSBTransferInfo_h
