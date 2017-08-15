/*
 * Copyright (C) 2010. Adam Barth. All rights reserved.
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
 * 3.  Neither the name of Adam Barth. ("Adam Barth") nor the names of
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

#ifndef DocumentWriter_h
#define DocumentWriter_h

#include "core/html/parser/ParserSynchronizationPolicy.h"
#include "core/loader/TextResourceDecoderBuilder.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class Document;
class DocumentParser;

class DocumentWriter final : public GarbageCollectedFinalized<DocumentWriter> {
  WTF_MAKE_NONCOPYABLE(DocumentWriter);

 public:
  static DocumentWriter* Create(Document*,
                                ParserSynchronizationPolicy,
                                const AtomicString& mime_type,
                                const AtomicString& encoding);

  ~DocumentWriter();
  DECLARE_TRACE();

  void end();

  void AddData(const char* bytes, size_t length);

  const AtomicString& MimeType() const { return decoder_builder_.MimeType(); }
  const AtomicString& Encoding() const { return decoder_builder_.Encoding(); }

  // Exposed for DocumentLoader::replaceDocumentWhileExecutingJavaScriptURL.
  void AppendReplacingData(const String&);

  void SetDocumentWasLoadedAsPartOfNavigation();

 private:
  DocumentWriter(Document*,
                 ParserSynchronizationPolicy,
                 const AtomicString& mime_type,
                 const AtomicString& encoding);

  Member<Document> document_;
  TextResourceDecoderBuilder decoder_builder_;

  Member<DocumentParser> parser_;
};

}  // namespace blink

#endif  // DocumentWriter_h
