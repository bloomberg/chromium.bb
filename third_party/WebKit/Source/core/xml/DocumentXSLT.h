// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentXSLT_h
#define DocumentXSLT_h

#include "core/dom/Document.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class Document;
class ProcessingInstruction;

class DocumentXSLT final : public GarbageCollected<DocumentXSLT>,
                           public Supplement<Document> {
  WTF_MAKE_NONCOPYABLE(DocumentXSLT);
  USING_GARBAGE_COLLECTED_MIXIN(DocumentXSLT);

 public:
  Document* TransformSourceDocument() {
    return transform_source_document_.Get();
  }

  void SetTransformSourceDocument(Document* document) {
    DCHECK(document);
    transform_source_document_ = document;
  }

  static DocumentXSLT& From(Document&);
  static const char* SupplementName();

  // The following static methods don't use any instance of DocumentXSLT.
  // They are just using DocumentXSLT namespace.
  static void ApplyXSLTransform(Document&, ProcessingInstruction*);
  static ProcessingInstruction* FindXSLStyleSheet(Document&);
  static bool ProcessingInstructionInsertedIntoDocument(Document&,
                                                        ProcessingInstruction*);
  static bool ProcessingInstructionRemovedFromDocument(Document&,
                                                       ProcessingInstruction*);
  static bool SheetLoaded(Document&, ProcessingInstruction*);
  static bool HasTransformSourceDocument(Document&);

  virtual void Trace(blink::Visitor*);

 private:
  explicit DocumentXSLT(Document&);

  Member<Document> transform_source_document_;
};

}  // namespace blink

#endif
