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

ExceptionCode WebCdmExceptionToExceptionCode(blink::WebContentDecryptionModuleException cdmException)
{
    switch (cdmException) {
    case blink::WebContentDecryptionModuleExceptionNotSupportedError:
        return NotSupportedError;
    case blink::WebContentDecryptionModuleExceptionInvalidStateError:
        return InvalidStateError;
    case blink::WebContentDecryptionModuleExceptionInvalidAccessError:
        return InvalidAccessError;
    case blink::WebContentDecryptionModuleExceptionQuotaExceededError:
        return QuotaExceededError;
    case blink::WebContentDecryptionModuleExceptionUnknownError:
        return UnknownError;
    case blink::WebContentDecryptionModuleExceptionClientError:
    case blink::WebContentDecryptionModuleExceptionOutputError:
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

void SimpleContentDecryptionModuleResult::completeWithSession(blink::WebContentDecryptionModuleResult::SessionStatus status)
{
    ASSERT_NOT_REACHED();
    completeWithDOMException(InvalidStateError, "Unexpected completion.");
}

void SimpleContentDecryptionModuleResult::completeWithError(blink::WebContentDecryptionModuleException exceptionCode, unsigned long systemCode, const blink::WebString& errorMessage)
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
