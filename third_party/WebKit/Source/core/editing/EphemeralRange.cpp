// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/EphemeralRange.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"

namespace blink {

EphemeralRange::EphemeralRange(const Position& start, const Position& end)
    : m_startPosition(start)
    , m_endPosition(end)
#if ENABLE(ASSERT)
    , m_domTreeVersion(start.isNull() ? 0 : start.document()->domTreeVersion())
#endif
{
    if (m_startPosition.isNull()) {
        ASSERT(m_endPosition.isNull());
        return;
    }
    ASSERT(m_endPosition.isNotNull());
    ASSERT(m_startPosition.document() == m_endPosition.document());
    ASSERT(m_startPosition.inDocument());
    ASSERT(m_endPosition.inDocument());
}

EphemeralRange::EphemeralRange(const EphemeralRange& other)
    : EphemeralRange(other.m_startPosition, other.m_endPosition)
{
    ASSERT(other.isValid());
}

EphemeralRange::EphemeralRange(const Position& position)
    : EphemeralRange(position, position)
{
}

EphemeralRange::EphemeralRange(const Range* range)
{
    if (!range)
        return;
    m_startPosition = range->startPosition();
    m_endPosition = range->endPosition();
#if ENABLE(ASSERT)
    m_domTreeVersion = range->ownerDocument().domTreeVersion();
#endif
}

EphemeralRange::EphemeralRange()
{
}

EphemeralRange::~EphemeralRange()
{
}

EphemeralRange& EphemeralRange::operator=(const EphemeralRange& other)
{
    ASSERT(other.isValid());
    m_startPosition = other.m_startPosition;
    m_endPosition = other.m_endPosition;
#if ENABLE(ASSERT)
    m_domTreeVersion = other.m_domTreeVersion;
#endif
    return *this;
}

Document& EphemeralRange::document() const
{
    ASSERT(isNotNull());
    return *m_startPosition.document();
}

Position EphemeralRange::startPosition() const
{
    ASSERT(isValid());
    return m_startPosition;
}

Position EphemeralRange::endPosition() const
{
    ASSERT(isValid());
    return m_endPosition;
}

bool EphemeralRange::isCollapsed() const
{
    ASSERT(isValid());
    return m_startPosition == m_endPosition;
}

bool EphemeralRange::isNotNull() const
{
    ASSERT(isValid());
    return m_startPosition.isNotNull();
}

EphemeralRange EphemeralRange::rangeOfContents(const Node& node)
{
    return EphemeralRange(Position::firstPositionInNode(&const_cast<Node&>(node)), Position::lastPositionInNode(&const_cast<Node&>(node)));
}

#if ENABLE(ASSERT)
bool EphemeralRange::isValid() const
{
    return m_startPosition.isNull() || m_domTreeVersion == m_startPosition.document()->domTreeVersion();
}
#else
bool EphemeralRange::isValid() const
{
    return true;
}
#endif

} // namespace blink
