/*
 * Copyright (C) 2014 Robert Hogan <robert@roberthogan.net>
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

#ifndef RenderTextControlPlaceholder_h
#define RenderTextControlPlaceholder_h

#include "core/rendering/RenderBlockFlow.h"

namespace WebCore {


class RenderTextControlPlaceholder FINAL : public RenderBlockFlow {
public:
    explicit RenderTextControlPlaceholder(Element*);
    virtual ~RenderTextControlPlaceholder();

    // Placeholder elements are not laid out until the dimensions of their parent text control are known,
    // so they don't get layout until their parent has had layout. This is a problem when we have iterated
    // a parent's children and want to call isSelfCollapsingBlock. Fortunately since placeholder elements
    // only exist when they have content we can always enforce the rule that they are never self-collapsing.
    virtual bool isSelfCollapsingBlock() const OVERRIDE FINAL { return false; }
};

} // namespace WebCore

#endif // RenderTextControlPlaceholder_h
