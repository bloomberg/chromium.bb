/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/DOMCoreException.h"

#include "DOMException.h"
#include "ExceptionCode.h"

namespace WebCore {

static struct CoreException {
    const char* const name;
    const char* const description;
    const int code;
} coreExceptions[] = {
    { "IndexSizeError", "Index or size was negative, or greater than the allowed value.", IndexSizeErrorLegacyCode },
    { "HierarchyRequestError", "A Node was inserted somewhere it doesn't belong.", HierarchyRequestErrorLegacyCode },
    { "WrongDocumentError", "A Node was used in a different document than the one that created it (that doesn't support it).", WrongDocumentErrorLegacyCode },
    { "InvalidCharacterError", "An invalid or illegal character was specified, such as in an XML name.", InvalidCharacterErrorLegacyCode },
    { "NoModificationAllowedError", "An attempt was made to modify an object where modifications are not allowed.", NoModificationAllowedErrorLegacyCode },
    { "NotFoundError", "An attempt was made to reference a Node in a context where it does not exist.", NotFoundErrorLegacyCode },
    { "NotSupportedError", "The implementation did not support the requested type of object or operation.", NotSupportedErrorLegacyCode },
    { "InUseAttributeError", "An attempt was made to add an attribute that is already in use elsewhere.", InuseAttributeErrorLegacyCode },
    { "InvalidStateError", "An attempt was made to use an object that is not, or is no longer, usable.", InvalidStateErrorLegacyCode },
    { "SyntaxError", "An invalid or illegal string was specified.", SyntaxErrorLegacyCode },
    { "InvalidModificationError", "An attempt was made to modify the type of the underlying object.", InvalidModificationErrorLegacyCode },
    { "NamespaceError", "An attempt was made to create or change an object in a way which is incorrect with regard to namespaces.", NamespaceErrorLegacyCode },
    { "InvalidAccessError", "A parameter or an operation was not supported by the underlying object.", InvalidAccessErrorLegacyCode },
    { "TypeMismatchError", "The type of an object was incompatible with the expected type of the parameter associated to the object.", TypeMismatchErrorLegacyCode },
    { "SecurityError", "An attempt was made to break through the security policy of the user agent.", SecurityErrorLegacyCode },
    // FIXME: Couldn't find a description in the HTML/DOM specifications for NETWORKERR, ABORTERR, URLMISMATCHERR, and QUOTAEXCEEDEDERR
    { "NetworkError", "A network error occurred.", NetworkErrorLegacyCode },
    { "AbortError", "The user aborted a request.", AbortErrorLegacyCode },
    { "URLMismatchError", "A worker global scope represented an absolute URL that is not equal to the resulting absolute URL.", UrlMismatchErrorLegacyCode },
    { "QuotaExceededError", "An attempt was made to add something to storage that exceeded the quota.", QuotaExceededErrorLegacyCode },
    { "TimeoutError", "A timeout occurred.", TimeoutErrorLegacyCode },
    { "InvalidNodeTypeError", "The supplied node is invalid or has an invalid ancestor for this operation.", InvalidNodeTypeErrorLegacyCode },
    { "DataCloneError", "An object could not be cloned.", DataCloneErrorLegacyCode }
};

static const CoreException* getErrorEntry(ExceptionCode ec)
{
    size_t tableSize = WTF_ARRAY_LENGTH(coreExceptions);
    size_t tableIndex = ec - INDEX_SIZE_ERR;

    return tableIndex < tableSize ? &coreExceptions[tableIndex] : 0;
}

bool DOMCoreException::initializeDescription(ExceptionCode ec, ExceptionCodeDescription* description)
{
    const CoreException* entry = getErrorEntry(ec);
    if (!entry)
        return false;

    description->type = DOMCoreExceptionType;
    description->name = entry->name;
    description->description = entry->description;
    description->code = entry->code;

    return true;
}

int DOMCoreException::getLegacyErrorCode(ExceptionCode ec)
{
    const CoreException* entry = getErrorEntry(ec);
    ASSERT(entry);

    return (entry && entry->code) ? entry->code : 0;
}

} // namespace WebCore
