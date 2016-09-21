// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BytesConsumerTestUtil_h
#define BytesConsumerTestUtil_h

#include "modules/fetch/BytesConsumer.h"
#include "platform/heap/Handle.h"
#include "wtf/Deque.h"
#include "wtf/Vector.h"

namespace blink {

class ExecutionContext;

class BytesConsumerTestUtil {
    STATIC_ONLY(BytesConsumerTestUtil);
public:
    class Command final {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        enum Name {
            Data,
            Done,
            Error,
            Wait,
        };

        explicit Command(Name name) : m_name(name) { }
        Command(Name name, const Vector<char>& body) : m_name(name), m_body(body) { }
        Command(Name name, const char* body, size_t size) : m_name(name)
        {
            m_body.append(body, size);
        }
        Command(Name name, const char* body) : Command(name, body, strlen(body)) { }
        Name getName() const { return m_name; }
        const Vector<char>& body() const { return m_body; }

    private:
        const Name m_name;
        Vector<char> m_body;
    };

    // ReplayingBytesConsumer stores commands via |add| and replays the stored
    // commends when read.
    class ReplayingBytesConsumer final : public BytesConsumer {
    public:
        // The ExecutionContext is needed to get a WebTaskRunner.
        explicit ReplayingBytesConsumer(ExecutionContext*);
        ~ReplayingBytesConsumer();

        // Add a command to this handle. This function must be called BEFORE
        // any BytesConsumer methods are called.
        void add(const Command& command) { m_commands.append(command); }

        Result beginRead(const char** buffer, size_t* available) override;
        Result endRead(size_t readSize) override;

        void setClient(Client*) override;
        void clearClient() override;
        void cancel() override;
        PublicState getPublicState() const override;
        Error getError() const override;
        String debugName() const override { return "ReplayingBytesConsumer"; }

        bool isCancelled() const { return m_isCancelled; }

        DECLARE_TRACE();

    private:
        void notifyAsReadable(int notificationToken);
        void close();
        void error(const Error&);

        Member<ExecutionContext> m_executionContext;
        Member<BytesConsumer::Client> m_client;
        InternalState m_state = InternalState::Waiting;
        Deque<Command> m_commands;
        size_t m_offset = 0;
        BytesConsumer::Error m_error;
        int m_notificationToken = 0;
        bool m_isCancelled = false;
    };

    class TwoPhaseReader final : public GarbageCollectedFinalized<TwoPhaseReader>, public BytesConsumer::Client {
        USING_GARBAGE_COLLECTED_MIXIN(TwoPhaseReader);
    public:
        // |consumer| must not have a client when called.
        explicit TwoPhaseReader(BytesConsumer* /* consumer */);

        void onStateChange() override;
        std::pair<BytesConsumer::Result, Vector<char>> run();

        DEFINE_INLINE_TRACE()
        {
            visitor->trace(m_consumer);
            BytesConsumer::Client::trace(visitor);
        }

    private:
        Member<BytesConsumer> m_consumer;
        BytesConsumer::Result m_result = BytesConsumer::Result::ShouldWait;
        Vector<char> m_data;
    };
};

} // namespace blink

#endif // BytesConsumerTestUtil_h
