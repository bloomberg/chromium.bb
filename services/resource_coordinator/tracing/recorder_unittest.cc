// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/tracing/recorder.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

class RecorderTest : public testing::Test {
 public:
  void SetUp() override { message_loop_.reset(new base::MessageLoop()); }

  void TearDown() override {
    recorder_.reset();
    message_loop_.reset();
  }

  void CreateRecorder(mojom::RecorderRequest request,
                      bool is_array,
                      const base::Closure& callback) {
    recorder_.reset(new Recorder(std::move(request), is_array, callback,
                                 base::ThreadTaskRunnerHandle::Get()));
  }

  void CreateRecorder(bool is_array, const base::Closure& callback) {
    CreateRecorder(nullptr, is_array, callback);
  }

  void AddChunk(const std::string& chunk) { recorder_->AddChunk(chunk); }

  void AddMetadata(std::unique_ptr<base::DictionaryValue> metadata) {
    recorder_->AddMetadata(std::move(metadata));
  }

  std::unique_ptr<Recorder> recorder_;

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
};

TEST_F(RecorderTest, AddChunkArray) {
  size_t num_calls = 0;
  CreateRecorder(true /* is_array */,
                 base::BindRepeating([](size_t* num_calls) { (*num_calls)++; },
                                     base::Unretained(&num_calls)));
  AddChunk("chunk1");
  AddChunk("chunk2");
  AddChunk("chunk3");
  EXPECT_EQ("chunk1,chunk2,chunk3", recorder_->data());

  // Verify that the recorder has called the callback every time it received a
  // chunk.
  EXPECT_EQ(3u, num_calls);
}

TEST_F(RecorderTest, AddChunkString) {
  size_t num_calls = 0;
  CreateRecorder(false /* is_array */,
                 base::BindRepeating([](size_t* num_calls) { (*num_calls)++; },
                                     base::Unretained(&num_calls)));
  AddChunk("chunk1");
  AddChunk("chunk2");
  AddChunk("chunk3");
  EXPECT_EQ("chunk1chunk2chunk3", recorder_->data());
  EXPECT_EQ(3u, num_calls);
}

TEST_F(RecorderTest, AddMetadata) {
  CreateRecorder(true /* is_array */, base::BindRepeating([] {}));

  auto dict1 = base::MakeUnique<base::DictionaryValue>();
  dict1->SetString("network-type", "Ethernet");
  AddMetadata(std::move(dict1));

  auto dict2 = base::MakeUnique<base::DictionaryValue>();
  dict2->SetString("os-name", "CrOS");
  AddMetadata(std::move(dict2));

  EXPECT_EQ(2u, recorder_->metadata().size());
  std::string net;
  EXPECT_TRUE(recorder_->metadata().GetString("network-type", &net));
  EXPECT_EQ("Ethernet", net);
  std::string os;
  EXPECT_TRUE(recorder_->metadata().GetString("os-name", &os));
  EXPECT_EQ("CrOS", os);
}

TEST_F(RecorderTest, OnConnectionError) {
  base::RunLoop run_loop;
  size_t num_calls = 0;
  {
    mojom::RecorderPtr ptr;
    auto request = MakeRequest(&ptr);
    CreateRecorder(std::move(request), false /* is_array */,
                   base::BindRepeating(
                       [](size_t* num_calls, base::Closure quit_closure) {
                         (*num_calls)++;
                         quit_closure.Run();
                       },
                       base::Unretained(&num_calls), run_loop.QuitClosure()));
  }
  // |ptr| is deleted at this point and so the recorder should notify us that
  // the client is not going to send any more data by running the callback.
  run_loop.Run();
  EXPECT_EQ(1u, num_calls);
}

}  // namespace tracing
