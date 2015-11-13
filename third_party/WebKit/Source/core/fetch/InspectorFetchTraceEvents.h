// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorFetchTraceEvents_h
#define InspectorFetchTraceEvents_h

#include "wtf/PassRefPtr.h"

namespace blink {

class ResourceRequest;
class ResourceResponse;

namespace TraceEvent {
class ConvertableToTraceFormat;
}

namespace InspectorSendRequestEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, const ResourceRequest&);
}

namespace InspectorReceiveResponseEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, const ResourceResponse&);
}

namespace InspectorReceiveDataEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, int encodedDataLength);
}

namespace InspectorResourceFinishEvent {
PassRefPtr<TraceEvent::ConvertableToTraceFormat> data(unsigned long identifier, double finishTime, bool didFail);
}

} // namespace blink

#endif // InspectorFetchTraceEvents_h
