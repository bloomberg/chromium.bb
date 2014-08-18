// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInspectorProxy_h
#define WorkerInspectorProxy_h

#include "wtf/text/WTFString.h"

namespace blink {

// A proxy for talking to the worker inspector on the worker thread.
// All of these methods should be called on the main thread.
class WorkerInspectorProxy {
public:
    virtual ~WorkerInspectorProxy() { }

    class PageInspector {
    public:
        virtual ~PageInspector() { }
        virtual void dispatchMessageFromWorker(const String&) = 0;
    };

    virtual void connectToInspector(PageInspector*) = 0;
    virtual void disconnectFromInspector() = 0;
    virtual void sendMessageToInspector(const String&) = 0;
    virtual void writeTimelineStartedEvent(const String& sessionId) = 0;
};

} // namespace blink

#endif // WorkerInspectorProxy_h
