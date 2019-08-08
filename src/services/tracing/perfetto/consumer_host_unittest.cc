// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/consumer_host.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

namespace tracing {

constexpr base::ProcessId kProducerPid = 1234;

// This is here so we can properly simulate this running on three
// different sequences (ProducerClient side, Service side, and
// whatever connects via Mojo to the Producer). This is needed
// so we don't get into read/write locks.
class ThreadedPerfettoService : public mojom::TracingSession {
 public:
  ThreadedPerfettoService()
      : task_runner_(base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
             base::WithBaseSyncPrimitives(),
             base::TaskPriority::BEST_EFFORT})) {
    perfetto_service_ = std::make_unique<PerfettoService>(task_runner_);
    base::RunLoop wait_for_construct;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::CreateConsumerOnSequence,
                       base::Unretained(this)),
        wait_for_construct.QuitClosure());
    wait_for_construct.Run();
  }

  ~ThreadedPerfettoService() override {
    if (binding_) {
      task_runner_->DeleteSoon(FROM_HERE, std::move(binding_));
    }

    task_runner_->DeleteSoon(FROM_HERE, std::move(producer_));
    if (consumer_) {
      task_runner_->DeleteSoon(FROM_HERE, std::move(consumer_));
    }

    task_runner_->DeleteSoon(FROM_HERE, std::move(perfetto_service_));

    {
      base::RunLoop wait_for_destruction;
      task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                     wait_for_destruction.QuitClosure());
      wait_for_destruction.Run();
    }

    {
      base::RunLoop wait_for_destruction;
      ProducerClient::GetTaskRunner()->task_runner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), wait_for_destruction.QuitClosure());
      wait_for_destruction.Run();
    }
  }

  // mojom::TracingSession implementation:
  void OnTracingEnabled() override {
    EXPECT_FALSE(tracing_enabled_);
    tracing_enabled_ = true;
  }

  void CreateProducer(const std::string& data_source_name,
                      size_t num_packets,
                      base::OnceClosure on_tracing_started) {
    base::RunLoop wait_for_datasource_registration;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::CreateProducerOnSequence,
                       base::Unretained(this), data_source_name,
                       wait_for_datasource_registration.QuitClosure(),
                       std::move(on_tracing_started), num_packets));
    wait_for_datasource_registration.Run();
  }

  void CreateConsumerOnSequence() {
    consumer_ = std::make_unique<ConsumerHost>(perfetto_service_.get());
  }

  void CreateProducerOnSequence(const std::string& data_source_name,
                                base::OnceClosure on_datasource_registered,
                                base::OnceClosure on_tracing_started,
                                size_t num_packets) {
    producer_ = std::make_unique<MockProducer>(
        base::StrCat({mojom::kPerfettoProducerNamePrefix,
                      base::NumberToString(kProducerPid)}),
        data_source_name, perfetto_service_->GetService(),
        std::move(on_datasource_registered), std::move(on_tracing_started),
        num_packets);
  }

  void EnableTracingWithConfig(const perfetto::TraceConfig& config) {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::EnableTracingOnSequence,
                       base::Unretained(this), std::move(config)),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  void EnableTracingOnSequence(const perfetto::TraceConfig& config) {
    tracing::mojom::TracingSessionPtr tracing_session;

    binding_ = std::make_unique<mojo::Binding<mojom::TracingSession>>(
        this, mojo::MakeRequest(&tracing_session));

    consumer_->EnableTracing(std::move(tracing_session), std::move(config));
  }

  void ReadBuffers(mojo::ScopedDataPipeProducerHandle stream,
                   ConsumerHost::ReadBuffersCallback callback) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ConsumerHost::ReadBuffers,
                                  base::Unretained(consumer_.get()),
                                  std::move(stream), std::move(callback)));
  }

  void FreeBuffers() {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ConsumerHost::FreeBuffers,
                       base::Unretained(consumer_.get())),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  void DisableTracing() {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ConsumerHost::DisableTracing,
                       base::Unretained(consumer_.get())),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  void WritePacketBigly() {
    base::RunLoop wait_for_call;
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&MockProducer::WritePacketBigly,
                                          base::Unretained(producer_.get()),
                                          wait_for_call.QuitClosure()));
    wait_for_call.Run();
  }

  void Flush(base::OnceClosure on_flush_complete) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ConsumerHost::Flush, base::Unretained(consumer_.get()),
                       10000u,
                       base::BindOnce(
                           [](base::OnceClosure callback, bool success) {
                             EXPECT_TRUE(success);
                             std::move(callback).Run();
                           },
                           std::move(on_flush_complete))));
  }

  void ExpectPid(base::ProcessId pid) {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&PerfettoService::AddActiveServicePid,
                       base::Unretained(perfetto_service_.get()), pid),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  void SetPidsInitialized() {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&PerfettoService::SetActiveServicePidsInitialized,
                       base::Unretained(perfetto_service_.get())),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  void RemovePid(base::ProcessId pid) {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&PerfettoService::RemoveActiveServicePid,
                       base::Unretained(perfetto_service_.get()), pid),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  bool IsTracingEnabled() const {
    bool tracing_enabled;
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::GetTracingEnabledOnSequence,
                       base::Unretained(this), &tracing_enabled),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
    return tracing_enabled;
  }

  void GetTracingEnabledOnSequence(bool* tracing_enabled) const {
    *tracing_enabled = tracing_enabled_;
  }

  perfetto::DataSourceConfig GetProducerClientConfig() {
    perfetto::DataSourceConfig config;
    base::RunLoop wait_loop;
    task_runner_->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          config = producer_->producer_client()->data_source()->config();
        }),
        wait_loop.QuitClosure());
    wait_loop.Run();
    return config;
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<PerfettoService> perfetto_service_;
  std::unique_ptr<ConsumerHost> consumer_;
  std::unique_ptr<MockProducer> producer_;
  std::unique_ptr<mojo::Binding<mojom::TracingSession>> binding_;
  bool tracing_enabled_ = false;
};

