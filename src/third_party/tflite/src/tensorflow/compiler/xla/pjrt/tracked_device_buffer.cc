/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/xla/pjrt/tracked_device_buffer.h"

#include <iterator>
#include <memory>

#include "absl/synchronization/mutex.h"
#include "tensorflow/compiler/xla/pjrt/local_device_state.h"
#include "tensorflow/compiler/xla/service/shaped_buffer.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/stream_executor/device_memory.h"
#include "tensorflow/stream_executor/device_memory_allocator.h"
#include "tensorflow/stream_executor/event.h"
#include "tensorflow/stream_executor/stream.h"

namespace xla {

void BufferSequencingEvent::SetSequencingEvent(EventPool::Handle event,
                                               se::Stream* stream) {
  absl::MutexLock lock(&mu_);
  CHECK(!event_.event());
  event_ = std::move(event);
  CHECK(streams_defined_on_.empty());
  streams_defined_on_.push_back(stream);
}

bool BufferSequencingEvent::EventHasBeenRecorded() const {
  return event_.event() != nullptr;
}

uint64 BufferSequencingEvent::sequence_number() const {
  absl::MutexLock lock(&mu_);
  CHECK(EventHasBeenRecorded());
  return event_.sequence_number();
}

void BufferSequencingEvent::WaitForEventOnStream(se::Stream* stream) {
  absl::MutexLock lock(&mu_);

  // We cannot wait for an event until ThenRecordEvent has been called; on GPU
  // newly created events are deemed to have already happened past.
  mu_.Await(
      absl::Condition(this, &BufferSequencingEvent::EventHasBeenRecorded));

  // The set of defined streams is expected to be very small indeed (usually
  // 1-2), so a simple linear scan should be fast enough.
  if (std::find(streams_defined_on_.begin(), streams_defined_on_.end(),
                stream) != streams_defined_on_.end()) {
    // stream is in streams_defined_on_; it doesn't need to be waited on.
    return;
  }

  stream->ThenWaitFor(event_.event());
  streams_defined_on_.push_back(stream);
}

bool BufferSequencingEvent::DefinedOn(se::Stream* stream) {
  absl::MutexLock lock(&mu_);

  // We cannot wait for an event until ThenRecordEvent has been called; on GPU
  // newly created events are deemed to have already happened past.
  mu_.Await(
      absl::Condition(this, &BufferSequencingEvent::EventHasBeenRecorded));

  // The set of defined streams is expected to be very small indeed (usually
  // 1-2), so a simple linear scan should be fast enough.
  return std::find(streams_defined_on_.begin(), streams_defined_on_.end(),
                   stream) != streams_defined_on_.end();
}

bool BufferSequencingEvent::IsComplete() {
  absl::MutexLock lock(&mu_);

  // We cannot wait for an event until ThenRecordEvent has been called; on
  // GPU newly created events are deemed to have already happened past.
  mu_.Await(
      absl::Condition(this, &BufferSequencingEvent::EventHasBeenRecorded));

  return event_.event()->PollForStatus() == se::Event::Status::kComplete;
}

/* static */ std::shared_ptr<TrackedDeviceBuffer>
TrackedDeviceBuffer::FromScopedShapedBuffer(
    ScopedShapedBuffer* shaped_buffer,
    absl::Span<const std::shared_ptr<BufferSequencingEvent>>
        definition_events) {
  ShapeTree<se::DeviceMemoryBase>::iterator iterator =
      shaped_buffer->buffers().begin();
  std::vector<se::DeviceMemoryBase> buffers;
  buffers.reserve(1);

  ShapeUtil::ForEachSubshape(
      shaped_buffer->on_device_shape(), [&](const Shape&, const ShapeIndex&) {
        CHECK(iterator != shaped_buffer->buffers().end());
        buffers.push_back(iterator->second);
        iterator->second = se::DeviceMemoryBase();
        ++iterator;
      });
  CHECK(iterator == shaped_buffer->buffers().end());
  return std::make_shared<TrackedDeviceBuffer>(
      shaped_buffer->memory_allocator(), shaped_buffer->device_ordinal(),
      absl::Span<se::DeviceMemoryBase>(buffers), definition_events,
      /*on_delete_callback=*/nullptr);
}

ShapedBuffer TrackedDeviceBuffer::AsShapedBuffer(const Shape& on_host_shape,
                                                 const Shape& on_device_shape,
                                                 se::Platform* platform) const {
  ShapedBuffer shaped_buffer(on_host_shape, on_device_shape, platform,
                             device_ordinal_);
  ShapeTree<se::DeviceMemoryBase>::iterator iterator =
      shaped_buffer.buffers().begin();
  for (const se::DeviceMemoryBase& buf : device_memory_) {
    CHECK(iterator != shaped_buffer.buffers().end());
    iterator->second = buf;
    ++iterator;
  }
  CHECK(iterator == shaped_buffer.buffers().end());
  return shaped_buffer;
}

// See comment on ExecutionInput in xla/service/executable.h to understand
// the meaning of owned/unowned in that class.

void TrackedDeviceBuffer::AddToInputAsImmutable(
    ShapeTree<MaybeOwningDeviceMemory>::iterator* iterator,
    const ShapeTree<MaybeOwningDeviceMemory>::iterator& end) const {
  for (const se::DeviceMemoryBase& buf : device_memory_) {
    CHECK(*iterator != end);
    // Set buffers to be case (1) in the comment on ExecutionInput.
    (*iterator)->second = MaybeOwningDeviceMemory(buf);
    ++(*iterator);
  }
}

void TrackedDeviceBuffer::AddToInputAsDonated(
    ShapeTree<MaybeOwningDeviceMemory>::iterator* iterator,
    const ShapeTree<MaybeOwningDeviceMemory>::iterator& end,
    ExecutionInput* execution_input,
    se::DeviceMemoryAllocator* allocator) const {
  for (const se::DeviceMemoryBase& buf : device_memory_) {
    CHECK(*iterator != end);
    // Set buffers to be case (2) in the comment on ExecutionInput.
    (*iterator)->second = MaybeOwningDeviceMemory(
        se::OwningDeviceMemory(buf, device_ordinal_, allocator));
    execution_input->SetUnownedIndex((*iterator)->first);
    ++(*iterator);
  }
}

namespace {

using MoveIterator =
    absl::Span<const std::shared_ptr<BufferSequencingEvent>>::iterator;

}  // namespace

TrackedDeviceBuffer::TrackedDeviceBuffer(
    se::DeviceMemoryAllocator* allocator, int device_ordinal,
    absl::Span<se::DeviceMemoryBase const> device_memory,
    absl::Span<const std::shared_ptr<BufferSequencingEvent>> definition_events,
    std::function<void()> on_delete_callback)
    : allocator_(allocator),
      device_ordinal_(device_ordinal),
      device_memory_(device_memory.begin(), device_memory.end()),
      definition_events_(
          std::move_iterator<MoveIterator>(definition_events.begin()),
          std::move_iterator<MoveIterator>(definition_events.end())),
      in_use_(true),
      on_delete_callback_(std::move(on_delete_callback)) {}

TrackedDeviceBuffer::~TrackedDeviceBuffer() {
  if (allocator_) {
    for (const se::DeviceMemoryBase& buffer : device_memory_) {
      Status status = allocator_->Deallocate(device_ordinal_, buffer);
      if (!status.ok()) {
        LOG(ERROR) << "Buffer deallocation failed: " << status;
      }
    }
  }
  if (on_delete_callback_) {
    on_delete_callback_();
  }
}

void TrackedDeviceBuffer::AddUsageEvent(
    se::Stream* usage_stream, std::shared_ptr<BufferSequencingEvent> event,
    bool reference_held) {
  CHECK(in_use_);

  for (auto& existing : usage_events_) {
    if (existing.stream == usage_stream) {
      if (*existing.event < *event) {
        existing.event = event;
        existing.reference_held = reference_held;
      }
      return;
    }
  }
  usage_events_.push_back({usage_stream, event, reference_held});
}

TrackedDeviceBuffer::StreamAndEventContainer
TrackedDeviceBuffer::LockUseAndTransferUsageEvents() {
  CHECK(in_use_);
  in_use_ = false;
  return std::move(usage_events_);
}

void GetDeviceBufferEvents(
    const TrackedDeviceBuffer& buffer, bool get_usage_events,
    absl::flat_hash_set<BufferSequencingEvent*>* events) {
  if (get_usage_events) {
    for (const auto& e : buffer.usage_events()) {
      events->insert(e.event.get());
    }
  } else {
    for (const auto& e : buffer.definition_events()) {
      events->insert(e.get());
    }
  }
}

void WaitForBufferDefinitionEventsOnStream(const TrackedDeviceBuffer& buffer,
                                           se::Stream* stream) {
  absl::flat_hash_set<BufferSequencingEvent*> events;
  GetDeviceBufferEvents(buffer, /*get_usage_events=*/false, &events);
  for (BufferSequencingEvent* event : events) {
    event->WaitForEventOnStream(stream);
  }
}

}  // namespace xla
