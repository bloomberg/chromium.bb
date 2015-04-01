/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/layout/PendingSelection.h"

#include "core/dom/Document.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/html/HTMLTextFormControlElement.h"

namespace blink {

PendingSelection::PendingSelection()
    : m_affinity(SEL_DEFAULT_AFFINITY)
    , m_hasPendingSelection(false)
    , m_shouldShowBlockCursor(false)
{
    clear();
}

void PendingSelection::setSelection(const FrameSelection& selection)
{
    m_start = selection.start();
    m_end = selection.end();
    m_extent = selection.extent();
    m_affinity = selection.affinity();
    m_shouldShowBlockCursor = selection.shouldShowBlockCursor();
    m_hasPendingSelection = true;
}

void PendingSelection::clear()
{
    m_hasPendingSelection = false;
    m_start.clear();
    m_end.clear();
    m_extent.clear();
    m_affinity = SEL_DEFAULT_AFFINITY;
    m_shouldShowBlockCursor = false;
}

bool PendingSelection::isInDocument(const Document& document) const
{
    if (m_start.isNotNull() && (!m_start.inDocument() || m_start.document() != document))
        return false;
    if (m_end.isNotNull() && (!m_end.inDocument() || m_end.document() != document))
        return false;
    if (m_extent.isNotNull() && (!m_extent.inDocument() || m_extent.document() != document))
        return false;
    return true;
}

VisibleSelection PendingSelection::calcVisibleSelection() const
{
    SelectionType selectionType = VisibleSelection::selectionType(m_start, m_end);

    Position start = m_start;
    Position end = m_end;

    bool paintBlockCursor = m_shouldShowBlockCursor && selectionType == SelectionType::CaretSelection && !isLogicalEndOfLine(VisiblePosition(end, m_affinity));
    VisibleSelection selection;
    if (enclosingTextFormControl(start)) {
        Position endPosition = paintBlockCursor ? m_extent.next() : m_end;
        selection.setWithoutValidation(start, endPosition);
        return selection;
    }

    VisiblePosition visibleStart = VisiblePosition(start, selectionType == SelectionType::RangeSelection ? DOWNSTREAM : m_affinity);
    if (paintBlockCursor) {
        VisiblePosition visibleExtent(end, m_affinity);
        visibleExtent = visibleExtent.next(CanSkipOverEditingBoundary);
        return VisibleSelection(visibleStart, visibleExtent);
    }
    VisiblePosition visibleEnd(end, selectionType == SelectionType::RangeSelection ? UPSTREAM : m_affinity);
    return VisibleSelection(visibleStart, visibleEnd);
}

DEFINE_TRACE(PendingSelection)
{
    visitor->trace(m_start);
    visitor->trace(m_end);
    visitor->trace(m_extent);
}

} // namespace blink
