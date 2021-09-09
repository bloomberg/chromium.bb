/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/tracing/core/trace_writer_impl.h"

#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/tracing/core/commit_data_request.h"
#include "perfetto/ext/tracing/core/trace_writer.h"
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "src/base/test/gtest_test_suite.h"
#include "src/base/test/test_task_runner.h"
#include "src/tracing/core/shared_memory_arbiter_impl.h"
#include "src/tracing/test/aligned_buffer_test.h"
#include "src/tracing/test/fake_producer_endpoint.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/trace/test_event.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {
namespace {

class TraceWriterImplTest : public AlignedBufferTest {
 public:
  void SetUp() override {
    SharedMemoryArbiterImpl::set_default_layout_for_testing(
        SharedMemoryABI::PageLayout::kPageDiv4);
    AlignedBufferTest::SetUp();
    task_runner_.reset(new base::TestTaskRunner());
    arbiter_.reset(new SharedMemoryArbiterImpl(buf(), buf_size(), page_size(),
                                               &fake_producer_endpoint_,
                                               task_runner_.get()));
  }

  void TearDown() override {
    arbiter_.reset();
    task_runner_.reset();
  }

  FakeProducerEndpoint fake_producer_endpoint_;
  std::unique_ptr<base::TestTaskRunner> task_runner_;
  std::unique_ptr<SharedMemoryArbiterImpl> arbiter_;
  std::function<void(const std::vector<uint32_t>&)> on_pages_complete_;
};

size_t const kPageSizes[] = {4096, 65536};
INSTANTIATE_TEST_SUITE_P(PageSize,
                         TraceWriterImplTest,
                         ::testing::ValuesIn(kPageSizes));

TEST_P(TraceWriterImplTest, SingleWriter) {
  const BufferID kBufId = 42;
  std::unique_ptr<TraceWriter> writer = arbiter_->CreateTraceWriter(kBufId);
  const size_t kNumPackets = 32;
  for (size_t i = 0; i < kNumPackets; i++) {
    auto packet = writer->NewTracePacket();
    char str[16];
    sprintf(str, "foobar %zu", i);
    packet->set_for_testing()->set_str(str);
  }

  // Destroying the TraceWriteImpl should cause the last packet to be finalized
  // and the chunk to be put back in the kChunkComplete state.
  writer.reset();

  SharedMemoryABI* abi = arbiter_->shmem_abi_for_testing();
  size_t packets_count = 0;
  for (size_t page_idx = 0; page_idx < kNumPages; page_idx++) {
    uint32_t page_layout = abi->GetPageLayout(page_idx);
    size_t num_chunks = SharedMemoryABI::GetNumChunksForLayout(page_layout);
    for (size_t chunk_idx = 0; chunk_idx < num_chunks; chunk_idx++) {
      auto chunk_state = abi->GetChunkState(page_idx, chunk_idx);
      ASSERT_TRUE(chunk_state == SharedMemoryABI::kChunkFree ||
                  chunk_state == SharedMemoryABI::kChunkComplete);
      auto chunk = abi->TryAcquireChunkForReading(page_idx, chunk_idx);
      if (!chunk.is_valid())
        continue;
      packets_count += chunk.header()->packets.load().count;
    }
  }
  EXPECT_EQ(kNumPackets, packets_count);
  // TODO(primiano): check also the content of the packets decoding the protos.
}

TEST_P(TraceWriterImplTest, FragmentingPacketWithProducerAndServicePatching) {
  const BufferID kBufId = 42;
  std::unique_ptr<TraceWriter> writer = arbiter_->CreateTraceWriter(kBufId);

  // Write a packet that's guaranteed to span more than a single chunk, but less
  // than two chunks.
  auto packet = writer->NewTracePacket();
  size_t chunk_size = page_size() / 4;
  std::stringstream large_string_writer;
  for (size_t pos = 0; pos < chunk_size; pos++)
    large_string_writer << "x";
  std::string large_string = large_string_writer.str();
  packet->set_for_testing()->set_str(large_string.data(), large_string.size());

  // First chunk should be committed.
  arbiter_->FlushPendingCommitDataRequests();
  const auto& last_commit = fake_producer_endpoint_.last_commit_data_request;
  ASSERT_EQ(1, last_commit.chunks_to_move_size());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].page());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].chunk());
  EXPECT_EQ(kBufId, last_commit.chunks_to_move()[0].target_buffer());
  EXPECT_EQ(0, last_commit.chunks_to_patch_size());

  // We will simulate a batching cycle by first setting the batching period to a
  // very large value and then force-flushing when we are done writing data.
  arbiter_->SetDirectSMBPatchingSupportedByService();
  ASSERT_TRUE(arbiter_->EnableDirectSMBPatching());
  arbiter_->SetBatchCommitsDuration(UINT32_MAX);

  // Write a second packet that's guaranteed to span more than a single chunk.
  // Starting a new trace packet should cause the patches for the first packet
  // (i.e. for the first chunk) to be queued for sending to the service. They
  // cannot be applied locally because the first chunk was already committed.
  packet->Finalize();
  auto packet2 = writer->NewTracePacket();
  packet2->set_for_testing()->set_str(large_string.data(), large_string.size());

  // Starting a new packet yet again should cause the patches for the second
  // packet (i.e. for the second chunk) to be applied in the producer, because
  // the second chunk has not been committed yet.
  packet2->Finalize();
  auto packet3 = writer->NewTracePacket();

  // Simulate the end of the batching period, which should trigger a commit to
  // the service.
  arbiter_->FlushPendingCommitDataRequests();

  SharedMemoryABI* abi = arbiter_->shmem_abi_for_testing();

  // The first allocated chunk should be complete but need patching, since the
  // packet extended past the chunk and no patches for the packet size or string
  // field size were applied yet.
  ASSERT_EQ(SharedMemoryABI::kChunkComplete, abi->GetChunkState(0u, 0u));
  auto chunk = abi->TryAcquireChunkForReading(0u, 0u);
  ASSERT_TRUE(chunk.is_valid());
  ASSERT_EQ(1, chunk.header()->packets.load().count);
  ASSERT_TRUE(chunk.header()->packets.load().flags &
              SharedMemoryABI::ChunkHeader::kChunkNeedsPatching);
  ASSERT_TRUE(chunk.header()->packets.load().flags &
              SharedMemoryABI::ChunkHeader::kLastPacketContinuesOnNextChunk);

  // Verify that a patch for the first chunk was sent to the service.
  ASSERT_EQ(1, last_commit.chunks_to_patch_size());
  EXPECT_EQ(writer->writer_id(), last_commit.chunks_to_patch()[0].writer_id());
  EXPECT_EQ(kBufId, last_commit.chunks_to_patch()[0].target_buffer());
  EXPECT_EQ(chunk.header()->chunk_id.load(),
            last_commit.chunks_to_patch()[0].chunk_id());
  EXPECT_FALSE(last_commit.chunks_to_patch()[0].has_more_patches());
  ASSERT_EQ(1, last_commit.chunks_to_patch()[0].patches_size());

  // Verify that the second chunk was committed.
  ASSERT_EQ(1, last_commit.chunks_to_move_size());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].page());
  EXPECT_EQ(1u, last_commit.chunks_to_move()[0].chunk());
  EXPECT_EQ(kBufId, last_commit.chunks_to_move()[0].target_buffer());

  // The second chunk should be in a complete state and should not need
  // patching, as the patches to it should have been applied in the producer.
  ASSERT_EQ(SharedMemoryABI::kChunkComplete, abi->GetChunkState(0u, 1u));
  auto chunk2 = abi->TryAcquireChunkForReading(0u, 1u);
  ASSERT_TRUE(chunk2.is_valid());
  ASSERT_EQ(2, chunk2.header()->packets.load().count);
  ASSERT_TRUE(chunk2.header()->packets.load().flags &
              SharedMemoryABI::ChunkHeader::kLastPacketContinuesOnNextChunk);
  ASSERT_FALSE(chunk2.header()->packets.load().flags &
               SharedMemoryABI::ChunkHeader::kChunkNeedsPatching);
}

