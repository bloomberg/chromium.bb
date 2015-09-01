/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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

#ifndef VisiblePosition_h
#define VisiblePosition_h

#include "core/CoreExport.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/editing/TextAffinity.h"
#include "platform/heap/Handle.h"

namespace blink {

// VisiblePosition default affinity is downstream because
// the callers do not really care (they just want the
// deep position without regard to line position), and this
// is cheaper than UPSTREAM
#define VP_DEFAULT_AFFINITY TextAffinity::Downstream

// Callers who do not know where on the line the position is,
// but would like UPSTREAM if at a line break or DOWNSTREAM
// otherwise, need a clear way to specify that.  The
// constructors auto-correct UPSTREAM to DOWNSTREAM if the
// position is not at a line break.
#define VP_UPSTREAM_IF_POSSIBLE TextAffinity::Upstream

class InlineBox;
class Range;

// |VisiblePosition| is an immutable object representing "canonical position"
// with affinity.
//
// "canonical position" is roughly equivalent to a position where we can place
// caret, see |canonicalPosition()| for actual definition.
//
// "affinity" represents a place of caret at wrapped line. UPSTREAM affinity
// means caret is placed at end of line. DOWNSTREAM affinity means caret is
// placed at start of line.
//
// Example of affinity:
//    abc^def where "^" represent |Position|
// When above text line wrapped after "abc"
//    abc|  UPSTREAM |VisiblePosition|
//    |def  DOWNSTREAM |VisiblePosition|
//
// NOTE: UPSTREAM affinity will be used only if pos is at end of a wrapped line,
// otherwise it will be converted to DOWNSTREAM.
class CORE_EXPORT VisiblePosition final {
    DISALLOW_ALLOCATION();
public:
    VisiblePosition() : m_affinity(VP_DEFAULT_AFFINITY) { }

    // TODO(yosin) We should use |visiblePositionOf()| instead of constructor,
    // and make constructors to have private accessibility, since constructors
    // aren't simple, e.g. they update layout tree.
    explicit VisiblePosition(const Position&, TextAffinity = VP_DEFAULT_AFFINITY);

    // Node: Other than |visiblePositionOf()|, we should not use
    // |createWithoutCanonicalization()|.
    static VisiblePosition createWithoutCanonicalization(const PositionWithAffinity& canonicalized);

    // Intentionally delete |operator==()| and |operator!=()| for reducing
    // compilation error message.
    // TODO(yosin) We'll have |equals()| when we have use cases of checking
    // equality of both position and affinity.
    bool operator==(const VisiblePosition&) const = delete;
    bool operator!=(const VisiblePosition&) const = delete;

    bool isNull() const { return m_deepPosition.isNull(); }
    bool isNotNull() const { return m_deepPosition.isNotNull(); }
    bool isOrphan() const { return m_deepPosition.isOrphan(); }

    Position deepEquivalent() const { return m_deepPosition; }
    Position toParentAnchoredPosition() const { return deepEquivalent().parentAnchoredEquivalent(); }
    PositionWithAffinity toPositionWithAffinity() const { return PositionWithAffinity(m_deepPosition, m_affinity); }
    TextAffinity affinity() const { return m_affinity; }

    DECLARE_TRACE();

#ifndef NDEBUG
    void debugPosition(const char* msg = "") const;
    void formatForDebugger(char* buffer, unsigned length) const;
    void showTreeForThis() const;
#endif

private:
    // TODO(yosin) We should use |PositionWithAffinity| to make
    // |toPositionWithAffinity()| simpler.
    Position m_deepPosition;
    TextAffinity m_affinity;
};

// TODO(yosin) We should move |absoluteCaretBoundsOf()| to "VisibleUnits.cpp".
// Bounds of (possibly transformed) caret in absolute coords
CORE_EXPORT IntRect absoluteCaretBoundsOf(const VisiblePosition&);

// Abs x/y position of the caret ignoring transforms.
// TODO(yosin) navigation with transforms should be smarter.
// TODO(yosin) We should move a glboal funciton
// |lineDirectionPointForBlockDirectionNavigationOf()| to "FrameSelection.h"
// as static function.
int lineDirectionPointForBlockDirectionNavigationOf(const VisiblePosition&);

// TODO(yosin) We should move |leftPositionOf()| and |rightPositionOf()| to
// "VisibleUnits.cpp".
VisiblePosition leftPositionOf(const VisiblePosition&);
VisiblePosition rightPositionOf(const VisiblePosition&);

CORE_EXPORT Position canonicalPositionOf(const Position&);
CORE_EXPORT PositionInComposedTree canonicalPositionOf(const PositionInComposedTree&);

// TODO(yosin) We should move |honorEditingBoundaryAtOr{Before,After} to
// "VisibleUnits.cpp" as static function.
// next() and previous() will increment/decrement by a character cluster.
VisiblePosition honorEditingBoundaryAtOrBefore(const VisiblePosition&, const Position& anchor);
PositionWithAffinity honorEditingBoundaryAtOrBeforeOf(const PositionWithAffinity&, const Position& anchor);
PositionInComposedTreeWithAffinity honorEditingBoundaryAtOrBeforeOf(const PositionInComposedTreeWithAffinity&, const PositionInComposedTree& anchor);
VisiblePosition honorEditingBoundaryAtOrAfter(const VisiblePosition&, const Position& anchor);

CORE_EXPORT VisiblePosition visiblePositionOf(const Position&, TextAffinity = VP_DEFAULT_AFFINITY);
CORE_EXPORT VisiblePosition visiblePositionOf(const PositionWithAffinity&);
CORE_EXPORT VisiblePosition visiblePositionOf(const PositionInComposedTree&, TextAffinity = VP_DEFAULT_AFFINITY);

} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const blink::VisiblePosition*);
void showTree(const blink::VisiblePosition&);
#endif

#endif // VisiblePosition_h
