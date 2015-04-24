// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/GranularityStrategy.h"

#include "core/editing/htmlediting.h"

namespace blink {

GranularityStrategy::GranularityStrategy() { }

GranularityStrategy::~GranularityStrategy() { }

CharacterGranularityStrategy::CharacterGranularityStrategy() { }

CharacterGranularityStrategy::~CharacterGranularityStrategy() { }

SelectionStrategy CharacterGranularityStrategy::GetType() const
{
    return SelectionStrategy::Character;
}

void CharacterGranularityStrategy::Clear() { };

VisibleSelection CharacterGranularityStrategy::updateExtent(const VisiblePosition& extentPosition, const VisibleSelection& selection)
{
    return VisibleSelection(selection.visibleBase(), extentPosition);
}

DirectionGranularityStrategy::DirectionGranularityStrategy()
    : m_granularity(CharacterGranularity)
    , m_lastMoveShrunkSelection(false) { }

DirectionGranularityStrategy::~DirectionGranularityStrategy() { }

SelectionStrategy DirectionGranularityStrategy::GetType() const
{
    return SelectionStrategy::Direction;
}

void DirectionGranularityStrategy::Clear()
{
    m_granularity = CharacterGranularity;
    m_lastMoveShrunkSelection = false;
}

VisiblePosition DirectionGranularityStrategy::nextWordBound(
    const VisiblePosition& pos,
    SearchDirection direction,
    BoundAdjust wordBoundAdjust)
{
    bool nextBoundIfOnBound = wordBoundAdjust ==  BoundAdjust::NextBoundIfOnBound;
    if (direction == SearchDirection::SearchForward) {
        EWordSide wordSide = nextBoundIfOnBound ? RightWordIfOnBoundary : LeftWordIfOnBoundary;
        return endOfWord(pos, wordSide);
    }
    EWordSide wordSide = nextBoundIfOnBound ? LeftWordIfOnBoundary : RightWordIfOnBoundary;
    return startOfWord(pos, wordSide);
}

VisibleSelection DirectionGranularityStrategy::updateExtent(const VisiblePosition& extentPosition, const VisibleSelection& selection)
{
    if (extentPosition == selection.visibleExtent())
        return selection;

    const VisiblePosition base = selection.visibleBase();
    const VisiblePosition oldExtentWithGranularity = selection.isBaseFirst() ? selection.visibleEnd() : selection.visibleStart();

    int extentBaseOrder = comparePositions(extentPosition, base);
    int oldExtentBaseOrder = comparePositions(oldExtentWithGranularity, base);

    bool extentBaseOrderSwitched = (extentBaseOrder > 0 && oldExtentBaseOrder < 0)
        || (extentBaseOrder < 0 && oldExtentBaseOrder > 0);

    // Determine the boundary of the 'current word', i.e. the boundary extending
    // beyond which should change the granularity to WordGranularity.
    // If the last move has shrunk the selection and is now exactly on the word
    // boundary - we need to take the next bound as the bound of the "current
    // word".
    VisiblePosition currentWordBoundary = nextWordBound(
        oldExtentWithGranularity,
        oldExtentBaseOrder > 0 ? SearchDirection::SearchForward : SearchDirection::SearchBackwards,
        m_lastMoveShrunkSelection ? BoundAdjust::NextBoundIfOnBound : BoundAdjust::CurrentPosIfOnBound);

    bool thisMoveShrunkSelection = (extentBaseOrder > 0 && comparePositions(extentPosition, selection.visibleExtent()) < 0)
        || (extentBaseOrder < 0 && comparePositions(extentPosition, selection.visibleExtent()) > 0);
    // If the extent-base order was switched, then the selection is now
    // expanding in a different direction than before. Therefore we need to
    // calculate the boundary of the 'current word' in this new direction in
    // order to be able to tell if the selection expanded beyond it.
    if (extentBaseOrderSwitched) {
        currentWordBoundary = nextWordBound(
            base,
            extentBaseOrder > 0 ? SearchDirection::SearchForward : SearchDirection::SearchBackwards,
            BoundAdjust::NextBoundIfOnBound);
        m_granularity = CharacterGranularity;
        // When the base/extent order switches it doesn't count as shrinking selection.
        thisMoveShrunkSelection = false;
    }

    bool expandedBeyondWordBoundary;
    if (extentBaseOrder > 0)
        expandedBeyondWordBoundary = comparePositions(extentPosition, currentWordBoundary) > 0;
    else
        expandedBeyondWordBoundary = comparePositions(extentPosition, currentWordBoundary) < 0;
    if (expandedBeyondWordBoundary) {
        m_granularity = WordGranularity;
    } else if (thisMoveShrunkSelection) {
        m_granularity = CharacterGranularity;
        m_lastMoveShrunkSelection = true;
    }

    m_lastMoveShrunkSelection = thisMoveShrunkSelection;
    VisibleSelection newSelection = selection;
    newSelection.setExtent(extentPosition);
    if (m_granularity == WordGranularity) {
        if (extentBaseOrder > 0)
            newSelection.setEndRespectingGranularity(m_granularity, LeftWordIfOnBoundary);
        else
            newSelection.setStartRespectingGranularity(m_granularity, RightWordIfOnBoundary);
    }

    return newSelection;
}

} // namespace blink
