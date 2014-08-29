// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleMessageStorage_h
#define ConsoleMessageStorage_h

#include "core/inspector/ConsoleMessage.h"
#include "wtf/Forward.h"

namespace blink {

class LocalDOMWindow;

class ConsoleMessageStorage FINAL {
    WTF_MAKE_NONCOPYABLE(ConsoleMessageStorage);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ConsoleMessageStorage> createForWorker(ExecutionContext* context)
    {
        return adoptPtr(new ConsoleMessageStorage(context));
    }

    static PassOwnPtr<ConsoleMessageStorage> createForFrame(LocalFrame* frame)
    {
        return adoptPtr(new ConsoleMessageStorage(frame));
    }

    void reportMessage(PassRefPtr<ConsoleMessage>);
    void clear();

    Vector<unsigned> argumentCounts() const;
    void frameWindowDiscarded(LocalDOMWindow*);

    size_t size() const;
    PassRefPtr<ConsoleMessage> at(size_t index) const;

    int expiredCount() const;

private:
    ConsoleMessageStorage(ExecutionContext*);
    ConsoleMessageStorage(LocalFrame*);

    ExecutionContext* executionContext() const;

    int m_expiredCount;
    Vector<RefPtr<ConsoleMessage> > m_messages;
    ExecutionContext* m_context;
    LocalFrame* m_frame;
};

} // namespace blink

#endif // ConsoleMessageStorage_h
