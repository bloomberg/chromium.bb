/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

#include "core/dom/StyleEngine.h"

#include "core/HTMLNames.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/FontFaceCache.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/invalidation/InvalidationSet.h"
#include "core/css/resolver/ScopedStyleResolver.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/DocumentStyleSheetCollector.h"
#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/ShadowTreeStyleSheetCollection.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/Page.h"
#include "core/svg/SVGStyleElement.h"
#include "platform/fonts/FontCache.h"
#include "platform/tracing/TraceEvent.h"

namespace blink {

using namespace HTMLNames;

StyleEngine::StyleEngine(Document& document)
    : m_document(&document),
      m_isMaster(!document.importsController() ||
                 document.importsController()->master() == &document),
      m_documentStyleSheetCollection(
          DocumentStyleSheetCollection::create(document)) {
  if (document.frame()) {
    // We don't need to create CSSFontSelector for imported document or
    // HTMLTemplateElement's document, because those documents have no frame.
    m_fontSelector = CSSFontSelector::create(&document);
    m_fontSelector->registerForInvalidationCallbacks(this);
  }
  if (document.isInMainFrame())
    m_viewportResolver = ViewportStyleResolver::create(document);
}

StyleEngine::~StyleEngine() {}

static bool isStyleElement(Node& node) {
  return isHTMLStyleElement(node) || isSVGStyleElement(node);
}

inline Document* StyleEngine::master() {
  if (isMaster())
    return m_document;
  HTMLImportsController* import = document().importsController();
  // Document::import() can return null while executing its destructor.
  if (!import)
    return nullptr;
  return import->master();
}

TreeScopeStyleSheetCollection* StyleEngine::ensureStyleSheetCollectionFor(
    TreeScope& treeScope) {
  if (treeScope == m_document)
    return &documentStyleSheetCollection();

  StyleSheetCollectionMap::AddResult result =
      m_styleSheetCollectionMap.add(&treeScope, nullptr);
  if (result.isNewEntry)
    result.storedValue->value =
        new ShadowTreeStyleSheetCollection(toShadowRoot(treeScope));
  return result.storedValue->value.get();
}

TreeScopeStyleSheetCollection* StyleEngine::styleSheetCollectionFor(
    TreeScope& treeScope) {
  if (treeScope == m_document)
    return &documentStyleSheetCollection();

  StyleSheetCollectionMap::iterator it =
      m_styleSheetCollectionMap.find(&treeScope);
  if (it == m_styleSheetCollectionMap.end())
    return nullptr;
  return it->value.get();
}

const HeapVector<TraceWrapperMember<StyleSheet>>&
StyleEngine::styleSheetsForStyleSheetList(TreeScope& treeScope) {
  if (treeScope == m_document)
    return documentStyleSheetCollection().styleSheetsForStyleSheetList();

  return ensureStyleSheetCollectionFor(treeScope)
      ->styleSheetsForStyleSheetList();
}

void StyleEngine::resetCSSFeatureFlags(const RuleFeatureSet& features) {
  m_usesSiblingRules = features.usesSiblingRules();
  m_usesFirstLineRules = features.usesFirstLineRules();
  m_usesWindowInactiveSelector = features.usesWindowInactiveSelector();
  m_maxDirectAdjacentSelectors = features.maxDirectAdjacentSelectors();
}

void StyleEngine::injectAuthorSheet(StyleSheetContents* authorSheet) {
  m_injectedAuthorStyleSheets.append(
      CSSStyleSheet::create(authorSheet, *m_document));
  markDocumentDirty();
  resolverChanged(AnalyzedStyleUpdate);
}

CSSStyleSheet& StyleEngine::ensureInspectorStyleSheet() {
  if (m_inspectorStyleSheet)
    return *m_inspectorStyleSheet;

  StyleSheetContents* contents =
      StyleSheetContents::create(CSSParserContext(*m_document, nullptr));
  m_inspectorStyleSheet = CSSStyleSheet::create(contents, *m_document);
  markDocumentDirty();
  resolverChanged(AnalyzedStyleUpdate);
  return *m_inspectorStyleSheet;
}

void StyleEngine::addPendingSheet(StyleEngineContext& context) {
  m_pendingScriptBlockingStylesheets++;

  context.addingPendingSheet(document());
  if (context.addedPendingSheetBeforeBody())
    m_pendingRenderBlockingStylesheets++;
}

// This method is called whenever a top-level stylesheet has finished loading.
void StyleEngine::removePendingSheet(Node& styleSheetCandidateNode,
                                     const StyleEngineContext& context) {
  TreeScope* treeScope = isStyleElement(styleSheetCandidateNode)
                             ? &styleSheetCandidateNode.treeScope()
                             : m_document.get();
  if (styleSheetCandidateNode.isConnected())
    markTreeScopeDirty(*treeScope);

  if (context.addedPendingSheetBeforeBody()) {
    DCHECK_GT(m_pendingRenderBlockingStylesheets, 0);
    m_pendingRenderBlockingStylesheets--;
  }

  // Make sure we knew this sheet was pending, and that our count isn't out of
  // sync.
  DCHECK_GT(m_pendingScriptBlockingStylesheets, 0);

  m_pendingScriptBlockingStylesheets--;
  if (m_pendingScriptBlockingStylesheets)
    return;

  document().didRemoveAllPendingStylesheet();
}

void StyleEngine::setNeedsActiveStyleUpdate(
    StyleSheet* sheet,
    StyleResolverUpdateMode updateMode) {
  // resolverChanged() is called for inactive non-master documents because
  // import documents are inactive documents. resolverChanged() for imports
  // will call resolverChanged() for the master document and update the active
  // stylesheets including the ones from the import.
  if (!document().isActive() && isMaster())
    return;

  if (sheet && document().isActive()) {
    Node* node = sheet->ownerNode();
    if (node && node->isConnected()) {
      TreeScope& treeScope =
          isStyleElement(*node) ? node->treeScope() : *m_document;
      DCHECK(isStyleElement(*node) || node->treeScope() == m_document);
      markTreeScopeDirty(treeScope);
    }
  }

  resolverChanged(updateMode);
}

void StyleEngine::addStyleSheetCandidateNode(Node& node) {
  if (!node.isConnected() || document().isDetached())
    return;

  DCHECK(!isXSLStyleSheet(node));
  TreeScope& treeScope = node.treeScope();
  TreeScopeStyleSheetCollection* collection =
      ensureStyleSheetCollectionFor(treeScope);
  DCHECK(collection);
  collection->addStyleSheetCandidateNode(node);

  markTreeScopeDirty(treeScope);
  if (treeScope != m_document)
    m_activeTreeScopes.add(&treeScope);
}

void StyleEngine::removeStyleSheetCandidateNode(Node& node) {
  removeStyleSheetCandidateNode(node, *m_document);
}

void StyleEngine::removeStyleSheetCandidateNode(Node& node,
                                                TreeScope& treeScope) {
  DCHECK(isStyleElement(node) || treeScope == m_document);
  DCHECK(!isXSLStyleSheet(node));

  TreeScopeStyleSheetCollection* collection =
      styleSheetCollectionFor(treeScope);
  // After detaching document, collection could be null. In the case,
  // we should not update anything. Instead, just return.
  if (!collection)
    return;
  collection->removeStyleSheetCandidateNode(node);

  markTreeScopeDirty(treeScope);
}

void StyleEngine::modifiedStyleSheetCandidateNode(Node& node) {
  if (!node.isConnected())
    return;

  TreeScope& treeScope = isStyleElement(node) ? node.treeScope() : *m_document;
  DCHECK(isStyleElement(node) || treeScope == m_document);
  markTreeScopeDirty(treeScope);
  resolverChanged(AnalyzedStyleUpdate);
}

void StyleEngine::watchedSelectorsChanged() {
  if (m_resolver) {
    m_resolver->initWatchedSelectorRules();
    m_resolver->resetRuleFeatures();
  }
  document().setNeedsStyleRecalc(SubtreeStyleChange,
                                 StyleChangeReasonForTracing::create(
                                     StyleChangeReason::DeclarativeContent));
}

bool StyleEngine::shouldUpdateDocumentStyleSheetCollection(
    StyleResolverUpdateMode updateMode) const {
  return m_documentScopeDirty || updateMode == FullStyleUpdate;
}

bool StyleEngine::shouldUpdateShadowTreeStyleSheetCollection(
    StyleResolverUpdateMode updateMode) const {
  return !m_dirtyTreeScopes.isEmpty() || updateMode == FullStyleUpdate;
}

void StyleEngine::clearMediaQueryRuleSetOnTreeScopeStyleSheets(
    UnorderedTreeScopeSet& treeScopes) {
  for (TreeScope* treeScope : treeScopes) {
    DCHECK(treeScope != m_document);
    ShadowTreeStyleSheetCollection* collection =
        toShadowTreeStyleSheetCollection(styleSheetCollectionFor(*treeScope));
    DCHECK(collection);
    collection->clearMediaQueryRuleSetStyleSheets();
  }
}

void StyleEngine::clearMediaQueryRuleSetStyleSheets() {
  resolverChanged(FullStyleUpdate);
  documentStyleSheetCollection().clearMediaQueryRuleSetStyleSheets();
  clearMediaQueryRuleSetOnTreeScopeStyleSheets(m_activeTreeScopes);
}

void StyleEngine::updateStyleSheetsInImport(
    DocumentStyleSheetCollector& parentCollector) {
  DCHECK(!isMaster());
  HeapVector<Member<StyleSheet>> sheetsForList;
  ImportedDocumentStyleSheetCollector subcollector(parentCollector,
                                                   sheetsForList);
  documentStyleSheetCollection().collectStyleSheets(*this, subcollector);
  documentStyleSheetCollection().swapSheetsForSheetList(sheetsForList);
}

void StyleEngine::updateActiveStyleSheetsInShadow(
    StyleResolverUpdateMode updateMode,
    TreeScope* treeScope,
    UnorderedTreeScopeSet& treeScopesRemoved) {
  DCHECK_NE(treeScope, m_document);
  ShadowTreeStyleSheetCollection* collection =
      toShadowTreeStyleSheetCollection(styleSheetCollectionFor(*treeScope));
  DCHECK(collection);
  collection->updateActiveStyleSheets(*this, updateMode);
  if (!collection->hasStyleSheetCandidateNodes()) {
    treeScopesRemoved.add(treeScope);
    // When removing TreeScope from ActiveTreeScopes,
    // its resolver should be destroyed by invoking resetAuthorStyle.
    DCHECK(!treeScope->scopedStyleResolver());
  }
}

void StyleEngine::updateActiveStyleSheets(StyleResolverUpdateMode updateMode) {
  DCHECK(isMaster());
  DCHECK(!document().inStyleRecalc());

  if (!document().isActive())
    return;

  TRACE_EVENT0("blink,blink_style", "StyleEngine::updateActiveStyleSheets");

  if (shouldUpdateDocumentStyleSheetCollection(updateMode))
    documentStyleSheetCollection().updateActiveStyleSheets(*this, updateMode);

  if (shouldUpdateShadowTreeStyleSheetCollection(updateMode)) {
    UnorderedTreeScopeSet treeScopesRemoved;

    if (updateMode == FullStyleUpdate) {
      for (TreeScope* treeScope : m_activeTreeScopes)
        updateActiveStyleSheetsInShadow(updateMode, treeScope,
                                        treeScopesRemoved);
    } else {
      for (TreeScope* treeScope : m_dirtyTreeScopes)
        updateActiveStyleSheetsInShadow(updateMode, treeScope,
                                        treeScopesRemoved);
    }
    for (TreeScope* treeScope : treeScopesRemoved)
      m_activeTreeScopes.remove(treeScope);
  }

  InspectorInstrumentation::activeStyleSheetsUpdated(m_document);

  m_dirtyTreeScopes.clear();
  m_documentScopeDirty = false;
}

const HeapVector<Member<CSSStyleSheet>>
StyleEngine::activeStyleSheetsForInspector() const {
  if (m_activeTreeScopes.isEmpty())
    return documentStyleSheetCollection().activeAuthorStyleSheets();

  HeapVector<Member<CSSStyleSheet>> activeStyleSheets;

  activeStyleSheets.appendVector(
      documentStyleSheetCollection().activeAuthorStyleSheets());
  for (TreeScope* treeScope : m_activeTreeScopes) {
    if (TreeScopeStyleSheetCollection* collection =
            m_styleSheetCollectionMap.get(treeScope))
      activeStyleSheets.appendVector(collection->activeAuthorStyleSheets());
  }

  // FIXME: Inspector needs a vector which has all active stylesheets.
  // However, creating such a large vector might cause performance regression.
  // Need to implement some smarter solution.
  return activeStyleSheets;
}

void StyleEngine::didRemoveShadowRoot(ShadowRoot* shadowRoot) {
  m_styleSheetCollectionMap.remove(shadowRoot);
  m_activeTreeScopes.remove(shadowRoot);
  m_dirtyTreeScopes.remove(shadowRoot);
}

void StyleEngine::shadowRootRemovedFromDocument(ShadowRoot* shadowRoot) {
  if (StyleResolver* styleResolver = resolver()) {
    styleResolver->resetRuleFeatures();

    if (TreeScopeStyleSheetCollection* collection =
            styleSheetCollectionFor(*shadowRoot))
      styleResolver->removePendingAuthorStyleSheets(
          collection->activeAuthorStyleSheets());
  }
  shadowRoot->clearScopedStyleResolver();
  m_styleSheetCollectionMap.remove(shadowRoot);
  m_activeTreeScopes.remove(shadowRoot);
  m_dirtyTreeScopes.remove(shadowRoot);
  m_treeBoundaryCrossingScopes.remove(&shadowRoot->rootNode());
}

void StyleEngine::addTreeBoundaryCrossingScope(const TreeScope& treeScope) {
  m_treeBoundaryCrossingScopes.add(&treeScope.rootNode());
}

void StyleEngine::resetAuthorStyle(TreeScope& treeScope) {
  m_treeBoundaryCrossingScopes.remove(&treeScope.rootNode());

  ScopedStyleResolver* scopedResolver = treeScope.scopedStyleResolver();
  if (!scopedResolver)
    return;

  DCHECK(m_resolver);
  m_resolver->resetRuleFeatures();
  if (treeScope.rootNode().isDocumentNode()) {
    scopedResolver->resetAuthorStyle();
    return;
  }

  treeScope.clearScopedStyleResolver();
}

void StyleEngine::appendActiveAuthorStyleSheets() {
  DCHECK(isMaster());

  viewportRulesChanged();

  m_resolver->appendAuthorStyleSheets(
      documentStyleSheetCollection().activeAuthorStyleSheets());
  for (TreeScope* treeScope : m_activeTreeScopes) {
    if (TreeScopeStyleSheetCollection* collection =
            m_styleSheetCollectionMap.get(treeScope))
      m_resolver->appendAuthorStyleSheets(
          collection->activeAuthorStyleSheets());
  }
  m_resolver->finishAppendAuthorStyleSheets();
}

void StyleEngine::createResolver() {
  m_resolver = StyleResolver::create(*m_document);

  // A scoped style resolver for document will be created during
  // appendActiveAuthorStyleSheets if needed.
  appendActiveAuthorStyleSheets();
}

void StyleEngine::clearResolver() {
  DCHECK(!document().inStyleRecalc());
  DCHECK(isMaster() || !m_resolver);

  document().clearScopedStyleResolver();
  // StyleEngine::shadowRootRemovedFromDocument removes not-in-document
  // treescopes from activeTreeScopes. StyleEngine::didRemoveShadowRoot
  // removes treescopes which are being destroyed from activeTreeScopes.
  // So we need to clearScopedStyleResolver for treescopes which have been
  // just removed from document. If document is destroyed before invoking
  // updateActiveStyleSheets, the treescope has a scopedStyleResolver which
  // has destroyed StyleSheetContents.
  for (TreeScope* treeScope : m_activeTreeScopes)
    treeScope->clearScopedStyleResolver();

  m_treeBoundaryCrossingScopes.clear();

  if (m_resolver) {
    TRACE_EVENT1("blink", "StyleEngine::clearResolver", "frame",
                 document().frame());
    m_resolver->dispose();
    m_resolver.clear();
  }
}

void StyleEngine::clearMasterResolver() {
  if (Document* master = this->master())
    master->styleEngine().clearResolver();
}

void StyleEngine::didDetach() {
  clearResolver();
  m_viewportResolver.clear();
}

bool StyleEngine::shouldClearResolver() const {
  return !m_didCalculateResolver && !haveScriptBlockingStylesheetsLoaded();
}

void StyleEngine::resolverChanged(StyleResolverUpdateMode mode) {
  if (!isMaster()) {
    if (Document* master = this->master())
      master->styleEngine().resolverChanged(mode);
    return;
  }

  // Don't bother updating, since we haven't loaded all our style info yet
  // and haven't calculated the style selector for the first time.
  if (!document().isActive() || shouldClearResolver()) {
    clearResolver();
    return;
  }

  m_didCalculateResolver = true;
  updateActiveStyleSheets(mode);
}

void StyleEngine::clearFontCache() {
  if (m_fontSelector)
    m_fontSelector->fontFaceCache()->clearCSSConnected();
  if (m_resolver)
    m_resolver->invalidateMatchedPropertiesCache();
}

void StyleEngine::updateGenericFontFamilySettings() {
  // FIXME: we should not update generic font family settings when
  // document is inactive.
  DCHECK(document().isActive());

  if (!m_fontSelector)
    return;

  m_fontSelector->updateGenericFontFamilySettings(*m_document);
  if (m_resolver)
    m_resolver->invalidateMatchedPropertiesCache();
  FontCache::fontCache()->invalidateShapeCache();
}

void StyleEngine::removeFontFaceRules(
    const HeapVector<Member<const StyleRuleFontFace>>& fontFaceRules) {
  if (!m_fontSelector)
    return;

  FontFaceCache* cache = m_fontSelector->fontFaceCache();
  for (unsigned i = 0; i < fontFaceRules.size(); ++i)
    cache->remove(fontFaceRules[i]);
  if (m_resolver)
    m_resolver->invalidateMatchedPropertiesCache();
}

void StyleEngine::markTreeScopeDirty(TreeScope& scope) {
  if (scope == m_document) {
    markDocumentDirty();
    return;
  }

  DCHECK(m_styleSheetCollectionMap.contains(&scope));
  m_dirtyTreeScopes.add(&scope);
}

void StyleEngine::markDocumentDirty() {
  m_documentScopeDirty = true;
  if (document().importLoader())
    document().importsController()->master()->styleEngine().markDocumentDirty();
}

CSSStyleSheet* StyleEngine::createSheet(Element& element,
                                        const String& text,
                                        TextPosition startPosition,
                                        StyleEngineContext& context) {
  DCHECK(element.document() == document());
  CSSStyleSheet* styleSheet = nullptr;

  addPendingSheet(context);

  AtomicString textContent(text);

  auto result = m_textToSheetCache.add(textContent, nullptr);
  StyleSheetContents* contents = result.storedValue->value;
  if (result.isNewEntry || !contents ||
      !contents->isCacheableForStyleElement()) {
    result.storedValue->value = nullptr;
    styleSheet = parseSheet(element, text, startPosition);
    if (styleSheet->contents()->isCacheableForStyleElement()) {
      result.storedValue->value = styleSheet->contents();
      m_sheetToTextCache.add(styleSheet->contents(), textContent);
    }
  } else {
    DCHECK(contents);
    DCHECK(contents->isCacheableForStyleElement());
    DCHECK(contents->hasSingleOwnerDocument());
    contents->setIsUsedFromTextCache();
    styleSheet = CSSStyleSheet::createInline(contents, element, startPosition);
  }

  DCHECK(styleSheet);
  if (!element.isInShadowTree()) {
    styleSheet->setTitle(element.title());
    setPreferredStylesheetSetNameIfNotSet(element.title(),
                                          DontUpdateActiveSheets);
  }
  return styleSheet;
}

CSSStyleSheet* StyleEngine::parseSheet(Element& element,
                                       const String& text,
                                       TextPosition startPosition) {
  CSSStyleSheet* styleSheet = nullptr;
  styleSheet = CSSStyleSheet::createInline(element, KURL(), startPosition,
                                           document().characterSet());
  styleSheet->contents()->parseStringAtPosition(text, startPosition);
  return styleSheet;
}

void StyleEngine::collectScopedStyleFeaturesTo(RuleFeatureSet& features) const {
  HeapHashSet<Member<const StyleSheetContents>> visitedSharedStyleSheetContents;
  if (document().scopedStyleResolver())
    document().scopedStyleResolver()->collectFeaturesTo(
        features, visitedSharedStyleSheetContents);
  for (TreeScope* treeScope : m_activeTreeScopes) {
    // When creating StyleResolver, dirty treescopes might not be processed.
    // So some active treescopes might not have a scoped style resolver.
    // In this case, we should skip collectFeatures for the treescopes without
    // scoped style resolvers. When invoking updateActiveStyleSheets,
    // the treescope's features will be processed.
    if (ScopedStyleResolver* resolver = treeScope->scopedStyleResolver())
      resolver->collectFeaturesTo(features, visitedSharedStyleSheetContents);
  }
}

void StyleEngine::fontsNeedUpdate(CSSFontSelector*) {
  if (!document().isActive())
    return;

  if (m_resolver)
    m_resolver->invalidateMatchedPropertiesCache();
  document().setNeedsStyleRecalc(
      SubtreeStyleChange,
      StyleChangeReasonForTracing::create(StyleChangeReason::Fonts));
  InspectorInstrumentation::fontsUpdated(m_document);
}

void StyleEngine::setFontSelector(CSSFontSelector* fontSelector) {
  if (m_fontSelector)
    m_fontSelector->unregisterForInvalidationCallbacks(this);
  m_fontSelector = fontSelector;
  if (m_fontSelector)
    m_fontSelector->registerForInvalidationCallbacks(this);
}

void StyleEngine::platformColorsChanged() {
  if (m_resolver)
    m_resolver->invalidateMatchedPropertiesCache();
  document().setNeedsStyleRecalc(SubtreeStyleChange,
                                 StyleChangeReasonForTracing::create(
                                     StyleChangeReason::PlatformColorChange));
}

bool StyleEngine::shouldSkipInvalidationFor(const Element& element) const {
  if (!resolver())
    return true;
  if (!element.inActiveDocument())
    return true;
  if (!element.parentNode())
    return true;
  return element.parentNode()->getStyleChangeType() >= SubtreeStyleChange;
}

void StyleEngine::classChangedForElement(const SpaceSplitString& changedClasses,
                                         Element& element) {
  if (shouldSkipInvalidationFor(element))
    return;
  InvalidationLists invalidationLists;
  unsigned changedSize = changedClasses.size();
  RuleFeatureSet& ruleFeatureSet =
      ensureResolver().ensureUpdatedRuleFeatureSet();
  for (unsigned i = 0; i < changedSize; ++i)
    ruleFeatureSet.collectInvalidationSetsForClass(invalidationLists, element,
                                                   changedClasses[i]);
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     element);
}

