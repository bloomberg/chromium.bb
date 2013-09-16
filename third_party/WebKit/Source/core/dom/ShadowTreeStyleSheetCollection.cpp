/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "core/dom/ShadowTreeStyleSheetCollection.h"

#include "HTMLNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLStyleElement.h"
#include "core/page/Settings.h"

namespace WebCore {

using namespace HTMLNames;

ShadowTreeStyleSheetCollection::ShadowTreeStyleSheetCollection(ShadowRoot& shadowRoot)
    : StyleSheetCollection(shadowRoot)
{
}

void ShadowTreeStyleSheetCollection::collectStyleSheets(StyleEngine* collections, Vector<RefPtr<StyleSheet> >& styleSheets, Vector<RefPtr<CSSStyleSheet> >& activeSheets)
{
    if (document()->settings() && !document()->settings()->authorAndUserStylesEnabled())
        return;

    DocumentOrderedList::iterator begin = m_styleSheetCandidateNodes.begin();
    DocumentOrderedList::iterator end = m_styleSheetCandidateNodes.end();
    for (DocumentOrderedList::iterator it = begin; it != end; ++it) {
        Node* node = *it;
        StyleSheet* sheet = 0;
        CSSStyleSheet* activeSheet = 0;

        if (!node->isHTMLElement() || !node->hasTagName(styleTag))
            continue;

        Element* element = toElement(node);
        AtomicString title = element->getAttribute(titleAttr);
        bool enabledViaScript = false;

        sheet = toHTMLStyleElement(node)->sheet();
        if (sheet && !sheet->disabled() && sheet->isCSSStyleSheet())
            activeSheet = static_cast<CSSStyleSheet*>(sheet);

        // FIXME: clarify how PREFERRED or ALTERNATE works in shadow trees.
        // Should we set preferred/selected stylesheets name in shadow trees and
        // use the name in document?
        AtomicString rel = element->getAttribute(relAttr);
        if (!enabledViaScript && sheet && !title.isEmpty()) {
            if (collections->preferredStylesheetSetName().isEmpty()) {
                if (element->hasLocalName(styleTag) || !rel.contains("alternate")) {
                    collections->setPreferredStylesheetSetName(title);
                    collections->setSelectedStylesheetSetName(title);
                }
            }
            if (title != collections->preferredStylesheetSetName())
                activeSheet = 0;
        }

        if (rel.contains("alternate") && title.isEmpty())
            activeSheet = 0;

        if (sheet)
            styleSheets.append(sheet);
        if (activeSheet)
            activeSheets.append(activeSheet);
    }
}

bool ShadowTreeStyleSheetCollection::updateActiveStyleSheets(StyleEngine* collections, StyleResolverUpdateMode updateMode)
{
    Vector<RefPtr<StyleSheet> > styleSheets;
    Vector<RefPtr<CSSStyleSheet> > activeCSSStyleSheets;
    collectStyleSheets(collections, styleSheets, activeCSSStyleSheets);

    bool requiresFullStyleRecalc = true;

    // If we have already decided to destroy StyleResolver, we don't need to analyze. Reconstruction will take care.
    if (StyleResolver* styleResolver = document()->styleResolverIfExists()) {
        StyleResolverUpdateType styleResolverUpdateType;

        analyzeStyleSheetChange(updateMode, activeAuthorStyleSheets(), activeCSSStyleSheets, styleResolverUpdateType, requiresFullStyleRecalc);

        // FIXME: We might have already had styles in child treescope. In this case, we cannot use buildScopedStyleTreeInDocumentOrder.
        // Need to change "false" to some valid condition.
        styleResolver->setBuildScopedStyleTreeInDocumentOrder(false);
        if (styleResolverUpdateType == Reset || styleResolverUpdateType == Reconstruct) {
            // We should not destroy StyleResolver when we find any stylesheet update in a shadow tree.
            // In this case, we will reset rulesets created from style elements in the shadow tree.
            resetAllRuleSetsInTreeScope(styleResolver);
            styleResolver->appendAuthorStyleSheets(0, activeCSSStyleSheets);
        } else {
            ASSERT(styleResolverUpdateType == Additive);
            styleResolver->appendAuthorStyleSheets(m_activeAuthorStyleSheets.size(), activeCSSStyleSheets);
        }
    }
    m_scopingNodesForStyleScoped.didRemoveScopingNodes();
    m_activeAuthorStyleSheets.swap(activeCSSStyleSheets);
    m_styleSheetsForStyleSheetList.swap(styleSheets);
    updateUsesRemUnits();

    return requiresFullStyleRecalc;
}

}
