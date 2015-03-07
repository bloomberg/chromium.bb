/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/PseudoElement.h"

#include "core/dom/FirstLetterPseudoElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutQuote.h"
#include "core/layout/style/ContentData.h"

namespace blink {

PassRefPtrWillBeRawPtr<PseudoElement> PseudoElement::create(Element* parent, PseudoId pseudoId)
{
    return adoptRefWillBeNoop(new PseudoElement(parent, pseudoId));
}

const QualifiedName& pseudoElementTagName(PseudoId pseudoId)
{
    switch (pseudoId) {
    case AFTER: {
        DEFINE_STATIC_LOCAL(QualifiedName, after, (nullAtom, "<pseudo:after>", nullAtom));
        return after;
    }
    case BEFORE: {
        DEFINE_STATIC_LOCAL(QualifiedName, before, (nullAtom, "<pseudo:before>", nullAtom));
        return before;
    }
    case BACKDROP: {
        DEFINE_STATIC_LOCAL(QualifiedName, backdrop, (nullAtom, "<pseudo:backdrop>", nullAtom));
        return backdrop;
    }
    case FIRST_LETTER: {
        DEFINE_STATIC_LOCAL(QualifiedName, firstLetter, (nullAtom, "<pseudo:first-letter>", nullAtom));
        return firstLetter;
    }
    default: {
        ASSERT_NOT_REACHED();
    }
    }
    DEFINE_STATIC_LOCAL(QualifiedName, name, (nullAtom, "<pseudo>", nullAtom));
    return name;
}

String PseudoElement::pseudoElementNameForEvents(PseudoId pseudoId)
{
    DEFINE_STATIC_LOCAL(const String, after, ("::after"));
    DEFINE_STATIC_LOCAL(const String, before, ("::before"));
    switch (pseudoId) {
    case AFTER:
        return after;
    case BEFORE:
        return before;
    default:
        return emptyString();
    }
}

PseudoElement::PseudoElement(Element* parent, PseudoId pseudoId)
    : Element(pseudoElementTagName(pseudoId), &parent->document(), CreateElement)
    , m_pseudoId(pseudoId)
{
    ASSERT(pseudoId != NOPSEUDO);
    parent->treeScope().adoptIfNeeded(*this);
    setParentOrShadowHostNode(parent);
    setHasCustomStyleCallbacks();
}

PassRefPtr<LayoutStyle> PseudoElement::customStyleForRenderer()
{
    return parentOrShadowHostElement()->layoutObject()->getCachedPseudoStyle(m_pseudoId);
}

void PseudoElement::dispose()
{
    ASSERT(parentOrShadowHostElement());

    InspectorInstrumentation::pseudoElementDestroyed(this);

    ASSERT(!nextSibling());
    ASSERT(!previousSibling());

    detach();
    RefPtrWillBeRawPtr<Element> parent = parentOrShadowHostElement();
    document().adoptIfNeeded(*this);
    setParentOrShadowHostNode(0);
    removedFrom(parent.get());
}

void PseudoElement::attach(const AttachContext& context)
{
    ASSERT(!layoutObject());

    Element::attach(context);

    LayoutObject* renderer = this->layoutObject();
    if (!renderer)
        return;

    LayoutStyle& style = renderer->mutableStyleRef();
    if (style.styleType() != BEFORE && style.styleType() != AFTER)
        return;
    ASSERT(style.contentData());

    for (const ContentData* content = style.contentData(); content; content = content->next()) {
        LayoutObject* child = content->createLayoutObject(document(), style);
        if (renderer->isChildAllowed(child, style)) {
            renderer->addChild(child);
            if (child->isQuote())
                toLayoutQuote(child)->attachQuote();
        } else
            child->destroy();
    }
}

bool PseudoElement::layoutObjectIsNeeded(const LayoutStyle& style)
{
    return pseudoElementRendererIsNeeded(&style);
}

void PseudoElement::didRecalcStyle(StyleRecalcChange)
{
    if (!layoutObject())
        return;

    // The renderers inside pseudo elements are anonymous so they don't get notified of recalcStyle and must have
    // the style propagated downward manually similar to LayoutObject::propagateStyleToAnonymousChildren.
    LayoutObject* renderer = this->layoutObject();
    for (LayoutObject* child = renderer->nextInPreOrder(renderer); child; child = child->nextInPreOrder(renderer)) {
        // We only manage the style for the generated content items.
        if (!child->isText() && !child->isQuote() && !child->isImage())
            continue;

        child->setPseudoStyle(renderer->style());
    }
}

} // namespace
