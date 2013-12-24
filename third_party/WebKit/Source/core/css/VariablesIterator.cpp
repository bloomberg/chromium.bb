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
#include "core/css/VariablesIterator.h"

#include "RuntimeEnabledFeatures.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Element.h"

namespace WebCore {

void AbstractVariablesIterator::initRemainingNames(
    const StylePropertySet* propertySet)
{
    const size_t propertyCount = propertySet->propertyCount();
    size_t variableCount = 0;
    Vector<AtomicString> remainingNames(propertyCount);
    for (int i = propertyCount; i--; ) {
        const StylePropertySet::PropertyReference& property = propertySet->propertyAt(i);
        if (property.id() == CSSPropertyVariable)
            remainingNames[variableCount++] = toCSSVariableValue(property.value())->name();
    }
    // FIXME: Make use of the Vector move constructor when rvalues are supported on all platforms.
    remainingNames.shrink(variableCount);

    takeRemainingNames(remainingNames);
}

VariablesIterator::VariablesIterator(MutableStylePropertySet* propertySet) :
    m_propertySet(propertySet)
{
    ASSERT(RuntimeEnabledFeatures::cssVariablesEnabled());
    initRemainingNames(propertySet);
}

PassRefPtr<VariablesIterator> VariablesIterator::create(MutableStylePropertySet* propertySet)
{
    return adoptRef(new VariablesIterator(propertySet));
}

String AbstractVariablesIterator::value() const
{
    return propertySet()->variableValue(name());
}

void AbstractVariablesIterator::addedVariable(const AtomicString& name)
{
    ASSERT(!m_remainingNames.contains(name));
    ASSERT(!m_newNames.contains(name));
    m_newNames.append(name);
}

void AbstractVariablesIterator::removedVariable(const AtomicString& name)
{
    size_t index = m_remainingNames.find(name);
    if (index != kNotFound)
        m_remainingNames.remove(index);
    index = m_newNames.find(name);
    if (index != kNotFound)
        m_newNames.remove(index);
}

void AbstractVariablesIterator::clearedVariables()
{
    m_remainingNames.clear();
    m_newNames.clear();
}

void AbstractVariablesIterator::advance()
{
    if (!atEnd())
        m_remainingNames.removeLast();
    if (!m_newNames.isEmpty()) {
        m_remainingNames.appendVector(m_newNames);
        m_newNames.clear();
    }
}

} // namespace WebCore
