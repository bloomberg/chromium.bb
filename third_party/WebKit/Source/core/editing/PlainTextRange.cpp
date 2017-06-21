/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
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

#include "core/editing/PlainTextRange.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/TextIterator.h"

namespace blink {

PlainTextRange::PlainTextRange() : start_(kNotFound), end_(kNotFound) {}

PlainTextRange::PlainTextRange(int location)
    : start_(location), end_(location) {
  DCHECK_GE(location, 0);
}

PlainTextRange::PlainTextRange(int start, int end) : start_(start), end_(end) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end, 0);
  DCHECK_LE(start, end);
}

EphemeralRange PlainTextRange::CreateRange(const ContainerNode& scope) const {
  return CreateRangeFor(scope, kForGeneric);
}

EphemeralRange PlainTextRange::CreateRangeForSelection(
    const ContainerNode& scope) const {
  return CreateRangeFor(scope, kForSelection);
}

EphemeralRange PlainTextRange::CreateRangeFor(const ContainerNode& scope,
                                              GetRangeFor get_range_for) const {
  DCHECK(IsNotNull());

  size_t doc_text_position = 0;
  bool start_range_found = false;

  Position text_run_start_position;
  Position text_run_end_position;

  const TextIteratorBehavior& behavior =
      TextIteratorBehavior::Builder()
          .SetEmitsObjectReplacementCharacter(true)
          .SetEmitsCharactersBetweenAllVisiblePositions(get_range_for ==
                                                        kForSelection)
          .Build();

  auto range = EphemeralRange::RangeOfContents(scope);

  TextIterator it(range.StartPosition(), range.EndPosition(), behavior);

  // FIXME: the atEnd() check shouldn't be necessary, workaround for
  // <http://bugs.webkit.org/show_bug.cgi?id=6289>.
  if (!Start() && !length() && it.AtEnd())
    return EphemeralRange(Position(it.CurrentContainer(), 0));

  Position result_start = Position(&scope.GetDocument(), 0);
  Position result_end = result_start;

  for (; !it.AtEnd(); it.Advance()) {
    int len = it.length();

    text_run_start_position = it.StartPositionInCurrentContainer();
    text_run_end_position = it.EndPositionInCurrentContainer();

    bool found_start =
        Start() >= doc_text_position && Start() <= doc_text_position + len;
    bool found_end =
        End() >= doc_text_position && End() <= doc_text_position + len;

    // Fix textRunRange->endPosition(), but only if foundStart || foundEnd,
    // because it is only in those cases that textRunRange is used.
    if (found_end) {
      // FIXME: This is a workaround for the fact that the end of a run
      // is often at the wrong position for emitted '\n's or if the
      // layoutObject of the current node is a replaced element.
      if (len == 1 &&
          (it.CharacterAt(0) == '\n' || it.IsInsideAtomicInlineElement())) {
        it.Advance();
        if (!it.AtEnd()) {
          text_run_end_position = it.StartPositionInCurrentContainer();
        } else {
          Position run_end =
              NextPositionOf(CreateVisiblePosition(text_run_start_position))
                  .DeepEquivalent();
          if (run_end.IsNotNull())
            text_run_end_position = run_end;
        }
      }
    }

    if (found_start) {
      start_range_found = true;
      if (text_run_start_position.ComputeContainerNode()->IsTextNode()) {
        int offset = Start() - doc_text_position;
        result_start =
            Position(text_run_start_position.ComputeContainerNode(),
                     offset + text_run_start_position.OffsetInContainerNode());
      } else {
        if (Start() == doc_text_position)
          result_start = text_run_start_position;
        else
          result_start = text_run_end_position;
      }
    }

    if (found_end) {
      if (text_run_start_position.ComputeContainerNode()->IsTextNode()) {
        int offset = End() - doc_text_position;
        result_end =
            Position(text_run_start_position.ComputeContainerNode(),
                     offset + text_run_start_position.OffsetInContainerNode());
      } else {
        if (End() == doc_text_position)
          result_end = text_run_start_position;
        else
          result_end = text_run_end_position;
      }
      doc_text_position += len;
      break;
    }
    doc_text_position += len;
  }

  if (!start_range_found)
    return EphemeralRange();

  if (length() && End() > doc_text_position) {  // End() is out of bounds
    result_end = text_run_end_position;
  }

  return EphemeralRange(result_start.ToOffsetInAnchor(),
                        result_end.ToOffsetInAnchor());
}

PlainTextRange PlainTextRange::Create(const ContainerNode& scope,
                                      const EphemeralRange& range) {
  if (range.IsNull())
    return PlainTextRange();

  // The critical assumption is that this only gets called with ranges that
  // concentrate on a given area containing the selection root. This is done
  // because of text fields and textareas. The DOM for those is not
  // directly in the document DOM, so ensure that the range does not cross a
  // boundary of one of those.
  Node* start_container = range.StartPosition().ComputeContainerNode();
  if (start_container != &scope && !start_container->IsDescendantOf(&scope))
    return PlainTextRange();
  Node* end_container = range.EndPosition().ComputeContainerNode();
  if (end_container != scope && !end_container->IsDescendantOf(&scope))
    return PlainTextRange();

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      scope.GetDocument().Lifecycle());

  size_t start =
      TextIterator::RangeLength(Position(scope, 0), range.StartPosition());
  size_t end =
      TextIterator::RangeLength(Position(scope, 0), range.EndPosition());

  return PlainTextRange(start, end);
}

PlainTextRange PlainTextRange::Create(const ContainerNode& scope,
                                      const Range& range) {
  return Create(scope, EphemeralRange(&range));
}

}  // namespace blink
