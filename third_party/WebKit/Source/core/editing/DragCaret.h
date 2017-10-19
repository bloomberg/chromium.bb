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

#ifndef DragCaret_h
#define DragCaret_h

#include <memory>
#include "core/dom/SynchronousMutationObserver.h"
#include "core/editing/CaretDisplayItemClient.h"
#include "core/editing/PositionWithAffinity.h"
#include "platform/graphics/PaintInvalidationReason.h"

namespace blink {

class LayoutBlock;
struct PaintInvalidatorContext;

class DragCaret final : public GarbageCollectedFinalized<DragCaret>,
                        public SynchronousMutationObserver {
  WTF_MAKE_NONCOPYABLE(DragCaret);
  USING_GARBAGE_COLLECTED_MIXIN(DragCaret);

 public:
  static DragCaret* Create();

  virtual ~DragCaret();

  // Paint invalidation methods delegating to CaretDisplayItemClient.
  void ClearPreviousVisualRect(const LayoutBlock&);
  void LayoutBlockWillBeDestroyed(const LayoutBlock&);
  void UpdateStyleAndLayoutIfNeeded();
  void InvalidatePaint(const LayoutBlock&, const PaintInvalidatorContext&);

  bool ShouldPaintCaret(const LayoutBlock&) const;
  void PaintDragCaret(const LocalFrame*,
                      GraphicsContext&,
                      const LayoutPoint&) const;

  bool IsContentRichlyEditable() const;

  bool HasCaret() const { return position_.IsNotNull(); }
  const PositionWithAffinity& CaretPosition() { return position_; }
  void SetCaretPosition(const PositionWithAffinity&);
  void Clear() { SetCaretPosition(PositionWithAffinity()); }

  void Trace(blink::Visitor*);

 private:
  DragCaret();

  // Implementations of |SynchronousMutationObserver|
  void NodeChildrenWillBeRemoved(ContainerNode&) final;
  void NodeWillBeRemoved(Node&) final;

  PositionWithAffinity position_;
  const std::unique_ptr<CaretDisplayItemClient> display_item_client_;
};

}  // namespace blink

#endif  // DragCaret_h