void StyleEngine::classChangedForElement(const SpaceSplitString& oldClasses,
                                         const SpaceSplitString& newClasses,
                                         Element& element) {
  if (shouldSkipInvalidationFor(element))
    return;

  if (!oldClasses.size()) {
    classChangedForElement(newClasses, element);
    return;
  }

  // Class vectors tend to be very short. This is faster than using a hash
  // table.
  BitVector remainingClassBits;
  remainingClassBits.ensureSize(oldClasses.size());

  InvalidationLists invalidationLists;
  RuleFeatureSet& ruleFeatureSet =
      ensureResolver().ensureUpdatedRuleFeatureSet();

  for (unsigned i = 0; i < newClasses.size(); ++i) {
    bool found = false;
    for (unsigned j = 0; j < oldClasses.size(); ++j) {
      if (newClasses[i] == oldClasses[j]) {
        // Mark each class that is still in the newClasses so we can skip doing
        // an n^2 search below when looking for removals. We can't break from
        // this loop early since a class can appear more than once.
        remainingClassBits.quickSet(j);
        found = true;
      }
    }
    // Class was added.
    if (!found)
      ruleFeatureSet.collectInvalidationSetsForClass(invalidationLists, element,
                                                     newClasses[i]);
  }

  for (unsigned i = 0; i < oldClasses.size(); ++i) {
    if (remainingClassBits.quickGet(i))
      continue;
    // Class was removed.
    ruleFeatureSet.collectInvalidationSetsForClass(invalidationLists, element,
                                                   oldClasses[i]);
  }

  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     element);
}

