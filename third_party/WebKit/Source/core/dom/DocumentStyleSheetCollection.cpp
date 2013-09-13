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
#include "core/dom/DocumentStyleSheetCollection.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/StyleSheetCollections.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/page/Settings.h"
#include "core/svg/SVGStyleElement.h"

namespace WebCore {

using namespace HTMLNames;

DocumentStyleSheetCollection::DocumentStyleSheetCollection(TreeScope& treeScope)
    : StyleSheetCollection(treeScope)
{
    ASSERT(treeScope.rootNode() == &treeScope.rootNode()->document());
}

void DocumentStyleSheetCollection::collectStyleSheets(StyleSheetCollections* collections, Vector<RefPtr<StyleSheet> >& styleSheets, Vector<RefPtr<CSSStyleSheet> >& activeSheets)
{
    if (document()->settings() && !document()->settings()->authorAndUserStylesEnabled())
        return;

    DocumentOrderedList::iterator begin = m_styleSheetCandidateNodes.begin();
    DocumentOrderedList::iterator end = m_styleSheetCandidateNodes.end();
    for (DocumentOrderedList::iterator it = begin; it != end; ++it) {
        Node* n = *it;
        StyleSheet* sheet = 0;
        CSSStyleSheet* activeSheet = 0;
        if (n->nodeType() == Node::PROCESSING_INSTRUCTION_NODE && !document()->isHTMLDocument()) {
            // Processing instruction (XML documents only).
            // We don't support linking to embedded CSS stylesheets, see <https://bugs.webkit.org/show_bug.cgi?id=49281> for discussion.
            ProcessingInstruction* pi = toProcessingInstruction(n);
            // Don't apply XSL transforms to already transformed documents -- <rdar://problem/4132806>
            if (pi->isXSL() && !document()->transformSourceDocument()) {
                // Don't apply XSL transforms until loading is finished.
                if (!document()->parsing() && !pi->isLoading())
                    document()->applyXSLTransform(pi);
                return;
            }
            sheet = pi->sheet();
            if (sheet && !sheet->disabled() && sheet->isCSSStyleSheet())
                activeSheet = static_cast<CSSStyleSheet*>(sheet);
        } else if ((n->isHTMLElement() && (n->hasTagName(linkTag) || n->hasTagName(styleTag))) || (n->isSVGElement() && n->hasTagName(SVGNames::styleTag))) {
            Element* e = toElement(n);
            AtomicString title = e->getAttribute(titleAttr);
            bool enabledViaScript = false;
            if (e->hasLocalName(linkTag)) {
                // <LINK> element
                HTMLLinkElement* linkElement = toHTMLLinkElement(n);
                enabledViaScript = linkElement->isEnabledViaScript();
                if (!linkElement->isDisabled() && linkElement->styleSheetIsLoading()) {
                    // it is loading but we should still decide which style sheet set to use
                    if (!enabledViaScript && !title.isEmpty() && collections->preferredStylesheetSetName().isEmpty()) {
                        const AtomicString& rel = e->getAttribute(relAttr);
                        if (!rel.contains("alternate")) {
                            collections->setPreferredStylesheetSetName(title);
                            collections->setSelectedStylesheetSetName(title);
                        }
                    }

                    continue;
                }
                sheet = linkElement->sheet();
                if (!sheet)
                    title = nullAtom;
            } else if (n->isSVGElement() && n->hasTagName(SVGNames::styleTag)) {
                sheet = toSVGStyleElement(n)->sheet();
            } else {
                sheet = toHTMLStyleElement(n)->sheet();
            }

            if (sheet && !sheet->disabled() && sheet->isCSSStyleSheet())
                activeSheet = static_cast<CSSStyleSheet*>(sheet);

            // Check to see if this sheet belongs to a styleset
            // (thus making it PREFERRED or ALTERNATE rather than
            // PERSISTENT).
            AtomicString rel = e->getAttribute(relAttr);
            if (!enabledViaScript && sheet && !title.isEmpty()) {
                // Yes, we have a title.
                if (collections->preferredStylesheetSetName().isEmpty()) {
                    // No preferred set has been established. If
                    // we are NOT an alternate sheet, then establish
                    // us as the preferred set. Otherwise, just ignore
                    // this sheet.
                    if (e->hasLocalName(styleTag) || !rel.contains("alternate")) {
                        collections->setPreferredStylesheetSetName(title);
                        collections->setSelectedStylesheetSetName(title);
                    }
                }
                if (title != collections->preferredStylesheetSetName())
                    activeSheet = 0;
            }

            if (rel.contains("alternate") && title.isEmpty())
                activeSheet = 0;
        }
        if (sheet)
            styleSheets.append(sheet);
        if (activeSheet)
            activeSheets.append(activeSheet);
    }
}

static void collectActiveCSSStyleSheetsFromSeamlessParents(Vector<RefPtr<CSSStyleSheet> >& sheets, Document* document)
{
    HTMLIFrameElement* seamlessParentIFrame = document->seamlessParentIFrame();
    if (!seamlessParentIFrame)
        return;
    sheets.append(seamlessParentIFrame->document().styleSheetCollections()->activeAuthorStyleSheets());
}

bool DocumentStyleSheetCollection::updateActiveStyleSheets(StyleSheetCollections* collections, StyleResolverUpdateMode updateMode)
{
    Vector<RefPtr<StyleSheet> > styleSheets;
    Vector<RefPtr<CSSStyleSheet> > activeCSSStyleSheets;
    activeCSSStyleSheets.append(collections->injectedAuthorStyleSheets());
    activeCSSStyleSheets.append(collections->documentAuthorStyleSheets());
    collectActiveCSSStyleSheetsFromSeamlessParents(activeCSSStyleSheets, document());
    collectStyleSheets(collections, styleSheets, activeCSSStyleSheets);

    StyleResolverUpdateType styleResolverUpdateType;
    bool requiresFullStyleRecalc;
    analyzeStyleSheetChange(updateMode, activeAuthorStyleSheets(), activeCSSStyleSheets, styleResolverUpdateType, requiresFullStyleRecalc);

    if (styleResolverUpdateType == Reconstruct) {
        document()->clearStyleResolver();
    } else {
        StyleResolver* styleResolver = document()->styleResolverIfExists();
        ASSERT(styleResolver);
        // FIXME: We might have already had styles in child treescope. In this case, we cannot use buildScopedStyleTreeInDocumentOrder.
        // Need to change "false" to some valid condition.
        styleResolver->setBuildScopedStyleTreeInDocumentOrder(false);
        if (styleResolverUpdateType == Reset) {
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
