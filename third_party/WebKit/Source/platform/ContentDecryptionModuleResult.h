// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentDecryptionModuleResult_h
#define ContentDecryptionModuleResult_h

#include "platform/heap/Handle.h"
#include "public/platform/WebContentDecryptionModuleException.h"
#include "public/platform/WebContentDecryptionModuleResult.h"
#include "public/platform/WebEncryptedMediaKeyInformation.h"

namespace blink {

class WebContentDecryptionModule;
class WebString;

// Used to notify completion of a CDM operation.
class ContentDecryptionModuleResult
    : public GarbageCollectedFinalized<ContentDecryptionModuleResult> {
 public:
  virtual ~ContentDecryptionModuleResult() {}

  virtual void Complete() = 0;
  virtual void CompleteWithContentDecryptionModule(
      WebContentDecryptionModule*) = 0;
  virtual void CompleteWithSession(
      WebContentDecryptionModuleResult::SessionStatus) = 0;
  virtual void CompleteWithKeyStatus(
      WebEncryptedMediaKeyInformation::KeyStatus) = 0;
  virtual void CompleteWithError(WebContentDecryptionModuleException,
                                 unsigned long system_code,
                                 const WebString&) = 0;

  WebContentDecryptionModuleResult Result() {
    return WebContentDecryptionModuleResult(this);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // ContentDecryptionModuleResult_h