TEST_P(TraceWriterImplTest, FragmentingPacketWithoutEnablingProducerPatching) {
  // We will simulate a batching cycle by first setting the batching period to a
  // very large value and will force flush to simulate a flush happening when we
  // believe it should - in this case when a patch is encountered.
  //
  // Note: direct producer-side patching should be disabled by default.
  arbiter_->SetBatchCommitsDuration(UINT32_MAX);

  const BufferID kBufId = 42;
  std::unique_ptr<TraceWriter> writer = arbiter_->CreateTraceWriter(kBufId);

  // Write a packet that's guaranteed to span more than a single chunk.
  auto packet = writer->NewTracePacket();
  size_t chunk_size = page_size() / 4;
  std::stringstream large_string_writer;
  for (size_t pos = 0; pos < chunk_size; pos++)
    large_string_writer << "x";
  std::string large_string = large_string_writer.str();
  packet->set_for_testing()->set_str(large_string.data(), large_string.size());

  // Starting a new packet should cause the first chunk and its patches to be
  // committed to the service.
  packet->Finalize();
  auto packet2 = writer->NewTracePacket();
  arbiter_->FlushPendingCommitDataRequests();

  // The first allocated chunk should be complete but need patching, since the
  // packet extended past the chunk and no patches for the packet size or string
  // field size were applied in the producer.
  SharedMemoryABI* abi = arbiter_->shmem_abi_for_testing();
  ASSERT_EQ(SharedMemoryABI::kChunkComplete, abi->GetChunkState(0u, 0u));
  auto chunk = abi->TryAcquireChunkForReading(0u, 0u);
  ASSERT_TRUE(chunk.is_valid());
  ASSERT_EQ(1, chunk.header()->packets.load().count);
  ASSERT_TRUE(chunk.header()->packets.load().flags &
              SharedMemoryABI::ChunkHeader::kChunkNeedsPatching);
  ASSERT_TRUE(chunk.header()->packets.load().flags &
              SharedMemoryABI::ChunkHeader::kLastPacketContinuesOnNextChunk);

  // The first chunk was committed.
  const auto& last_commit = fake_producer_endpoint_.last_commit_data_request;
  ASSERT_EQ(1, last_commit.chunks_to_move_size());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].page());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].chunk());
  EXPECT_EQ(kBufId, last_commit.chunks_to_move()[0].target_buffer());

  // The patches for the first chunk were committed.
  ASSERT_EQ(1, last_commit.chunks_to_patch_size());
  EXPECT_EQ(writer->writer_id(), last_commit.chunks_to_patch()[0].writer_id());
  EXPECT_EQ(kBufId, last_commit.chunks_to_patch()[0].target_buffer());
  EXPECT_EQ(chunk.header()->chunk_id.load(),
            last_commit.chunks_to_patch()[0].chunk_id());
  EXPECT_FALSE(last_commit.chunks_to_patch()[0].has_more_patches());
  ASSERT_EQ(1, last_commit.chunks_to_patch()[0].patches_size());
}

