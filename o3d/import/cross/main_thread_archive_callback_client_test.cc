/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
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


// Tests functionality of the MainThreadArchiveCallbackClient class

#include "tests/common/win/testing_common.h"
#include "base/task.h"
#include "core/cross/imain_thread_task_poster.h"
#include "core/cross/service_implementation.h"
#include "import/cross/main_thread_archive_callback_client.h"

namespace o3d {

// Simple standin for IMainThreadTaskPoster that invokes tasks asynchronously.
class TestMainThreadTaskPoster : IMainThreadTaskPoster {
 public:
  TestMainThreadTaskPoster(ServiceLocator* service_locator)
      : service_(service_locator, this) {
  }

  virtual bool IsSupported() {
    return true;
  }

  virtual void PostTask(Task* task) {
    tasks_.push_back(task);
  }

  void Flush() {
    for (size_t i = 0; i < tasks_.size(); ++i) {
      tasks_[i]->Run();
      delete tasks_[i];
    }
    tasks_.clear();
  }

 private:
  ServiceImplementation<IMainThreadTaskPoster> service_;
  std::vector<Task*> tasks_;
};

class MockArchiveCallbackClient : public ArchiveCallbackClient {
 public:
  MockArchiveCallbackClient()
      : receive_file_header_calls_(0),
        receive_file_header_calls_file_info_("", 0),
        receive_file_data_calls_(0),
        receive_file_data_stream_(NULL),
        receive_file_data_size_(0),
        receive_file_data_result_(false),
        close_calls_(0),
        close_success_(false) {
  }

  virtual void ReceiveFileHeader(const ArchiveFileInfo &file_info) {
    ++receive_file_header_calls_;
    receive_file_header_calls_file_info_ = file_info;
  }

  virtual bool ReceiveFileData(MemoryReadStream* stream, size_t size) {
    ++receive_file_data_calls_;
    receive_file_data_stream_.reset(new uint8[size]);
    stream->Read(receive_file_data_stream_.get(), size);
    receive_file_data_size_ = size;
    return receive_file_data_result_;
  }

  virtual void Close(bool success) {
    ++close_calls_;
    close_success_ = success;
  }

  int receive_file_header_calls_;
  ArchiveFileInfo receive_file_header_calls_file_info_;

  int receive_file_data_calls_;
  scoped_array<uint8> receive_file_data_stream_;
  size_t receive_file_data_size_;
  bool receive_file_data_result_;

  int close_calls_;
  bool close_success_;
};

class MainThreadArchiveCallbackClientTest : public testing::Test {
 public:
  MainThreadArchiveCallbackClientTest()
      : main_thread_task_poster_(&service_locator_) {
  }

 protected:
  virtual void SetUp() {
    receiver_client_ = new MockArchiveCallbackClient;
    main_thread_client_ = new MainThreadArchiveCallbackClient(
        &service_locator_, receiver_client_);
  }

  virtual void TearDown() {
    delete receiver_client_;
    delete main_thread_client_;
  }

  ServiceLocator service_locator_;
  TestMainThreadTaskPoster main_thread_task_poster_;
  MockArchiveCallbackClient* receiver_client_;
  MainThreadArchiveCallbackClient* main_thread_client_;
};

TEST_F(MainThreadArchiveCallbackClientTest,
       ReceiveFileHeaderForwardsToReceiver) {
  ArchiveFileInfo info("hello", 7);
  main_thread_client_->ReceiveFileHeader(info);
  main_thread_task_poster_.Flush();

  EXPECT_EQ(1, receiver_client_->receive_file_header_calls_);
  EXPECT_EQ("hello",
      receiver_client_->receive_file_header_calls_file_info_.GetFileName());
  EXPECT_EQ(7,
      receiver_client_->receive_file_header_calls_file_info_.GetFileSize());
}

TEST_F(MainThreadArchiveCallbackClientTest,
       ReceiveFileDataForwardsToReceiverAndReturnsTrueFirstTime) {
  uint8 buffer[] = {1, 2, 3};
  MemoryReadStream stream(buffer, sizeof(buffer));
  receiver_client_->receive_file_data_result_ = true;
  EXPECT_TRUE(main_thread_client_->ReceiveFileData(&stream, sizeof(buffer)));
  main_thread_task_poster_.Flush();

  EXPECT_EQ(1, receiver_client_->receive_file_data_calls_);
  EXPECT_EQ(0, memcmp(receiver_client_->receive_file_data_stream_.get(),
                      buffer, sizeof(buffer)));
  EXPECT_EQ(sizeof(buffer), receiver_client_->receive_file_data_size_);
}

TEST_F(MainThreadArchiveCallbackClientTest,
       ReceiveFileDataReportsFailureInSubsequentCall) {
  receiver_client_->receive_file_data_result_ = true;

  uint8 buffer[] = {1, 2, 3, 4, 5, 6};
  MemoryReadStream stream(buffer, sizeof(buffer));

  receiver_client_->receive_file_data_result_ = false;
  EXPECT_TRUE(main_thread_client_->ReceiveFileData(&stream, 3));
  main_thread_task_poster_.Flush();
  EXPECT_EQ(1, receiver_client_->receive_file_data_calls_);


  receiver_client_->receive_file_data_result_ = false;
  EXPECT_FALSE(main_thread_client_->ReceiveFileData(&stream, 3));
  main_thread_task_poster_.Flush();
  EXPECT_EQ(1, receiver_client_->receive_file_data_calls_);
}

TEST_F(MainThreadArchiveCallbackClientTest,
       ReceiveFileHeaderForwardsUnsuccessfulClose) {
  main_thread_client_->Close(false);
  main_thread_task_poster_.Flush();

  EXPECT_EQ(1, receiver_client_->close_calls_);
  EXPECT_FALSE(receiver_client_->close_success_);
}

TEST_F(MainThreadArchiveCallbackClientTest,
       ReceiveFileHeaderForwardsSuccessfulClose) {
  main_thread_client_->Close(true);
  main_thread_task_poster_.Flush();

  EXPECT_EQ(1, receiver_client_->close_calls_);
  EXPECT_TRUE(receiver_client_->close_success_);
}

TEST_F(MainThreadArchiveCallbackClientTest,
       CloseForwardsFailureOfPreviousUnreportedCall) {
  receiver_client_->receive_file_data_result_ = true;

  uint8 buffer[] = {1, 2, 3};
  MemoryReadStream stream(buffer, sizeof(buffer));

  receiver_client_->receive_file_data_result_ = false;
  EXPECT_TRUE(main_thread_client_->ReceiveFileData(&stream, sizeof(buffer)));
  main_thread_task_poster_.Flush();
  EXPECT_EQ(1, receiver_client_->receive_file_data_calls_);

  main_thread_client_->Close(true);
  main_thread_task_poster_.Flush();
  EXPECT_FALSE(receiver_client_->close_success_);
}

}  // namespace o3d
