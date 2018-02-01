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

#include "core/fileapi/FileError.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "platform/heap/Handle.h"

namespace blink {

// Helper class to support DOMFileSystemSync implementation.
template <typename SuccessCallback, typename CallbackArg>
class DOMFileSystemCallbacksSyncHelper final
    : public GarbageCollected<
          DOMFileSystemCallbacksSyncHelper<SuccessCallback, CallbackArg>> {
 public:
  static DOMFileSystemCallbacksSyncHelper* Create() {
    return new DOMFileSystemCallbacksSyncHelper();
  }

  void Trace(blink::Visitor* visitor) { visitor->Trace(result_); }

  SuccessCallback* GetSuccessCallback() {
    return new SuccessCallbackImpl(this);
  }
  ErrorCallbackBase* GetErrorCallback() { return new ErrorCallbackImpl(this); }

  CallbackArg* GetResultOrThrow(ExceptionState& exception_state) {
    if (error_code_ != FileError::ErrorCode::kOK) {
      FileError::ThrowDOMException(exception_state, error_code_);
      return nullptr;
    }

    return result_;
  }

 private:
  class SuccessCallbackImpl final : public SuccessCallback {
   public:
    void Trace(blink::Visitor* visitor) override {
      visitor->Trace(helper_);
      SuccessCallback::Trace(visitor);
    }
    void OnSuccess(CallbackArg* arg) override {
      DCHECK(arg);
      helper_->result_ = arg;
    }

   private:
    explicit SuccessCallbackImpl(DOMFileSystemCallbacksSyncHelper* helper)
        : helper_(helper) {}
    Member<DOMFileSystemCallbacksSyncHelper> helper_;

    friend class DOMFileSystemCallbacksSyncHelper;
  };

  class ErrorCallbackImpl final : public ErrorCallbackBase {
   public:
    void Trace(blink::Visitor* visitor) override {
      visitor->Trace(helper_);
      ErrorCallbackBase::Trace(visitor);
    }
    void Invoke(FileError::ErrorCode error_code) override {
      DCHECK_NE(error_code, FileError::ErrorCode::kOK);
      helper_->error_code_ = error_code;
    }

   private:
    explicit ErrorCallbackImpl(DOMFileSystemCallbacksSyncHelper* helper)
        : helper_(helper) {}
    Member<DOMFileSystemCallbacksSyncHelper> helper_;

    friend class DOMFileSystemCallbacksSyncHelper;
  };

  DOMFileSystemCallbacksSyncHelper() = default;

  Member<CallbackArg> result_;
  FileError::ErrorCode error_code_ = FileError::ErrorCode::kOK;

  friend class SuccessCallbackImpl;
  friend class ErrorCallbackImpl;
};

using EntryCallbacksSyncHelper =
    DOMFileSystemCallbacksSyncHelper<EntryCallbacks::OnDidGetEntryCallback,
                                     Entry>;

using FileSystemCallbacksSyncHelper = DOMFileSystemCallbacksSyncHelper<
    FileSystemCallbacks::OnDidOpenFileSystemCallback,
    DOMFileSystem>;

using FileWriterCallbacksSyncHelper = DOMFileSystemCallbacksSyncHelper<
    FileWriterCallbacks::OnDidCreateFileWriterCallback,
    FileWriterBase>;

using MetadataCallbacksSyncHelper = DOMFileSystemCallbacksSyncHelper<
    MetadataCallbacks::OnDidReadMetadataCallback,
    Metadata>;

using VoidCallbacksSyncHelper = DOMFileSystemCallbacksSyncHelper<
    VoidCallbacks::OnDidSucceedCallback,
    ExecutionContext /* dummy_arg_for_sync_helper */>;

}  // namespace blink

#endif  // SyncCallbackHelper_h
