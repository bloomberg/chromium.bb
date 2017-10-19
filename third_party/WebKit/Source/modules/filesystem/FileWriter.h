/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#ifndef FileWriter_h
#define FileWriter_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/FileError.h"
#include "modules/EventTargetModules.h"
#include "modules/filesystem/FileWriterBase.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebFileWriterClient.h"

namespace blink {

class Blob;
class ExceptionState;
class ExecutionContext;

class FileWriter final : public EventTargetWithInlineData,
                         public FileWriterBase,
                         public ActiveScriptWrappable<FileWriter>,
                         public ContextLifecycleObserver,
                         public WebFileWriterClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(FileWriter);
  USING_PRE_FINALIZER(FileWriter, Dispose);

 public:
  static FileWriter* Create(ExecutionContext*);
  ~FileWriter() override;

  enum ReadyState { kInit = 0, kWriting = 1, kDone = 2 };

  void write(Blob*, ExceptionState&);
  void seek(long long position, ExceptionState&);
  void truncate(long long length, ExceptionState&);
  void abort(ExceptionState&);
  ReadyState getReadyState() const { return ready_state_; }
  DOMException* error() const { return error_.Get(); }

  // WebFileWriterClient
  void DidWrite(long long bytes, bool complete) override;
  void DidTruncate() override;
  void DidFail(WebFileError) override;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override {
    return ContextLifecycleObserver::GetExecutionContext();
  }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(writestart);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(progress);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(write);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(writeend);

  virtual void Trace(blink::Visitor*);

 private:
  enum Operation {
    kOperationNone,
    kOperationWrite,
    kOperationTruncate,
    kOperationAbort
  };

  explicit FileWriter(ExecutionContext*);

  void CompleteAbort();

  void DoOperation(Operation);

  void SignalCompletion(FileError::ErrorCode);

  void FireEvent(const AtomicString& type);

  void SetError(FileError::ErrorCode, ExceptionState&);

  void Dispose();

  Member<DOMException> error_;
  ReadyState ready_state_;
  Operation operation_in_progress_;
  Operation queued_operation_;
  long long bytes_written_;
  long long bytes_to_write_;
  long long truncate_length_;
  long long num_aborts_;
  long long recursion_depth_;
  double last_progress_notification_time_ms_;
  Member<Blob> blob_being_written_;
};

}  // namespace blink

#endif  // FileWriter_h
