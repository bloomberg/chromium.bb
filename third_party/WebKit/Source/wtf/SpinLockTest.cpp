/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "wtf/SpinLock.h"

#include "wtf/Threading.h"
#include <gtest/gtest.h>

namespace {

static const size_t bufferSize = 16;

static int lock = 0;

static void fillBuffer(volatile char* buffer, char fillPattern)
{
    for (int i = 0; i < bufferSize; ++i)
        buffer[i] = fillPattern;
}

static void changeAndCheckBuffer(volatile char* buffer)
{
    fillBuffer(buffer, '\0');
    int total = 0;
    for (int i = 0; i < bufferSize; ++i)
        total += buffer[i];

    EXPECT_EQ(0, total);

    // This will mess with the other thread's calculation if we accidentally get
    // concurrency.
    fillBuffer(buffer, '!');
}

static void threadMain(void* arg)
{
    volatile char* buffer = reinterpret_cast<volatile char*>(arg);
    for (int i = 0; i < 500000; ++i) {
        spinLockLock(&lock);
        changeAndCheckBuffer(buffer);
        spinLockUnlock(&lock);
    }
}

TEST(WTF_SpinLock, Torture)
{
    char sharedBuffer[bufferSize];

    ThreadIdentifier thread1 = createThread(threadMain, sharedBuffer, "thread1");
    ThreadIdentifier thread2 = createThread(threadMain, sharedBuffer, "thread2");
    EXPECT_NE(0, thread1);
    EXPECT_NE(0, thread2);

    int ret = waitForThreadCompletion(thread1);
    EXPECT_EQ(0, ret);
    ret = waitForThreadCompletion(thread2);
    EXPECT_EQ(0, ret);
}

} // namespace
