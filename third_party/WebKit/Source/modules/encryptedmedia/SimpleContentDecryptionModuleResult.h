// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimpleContentDecryptionModuleResult_h
#define SimpleContentDecryptionModuleResult_h

#include "core/dom/ExceptionCode.h"
#include "platform/ContentDecryptionModuleResult.h"
#include "wtf/Forward.h"

namespace blink {

class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class WebString;

ExceptionCode WebCdmExceptionToExceptionCode(WebContentDecryptionModuleException);

// This class wraps the promise resolver and is passed (indirectly) to Chromium
// to fullfill the promise. This implementation of complete() will resolve the
// promise with undefined, while completeWithError() reject the promise with an
// exception. completeWithSession() is not expected to be called, and will
// reject the promise.
class SimpleContentDecryptionModuleResult : public ContentDecryptionModuleResult {
public:
    explicit SimpleContentDecryptionModuleResult(ScriptState*);
    virtual ~SimpleContentDecryptionModuleResult();

    // ContentDecryptionModuleResult implementation.
    virtual void complete() override;
    virtual void completeWithSession(WebContentDecryptionModuleResult::SessionStatus) override;
    virtual void completeWithError(WebContentDecryptionModuleException, unsigned long systemCode, const WebString&) override;

    // It is only valid to call this before completion.
    ScriptPromise promise();

private:
    // Reject the promise with a DOMException.
    void completeWithDOMException(ExceptionCode, const String& errorMessage);

    RefPtr<ScriptPromiseResolver> m_resolver;
};

} // namespace blink

#endif // SimpleContentDecryptionModuleResult_h
