// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentDecryptionModuleResultPromise_h
#define ContentDecryptionModuleResultPromise_h

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ExceptionCode.h"
#include "platform/ContentDecryptionModuleResult.h"
#include "public/platform/WebEncryptedMediaKeyInformation.h"

namespace blink {

ExceptionCode WebCdmExceptionToExceptionCode(
    WebContentDecryptionModuleException);

// This class wraps the promise resolver to simplify creation of
// ContentDecryptionModuleResult objects. The default implementations of the
// complete(), completeWithSession(), etc. methods will reject the promise
// with an error. It needs to be subclassed and the appropriate complete()
// method overridden to resolve the promise as needed.
//
// Subclasses need to keep a Member<> to the object that created them so
// that the creator remains around as long as this promise is pending. This
// promise is not referenced by the object that created it (e.g MediaKeys,
// MediaKeySession, Navigator.requestMediaKeySystemAccess), so this promise
// may be cleaned up before or after it's creator once both become unreachable.
// If it is after, the destruction of the creator may trigger this promise,
// so use isValidToFulfillPromise() to verify that it is safe to fulfill
// the promise.
class ContentDecryptionModuleResultPromise
    : public ContentDecryptionModuleResult {
 public:
  ~ContentDecryptionModuleResultPromise() override;

  // ContentDecryptionModuleResult implementation.
  void Complete() override;
  void CompleteWithContentDecryptionModule(
      WebContentDecryptionModule*) override;
  void CompleteWithSession(
      WebContentDecryptionModuleResult::SessionStatus) override;
  void CompleteWithKeyStatus(
      WebEncryptedMediaKeyInformation::KeyStatus) override;
  void CompleteWithError(WebContentDecryptionModuleException,
                         unsigned long system_code,
                         const WebString&) override;

  // It is only valid to call this before completion.
  ScriptPromise Promise();

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit ContentDecryptionModuleResultPromise(ScriptState*);

  // Resolves the promise with |value|. Used by subclasses to resolve the
  // promise.
  template <typename... T>
  void Resolve(T... value) {
    DCHECK(IsValidToFulfillPromise());

    resolver_->Resolve(value...);
    resolver_.Clear();
  }

  // Rejects the promise with a DOMException.
  void Reject(ExceptionCode, const String& error_message);

  ExecutionContext* GetExecutionContext() const;

  // Determine if it's OK to resolve/reject this promise.
  bool IsValidToFulfillPromise();

 private:
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // ContentDecryptionModuleResultPromise_h
