/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
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

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/VisibleUnits.h"

#include "core/editing/EditingUtilities.h"
#include "core/editing/VisiblePosition.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/text/TextBoundaries.h"

namespace blink {

namespace {

unsigned EndWordBoundary(
    const UChar* characters,
    unsigned length,
    unsigned offset,
    BoundarySearchContextAvailability may_have_more_context,
    bool& need_more_context) {
  DCHECK_LE(offset, length);
  if (may_have_more_context &&
      EndOfFirstWordBoundaryContext(characters + offset, length - offset) ==
          static_cast<int>(length - offset)) {
    need_more_context = true;
    return length;
  }
  need_more_context = false;
  return FindWordEndBoundary(characters, length, offset);
}

template <typename Strategy>
PositionTemplate<Strategy> EndOfWordAlgorithm(
    const VisiblePositionTemplate<Strategy>& c,
    EWordSide side) {
  DCHECK(c.IsValid()) << c;
  VisiblePositionTemplate<Strategy> p = c;
  if (side == kLeftWordIfOnBoundary) {
    if (IsStartOfParagraph(c))
      return c.DeepEquivalent();

    p = PreviousPositionOf(c);
    if (p.IsNull())
      return c.DeepEquivalent();
  } else if (IsEndOfParagraph(c)) {
    return c.DeepEquivalent();
  }

  return NextBoundary(p, EndWordBoundary);
}

unsigned NextWordPositionBoundary(
    const UChar* characters,
    unsigned length,
    unsigned offset,
    BoundarySearchContextAvailability may_have_more_context,
    bool& need_more_context) {
  if (may_have_more_context &&
      EndOfFirstWordBoundaryContext(characters + offset, length - offset) ==
          static_cast<int>(length - offset)) {
    need_more_context = true;
    return length;
  }
  need_more_context = false;
  return FindNextWordFromIndex(characters, length, offset, true);
}

unsigned PreviousWordPositionBoundary(
    const UChar* characters,
    unsigned length,
    unsigned offset,
    BoundarySearchContextAvailability may_have_more_context,
    bool& need_more_context) {
  if (may_have_more_context &&
      !StartOfLastWordBoundaryContext(characters, offset)) {
    need_more_context = true;
    return 0;
  }
  need_more_context = false;
  return FindNextWordFromIndex(characters, length, offset, false);
}

unsigned StartWordBoundary(
    const UChar* characters,
    unsigned length,
    unsigned offset,
    BoundarySearchContextAvailability may_have_more_context,
    bool& need_more_context) {
  TRACE_EVENT0("blink", "startWordBoundary");
  DCHECK(offset);
  if (may_have_more_context &&
      !StartOfLastWordBoundaryContext(characters, offset)) {
    need_more_context = true;
    return 0;
  }
  need_more_context = false;
  int start, end;
  U16_BACK_1(characters, 0, offset);
  FindWordBoundary(characters, length, offset, &start, &end);
  return start;
}

template <typename Strategy>
PositionTemplate<Strategy> StartOfWordAlgorithm(
    const VisiblePositionTemplate<Strategy>& c,
    EWordSide side) {
  DCHECK(c.IsValid()) << c;
  // TODO(yosin) This returns a null VP for c at the start of the document
  // and |side| == |LeftWordIfOnBoundary|
  VisiblePositionTemplate<Strategy> p = c;
  if (side == kRightWordIfOnBoundary) {
    // at paragraph end, the startofWord is the current position
    if (IsEndOfParagraph(c))
      return c.DeepEquivalent();

    p = NextPositionOf(c);
    if (p.IsNull())
      return c.DeepEquivalent();
  }
  return PreviousBoundary(p, StartWordBoundary);
}

}  // namespace

Position EndOfWordPosition(const VisiblePosition& position, EWordSide side) {
  return EndOfWordAlgorithm<EditingStrategy>(position, side);
}

VisiblePosition EndOfWord(const VisiblePosition& position, EWordSide side) {
  return CreateVisiblePosition(EndOfWordPosition(position, side),
                               TextAffinity::kUpstreamIfPossible);
}

PositionInFlatTree EndOfWordPosition(const VisiblePositionInFlatTree& position,
                                     EWordSide side) {
  return EndOfWordAlgorithm<EditingInFlatTreeStrategy>(position, side);
}

VisiblePositionInFlatTree EndOfWord(const VisiblePositionInFlatTree& position,
                                    EWordSide side) {
  return CreateVisiblePosition(EndOfWordPosition(position, side),
                               TextAffinity::kUpstreamIfPossible);
}

VisiblePosition NextWordPosition(const VisiblePosition& c) {
  DCHECK(c.IsValid()) << c;
  VisiblePosition next =
      CreateVisiblePosition(NextBoundary(c, NextWordPositionBoundary),
                            TextAffinity::kUpstreamIfPossible);
  return HonorEditingBoundaryAtOrAfter(next, c.DeepEquivalent());
}

VisiblePosition PreviousWordPosition(const VisiblePosition& c) {
  DCHECK(c.IsValid()) << c;
  VisiblePosition prev =
      CreateVisiblePosition(PreviousBoundary(c, PreviousWordPositionBoundary));
  return HonorEditingBoundaryAtOrBefore(prev, c.DeepEquivalent());
}

Position StartOfWordPosition(const VisiblePosition& position, EWordSide side) {
  return StartOfWordAlgorithm<EditingStrategy>(position, side);
}

VisiblePosition StartOfWord(const VisiblePosition& position, EWordSide side) {
  return CreateVisiblePosition(StartOfWordPosition(position, side));
}

PositionInFlatTree StartOfWordPosition(
    const VisiblePositionInFlatTree& position,
    EWordSide side) {
  return StartOfWordAlgorithm<EditingInFlatTreeStrategy>(position, side);
}

VisiblePositionInFlatTree StartOfWord(const VisiblePositionInFlatTree& position,
                                      EWordSide side) {
  return CreateVisiblePosition(StartOfWordPosition(position, side));
}

}  // namespace blink
