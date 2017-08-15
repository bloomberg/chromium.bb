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

#include "core/loader/DocumentWriter.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

DocumentWriter* DocumentWriter::Create(
    Document* document,
    ParserSynchronizationPolicy parsing_policy,
    const AtomicString& mime_type,
    const AtomicString& encoding) {
  return new DocumentWriter(document, parsing_policy, mime_type, encoding);
}

DocumentWriter::DocumentWriter(Document* document,
                               ParserSynchronizationPolicy parser_sync_policy,
                               const AtomicString& mime_type,
                               const AtomicString& encoding)
    : document_(document),
      decoder_builder_(mime_type, encoding),
      // We grab a reference to the parser so that we'll always send data to the
      // original parser, even if the document acquires a new parser (e.g., via
      // document.open).
      parser_(document_->ImplicitOpen(parser_sync_policy)) {
  if (document_->GetFrame()) {
    if (LocalFrameView* view = document_->GetFrame()->View())
      view->SetContentsSize(IntSize());
  }
}

DocumentWriter::~DocumentWriter() {}

DEFINE_TRACE(DocumentWriter) {
  visitor->Trace(document_);
  visitor->Trace(parser_);
}

void DocumentWriter::AppendReplacingData(const String& source) {
  document_->SetCompatibilityMode(Document::kNoQuirksMode);

  // FIXME: This should call DocumentParser::appendBytes instead of append
  // to support RawDataDocumentParsers.
  if (DocumentParser* parser = document_->Parser())
    parser->Append(source);
}

void DocumentWriter::AddData(const char* bytes, size_t length) {
  DCHECK(parser_);
  if (parser_->NeedsDecoder() && 0 < length) {
    std::unique_ptr<TextResourceDecoder> decoder =
        decoder_builder_.BuildFor(document_);
    parser_->SetDecoder(std::move(decoder));
  }
  // appendBytes() can result replacing DocumentLoader::m_writer.
  parser_->AppendBytes(bytes, length);
}

void DocumentWriter::end() {
  DCHECK(document_);

  if (!parser_)
    return;

  if (parser_->NeedsDecoder()) {
    std::unique_ptr<TextResourceDecoder> decoder =
        decoder_builder_.BuildFor(document_);
    parser_->SetDecoder(std::move(decoder));
  }

  parser_->Finish();
  parser_ = nullptr;
  document_ = nullptr;
}

void DocumentWriter::SetDocumentWasLoadedAsPartOfNavigation() {
  DCHECK(parser_);
  DCHECK(!parser_->IsStopped());
  parser_->SetDocumentWasLoadedAsPartOfNavigation();
}

}  // namespace blink
