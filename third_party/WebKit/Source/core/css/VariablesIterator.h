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

#ifndef VariablesIterator_h
#define VariablesIterator_h

#include "core/css/CSSVariablesIterator.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace WebCore {

class MutableStylePropertySet;
class StylePropertySet;

class AbstractVariablesIterator : public CSSVariablesIterator {
public:
    virtual ~AbstractVariablesIterator() { }

protected:
    void takeRemainingNames(Vector<AtomicString>& remainingNames) { m_remainingNames.swap(remainingNames); }

    void initRemainingNames(const StylePropertySet*);

private:
    virtual void advance() OVERRIDE FINAL;
    virtual bool atEnd() const OVERRIDE FINAL { return m_remainingNames.isEmpty(); }
    virtual AtomicString name() const OVERRIDE FINAL { return m_remainingNames.last(); }
    virtual String value() const OVERRIDE FINAL;
    virtual void addedVariable(const AtomicString& name) OVERRIDE FINAL;
    virtual void removedVariable(const AtomicString& name) OVERRIDE FINAL;
    virtual void clearedVariables() OVERRIDE FINAL;

    virtual MutableStylePropertySet* propertySet() const = 0;

    Vector<AtomicString> m_remainingNames;
    Vector<AtomicString> m_newNames;
};

class VariablesIterator FINAL : public AbstractVariablesIterator {
public:
    static PassRefPtr<VariablesIterator> create(MutableStylePropertySet*);

private:
    explicit VariablesIterator(MutableStylePropertySet*);
    virtual MutableStylePropertySet* propertySet() const OVERRIDE { return m_propertySet.get(); }

    RefPtr<MutableStylePropertySet> m_propertySet;
};

} // namespace WebCore

#endif // VariablesIterator_h
