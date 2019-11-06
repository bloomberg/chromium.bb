// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mime_sniffing_throttle.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/common/mime_sniffing_url_loader.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace content {

namespace {

class MojoDataPipeSender {
 public:
  MojoDataPipeSender(mojo::ScopedDataPipeProducerHandle handle)
      : handle_(std::move(handle)),
        watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC) {}

  void Start(std::string data, base::OnceClosure done_callback) {
    data_ = std::move(data);
    done_callback_ = std::move(done_callback);
    watcher_.Watch(handle_.get(),
                   MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                   base::BindRepeating(&MojoDataPipeSender::OnWritable,
                                       base::Unretained(this)));
  }

  void OnWritable(MojoResult) {
    uint32_t sending_bytes = data_.size() - sent_bytes_;
    MojoResult result = handle_->WriteData(
        data_.c_str() + sent_bytes_, &sending_bytes, MOJO_WRITE_DATA_FLAG_NONE);
    switch (result) {
      case MOJO_RESULT_OK:
        break;
      case MOJO_RESULT_FAILED_PRECONDITION:
        // Finished unexpectedly.
        std::move(done_callback_).Run();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        // Just wait until OnWritable() is called by the watcher.
        return;
      default:
        NOTREACHED();
        return;
    }
    sent_bytes_ += sending_bytes;
    if (data_.size() == sent_bytes_)
      std::move(done_callback_).Run();
  }

  mojo::ScopedDataPipeProducerHandle ReleaseHandle() {
    return std::move(handle_);
  }

  bool has_succeeded() const { return data_.size() == sent_bytes_; }

 private:
  mojo::ScopedDataPipeProducerHandle handle_;
  mojo::SimpleWatcher watcher_;
  base::OnceClosure done_callback_;
  std::string data_;
  uint32_t sent_bytes_ = 0;
};

class MockDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  // Implements blink::URLLoaderThrottle::Delegate.
  void CancelWithError(int error_code,
                       base::StringPiece custom_reason) override {
    NOTIMPLEMENTED();
  }
  void Resume() override {
    is_resumed_ = true;
    // Resume from OnReceiveResponse() with a customized response header.
    destination_loader_client()->OnReceiveResponse(
        updated_response_head().value());
  }

  void SetPriority(net::RequestPriority priority) override { NOTIMPLEMENTED(); }
  void UpdateDeferredResponseHead(
      const network::ResourceResponseHead& new_response_head) override {
    updated_response_head_ = new_response_head;
  }
  void PauseReadingBodyFromNet() override { NOTIMPLEMENTED(); }
  void ResumeReadingBodyFromNet() override { NOTIMPLEMENTED(); }
  void InterceptResponse(
      network::mojom::URLLoaderPtr new_loader,
      network::mojom::URLLoaderClientRequest new_client_request,
      network::mojom::URLLoaderPtr* original_loader,
      network::mojom::URLLoaderClientRequest* original_client_request)
      override {
    is_intercepted_ = true;

    destination_loader_ptr_ = std::move(new_loader);
    ASSERT_TRUE(mojo::FuseInterface(
        std::move(new_client_request),
        destination_loader_client_.CreateInterfacePtr().PassInterface()));
    source_loader_request_ = mojo::MakeRequest(original_loader);
    *original_client_request = mojo::MakeRequest(&source_loader_client_ptr_);
  }

  void LoadResponseBody(const std::string& body) {
    if (!source_body_handle_.is_valid()) {
      // Send OnStartLoadingResponseBody() if it's the first call.
      mojo::ScopedDataPipeConsumerHandle consumer;
      EXPECT_EQ(MOJO_RESULT_OK,
                mojo::CreateDataPipe(nullptr, &source_body_handle_, &consumer));
      source_loader_client()->OnStartLoadingResponseBody(std::move(consumer));
    }

    MojoDataPipeSender sender(std::move(source_body_handle_));
    base::RunLoop loop;
    sender.Start(body, loop.QuitClosure());
    loop.Run();

    EXPECT_TRUE(sender.has_succeeded());
    source_body_handle_ = sender.ReleaseHandle();
  }

  void CompleteResponse() {
    source_loader_client()->OnComplete(network::URLLoaderCompletionStatus());
    source_body_handle_.reset();
  }

  uint32_t ReadResponseBody(uint32_t size) {
    std::vector<uint8_t> buffer(size);
    MojoResult result = destination_loader_client_.response_body().ReadData(
        buffer.data(), &size, MOJO_READ_DATA_FLAG_NONE);
    switch (result) {
      case MOJO_RESULT_OK:
        return size;
      case MOJO_RESULT_FAILED_PRECONDITION:
        return 0;
      case MOJO_RESULT_SHOULD_WAIT:
        return 0;
      default:
        NOTREACHED();
    }
    return 0;
  }

  bool is_intercepted() const { return is_intercepted_; }
  bool is_resumed() const { return is_resumed_; }

  const base::Optional<network::ResourceResponseHead>& updated_response_head()
      const {
    return updated_response_head_;
  }

  network::TestURLLoaderClient* destination_loader_client() {
    return &destination_loader_client_;
  }

  network::mojom::URLLoaderClient* source_loader_client() {
    return source_loader_client_ptr_.get();
  }

 private:
  bool is_intercepted_ = false;
  bool is_resumed_ = false;
  base::Optional<network::ResourceResponseHead> updated_response_head_;

  // A pair of a loader and a loader client for destination of the response.
  network::mojom::URLLoaderPtr destination_loader_ptr_;
  network::TestURLLoaderClient destination_loader_client_;

  // A pair of a loader and a loader client for source of the response.
  network::mojom::URLLoaderClientPtr source_loader_client_ptr_;
  network::mojom::URLLoaderRequest source_loader_request_;

  mojo::ScopedDataPipeProducerHandle source_body_handle_;
};

}  // namespace

