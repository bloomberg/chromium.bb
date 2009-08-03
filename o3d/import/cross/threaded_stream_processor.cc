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

// ThreadedStreamProcessor forwards data from one thread to
// another processor that will run on a new thread owned by the
// ThreadedStreamProcessor.

#include "import/cross/threaded_stream_processor.h"
#include "base/task.h"

namespace o3d {

ThreadedStreamProcessor::ThreadedStreamProcessor(StreamProcessor *receiver)
    : receiver_(receiver),
      thread_("ThreadedStreamProcessor"),
      status_(IN_PROGRESS) {
}

ThreadedStreamProcessor::~ThreadedStreamProcessor() {
  // Wait for the other thread to stop so it does not access this object after
  // it has been destroyed.
  StopThread();
}

void ThreadedStreamProcessor::StartThread() {
  if (!thread_.IsRunning()) {
    thread_.Start();
  }
}

void ThreadedStreamProcessor::StopThread() {
  if (thread_.IsRunning()) {
    thread_.Stop();
  }
}

StreamProcessor::Status ThreadedStreamProcessor::ProcessBytes(
    MemoryReadStream *stream,
    size_t bytes_to_process) {
  // Report any error on the other thread.
  if (status_ == FAILURE) {
    return status_;
  }

  StartThread();

  // Copy the bytes. They are deleted by the other thread when it has processed
  // them.
  uint8* copy = new uint8[bytes_to_process];
  stream->Read(copy, bytes_to_process);

  // Post a task to call ForwardBytes on the other thread.
  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableFunction(
      &ThreadedStreamProcessor::ForwardBytes, this, copy, bytes_to_process));

  return IN_PROGRESS;
}

void ThreadedStreamProcessor::Close(bool success) {
  StartThread();

  // Post a task to call ForwardClose on the other thread.
  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableFunction(
      &ThreadedStreamProcessor::ForwardClose, this, success));
}

void ThreadedStreamProcessor::ForwardBytes(
    ThreadedStreamProcessor* processor, const uint8* bytes, size_t size) {
  // Do not forward if an error has ocurred. Just delete the buffer.
  if (processor->status_ == FAILURE) {
    delete[] bytes;
    return;
  }

  // Pass bytes to the receiver.
  MemoryReadStream stream(bytes, size);
  processor->status_ = processor->receiver_->ProcessBytes(&stream, size);

  // Delete the buffer once the receiver is finished with them.
  delete[] bytes;
}

void ThreadedStreamProcessor::ForwardClose(ThreadedStreamProcessor* processor,
                                         bool success) {
  processor->receiver_->Close(success && (processor->status_ != FAILURE));
}

}  // namespace o3d
