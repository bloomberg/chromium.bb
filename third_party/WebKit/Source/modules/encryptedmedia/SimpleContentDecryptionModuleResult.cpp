// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/encryptedmedia/SimpleContentDecryptionModuleResult.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMException.h"
#include "platform/Logging.h"
#include "public/platform/WebString.h"
#include "wtf/Assertions.h"

namespace blink {

ExceptionCode WebCdmExceptionToExceptionCode(WebContentDecryptionModuleException cdmException)
{
    switch (cdmException) {
    case WebContentDecryptionModuleExceptionNotSupportedError:
        return NotSupportedError;
    case WebContentDecryptionModuleExceptionInvalidStateError:
        return InvalidStateError;
    case WebContentDecryptionModuleExceptionInvalidAccessError:
        return InvalidAccessError;
    case WebContentDecryptionModuleExceptionQuotaExceededError:
        return QuotaExceededError;
    case WebContentDecryptionModuleExceptionUnknownError:
        return UnknownError;
    case WebContentDecryptionModuleExceptionClientError:
    case WebContentDecryptionModuleExceptionOutputError:
        // Currently no matching DOMException for these 2 errors.
        // FIXME: Update DOMException to handle these if actually added to
        // the EME spec.
        return UnknownError;
    }

    ASSERT_NOT_REACHED();
    return UnknownError;
}

SimpleContentDecryptionModuleResult::SimpleContentDecryptionModuleResult(ScriptState* scriptState)
    : m_resolver(ScriptPromiseResolver::create(scriptState))
{
}

SimpleContentDecryptionModuleResult::~SimpleContentDecryptionModuleResult()
{
}

void SimpleContentDecryptionModuleResult::complete()
{
    m_resolver->resolve(V8UndefinedType());
    m_resolver.clear();
}

void SimpleContentDecryptionModuleResult::completeWithSession(WebContentDecryptionModuleResult::SessionStatus status)
{
    ASSERT_NOT_REACHED();
    completeWithDOMException(InvalidStateError, "Unexpected completion.");
}

void SimpleContentDecryptionModuleResult::completeWithError(WebContentDecryptionModuleException exceptionCode, unsigned long systemCode, const WebString& errorMessage)
{
    completeWithDOMException(WebCdmExceptionToExceptionCode(exceptionCode), errorMessage);
}

ScriptPromise SimpleContentDecryptionModuleResult::promise()
{
    return m_resolver->promise();
}

void SimpleContentDecryptionModuleResult::completeWithDOMException(ExceptionCode code, const String& errorMessage)
{
    m_resolver->reject(DOMException::create(code, errorMessage));
    m_resolver.clear();
}

} // namespace blink