class MimeSniffingThrottleTest : public testing::Test {
 protected:
  // Be the first member so it is destroyed last.
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MimeSniffingThrottleTest, NoMimeTypeWithSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  network::ResourceResponseHead response_head;
  bool defer = false;
  throttle->WillProcessResponse(GURL("https://example.com"), &response_head,
                                &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, SniffableMimeTypeWithSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  network::ResourceResponseHead response_head;
  response_head.mime_type = "text/plain";
  bool defer = false;
  throttle->WillProcessResponse(GURL("https://example.com"), &response_head,
                                &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, NotSniffableMimeTypeWithSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  network::ResourceResponseHead response_head;
  response_head.mime_type = "text/javascript";
  bool defer = false;
  throttle->WillProcessResponse(GURL("https://example.com"), &response_head,
                                &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, NoMimeTypeWithNotSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  network::ResourceResponseHead response_head;
  bool defer = false;
  throttle->WillProcessResponse(GURL("wss://example.com"), &response_head,
                                &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, SniffableMimeTypeWithNotSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  network::ResourceResponseHead response_head;
  response_head.mime_type = "text/plain";
  bool defer = false;
  throttle->WillProcessResponse(GURL("wss://example.com"), &response_head,
                                &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, NotSniffableMimeTypeWithNotSniffableScheme) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  network::ResourceResponseHead response_head;
  response_head.mime_type = "text/javascript";
  bool defer = false;
  throttle->WillProcessResponse(GURL("wss://example.com"), &response_head,
                                &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, SniffableButAlreadySniffed) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  network::ResourceResponseHead response_head;
  response_head.mime_type = "text/plain";
  response_head.did_mime_sniff = true;
  bool defer = false;
  throttle->WillProcessResponse(GURL("https://example.com"), &response_head,
                                &defer);
  EXPECT_FALSE(defer);
  EXPECT_FALSE(delegate->is_intercepted());
}

TEST_F(MimeSniffingThrottleTest, NoBody) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com");
  network::ResourceResponseHead response_head;
  bool defer = false;
  throttle->WillProcessResponse(response_url, &response_head, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Call OnComplete() without sending body.
  delegate->source_loader_client()->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_FAILED));
  delegate->destination_loader_client()->RunUntilComplete();

  // The mime type should be updated to the default mime type ("text/plain").
  EXPECT_TRUE(delegate->destination_loader_client()->has_received_response());
  EXPECT_EQ("text/plain",
            delegate->destination_loader_client()->response_head().mime_type);
}

TEST_F(MimeSniffingThrottleTest, EmptyBody) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com");
  network::ResourceResponseHead response_head;
  bool defer = false;
  throttle->WillProcessResponse(response_url, &response_head, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  mojo::DataPipe pipe;
  delegate->source_loader_client()->OnStartLoadingResponseBody(
      std::move(pipe.consumer_handle));
  pipe.producer_handle.reset();  // The pipe is empty.

  delegate->source_loader_client()->OnComplete(
      network::URLLoaderCompletionStatus());
  delegate->destination_loader_client()->RunUntilComplete();

  // The mime type should be updated to the default mime type ("text/plain").
  EXPECT_TRUE(delegate->destination_loader_client()->has_received_response());
  EXPECT_EQ("text/plain",
            delegate->destination_loader_client()->response_head().mime_type);
}

TEST_F(MimeSniffingThrottleTest, Body_PlainText) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com");
  network::ResourceResponseHead response_head;
  bool defer = false;
  throttle->WillProcessResponse(response_url, &response_head, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Send the body and complete the response.
  delegate->LoadResponseBody("This is a text.");
  delegate->CompleteResponse();
  delegate->destination_loader_client()->RunUntilComplete();

  // The mime type should be updated.
  EXPECT_TRUE(delegate->is_resumed());
  EXPECT_EQ("text/plain",
            delegate->destination_loader_client()->response_head().mime_type);
}

TEST_F(MimeSniffingThrottleTest, Body_Docx) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com/hogehoge.docx");
  network::ResourceResponseHead response_head;
  bool defer = false;
  throttle->WillProcessResponse(response_url, &response_head, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Send the body and complete the response.
  delegate->LoadResponseBody("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1");
  delegate->CompleteResponse();
  delegate->destination_loader_client()->RunUntilComplete();

  // The mime type should be updated.
  EXPECT_TRUE(delegate->is_resumed());
  EXPECT_EQ("application/msword",
            delegate->destination_loader_client()->response_head().mime_type);
}

TEST_F(MimeSniffingThrottleTest, Body_PNG) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com/hogehoge.docx");
  network::ResourceResponseHead response_head;
  bool defer = false;
  throttle->WillProcessResponse(response_url, &response_head, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Send the body and complete the response.
  delegate->LoadResponseBody("\x89PNG\x0D\x0A\x1A\x0A");
  delegate->CompleteResponse();
  delegate->destination_loader_client()->RunUntilComplete();

  // The mime type should be updated.
  EXPECT_TRUE(delegate->is_resumed());
  EXPECT_EQ("image/png",
            delegate->destination_loader_client()->response_head().mime_type);
}

TEST_F(MimeSniffingThrottleTest, Body_LongPlainText) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com");
  network::ResourceResponseHead response_head;
  bool defer = false;
  throttle->WillProcessResponse(response_url, &response_head, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // 64KiB is coming from the default value used in
  // mojo::core::Core::CreateDataPipe().
  const uint32_t kDefaultDataPipeBufferSize = 64 * 1024;
  std::string long_body(kDefaultDataPipeBufferSize * 2, 'x');

  // Send the data to the MimeSniffingURLLoader.
  // |delegate|'s MojoDataPipeSender sends the first
  // |kDefaultDataPipeBufferSize| bytes to MimeSniffingURLLoader and
  // MimeSniffingURLLoader will read the first |kDefaultDataPipeBufferSize|
  // bytes of the body, so the MojoDataPipeSender can push the rest of
  // |kDefaultDataPipeBufferSize| of the body soon and finishes sending the
  // body. After this, MimeSniffingURLLoader is waiting to push the body to the
  // destination data pipe since the pipe should be full until it's read.
  delegate->LoadResponseBody(long_body);
  task_environment_.RunUntilIdle();

  // Send OnComplete() to the MimeSniffingURLLoader.
  delegate->CompleteResponse();
  task_environment_.RunUntilIdle();
  // MimeSniffingURLLoader should not send OnComplete() to the destination
  // client until it finished writing all the data.
  EXPECT_FALSE(
      delegate->destination_loader_client()->has_received_completion());

  // Read the half of the body. This unblocks MimeSniffingURLLoader to push the
  // rest of the body to the data pipe.
  uint32_t read_bytes = delegate->ReadResponseBody(long_body.size() / 2);
  task_environment_.RunUntilIdle();

  // Read the rest of the body.
  read_bytes += delegate->ReadResponseBody(long_body.size() / 2);
  task_environment_.RunUntilIdle();
  delegate->destination_loader_client()->RunUntilComplete();

  // Check if all data has been read.
  EXPECT_EQ(long_body.size(), read_bytes);

  // The mime type should be updated.
  EXPECT_TRUE(delegate->is_resumed());
  EXPECT_EQ("text/plain",
            delegate->destination_loader_client()->response_head().mime_type);
}

TEST_F(MimeSniffingThrottleTest, Abort_NoBodyPipe) {
  auto throttle = std::make_unique<MimeSniffingThrottle>(
      task_environment_.GetMainThreadTaskRunner());
  auto delegate = std::make_unique<MockDelegate>();
  throttle->set_delegate(delegate.get());

  GURL response_url("https://example.com");
  network::ResourceResponseHead response_head;
  bool defer = false;
  throttle->WillProcessResponse(response_url, &response_head, &defer);
  EXPECT_TRUE(defer);
  EXPECT_TRUE(delegate->is_intercepted());

  // Send the body
  std::string body = "This should be long enough to complete sniffing.";
  body.resize(1024, 'a');
  delegate->LoadResponseBody(body);
  task_environment_.RunUntilIdle();

  // Release a pipe for the body on the receiver side.
  delegate->destination_loader_client()->response_body_release();
  task_environment_.RunUntilIdle();

  // Send the body after the pipe is closed. The the loader aborts.
  delegate->LoadResponseBody("This is a text.");

  // Calling OnComplete should not crash.
  delegate->CompleteResponse();
  task_environment_.RunUntilIdle();
}

}  // namespace content
