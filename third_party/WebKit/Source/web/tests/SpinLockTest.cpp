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

#include "platform/wtf/SpinLock.h"

#include <memory>
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static const size_t kBufferSize = 16;

static SpinLock g_lock;

static void FillBuffer(volatile char* buffer, char fill_pattern) {
  for (size_t i = 0; i < kBufferSize; ++i)
    buffer[i] = fill_pattern;
}

static void ChangeAndCheckBuffer(volatile char* buffer) {
  FillBuffer(buffer, '\0');
  int total = 0;
  for (size_t i = 0; i < kBufferSize; ++i)
    total += buffer[i];

  EXPECT_EQ(0, total);

  // This will mess with the other thread's calculation if we accidentally get
  // concurrency.
  FillBuffer(buffer, '!');
}

static void ThreadMain(volatile char* buffer) {
  for (int i = 0; i < 500000; ++i) {
    SpinLock::Guard guard(g_lock);
    ChangeAndCheckBuffer(buffer);
  }
}

TEST(SpinLockTest, Torture) {
  char shared_buffer[kBufferSize];

  std::unique_ptr<WebThread> thread1 =
      WTF::WrapUnique(Platform::Current()->CreateThread("thread1"));
  std::unique_ptr<WebThread> thread2 =
      WTF::WrapUnique(Platform::Current()->CreateThread("thread2"));

  thread1->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&ThreadMain, CrossThreadUnretained(
                                       static_cast<char*>(shared_buffer))));
  thread2->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&ThreadMain, CrossThreadUnretained(
                                       static_cast<char*>(shared_buffer))));

  thread1.reset();
  thread2.reset();
}

}  // namespace blink
