/*
 * Copyright (C) 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef HTMLPlugInImageElement_h
#define HTMLPlugInImageElement_h

#include "core/html/HTMLPlugInElement.h"

namespace WebCore {

// Base class for HTMLObjectElement and HTMLEmbedElement
class HTMLPlugInImageElement : public HTMLPlugInElement {
public:
    virtual ~HTMLPlugInImageElement();

protected:
    HTMLPlugInImageElement(const QualifiedName& tagName, Document&, bool createdByParser, PreferPlugInsForImagesOption);

private:
    virtual bool isPlugInImageElement() const OVERRIDE { return true; }

};

inline HTMLPlugInImageElement* toHTMLPlugInImageElement(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isPluginElement());
    HTMLPlugInElement* plugInElement = static_cast<HTMLPlugInElement*>(node);
    ASSERT_WITH_SECURITY_IMPLICATION(plugInElement->isPlugInImageElement());
    return static_cast<HTMLPlugInImageElement*>(plugInElement);
}

inline const HTMLPlugInImageElement* toHTMLPlugInImageElement(const Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isPluginElement());
    const HTMLPlugInElement* plugInElement = static_cast<const HTMLPlugInElement*>(node);
    ASSERT_WITH_SECURITY_IMPLICATION(plugInElement->isPlugInImageElement());
    return static_cast<const HTMLPlugInImageElement*>(plugInElement);
}

// This will catch anyone doing an unnecessary cast.
void toHTMLPlugInImageElement(const HTMLPlugInImageElement*);

} // namespace WebCore

#endif // HTMLPlugInImageElement_h
