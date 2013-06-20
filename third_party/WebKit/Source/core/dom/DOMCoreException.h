/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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

#ifndef DOMCoreException_h
#define DOMCoreException_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ExceptionBase.h"

namespace WebCore {

// FIXME: This should be private once IDBDatabaseException is gone.
enum {
    IndexSizeErrorLegacyCode = 1,
    HierarchyRequestErrorLegacyCode = 3,
    WrongDocumentErrorLegacyCode = 4,
    InvalidCharacterErrorLegacyCode = 5,
    NoModificationAllowedErrorLegacyCode = 7,
    NotFoundErrorLegacyCode = 8,
    NotSupportedErrorLegacyCode = 9,
    InuseAttributeErrorLegacyCode = 10, // Historical. Only used in setAttributeNode etc which have been removed from the DOM specs.

    // Introduced in DOM Level 2:
    InvalidStateErrorLegacyCode = 11,
    SyntaxErrorLegacyCode = 12,
    InvalidModificationErrorLegacyCode = 13,
    NamespaceErrorLegacyCode = 14,
    InvalidAccessErrorLegacyCode = 15,

    // Introduced in DOM Level 3:
    TypeMismatchErrorLegacyCode = 17, // Historical; use TypeError instead

    // XMLHttpRequest extension:
    SecurityErrorLegacyCode = 18,

    // Others introduced in HTML5:
    NetworkErrorLegacyCode = 19,
    AbortErrorLegacyCode = 20,
    UrlMismatchErrorLegacyCode = 21,
    QuotaExceededErrorLegacyCode = 22,
    TimeoutErrorLegacyCode = 23,
    InvalidNodeTypeErrorLegacyCode = 24,
    DataCloneErrorLegacyCode = 25
};

class DOMCoreException : public ExceptionBase, public ScriptWrappable {
public:
    static PassRefPtr<DOMCoreException> create(const ExceptionCodeDescription& description)
    {
        return adoptRef(new DOMCoreException(description));
    }

    static bool initializeDescription(ExceptionCode, ExceptionCodeDescription*);
    static int getLegacyErrorCode(ExceptionCode);

private:
    explicit DOMCoreException(const ExceptionCodeDescription& description)
        : ExceptionBase(description)
    {
        ScriptWrappable::init(this);
    }
};

} // namespace WebCore

#endif // DOMCoreException_h