class TracingConsumerTest : public testing::Test,
                            public mojo::DataPipeDrainer::Client {
 public:
  void SetUp() override {
    ProducerClient::ResetTaskRunnerForTesting();
    threaded_service_ = std::make_unique<ThreadedPerfettoService>();

    matching_packet_count_ = 0;
    total_bytes_received_ = 0;
  }

  void TearDown() override { threaded_service_.reset(); }

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    total_bytes_received_ += num_bytes;
    std::copy(static_cast<const uint8_t*>(data),
              static_cast<const uint8_t*>(data) + num_bytes,
              std::back_inserter(received_data_));
  }

  // mojo::DataPipeDrainer::Client
  void OnDataComplete() override {
    auto proto = std::make_unique<perfetto::protos::Trace>();
    EXPECT_TRUE(
        proto->ParseFromArray(received_data_.data(), received_data_.size()));

    for (int i = 0; i < proto->packet_size(); ++i) {
      if (proto->packet(i).for_testing().str() == packet_testing_str_) {
        matching_packet_count_++;
      }
    }

    if (on_data_complete_) {
      std::move(on_data_complete_).Run();
    }
  }

  void ExpectPackets(const std::string& testing_str,
                     base::OnceClosure on_data_complete) {
    on_data_complete_ = std::move(on_data_complete);
    packet_testing_str_ = testing_str;
    matching_packet_count_ = 0;
  }

  void ReadBuffers() {
    mojo::DataPipe data_pipe;
    threaded_service_->ReadBuffers(std::move(data_pipe.producer_handle),
                                   base::OnceClosure());
    drainer_.reset(
        new mojo::DataPipeDrainer(this, std::move(data_pipe.consumer_handle)));
  }

  perfetto::TraceConfig GetDefaultTraceConfig(
      const std::string& data_source_name) {
    perfetto::TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(32 * 1024);

    auto* trace_event_config =
        trace_config.add_data_sources()->mutable_config();
    trace_event_config->set_name(data_source_name);
    trace_event_config->set_target_buffer(0);

    return trace_config;
  }

  void EnableTracingWithDataSourceName(const std::string& data_source_name,
                                       bool enable_privacy_filtering = false) {
    perfetto::TraceConfig config = GetDefaultTraceConfig(data_source_name);
    if (enable_privacy_filtering) {
      for (auto& source : *config.mutable_data_sources()) {
        source.mutable_config()
            ->mutable_chrome_config()
            ->set_privacy_filtering_enabled(true);
      }
    }
    threaded_service_->EnableTracingWithConfig(config);
  }

  bool IsTracingEnabled() {
    // Flush any other pending tasks on the perfetto task runner to ensure that
    // any pending data source start callbacks have propagated.
    scoped_task_environment_.RunUntilIdle();

    return threaded_service_->IsTracingEnabled();
  }

  size_t matching_packet_count() const { return matching_packet_count_; }
  size_t total_bytes_received() const { return total_bytes_received_; }
  ThreadedPerfettoService* threaded_perfetto_service() const {
    return threaded_service_.get();
  }

 private:
  std::unique_ptr<ThreadedPerfettoService> threaded_service_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::OnceClosure on_data_complete_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  std::vector<uint8_t> received_data_;
  std::string packet_testing_str_;
  size_t matching_packet_count_ = 0;
  size_t total_bytes_received_ = 0;
};

TEST_F(TracingConsumerTest, EnableAndDisableTracing) {
  EnableTracingWithDataSourceName(mojom::kTraceEventDataSourceName);

  base::RunLoop no_more_data;
  ExpectPackets(kPerfettoTestString, no_more_data.QuitClosure());

  threaded_perfetto_service()->DisableTracing();
  ReadBuffers();

  no_more_data.Run();

  EXPECT_EQ(0u, matching_packet_count());
}

TEST_F(TracingConsumerTest, ReceiveTestPackets) {
  EnableTracingWithDataSourceName(mojom::kTraceEventDataSourceName);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      mojom::kTraceEventDataSourceName, 10u,
      wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();

  base::RunLoop no_more_data;
  ExpectPackets(kPerfettoTestString, no_more_data.QuitClosure());

  threaded_perfetto_service()->DisableTracing();
  ReadBuffers();

  no_more_data.Run();

  EXPECT_EQ(10u, matching_packet_count());
}

