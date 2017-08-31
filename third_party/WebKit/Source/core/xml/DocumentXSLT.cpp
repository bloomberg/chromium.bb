// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/xml/DocumentXSLT.h"

#include "bindings/core/v8/V8AbstractEventListener.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/Node.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/events/Event.h"
#include "core/dom/events/EventListener.h"
#include "core/frame/UseCounter.h"
#include "core/probe/CoreProbes.h"
#include "core/xml/XSLStyleSheet.h"
#include "core/xml/XSLTProcessor.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

class DOMContentLoadedListener final
    : public V8AbstractEventListener,
      public ProcessingInstruction::DetachableEventListener {
  USING_GARBAGE_COLLECTED_MIXIN(DOMContentLoadedListener);

 public:
  static DOMContentLoadedListener* Create(ScriptState* script_state,
                                          ProcessingInstruction* pi) {
    return new DOMContentLoadedListener(script_state, pi);
  }

  bool operator==(const EventListener&) const override { return true; }

  virtual void HandleEvent(ScriptState* script_state, Event* event) {
    DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
    DCHECK_EQ(event->type(), "DOMContentLoaded");
    ScriptState::Scope scope(script_state);

    Document& document =
        *ToDocument(ToExecutionContext(script_state->GetContext()));
    DCHECK(!document.Parsing());

    // Processing instruction (XML documents only).
    // We don't support linking to embedded CSS stylesheets,
    // see <https://bugs.webkit.org/show_bug.cgi?id=49281> for discussion.
    // Don't apply XSL transforms to already transformed documents.
    if (DocumentXSLT::HasTransformSourceDocument(document))
      return;

    ProcessingInstruction* pi = DocumentXSLT::FindXSLStyleSheet(document);
    if (!pi || pi != processing_instruction_ || pi->IsLoading())
      return;
    DocumentXSLT::ApplyXSLTransform(document, pi);
  }

  void Detach() override { processing_instruction_ = nullptr; }

  EventListener* ToEventListener() override { return this; }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(processing_instruction_);
    V8AbstractEventListener::Trace(visitor);
    ProcessingInstruction::DetachableEventListener::Trace(visitor);
  }

 private:
  DOMContentLoadedListener(ScriptState* script_state, ProcessingInstruction* pi)
      : V8AbstractEventListener(false,
                                script_state->World(),
                                script_state->GetIsolate()),
        processing_instruction_(pi) {}

  virtual v8::Local<v8::Value> CallListenerFunction(ScriptState*,
                                                    v8::Local<v8::Value>,
                                                    Event*) {
    NOTREACHED();
    return v8::Local<v8::Value>();
  }

  // If this event listener is attached to a ProcessingInstruction, keep a
  // weak reference back to it. That ProcessingInstruction is responsible for
  // detaching itself and clear out the reference.
  Member<ProcessingInstruction> processing_instruction_;
};

DocumentXSLT::DocumentXSLT(Document& document)
    : Supplement<Document>(document), transform_source_document_(nullptr) {}

void DocumentXSLT::ApplyXSLTransform(Document& document,
                                     ProcessingInstruction* pi) {
  DCHECK(!pi->IsLoading());
  UseCounter::Count(document, WebFeature::kXSLProcessingInstruction);
  XSLTProcessor* processor = XSLTProcessor::Create(document);
  processor->SetXSLStyleSheet(ToXSLStyleSheet(pi->sheet()));
  String result_mime_type;
  String new_source;
  String result_encoding;
  document.SetParsingState(Document::kParsing);
  if (!processor->TransformToString(&document, result_mime_type, new_source,
                                    result_encoding)) {
    document.SetParsingState(Document::kFinishedParsing);
    return;
  }
  // FIXME: If the transform failed we should probably report an error (like
  // Mozilla does).
  LocalFrame* owner_frame = document.GetFrame();
  processor->CreateDocumentFromSource(new_source, result_encoding,
                                      result_mime_type, &document, owner_frame);
  probe::frameDocumentUpdated(owner_frame);
  document.SetParsingState(Document::kFinishedParsing);
}

ProcessingInstruction* DocumentXSLT::FindXSLStyleSheet(Document& document) {
  for (Node* node = document.firstChild(); node; node = node->nextSibling()) {
    if (node->getNodeType() != Node::kProcessingInstructionNode)
      continue;

    ProcessingInstruction* pi = ToProcessingInstruction(node);
    if (pi->IsXSL())
      return pi;
  }
  return nullptr;
}

bool DocumentXSLT::ProcessingInstructionInsertedIntoDocument(
    Document& document,
    ProcessingInstruction* pi) {
  if (!pi->IsXSL())
    return false;

  if (!RuntimeEnabledFeatures::XSLTEnabled() || !document.GetFrame())
    return true;

  ScriptState* script_state = ToScriptStateForMainWorld(document.GetFrame());
  if (!script_state)
    return false;
  DOMContentLoadedListener* listener =
      DOMContentLoadedListener::Create(script_state, pi);
  document.addEventListener(EventTypeNames::DOMContentLoaded, listener, false);
  DCHECK(!pi->EventListenerForXSLT());
  pi->SetEventListenerForXSLT(listener);
  return true;
}

bool DocumentXSLT::ProcessingInstructionRemovedFromDocument(
    Document& document,
    ProcessingInstruction* pi) {
  if (!pi->IsXSL())
    return false;

  if (!pi->EventListenerForXSLT())
    return true;

  DCHECK(RuntimeEnabledFeatures::XSLTEnabled());
  document.removeEventListener(EventTypeNames::DOMContentLoaded,
                               pi->EventListenerForXSLT(), false);
  pi->ClearEventListenerForXSLT();
  return true;
}

bool DocumentXSLT::SheetLoaded(Document& document, ProcessingInstruction* pi) {
  if (!pi->IsXSL())
    return false;

  if (RuntimeEnabledFeatures::XSLTEnabled() && !document.Parsing() &&
      !pi->IsLoading() && !DocumentXSLT::HasTransformSourceDocument(document)) {
    if (FindXSLStyleSheet(document) == pi)
      ApplyXSLTransform(document, pi);
  }
  return true;
}

const char* DocumentXSLT::SupplementName() {
  return "DocumentXSLT";
}

bool DocumentXSLT::HasTransformSourceDocument(Document& document) {
  return static_cast<DocumentXSLT*>(
      Supplement<Document>::From(document, SupplementName()));
}

DocumentXSLT& DocumentXSLT::From(Document& document) {
  DocumentXSLT* supplement = static_cast<DocumentXSLT*>(
      Supplement<Document>::From(document, SupplementName()));
  if (!supplement) {
    supplement = new DocumentXSLT(document);
    Supplement<Document>::ProvideTo(document, SupplementName(), supplement);
  }
  return *supplement;
}

DEFINE_TRACE(DocumentXSLT) {
  visitor->Trace(transform_source_document_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
