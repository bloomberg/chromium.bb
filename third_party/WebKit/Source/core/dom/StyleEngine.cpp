/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
#include "core/dom/StyleEngine.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleInvalidationAnalysis.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/ShadowTreeStyleSheetCollection.h"
#include "core/dom/shadow/ShadowRoot.h"
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

StyleEngine::StyleEngine(Document& document)
    : m_document(document)
    , m_pendingStylesheets(0)
    , m_injectedStyleSheetCacheValid(false)
    , m_needsUpdateActiveStylesheetsOnStyleRecalc(false)
    , m_usesSiblingRules(false)
    , m_usesSiblingRulesOverride(false)
    , m_usesFirstLineRules(false)
    , m_usesFirstLetterRules(false)
    , m_usesRemUnits(false)
    , m_documentStyleSheetCollection(document)
    , m_needsDocumentStyleSheetsUpdate(true)
{
}

StyleEngine::~StyleEngine()
{
    if (m_pageUserSheet)
        m_pageUserSheet->clearOwnerNode();
    for (unsigned i = 0; i < m_injectedUserStyleSheets.size(); ++i)
        m_injectedUserStyleSheets[i]->clearOwnerNode();
    for (unsigned i = 0; i < m_injectedAuthorStyleSheets.size(); ++i)
        m_injectedAuthorStyleSheets[i]->clearOwnerNode();
    for (unsigned i = 0; i < m_userStyleSheets.size(); ++i)
        m_userStyleSheets[i]->clearOwnerNode();
    for (unsigned i = 0; i < m_authorStyleSheets.size(); ++i)
        m_authorStyleSheets[i]->clearOwnerNode();
}

void StyleEngine::insertTreeScopeInDocumentOrder(TreeScopeSet& treeScopes, TreeScope* treeScope)
{
    if (treeScopes.isEmpty()) {
        treeScopes.add(treeScope);
        return;
    }
    if (treeScopes.contains(treeScope))
        return;

    TreeScopeSet::iterator begin = treeScopes.begin();
    TreeScopeSet::iterator end = treeScopes.end();
    TreeScopeSet::iterator it = end;
    TreeScope* followingTreeScope = 0;
    do {
        --it;
        TreeScope* n = *it;
        unsigned short position = n->comparePosition(*treeScope);
        if (position & Node::DOCUMENT_POSITION_FOLLOWING) {
            treeScopes.insertBefore(followingTreeScope, treeScope);
            return;
        }
        followingTreeScope = n;
    } while (it != begin);

    treeScopes.insertBefore(followingTreeScope, treeScope);
}

StyleSheetCollection* StyleEngine::ensureStyleSheetCollectionFor(TreeScope& treeScope)
{
    if (&treeScope == &m_document)
        return &m_documentStyleSheetCollection;

    HashMap<TreeScope*, OwnPtr<StyleSheetCollection> >::AddResult result = m_styleSheetCollectionMap.add(&treeScope, nullptr);
    if (result.isNewEntry)
        result.iterator->value = adoptPtr(new ShadowTreeStyleSheetCollection(toShadowRoot(treeScope)));
    return result.iterator->value.get();
}

StyleSheetCollection* StyleEngine::styleSheetCollectionFor(TreeScope& treeScope)
{
    if (&treeScope == &m_document)
        return &m_documentStyleSheetCollection;

    HashMap<TreeScope*, OwnPtr<StyleSheetCollection> >::iterator it = m_styleSheetCollectionMap.find(&treeScope);
    if (it == m_styleSheetCollectionMap.end())
        return 0;
    return it->value.get();
}

const Vector<RefPtr<StyleSheet> >& StyleEngine::styleSheetsForStyleSheetList()
{
    return m_documentStyleSheetCollection.styleSheetsForStyleSheetList();
}

const Vector<RefPtr<CSSStyleSheet> >& StyleEngine::activeAuthorStyleSheets() const
{
    return m_documentStyleSheetCollection.activeAuthorStyleSheets();
}

void StyleEngine::getActiveAuthorStyleSheets(Vector<const Vector<RefPtr<CSSStyleSheet> >*>& activeAuthorStyleSheets) const
{
    activeAuthorStyleSheets.append(&m_documentStyleSheetCollection.activeAuthorStyleSheets());

    HashMap<TreeScope*, OwnPtr<StyleSheetCollection> >::const_iterator::Values begin = m_styleSheetCollectionMap.values().begin();
    HashMap<TreeScope*, OwnPtr<StyleSheetCollection> >::const_iterator::Values end = m_styleSheetCollectionMap.values().end();
    HashMap<TreeScope*, OwnPtr<StyleSheetCollection> >::const_iterator::Values it = begin;
    for (; it != end; ++it) {
        const StyleSheetCollection* collection = it->get();
        activeAuthorStyleSheets.append(&collection->activeAuthorStyleSheets());
    }
}

