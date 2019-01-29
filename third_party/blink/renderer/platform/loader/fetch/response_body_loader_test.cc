// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"

#include <memory>
#include <string>
#include <utility>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/testing/replaying_bytes_consumer.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

namespace blink {

namespace {

class ResponseBodyLoaderTest : public testing::Test {
 protected:
  using Command = ReplayingBytesConsumer::Command;
  class TestClient final : public GarbageCollectedFinalized<TestClient>,
                           public ResponseBodyLoaderClient {
    USING_GARBAGE_COLLECTED_MIXIN(TestClient);

   public:
    enum class Option {
      kNone,
      kAbortOnDidReceiveData,
      kSuspendOnDidReceiveData,
    };

    TestClient() : TestClient(Option::kNone) {}
    TestClient(Option option) : option_(option) {}
    virtual ~TestClient() {}

    String GetData() const { return data_; }
    bool LoadingIsFinished() { return finished_; }
    bool LoadingIsFailed() { return failed_; }

    void DidReceiveData(base::span<const char> data) override {
      DCHECK(!finished_);
      DCHECK(!failed_);
      data_.append(String(data.data(), data.size()));
      switch (option_) {
        case Option::kNone:
          break;
        case Option::kAbortOnDidReceiveData:
          loader_->Abort();
          break;
        case Option::kSuspendOnDidReceiveData:
          loader_->Suspend();
          break;
      }
    }
    void DidFinishLoadingBody() override {
      DCHECK(!finished_);
      DCHECK(!failed_);
      finished_ = true;
    }
    void DidFailLoadingBody() override {
      DCHECK(!finished_);
      DCHECK(!failed_);
      failed_ = true;
    }

    void SetLoader(ResponseBodyLoader& loader) { loader_ = loader; }
    void Trace(Visitor* visitor) override { visitor->Trace(loader_); }

   private:
    const Option option_;
    Member<ResponseBodyLoader> loader_;
    String data_;
    bool finished_ = false;
    bool failed_ = false;
  };
};

TEST_F(ResponseBodyLoaderTest, Load) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "he"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, "llo"));
  consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ(String(), client->GetData());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, LoadFailure) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "he"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, "llo"));
  consumer->Add(Command(Command::kError));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ(String(), client->GetData());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_TRUE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, LoadWithDataAndDone) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "he"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kDataAndDone, "llo"));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ(String(), client->GetData());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, Abort) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "he"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, "llo"));
  consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>(
      TestClient::Option::kAbortOnDidReceiveData);
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);
  client->SetLoader(*body_loader);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ(String(), client->GetData());
  EXPECT_FALSE(body_loader->IsAborted());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());
  EXPECT_TRUE(body_loader->IsAborted());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());
  EXPECT_TRUE(body_loader->IsAborted());
}

TEST_F(ResponseBodyLoaderTest, Suspend) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "h"));
  consumer->Add(Command(Command::kDataAndDone, "ello"));

  auto* client = MakeGarbageCollected<TestClient>(
      TestClient::Option::kSuspendOnDidReceiveData);
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);
  client->SetLoader(*body_loader);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ(String(), client->GetData());
  EXPECT_FALSE(body_loader->IsSuspended());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("h", client->GetData());
  EXPECT_TRUE(body_loader->IsSuspended());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("h", client->GetData());
  EXPECT_TRUE(body_loader->IsSuspended());

  body_loader->Resume();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("h", client->GetData());
  EXPECT_FALSE(body_loader->IsSuspended());

  task_runner->RunUntilIdle();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
  EXPECT_TRUE(body_loader->IsSuspended());

  body_loader->Resume();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
  EXPECT_FALSE(body_loader->IsSuspended());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
  EXPECT_FALSE(body_loader->IsSuspended());
}

