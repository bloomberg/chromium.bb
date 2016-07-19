/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/fileapi/FileError.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

namespace FileError {

const char abortErrorMessage[] = "An ongoing operation was aborted, typically with a call to abort().";
const char encodingErrorMessage[] = "A URI supplied to the API was malformed, or the resulting Data URL has exceeded the URL length limitations for Data URLs.";
const char invalidStateErrorMessage[] = "An operation that depends on state cached in an interface object was made but the state had changed since it was read from disk.";
const char noModificationAllowedErrorMessage[] = "An attempt was made to write to a file or directory which could not be modified due to the state of the underlying filesystem.";
const char notFoundErrorMessage[] = "A requested file or directory could not be found at the time an operation was processed.";
const char notReadableErrorMessage[] = "The requested file could not be read, typically due to permission problems that have occurred after a reference to a file was acquired.";
const char pathExistsErrorMessage[] = "An attempt was made to create a file or directory where an element already exists.";
const char quotaExceededErrorMessage[] = "The operation failed because it would cause the application to exceed its storage quota.";
const char securityErrorMessage[] = "It was determined that certain files are unsafe for access within a Web application, or that too many calls are being made on file resources.";
const char syntaxErrorMessage[] = "An invalid or unsupported argument was given, like an invalid line ending specifier.";
const char typeMismatchErrorMessage[] = "The path supplied exists, but was not an entry of requested type.";

namespace {

ExceptionCode errorCodeToExceptionCode(ErrorCode code)
{
    switch (code) {
    case OK:
        return 0;
    case NOT_FOUND_ERR:
        return NotFoundError;
    case SECURITY_ERR:
        return SecurityError;
    case ABORT_ERR:
        return AbortError;
    case NOT_READABLE_ERR:
        return NotReadableError;
    case ENCODING_ERR:
        return EncodingError;
    case NO_MODIFICATION_ALLOWED_ERR:
        return NoModificationAllowedError;
    case INVALID_STATE_ERR:
        return InvalidStateError;
    case SYNTAX_ERR:
        return SyntaxError;
    case INVALID_MODIFICATION_ERR:
        return InvalidModificationError;
    case QUOTA_EXCEEDED_ERR:
        return QuotaExceededError;
    case TYPE_MISMATCH_ERR:
        return TypeMismatchError;
    case PATH_EXISTS_ERR:
        return PathExistsError;
    default:
        ASSERT_NOT_REACHED();
        return code;
    }
}

const char* errorCodeToMessage(ErrorCode code)
{
    // Note that some of these do not set message. If message is 0 then the default message is used.
    switch (code) {
    case OK:
        return 0;
    case SECURITY_ERR:
        return securityErrorMessage;
    case NOT_FOUND_ERR:
        return notFoundErrorMessage;
    case ABORT_ERR:
        return abortErrorMessage;
    case NOT_READABLE_ERR:
        return notReadableErrorMessage;
    case ENCODING_ERR:
        return encodingErrorMessage;
    case NO_MODIFICATION_ALLOWED_ERR:
        return noModificationAllowedErrorMessage;
    case INVALID_STATE_ERR:
        return invalidStateErrorMessage;
    case SYNTAX_ERR:
        return syntaxErrorMessage;
    case INVALID_MODIFICATION_ERR:
        return 0;
    case QUOTA_EXCEEDED_ERR:
        return quotaExceededErrorMessage;
    case TYPE_MISMATCH_ERR:
        return 0;
    case PATH_EXISTS_ERR:
        return pathExistsErrorMessage;
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

} // namespace

void throwDOMException(ExceptionState& exceptionState, ErrorCode code)
{
    if (code == OK)
        return;

    // SecurityError is special-cased, as we want to route those exceptions through ExceptionState::throwSecurityError.
    if (code == SECURITY_ERR) {
        exceptionState.throwSecurityError(securityErrorMessage);
        return;
    }

    exceptionState.throwDOMException(errorCodeToExceptionCode(code), errorCodeToMessage(code));
}

DOMException* createDOMException(ErrorCode code)
{
    DCHECK_NE(code, OK);
    return DOMException::create(errorCodeToExceptionCode(code), errorCodeToMessage(code));
}

} // namespace FileError

} // namespace blink