void StyleEngine::combineCSSFeatureFlags(const RuleFeatureSet& features)
{
    // Delay resetting the flags until after next style recalc since unapplying the style may not work without these set (this is true at least with before/after).
    m_usesSiblingRules = m_usesSiblingRules || features.usesSiblingRules();
    m_usesFirstLineRules = m_usesFirstLineRules || features.usesFirstLineRules();
}

void StyleEngine::resetCSSFeatureFlags(const RuleFeatureSet& features)
{
    m_usesSiblingRules = features.usesSiblingRules();
    m_usesFirstLineRules = features.usesFirstLineRules();
}

CSSStyleSheet* StyleEngine::pageUserSheet()
{
    if (m_pageUserSheet)
        return m_pageUserSheet.get();

    Page* owningPage = m_document.page();
    if (!owningPage)
        return 0;

    String userSheetText = owningPage->userStyleSheet();
    if (userSheetText.isEmpty())
        return 0;

    // Parse the sheet and cache it.
    m_pageUserSheet = CSSStyleSheet::createInline(&m_document, m_document.settings()->userStyleSheetLocation());
    m_pageUserSheet->contents()->setIsUserStyleSheet(true);
    m_pageUserSheet->contents()->parseString(userSheetText);
    return m_pageUserSheet.get();
}

void StyleEngine::clearPageUserSheet()
{
    if (m_pageUserSheet) {
        RefPtr<StyleSheet> removedSheet = m_pageUserSheet;
        m_pageUserSheet = 0;
        m_document.removedStyleSheet(removedSheet.get());
    }
}

void StyleEngine::updatePageUserSheet()
{
    clearPageUserSheet();
    // FIXME: Why is this immediately and not defer?
    if (StyleSheet* addedSheet = pageUserSheet())
        m_document.addedStyleSheet(addedSheet, RecalcStyleImmediately);
}

const Vector<RefPtr<CSSStyleSheet> >& StyleEngine::injectedUserStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedUserStyleSheets;
}

const Vector<RefPtr<CSSStyleSheet> >& StyleEngine::injectedAuthorStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedAuthorStyleSheets;
}

void StyleEngine::updateInjectedStyleSheetCache() const
{
    if (m_injectedStyleSheetCacheValid)
        return;
    m_injectedStyleSheetCacheValid = true;
    m_injectedUserStyleSheets.clear();
    m_injectedAuthorStyleSheets.clear();

    Page* owningPage = m_document.page();
    if (!owningPage)
        return;

    const PageGroup& pageGroup = owningPage->group();
    const UserStyleSheetVector& sheets = pageGroup.userStyleSheets();
    for (unsigned i = 0; i < sheets.size(); ++i) {
        const UserStyleSheet* sheet = sheets[i].get();
        if (sheet->injectedFrames() == InjectInTopFrameOnly && m_document.ownerElement())
            continue;
        if (!UserContentURLPattern::matchesPatterns(m_document.url(), sheet->whitelist(), sheet->blacklist()))
            continue;
        RefPtr<CSSStyleSheet> groupSheet = CSSStyleSheet::createInline(const_cast<Document*>(&m_document), sheet->url());
        bool isUserStyleSheet = sheet->level() == UserStyleUserLevel;
        if (isUserStyleSheet)
            m_injectedUserStyleSheets.append(groupSheet);
        else
            m_injectedAuthorStyleSheets.append(groupSheet);
        groupSheet->contents()->setIsUserStyleSheet(isUserStyleSheet);
        groupSheet->contents()->parseString(sheet->source());
    }
}

void StyleEngine::invalidateInjectedStyleSheetCache()
{
    m_injectedStyleSheetCacheValid = false;
    m_needsDocumentStyleSheetsUpdate = true;
    // FIXME: updateInjectedStyleSheetCache is called inside StyleSheetCollection::updateActiveStyleSheets
    // and batch updates lots of sheets so we can't call addedStyleSheet() or removedStyleSheet().
    m_document.styleResolverChanged(RecalcStyleDeferred);
}

