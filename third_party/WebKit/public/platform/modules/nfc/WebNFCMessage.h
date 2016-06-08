// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNFCMessage_h
#define WebNFCMessage_h

#include "public/platform/WebData.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebVector.h"

namespace blink {

// Enumeration that is mapped to NFCRecordType as specified in IDL.
enum class WebNFCRecordType {
    Empty,
    Text,
    Url,
    Json,
    Opaque,
    ENUM_MAX_VALUE = Opaque
};

// Contains members of NFCRecord dictionary as specified in the IDL.
struct WebNFCRecord {
    WebNFCRecordType recordType;
    WebString mediaType;
    WebData data;
};

// Contains members of NFCMessage dictionary as specified in the IDL.
struct WebNFCMessage {
    WebNFCMessage(WebVector<WebNFCRecord>& data, const WebURL& url)
    : data(data)
    , url(url)
    {
    }

    WebVector<WebNFCRecord> data;
    WebURL url;
};

} // namespace blink

#endif // WebNFCMessage_h
