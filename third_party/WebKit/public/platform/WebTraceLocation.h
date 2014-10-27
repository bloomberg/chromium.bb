// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTraceLocation_h
#define WebTraceLocation_h

#include "WebCommon.h"

#if INSIDE_BLINK
#include "platform/TraceLocation.h"
#endif

namespace blink {

// This class is used to keep track of where posted tasks originate. See base/location.h in Chromium.
class BLINK_PLATFORM_EXPORT WebTraceLocation {
public:
#if INSIDE_BLINK
    explicit WebTraceLocation(const TraceLocation&);
#endif

    const char* functionName() const;
    const char* fileName() const;

private:
    WebTraceLocation();

#if INSIDE_BLINK
    TraceLocation m_location;
#endif
};

}

#endif // WebTraceLocation_h
