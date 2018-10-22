// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_stream_handle.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "base/test/mock_callback.h"
#include "media/audio/audio_input_delegate.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using testing::StrictMock;
using testing::Mock;
using testing::Test;

class FakeAudioInputDelegate : public media::AudioInputDelegate {
 public:
  FakeAudioInputDelegate() {}

  ~FakeAudioInputDelegate() override {}

  int GetStreamId() override { return 0; };
  void OnRecordStream() override{};
  void OnSetVolume(double volume) override{};
  void OnSetOutputDeviceForAec(const std::string& output_device_id) override{};

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAudioInputDelegate);
};

class MockRendererAudioInputStreamFactoryClient
    : public mojom::RendererAudioInputStreamFactoryClient {
 public:
  MOCK_METHOD0(Created, void());

  void StreamCreated(
      media::mojom::AudioInputStreamPtr input_stream,
      media::mojom::AudioInputStreamClientRequest client_request,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
      bool initially_muted,
      const base::Optional<base::UnguessableToken>& stream_id) override {
    EXPECT_TRUE(stream_id.has_value());
    input_stream_ = std::move(input_stream);
    client_request_ = std::move(client_request);
    Created();
  }

 private:
  media::mojom::AudioInputStreamPtr input_stream_;
  media::mojom::AudioInputStreamClientRequest client_request_;
};

using MockDeleter =
    base::MockCallback<base::OnceCallback<void(AudioInputStreamHandle*)>>;

// Creates a fake delegate and saves the provided event handler in
// |event_handler_out|.
std::unique_ptr<media::AudioInputDelegate> CreateFakeDelegate(
    media::AudioInputDelegate::EventHandler** event_handler_out,
    media::AudioInputDelegate::EventHandler* event_handler) {
  *event_handler_out = event_handler;
  return std::make_unique<FakeAudioInputDelegate>();
}

}  // namespace

class AudioInputStreamHandleTest : public Test {
 public:
  AudioInputStreamHandleTest()
      : client_binding_(&client_, mojo::MakeRequest(&client_ptr_)),
        handle_(std::make_unique<AudioInputStreamHandle>(
            std::move(client_ptr_),
            base::BindOnce(&CreateFakeDelegate, &event_handler_),
            deleter_.Get())),
        local_(std::make_unique<base::CancelableSyncSocket>()),
        remote_(std::make_unique<base::CancelableSyncSocket>()) {
    // Wait for |event_handler| to be set.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(event_handler_);

    const size_t kSize = 1234;
    shared_memory_region_ =
        base::ReadOnlySharedMemoryRegion::Create(kSize).region;
    EXPECT_TRUE(shared_memory_region_.IsValid());
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(local_.get(), remote_.get()));
  }

  void SendCreatedNotification() {
    const int kIrrelevantStreamId = 0;
    const bool kInitiallyMuted = false;
    event_handler_->OnStreamCreated(kIrrelevantStreamId,
                                    std::move(shared_memory_region_),
                                    std::move(remote_), kInitiallyMuted);
  }

  MockRendererAudioInputStreamFactoryClient* client() { return &client_; }

  void UnbindClientBinding() { client_binding_.Unbind(); }

  void ExpectHandleWillCallDeleter() {
    EXPECT_CALL(deleter_, Run(handle_.release()))
        .WillOnce(testing::DeleteArg<0>());
  }

  // Note: Must call ExpectHandleWillCallDeleter() first.
  void VerifyDeleterWasCalled() {
    EXPECT_TRUE(Mock::VerifyAndClear(&deleter_));
  }

 private:
  base::MessageLoop message_loop_;
  StrictMock<MockRendererAudioInputStreamFactoryClient> client_;
  mojom::RendererAudioInputStreamFactoryClientPtr client_ptr_;
  mojo::Binding<mojom::RendererAudioInputStreamFactoryClient> client_binding_;
  StrictMock<MockDeleter> deleter_;
  media::AudioInputDelegate::EventHandler* event_handler_ = nullptr;
  std::unique_ptr<AudioInputStreamHandle> handle_;

  base::ReadOnlySharedMemoryRegion shared_memory_region_;
  std::unique_ptr<base::CancelableSyncSocket> local_;
  std::unique_ptr<base::CancelableSyncSocket> remote_;
};

TEST_F(AudioInputStreamHandleTest, CreateStream) {
  EXPECT_CALL(*client(), Created());

  SendCreatedNotification();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(Mock::VerifyAndClear(client()));
}

TEST_F(AudioInputStreamHandleTest,
       DestructClientBeforeCreationFinishes_CancelsStreamCreation) {
  ExpectHandleWillCallDeleter();

  UnbindClientBinding();
  base::RunLoop().RunUntilIdle();

  VerifyDeleterWasCalled();
}

TEST_F(AudioInputStreamHandleTest,
       CreateStreamAndDisconnectClient_DestroysStream) {
  EXPECT_CALL(*client(), Created());

  SendCreatedNotification();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(Mock::VerifyAndClear(client()));

  ExpectHandleWillCallDeleter();

  UnbindClientBinding();
  base::RunLoop().RunUntilIdle();

  VerifyDeleterWasCalled();
}

}  // namespace content
