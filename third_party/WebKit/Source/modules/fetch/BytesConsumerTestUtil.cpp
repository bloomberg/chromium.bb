// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BytesConsumerTestUtil.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebTaskRunner.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"

namespace blink {

using Result = BytesConsumer::Result;

BytesConsumerTestUtil::ReplayingBytesConsumer::ReplayingBytesConsumer(ExecutionContext* executionContext)
    : m_executionContext(executionContext)
{
}

BytesConsumerTestUtil::ReplayingBytesConsumer::~ReplayingBytesConsumer()
{
}

Result BytesConsumerTestUtil::ReplayingBytesConsumer::beginRead(const char** buffer, size_t* available)
{
    ++m_notificationToken;
    if (m_commands.isEmpty()) {
        switch (m_state) {
        case BytesConsumer::InternalState::Readable:
        case BytesConsumer::InternalState::Waiting:
            return Result::ShouldWait;
        case BytesConsumer::InternalState::Closed:
            return Result::Done;
        case BytesConsumer::InternalState::Errored:
            return Result::Error;
        }
    }
    const Command& command = m_commands[0];
    switch (command.getName()) {
    case Command::Data:
        DCHECK_LE(m_offset, command.body().size());
        *buffer = command.body().data() + m_offset;
        *available = command.body().size() - m_offset;
        return Result::Ok;
    case Command::Done:
        m_commands.removeFirst();
        close();
        return Result::Done;
    case Command::Error: {
        Error e(String::fromUTF8(command.body().data(), command.body().size()));
        m_commands.removeFirst();
        error(std::move(e));
        return Result::Error;
    }
    case Command::Wait:
        m_commands.removeFirst();
        m_state = InternalState::Waiting;
        TaskRunnerHelper::get(TaskType::Networking, m_executionContext)->postTask(
            BLINK_FROM_HERE,
            WTF::bind(&ReplayingBytesConsumer::notifyAsReadable, wrapPersistent(this), m_notificationToken));
        return Result::ShouldWait;
    }
    NOTREACHED();
    return Result::Error;
}

Result BytesConsumerTestUtil::ReplayingBytesConsumer::endRead(size_t read)
{
    DCHECK(!m_commands.isEmpty());
    const Command& command = m_commands[0];
    DCHECK_EQ(Command::Data, command.getName());
    m_offset += read;
    DCHECK_LE(m_offset, command.body().size());
    if (m_offset < command.body().size())
        return Result::Ok;

    m_offset = 0;
    m_commands.removeFirst();
    return Result::Ok;
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::setClient(Client* client)
{
    DCHECK(!m_client);
    DCHECK(client);
    m_client = client;
    ++m_notificationToken;
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::clearClient()
{
    DCHECK(m_client);
    m_client = nullptr;
    ++m_notificationToken;
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::cancel()
{
    close();
    m_isCancelled = true;
}

BytesConsumer::PublicState BytesConsumerTestUtil::ReplayingBytesConsumer::getPublicState() const
{
    return getPublicStateFromInternalState(m_state);
}

BytesConsumer::Error BytesConsumerTestUtil::ReplayingBytesConsumer::getError() const
{
    return m_error;
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::notifyAsReadable(int notificationToken)
{
    if (m_notificationToken != notificationToken) {
        // The notification is cancelled.
        return;
    }
    DCHECK(m_client);
    DCHECK_NE(InternalState::Closed, m_state);
    DCHECK_NE(InternalState::Errored, m_state);
    m_client->onStateChange();
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::close()
{
    m_commands.clear();
    m_offset = 0;
    m_state = InternalState::Closed;
    ++m_notificationToken;
}

void BytesConsumerTestUtil::ReplayingBytesConsumer::error(const Error& e)
{
    m_commands.clear();
    m_offset = 0;
    m_error = e;
    m_state = InternalState::Errored;
    ++m_notificationToken;
}

DEFINE_TRACE(BytesConsumerTestUtil::ReplayingBytesConsumer)
{
    visitor->trace(m_executionContext);
    visitor->trace(m_client);
    BytesConsumer::trace(visitor);
}

BytesConsumerTestUtil::Reader::Reader(BytesConsumer* consumer)
    : m_consumer(consumer)
{
    m_consumer->setClient(this);
}

void BytesConsumerTestUtil::Reader::onStateChange()
{
    while (true) {
        // We choose 3 here because of the following reasons.
        //  - We want to split a string with multiple chunks, so we need to
        //    choose a small number.
        //  - An odd number is preferable to check an out-of-range error.
        //  - With 1, every chunk consists of one byte it's too simple.
        char buffer[3];
        size_t read = 0;
        switch (m_consumer->read(buffer, sizeof(buffer), &read)) {
        case BytesConsumer::Result::Ok:
            m_data.append(buffer, read);
            break;
        case BytesConsumer::Result::ShouldWait:
            return;
        case BytesConsumer::Result::Done:
            m_result = BytesConsumer::Result::Done;
            return;
        case BytesConsumer::Result::Error:
            m_result = BytesConsumer::Result::Error;
            return;
        }
    }
}

std::pair<BytesConsumer::Result, Vector<char>> BytesConsumerTestUtil::Reader::run()
{
    onStateChange();
    while (m_result != BytesConsumer::Result::Done && m_result != BytesConsumer::Result::Error)
        testing::runPendingTasks();
    testing::runPendingTasks();
    return std::make_pair(m_result, std::move(m_data));
}

BytesConsumerTestUtil::TwoPhaseReader::TwoPhaseReader(BytesConsumer* consumer)
    : m_consumer(consumer)
{
    m_consumer->setClient(this);
}


void BytesConsumerTestUtil::TwoPhaseReader::onStateChange()
{
    while (true) {
        const char* buffer = nullptr;
        size_t available = 0;
        switch (m_consumer->beginRead(&buffer, &available)) {
        case BytesConsumer::Result::Ok: {
            // We don't use |available| as-is to test cases where endRead
            // is called with a number smaller than |available|. We choose 3
            // because of the same reasons as Reader::onStateChange.
            size_t read = std::max(static_cast<size_t>(3), available);
            m_data.append(buffer, read);
            m_consumer->endRead(read);
            break;
        }
        case BytesConsumer::Result::ShouldWait:
            return;
        case BytesConsumer::Result::Done:
            m_result = BytesConsumer::Result::Done;
            return;
        case BytesConsumer::Result::Error:
            m_result = BytesConsumer::Result::Error;
            return;
        }
    }
}

std::pair<BytesConsumer::Result, Vector<char>> BytesConsumerTestUtil::TwoPhaseReader::run()
{
    onStateChange();
    while (m_result != BytesConsumer::Result::Done && m_result != BytesConsumer::Result::Error)
        testing::runPendingTasks();
    testing::runPendingTasks();
    return std::make_pair(m_result, std::move(m_data));
}

} // namespace blink
