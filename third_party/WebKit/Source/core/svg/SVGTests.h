/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2010 Rob Buis <buis@kde.org>
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

#ifndef SVGTests_h
#define SVGTests_h

#include "core/svg/SVGStaticStringList.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"

namespace blink {

class Document;
class QualifiedName;
class SVGElement;

class SVGTests : public WillBeGarbageCollectedMixin {
public:
    // JS API
    SVGStringListTearOff* requiredFeatures() { return m_requiredFeatures->tearOff(); }
    SVGStringListTearOff* requiredExtensions() { return m_requiredExtensions->tearOff(); }
    SVGStringListTearOff* systemLanguage() { return m_systemLanguage->tearOff(); }
    bool hasExtension(const String&);

    bool isValid(Document&) const;

    bool parseAttribute(const QualifiedName&, const AtomicString&);
    bool isKnownAttribute(const QualifiedName&);

    void addSupportedAttributes(HashSet<QualifiedName>&);

    DECLARE_VIRTUAL_TRACE();

protected:
    SVGTests();

    // FIXME: Oilpan: the construction of this mixin requires heap allocation,
    // which cannot be safely done with Oilpan until the object that include
    // this mixin have had its vptr initialized -- so as to be able to
    // accurately trace the entire object should a GC strike while constructing
    // the mixin.
    //
    // Try to come up with a more natural alternative and solution that doesn't
    // require hoisting the constructor code for mixins into a separate method
    // like initialize(). It makes construction of these heap-allocation mixins
    // safe in the meantime, however.
    //
    void initialize(SVGElement*);

private:
    RefPtrWillBeMember<SVGStaticStringList> m_requiredFeatures;
    RefPtrWillBeMember<SVGStaticStringList> m_requiredExtensions;
    RefPtrWillBeMember<SVGStaticStringList> m_systemLanguage;
};

} // namespace blink

#endif // SVGTests_h