void StyleEngine::attributeChangedForElement(const QualifiedName& attributeName,
                                             Element& element) {
  if (shouldSkipInvalidationFor(element))
    return;

  InvalidationLists invalidationLists;
  ensureResolver()
      .ensureUpdatedRuleFeatureSet()
      .collectInvalidationSetsForAttribute(invalidationLists, element,
                                           attributeName);
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     element);
}

void StyleEngine::idChangedForElement(const AtomicString& oldId,
                                      const AtomicString& newId,
                                      Element& element) {
  if (shouldSkipInvalidationFor(element))
    return;

  InvalidationLists invalidationLists;
  RuleFeatureSet& ruleFeatureSet =
      ensureResolver().ensureUpdatedRuleFeatureSet();
  if (!oldId.isEmpty())
    ruleFeatureSet.collectInvalidationSetsForId(invalidationLists, element,
                                                oldId);
  if (!newId.isEmpty())
    ruleFeatureSet.collectInvalidationSetsForId(invalidationLists, element,
                                                newId);
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     element);
}

void StyleEngine::pseudoStateChangedForElement(
    CSSSelector::PseudoType pseudoType,
    Element& element) {
  if (shouldSkipInvalidationFor(element))
    return;

  InvalidationLists invalidationLists;
  ensureResolver()
      .ensureUpdatedRuleFeatureSet()
      .collectInvalidationSetsForPseudoClass(invalidationLists, element,
                                             pseudoType);
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     element);
}