TEST_F(TracingConsumerTest, FlushProducers) {
  EnableTracingWithDataSourceName(mojom::kTraceEventDataSourceName);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      mojom::kTraceEventDataSourceName, 10u,
      wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();

  base::RunLoop wait_for_packets;
  ExpectPackets(kPerfettoTestString, wait_for_packets.QuitClosure());

  base::RunLoop wait_for_flush;
  threaded_perfetto_service()->Flush(wait_for_flush.QuitClosure());
  ReadBuffers();

  wait_for_flush.Run();
  wait_for_packets.Run();

  EXPECT_EQ(10u, matching_packet_count());

  threaded_perfetto_service()->FreeBuffers();
}

TEST_F(TracingConsumerTest, LargeDataSize) {
  EnableTracingWithDataSourceName(mojom::kTraceEventDataSourceName);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      mojom::kTraceEventDataSourceName, 0u,
      wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();

  base::RunLoop no_more_data;
  ExpectPackets(kPerfettoTestString, no_more_data.QuitClosure());

  threaded_perfetto_service()->WritePacketBigly();

  threaded_perfetto_service()->DisableTracing();
  ReadBuffers();

  no_more_data.Run();

  EXPECT_GE(total_bytes_received(), kLargeMessageSize);
}

TEST_F(TracingConsumerTest, NotifiesOnTracingEnabled) {
  threaded_perfetto_service()->SetPidsInitialized();

  EnableTracingWithDataSourceName(mojom::kTraceEventDataSourceName);
  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest, NotifiesOnTracingEnabledWaitsForProducer) {
  threaded_perfetto_service()->ExpectPid(kProducerPid);
  threaded_perfetto_service()->SetPidsInitialized();

  EnableTracingWithDataSourceName(mojom::kTraceEventDataSourceName);

  // Tracing is only marked as enabled once the expected producer has acked that
  // its data source has started.
  EXPECT_FALSE(IsTracingEnabled());

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      mojom::kTraceEventDataSourceName, 0u,
      wait_for_tracing_start.QuitClosure());
  wait_for_tracing_start.Run();

  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest, NotifiesOnTracingEnabledWaitsForFilteredProducer) {
  threaded_perfetto_service()->ExpectPid(kProducerPid);
  threaded_perfetto_service()->SetPidsInitialized();

  // Filter for the expected producer.
  auto config = GetDefaultTraceConfig(mojom::kTraceEventDataSourceName);
  *config.mutable_data_sources()->front().add_producer_name_filter() =
      base::StrCat({mojom::kPerfettoProducerNamePrefix,
                    base::NumberToString(kProducerPid)});
  threaded_perfetto_service()->EnableTracingWithConfig(config);

  // Tracing is only marked as enabled once the expected producer has acked that
  // its data source has started.
  EXPECT_FALSE(IsTracingEnabled());

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      mojom::kTraceEventDataSourceName, 0u,
      wait_for_tracing_start.QuitClosure());
  wait_for_tracing_start.Run();

  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest,
       NotifiesOnTracingEnabledDoesNotWaitForUnfilteredProducer) {
  threaded_perfetto_service()->ExpectPid(kProducerPid);
  threaded_perfetto_service()->SetPidsInitialized();

  // Filter for an unexpected producer whose PID is not active.
  auto config = GetDefaultTraceConfig(mojom::kTraceEventDataSourceName);
  *config.mutable_data_sources()->front().add_producer_name_filter() =
      base::StrCat({mojom::kPerfettoProducerNamePrefix,
                    base::NumberToString(kProducerPid + 1)});
  threaded_perfetto_service()->EnableTracingWithConfig(config);

  // Tracing should already have been enabled even though the host was told
  // about a service with kProducerPid. Since kProducerPid is not included in
  // the producer_name_filter, the host should not wait for it.
  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest,
       NotifiesOnTracingEnabledWaitsForProducerAndInitializedPids) {
  threaded_perfetto_service()->ExpectPid(kProducerPid);

  EnableTracingWithDataSourceName(mojom::kTraceEventDataSourceName);

  // Tracing is only marked as enabled once the expected producer has acked that
  // its data source has started and once the PIDs are initialized.
  EXPECT_FALSE(IsTracingEnabled());

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      mojom::kTraceEventDataSourceName, 0u,
      wait_for_tracing_start.QuitClosure());
  wait_for_tracing_start.Run();

  EXPECT_FALSE(IsTracingEnabled());

  threaded_perfetto_service()->SetPidsInitialized();
  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest, PrivacyFilterConfig) {
  EnableTracingWithDataSourceName(mojom::kTraceEventDataSourceName,
                                  /* enable_privacy_filtering =*/true);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      mojom::kTraceEventDataSourceName, 10u,
      wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();
  EXPECT_TRUE(threaded_perfetto_service()
                  ->GetProducerClientConfig()
                  .chrome_config()
                  .privacy_filtering_enabled());
}

}  // namespace tracing