// Sets up a scenario in which the SMB is exhausted and TraceWriter fails to get
// a new chunk while fragmenting a packet. Verifies that data is dropped until
// the SMB is freed up and TraceWriter can get a new chunk.
TEST_P(TraceWriterImplTest, FragmentingPacketWhileBufferExhausted) {
  arbiter_.reset(new SharedMemoryArbiterImpl(buf(), buf_size(), page_size(),
                                             &fake_producer_endpoint_,
                                             task_runner_.get()));

  const BufferID kBufId = 42;
  std::unique_ptr<TraceWriter> writer =
      arbiter_->CreateTraceWriter(kBufId, BufferExhaustedPolicy::kDrop);

  // Write a small first packet, so that |writer| owns a chunk.
  auto packet = writer->NewTracePacket();
  EXPECT_FALSE(reinterpret_cast<TraceWriterImpl*>(writer.get())
                   ->drop_packets_for_testing());
  EXPECT_EQ(packet->Finalize(), 0u);

  // Grab all the remaining chunks in the SMB in new writers.
  std::array<std::unique_ptr<TraceWriter>, kNumPages * 4 - 1> other_writers;
  for (size_t i = 0; i < other_writers.size(); i++) {
    other_writers[i] =
        arbiter_->CreateTraceWriter(kBufId, BufferExhaustedPolicy::kDrop);
    auto other_writer_packet = other_writers[i]->NewTracePacket();
    EXPECT_FALSE(reinterpret_cast<TraceWriterImpl*>(other_writers[i].get())
                     ->drop_packets_for_testing());
  }

  // Write a packet that's guaranteed to span more than a single chunk, causing
  // |writer| to attempt to acquire a new chunk but fail to do so.
  auto packet2 = writer->NewTracePacket();
  size_t chunk_size = page_size() / 4;
  std::stringstream large_string_writer;
  for (size_t pos = 0; pos < chunk_size; pos++)
    large_string_writer << "x";
  std::string large_string = large_string_writer.str();
  packet2->set_for_testing()->set_str(large_string.data(), large_string.size());

  EXPECT_TRUE(reinterpret_cast<TraceWriterImpl*>(writer.get())
                  ->drop_packets_for_testing());

  // First chunk should be committed.
  arbiter_->FlushPendingCommitDataRequests();
  const auto& last_commit = fake_producer_endpoint_.last_commit_data_request;
  ASSERT_EQ(1, last_commit.chunks_to_move_size());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].page());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].chunk());
  EXPECT_EQ(kBufId, last_commit.chunks_to_move()[0].target_buffer());
  EXPECT_EQ(0, last_commit.chunks_to_patch_size());

  // It should not need patching and not have the continuation flag set.
  SharedMemoryABI* abi = arbiter_->shmem_abi_for_testing();
  ASSERT_EQ(SharedMemoryABI::kChunkComplete, abi->GetChunkState(0u, 0u));
  auto chunk = abi->TryAcquireChunkForReading(0u, 0u);
  ASSERT_TRUE(chunk.is_valid());
  ASSERT_EQ(2, chunk.header()->packets.load().count);
  ASSERT_FALSE(chunk.header()->packets.load().flags &
               SharedMemoryABI::ChunkHeader::kChunkNeedsPatching);
  ASSERT_FALSE(chunk.header()->packets.load().flags &
               SharedMemoryABI::ChunkHeader::kLastPacketContinuesOnNextChunk);

  // Writing more data while in garbage mode succeeds. This data is dropped.
  packet2->Finalize();
  auto packet3 = writer->NewTracePacket();
  packet3->set_for_testing()->set_str(large_string.data(), large_string.size());

  // Release the |writer|'s first chunk as free, so that it can grab it again.
  abi->ReleaseChunkAsFree(std::move(chunk));

  // Starting a new packet should cause TraceWriter to attempt to grab a new
  // chunk again, because we wrote enough data to wrap the garbage chunk.
  packet3->Finalize();
  auto packet4 = writer->NewTracePacket();

  // Grabbing the chunk should have succeeded.
  EXPECT_FALSE(reinterpret_cast<TraceWriterImpl*>(writer.get())
                   ->drop_packets_for_testing());

  // The first packet in the chunk should have the previous_packet_dropped flag
  // set, so shouldn't be empty.
  EXPECT_GT(packet4->Finalize(), 0u);

  // Flushing the writer causes the chunk to be released again.
  writer->Flush();
  EXPECT_EQ(1, last_commit.chunks_to_move_size());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].page());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].chunk());
  ASSERT_EQ(0, last_commit.chunks_to_patch_size());

  // Chunk should contain only |packet4| and not have any continuation flag set.
  ASSERT_EQ(SharedMemoryABI::kChunkComplete, abi->GetChunkState(0u, 0u));
  chunk = abi->TryAcquireChunkForReading(0u, 0u);
  ASSERT_TRUE(chunk.is_valid());
  ASSERT_EQ(1, chunk.header()->packets.load().count);
  ASSERT_FALSE(chunk.header()->packets.load().flags &
               SharedMemoryABI::ChunkHeader::kChunkNeedsPatching);
  ASSERT_FALSE(
      chunk.header()->packets.load().flags &
      SharedMemoryABI::ChunkHeader::kFirstPacketContinuesFromPrevChunk);
  ASSERT_FALSE(chunk.header()->packets.load().flags &
               SharedMemoryABI::ChunkHeader::kLastPacketContinuesOnNextChunk);
}

