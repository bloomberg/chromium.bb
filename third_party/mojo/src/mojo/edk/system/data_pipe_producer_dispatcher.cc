// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/data_pipe_producer_dispatcher.h"

#include "base/logging.h"
#include "mojo/edk/system/data_pipe.h"
#include "mojo/edk/system/memory.h"

namespace mojo {
namespace system {

DataPipeProducerDispatcher::DataPipeProducerDispatcher() {
}

void DataPipeProducerDispatcher::Init(scoped_refptr<DataPipe> data_pipe) {
  DCHECK(data_pipe);
  data_pipe_ = data_pipe;
}

Dispatcher::Type DataPipeProducerDispatcher::GetType() const {
  return kTypeDataPipeProducer;
}

DataPipeProducerDispatcher::~DataPipeProducerDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the pipe.
  DCHECK(!data_pipe_);
}

void DataPipeProducerDispatcher::CancelAllAwakablesNoLock() {
  lock().AssertAcquired();
  data_pipe_->ProducerCancelAllAwakables();
}

void DataPipeProducerDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  data_pipe_->ProducerClose();
  data_pipe_ = nullptr;
}

scoped_refptr<Dispatcher>
DataPipeProducerDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  scoped_refptr<DataPipeProducerDispatcher> rv =
      new DataPipeProducerDispatcher();
  rv->Init(data_pipe_);
  data_pipe_ = nullptr;
  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult DataPipeProducerDispatcher::WriteDataImplNoLock(
    UserPointer<const void> elements,
    UserPointer<uint32_t> num_bytes,
    MojoWriteDataFlags flags) {
  lock().AssertAcquired();
  return data_pipe_->ProducerWriteData(
      elements, num_bytes, (flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
}

MojoResult DataPipeProducerDispatcher::BeginWriteDataImplNoLock(
    UserPointer<void*> buffer,
    UserPointer<uint32_t> buffer_num_bytes,
    MojoWriteDataFlags flags) {
  lock().AssertAcquired();

  return data_pipe_->ProducerBeginWriteData(
      buffer, buffer_num_bytes, (flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
}

MojoResult DataPipeProducerDispatcher::EndWriteDataImplNoLock(
    uint32_t num_bytes_written) {
  lock().AssertAcquired();

  return data_pipe_->ProducerEndWriteData(num_bytes_written);
}

HandleSignalsState DataPipeProducerDispatcher::GetHandleSignalsStateImplNoLock()
    const {
  lock().AssertAcquired();
  return data_pipe_->ProducerGetHandleSignalsState();
}

MojoResult DataPipeProducerDispatcher::AddAwakableImplNoLock(
    Awakable* awakable,
    MojoHandleSignals signals,
    uint32_t context,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  return data_pipe_->ProducerAddAwakable(awakable, signals, context,
                                         signals_state);
}

void DataPipeProducerDispatcher::RemoveAwakableImplNoLock(
    Awakable* awakable,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  data_pipe_->ProducerRemoveAwakable(awakable, signals_state);
}

void DataPipeProducerDispatcher::StartSerializeImplNoLock(
    Channel* channel,
    size_t* max_size,
    size_t* max_platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  data_pipe_->ProducerStartSerialize(channel, max_size, max_platform_handles);
}

bool DataPipeProducerDispatcher::EndSerializeAndCloseImplNoLock(
    Channel* channel,
    void* destination,
    size_t* actual_size,
    embedder::PlatformHandleVector* platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.

  bool rv = data_pipe_->ProducerEndSerialize(channel, destination, actual_size,
                                             platform_handles);
  data_pipe_ = nullptr;
  return rv;
}

bool DataPipeProducerDispatcher::IsBusyNoLock() const {
  lock().AssertAcquired();
  return data_pipe_->ProducerIsBusy();
}

}  // namespace system
}  // namespace mojo
