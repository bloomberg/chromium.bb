// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XMLParserScriptRunner_h
#define XMLParserScriptRunner_h

#include "base/macros.h"
#include "core/script/PendingScript.h"
#include "platform/wtf/text/TextPosition.h"

namespace blink {

class Element;
class XMLParserScriptRunnerHost;

// XMLParserScriptRunner implements the interaction between an XML parser
// (XMLDocumentParser) and <script> elements and their loading/execution.
//
// https://html.spec.whatwg.org/#parsing-xhtml-documents
class XMLParserScriptRunner final
    : public GarbageCollectedFinalized<XMLParserScriptRunner>,
      public PendingScriptClient {
  USING_GARBAGE_COLLECTED_MIXIN(XMLParserScriptRunner);

 public:
  static XMLParserScriptRunner* Create(XMLParserScriptRunnerHost* host) {
    return new XMLParserScriptRunner(host);
  }
  ~XMLParserScriptRunner();

  bool HasParserBlockingScript() const { return parser_blocking_script_; }

  void ProcessScriptElement(Document&, Element*, TextPosition);
  void Detach();

  void Trace(Visitor*);

 private:
  explicit XMLParserScriptRunner(XMLParserScriptRunnerHost*);

  // from PendingScriptClient
  void PendingScriptFinished(PendingScript*) override;

  // https://html.spec.whatwg.org/#pending-parsing-blocking-script
  // TODO(crbug/717643): Support module scripts, and turn this into
  // TraceWrapperMember<>.
  Member<PendingScript> parser_blocking_script_;

  Member<XMLParserScriptRunnerHost> host_;

  // TODO(crbug/717643): Implement
  // https://html.spec.whatwg.org/#list-of-scripts-that-will-execute-when-the-document-has-finished-parsing
  DISALLOW_COPY_AND_ASSIGN(XMLParserScriptRunner);
};

}  // namespace blink

#endif
