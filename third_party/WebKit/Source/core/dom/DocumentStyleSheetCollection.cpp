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
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/StyleEngine.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/frame/Settings.h"
#include "core/svg/SVGStyleElement.h"

namespace WebCore {

using namespace HTMLNames;

DocumentStyleSheetCollection::DocumentStyleSheetCollection(TreeScope& treeScope)
    : StyleSheetCollection(treeScope)
{
    ASSERT(treeScope.rootNode() == treeScope.rootNode()->document());
}

void DocumentStyleSheetCollection::collectStyleSheetsFromCandidates(StyleEngine* engine, StyleSheetCollectionBase& collection, DocumentStyleSheetCollection::CollectFor collectFor)
{
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
            if (RuntimeEnabledFeatures::xsltEnabled() && pi->isXSL() && !document()->transformSourceDocument()) {
                // Don't apply XSL transforms until loading is finished.
                if (!document()->parsing() && !pi->isLoading())
                    document()->applyXSLTransform(pi);
                return;
            }
            sheet = pi->sheet();
            if (sheet && !sheet->disabled() && sheet->isCSSStyleSheet())
                activeSheet = toCSSStyleSheet(sheet);
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
                    if (!enabledViaScript && !title.isEmpty() && engine->preferredStylesheetSetName().isEmpty()) {
                        const AtomicString& rel = e->getAttribute(relAttr);
                        if (!rel.contains("alternate")) {
                            engine->setPreferredStylesheetSetName(title);
                            engine->setSelectedStylesheetSetName(title);
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
                activeSheet = toCSSStyleSheet(sheet);

            // Check to see if this sheet belongs to a styleset
            // (thus making it PREFERRED or ALTERNATE rather than
            // PERSISTENT).
            AtomicString rel = e->getAttribute(relAttr);
            if (!enabledViaScript && sheet && !title.isEmpty()) {
                // Yes, we have a title.
                if (engine->preferredStylesheetSetName().isEmpty()) {
                    // No preferred set has been established. If
                    // we are NOT an alternate sheet, then establish
                    // us as the preferred set. Otherwise, just ignore
                    // this sheet.
                    if (e->hasLocalName(styleTag) || !rel.contains("alternate")) {
                        engine->setPreferredStylesheetSetName(title);
                        engine->setSelectedStylesheetSetName(title);
                    }
                }
                if (title != engine->preferredStylesheetSetName())
                    activeSheet = 0;
            }

            if (rel.contains("alternate") && title.isEmpty())
                activeSheet = 0;
        }

        if (sheet && collectFor == CollectForList)
            collection.appendSheetForList(sheet);
        if (activeSheet)
            collection.appendActiveStyleSheet(activeSheet);
    }
}

static void collectActiveCSSStyleSheetsFromSeamlessParents(StyleSheetCollectionBase& collection, Document* document)
{
    HTMLIFrameElement* seamlessParentIFrame = document->seamlessParentIFrame();
    if (!seamlessParentIFrame)
        return;
    collection.appendActiveStyleSheets(seamlessParentIFrame->document().styleEngine()->activeAuthorStyleSheets());
}

void DocumentStyleSheetCollection::collectStyleSheets(StyleEngine* engine, StyleSheetCollectionBase& collection, DocumentStyleSheetCollection::CollectFor colletFor)
{
    ASSERT(document()->styleEngine() == engine);
    collection.appendActiveStyleSheets(engine->injectedAuthorStyleSheets());
    collection.appendActiveStyleSheets(engine->documentAuthorStyleSheets());
    collectActiveCSSStyleSheetsFromSeamlessParents(collection, document());
    collectStyleSheetsFromCandidates(engine, collection, colletFor);
}

bool DocumentStyleSheetCollection::updateActiveStyleSheets(StyleEngine* engine, StyleResolverUpdateMode updateMode)
{
    StyleSheetCollectionBase collection;
    engine->collectDocumentActiveStyleSheets(collection);

    StyleSheetChange change;
    analyzeStyleSheetChange(updateMode, collection, change);

    if (change.styleResolverUpdateType == Reconstruct) {
        engine->clearMasterResolver();
        engine->resetFontSelector();
    } else if (StyleResolver* styleResolver = engine->resolver()) {
        // FIXME: We might have already had styles in child treescope. In this case, we cannot use buildScopedStyleTreeInDocumentOrder.
        // Need to change "false" to some valid condition.
        styleResolver->setBuildScopedStyleTreeInDocumentOrder(false);
        if (change.styleResolverUpdateType != Additive) {
            ASSERT(change.styleResolverUpdateType == Reset || change.styleResolverUpdateType == ResetStyleResolverAndFontSelector);
            resetAllRuleSetsInTreeScope(styleResolver);
            if (change.styleResolverUpdateType == ResetStyleResolverAndFontSelector)
                engine->resetFontSelector();
            styleResolver->removePendingAuthorStyleSheets(m_activeAuthorStyleSheets);
            styleResolver->lazyAppendAuthorStyleSheets(0, collection.activeAuthorStyleSheets());
        } else {
            styleResolver->lazyAppendAuthorStyleSheets(m_activeAuthorStyleSheets.size(), collection.activeAuthorStyleSheets());
        }
    } else if (change.styleResolverUpdateType == ResetStyleResolverAndFontSelector) {
        engine->resetFontSelector();
    }
    m_scopingNodesForStyleScoped.didRemoveScopingNodes();
    collection.swap(*this);
    updateUsesRemUnits();

    return change.requiresFullStyleRecalc;
}

}
