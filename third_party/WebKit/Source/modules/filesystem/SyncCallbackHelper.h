/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#ifndef SyncCallbackHelper_h
#define SyncCallbackHelper_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/fileapi/FileError.h"
#include "core/html/VoidCallback.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/EntriesCallback.h"
#include "modules/filesystem/EntryCallback.h"
#include "modules/filesystem/EntrySync.h"
#include "modules/filesystem/FileEntry.h"
#include "modules/filesystem/FileSystemCallback.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/MetadataCallback.h"
#include "platform/heap/Handle.h"

namespace blink {

// A helper template for FileSystemSync implementation.
template <typename SuccessCallback, typename CallbackArg, typename ResultType>
class SyncCallbackHelper final
    : public GarbageCollected<
          SyncCallbackHelper<SuccessCallback, CallbackArg, ResultType>> {
 public:
  typedef SyncCallbackHelper<SuccessCallback, CallbackArg, ResultType>
      HelperType;

  static HelperType* Create() { return new SyncCallbackHelper(); }

  ResultType* GetResult(ExceptionState& exception_state) {
    if (error_code_)
      FileError::ThrowDOMException(exception_state, error_code_);

    return result_;
  }

  SuccessCallback* GetSuccessCallback() {
    return SuccessCallbackImpl::Create(this);
  }
  ErrorCallbackBase* GetErrorCallback() {
    return ErrorCallbackImpl::Create(this);
  }

  void Trace(blink::Visitor* visitor) { visitor->Trace(result_); }

 private:
  SyncCallbackHelper() : error_code_(FileError::kOK), completed_(false) {}

  class SuccessCallbackImpl final : public SuccessCallback {
   public:
    static SuccessCallbackImpl* Create(HelperType* helper) {
      return new SuccessCallbackImpl(helper);
    }

    virtual void handleEvent() { helper_->SetError(FileError::kOK); }

    virtual void handleEvent(CallbackArg arg) { helper_->SetResult(arg); }

    void Trace(blink::Visitor* visitor) {
      visitor->Trace(helper_);
      SuccessCallback::Trace(visitor);
    }

   private:
    explicit SuccessCallbackImpl(HelperType* helper) : helper_(helper) {}
    Member<HelperType> helper_;
  };

  class ErrorCallbackImpl final : public ErrorCallbackBase {
   public:
    static ErrorCallbackImpl* Create(HelperType* helper) {
      return new ErrorCallbackImpl(helper);
    }

    void Invoke(FileError::ErrorCode error) override {
      helper_->SetError(error);
    }

    void Trace(blink::Visitor* visitor) {
      visitor->Trace(helper_);
      ErrorCallbackBase::Trace(visitor);
    }

   private:
    explicit ErrorCallbackImpl(HelperType* helper) : helper_(helper) {}
    Member<HelperType> helper_;
  };

  void SetError(FileError::ErrorCode error) {
    error_code_ = error;
    completed_ = true;
  }

  void SetResult(CallbackArg result) {
    result_ = ResultType::Create(result);
    completed_ = true;
  }

  Member<ResultType> result_;
  FileError::ErrorCode error_code_;
  bool completed_;
};

struct EmptyType : public GarbageCollected<EmptyType> {
  static EmptyType* Create(EmptyType*) { return 0; }

  void Trace(blink::Visitor* visitor) {}
};

typedef SyncCallbackHelper<EntryCallback, Entry*, EntrySync>
    EntrySyncCallbackHelper;
typedef SyncCallbackHelper<MetadataCallback, Metadata*, Metadata>
    MetadataSyncCallbackHelper;
typedef SyncCallbackHelper<VoidCallback, EmptyType*, EmptyType>
    VoidSyncCallbackHelper;
typedef SyncCallbackHelper<FileSystemCallback,
                           DOMFileSystem*,
                           DOMFileSystemSync>
    FileSystemSyncCallbackHelper;

}  // namespace blink

#endif  // SyncCallbackHelper_h
