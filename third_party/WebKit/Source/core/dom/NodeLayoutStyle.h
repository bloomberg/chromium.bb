/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2008 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef NodeLayoutStyle_h
#define NodeLayoutStyle_h

#include "core/dom/Node.h"
#include "core/dom/NodeRenderingTraversal.h"
#include "core/html/HTMLOptGroupElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/style/LayoutStyle.h"

namespace blink {

inline const LayoutStyle* Node::layoutStyle() const
{
    return mutableLayoutStyle();
}

inline LayoutStyle* Node::mutableLayoutStyle() const
{
    if (LayoutObject* renderer = this->layoutObject())
        return renderer->style();
    // <option> and <optgroup> can be styled even if they don't get renderers,
    // so they store their style internally and return it through nonRendererStyle().
    // We check here explicitly to avoid the virtual call in the common case.
    if (isHTMLOptGroupElement(*this) || isHTMLOptionElement(this))
        return nonRendererStyle();
    return 0;
}

inline const LayoutStyle* Node::parentLayoutStyle() const
{
    ContainerNode* parent = NodeRenderingTraversal::parent(*this);
    return parent ? parent->layoutStyle() : 0;
}

inline const LayoutStyle& Node::layoutStyleRef() const
{
    const LayoutStyle* style = layoutStyle();
    ASSERT(style);
    return *style;
}

}
#endif // NodeLayoutStyle_h
