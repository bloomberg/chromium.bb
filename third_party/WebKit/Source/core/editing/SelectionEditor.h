/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
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

#ifndef SelectionEditor_h
#define SelectionEditor_h

#include "core/dom/SynchronousMutationObserver.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/events/EventDispatchResult.h"

namespace blink {

// TODO(yosin): We will rename |SelectionEditor| to appropriate name since
// it is no longer have a changing selection functionality, it was moved to
// |SelectionModifier| class.
class SelectionEditor final : public GarbageCollectedFinalized<SelectionEditor>,
                              public SynchronousMutationObserver {
  WTF_MAKE_NONCOPYABLE(SelectionEditor);
  USING_GARBAGE_COLLECTED_MIXIN(SelectionEditor);

 public:
  static SelectionEditor* create(LocalFrame& frame) {
    return new SelectionEditor(frame);
  }
  virtual ~SelectionEditor();
  void dispose();

  bool hasEditableStyle() const;
  bool isContentEditable() const;
  bool isContentRichlyEditable() const;

  const SelectionInDOMTree& selectionInDOMTree() const;

  const VisibleSelection& computeVisibleSelectionInDOMTree() const;
  const VisibleSelectionInFlatTree& computeVisibleSelectionInFlatTree() const;
  void setSelection(const SelectionInDOMTree&);

  void documentAttached(Document*);

  // If this FrameSelection has a logical range which is still valid, this
  // function return its clone. Otherwise, the return value from underlying
  // |VisibleSelection|'s |firstRange()| is returned.
  Range* firstRange() const;

  // There functions are exposed for |FrameSelection|.
  void resetLogicalRange();
  void setLogicalRange(Range*);

  void cacheRangeOfDocument(Range*);
  Range* documentCachedRange() const;
  void clearDocumentCachedRange();

  DECLARE_TRACE();

 private:
  explicit SelectionEditor(LocalFrame&);

  Document& document() const;
  LocalFrame* frame() const { return m_frame.get(); }

  void assertSelectionValid() const;
  void clearVisibleSelection();
  void markCacheDirty();
  bool shouldAlwaysUseDirectionalSelection() const;

  // VisibleSelection cache related
  bool needsUpdateVisibleSelection() const;
  void updateCachedVisibleSelectionIfNeeded() const;

  void didFinishTextChange(const Position& base, const Position& extent);
  void didFinishDOMMutation();

  // Implementation of |SynchronousMutationObsderver| member functions.
  void contextDestroyed(Document*) final;
  void didChangeChildren(const ContainerNode&) final;
  void didMergeTextNodes(const Text& mergedNode,
                         const NodeWithIndex& nodeToBeRemovedWithIndex,
                         unsigned oldLength) final;
  void didSplitTextNode(const Text&) final;
  void didUpdateCharacterData(CharacterData*,
                              unsigned offset,
                              unsigned oldLength,
                              unsigned newLength) final;
  void nodeChildrenWillBeRemoved(ContainerNode&) final;
  void nodeWillBeRemoved(Node&) final;

  Member<LocalFrame> m_frame;

  SelectionInDOMTree m_selection;

  // TODO(editing-dev): Removing |m_logicalRange|
  // The range specified by the user, which may not be visually canonicalized
  // (hence "logical"). This will be invalidated if the underlying
  // |VisibleSelection| changes. If that happens, this variable will
  // become |nullptr|, in which case logical positions == visible positions.
  Member<Range> m_logicalRange;

  // If document is root, document.getSelection().addRange(range) is cached on
  // this.
  Member<Range> m_cachedRange;

  mutable VisibleSelection m_cachedVisibleSelectionInDOMTree;
  mutable VisibleSelectionInFlatTree m_cachedVisibleSelectionInFlatTree;
  mutable uint64_t m_styleVersion = static_cast<uint64_t>(-1);
  mutable bool m_cacheIsDirty = false;
};

}  // namespace blink

#endif  // SelectionEditor_h
