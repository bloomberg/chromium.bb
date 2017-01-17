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

#ifndef CaretBase_h
#define CaretBase_h

#include "core/editing/VisiblePosition.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Noncopyable.h"

namespace blink {

class GraphicsContext;
class LayoutBlock;

class CaretBase final : public DisplayItemClient {
  WTF_MAKE_NONCOPYABLE(CaretBase);

 public:
  CaretBase();
  virtual ~CaretBase();

  void invalidateCaretRect(Node*, const LayoutRect&);
  // Creating VisiblePosition causes synchronous layout so we should use the
  // PositionWithAffinity version if possible.
  // A position in HTMLTextFromControlElement is a typical example.
  static LayoutRect computeCaretRect(const PositionWithAffinity& caretPosition);

  // TODO(yosin): We should move |computeCaretRect()| with |VisiblePosition| to
  // "FrameCaret.cpp" as static file local function.
  static LayoutRect computeCaretRect(const VisiblePosition& caretPosition);

  // TODO(yosin): We should move |absoluteBoundsForLocalRect()| with
  // |VisiblePosition| to "FrameCaret.cpp" as static file local function.
  static IntRect absoluteBoundsForLocalRect(Node*, const LayoutRect&);

  // TODO(yosin): We should move |shouldRepaintCaret()| to "FrameCaret.cpp" as
  // static file local function.
  static bool shouldRepaintCaret(Node&);

  void paintCaret(Node*,
                  GraphicsContext&,
                  const LayoutRect& caretLocalRect,
                  const LayoutPoint&,
                  DisplayItem::Type);

  static LayoutBlock* caretLayoutObject(Node*);
  void invalidateLocalCaretRect(Node*, const LayoutRect&);

  // DisplayItemClient methods.
  LayoutRect visualRect() const final;
  String debugName() const final;

 private:
  LayoutRect m_visualRect;
};

}  // namespace blink

#endif  // CaretBase_h
