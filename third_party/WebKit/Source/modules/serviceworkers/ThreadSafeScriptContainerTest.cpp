// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ThreadSafeScriptContainer.h"

#include <memory>
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/wtf/Functional.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ScriptStatus = ThreadSafeScriptContainer::ScriptStatus;

class ThreadSafeScriptContainerTest : public ::testing::Test {
 public:
  ThreadSafeScriptContainerTest()
      : writer_thread_(Platform::Current()->CreateThread("writer_thread")),
        reader_thread_(Platform::Current()->CreateThread("reader_thread")),
        writer_task_runner_(writer_thread_->GetSingleThreadTaskRunner()),
        reader_task_runner_(reader_thread_->GetSingleThreadTaskRunner()),
        container_(base::MakeRefCounted<ThreadSafeScriptContainer>()) {}

 protected:
  WaitableEvent* AddOnWriterThread(
      const KURL& url,
      ThreadSafeScriptContainer::RawScriptData** out_data) {
    writer_task_runner_->PostTask(
        FROM_HERE,
        ConvertToBaseCallback(CrossThreadBind(
            [](scoped_refptr<ThreadSafeScriptContainer> container,
               const KURL& url,
               ThreadSafeScriptContainer::RawScriptData** out_data,
               WaitableEvent* waiter) {
              auto data = ThreadSafeScriptContainer::RawScriptData::Create(
                  WTF::String::FromUTF8("utf-8") /* encoding */,
                  WTF::Vector<WebVector<char>>() /* script_text */,
                  WTF::Vector<WebVector<char>>() /* meta_data */);
              *out_data = data.get();
              container->AddOnIOThread(url, std::move(data));
              waiter->Signal();
            },
            container_, url, CrossThreadUnretained(out_data),
            CrossThreadUnretained(&writer_waiter_))));
    return &writer_waiter_;
  }

  WaitableEvent* OnAllDataAddedOnWriterThread() {
    writer_task_runner_->PostTask(
        FROM_HERE, ConvertToBaseCallback(CrossThreadBind(
                       [](scoped_refptr<ThreadSafeScriptContainer> container,
                          WaitableEvent* waiter) {
                         container->OnAllDataAddedOnIOThread();
                         waiter->Signal();
                       },
                       container_, CrossThreadUnretained(&writer_waiter_))));
    return &writer_waiter_;
  }

  WaitableEvent* GetStatusOnReaderThread(const KURL& url,
                                         ScriptStatus* out_status) {
    reader_task_runner_->PostTask(
        FROM_HERE, ConvertToBaseCallback(CrossThreadBind(
                       [](scoped_refptr<ThreadSafeScriptContainer> container,
                          const KURL& url, ScriptStatus* out_status,
                          WaitableEvent* waiter) {
                         *out_status = container->GetStatusOnWorkerThread(url);
                         waiter->Signal();
                       },
                       container_, url, CrossThreadUnretained(out_status),
                       CrossThreadUnretained(&reader_waiter_))));
    return &reader_waiter_;
  }

  WaitableEvent* WaitOnReaderThread(const KURL& url, bool* out_exists) {
    reader_task_runner_->PostTask(
        FROM_HERE,
        ConvertToBaseCallback(CrossThreadBind(
            [](scoped_refptr<ThreadSafeScriptContainer> container,
               const KURL& url, bool* out_exists, WaitableEvent* waiter) {
              *out_exists = container->WaitOnWorkerThread(url);
              waiter->Signal();
            },
            container_, url, CrossThreadUnretained(out_exists),
            CrossThreadUnretained(&reader_waiter_))));
    return &reader_waiter_;
  }

  WaitableEvent* TakeOnReaderThread(
      const KURL& url,
      ThreadSafeScriptContainer::RawScriptData** out_data) {
    reader_task_runner_->PostTask(
        FROM_HERE, ConvertToBaseCallback(CrossThreadBind(
                       [](scoped_refptr<ThreadSafeScriptContainer> container,
                          const KURL& url,
                          ThreadSafeScriptContainer::RawScriptData** out_data,
                          WaitableEvent* waiter) {
                         auto data = container->TakeOnWorkerThread(url);
                         *out_data = data.get();
                         waiter->Signal();
                       },
                       container_, url, CrossThreadUnretained(out_data),
                       CrossThreadUnretained(&reader_waiter_))));
    return &reader_waiter_;
  }

