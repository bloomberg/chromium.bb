// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vtl): I currently potentially overflow in doing index calculations.
// E.g., |start_index_| and |current_num_bytes_| fit into a |uint32_t|, but
// their sum may not. This is bad and poses a security risk. (We're currently
// saved by the limit on capacity -- the maximum size of the buffer, checked in
// |DataPipe::ValidateOptions()|, is currently sufficiently small.)

#include "mojo/edk/system/local_data_pipe_impl.h"

#include <string.h>

#include <algorithm>

#include "base/logging.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/data_pipe.h"

namespace mojo {
namespace system {

LocalDataPipeImpl::LocalDataPipeImpl()
    : start_index_(0), current_num_bytes_(0) {
  // Note: |buffer_| is lazily allocated, since a common case will be that one
  // of the handles is immediately passed off to another process.
}

LocalDataPipeImpl::~LocalDataPipeImpl() {
}

void LocalDataPipeImpl::ProducerClose() {
  // If the consumer is still open and we still have data, we have to keep the
  // buffer around. Currently, we won't free it even if it empties later. (We
  // could do this -- requiring a check on every read -- but that seems to be
  // optimizing for the uncommon case.)
  if (!consumer_open() || !current_num_bytes_) {
    // Note: There can only be a two-phase *read* (by the consumer) if we still
    // have data.
    DCHECK(!consumer_in_two_phase_read());
    DestroyBuffer();
  }
}

MojoResult LocalDataPipeImpl::ProducerWriteData(
    UserPointer<const void> elements,
    UserPointer<uint32_t> num_bytes,
    uint32_t max_num_bytes_to_write,
    uint32_t min_num_bytes_to_write) {
  DCHECK_EQ(max_num_bytes_to_write % element_num_bytes(), 0u);
  DCHECK_EQ(min_num_bytes_to_write % element_num_bytes(), 0u);
  DCHECK_GT(max_num_bytes_to_write, 0u);
  DCHECK(consumer_open());

  size_t num_bytes_to_write = 0;
  if (may_discard()) {
    if (min_num_bytes_to_write > capacity_num_bytes())
      return MOJO_RESULT_OUT_OF_RANGE;

    num_bytes_to_write = std::min(static_cast<size_t>(max_num_bytes_to_write),
                                  capacity_num_bytes());
    if (num_bytes_to_write > capacity_num_bytes() - current_num_bytes_) {
      // Discard as much as needed (discard oldest first).
      MarkDataAsConsumed(num_bytes_to_write -
                         (capacity_num_bytes() - current_num_bytes_));
      // No need to wake up write waiters, since we're definitely going to leave
      // the buffer full.
    }
  } else {
    if (min_num_bytes_to_write > capacity_num_bytes() - current_num_bytes_) {
      // Don't return "should wait" since you can't wait for a specified amount
      // of data.
      return MOJO_RESULT_OUT_OF_RANGE;
    }

    num_bytes_to_write = std::min(static_cast<size_t>(max_num_bytes_to_write),
                                  capacity_num_bytes() - current_num_bytes_);
  }
  if (num_bytes_to_write == 0)
    return MOJO_RESULT_SHOULD_WAIT;

  // The amount we can write in our first |memcpy()|.
  size_t num_bytes_to_write_first =
      std::min(num_bytes_to_write, GetMaxNumBytesToWrite());
  // Do the first (and possibly only) |memcpy()|.
  size_t first_write_index =
      (start_index_ + current_num_bytes_) % capacity_num_bytes();
  EnsureBuffer();
  elements.GetArray(buffer_.get() + first_write_index,
                    num_bytes_to_write_first);

  if (num_bytes_to_write_first < num_bytes_to_write) {
    // The "second write index" is zero.
    elements.At(num_bytes_to_write_first)
        .GetArray(buffer_.get(), num_bytes_to_write - num_bytes_to_write_first);
  }

  current_num_bytes_ += num_bytes_to_write;
  DCHECK_LE(current_num_bytes_, capacity_num_bytes());
  num_bytes.Put(static_cast<uint32_t>(num_bytes_to_write));
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipeImpl::ProducerBeginWriteData(
    UserPointer<void*> buffer,
    UserPointer<uint32_t> buffer_num_bytes,
    uint32_t min_num_bytes_to_write) {
  DCHECK(consumer_open());

  // The index we need to start writing at.
  size_t write_index =
      (start_index_ + current_num_bytes_) % capacity_num_bytes();

  size_t max_num_bytes_to_write = GetMaxNumBytesToWrite();
  if (min_num_bytes_to_write > max_num_bytes_to_write) {
    // In "may discard" mode, we can always write from the write index to the
    // end of the buffer.
    if (may_discard() &&
        min_num_bytes_to_write <= capacity_num_bytes() - write_index) {
      // To do so, we need to discard an appropriate amount of data.
      // We should only reach here if the start index is after the write index!
      DCHECK_GE(start_index_, write_index);
      DCHECK_GT(min_num_bytes_to_write - max_num_bytes_to_write, 0u);
      MarkDataAsConsumed(min_num_bytes_to_write - max_num_bytes_to_write);
      max_num_bytes_to_write = min_num_bytes_to_write;
    } else {
      // Don't return "should wait" since you can't wait for a specified amount
      // of data.
      return MOJO_RESULT_OUT_OF_RANGE;
    }
  }

  // Don't go into a two-phase write if there's no room.
  if (max_num_bytes_to_write == 0)
    return MOJO_RESULT_SHOULD_WAIT;

  EnsureBuffer();
  buffer.Put(buffer_.get() + write_index);
  buffer_num_bytes.Put(static_cast<uint32_t>(max_num_bytes_to_write));
  set_producer_two_phase_max_num_bytes_written(
      static_cast<uint32_t>(max_num_bytes_to_write));
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipeImpl::ProducerEndWriteData(uint32_t num_bytes_written) {
  DCHECK_LE(num_bytes_written, producer_two_phase_max_num_bytes_written());
  current_num_bytes_ += num_bytes_written;
  DCHECK_LE(current_num_bytes_, capacity_num_bytes());
  set_producer_two_phase_max_num_bytes_written(0);
  return MOJO_RESULT_OK;
}

HandleSignalsState LocalDataPipeImpl::ProducerGetHandleSignalsState() const {
  HandleSignalsState rv;
  if (consumer_open()) {
    if ((may_discard() || current_num_bytes_ < capacity_num_bytes()) &&
        !producer_in_two_phase_write())
      rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
  } else {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  }
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  return rv;
}

void LocalDataPipeImpl::ProducerStartSerialize(Channel* channel,
                                               size_t* max_size,
                                               size_t* max_platform_handles) {
  // TODO(vtl): Support serializing producer data pipe handles.
  *max_size = 0;
  *max_platform_handles = 0;
}

bool LocalDataPipeImpl::ProducerEndSerialize(
    Channel* channel,
    void* destination,
    size_t* actual_size,
    embedder::PlatformHandleVector* platform_handles) {
  // TODO(vtl): Support serializing producer data pipe handles.
  owner()->ProducerCloseNoLock();
  return false;
}

void LocalDataPipeImpl::ConsumerClose() {
  // If the producer is around and in a two-phase write, we have to keep the
  // buffer around. (We then don't free it until the producer is closed. This
  // could be rectified, but again seems like optimizing for the uncommon case.)
  if (!producer_open() || !producer_in_two_phase_write())
    DestroyBuffer();
  current_num_bytes_ = 0;
}

MojoResult LocalDataPipeImpl::ConsumerReadData(UserPointer<void> elements,
                                               UserPointer<uint32_t> num_bytes,
                                               uint32_t max_num_bytes_to_read,
                                               uint32_t min_num_bytes_to_read,
                                               bool peek) {
  DCHECK_EQ(max_num_bytes_to_read % element_num_bytes(), 0u);
  DCHECK_EQ(min_num_bytes_to_read % element_num_bytes(), 0u);
  DCHECK_GT(max_num_bytes_to_read, 0u);

  if (min_num_bytes_to_read > current_num_bytes_) {
    // Don't return "should wait" since you can't wait for a specified amount of
    // data.
    return producer_open() ? MOJO_RESULT_OUT_OF_RANGE
                           : MOJO_RESULT_FAILED_PRECONDITION;
  }

  size_t num_bytes_to_read =
      std::min(static_cast<size_t>(max_num_bytes_to_read), current_num_bytes_);
  if (num_bytes_to_read == 0) {
    return producer_open() ? MOJO_RESULT_SHOULD_WAIT
                           : MOJO_RESULT_FAILED_PRECONDITION;
  }

  // The amount we can read in our first |memcpy()|.
  size_t num_bytes_to_read_first =
      std::min(num_bytes_to_read, GetMaxNumBytesToRead());
  elements.PutArray(buffer_.get() + start_index_, num_bytes_to_read_first);

  if (num_bytes_to_read_first < num_bytes_to_read) {
    // The "second read index" is zero.
    elements.At(num_bytes_to_read_first)
        .PutArray(buffer_.get(), num_bytes_to_read - num_bytes_to_read_first);
  }

  if (!peek)
    MarkDataAsConsumed(num_bytes_to_read);
  num_bytes.Put(static_cast<uint32_t>(num_bytes_to_read));
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipeImpl::ConsumerDiscardData(
    UserPointer<uint32_t> num_bytes,
    uint32_t max_num_bytes_to_discard,
    uint32_t min_num_bytes_to_discard) {
  DCHECK_EQ(max_num_bytes_to_discard % element_num_bytes(), 0u);
  DCHECK_EQ(min_num_bytes_to_discard % element_num_bytes(), 0u);
  DCHECK_GT(max_num_bytes_to_discard, 0u);

  if (min_num_bytes_to_discard > current_num_bytes_) {
    // Don't return "should wait" since you can't wait for a specified amount of
    // data.
    return producer_open() ? MOJO_RESULT_OUT_OF_RANGE
                           : MOJO_RESULT_FAILED_PRECONDITION;
  }

  // Be consistent with other operations; error if no data available.
  if (current_num_bytes_ == 0) {
    return producer_open() ? MOJO_RESULT_SHOULD_WAIT
                           : MOJO_RESULT_FAILED_PRECONDITION;
  }

  size_t num_bytes_to_discard = std::min(
      static_cast<size_t>(max_num_bytes_to_discard), current_num_bytes_);
  MarkDataAsConsumed(num_bytes_to_discard);
  num_bytes.Put(static_cast<uint32_t>(num_bytes_to_discard));
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipeImpl::ConsumerQueryData(
    UserPointer<uint32_t> num_bytes) {
  // Note: This cast is safe, since the capacity fits into a |uint32_t|.
  num_bytes.Put(static_cast<uint32_t>(current_num_bytes_));
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipeImpl::ConsumerBeginReadData(
    UserPointer<const void*> buffer,
    UserPointer<uint32_t> buffer_num_bytes,
    uint32_t min_num_bytes_to_read) {
  size_t max_num_bytes_to_read = GetMaxNumBytesToRead();
  if (min_num_bytes_to_read > max_num_bytes_to_read) {
    // Don't return "should wait" since you can't wait for a specified amount of
    // data.
    return producer_open() ? MOJO_RESULT_OUT_OF_RANGE
                           : MOJO_RESULT_FAILED_PRECONDITION;
  }

  // Don't go into a two-phase read if there's no data.
  if (max_num_bytes_to_read == 0) {
    return producer_open() ? MOJO_RESULT_SHOULD_WAIT
                           : MOJO_RESULT_FAILED_PRECONDITION;
  }

  buffer.Put(buffer_.get() + start_index_);
  buffer_num_bytes.Put(static_cast<uint32_t>(max_num_bytes_to_read));
  set_consumer_two_phase_max_num_bytes_read(
      static_cast<uint32_t>(max_num_bytes_to_read));
  return MOJO_RESULT_OK;
}

MojoResult LocalDataPipeImpl::ConsumerEndReadData(uint32_t num_bytes_read) {
  DCHECK_LE(num_bytes_read, consumer_two_phase_max_num_bytes_read());
  DCHECK_LE(start_index_ + num_bytes_read, capacity_num_bytes());
  MarkDataAsConsumed(num_bytes_read);
  set_consumer_two_phase_max_num_bytes_read(0);
  return MOJO_RESULT_OK;
}

HandleSignalsState LocalDataPipeImpl::ConsumerGetHandleSignalsState() const {
  HandleSignalsState rv;
  if (current_num_bytes_ > 0) {
    if (!consumer_in_two_phase_read())
      rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_READABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  } else if (producer_open()) {
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  }
  if (!producer_open())
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  return rv;
}

void LocalDataPipeImpl::ConsumerStartSerialize(Channel* channel,
                                               size_t* max_size,
                                               size_t* max_platform_handles) {
  // TODO(vtl): Support serializing consumer data pipe handles.
  *max_size = 0;
  *max_platform_handles = 0;
}

bool LocalDataPipeImpl::ConsumerEndSerialize(
    Channel* channel,
    void* destination,
    size_t* actual_size,
    embedder::PlatformHandleVector* platform_handles) {
  // TODO(vtl): Support serializing consumer data pipe handles.
  owner()->ConsumerCloseNoLock();
  return false;
}

void LocalDataPipeImpl::EnsureBuffer() {
  DCHECK(producer_open());
  if (buffer_)
    return;
  buffer_.reset(static_cast<char*>(
      base::AlignedAlloc(capacity_num_bytes(),
                         GetConfiguration().data_pipe_buffer_alignment_bytes)));
}

void LocalDataPipeImpl::DestroyBuffer() {
#ifndef NDEBUG
  // Scribble on the buffer to help detect use-after-frees. (This also helps the
  // unit test detect certain bugs without needing ASAN or similar.)
  if (buffer_)
    memset(buffer_.get(), 0xcd, capacity_num_bytes());
#endif
  buffer_.reset();
}

size_t LocalDataPipeImpl::GetMaxNumBytesToWrite() {
  size_t next_index = start_index_ + current_num_bytes_;
  if (next_index >= capacity_num_bytes()) {
    next_index %= capacity_num_bytes();
    DCHECK_GE(start_index_, next_index);
    DCHECK_EQ(start_index_ - next_index,
              capacity_num_bytes() - current_num_bytes_);
    return start_index_ - next_index;
  }
  return capacity_num_bytes() - next_index;
}

size_t LocalDataPipeImpl::GetMaxNumBytesToRead() {
  if (start_index_ + current_num_bytes_ > capacity_num_bytes())
    return capacity_num_bytes() - start_index_;
  return current_num_bytes_;
}

void LocalDataPipeImpl::MarkDataAsConsumed(size_t num_bytes) {
  DCHECK_LE(num_bytes, current_num_bytes_);
  start_index_ += num_bytes;
  start_index_ %= capacity_num_bytes();
  current_num_bytes_ -= num_bytes;
}

}  // namespace system
}  // namespace mojo
