/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef FrameCaret_h
#define FrameCaret_h

#include <memory>
#include "core/CoreExport.h"
#include "core/editing/PositionWithAffinity.h"
#include "platform/Timer.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Member.h"

namespace blink {

class CaretDisplayItemClient;
class DisplayItemClient;
class Document;
class FrameCaret;
class GraphicsContext;
class LayoutBlock;
class LocalFrame;
class SelectionEditor;
struct PaintInvalidatorContext;

enum class CaretVisibility { Visible, Hidden };

class CORE_EXPORT FrameCaret final
    : public GarbageCollectedFinalized<FrameCaret> {
 public:
  FrameCaret(LocalFrame&, const SelectionEditor&);
  ~FrameCaret();

  const DisplayItemClient& displayItemClient() const;
  bool isActive() const { return caretPosition().isNotNull(); }

  void scheduleVisualUpdateForPaintInvalidationIfNeeded();

  // Used to suspend caret blinking while the mouse is down.
  void setCaretBlinkingSuspended(bool suspended) {
    m_isCaretBlinkingSuspended = suspended;
  }
  bool isCaretBlinkingSuspended() const { return m_isCaretBlinkingSuspended; }
  void stopCaretBlinkTimer();
  void startBlinkCaret();
  void setCaretVisibility(CaretVisibility);
  IntRect absoluteCaretBounds() const;

  bool shouldShowBlockCursor() const { return m_shouldShowBlockCursor; }
  void setShouldShowBlockCursor(bool);

  // Paint invalidation methods delegating to DisplayItemClient.
  void clearPreviousVisualRect(const LayoutBlock&);
  void layoutBlockWillBeDestroyed(const LayoutBlock&);
  void updateStyleAndLayoutIfNeeded();
  void invalidatePaintIfNeeded(const LayoutBlock&,
                               const PaintInvalidatorContext&);

  bool shouldPaintCaret(const LayoutBlock&) const;
  void paintCaret(GraphicsContext&, const LayoutPoint&) const;

  // For unit tests.
  const DisplayItemClient& caretDisplayItemClientForTesting() const;
  const LayoutBlock* caretLayoutBlockForTesting() const;
  bool shouldPaintCaretForTesting() const { return m_shouldPaintCaret; }
  void recreateCaretBlinkTimerForTesting(RefPtr<WebTaskRunner>);

  DECLARE_TRACE();

 private:
  friend class FrameSelectionTest;

  const PositionWithAffinity caretPosition() const;

  bool shouldBlinkCaret() const;
  void caretBlinkTimerFired(TimerBase*);
  bool caretPositionIsValidForDocument(const Document&) const;
  void updateAppearance();

  const Member<const SelectionEditor> m_selectionEditor;
  const Member<LocalFrame> m_frame;
  const std::unique_ptr<CaretDisplayItemClient> m_displayItemClient;
  CaretVisibility m_caretVisibility;
  // TODO(https://crbug.com/668758): Consider using BeginFrame update for this.
  std::unique_ptr<TaskRunnerTimer<FrameCaret>> m_caretBlinkTimer;
  bool m_shouldPaintCaret : 1;
  bool m_isCaretBlinkingSuspended : 1;
  bool m_shouldShowBlockCursor : 1;

  DISALLOW_COPY_AND_ASSIGN(FrameCaret);
};

}  // namespace blink

#endif  // FrameCaret_h