 private:
  std::unique_ptr<WebThread> writer_thread_;
  std::unique_ptr<WebThread> reader_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> writer_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> reader_task_runner_;

  WaitableEvent writer_waiter_;
  WaitableEvent reader_waiter_;

  scoped_refptr<ThreadSafeScriptContainer> container_;
};

TEST_F(ThreadSafeScriptContainerTest, WaitExistingKey) {
  const KURL kKey("https://example.com/key");
  {
    ScriptStatus result = ScriptStatus::kReceived;
    GetStatusOnReaderThread(kKey, &result)->Wait();
    EXPECT_EQ(ScriptStatus::kPending, result);
  }

  ThreadSafeScriptContainer::RawScriptData* added_data;
  {
    bool result = false;
    WaitableEvent* pending_wait = WaitOnReaderThread(kKey, &result);
    // This should not be signaled until data is added.
    EXPECT_FALSE(pending_wait->IsSignaled());
    WaitableEvent* pending_write = AddOnWriterThread(kKey, &added_data);
    pending_wait->Wait();
    pending_write->Wait();
    EXPECT_TRUE(result);
  }

  {
    ScriptStatus result = ScriptStatus::kFailed;
    GetStatusOnReaderThread(kKey, &result)->Wait();
    EXPECT_EQ(ScriptStatus::kReceived, result);
  }

  {
    ThreadSafeScriptContainer::RawScriptData* taken_data;
    TakeOnReaderThread(kKey, &taken_data)->Wait();
    EXPECT_EQ(added_data, taken_data);
  }

  {
    ScriptStatus result = ScriptStatus::kFailed;
    GetStatusOnReaderThread(kKey, &result)->Wait();
    // The record of |kKey| should be exist though it's already taken.
    EXPECT_EQ(ScriptStatus::kTaken, result);
  }

  {
    bool result = false;
    WaitOnReaderThread(kKey, &result)->Wait();
    // Waiting for |kKey| should succeed.
    EXPECT_TRUE(result);

    ThreadSafeScriptContainer::RawScriptData* taken_data;
    TakeOnReaderThread(kKey, &taken_data)->Wait();
    // |taken_data| should be nullptr because it's already taken.
    EXPECT_EQ(nullptr, taken_data);
  }

  // Finish adding data.
  OnAllDataAddedOnWriterThread()->Wait();

  {
    bool result = false;
    WaitOnReaderThread(kKey, &result)->Wait();
    // The record has been already added, so Wait shouldn't fail.
    EXPECT_TRUE(result);

    ThreadSafeScriptContainer::RawScriptData* taken_data;
    TakeOnReaderThread(kKey, &taken_data)->Wait();
    // |taken_data| should be nullptr because it's already taken.
    EXPECT_EQ(nullptr, taken_data);
  }
}

TEST_F(ThreadSafeScriptContainerTest, WaitNonExistingKey) {
  const KURL kKey("https://example.com/key");
  {
    ScriptStatus result = ScriptStatus::kReceived;
    GetStatusOnReaderThread(kKey, &result)->Wait();
    EXPECT_EQ(ScriptStatus::kPending, result);
  }

  {
    bool result = true;
    WaitableEvent* pending_wait = WaitOnReaderThread(kKey, &result);
    // This should not be signaled until OnAllDataAdded is called.
    EXPECT_FALSE(pending_wait->IsSignaled());
    WaitableEvent* pending_on_all_data_added = OnAllDataAddedOnWriterThread();
    pending_wait->Wait();
    pending_on_all_data_added->Wait();
    // Aborted wait should return false.
    EXPECT_FALSE(result);
  }

  {
    bool result = true;
    WaitOnReaderThread(kKey, &result)->Wait();
    // Wait fails immediately because OnAllDataAdded is called.
    EXPECT_FALSE(result);
  }
}

}  // namespace blink
