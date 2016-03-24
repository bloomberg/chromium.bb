// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultSpacing.h"

#include "platform/fonts/FontDescription.h"
#include "platform/text/TextRun.h"

namespace blink {

ShapeResultSpacing::ShapeResultSpacing(const TextRun& run,
    const FontDescription& fontDescription)
    : m_textRun(run)
    , m_letterSpacing(fontDescription.letterSpacing())
    , m_wordSpacing(fontDescription.wordSpacing())
    , m_expansion(run.expansion())
    , m_expansionPerOpportunity(0)
    , m_expansionOpportunityCount(0)
    , m_textJustify(TextJustify::TextJustifyAuto)
    , m_hasSpacing(false)
    , m_normalizeSpace(run.normalizeSpace())
    , m_allowTabs(run.allowTabs())
    , m_isAfterExpansion(false)
    , m_isVerticalOffset(fontDescription.isVerticalAnyUpright())
{
    if (m_textRun.spacingDisabled())
        return;

    if (!m_letterSpacing && !m_wordSpacing && !m_expansion)
        return;

    m_hasSpacing = true;

    if (!m_expansion)
        return;

    // Setup for justifications (expansions.)
    m_textJustify = run.getTextJustify();
    m_isAfterExpansion = !run.allowsLeadingExpansion();

    bool isAfterExpansion = m_isAfterExpansion;
    m_expansionOpportunityCount = Character::expansionOpportunityCount(run,
        isAfterExpansion);
    if (isAfterExpansion && !run.allowsTrailingExpansion()) {
        ASSERT(m_expansionOpportunityCount > 0);
        --m_expansionOpportunityCount;
    }

    if (m_expansionOpportunityCount)
        m_expansionPerOpportunity = m_expansion / m_expansionOpportunityCount;
}

float ShapeResultSpacing::nextExpansion()
{
    if (!m_expansionOpportunityCount) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    m_isAfterExpansion = true;

    if (!--m_expansionOpportunityCount) {
        float remaining = m_expansion;
        m_expansion = 0;
        return remaining;
    }

    m_expansion -= m_expansionPerOpportunity;
    return m_expansionPerOpportunity;
}

float ShapeResultSpacing::computeSpacing(const TextRun& run, size_t index,
    float& offset)
{
    if (run.is8Bit())
        return computeSpacing(run.characters8(), run.length(), index, offset);
    return computeSpacing(run.characters16(), run.length(), index, offset);
}

} // namespace blink
