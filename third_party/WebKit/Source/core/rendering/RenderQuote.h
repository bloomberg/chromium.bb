/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef RenderQuote_h
#define RenderQuote_h

#include "core/rendering/RenderInline.h"
#include "core/rendering/style/QuotesData.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/RenderStyleConstants.h"

namespace blink {

class Document;

class RenderQuote final : public RenderInline {
public:
    RenderQuote(Document*, const QuoteType);
    virtual ~RenderQuote();
    virtual void trace(Visitor*) override;
    void attachQuote();

private:
    void detachQuote();

    virtual void willBeDestroyed() override;
    virtual const char* renderName() const override { return "RenderQuote"; };
    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectQuote || RenderInline::isOfType(type); }
    virtual void styleDidChange(StyleDifference, const RenderStyle*) override;
    virtual void willBeRemovedFromTree() override;

    String computeText() const;
    void updateText();
    const QuotesData* quotesData() const;
    void updateDepth();
    bool isAttached() { return m_attached; }

    QuoteType m_type;
    int m_depth;
    RawPtrWillBeMember<RenderQuote> m_next;
    RawPtrWillBeMember<RenderQuote> m_previous;
    bool m_attached;
    String m_text;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderQuote, isQuote());

} // namespace blink

#endif // RenderQuote_h