TEST_F(ResponseBodyLoaderTest, ReadTooBigBuffer) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  constexpr auto kMax = ResponseBodyLoader::kMaxNumConsumedBytesInTask;

  consumer->Add(Command(Command::kData, std::string(kMax - 1, 'a').data()));
  consumer->Add(Command(Command::kData, std::string(2, 'b').data()));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, std::string(kMax, 'c').data()));
  consumer->Add(Command(Command::kData, std::string(kMax + 3, 'd').data()));
  consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ(String(), client->GetData());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ((std::string(kMax - 1, 'a') + 'b').data(), client->GetData());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ((std::string(kMax - 1, 'a') + "bb" + std::string(kMax, 'c') +
             std::string(kMax + 3, 'd'))
                .data(),
            client->GetData());
}

TEST_F(ResponseBodyLoaderTest, NotDrainable) {
  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<ReplayingBytesConsumer>(task_runner);
  consumer->Add(Command(Command::kData, "he"));
  consumer->Add(Command(Command::kWait));
  consumer->Add(Command(Command::kData, "llo"));
  consumer->Add(Command(Command::kDone));

  auto* client = MakeGarbageCollected<TestClient>();
  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  ResponseBodyLoaderClient* intermediate_client = nullptr;
  auto data_pipe = body_loader->DrainAsDataPipe(&intermediate_client);

  ASSERT_FALSE(data_pipe);
  EXPECT_FALSE(intermediate_client);
  EXPECT_FALSE(body_loader->IsDrained());

  // We can start loading.

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ(String(), client->GetData());

  body_loader->Start();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("he", client->GetData());

  task_runner->RunUntilIdle();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("hello", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, DrainAsDataPipe) {
  mojo::ScopedDataPipeConsumerHandle consumer_end;
  mojo::ScopedDataPipeProducerHandle producer_end;
  auto result = mojo::CreateDataPipe(nullptr, &producer_end, &consumer_end);

  ASSERT_EQ(result, MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(consumer_end), &completion_notifier);
  auto* client = MakeGarbageCollected<TestClient>();

  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  ResponseBodyLoaderClient* client_for_draining = nullptr;
  auto data_pipe = body_loader->DrainAsDataPipe(&client_for_draining);

  ASSERT_TRUE(data_pipe);
  ASSERT_TRUE(client);
  EXPECT_TRUE(body_loader->IsDrained());

  client_for_draining->DidReceiveData(base::make_span("xyz", 3));
  client_for_draining->DidReceiveData(base::make_span("abc", 3));

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("xyzabc", client->GetData());

  client_for_draining->DidFinishLoadingBody();

  EXPECT_TRUE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("xyzabc", client->GetData());
}

TEST_F(ResponseBodyLoaderTest, DrainAsDataPipeAndReportError) {
  mojo::ScopedDataPipeConsumerHandle consumer_end;
  mojo::ScopedDataPipeProducerHandle producer_end;
  auto result = mojo::CreateDataPipe(nullptr, &producer_end, &consumer_end);

  ASSERT_EQ(result, MOJO_RESULT_OK);

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* consumer = MakeGarbageCollected<DataPipeBytesConsumer>(
      task_runner, std::move(consumer_end), &completion_notifier);
  auto* client = MakeGarbageCollected<TestClient>();

  auto* body_loader =
      MakeGarbageCollected<ResponseBodyLoader>(*consumer, *client, task_runner);

  ResponseBodyLoaderClient* client_for_draining = nullptr;
  auto data_pipe = body_loader->DrainAsDataPipe(&client_for_draining);

  ASSERT_TRUE(data_pipe);
  ASSERT_TRUE(client);
  EXPECT_TRUE(body_loader->IsDrained());

  client_for_draining->DidReceiveData(base::make_span("xyz", 3));
  client_for_draining->DidReceiveData(base::make_span("abc", 3));

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_FALSE(client->LoadingIsFailed());
  EXPECT_EQ("xyzabc", client->GetData());

  client_for_draining->DidFailLoadingBody();

  EXPECT_FALSE(client->LoadingIsFinished());
  EXPECT_TRUE(client->LoadingIsFailed());
  EXPECT_EQ("xyzabc", client->GetData());
}

}  // namespace

}  // namespace blink
