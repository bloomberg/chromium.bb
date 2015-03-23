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

#include "core/editing/FrameSelection.h"

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

DEFINE_TRACE(PendingSelection)
{
    visitor->trace(m_start);
    visitor->trace(m_end);
    visitor->trace(m_extent);
}

} // namespace blink
