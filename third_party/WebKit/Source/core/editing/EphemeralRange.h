// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EphemeralRange_h
#define EphemeralRange_h

#include "core/dom/Position.h"

namespace blink {

class Document;
class Range;

// Unlike |Range| objects, |EphemeralRange| objects aren't relocated. You
// should not use |EphemeralRange| objects after DOM modification.
//
// EphemeralRange is supposed to use returning or passing start and end
// position.
//
//  Example usage:
//    RefPtrWillBeRawPtr<Range> range = produceRange();
//    consumeRange(range.get());
//    ... no DOM modification ...
//    consumeRange2(range.get());
//
//  Above code should be:
//    EphemeralRange range = produceRange();
//    consumeRange(range);
//    ... no DOM modification ...
//    consumeRange2(range);
//
//  Because of |Range| objects consume heap memory and inserted into |Range|
//  object list in |Document| for relocation. These operations are redundant
//  if |Range| objects doesn't live after DOM mutation.
//
class CORE_EXPORT EphemeralRange final {
    STACK_ALLOCATED();
public:
    EphemeralRange(const Position& start, const Position& end);
    EphemeralRange(const EphemeralRange& other);
    // |position| should be null or in-document.
    explicit EphemeralRange(const Position& /* position */);
    // When |range| is nullptr, |EphemeralRange| is null.
    explicit EphemeralRange(const Range* /* range */);
    EphemeralRange();
    ~EphemeralRange();

    EphemeralRange& operator=(const EphemeralRange& other);

    Document& document() const;
    Position startPosition() const;
    Position endPosition() const;

    // Returns true if |m_startPositoin| == |m_endPosition| or |isNull()|.
    bool isCollapsed() const;
    bool isNull() const { return !isNotNull(); }
    bool isNotNull() const;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_startPosition);
        visitor->trace(m_endPosition);
    }

    // |node| should be in-document and valid for anchor node of
    // |PositionAlgorithm<Strategy>|.
    static EphemeralRange rangeOfContents(const Node& /* node */);

private:
    bool isValid() const;

    Position m_startPosition;
    Position m_endPosition;
#if ENABLE(ASSERT)
    uint64_t m_domTreeVersion;
#endif
};

} // namespace blink

#endif
