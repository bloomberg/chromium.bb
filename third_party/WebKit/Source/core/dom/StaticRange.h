// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StaticRange_h
#define StaticRange_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/Range.h"
#include "core/editing/EphemeralRange.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class ExceptionState;

class CORE_EXPORT StaticRange final : public GarbageCollected<StaticRange>,
                                      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static StaticRange* create(Document& document) {
    return new StaticRange(document);
  }
  static StaticRange* create(Document& document,
                             Node* startContainer,
                             unsigned startOffset,
                             Node* endContainer,
                             unsigned endOffset) {
    return new StaticRange(document, startContainer, startOffset, endContainer,
                           endOffset);
  }
  static StaticRange* create(const Range* range) {
    return new StaticRange(range->ownerDocument(), range->startContainer(),
                           range->startOffset(), range->endContainer(),
                           range->endOffset());
  }
  static StaticRange* create(const EphemeralRange& range) {
    DCHECK(!range.isNull());
    return new StaticRange(range.document(),
                           range.startPosition().computeContainerNode(),
                           range.startPosition().computeOffsetInContainerNode(),
                           range.endPosition().computeContainerNode(),
                           range.endPosition().computeOffsetInContainerNode());
  }

  Node* startContainer() const { return m_startContainer.get(); }
  void setStartContainer(Node* startContainer) {
    m_startContainer = startContainer;
  }

  unsigned startOffset() const { return m_startOffset; }
  void setStartOffset(unsigned startOffset) { m_startOffset = startOffset; }

  Node* endContainer() const { return m_endContainer.get(); }
  void setEndContainer(Node* endContainer) { m_endContainer = endContainer; }

  unsigned endOffset() const { return m_endOffset; }
  void setEndOffset(unsigned endOffset) { m_endOffset = endOffset; }

  bool collapsed() const {
    return m_startContainer == m_endContainer && m_startOffset == m_endOffset;
  }

  void setStart(Node* container, unsigned offset);
  void setEnd(Node* container, unsigned offset);

  Range* toRange(ExceptionState& = ASSERT_NO_EXCEPTION) const;

  DECLARE_TRACE();

 private:
  explicit StaticRange(Document&);
  StaticRange(Document&,
              Node* startContainer,
              unsigned startOffset,
              Node* endContainer,
              unsigned endOffset);

  Member<Document> m_ownerDocument;  // Required by |toRange()|.
  Member<Node> m_startContainer;
  unsigned m_startOffset;
  Member<Node> m_endContainer;
  unsigned m_endOffset;
};

using StaticRangeVector = HeapVector<Member<StaticRange>>;

}  // namespace blink

#endif  // StaticRange_h