void StyleEngine::scheduleSiblingInvalidationsForElement(
    Element& element,
    ContainerNode& schedulingParent,
    unsigned minDirectAdjacent) {
  DCHECK(minDirectAdjacent);

  InvalidationLists invalidationLists;

  RuleFeatureSet& ruleFeatureSet =
      ensureResolver().ensureUpdatedRuleFeatureSet();

  if (element.hasID())
    ruleFeatureSet.collectSiblingInvalidationSetForId(
        invalidationLists, element, element.idForStyleResolution(),
        minDirectAdjacent);

  if (element.hasClass()) {
    const SpaceSplitString& classNames = element.classNames();
    for (size_t i = 0; i < classNames.size(); i++)
      ruleFeatureSet.collectSiblingInvalidationSetForClass(
          invalidationLists, element, classNames[i], minDirectAdjacent);
  }

  for (const Attribute& attribute : element.attributes())
    ruleFeatureSet.collectSiblingInvalidationSetForAttribute(
        invalidationLists, element, attribute.name(), minDirectAdjacent);

  ruleFeatureSet.collectUniversalSiblingInvalidationSet(invalidationLists,
                                                        minDirectAdjacent);

  m_styleInvalidator.scheduleSiblingInvalidationsAsDescendants(
      invalidationLists, schedulingParent);
}

