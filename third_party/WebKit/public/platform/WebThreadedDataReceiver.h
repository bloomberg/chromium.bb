// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebThreadedDataReceiver_h
#define WebThreadedDataReceiver_h

#include "public/platform/WebPrivatePtr.h"

namespace blink {

class ThreadedDataReceiver;
class WebThread;

class WebThreadedDataReceiver {
public:
    ~WebThreadedDataReceiver() { reset(); }

    WebThreadedDataReceiver() { }
    WebThreadedDataReceiver(const WebThreadedDataReceiver& r) { assign(r); }
    WebThreadedDataReceiver& operator=(const WebThreadedDataReceiver& r)
    {
        assign(r);
        return *this;
    }

    BLINK_EXPORT void reset();
    BLINK_EXPORT void assign(const WebThreadedDataReceiver&);

    bool isNull() const { return m_private.isNull(); }

#if BLINK_IMPLEMENTATION
    explicit WebThreadedDataReceiver(const PassRefPtrWillBeRawPtr<ThreadedDataReceiver>&);
#endif

    BLINK_EXPORT void acceptData(const char* data, int dataLength);
    BLINK_EXPORT WebThread* backgroundThread();

    BLINK_EXPORT bool needsMainthreadDataCopy();
    BLINK_EXPORT void acceptMainthreadDataNotification(const char* data, int dataLength, int encodedDataLength);

private:
    WebPrivatePtr<ThreadedDataReceiver> m_private;
};

} // namespace blink

#endif // WebThreadedDataReceiver_h
