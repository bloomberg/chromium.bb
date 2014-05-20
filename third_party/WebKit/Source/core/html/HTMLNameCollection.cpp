/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2011, 2012 Apple Inc. All rights reserved.
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
#include "core/html/HTMLNameCollection.h"

#include "HTMLNames.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeRareData.h"
#include "core/html/HTMLEmbedElement.h"
#include "core/html/HTMLObjectElement.h"

namespace WebCore {

using namespace HTMLNames;

HTMLNameCollection::HTMLNameCollection(ContainerNode& document, CollectionType type, const AtomicString& name)
    : HTMLCollection(document, type, OverridesItemAfter)
    , m_name(name)
{
}

HTMLNameCollection::~HTMLNameCollection()
{
    ASSERT(type() == WindowNamedItems || type() == DocumentNamedItems);
#if !ENABLE(OILPAN)
    ASSERT(ownerNode().isDocumentNode());
    ownerNode().nodeLists()->removeCache(this, type(), m_name);
#endif
}

Element* HTMLNameCollection::virtualItemAfter(Element* previous) const
{
    ASSERT(previous != ownerNode());

    Element* current;
    if (!previous)
        current = ElementTraversal::firstWithin(ownerNode());
    else
        current = ElementTraversal::next(*previous, &ownerNode());

    for (; current; current = ElementTraversal::next(*current, &ownerNode())) {
        switch (type()) {
        case WindowNamedItems:
            // find only images, forms, applets, embeds and objects by name,
            // but anything by id
            if (isHTMLImageElement(*current)
                || isHTMLFormElement(*current)
                || isHTMLAppletElement(*current)
                || isHTMLEmbedElement(*current)
                || isHTMLObjectElement(*current)) {
                if (current->getNameAttribute() == m_name)
                    return current;
            }
            if (current->getIdAttribute() == m_name)
                return current;
            break;
        case DocumentNamedItems:
            // find images, forms, applets, embeds, objects and iframes by name,
            // applets and object by id, and images by id but only if they have
            // a name attribute (this very strange rule matches IE)
            if (isHTMLFormElement(*current)
                || isHTMLIFrameElement(*current)
                || (isHTMLEmbedElement(*current) && toHTMLEmbedElement(*current).isExposed())) {
                if (current->getNameAttribute() == m_name)
                    return current;
            } else if (isHTMLAppletElement(*current)
                || (isHTMLObjectElement(*current) && toHTMLObjectElement(*current).isExposed())) {
                if (current->getNameAttribute() == m_name || current->getIdAttribute() == m_name)
                    return current;
            } else if (isHTMLImageElement(*current)) {
                if (current->getNameAttribute() == m_name || (current->getIdAttribute() == m_name && current->hasName()))
                    return current;
            }
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    return 0;
}

}
