/**
 * Copyright (C) 2006, 2007, 2010 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 *
 */

#include "config.h"
#include "core/layout/LayoutSearchField.h"

#include "core/InputTypeNames.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"

namespace blink {

using namespace HTMLNames;

// ----------------------------

LayoutSearchField::LayoutSearchField(HTMLInputElement* element)
    : LayoutTextControlSingleLine(element)
{
    ASSERT(element->type() == InputTypeNames::search);
}

LayoutSearchField::~LayoutSearchField()
{
}

inline Element* LayoutSearchField::searchDecorationElement() const
{
    return inputElement()->closedShadowRoot()->getElementById(ShadowElementNames::searchDecoration());
}

inline Element* LayoutSearchField::cancelButtonElement() const
{
    return inputElement()->closedShadowRoot()->getElementById(ShadowElementNames::clearButton());
}

LayoutUnit LayoutSearchField::computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const
{
    Element* searchDecoration = searchDecorationElement();
    if (LayoutBox* decorationRenderer = searchDecoration ? searchDecoration->layoutBox() : 0) {
        decorationRenderer->updateLogicalHeight();
        nonContentHeight = max(nonContentHeight, decorationRenderer->borderAndPaddingLogicalHeight() + decorationRenderer->marginLogicalHeight());
        lineHeight = max(lineHeight, decorationRenderer->logicalHeight());
    }
    Element* cancelButton = cancelButtonElement();
    if (LayoutBox* cancelRenderer = cancelButton ? cancelButton->layoutBox() : 0) {
        cancelRenderer->updateLogicalHeight();
        nonContentHeight = max(nonContentHeight, cancelRenderer->borderAndPaddingLogicalHeight() + cancelRenderer->marginLogicalHeight());
        lineHeight = max(lineHeight, cancelRenderer->logicalHeight());
    }

    return lineHeight + nonContentHeight;
}

LayoutUnit LayoutSearchField::computeLogicalHeightLimit() const
{
    return logicalHeight();
}

void LayoutSearchField::centerContainerIfNeeded(LayoutBox* containerRenderer) const
{
    if (!containerRenderer)
        return;

    if (containerRenderer->logicalHeight() <= contentLogicalHeight())
        return;

    // A quirk for find-in-page box on Safari Windows.
    // http://webkit.org/b/63157
    LayoutUnit logicalHeightDiff = containerRenderer->logicalHeight() - contentLogicalHeight();
    containerRenderer->setLogicalTop(containerRenderer->logicalTop() - (logicalHeightDiff / 2 + layoutMod(logicalHeightDiff, 2)));
}

}
