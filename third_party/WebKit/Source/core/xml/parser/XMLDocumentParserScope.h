/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XMLDocumentParserScope_h
#define XMLDocumentParserScope_h

#include <libxml/xmlerror.h>
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class Document;

class XMLDocumentParserScope {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(XMLDocumentParserScope);

 public:
  explicit XMLDocumentParserScope(Document*);
  XMLDocumentParserScope(Document*,
                         xmlGenericErrorFunc,
                         xmlStructuredErrorFunc = nullptr,
                         void* error_context = nullptr);
  ~XMLDocumentParserScope();

  static Document* current_document_;

 private:
  Member<Document> old_document_;

  xmlGenericErrorFunc old_generic_error_func_;
  xmlStructuredErrorFunc old_structured_error_func_;
  void* old_error_context_;
};

}  // namespace blink

#endif  // XMLDocumentParserScope_h
