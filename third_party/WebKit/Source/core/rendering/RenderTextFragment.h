/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef RenderTextFragment_h
#define RenderTextFragment_h

#include "core/rendering/RenderText.h"

namespace blink {

// Used to represent a text substring of an element, e.g., for text runs that are split because of
// first letter and that must therefore have different styles (and positions in the render tree).
// We cache offsets so that text transformations can be applied in such a way that we can recover
// the original unaltered string from our corresponding DOM node.
class RenderTextFragment final : public RenderText {
public:
    RenderTextFragment(Node*, StringImpl*, int startOffset, int length);
    RenderTextFragment(Node*, StringImpl*);
    virtual ~RenderTextFragment();
    virtual void trace(Visitor*) override;

    virtual bool isTextFragment() const override { return true; }

    virtual bool canBeSelectionLeaf() const override { return node() && node()->hasEditableStyle(); }

    unsigned start() const { return m_start; }
    unsigned end() const { return m_end; }
    virtual unsigned textStartOffset() const override { return start(); }

    RenderBoxModelObject* firstLetter() const { return m_firstLetter; }
    void setFirstLetter(RenderBoxModelObject* firstLetter) { m_firstLetter = firstLetter; }
    RenderText* firstRenderTextInFirstLetter() const;

    StringImpl* contentString() const { return m_contentString.get(); }
    virtual PassRefPtr<StringImpl> originalText() const override;

    virtual void setText(PassRefPtr<StringImpl>, bool force = false) override;

    virtual void transformText() override;

    virtual const char* renderName() const override final { return "RenderTextFragment"; }

protected:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

private:
    virtual void willBeDestroyed() override;

    virtual UChar previousCharacter() const override;
    RenderBlock* blockForAccompanyingFirstLetter() const;
    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&) override;

    unsigned m_start;
    unsigned m_end;
    RefPtr<StringImpl> m_contentString;
    RawPtrWillBeMember<RenderBoxModelObject> m_firstLetter;
};

DEFINE_TYPE_CASTS(RenderTextFragment, RenderObject, object, toRenderText(object)->isTextFragment(), toRenderText(object).isTextFragment());

} // namespace blink

#endif // RenderTextFragment_h
