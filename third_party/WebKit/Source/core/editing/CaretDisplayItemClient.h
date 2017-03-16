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

#ifndef CaretDisplayItemClient_h
#define CaretDisplayItemClient_h

#include "core/editing/PositionWithAffinity.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Noncopyable.h"

namespace blink {

class GraphicsContext;
class LayoutBlock;
struct PaintInvalidatorContext;

class CaretDisplayItemClient final : public DisplayItemClient {
  WTF_MAKE_NONCOPYABLE(CaretDisplayItemClient);

 public:
  CaretDisplayItemClient();
  virtual ~CaretDisplayItemClient();

  // TODO(yosin,wangxianzhu): Make these two static functions private or
  // combine them into updateForPaintInvalidation() when the callsites in
  // FrameCaret are removed.

  // Creating VisiblePosition causes synchronous layout so we should use the
  // PositionWithAffinity version if possible.
  // A position in HTMLTextFromControlElement is a typical example.
  static LayoutRect computeCaretRect(const PositionWithAffinity& caretPosition);
  static LayoutBlock* caretLayoutBlock(const Node*);

  // Called indirectly from LayoutBlock::clearPreviousVisualRects().
  void clearPreviousVisualRect(const LayoutBlock&);

  // Called indirectly from LayoutBlock::willBeDestroyed().
  void layoutBlockWillBeDestroyed(const LayoutBlock&);

  // Called when a FrameView finishes layout. Updates style and geometry of the
  // caret for paint invalidation and painting.
  void updateStyleAndLayoutIfNeeded(const PositionWithAffinity& caretPosition);

  // Called during LayoutBlock paint invalidation.
  void invalidatePaintIfNeeded(const LayoutBlock&,
                               const PaintInvalidatorContext&);

  bool shouldPaintCaret(const LayoutBlock& block) const {
    return &block == m_layoutBlock;
  }
  void paintCaret(GraphicsContext&,
                  const LayoutPoint& paintOffset,
                  DisplayItem::Type) const;

  // DisplayItemClient methods.
  LayoutRect visualRect() const final;
  String debugName() const final;

 private:
  friend class CaretDisplayItemClientTest;

  void invalidatePaintInCurrentLayoutBlock(const PaintInvalidatorContext&);
  void invalidatePaintInPreviousLayoutBlock(const PaintInvalidatorContext&);

  // These are updated by updateStyleAndLayoutIfNeeded().
  Color m_color;
  LayoutRect m_localRect;
  LayoutBlock* m_layoutBlock = nullptr;

  // Visual rect of the caret in m_layoutBlock. This is updated by
  // invalidatePaintIfNeeded().
  LayoutRect m_visualRect;

  // These are set to the previous value of m_layoutBlock and m_visualRect
  // during updateStyleAndLayoutIfNeeded() if they haven't been set since the
  // last paint invalidation. They can only be used in invalidatePaintIfNeeded()
  // to invalidate the caret in the previous layout block.
  const LayoutBlock* m_previousLayoutBlock = nullptr;
  LayoutRect m_visualRectInPreviousLayoutBlock;

  bool m_needsPaintInvalidation = false;
};

}  // namespace blink

#endif  // CaretDisplayItemClient_h
