/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/speech/SpeechGrammarList.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"

namespace blink {

SpeechGrammarList* SpeechGrammarList::create() {
  return new SpeechGrammarList;
}

SpeechGrammar* SpeechGrammarList::item(unsigned index) const {
  if (index >= m_grammars.size())
    return nullptr;

  return m_grammars[index];
}

void SpeechGrammarList::addFromUri(ScriptState* scriptState,
                                   const String& src,
                                   double weight) {
  Document* document = toDocument(scriptState->getExecutionContext());
  m_grammars.push_back(
      SpeechGrammar::create(document->completeURL(src), weight));
}

void SpeechGrammarList::addFromString(const String& string, double weight) {
  String urlString =
      String("data:application/xml,") + encodeWithURLEscapeSequences(string);
  m_grammars.push_back(SpeechGrammar::create(KURL(KURL(), urlString), weight));
}

SpeechGrammarList::SpeechGrammarList() {}

DEFINE_TRACE(SpeechGrammarList) {
  visitor->trace(m_grammars);
}

}  // namespace blink