// Verifies that a TraceWriter that is flushed before the SMB is full and then
// acquires a garbage chunk later recovers and writes a previous_packet_dropped
// marker into the trace.
TEST_P(TraceWriterImplTest, FlushBeforeBufferExhausted) {
  arbiter_.reset(new SharedMemoryArbiterImpl(buf(), buf_size(), page_size(),
                                             &fake_producer_endpoint_,
                                             task_runner_.get()));

  const BufferID kBufId = 42;
  std::unique_ptr<TraceWriter> writer =
      arbiter_->CreateTraceWriter(kBufId, BufferExhaustedPolicy::kDrop);

  // Write a small first packet and flush it, so that |writer| no longer owns
  // any chunk.
  auto packet = writer->NewTracePacket();
  EXPECT_FALSE(reinterpret_cast<TraceWriterImpl*>(writer.get())
                   ->drop_packets_for_testing());
  EXPECT_EQ(packet->Finalize(), 0u);

  // Flush the first chunk away.
  writer->Flush();

  // First chunk should be committed. Don't release it as free just yet.
  arbiter_->FlushPendingCommitDataRequests();
  const auto& last_commit = fake_producer_endpoint_.last_commit_data_request;
  ASSERT_EQ(1, last_commit.chunks_to_move_size());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].page());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].chunk());

  // Grab all the remaining chunks in the SMB in new writers.
  std::array<std::unique_ptr<TraceWriter>, kNumPages * 4 - 1> other_writers;
  for (size_t i = 0; i < other_writers.size(); i++) {
    other_writers[i] =
        arbiter_->CreateTraceWriter(kBufId, BufferExhaustedPolicy::kDrop);
    auto other_writer_packet = other_writers[i]->NewTracePacket();
    EXPECT_FALSE(reinterpret_cast<TraceWriterImpl*>(other_writers[i].get())
                     ->drop_packets_for_testing());
  }

  // Write another packet, causing |writer| to acquire a garbage chunk.
  auto packet2 = writer->NewTracePacket();
  EXPECT_TRUE(reinterpret_cast<TraceWriterImpl*>(writer.get())
                  ->drop_packets_for_testing());

  // Writing more data while in garbage mode succeeds. This data is dropped.
  // Make sure that we fill the garbage chunk, so that |writer| tries to
  // re-acquire a valid chunk for the next packet.
  size_t chunk_size = page_size() / 4;
  std::stringstream large_string_writer;
  for (size_t pos = 0; pos < chunk_size; pos++)
    large_string_writer << "x";
  std::string large_string = large_string_writer.str();
  packet2->set_for_testing()->set_str(large_string.data(), large_string.size());
  packet2->Finalize();

  // Next packet should still be in the garbage chunk.
  auto packet3 = writer->NewTracePacket();
  EXPECT_TRUE(reinterpret_cast<TraceWriterImpl*>(writer.get())
                  ->drop_packets_for_testing());

  // Release the first chunk as free, so |writer| can acquire it again.
  SharedMemoryABI* abi = arbiter_->shmem_abi_for_testing();
  ASSERT_EQ(SharedMemoryABI::kChunkComplete, abi->GetChunkState(0u, 0u));
  auto chunk = abi->TryAcquireChunkForReading(0u, 0u);
  ASSERT_TRUE(chunk.is_valid());
  abi->ReleaseChunkAsFree(std::move(chunk));

  // Fill the garbage chunk, so that the writer attempts to grab another chunk
  // for |packet4|.
  packet3->set_for_testing()->set_str(large_string.data(), large_string.size());
  packet3->Finalize();

  // Next packet should go into the reacquired chunk we just released.
  auto packet4 = writer->NewTracePacket();
  EXPECT_FALSE(reinterpret_cast<TraceWriterImpl*>(writer.get())
                   ->drop_packets_for_testing());

  // The first packet in the chunk should have the previous_packet_dropped flag
  // set, so shouldn't be empty.
  EXPECT_GT(packet4->Finalize(), 0u);

  // Flushing the writer causes the chunk to be released again.
  writer->Flush();
  EXPECT_EQ(1, last_commit.chunks_to_move_size());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].page());
  EXPECT_EQ(0u, last_commit.chunks_to_move()[0].chunk());
  ASSERT_EQ(0, last_commit.chunks_to_patch_size());

  // Chunk should contain only |packet4| and not have any continuation flag set.
  ASSERT_EQ(SharedMemoryABI::kChunkComplete, abi->GetChunkState(0u, 0u));
  chunk = abi->TryAcquireChunkForReading(0u, 0u);
  ASSERT_TRUE(chunk.is_valid());
  ASSERT_EQ(1, chunk.header()->packets.load().count);
  ASSERT_FALSE(chunk.header()->packets.load().flags &
               SharedMemoryABI::ChunkHeader::kChunkNeedsPatching);
  ASSERT_FALSE(
      chunk.header()->packets.load().flags &
      SharedMemoryABI::ChunkHeader::kFirstPacketContinuesFromPrevChunk);
  ASSERT_FALSE(chunk.header()->packets.load().flags &
               SharedMemoryABI::ChunkHeader::kLastPacketContinuesOnNextChunk);
}

