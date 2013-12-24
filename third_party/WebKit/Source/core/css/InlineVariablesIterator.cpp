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
#include "core/css/InlineVariablesIterator.h"

#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Element.h"

namespace WebCore {

InlineVariablesIterator::InlineVariablesIterator(Element* element)
    : m_element(element)
{
    ASSERT(RuntimeEnabledFeatures::cssVariablesEnabled());

    const StylePropertySet* propertySet = element->inlineStyle();
    if (propertySet) {
        initRemainingNames(propertySet);
    }
}

PassRefPtr<InlineVariablesIterator> InlineVariablesIterator::create(Element* element)
{
    return adoptRef(new InlineVariablesIterator(element));
}

MutableStylePropertySet* InlineVariablesIterator::propertySet() const
{
    return m_element->ensureMutableInlineStyle();
}

} // namespace WebCore
