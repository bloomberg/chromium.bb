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
#include "core/dom/StyleSheetCollection.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleInvalidationAnalysis.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/Element.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/page/Page.h"
#include "core/page/PageGroup.h"
#include "core/page/Settings.h"
#include "core/page/UserContentURLPattern.h"
#include "core/svg/SVGStyleElement.h"

namespace WebCore {

using namespace HTMLNames;

StyleSheetCollection::StyleSheetCollection(TreeScope* treeScope)
    : m_treeScope(treeScope)
    , m_hadActiveLoadingStylesheet(false)
{
}

void StyleSheetCollection::addStyleSheetCandidateNode(Node* node, bool createdByParser)
{
    if (!node->inDocument())
        return;

    // Until the <body> exists, we have no choice but to compare document positions,
    // since styles outside of the body and head continue to be shunted into the head
    // (and thus can shift to end up before dynamically added DOM content that is also
    // outside the body).
    if (createdByParser && document()->body())
        m_styleSheetCandidateNodes.parserAdd(node);
    else
        m_styleSheetCandidateNodes.add(node);

    if (!isHTMLStyleElement(node))
        return;

    ContainerNode* scopingNode = toHTMLStyleElement(node)->scopingNode();
    if (!isTreeScopeRoot(scopingNode))
        m_scopingNodesForStyleScoped.add(scopingNode);
}

void StyleSheetCollection::removeStyleSheetCandidateNode(Node* node, ContainerNode* scopingNode)
{
    m_styleSheetCandidateNodes.remove(node);

    if (!isTreeScopeRoot(scopingNode))
        m_scopingNodesForStyleScoped.remove(scopingNode);
}

void StyleSheetCollection::collectStyleSheets(DocumentStyleSheetCollection* collections, Vector<RefPtr<StyleSheet> >& styleSheets, Vector<RefPtr<CSSStyleSheet> >& activeSheets)
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
            ProcessingInstruction* pi = static_cast<ProcessingInstruction*>(n);
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
                sheet = static_cast<SVGStyleElement*>(n)->sheet();
            } else {
                sheet = static_cast<HTMLStyleElement*>(n)->sheet();
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

StyleSheetCollection::StyleResolverUpdateType StyleSheetCollection::compareStyleSheets(const Vector<RefPtr<CSSStyleSheet> >& oldStyleSheets, const Vector<RefPtr<CSSStyleSheet> >& newStylesheets, Vector<StyleSheetContents*>& addedSheets)
{
    // Find out which stylesheets are new.
    unsigned newStylesheetCount = newStylesheets.size();
    unsigned oldStylesheetCount = oldStyleSheets.size();
    if (newStylesheetCount < oldStylesheetCount)
        return Reconstruct;

    unsigned newIndex = 0;
    for (unsigned oldIndex = 0; oldIndex < oldStylesheetCount; ++oldIndex) {
        if (newIndex >= newStylesheetCount)
            return Reconstruct;
        while (oldStyleSheets[oldIndex] != newStylesheets[newIndex]) {
            addedSheets.append(newStylesheets[newIndex]->contents());
            ++newIndex;
            if (newIndex == newStylesheetCount)
                return Reconstruct;
        }
        ++newIndex;
    }
    bool hasInsertions = !addedSheets.isEmpty();
    while (newIndex < newStylesheetCount) {
        addedSheets.append(newStylesheets[newIndex]->contents());
        ++newIndex;
    }
    // If all new sheets were added at the end of the list we can just add them to existing StyleResolver.
    // If there were insertions we need to re-add all the stylesheets so rules are ordered correctly.
    return hasInsertions ? Reset : Additive;
}

bool StyleSheetCollection::activeLoadingStyleSheetLoaded(const Vector<RefPtr<CSSStyleSheet> >& newStyleSheets)
{
    // StyleSheets of <style> elements that @import stylesheets are active but loading. We need to trigger a full recalc when such loads are done.
    bool hasActiveLoadingStylesheet = false;
    unsigned newStylesheetCount = newStyleSheets.size();
    for (unsigned i = 0; i < newStylesheetCount; ++i) {
        if (newStyleSheets[i]->isLoading())
            hasActiveLoadingStylesheet = true;
    }
    if (m_hadActiveLoadingStylesheet && !hasActiveLoadingStylesheet) {
        m_hadActiveLoadingStylesheet = false;
        return true;
    }
    m_hadActiveLoadingStylesheet = hasActiveLoadingStylesheet;
    return false;
}

void StyleSheetCollection::analyzeStyleSheetChange(StyleResolverUpdateMode updateMode, const Vector<RefPtr<CSSStyleSheet> >& oldStyleSheets, const Vector<RefPtr<CSSStyleSheet> >& newStyleSheets, StyleResolverUpdateType& styleResolverUpdateType, bool& requiresFullStyleRecalc)
{
    styleResolverUpdateType = Reconstruct;
    requiresFullStyleRecalc = true;

    if (activeLoadingStyleSheetLoaded(newStyleSheets))
        return;

    if (updateMode != AnalyzedStyleUpdate)
        return;
    if (!document()->styleResolverIfExists())
        return;

    // Find out which stylesheets are new.
    Vector<StyleSheetContents*> addedSheets;
    styleResolverUpdateType = compareStyleSheets(oldStyleSheets, newStyleSheets, addedSheets);

    // If we are already parsing the body and so may have significant amount of elements, put some effort into trying to avoid style recalcs.
    if (!document()->body() || document()->hasNodesWithPlaceholderStyle())
        return;
    StyleInvalidationAnalysis invalidationAnalysis(addedSheets);
    if (invalidationAnalysis.dirtiesAllStyle())
        return;
    invalidationAnalysis.invalidateStyle(document());
    requiresFullStyleRecalc = false;
}

static void collectActiveCSSStyleSheetsFromSeamlessParents(Vector<RefPtr<CSSStyleSheet> >& sheets, Document* document)
{
    HTMLIFrameElement* seamlessParentIFrame = document->seamlessParentIFrame();
    if (!seamlessParentIFrame)
        return;
    sheets.append(seamlessParentIFrame->document()->styleSheetCollection()->activeAuthorStyleSheets());
}

bool StyleSheetCollection::updateActiveStyleSheets(DocumentStyleSheetCollection* collections, StyleResolverUpdateMode updateMode, StyleResolverUpdateType& styleResolverUpdateType)
{
    Vector<RefPtr<StyleSheet> > styleSheets;
    Vector<RefPtr<CSSStyleSheet> > activeCSSStyleSheets;
    activeCSSStyleSheets.append(collections->injectedAuthorStyleSheets());
    activeCSSStyleSheets.append(collections->documentAuthorStyleSheets());
    collectActiveCSSStyleSheetsFromSeamlessParents(activeCSSStyleSheets, document());
    collectStyleSheets(collections, styleSheets, activeCSSStyleSheets);

    bool requiresFullStyleRecalc;
    analyzeStyleSheetChange(updateMode, activeAuthorStyleSheets(), activeCSSStyleSheets, styleResolverUpdateType, requiresFullStyleRecalc);

    if (styleResolverUpdateType == Reconstruct) {
        document()->clearStyleResolver();
    } else {
        StyleResolver* styleResolver = document()->styleResolver();
        styleResolver->setBuildScopedStyleTreeInDocumentOrder(!scopingNodesForStyleScoped());
        if (styleResolverUpdateType == Reset) {
            if (DocumentOrderedList* styleScopedScopingNodes = scopingNodesForStyleScoped()) {
                for (DocumentOrderedList::iterator it = styleScopedScopingNodes->begin(); it != styleScopedScopingNodes->end(); ++it)
                    styleResolver->resetAuthorStyle(toContainerNode(*it));
            }
            if (ListHashSet<Node*, 4>* removedNodes = scopingNodesRemoved()) {
                for (ListHashSet<Node*, 4>::iterator it = removedNodes->begin(); it != removedNodes->end(); ++it)
                    styleResolver->resetAuthorStyle(toContainerNode(*it));
            }
            ASSERT(m_treeScope->rootNode() == document());
            styleResolver->resetAuthorStyle(toContainerNode(m_treeScope->rootNode()));
            styleResolver->appendAuthorStyleSheets(0, activeCSSStyleSheets);
        } else {
            ASSERT(styleResolverUpdateType == Additive);
            styleResolver->appendAuthorStyleSheets(m_activeAuthorStyleSheets.size(), activeCSSStyleSheets);
        }
    }
    m_scopingNodesForStyleScoped.didRemoveScopingNodes();
    m_activeAuthorStyleSheets.swap(activeCSSStyleSheets);
    m_styleSheetsForStyleSheetList.swap(styleSheets);

    return requiresFullStyleRecalc;
}

}
