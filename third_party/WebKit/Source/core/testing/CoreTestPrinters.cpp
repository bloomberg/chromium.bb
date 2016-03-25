// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/Position.h"
#include <ios>
#include <ostream> // NOLINT

namespace blink {

namespace {

template <typename PositionType>
std::ostream& printPosition(std::ostream& ostream, const PositionType& position)
{
    if (position.isNull())
        return ostream << "null";
    ostream << position.anchorNode() << "@";
    if (position.isOffsetInAnchor())
        return ostream << position.offsetInContainerNode();
    return ostream << position.anchorType();
}

} // namespace

std::ostream& operator<<(std::ostream& ostream, PositionAnchorType anchorType)
{
    switch (anchorType) {
    case PositionAnchorType::AfterAnchor:
        return ostream << "afterAnchor";
    case PositionAnchorType::AfterChildren:
        return ostream << "afterChildren";
    case PositionAnchorType::BeforeAnchor:
        return ostream << "beforeAnchor";
    case PositionAnchorType::BeforeChildren:
        return ostream << "beforeChildren";
    case PositionAnchorType::OffsetInAnchor:
        return ostream << "offsetInAnchor";
    }
    ASSERT_NOT_REACHED();
    return ostream << "anchorType=" << static_cast<int>(anchorType);
}

std::ostream& operator<<(std::ostream& ostream, const Position& position)
{
    return printPosition(ostream, position);
}

std::ostream& operator<<(std::ostream& ostream, const PositionInFlatTree& position)
{
    return printPosition(ostream, position);
}

} // namespace blink
