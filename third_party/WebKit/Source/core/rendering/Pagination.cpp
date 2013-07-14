/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/rendering/Pagination.h"

#include "core/rendering/style/RenderStyle.h"

namespace WebCore {

void Pagination::setStylesForPaginationMode(Mode paginationMode, RenderStyle* style)
{
    if (paginationMode == Unpaginated)
        return;

    switch (paginationMode) {
    case LeftToRightPaginated:
        style->setColumnAxis(HorizontalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isLeftToRightDirection() ? NormalColumnProgression : ReverseColumnProgression);
        else
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? ReverseColumnProgression : NormalColumnProgression);
        break;
    case RightToLeftPaginated:
        style->setColumnAxis(HorizontalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isLeftToRightDirection() ? ReverseColumnProgression : NormalColumnProgression);
        else
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? NormalColumnProgression : ReverseColumnProgression);
        break;
    case TopToBottomPaginated:
        style->setColumnAxis(VerticalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? ReverseColumnProgression : NormalColumnProgression);
        else
            style->setColumnProgression(style->isLeftToRightDirection() ? NormalColumnProgression : ReverseColumnProgression);
        break;
    case BottomToTopPaginated:
        style->setColumnAxis(VerticalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? NormalColumnProgression : ReverseColumnProgression);
        else
            style->setColumnProgression(style->isLeftToRightDirection() ? ReverseColumnProgression : NormalColumnProgression);
        break;
    case Unpaginated:
        ASSERT_NOT_REACHED();
        break;
    }
}

} // namespace WebCore
