/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef LocalFileSystem_h
#define LocalFileSystem_h

#include <memory>
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerClients.h"
#include "platform/FileSystemType.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Functional.h"

namespace blink {

class AsyncFileSystemCallbacks;
class CallbackWrapper;
class FileSystemClient;
class ExecutionContext;
class KURL;
class WebFileSystem;

class LocalFileSystem final : public GarbageCollectedFinalized<LocalFileSystem>,
                              public Supplement<LocalFrame>,
                              public Supplement<WorkerClients>,
                              public TraceWrapperBase {
  USING_GARBAGE_COLLECTED_MIXIN(LocalFileSystem);
  WTF_MAKE_NONCOPYABLE(LocalFileSystem);

 public:
  LocalFileSystem(LocalFrame&, std::unique_ptr<FileSystemClient>);
  LocalFileSystem(WorkerClients&, std::unique_ptr<FileSystemClient>);
  ~LocalFileSystem();

  void ResolveURL(ExecutionContext*,
                  const KURL&,
                  std::unique_ptr<AsyncFileSystemCallbacks>);
  void RequestFileSystem(ExecutionContext*,
                         FileSystemType,
                         long long size,
                         std::unique_ptr<AsyncFileSystemCallbacks>);

  FileSystemClient& Client() const { return *client_; }

  static const char* SupplementName();
  static LocalFileSystem* From(ExecutionContext&);

  virtual void Trace(blink::Visitor*);

 private:
  WebFileSystem* GetFileSystem() const;
  void FileSystemNotAvailable(ExecutionContext*, CallbackWrapper*);

  void RequestFileSystemAccessInternal(ExecutionContext*,
                                       WTF::Closure allowed,
                                       WTF::Closure denied);
  void FileSystemNotAllowedInternal(ExecutionContext*, CallbackWrapper*);
  void FileSystemAllowedInternal(ExecutionContext*,
                                 FileSystemType,
                                 CallbackWrapper*);
  void ResolveURLInternal(ExecutionContext*, const KURL&, CallbackWrapper*);

  const std::unique_ptr<FileSystemClient> client_;
};

}  // namespace blink

#endif  // LocalFileSystem_h
