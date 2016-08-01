// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCStats_h
#define WebRTCStats_h

#include "WebCommon.h"
#include "WebString.h"

namespace blink {

class WebRTCStatsMemberIterator;
class WebRTCStatsMember;

enum WebRTCStatsType {
    WebRTCStatsTypeUnknown
};

enum WebRTCStatsMemberName {
    WebRTCStatsMemberNameUnknown
};

enum WebRTCStatsMemberType {
    WebRTCStatsMemberTypeInt,
    WebRTCStatsMemberTypeInt64,
    WebRTCStatsMemberTypeFloat,
    WebRTCStatsMemberTypeString,
    WebRTCStatsMemberTypeBool,
    WebRTCStatsMemberTypeId,
};

class WebRTCStats {
public:
    virtual ~WebRTCStats() {}

    virtual WebString id() const = 0;
    virtual WebRTCStatsType type() const = 0;
    virtual WebString typeToString() const = 0;
    virtual double timestamp() const = 0;

    // The caller owns the iterator. The iterator must not be used after
    // the |WebRTCStats| that created it is destroyed.
    virtual WebRTCStatsMemberIterator* iterator() const = 0;
};

class WebRTCStatsMemberIterator {
public:
    virtual ~WebRTCStatsMemberIterator() {}
    virtual bool isEnd() const = 0;
    virtual void next() = 0;

    virtual WebRTCStatsMemberName name() const = 0;
    virtual WebString displayName() const = 0;

    virtual WebRTCStatsMemberType type() const = 0;
    // Value getters. No conversion is performed; the function must match the
    // member's |type|.
    virtual int valueInt() const = 0; // WebRTCStatsMemberTypeInt
    virtual int64_t valueInt64() const = 0; // WebRTCStatsMemberTypeInt64
    virtual float valueFloat() const = 0; // WebRTCStatsMemberTypeFloat
    virtual WebString valueString() const = 0; // WebRTCStatsMemberTypeString
    virtual bool valueBool() const = 0; // WebRTCStatsMemberTypeBool

    // Converts the value to string (regardless of |type|).
    virtual WebString valueToString() const = 0;
};

} // namespace blink

#endif // WebRTCStats_h
