/*
 * Copyright (C) 2005 Apple Computer
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

#ifndef RenderButton_h
#define RenderButton_h

#include "core/html/HTMLInputElement.h"
#include "core/rendering/RenderFlexibleBox.h"

namespace blink {

// RenderButtons are just like normal flexboxes except that they will generate an anonymous block child.
// For inputs, they will also generate an anonymous RenderText and keep its style and content up
// to date as the button changes.
class RenderButton final : public RenderFlexibleBox {
public:
    explicit RenderButton(Element*);
    virtual ~RenderButton();

    virtual const char* renderName() const override { return "RenderButton"; }
    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectRenderButton || RenderFlexibleBox::isOfType(type); }

    virtual bool canBeSelectionLeaf() const override { return node() && node()->hasEditableStyle(); }
    virtual bool canCollapseAnonymousBlockChild() const override { return true; }

    virtual void addChild(RenderObject* newChild, RenderObject *beforeChild = 0) override;
    virtual void removeChild(RenderObject*) override;
    virtual void removeLeftoverAnonymousBlock(RenderBlock*) override { }
    virtual bool createsAnonymousWrapper() const override { return true; }

    // <button> should allow whitespace even though RenderFlexibleBox doesn't.
    virtual bool canHaveWhitespaceChildren() const override { return true; }

    virtual bool canHaveGeneratedChildren() const override;
    virtual bool hasControlClip() const override { return true; }
    virtual LayoutRect controlClipRect(const LayoutPoint&) const override;

    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode) const override;

private:
    virtual void updateAnonymousChildStyle(const RenderObject* child, RenderStyle* childStyle) const override;

    virtual bool hasLineIfEmpty() const override { return isHTMLInputElement(node()); }

    RenderBlock* m_inner;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderButton, isRenderButton());

} // namespace blink

#endif // RenderButton_h
