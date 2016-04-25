// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RejectedPromises_h
#define RejectedPromises_h

#include "core/fetch/AccessControlStatus.h"
#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace v8 {
class PromiseRejectMessage;
};

namespace blink {

class ScriptCallStack;
class ScriptState;

class RejectedPromises final : public RefCounted<RejectedPromises> {
    USING_FAST_MALLOC(RejectedPromises);
public:
    static PassRefPtr<RejectedPromises> create()
    {
        return adoptRef(new RejectedPromises());
    }

    ~RejectedPromises();
    void dispose();

    void rejectedWithNoHandler(ScriptState*, v8::PromiseRejectMessage, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtr<ScriptCallStack>, AccessControlStatus);
    void handlerAdded(v8::PromiseRejectMessage);

    void processQueue();

private:
    class Message;

    RejectedPromises();

    using MessageQueue = Deque<OwnPtr<Message>>;
    PassOwnPtr<MessageQueue> createMessageQueue();

    void processQueueNow(PassOwnPtr<MessageQueue>);
    void revokeNow(PassOwnPtr<Message>);

    MessageQueue m_queue;
    Vector<OwnPtr<Message>> m_reportedAsErrors;
};

} // namespace blink

#endif // RejectedPromises_h
