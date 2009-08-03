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

// MainThreadArchiveCallbackClient forwards data from one thread to
// another client that will run on the main thread.

#include "core/cross/imain_thread_task_poster.h"
#include "core/cross/service_locator.h"
#include "import/cross/main_thread_archive_callback_client.h"
#include "base/task.h"

namespace o3d {

// Called on main thread.
MainThreadArchiveCallbackClient::MainThreadArchiveCallbackClient(
    ServiceLocator* service_locator,
    ArchiveCallbackClient *receiver)
    : main_thread_task_poster_(
          service_locator->GetService<IMainThreadTaskPoster>()),
      receiver_(receiver),
      success_(new Success) {
  success_->value = true;
}

// Called on main thread.
MainThreadArchiveCallbackClient::~MainThreadArchiveCallbackClient() {
  // Signal pending tasks on main thread not to call the receiver. They are not
  // running concurrently because this destructor is also called on the main
  // thread.
  success_->value = false;
}

// Called on other thread.
void MainThreadArchiveCallbackClient::ReceiveFileHeader(
    const ArchiveFileInfo &file_info) {
  main_thread_task_poster_->PostTask(NewRunnableFunction(
      &MainThreadArchiveCallbackClient::ForwardReceiveFileHeader,
      success_,
      receiver_,
      file_info));
}

// Called on other thread.
bool MainThreadArchiveCallbackClient::ReceiveFileData(
    MemoryReadStream *stream, size_t size) {
  // Copy stream into temporary buffer. Deleted after it is received by the main
  // thread.
  uint8* buffer = new uint8[size];
  stream->Read(buffer, size);

  main_thread_task_poster_->PostTask(NewRunnableFunction(
      &MainThreadArchiveCallbackClient::ForwardReceiveFileData,
      success_,
      receiver_,
      buffer,
      size));

  return success_->value;
}

void MainThreadArchiveCallbackClient::Close(bool success) {
  main_thread_task_poster_->PostTask(NewRunnableFunction(
      &MainThreadArchiveCallbackClient::ForwardClose,
      success_,
      receiver_,
      success));
}

// Called on main thread.
void MainThreadArchiveCallbackClient::ForwardReceiveFileHeader(
    SuccessPtr success,
    ArchiveCallbackClient* client,
    ArchiveFileInfo file_info) {
  if (success->value) {
    client->ReceiveFileHeader(file_info);
  }
}

// Called on main thread.
void MainThreadArchiveCallbackClient::ForwardReceiveFileData(
    SuccessPtr success,
    ArchiveCallbackClient* client,
    uint8* bytes, size_t size) {
  // Just delete the buffer if there was a previous failure.
  if (!success->value) {
    delete[] bytes;
    return;
  }

  MemoryReadStream stream(bytes, size);
  if (!client->ReceiveFileData(&stream, size)) {
    // Do not set this to true on success. That might overwrite a false written
    // by the other thread intended to indicate that the receiver is no
    // longer valid.
    success->value = false;
  }

  // Delete temporary buffer allocated by other thread when the main thread is
  // finished with it.
  delete[] bytes;
}

void MainThreadArchiveCallbackClient::ForwardClose(
    SuccessPtr success_ptr,
    ArchiveCallbackClient* client,
    bool success) {
  client->Close(success && success_ptr->value);
}

}  // namespace o3d
