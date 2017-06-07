/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/SinkDocument.h"

#include "core/dom/RawDataDocumentParser.h"
#include "core/frame/UseCounter.h"

namespace blink {

class SinkDocumentParser : public RawDataDocumentParser {
 public:
  static SinkDocumentParser* Create(SinkDocument* document) {
    return new SinkDocumentParser(document);
  }

 private:
  explicit SinkDocumentParser(SinkDocument* document)
      : RawDataDocumentParser(document) {}

  // Ignore all data.
  void AppendBytes(const char*, size_t) override {}
};

SinkDocument::SinkDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer) {
  SetCompatibilityMode(kQuirksMode);
  LockCompatibilityMode();
  UseCounter::Count(*this, WebFeature::kSinkDocument);
  if (!IsInMainFrame())
    UseCounter::Count(*this, WebFeature::kSinkDocumentInFrame);
}

DocumentParser* SinkDocument::CreateParser() {
  return SinkDocumentParser::Create(this);
}

}  // namespace blink