// Regression test that verifies that flushing a TraceWriter while a fragmented
// packet still has uncommitted patches doesn't hit a DCHECK / crash the writer
// thread.
TEST_P(TraceWriterImplTest, FlushAfterFragmentingPacketWhileBufferExhausted) {
  arbiter_.reset(new SharedMemoryArbiterImpl(buf(), buf_size(), page_size(),
                                             &fake_producer_endpoint_,
                                             task_runner_.get()));

  const BufferID kBufId = 42;
  std::unique_ptr<TraceWriter> writer =
      arbiter_->CreateTraceWriter(kBufId, BufferExhaustedPolicy::kDrop);

  // Write a small first packet, so that |writer| owns a chunk.
  auto packet = writer->NewTracePacket();
  EXPECT_FALSE(reinterpret_cast<TraceWriterImpl*>(writer.get())
                   ->drop_packets_for_testing());
  EXPECT_EQ(packet->Finalize(), 0u);

  // Grab all but one of the remaining chunks in the SMB in new writers.
  std::array<std::unique_ptr<TraceWriter>, kNumPages * 4 - 2> other_writers;
  for (size_t i = 0; i < other_writers.size(); i++) {
    other_writers[i] =
        arbiter_->CreateTraceWriter(kBufId, BufferExhaustedPolicy::kDrop);
    auto other_writer_packet = other_writers[i]->NewTracePacket();
    EXPECT_FALSE(reinterpret_cast<TraceWriterImpl*>(other_writers[i].get())
                     ->drop_packets_for_testing());
  }

  // Write a packet that's guaranteed to span more than a two chunks, causing
  // |writer| to attempt to acquire two new chunks, but fail to acquire the
  // second.
  auto packet2 = writer->NewTracePacket();
  size_t chunk_size = page_size() / 4;
  std::stringstream large_string_writer;
  for (size_t pos = 0; pos < chunk_size * 2; pos++)
    large_string_writer << "x";
  std::string large_string = large_string_writer.str();
  packet2->set_for_testing()->set_str(large_string.data(), large_string.size());

  EXPECT_TRUE(reinterpret_cast<TraceWriterImpl*>(writer.get())
                  ->drop_packets_for_testing());

  // First two chunks should be committed.
  arbiter_->FlushPendingCommitDataRequests();
  const auto& last_commit = fake_producer_endpoint_.last_commit_data_request;
  ASSERT_EQ(2, last_commit.chunks_to_move_size());

  // Flushing should succeed, even though some patches are still in the writer's
  // patch list.
  packet2->Finalize();
  writer->Flush();
}

// TODO(primiano): add multi-writer test.
// TODO(primiano): add Flush() test.

}  // namespace
}  // namespace perfetto
