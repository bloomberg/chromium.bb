// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNFCWatchOptions_h
#define WebNFCWatchOptions_h

#include "public/platform/WebString.h"

namespace blink {

enum class WebNFCRecordType;

// Enumeration that is mapped to NFCWatchMode as specified in IDL.
enum class WebNFCWatchMode {
    WebNFCOnly,
    Any,
    ENUM_MAX_VALUE = Any
};

// Helper wrapper that is used to represent filter for all types of NFC records.
class WebNFCRecordTypeFilter final {
public:
    WebNFCRecordTypeFilter() : m_matchAnyRecord(true) { }
    explicit WebNFCRecordTypeFilter(const WebNFCRecordType& recordType) :
        m_matchAnyRecord(false), m_recordType(recordType) { }

    bool matchAnyRecord() const { return m_matchAnyRecord; }
    WebNFCRecordType recordType() const { return m_recordType; }

private:
    bool m_matchAnyRecord;
    WebNFCRecordType m_recordType;
};

// Contains members of NFCWatchOptions dictionary as specified in the IDL.
struct WebNFCWatchOptions {
    WebNFCWatchOptions(const WebString& url = "", const WebNFCRecordTypeFilter& recordFilter = WebNFCRecordTypeFilter(),
        const WebString& mediaType = "", const WebNFCWatchMode& mode = WebNFCWatchMode::WebNFCOnly)
    : url(url)
    , recordFilter(recordFilter)
    , mediaType(mediaType)
    , mode(mode)
    {
    }

    WebString url;
    WebNFCRecordTypeFilter recordFilter;
    WebString mediaType;
    WebNFCWatchMode mode;
};

} // namespace blink

#endif // WebNFCWatchOptions_h
