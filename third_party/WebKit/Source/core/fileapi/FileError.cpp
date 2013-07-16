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

#include "config.h"
#include "core/fileapi/FileError.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"

namespace WebCore {

const char FileError::abortErrorMessage[] = "An ongoing operation was aborted, typically with a call to abort().";
const char FileError::encodingErrorMessage[] = "A URI supplied to the API was malformed, or the resulting Data URL has exceeded the URL length limitations for Data URLs.";
const char FileError::invalidStateErrorMessage[] = "An operation that depends on state cached in an interface object was made but the state had changed since it was read from disk.";
const char FileError::noModificationAllowedErrorMessage[] = "An attempt was made to write to a file or directory which could not be modified due to the state of the underlying filesystem.";
const char FileError::notFoundErrorMessage[] = "A requested file or directory could not be found at the time an operation was processed.";
const char FileError::notReadableErrorMessage[] = "The requested file could not be read, typically due to permission problems that have occurred after a reference to a file was acquired.";
const char FileError::pathExistsErrorMessage[] = "An attempt was made to create a file or directory where an element already exists.";
const char FileError::quotaExceededErrorMessage[] = "The operation failed because it would cause the application to exceed its storage quota.";
const char FileError::securityErrorMessage[] = "It was determined that certain files are unsafe for access within a Web application, or that too many calls are being made on file resources.";
const char FileError::syntaxErrorMessage[] = "An invalid or unsupported argument was given, like an invalid line ending specifier.";
const char FileError::typeMismatchErrorMessage[] = "The path supplied exists, but was not an entry of requested type.";

void FileError::throwDOMException(ExceptionState& es, ErrorCode code)
{
    if (code == FileError::OK)
        return;

    ExceptionCode ec;
    const char* message = 0;

    // Note that some of these do not set message. If message is 0 then the default message is used.
    switch (code) {
    case FileError::NOT_FOUND_ERR:
        ec = NotFoundError;
        message = FileError::notFoundErrorMessage;
        break;
    case FileError::SECURITY_ERR:
        ec = SecurityError;
        message = FileError::securityErrorMessage;
        break;
    case FileError::ABORT_ERR:
        ec = AbortError;
        message = FileError::abortErrorMessage;
        break;
    case FileError::NOT_READABLE_ERR:
        ec = NotReadableError;
        message = FileError::notReadableErrorMessage;
        break;
    case FileError::ENCODING_ERR:
        ec = EncodingError;
        message = FileError::encodingErrorMessage;
        break;
    case FileError::NO_MODIFICATION_ALLOWED_ERR:
        ec = NoModificationAllowedError;
        message = FileError::noModificationAllowedErrorMessage;
        break;
    case FileError::INVALID_STATE_ERR:
        ec = InvalidStateError;
        message = FileError::invalidStateErrorMessage;
        break;
    case FileError::SYNTAX_ERR:
        ec = SyntaxError;
        message = FileError::syntaxErrorMessage;
        break;
    case FileError::INVALID_MODIFICATION_ERR:
        ec = InvalidModificationError;
        break;
    case FileError::QUOTA_EXCEEDED_ERR:
        ec = QuotaExceededError;
        message = FileError::quotaExceededErrorMessage;
        break;
    case FileError::TYPE_MISMATCH_ERR:
        ec = TypeMismatchError;
        break;
    case FileError::PATH_EXISTS_ERR:
        ec = PathExistsError;
        message = FileError::pathExistsErrorMessage;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    es.throwDOMException(ec, message);
}

} // namespace WebCore