void StyleEngine::scheduleInvalidationsForInsertedSibling(
    Element* beforeElement,
    Element& insertedElement) {
  unsigned affectedSiblings =
      insertedElement.parentNode()->childrenAffectedByIndirectAdjacentRules()
          ? UINT_MAX
          : m_maxDirectAdjacentSelectors;

  ContainerNode* schedulingParent = insertedElement.parentElementOrShadowRoot();
  if (!schedulingParent)
    return;

  scheduleSiblingInvalidationsForElement(insertedElement, *schedulingParent, 1);

  for (unsigned i = 1; beforeElement && i <= affectedSiblings;
       i++, beforeElement = ElementTraversal::previousSibling(*beforeElement))
    scheduleSiblingInvalidationsForElement(*beforeElement, *schedulingParent,
                                           i);
}

void StyleEngine::scheduleInvalidationsForRemovedSibling(
    Element* beforeElement,
    Element& removedElement,
    Element& afterElement) {
  unsigned affectedSiblings =
      afterElement.parentNode()->childrenAffectedByIndirectAdjacentRules()
          ? UINT_MAX
          : m_maxDirectAdjacentSelectors;

  ContainerNode* schedulingParent = afterElement.parentElementOrShadowRoot();
  if (!schedulingParent)
    return;

  scheduleSiblingInvalidationsForElement(removedElement, *schedulingParent, 1);

  for (unsigned i = 1; beforeElement && i <= affectedSiblings;
       i++, beforeElement = ElementTraversal::previousSibling(*beforeElement))
    scheduleSiblingInvalidationsForElement(*beforeElement, *schedulingParent,
                                           i);
}

