// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GranularityStrategy_h
#define GranularityStrategy_h

#include "core/editing/SelectionStrategy.h"
#include "core/editing/VisibleSelection.h"

namespace blink {

class GranularityStrategy {
public:
    virtual ~GranularityStrategy();
    virtual SelectionStrategy GetType() const = 0;
    virtual void Clear() = 0;

    // Calculates and returns the new selection based on the updated user
    // selection extent |extentPosition| and the granularity strategy.
    virtual VisibleSelection updateExtent(const VisiblePosition& extentPosition, const VisibleSelection&) = 0;

protected:
    GranularityStrategy();
};

// Always uses character granularity.
class CharacterGranularityStrategy final : public GranularityStrategy {
public:
    CharacterGranularityStrategy();
    ~CharacterGranularityStrategy() final;

    // GranularityStrategy:
    SelectionStrategy GetType() const final;
    void Clear() final;
    VisibleSelection updateExtent(const VisiblePosition& extentPosition, const VisibleSelection&) final;
};

// "Expand by word, shrink by character" selection strategy.
// Uses character granularity when selection is shrinking. If the selection is
// expanding, granularity doesn't change until a word boundary is passed, after
// which the granularity switches to "word".
class DirectionGranularityStrategy final : public GranularityStrategy {
public:
    DirectionGranularityStrategy();
    ~DirectionGranularityStrategy() final;

    // GranularityStrategy:
    SelectionStrategy GetType() const final;
    void Clear() final;
    VisibleSelection updateExtent(const VisiblePosition&, const VisibleSelection&) final;

private:
    enum class BoundAdjust {CurrentPosIfOnBound, NextBoundIfOnBound};
    enum class SearchDirection {SearchBackwards, SearchForward};

    // Returns the next word boundary starting from |pos|. |direction| specifies
    // the direction in which to search for the next bound. nextIfOnBound
    // controls whether |pos| or the next boundary is returned when |pos| is
    // located exactly on word boundary.
    VisiblePosition nextWordBound(const VisiblePosition& /*pos*/, SearchDirection /*direction*/, BoundAdjust /*nextIfOnBound*/);

    // Current selection granularity being used
    TextGranularity m_granularity;
    // Set to true if the selection was shrunk (without changing relative
    // base/extent order) as a result of the most recent updateExtent call.
    bool m_lastMoveShrunkSelection;
};

} // namespace blink

#endif // GranularityStrategy_h
