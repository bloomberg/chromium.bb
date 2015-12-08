// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RejectedPromises_h
#define RejectedPromises_h

#include "core/fetch/AccessControlStatus.h"
#include "platform/heap/Handle.h"

namespace v8 {
class PromiseRejectMessage;
};

namespace blink {

class ScriptCallStack;
class ScriptState;

class RejectedPromises final : public RefCountedWillBeGarbageCollected<RejectedPromises> {
    USING_FAST_MALLOC_WILL_BE_REMOVED(RejectedPromises);
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(RejectedPromises);
public:
    static PassRefPtrWillBeRawPtr<RejectedPromises> create()
    {
        return adoptRefWillBeNoop(new RejectedPromises);
    }

    void dispose();
    DECLARE_TRACE();

    void rejectedWithNoHandler(ScriptState*, v8::PromiseRejectMessage, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack>, AccessControlStatus);
    void handlerAdded(v8::PromiseRejectMessage);

    void processQueue();

private:
    class Message;

    RejectedPromises();

    using MessageQueue = WillBeHeapDeque<OwnPtrWillBeMember<Message>>;
    PassOwnPtrWillBeRawPtr<MessageQueue> createMessageQueue();

    void processQueueNow(PassOwnPtrWillBeRawPtr<MessageQueue>);
    void revokeNow(PassOwnPtrWillBeRawPtr<Message>);

    MessageQueue m_queue;
    WillBeHeapVector<OwnPtrWillBeMember<Message>> m_reportedAsErrors;
};

} // namespace blink

#endif // RejectedPromises_h