void StyleEngine::scheduleNthPseudoInvalidations(ContainerNode& nthParent) {
  InvalidationLists invalidationLists;
  ensureResolver().ensureUpdatedRuleFeatureSet().collectNthInvalidationSet(
      invalidationLists);
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     nthParent);
}

void StyleEngine::scheduleRuleSetInvalidationsForElement(
    Element& element,
    const HeapVector<Member<RuleSet>>& ruleSets) {
  AtomicString id;
  const SpaceSplitString* classNames = nullptr;

  if (element.hasID())
    id = element.idForStyleResolution();
  if (element.hasClass())
    classNames = &element.classNames();

  InvalidationLists invalidationLists;
  for (const auto& ruleSet : ruleSets) {
    if (!id.isNull())
      ruleSet->features().collectInvalidationSetsForId(invalidationLists,
                                                       element, id);
    if (classNames) {
      unsigned classNameCount = classNames->size();
      for (size_t i = 0; i < classNameCount; i++)
        ruleSet->features().collectInvalidationSetsForClass(
            invalidationLists, element, (*classNames)[i]);
    }
    for (const Attribute& attribute : element.attributes())
      ruleSet->features().collectInvalidationSetsForAttribute(
          invalidationLists, element, attribute.name());
    if (ruleSet->tagRules(element.localNameForSelectorMatching()))
      element.setNeedsStyleRecalc(LocalStyleChange,
                                  StyleChangeReasonForTracing::create(
                                      StyleChangeReason::StyleSheetChange));
  }
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     element);
}

