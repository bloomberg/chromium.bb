// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XMLParserScriptRunner_h
#define XMLParserScriptRunner_h

#include "core/dom/PendingScript.h"
#include "platform/wtf/text/TextPosition.h"

namespace blink {

class Element;
class ScriptElementBase;
class XMLParserScriptRunnerHost;

// XMLParserScriptRunner implements the interaction between an XML parser
// (XMLDocumentParser) and <script> elements and their loading/execution.
class XMLParserScriptRunner final
    : public GarbageCollectedFinalized<XMLParserScriptRunner>,
      public PendingScriptClient {
  WTF_MAKE_NONCOPYABLE(XMLParserScriptRunner);
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

  Member<ScriptElementBase> script_element_;

  // https://html.spec.whatwg.org/#pending-parsing-blocking-script
  // TODO(crbug/717643): Support module scripts, and turn this into
  // TraceWrapperMember<>.
  Member<PendingScript> parser_blocking_script_;

  Member<XMLParserScriptRunnerHost> host_;

  // TODO(crbug/717643): Implement
  // https://html.spec.whatwg.org/#list-of-scripts-that-will-execute-when-the-document-has-finished-parsing
};

}  // namespace blink

#endif