void StyleEngine::addAuthorSheet(PassRefPtr<StyleSheetContents> authorSheet)
{
    ASSERT(!authorSheet->isUserStyleSheet());
    m_authorStyleSheets.append(CSSStyleSheet::create(authorSheet, &m_document));
    m_document.addedStyleSheet(m_authorStyleSheets.last().get(), RecalcStyleImmediately);
    m_needsDocumentStyleSheetsUpdate = true;
}

void StyleEngine::addUserSheet(PassRefPtr<StyleSheetContents> userSheet)
{
    ASSERT(userSheet->isUserStyleSheet());
    m_userStyleSheets.append(CSSStyleSheet::create(userSheet, &m_document));
    m_document.addedStyleSheet(m_userStyleSheets.last().get(), RecalcStyleImmediately);
    m_needsDocumentStyleSheetsUpdate = true;
}

// This method is called whenever a top-level stylesheet has finished loading.
void StyleEngine::removePendingSheet(Node* styleSheetCandidateNode, RemovePendingSheetNotificationType notification)
{
    // Make sure we knew this sheet was pending, and that our count isn't out of sync.
    ASSERT(m_pendingStylesheets > 0);

    m_pendingStylesheets--;

    TreeScope* treeScope = isHTMLStyleElement(styleSheetCandidateNode) ? &styleSheetCandidateNode->treeScope() : &m_document;
    if (treeScope == &m_document)
        m_needsDocumentStyleSheetsUpdate = true;
    else
        m_dirtyTreeScopes.add(treeScope);

    if (m_pendingStylesheets)
        return;

    if (notification == RemovePendingSheetNotifyLater) {
        m_document.setNeedsNotifyRemoveAllPendingStylesheet();
        return;
    }

    // FIXME: We can't call addedStyleSheet or removedStyleSheet here because we don't know
    // what's new. We should track that to tell the style system what changed.
    m_document.didRemoveAllPendingStylesheet();
}

void StyleEngine::addStyleSheetCandidateNode(Node* node, bool createdByParser)
{
    if (!node->inDocument())
        return;

    TreeScope& treeScope = isHTMLStyleElement(node) ? node->treeScope() : m_document;
    ASSERT(isHTMLStyleElement(node) || &treeScope == &m_document);

    StyleSheetCollection* collection = ensureStyleSheetCollectionFor(treeScope);
    ASSERT(collection);
    collection->addStyleSheetCandidateNode(node, createdByParser);

    if (&treeScope == &m_document) {
        m_needsDocumentStyleSheetsUpdate = true;
        return;
    }

    insertTreeScopeInDocumentOrder(m_activeTreeScopes, &treeScope);
    m_dirtyTreeScopes.add(&treeScope);
}

void StyleEngine::removeStyleSheetCandidateNode(Node* node, ContainerNode* scopingNode)
{
    TreeScope& treeScope = scopingNode ? scopingNode->treeScope() : m_document;
    ASSERT(isHTMLStyleElement(node) || &treeScope == &m_document);

    StyleSheetCollection* collection = styleSheetCollectionFor(treeScope);
    ASSERT(collection);
    collection->removeStyleSheetCandidateNode(node, scopingNode);

    if (&treeScope == &m_document) {
        m_needsDocumentStyleSheetsUpdate = true;
        return;
    }
    m_dirtyTreeScopes.add(&treeScope);
    m_activeTreeScopes.remove(&treeScope);
}

void StyleEngine::modifiedStyleSheetCandidateNode(Node* node)
{
    if (!node->inDocument())
        return;

    TreeScope& treeScope = isHTMLStyleElement(node) ? node->treeScope() : m_document;
    ASSERT(isHTMLStyleElement(node) || &treeScope == &m_document);
    if (&treeScope == &m_document) {
        m_needsDocumentStyleSheetsUpdate = true;
        return;
    }
    m_dirtyTreeScopes.add(&treeScope);
}

bool StyleEngine::shouldUpdateShadowTreeStyleSheetCollection(StyleResolverUpdateMode updateMode)
{
    return !m_dirtyTreeScopes.isEmpty() || updateMode == FullStyleUpdate;
}