void StyleEngine::invalidateSlottedElements(HTMLSlotElement& slot) {
  for (auto& node : slot.getDistributedNodes()) {
    if (node->isElementNode())
      node->setNeedsStyleRecalc(LocalStyleChange,
                                StyleChangeReasonForTracing::create(
                                    StyleChangeReason::StyleSheetChange));
  }
}

void StyleEngine::scheduleInvalidationsForRuleSets(
    TreeScope& treeScope,
    const HeapVector<Member<RuleSet>>& ruleSets) {
#if DCHECK_IS_ON()
  // Full scope recalcs should be handled while collecting the ruleSets before
  // calling this method.
  for (auto ruleSet : ruleSets)
    DCHECK(!ruleSet->features().needsFullRecalcForRuleSetInvalidation());
#endif  // DCHECK_IS_ON()

  bool invalidateSlotted = false;
  if (treeScope.rootNode().isShadowRoot()) {
    Element& host = toShadowRoot(treeScope.rootNode()).host();
    scheduleRuleSetInvalidationsForElement(host, ruleSets);
    if (host.getStyleChangeType() >= SubtreeStyleChange)
      return;
    for (auto ruleSet : ruleSets) {
      if (ruleSet->hasSlottedRules()) {
        invalidateSlotted = true;
        break;
      }
    }
  }

  Node* stayWithin = &treeScope.rootNode();
  Element* element = ElementTraversal::firstChild(*stayWithin);
  while (element) {
    scheduleRuleSetInvalidationsForElement(*element, ruleSets);
    if (invalidateSlotted && isHTMLSlotElement(element))
      invalidateSlottedElements(toHTMLSlotElement(*element));

    if (element->getStyleChangeType() < SubtreeStyleChange)
      element = ElementTraversal::next(*element, stayWithin);
    else
      element = ElementTraversal::nextSkippingChildren(*element, stayWithin);
  }
}

