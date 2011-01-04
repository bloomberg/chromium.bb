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

#ifndef O3D_IMPORT_CROSS_MAIN_THREAD_ARCHIVE_CALLBACK_CLIENT_H_
#define O3D_IMPORT_CROSS_MAIN_THREAD_ARCHIVE_CALLBACK_CLIENT_H_

#include "base/basictypes.h"
#include "base/thread.h"
#include "import/cross/archive_processor.h"
#include "import/cross/memory_stream.h"

namespace o3d {

class IMainThreadTaskPoster;
class ServiceLocator;

class MainThreadArchiveCallbackClient : public ArchiveCallbackClient {
 public:
  MainThreadArchiveCallbackClient(ServiceLocator* service_locator,
                                  ArchiveCallbackClient *receiver);
  virtual ~MainThreadArchiveCallbackClient();

  virtual void ReceiveFileHeader(const ArchiveFileInfo &file_info);
  virtual bool ReceiveFileData(MemoryReadStream *stream, size_t size);
  virtual void Close(bool success);

 private:
  struct Success : ::base::RefCountedThreadSafe< Success > {
    bool value;
  };

  typedef scoped_refptr<Success> SuccessPtr;

  static void ForwardReceiveFileHeader(SuccessPtr success,
                                       ArchiveCallbackClient* client,
                                       ArchiveFileInfo file_info);

  static void ForwardReceiveFileData(SuccessPtr success,
                                     ArchiveCallbackClient* client,
                                     uint8* bytes, size_t size);

  static void ForwardClose(SuccessPtr success_ptr,
                           ArchiveCallbackClient* client,
                           bool success);

  IMainThreadTaskPoster* main_thread_task_poster_;
  ArchiveCallbackClient* receiver_;
  SuccessPtr success_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadArchiveCallbackClient);
};
}  // namespace o3d

#endif  //  O3D_IMPORT_CROSS_MAIN_THREAD_ARCHIVE_CALLBACK_CLIENT_H_