bool StyleEngine::updateActiveStyleSheets(StyleResolverUpdateMode updateMode)
{
    if (m_document.inStyleRecalc()) {
        // SVG <use> element may manage to invalidate style selector in the middle of a style recalc.
        // https://bugs.webkit.org/show_bug.cgi?id=54344
        // FIXME: This should be fixed in SVG and the call site replaced by ASSERT(!m_inStyleRecalc).
        m_needsUpdateActiveStylesheetsOnStyleRecalc = true;
        return false;

    }
    if (!m_document.renderer() || !m_document.confusingAndOftenMisusedAttached())
        return false;

    bool requiresFullStyleRecalc = false;
    if (m_needsDocumentStyleSheetsUpdate || updateMode == FullStyleUpdate)
        requiresFullStyleRecalc = m_documentStyleSheetCollection.updateActiveStyleSheets(this, updateMode);

    if (shouldUpdateShadowTreeStyleSheetCollection(updateMode)) {
        TreeScopeSet treeScopes = updateMode == FullStyleUpdate ? m_activeTreeScopes : m_dirtyTreeScopes;
        HashSet<TreeScope*> treeScopesRemoved;

        for (TreeScopeSet::iterator it = treeScopes.begin(); it != treeScopes.end(); ++it) {
            TreeScope* treeScope = *it;
            ASSERT(treeScope != &m_document);
            ShadowTreeStyleSheetCollection* collection = static_cast<ShadowTreeStyleSheetCollection*>(styleSheetCollectionFor(*treeScope));
            ASSERT(collection);
            collection->updateActiveStyleSheets(this, updateMode);
            if (!collection->hasStyleSheetCandidateNodes())
                treeScopesRemoved.add(treeScope);
        }
        if (!treeScopesRemoved.isEmpty())
            for (HashSet<TreeScope*>::iterator it = treeScopesRemoved.begin(); it != treeScopesRemoved.end(); ++it)
                m_activeTreeScopes.remove(*it);
        m_dirtyTreeScopes.clear();
    }

    if (StyleResolver* styleResolver = m_document.styleResolverIfExists()) {
        styleResolver->finishAppendAuthorStyleSheets();
        resetCSSFeatureFlags(styleResolver->ruleFeatureSet());
    }

    m_needsUpdateActiveStylesheetsOnStyleRecalc = false;
    activeStyleSheetsUpdatedForInspector();
    m_usesRemUnits = m_documentStyleSheetCollection.usesRemUnits();

    if (m_needsDocumentStyleSheetsUpdate || updateMode == FullStyleUpdate) {
        m_document.notifySeamlessChildDocumentsOfStylesheetUpdate();
        m_needsDocumentStyleSheetsUpdate = false;
    }

    return requiresFullStyleRecalc;
}

void StyleEngine::activeStyleSheetsUpdatedForInspector()
{
    if (m_activeTreeScopes.isEmpty()) {
        InspectorInstrumentation::activeStyleSheetsUpdated(&m_document, m_documentStyleSheetCollection.styleSheetsForStyleSheetList());
        return;
    }
    Vector<RefPtr<StyleSheet> > activeStyleSheets;

    activeStyleSheets.append(m_documentStyleSheetCollection.styleSheetsForStyleSheetList());

    TreeScopeSet::iterator begin = m_activeTreeScopes.begin();
    TreeScopeSet::iterator end = m_activeTreeScopes.end();
    for (TreeScopeSet::iterator it = begin; it != end; ++it) {
        if (StyleSheetCollection* collection = m_styleSheetCollectionMap.get(*it))
            activeStyleSheets.append(collection->styleSheetsForStyleSheetList());
    }

    // FIXME: Inspector needs a vector which has all active stylesheets.
    // However, creating such a large vector might cause performance regression.
    // Need to implement some smarter solution.
    InspectorInstrumentation::activeStyleSheetsUpdated(&m_document, activeStyleSheets);
}

void StyleEngine::didRemoveShadowRoot(ShadowRoot* shadowRoot)
{
    m_styleSheetCollectionMap.remove(shadowRoot);
}

void StyleEngine::appendActiveAuthorStyleSheets(StyleResolver* styleResolver)
{
    ASSERT(styleResolver);

    styleResolver->setBuildScopedStyleTreeInDocumentOrder(true);
    styleResolver->appendAuthorStyleSheets(0, m_documentStyleSheetCollection.activeAuthorStyleSheets());

    TreeScopeSet::iterator begin = m_activeTreeScopes.begin();
    TreeScopeSet::iterator end = m_activeTreeScopes.end();
    for (TreeScopeSet::iterator it = begin; it != end; ++it) {
        if (StyleSheetCollection* collection = m_styleSheetCollectionMap.get(*it)) {
            styleResolver->setBuildScopedStyleTreeInDocumentOrder(!collection->scopingNodesForStyleScoped());
            styleResolver->appendAuthorStyleSheets(0, collection->activeAuthorStyleSheets());
        }
    }
    styleResolver->finishAppendAuthorStyleSheets();
    styleResolver->setBuildScopedStyleTreeInDocumentOrder(false);
}

}