void StyleEngine::setStatsEnabled(bool enabled) {
  if (!enabled) {
    m_styleResolverStats = nullptr;
    return;
  }
  if (!m_styleResolverStats)
    m_styleResolverStats = StyleResolverStats::create();
  else
    m_styleResolverStats->reset();
}

void StyleEngine::setPreferredStylesheetSetNameIfNotSet(
    const String& name,
    ActiveSheetsUpdate activeSheetsUpdate) {
  if (!m_preferredStylesheetSetName.isEmpty())
    return;
  m_preferredStylesheetSetName = name;
  // TODO(rune@opera.com): Setting the selected set here is wrong if the set
  // has been previously set by through Document.selectedStylesheetSet. Our
  // current implementation ignores the effect of Document.selectedStylesheetSet
  // and either only collects persistent style, or additionally preferred
  // style when present.
  m_selectedStylesheetSetName = name;

  // TODO(rune@opera.com): For async stylesheet update, we should always mark
  // the TreeScope dirty here, and the synchronous active stylesheet update
  // (resolverChanged) should go away.
  if (activeSheetsUpdate == UpdateActiveSheets) {
    markDocumentDirty();
    resolverChanged(AnalyzedStyleUpdate);
  }
}

void StyleEngine::setSelectedStylesheetSetName(const String& name) {
  m_selectedStylesheetSetName = name;
  // TODO(rune@opera.com): Setting Document.selectedStylesheetSet currently
  // has no other effect than the ability to read back the set value using
  // the same api. If it did have an effect, we should have marked the
  // document scope dirty and triggered an update of the active stylesheets
  // from here.
}

void StyleEngine::setHttpDefaultStyle(const String& content) {
  setPreferredStylesheetSetNameIfNotSet(content, UpdateActiveSheets);
}

void StyleEngine::ensureFullscreenUAStyle() {
  CSSDefaultStyleSheets::instance().ensureDefaultStyleSheetForFullscreen();
  if (!m_resolver)
    return;
  if (!m_resolver->hasFullscreenUAStyle())
    m_resolver->resetRuleFeatures();
}

void StyleEngine::initialViewportChanged() {
  if (!m_viewportResolver)
    return;

  m_viewportResolver->initialViewportChanged();

  // TODO(rune@opera.com): for async stylesheet update, updateViewport() should
  // be called as part of the lifecycle update for active style. Synchronous for
  // now.
  m_viewportResolver->updateViewport(documentStyleSheetCollection());
}

void StyleEngine::viewportRulesChanged() {
  if (!m_viewportResolver)
    return;
  m_viewportResolver->setNeedsCollectRules();

  // TODO(rune@opera.com): for async stylesheet update, updateViewport() should
  // be called as part of the lifecycle update for active style. Synchronous for
  // now.
  m_viewportResolver->updateViewport(documentStyleSheetCollection());
}

DEFINE_TRACE(StyleEngine) {
  visitor->trace(m_document);
  visitor->trace(m_injectedAuthorStyleSheets);
  visitor->trace(m_inspectorStyleSheet);
  visitor->trace(m_documentStyleSheetCollection);
  visitor->trace(m_styleSheetCollectionMap);
  visitor->trace(m_resolver);
  visitor->trace(m_viewportResolver);
  visitor->trace(m_styleInvalidator);
  visitor->trace(m_dirtyTreeScopes);
  visitor->trace(m_activeTreeScopes);
  visitor->trace(m_treeBoundaryCrossingScopes);
  visitor->trace(m_fontSelector);
  visitor->trace(m_textToSheetCache);
  visitor->trace(m_sheetToTextCache);
  CSSFontSelectorClient::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(StyleEngine) {
  for (auto sheet : m_injectedAuthorStyleSheets) {
    visitor->traceWrappers(sheet);
  }
  visitor->traceWrappers(m_documentStyleSheetCollection);
}

}  // namespace blink
