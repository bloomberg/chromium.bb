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
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/FontFaceCache.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/DocumentStyleSheetCollector.h"
#include "core/dom/Element.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/ShadowTreeStyleSheetCollection.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/imports/HTMLImport.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/InjectedStyleSheets.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "core/svg/SVGStyleElement.h"
#include "platform/URLPatternMatcher.h"

namespace WebCore {

using namespace HTMLNames;

StyleEngine::StyleEngine(Document& document)
    : m_document(document)
    , m_isMaster(HTMLImport::isMaster(&document))
    , m_pendingStylesheets(0)
    , m_injectedStyleSheetCacheValid(false)
    , m_documentStyleSheetCollection(document)
    , m_documentScopeDirty(true)
    , m_usesSiblingRules(false)
    , m_usesSiblingRulesOverride(false)
    , m_usesFirstLineRules(false)
    , m_usesFirstLetterRules(false)
    , m_usesRemUnits(false)
    , m_maxDirectAdjacentSelectors(0)
    , m_ignorePendingStylesheets(false)
    , m_didCalculateResolver(false)
    // We don't need to create CSSFontSelector for imported document or
    // HTMLTemplateElement's document, because those documents have no frame.
    , m_fontSelector(document.frame() ? CSSFontSelector::create(&document) : nullptr)
{
}

StyleEngine::~StyleEngine()
{
}

void StyleEngine::detachFromDocument()
{
    // Cleanup is performed eagerly when the StyleEngine is removed from the
    // document. The StyleEngine is unreachable after this, since only the
    // document has a reference to it.
    for (unsigned i = 0; i < m_injectedAuthorStyleSheets.size(); ++i)
        m_injectedAuthorStyleSheets[i]->clearOwnerNode();
    for (unsigned i = 0; i < m_authorStyleSheets.size(); ++i)
        m_authorStyleSheets[i]->clearOwnerNode();

    if (m_fontSelector) {
        m_fontSelector->clearDocument();
#if !ENABLE(OILPAN)
        if (m_resolver)
            m_fontSelector->unregisterForInvalidationCallbacks(m_resolver.get());
#endif
    }

    // Decrement reference counts for things we could be keeping alive.
    m_fontSelector.clear();
    m_resolver.clear();
    m_styleSheetCollectionMap.clear();
}

inline Document* StyleEngine::master()
{
    if (isMaster())
        return &m_document;
    HTMLImport* import = m_document.import();
    if (!import) // Document::import() can return null while executing its destructor.
        return 0;
    return import->master();
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

TreeScopeStyleSheetCollection* StyleEngine::ensureStyleSheetCollectionFor(TreeScope& treeScope)
{
    if (treeScope == m_document)
        return &m_documentStyleSheetCollection;

    WillBeHeapHashMap<TreeScope*, OwnPtrWillBeMember<ShadowTreeStyleSheetCollection> >::AddResult result = m_styleSheetCollectionMap.add(&treeScope, nullptr);
    if (result.isNewEntry)
        result.storedValue->value = adoptPtrWillBeNoop(new ShadowTreeStyleSheetCollection(toShadowRoot(treeScope)));
    return result.storedValue->value.get();
}

TreeScopeStyleSheetCollection* StyleEngine::styleSheetCollectionFor(TreeScope& treeScope)
{
    if (treeScope == m_document)
        return &m_documentStyleSheetCollection;

    WillBeHeapHashMap<TreeScope*, OwnPtrWillBeMember<ShadowTreeStyleSheetCollection> >::iterator it = m_styleSheetCollectionMap.find(&treeScope);
    if (it == m_styleSheetCollectionMap.end())
        return 0;
    return it->value.get();
}

const WillBeHeapVector<RefPtrWillBeMember<StyleSheet> >& StyleEngine::styleSheetsForStyleSheetList(TreeScope& treeScope)
{
    if (treeScope == m_document)
        return m_documentStyleSheetCollection.styleSheetsForStyleSheetList();

    return ensureStyleSheetCollectionFor(treeScope)->styleSheetsForStyleSheetList();
}

const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >& StyleEngine::activeAuthorStyleSheets() const
{
    return m_documentStyleSheetCollection.activeAuthorStyleSheets();
}

void StyleEngine::combineCSSFeatureFlags(const RuleFeatureSet& features)
{
    // Delay resetting the flags until after next style recalc since unapplying the style may not work without these set (this is true at least with before/after).
    m_usesSiblingRules = m_usesSiblingRules || features.usesSiblingRules();
    m_usesFirstLineRules = m_usesFirstLineRules || features.usesFirstLineRules();
    m_maxDirectAdjacentSelectors = max(m_maxDirectAdjacentSelectors, features.maxDirectAdjacentSelectors());
}

void StyleEngine::resetCSSFeatureFlags(const RuleFeatureSet& features)
{
    m_usesSiblingRules = features.usesSiblingRules();
    m_usesFirstLineRules = features.usesFirstLineRules();
    m_maxDirectAdjacentSelectors = features.maxDirectAdjacentSelectors();
}

const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >& StyleEngine::injectedAuthorStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedAuthorStyleSheets;
}

void StyleEngine::updateInjectedStyleSheetCache() const
{
    if (m_injectedStyleSheetCacheValid)
        return;
    m_injectedStyleSheetCacheValid = true;
    m_injectedAuthorStyleSheets.clear();

    Page* owningPage = m_document.page();
    if (!owningPage)
        return;

    const InjectedStyleSheetEntryVector& entries = InjectedStyleSheets::instance().entries();
    for (unsigned i = 0; i < entries.size(); ++i) {
        const InjectedStyleSheetEntry* entry = entries[i].get();
        if (entry->injectedFrames() == InjectStyleInTopFrameOnly && m_document.ownerElement())
            continue;
        if (!URLPatternMatcher::matchesPatterns(m_document.url(), entry->whitelist()))
            continue;
        RefPtrWillBeRawPtr<CSSStyleSheet> groupSheet = CSSStyleSheet::createInline(const_cast<Document*>(&m_document), KURL());
        m_injectedAuthorStyleSheets.append(groupSheet);
        groupSheet->contents()->parseString(entry->source());
    }
}

void StyleEngine::invalidateInjectedStyleSheetCache()
{
    m_injectedStyleSheetCacheValid = false;
    markDocumentDirty();
    // FIXME: updateInjectedStyleSheetCache is called inside StyleSheetCollection::updateActiveStyleSheets
    // and batch updates lots of sheets so we can't call addedStyleSheet() or removedStyleSheet().
    m_document.styleResolverChanged(RecalcStyleDeferred);
}

void StyleEngine::addAuthorSheet(PassRefPtrWillBeRawPtr<StyleSheetContents> authorSheet)
{
    m_authorStyleSheets.append(CSSStyleSheet::create(authorSheet, &m_document));
    m_document.addedStyleSheet(m_authorStyleSheets.last().get(), RecalcStyleImmediately);
    markDocumentDirty();
}

void StyleEngine::addPendingSheet()
{
    m_pendingStylesheets++;
}

// This method is called whenever a top-level stylesheet has finished loading.
void StyleEngine::removePendingSheet(Node* styleSheetCandidateNode, RemovePendingSheetNotificationType notification)
{
    ASSERT(styleSheetCandidateNode);
    TreeScope* treeScope = isHTMLStyleElement(*styleSheetCandidateNode) ? &styleSheetCandidateNode->treeScope() : &m_document;
    markTreeScopeDirty(*treeScope);

    // Make sure we knew this sheet was pending, and that our count isn't out of sync.
    ASSERT(m_pendingStylesheets > 0);

    m_pendingStylesheets--;
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

void StyleEngine::modifiedStyleSheet(StyleSheet* sheet)
{
    if (!sheet)
        return;

    Node* node = sheet->ownerNode();
    if (!node || !node->inDocument())
        return;

    TreeScope& treeScope = isHTMLStyleElement(*node) ? node->treeScope() : m_document;
    ASSERT(isHTMLStyleElement(node) || treeScope == m_document);

    markTreeScopeDirty(treeScope);
}

void StyleEngine::addStyleSheetCandidateNode(Node* node, bool createdByParser)
{
    if (!node->inDocument())
        return;

    TreeScope& treeScope = isHTMLStyleElement(*node) ? node->treeScope() : m_document;
    ASSERT(isHTMLStyleElement(node) || treeScope == m_document);

    TreeScopeStyleSheetCollection* collection = ensureStyleSheetCollectionFor(treeScope);
    ASSERT(collection);
    collection->addStyleSheetCandidateNode(node, createdByParser);

    markTreeScopeDirty(treeScope);
    if (treeScope != m_document)
        insertTreeScopeInDocumentOrder(m_activeTreeScopes, &treeScope);
}

void StyleEngine::removeStyleSheetCandidateNode(Node* node)
{
    removeStyleSheetCandidateNode(node, 0, m_document);
}

void StyleEngine::removeStyleSheetCandidateNode(Node* node, ContainerNode* scopingNode, TreeScope& treeScope)
{
    ASSERT(isHTMLStyleElement(node) || treeScope == m_document);

    TreeScopeStyleSheetCollection* collection = styleSheetCollectionFor(treeScope);
    ASSERT(collection);
    collection->removeStyleSheetCandidateNode(node, scopingNode);

    markTreeScopeDirty(treeScope);
    m_activeTreeScopes.remove(&treeScope);
}

void StyleEngine::modifiedStyleSheetCandidateNode(Node* node)
{
    if (!node->inDocument())
        return;

    TreeScope& treeScope = isHTMLStyleElement(*node) ? node->treeScope() : m_document;
    ASSERT(isHTMLStyleElement(node) || treeScope == m_document);
    markTreeScopeDirty(treeScope);
}

bool StyleEngine::shouldUpdateShadowTreeStyleSheetCollection(StyleResolverUpdateMode updateMode)
{
    return !m_dirtyTreeScopes.isEmpty() || updateMode == FullStyleUpdate;
}

void StyleEngine::clearMediaQueryRuleSetOnTreeScopeStyleSheets(TreeScopeSet treeScopes)
{
    for (TreeScopeSet::iterator it = treeScopes.begin(); it != treeScopes.end(); ++it) {
        TreeScope& treeScope = **it;
        ASSERT(treeScope != m_document);
        ShadowTreeStyleSheetCollection* collection = static_cast<ShadowTreeStyleSheetCollection*>(styleSheetCollectionFor(treeScope));
        ASSERT(collection);
        collection->clearMediaQueryRuleSetStyleSheets();
    }
}

void StyleEngine::clearMediaQueryRuleSetStyleSheets()
{
    m_documentStyleSheetCollection.clearMediaQueryRuleSetStyleSheets();
    clearMediaQueryRuleSetOnTreeScopeStyleSheets(m_activeTreeScopes);
    clearMediaQueryRuleSetOnTreeScopeStyleSheets(m_dirtyTreeScopes);
}

void StyleEngine::updateStyleSheetsInImport(DocumentStyleSheetCollector& parentCollector)
{
    ASSERT(!isMaster());
    WillBeHeapVector<RefPtrWillBeMember<StyleSheet> > sheetsForList;
    ImportedDocumentStyleSheetCollector subcollector(parentCollector, sheetsForList);
    m_documentStyleSheetCollection.collectStyleSheets(this, subcollector);
    m_documentStyleSheetCollection.swapSheetsForSheetList(sheetsForList);
}

bool StyleEngine::updateActiveStyleSheets(StyleResolverUpdateMode updateMode)
{
    ASSERT(isMaster());
    ASSERT(!m_document.inStyleRecalc());

    if (!m_document.isActive())
        return false;

    bool requiresFullStyleRecalc = false;
    if (m_documentScopeDirty || updateMode == FullStyleUpdate)
        requiresFullStyleRecalc = m_documentStyleSheetCollection.updateActiveStyleSheets(this, updateMode);

    if (shouldUpdateShadowTreeStyleSheetCollection(updateMode)) {
        TreeScopeSet treeScopes = updateMode == FullStyleUpdate ? m_activeTreeScopes : m_dirtyTreeScopes;
        HashSet<TreeScope*> treeScopesRemoved;

        for (TreeScopeSet::iterator it = treeScopes.begin(); it != treeScopes.end(); ++it) {
            TreeScope* treeScope = *it;
            ASSERT(treeScope != m_document);
            ShadowTreeStyleSheetCollection* collection = static_cast<ShadowTreeStyleSheetCollection*>(styleSheetCollectionFor(*treeScope));
            ASSERT(collection);
            collection->updateActiveStyleSheets(this, updateMode);
            if (!collection->hasStyleSheetCandidateNodes())
                treeScopesRemoved.add(treeScope);
        }
        if (!treeScopesRemoved.isEmpty())
            for (HashSet<TreeScope*>::iterator it = treeScopesRemoved.begin(); it != treeScopesRemoved.end(); ++it)
                m_activeTreeScopes.remove(*it);
    }

    InspectorInstrumentation::activeStyleSheetsUpdated(&m_document);
    m_usesRemUnits = m_documentStyleSheetCollection.usesRemUnits();

    m_dirtyTreeScopes.clear();
    m_documentScopeDirty = false;

    return requiresFullStyleRecalc;
}

const WillBeHeapVector<RefPtrWillBeMember<StyleSheet> > StyleEngine::activeStyleSheetsForInspector() const
{
    if (m_activeTreeScopes.isEmpty())
        return m_documentStyleSheetCollection.styleSheetsForStyleSheetList();

    WillBeHeapVector<RefPtrWillBeMember<StyleSheet> > activeStyleSheets;

    activeStyleSheets.appendVector(m_documentStyleSheetCollection.styleSheetsForStyleSheetList());

    TreeScopeSet::const_iterator begin = m_activeTreeScopes.begin();
    TreeScopeSet::const_iterator end = m_activeTreeScopes.end();
    for (TreeScopeSet::const_iterator it = begin; it != end; ++it) {
        if (TreeScopeStyleSheetCollection* collection = m_styleSheetCollectionMap.get(*it))
            activeStyleSheets.appendVector(collection->styleSheetsForStyleSheetList());
    }

    // FIXME: Inspector needs a vector which has all active stylesheets.
    // However, creating such a large vector might cause performance regression.
    // Need to implement some smarter solution.
    return activeStyleSheets;
}

void StyleEngine::didRemoveShadowRoot(ShadowRoot* shadowRoot)
{
    m_styleSheetCollectionMap.remove(shadowRoot);
}

void StyleEngine::appendActiveAuthorStyleSheets()
{
    ASSERT(isMaster());

    m_resolver->setBuildScopedStyleTreeInDocumentOrder(true);
    m_resolver->appendAuthorStyleSheets(0, m_documentStyleSheetCollection.activeAuthorStyleSheets());

    TreeScopeSet::iterator begin = m_activeTreeScopes.begin();
    TreeScopeSet::iterator end = m_activeTreeScopes.end();
    for (TreeScopeSet::iterator it = begin; it != end; ++it) {
        if (TreeScopeStyleSheetCollection* collection = m_styleSheetCollectionMap.get(*it)) {
            m_resolver->setBuildScopedStyleTreeInDocumentOrder(!collection->scopingNodesForStyleScoped());
            m_resolver->appendAuthorStyleSheets(0, collection->activeAuthorStyleSheets());
        }
    }
    m_resolver->finishAppendAuthorStyleSheets();
    m_resolver->setBuildScopedStyleTreeInDocumentOrder(false);
}

void StyleEngine::createResolver()
{
    // It is a programming error to attempt to resolve style on a Document
    // which is not in a frame. Code which hits this should have checked
    // Document::isActive() before calling into code which could get here.

    ASSERT(m_document.frame());
    ASSERT(m_fontSelector);

    m_resolver = adoptPtrWillBeNoop(new StyleResolver(m_document));
    appendActiveAuthorStyleSheets();
    m_fontSelector->registerForInvalidationCallbacks(m_resolver.get());
    combineCSSFeatureFlags(m_resolver->ensureUpdatedRuleFeatureSet());
}

void StyleEngine::clearResolver()
{
    ASSERT(!m_document.inStyleRecalc());
    ASSERT(isMaster() || !m_resolver);
    ASSERT(m_fontSelector || !m_resolver);
    if (m_resolver) {
        m_document.updateStyleInvalidationIfNeeded();
#if !ENABLE(OILPAN)
        m_fontSelector->unregisterForInvalidationCallbacks(m_resolver.get());
#endif
    }
    m_resolver.clear();
}

void StyleEngine::clearMasterResolver()
{
    if (Document* master = this->master())
        master->styleEngine()->clearResolver();
}

unsigned StyleEngine::resolverAccessCount() const
{
    return m_resolver ? m_resolver->accessCount() : 0;
}

void StyleEngine::didDetach()
{
    clearResolver();
}

bool StyleEngine::shouldClearResolver() const
{
    return !m_didCalculateResolver && !haveStylesheetsLoaded();
}

StyleResolverChange StyleEngine::resolverChanged(RecalcStyleTime time, StyleResolverUpdateMode mode)
{
    StyleResolverChange change;

    if (!isMaster()) {
        if (Document* master = this->master())
            master->styleResolverChanged(time, mode);
        return change;
    }

    // Don't bother updating, since we haven't loaded all our style info yet
    // and haven't calculated the style selector for the first time.
    if (!m_document.isActive() || shouldClearResolver()) {
        clearResolver();
        return change;
    }

    m_didCalculateResolver = true;
    if (m_document.didLayoutWithPendingStylesheets() && !hasPendingSheets())
        change.setNeedsRepaint();

    if (updateActiveStyleSheets(mode))
        change.setNeedsStyleRecalc();

    return change;
}

void StyleEngine::clearFontCache()
{
    // We should not recreate FontSelector. Instead, clear fontFaceCache.
    if (m_fontSelector)
        m_fontSelector->fontFaceCache()->clear();
    if (m_resolver)
        m_resolver->invalidateMatchedPropertiesCache();
}

void StyleEngine::updateGenericFontFamilySettings()
{
    if (!m_fontSelector)
        return;

    m_fontSelector->updateGenericFontFamilySettings(m_document);
    if (m_resolver)
        m_resolver->invalidateMatchedPropertiesCache();
}

void StyleEngine::removeFontFaceRules(const WillBeHeapVector<RawPtrWillBeMember<const StyleRuleFontFace> >& fontFaceRules)
{
    if (!m_fontSelector)
        return;

    FontFaceCache* cache = m_fontSelector->fontFaceCache();
    for (unsigned i = 0; i < fontFaceRules.size(); ++i)
        cache->remove(fontFaceRules[i]);
    if (m_resolver)
        m_resolver->invalidateMatchedPropertiesCache();
}

void StyleEngine::markTreeScopeDirty(TreeScope& scope)
{
    if (scope == m_document) {
        markDocumentDirty();
        return;
    }

    m_dirtyTreeScopes.add(&scope);
}

void StyleEngine::markDocumentDirty()
{
    m_documentScopeDirty = true;
    if (!HTMLImport::isMaster(&m_document))
        m_document.import()->master()->styleEngine()->markDocumentDirty();
}

static bool isCacheableForStyleElement(const StyleSheetContents& contents)
{
    // FIXME: Support copying import rules.
    if (!contents.importRules().isEmpty())
        return false;
    // Until import rules are supported in cached sheets it's not possible for loading to fail.
    ASSERT(!contents.didLoadErrorOccur());
    // It is not the original sheet anymore.
    if (contents.isMutable())
        return false;
    if (!contents.hasSyntacticallyValidCSSHeader())
        return false;
    return true;
}

PassRefPtrWillBeRawPtr<CSSStyleSheet> StyleEngine::createSheet(Element* e, const String& text, TextPosition startPosition, bool createdByParser)
{
    RefPtrWillBeRawPtr<CSSStyleSheet> styleSheet = nullptr;

    e->document().styleEngine()->addPendingSheet();

    if (!e->document().inQuirksMode()) {
        AtomicString textContent(text);

        WillBeHeapHashMap<AtomicString, RawPtrWillBeMember<StyleSheetContents> >::AddResult result = m_textToSheetCache.add(textContent, nullptr);
        if (result.isNewEntry || !result.storedValue->value) {
            styleSheet = StyleEngine::parseSheet(e, text, startPosition, createdByParser);
            if (result.isNewEntry && isCacheableForStyleElement(*styleSheet->contents())) {
                result.storedValue->value = styleSheet->contents();
                m_sheetToTextCache.add(styleSheet->contents(), textContent);
            }
        } else {
            StyleSheetContents* contents = result.storedValue->value;
            ASSERT(contents);
            ASSERT(isCacheableForStyleElement(*contents));
            ASSERT(contents->singleOwnerDocument() == e->document());
            styleSheet = CSSStyleSheet::createInline(contents, e, startPosition);
        }
    } else {
        // FIXME: currently we don't cache StyleSheetContents inQuirksMode.
        styleSheet = StyleEngine::parseSheet(e, text, startPosition, createdByParser);
    }

    ASSERT(styleSheet);
    styleSheet->setTitle(e->title());
    return styleSheet;
}

PassRefPtrWillBeRawPtr<CSSStyleSheet> StyleEngine::parseSheet(Element* e, const String& text, TextPosition startPosition, bool createdByParser)
{
    RefPtrWillBeRawPtr<CSSStyleSheet> styleSheet = nullptr;
    styleSheet = CSSStyleSheet::createInline(e, KURL(), startPosition, e->document().inputEncoding());
    styleSheet->contents()->parseStringAtPosition(text, startPosition, createdByParser);
    return styleSheet;
}

void StyleEngine::removeSheet(StyleSheetContents* contents)
{
    WillBeHeapHashMap<RawPtrWillBeMember<StyleSheetContents>, AtomicString>::iterator it = m_sheetToTextCache.find(contents);
    if (it == m_sheetToTextCache.end())
        return;

    m_textToSheetCache.remove(it->value);
    m_sheetToTextCache.remove(contents);
}

void StyleEngine::trace(Visitor* visitor)
{
    visitor->trace(m_injectedAuthorStyleSheets);
    visitor->trace(m_authorStyleSheets);
    visitor->trace(m_documentStyleSheetCollection);
    visitor->trace(m_styleSheetCollectionMap);
    visitor->trace(m_resolver);
    visitor->trace(m_textToSheetCache);
    visitor->trace(m_sheetToTextCache);
}

}
