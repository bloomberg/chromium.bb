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
#include "core/css/resolver/SharedStyleFinder.h"
#include "core/css/resolver/StyleRuleUsageTracker.h"
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
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/Page.h"
#include "core/svg/SVGStyleElement.h"
#include "platform/fonts/FontCache.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

using namespace HTMLNames;

StyleEngine::StyleEngine(Document& document)
    : m_document(&document),
      m_isMaster(!document.importsController() ||
                 document.importsController()->master() == &document),
      m_documentStyleSheetCollection(
          this,
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
      m_styleSheetCollectionMap.insert(&treeScope, nullptr);
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
  // TODO(rune@opera.com): we could split styleSheets and active stylesheet
  // update to have a lighter update while accessing the styleSheets list.
  DCHECK(master());
  if (master()->isActive()) {
    if (isMaster())
      updateActiveStyle();
    else
      master()->styleEngine().updateActiveStyle();
  }

  if (treeScope == m_document)
    return documentStyleSheetCollection().styleSheetsForStyleSheetList();

  return ensureStyleSheetCollectionFor(treeScope)
      ->styleSheetsForStyleSheetList();
}

void StyleEngine::injectAuthorSheet(StyleSheetContents* authorSheet) {
  m_injectedAuthorStyleSheets.push_back(TraceWrapperMember<CSSStyleSheet>(
      this, CSSStyleSheet::create(authorSheet, *m_document)));
  markDocumentDirty();
}

CSSStyleSheet& StyleEngine::ensureInspectorStyleSheet() {
  if (m_inspectorStyleSheet)
    return *m_inspectorStyleSheet;

  StyleSheetContents* contents =
      StyleSheetContents::create(CSSParserContext::create(*m_document));
  m_inspectorStyleSheet = CSSStyleSheet::create(contents, *m_document);
  markDocumentDirty();
  // TODO(rune@opera.com): Making the active stylesheets up-to-date here is
  // required by some inspector tests, at least. I theory this should not be
  // necessary. Need to investigate to figure out if/why.
  updateActiveStyle();
  return *m_inspectorStyleSheet;
}

void StyleEngine::addPendingSheet(StyleEngineContext& context) {
  m_pendingScriptBlockingStylesheets++;

  context.addingPendingSheet(document());
  if (context.addedPendingSheetBeforeBody()) {
    m_pendingRenderBlockingStylesheets++;
  } else {
    m_pendingBodyStylesheets++;
    document().didAddPendingStylesheetInBody();
  }
}

// This method is called whenever a top-level stylesheet has finished loading.
void StyleEngine::removePendingSheet(Node& styleSheetCandidateNode,
                                     const StyleEngineContext& context) {
  if (styleSheetCandidateNode.isConnected())
    setNeedsActiveStyleUpdate(styleSheetCandidateNode.treeScope());

  if (context.addedPendingSheetBeforeBody()) {
    DCHECK_GT(m_pendingRenderBlockingStylesheets, 0);
    m_pendingRenderBlockingStylesheets--;
  } else {
    DCHECK_GT(m_pendingBodyStylesheets, 0);
    m_pendingBodyStylesheets--;
    if (!m_pendingBodyStylesheets)
      document().didRemoveAllPendingBodyStylesheets();
  }

  // Make sure we knew this sheet was pending, and that our count isn't out of
  // sync.
  DCHECK_GT(m_pendingScriptBlockingStylesheets, 0);

  m_pendingScriptBlockingStylesheets--;
  if (m_pendingScriptBlockingStylesheets)
    return;

  document().didRemoveAllPendingStylesheet();
}

void StyleEngine::setNeedsActiveStyleUpdate(TreeScope& treeScope) {
  if (document().isActive() || !isMaster())
    markTreeScopeDirty(treeScope);
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

  setNeedsActiveStyleUpdate(treeScope);
  if (treeScope != m_document)
    m_activeTreeScopes.insert(&treeScope);
}

void StyleEngine::removeStyleSheetCandidateNode(Node& node,
                                                ContainerNode& insertionPoint) {
  DCHECK(!isXSLStyleSheet(node));
  DCHECK(insertionPoint.isConnected());

  ShadowRoot* shadowRoot = node.containingShadowRoot();
  if (!shadowRoot)
    shadowRoot = insertionPoint.containingShadowRoot();

  TreeScope& treeScope =
      shadowRoot ? *toTreeScope(shadowRoot) : toTreeScope(document());
  TreeScopeStyleSheetCollection* collection =
      styleSheetCollectionFor(treeScope);
  // After detaching document, collection could be null. In the case,
  // we should not update anything. Instead, just return.
  if (!collection)
    return;
  collection->removeStyleSheetCandidateNode(node);

  setNeedsActiveStyleUpdate(treeScope);
}

void StyleEngine::modifiedStyleSheetCandidateNode(Node& node) {
  if (node.isConnected())
    setNeedsActiveStyleUpdate(node.treeScope());
}

void StyleEngine::mediaQueriesChangedInScope(TreeScope& treeScope) {
  if (ScopedStyleResolver* resolver = treeScope.scopedStyleResolver())
    resolver->setNeedsAppendAllSheets();
  setNeedsActiveStyleUpdate(treeScope);
}

void StyleEngine::watchedSelectorsChanged() {
  m_globalRuleSet.initWatchedSelectorsRuleSet(document());
  // TODO(rune@opera.com): Should be able to use RuleSetInvalidation here.
  document().setNeedsStyleRecalc(SubtreeStyleChange,
                                 StyleChangeReasonForTracing::create(
                                     StyleChangeReason::DeclarativeContent));
}

bool StyleEngine::shouldUpdateDocumentStyleSheetCollection() const {
  return m_allTreeScopesDirty || m_documentScopeDirty;
}

bool StyleEngine::shouldUpdateShadowTreeStyleSheetCollection() const {
  return m_allTreeScopesDirty || !m_dirtyTreeScopes.isEmpty();
}

void StyleEngine::mediaQueryAffectingValueChanged(
    UnorderedTreeScopeSet& treeScopes) {
  for (TreeScope* treeScope : treeScopes) {
    DCHECK(treeScope != m_document);
    ShadowTreeStyleSheetCollection* collection =
        toShadowTreeStyleSheetCollection(styleSheetCollectionFor(*treeScope));
    DCHECK(collection);
    if (collection->mediaQueryAffectingValueChanged())
      setNeedsActiveStyleUpdate(*treeScope);
  }
}

void StyleEngine::mediaQueryAffectingValueChanged() {
  if (documentStyleSheetCollection().mediaQueryAffectingValueChanged())
    setNeedsActiveStyleUpdate(document());
  mediaQueryAffectingValueChanged(m_activeTreeScopes);
  if (m_resolver)
    m_resolver->updateMediaType();
}

void StyleEngine::updateStyleSheetsInImport(
    StyleEngine& masterEngine,
    DocumentStyleSheetCollector& parentCollector) {
  DCHECK(!isMaster());
  HeapVector<Member<StyleSheet>> sheetsForList;
  ImportedDocumentStyleSheetCollector subcollector(parentCollector,
                                                   sheetsForList);
  documentStyleSheetCollection().collectStyleSheets(masterEngine, subcollector);
  documentStyleSheetCollection().swapSheetsForSheetList(sheetsForList);
}

void StyleEngine::updateActiveStyleSheetsInShadow(
    TreeScope* treeScope,
    UnorderedTreeScopeSet& treeScopesRemoved) {
  DCHECK_NE(treeScope, m_document);
  ShadowTreeStyleSheetCollection* collection =
      toShadowTreeStyleSheetCollection(styleSheetCollectionFor(*treeScope));
  DCHECK(collection);
  collection->updateActiveStyleSheets(*this);
  if (!collection->hasStyleSheetCandidateNodes()) {
    treeScopesRemoved.insert(treeScope);
    // When removing TreeScope from ActiveTreeScopes,
    // its resolver should be destroyed by invoking resetAuthorStyle.
    DCHECK(!treeScope->scopedStyleResolver());
  }
}

void StyleEngine::updateActiveStyleSheets() {
  if (!needsActiveStyleSheetUpdate())
    return;

  DCHECK(isMaster());
  DCHECK(!document().inStyleRecalc());
  DCHECK(document().isActive());

  TRACE_EVENT0("blink,blink_style", "StyleEngine::updateActiveStyleSheets");

  if (shouldUpdateDocumentStyleSheetCollection())
    documentStyleSheetCollection().updateActiveStyleSheets(*this);

  if (shouldUpdateShadowTreeStyleSheetCollection()) {
    UnorderedTreeScopeSet treeScopesRemoved;

    if (m_allTreeScopesDirty) {
      for (TreeScope* treeScope : m_activeTreeScopes)
        updateActiveStyleSheetsInShadow(treeScope, treeScopesRemoved);
    } else {
      for (TreeScope* treeScope : m_dirtyTreeScopes)
        updateActiveStyleSheetsInShadow(treeScope, treeScopesRemoved);
    }
    for (TreeScope* treeScope : treeScopesRemoved)
      m_activeTreeScopes.erase(treeScope);
  }

  probe::activeStyleSheetsUpdated(m_document);

  m_dirtyTreeScopes.clear();
  m_documentScopeDirty = false;
  m_allTreeScopesDirty = false;
}

void StyleEngine::updateViewport() {
  if (m_viewportResolver)
    m_viewportResolver->updateViewport(documentStyleSheetCollection());
}

bool StyleEngine::needsActiveStyleUpdate() const {
  return (m_viewportResolver && m_viewportResolver->needsUpdate()) ||
         needsActiveStyleSheetUpdate() || m_globalRuleSet.isDirty();
}

void StyleEngine::updateActiveStyle() {
  DCHECK(document().isActive());
  updateViewport();
  updateActiveStyleSheets();
  updateGlobalRuleSet();
}

const ActiveStyleSheetVector StyleEngine::activeStyleSheetsForInspector() {
  if (document().isActive())
    updateActiveStyle();

  if (m_activeTreeScopes.isEmpty())
    return documentStyleSheetCollection().activeAuthorStyleSheets();

  ActiveStyleSheetVector activeStyleSheets;

  activeStyleSheets.appendVector(
      documentStyleSheetCollection().activeAuthorStyleSheets());
  for (TreeScope* treeScope : m_activeTreeScopes) {
    if (TreeScopeStyleSheetCollection* collection =
            m_styleSheetCollectionMap.at(treeScope))
      activeStyleSheets.appendVector(collection->activeAuthorStyleSheets());
  }

  // FIXME: Inspector needs a vector which has all active stylesheets.
  // However, creating such a large vector might cause performance regression.
  // Need to implement some smarter solution.
  return activeStyleSheets;
}

void StyleEngine::shadowRootRemovedFromDocument(ShadowRoot* shadowRoot) {
  m_styleSheetCollectionMap.erase(shadowRoot);
  m_activeTreeScopes.erase(shadowRoot);
  m_dirtyTreeScopes.erase(shadowRoot);
  resetAuthorStyle(*shadowRoot);
}

void StyleEngine::addTreeBoundaryCrossingScope(const TreeScope& treeScope) {
  m_treeBoundaryCrossingScopes.add(&treeScope.rootNode());
}

void StyleEngine::resetAuthorStyle(TreeScope& treeScope) {
  m_treeBoundaryCrossingScopes.remove(&treeScope.rootNode());

  ScopedStyleResolver* scopedResolver = treeScope.scopedStyleResolver();
  if (!scopedResolver)
    return;

  m_globalRuleSet.markDirty();
  if (treeScope.rootNode().isDocumentNode()) {
    scopedResolver->resetAuthorStyle();
    return;
  }

  treeScope.clearScopedStyleResolver();
}

void StyleEngine::setRuleUsageTracker(StyleRuleUsageTracker* tracker) {
  m_tracker = tracker;

  if (m_resolver)
    m_resolver->setRuleUsageTracker(m_tracker);
}

RuleSet* StyleEngine::ruleSetForSheet(CSSStyleSheet& sheet) {
  if (!sheet.matchesMediaQueries(ensureMediaQueryEvaluator()))
    return nullptr;

  AddRuleFlags addRuleFlags = RuleHasNoSpecialState;
  if (m_document->getSecurityOrigin()->canRequest(sheet.baseURL()))
    addRuleFlags = RuleHasDocumentSecurityOrigin;
  return &sheet.contents()->ensureRuleSet(*m_mediaQueryEvaluator, addRuleFlags);
}

void StyleEngine::createResolver() {
  m_resolver = StyleResolver::create(*m_document);
  m_resolver->setRuleUsageTracker(m_tracker);
}

void StyleEngine::clearResolvers() {
  DCHECK(!document().inStyleRecalc());
  DCHECK(isMaster() || !m_resolver);

  document().clearScopedStyleResolver();
  for (TreeScope* treeScope : m_activeTreeScopes)
    treeScope->clearScopedStyleResolver();

  if (m_resolver) {
    TRACE_EVENT1("blink", "StyleEngine::clearResolver", "frame",
                 document().frame());
    m_resolver->dispose();
    m_resolver.clear();
  }
}

void StyleEngine::didDetach() {
  clearResolvers();
  m_globalRuleSet.dispose();
  m_treeBoundaryCrossingScopes.clear();
  m_dirtyTreeScopes.clear();
  m_activeTreeScopes.clear();
  m_viewportResolver = nullptr;
  m_mediaQueryEvaluator = nullptr;
  if (m_fontSelector)
    m_fontSelector->fontFaceCache()->clearAll();
  m_fontSelector = nullptr;
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
  for (const auto& rule : fontFaceRules)
    cache->remove(rule);
  if (m_resolver)
    m_resolver->invalidateMatchedPropertiesCache();
}

void StyleEngine::markTreeScopeDirty(TreeScope& scope) {
  if (scope == m_document) {
    markDocumentDirty();
    return;
  }

  DCHECK(m_styleSheetCollectionMap.contains(&scope));
  m_dirtyTreeScopes.insert(&scope);
  document().scheduleLayoutTreeUpdateIfNeeded();
}

void StyleEngine::markDocumentDirty() {
  m_documentScopeDirty = true;
  if (RuntimeEnabledFeatures::cssViewportEnabled())
    viewportRulesChanged();
  if (document().importLoader())
    document().importsController()->master()->styleEngine().markDocumentDirty();
  else
    document().scheduleLayoutTreeUpdateIfNeeded();
}

CSSStyleSheet* StyleEngine::createSheet(Element& element,
                                        const String& text,
                                        TextPosition startPosition,
                                        StyleEngineContext& context) {
  DCHECK(element.document() == document());
  CSSStyleSheet* styleSheet = nullptr;

  addPendingSheet(context);

  AtomicString textContent(text);

  auto result = m_textToSheetCache.insert(textContent, nullptr);
  StyleSheetContents* contents = result.storedValue->value;
  if (result.isNewEntry || !contents ||
      !contents->isCacheableForStyleElement()) {
    result.storedValue->value = nullptr;
    styleSheet = parseSheet(element, text, startPosition);
    if (styleSheet->contents()->isCacheableForStyleElement()) {
      result.storedValue->value = styleSheet->contents();
      m_sheetToTextCache.insert(styleSheet->contents(), textContent);
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
    setPreferredStylesheetSetNameIfNotSet(element.title());
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
  probe::fontsUpdated(m_document);
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
  const RuleFeatureSet& features = ruleFeatureSet();
  for (unsigned i = 0; i < changedSize; ++i) {
    features.collectInvalidationSetsForClass(invalidationLists, element,
                                             changedClasses[i]);
  }
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
  const RuleFeatureSet& features = ruleFeatureSet();

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
    if (!found) {
      features.collectInvalidationSetsForClass(invalidationLists, element,
                                               newClasses[i]);
    }
  }

  for (unsigned i = 0; i < oldClasses.size(); ++i) {
    if (remainingClassBits.quickGet(i))
      continue;
    // Class was removed.
    features.collectInvalidationSetsForClass(invalidationLists, element,
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
  ruleFeatureSet().collectInvalidationSetsForAttribute(invalidationLists,
                                                       element, attributeName);
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     element);
}

void StyleEngine::idChangedForElement(const AtomicString& oldId,
                                      const AtomicString& newId,
                                      Element& element) {
  if (shouldSkipInvalidationFor(element))
    return;

  InvalidationLists invalidationLists;
  const RuleFeatureSet& features = ruleFeatureSet();
  if (!oldId.isEmpty())
    features.collectInvalidationSetsForId(invalidationLists, element, oldId);
  if (!newId.isEmpty())
    features.collectInvalidationSetsForId(invalidationLists, element, newId);
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     element);
}

void StyleEngine::pseudoStateChangedForElement(
    CSSSelector::PseudoType pseudoType,
    Element& element) {
  if (shouldSkipInvalidationFor(element))
    return;

  InvalidationLists invalidationLists;
  ruleFeatureSet().collectInvalidationSetsForPseudoClass(invalidationLists,
                                                         element, pseudoType);
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     element);
}

void StyleEngine::scheduleSiblingInvalidationsForElement(
    Element& element,
    ContainerNode& schedulingParent,
    unsigned minDirectAdjacent) {
  DCHECK(minDirectAdjacent);

  InvalidationLists invalidationLists;

  const RuleFeatureSet& features = ruleFeatureSet();

  if (element.hasID()) {
    features.collectSiblingInvalidationSetForId(invalidationLists, element,
                                                element.idForStyleResolution(),
                                                minDirectAdjacent);
  }

  if (element.hasClass()) {
    const SpaceSplitString& classNames = element.classNames();
    for (size_t i = 0; i < classNames.size(); i++)
      features.collectSiblingInvalidationSetForClass(
          invalidationLists, element, classNames[i], minDirectAdjacent);
  }

  for (const Attribute& attribute : element.attributes())
    features.collectSiblingInvalidationSetForAttribute(
        invalidationLists, element, attribute.name(), minDirectAdjacent);

  features.collectUniversalSiblingInvalidationSet(invalidationLists,
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
          : maxDirectAdjacentSelectors();

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
          : maxDirectAdjacentSelectors();

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
  ruleFeatureSet().collectNthInvalidationSet(invalidationLists);
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     nthParent);
}

void StyleEngine::scheduleRuleSetInvalidationsForElement(
    Element& element,
    const HeapHashSet<Member<RuleSet>>& ruleSets) {
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
  }
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists,
                                                     element);
}

void StyleEngine::scheduleTypeRuleSetInvalidations(
    ContainerNode& node,
    const HeapHashSet<Member<RuleSet>>& ruleSets) {
  InvalidationLists invalidationLists;
  for (const auto& ruleSet : ruleSets)
    ruleSet->features().collectTypeRuleInvalidationSet(invalidationLists, node);
  DCHECK(invalidationLists.siblings.isEmpty());
  m_styleInvalidator.scheduleInvalidationSetsForNode(invalidationLists, node);
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
    const HeapHashSet<Member<RuleSet>>& ruleSets) {
#if DCHECK_IS_ON()
  // Full scope recalcs should be handled while collecting the ruleSets before
  // calling this method.
  for (auto ruleSet : ruleSets)
    DCHECK(!ruleSet->features().needsFullRecalcForRuleSetInvalidation());
#endif  // DCHECK_IS_ON()

  TRACE_EVENT0("blink,blink_style",
               "StyleEngine::scheduleInvalidationsForRuleSets");

  scheduleTypeRuleSetInvalidations(treeScope.rootNode(), ruleSets);

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

void StyleEngine::setPreferredStylesheetSetNameIfNotSet(const String& name) {
  if (!m_preferredStylesheetSetName.isEmpty())
    return;
  m_preferredStylesheetSetName = name;
  // TODO(rune@opera.com): Setting the selected set here is wrong if the set
  // has been previously set by through Document.selectedStylesheetSet. Our
  // current implementation ignores the effect of Document.selectedStylesheetSet
  // and either only collects persistent style, or additionally preferred
  // style when present.
  m_selectedStylesheetSetName = name;
  markDocumentDirty();
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
  setPreferredStylesheetSetNameIfNotSet(content);
}

void StyleEngine::ensureUAStyleForFullscreen() {
  if (m_globalRuleSet.hasFullscreenUAStyle())
    return;
  CSSDefaultStyleSheets::instance().ensureDefaultStyleSheetForFullscreen();
  m_globalRuleSet.markDirty();
  updateActiveStyle();
}

void StyleEngine::ensureUAStyleForElement(const Element& element) {
  if (CSSDefaultStyleSheets::instance().ensureDefaultStyleSheetsForElement(
          element)) {
    m_globalRuleSet.markDirty();
    updateActiveStyle();
  }
}

bool StyleEngine::hasRulesForId(const AtomicString& id) const {
  return m_globalRuleSet.ruleFeatureSet().hasSelectorForId(id);
}

void StyleEngine::initialViewportChanged() {
  if (m_viewportResolver)
    m_viewportResolver->initialViewportChanged();
}

void StyleEngine::viewportRulesChanged() {
  if (m_viewportResolver)
    m_viewportResolver->setNeedsCollectRules();
}

void StyleEngine::htmlImportAddedOrRemoved() {
  if (document().importLoader()) {
    document()
        .importsController()
        ->master()
        ->styleEngine()
        .htmlImportAddedOrRemoved();
    return;
  }

  // When we remove an import link and re-insert it into the document, the
  // import Document and CSSStyleSheet pointers are persisted. That means the
  // comparison of active stylesheets is not able to figure out that the order
  // of the stylesheets have changed after insertion.
  //
  // This is also the case when we import the same document twice where the
  // last inserted document is inserted before the first one in dom order where
  // the last would take precedence.
  //
  // Fall back to re-add all sheets to the scoped resolver and recalculate style
  // for the whole document when we remove or insert an import document.
  if (ScopedStyleResolver* resolver = document().scopedStyleResolver()) {
    markDocumentDirty();
    resolver->setNeedsAppendAllSheets();
    document().setNeedsStyleRecalc(
        SubtreeStyleChange, StyleChangeReasonForTracing::create(
                                StyleChangeReason::ActiveStylesheetsUpdate));
  }
}

PassRefPtr<ComputedStyle> StyleEngine::findSharedStyle(
    const ElementResolveContext& elementResolveContext) {
  DCHECK(m_resolver);
  return SharedStyleFinder(
             elementResolveContext, m_globalRuleSet.ruleFeatureSet(),
             m_globalRuleSet.siblingRuleSet(),
             m_globalRuleSet.uncommonAttributeRuleSet(), *m_resolver)
      .findSharedStyle();
}
namespace {

enum RuleSetFlags {
  FontFaceRules = 1 << 0,
  KeyframesRules = 1 << 1,
  FullRecalcRules = 1 << 2
};

unsigned getRuleSetFlags(const HeapHashSet<Member<RuleSet>> ruleSets) {
  unsigned flags = 0;
  for (auto& ruleSet : ruleSets) {
    ruleSet->compactRulesIfNeeded();
    if (!ruleSet->keyframesRules().isEmpty())
      flags |= KeyframesRules;
    if (!ruleSet->fontFaceRules().isEmpty())
      flags |= FontFaceRules;
    if (ruleSet->needsFullRecalcForRuleSetInvalidation())
      flags |= FullRecalcRules;
  }
  return flags;
}

}  // namespace

void StyleEngine::applyRuleSetChanges(
    TreeScope& treeScope,
    const ActiveStyleSheetVector& oldStyleSheets,
    const ActiveStyleSheetVector& newStyleSheets) {
  HeapHashSet<Member<RuleSet>> changedRuleSets;

  ScopedStyleResolver* scopedResolver = treeScope.scopedStyleResolver();
  bool appendAllSheets =
      scopedResolver && scopedResolver->needsAppendAllSheets();

  ActiveSheetsChange change =
      compareActiveStyleSheets(oldStyleSheets, newStyleSheets, changedRuleSets);
  if (change == NoActiveSheetsChanged && !appendAllSheets)
    return;

  // With rules added or removed, we need to re-aggregate rule meta data.
  m_globalRuleSet.markDirty();

  unsigned changedRuleFlags = getRuleSetFlags(changedRuleSets);
  bool fontsChanged = treeScope.rootNode().isDocumentNode() &&
                      (changedRuleFlags & FontFaceRules);
  unsigned appendStartIndex = 0;

  // We don't need to clear the font cache if new sheets are appended.
  if (fontsChanged && change == ActiveSheetsChanged)
    clearFontCache();

  // - If all sheets were removed, we remove the ScopedStyleResolver.
  // - If new sheets were appended to existing ones, start appending after the
  //   common prefix.
  // - For other diffs, reset author style and re-add all sheets for the
  //   TreeScope.
  if (treeScope.scopedStyleResolver()) {
    if (newStyleSheets.isEmpty())
      resetAuthorStyle(treeScope);
    else if (change == ActiveSheetsAppended && !appendAllSheets)
      appendStartIndex = oldStyleSheets.size();
    else
      treeScope.scopedStyleResolver()->resetAuthorStyle();
  }

  if (!newStyleSheets.isEmpty()) {
    treeScope.ensureScopedStyleResolver().appendActiveStyleSheets(
        appendStartIndex, newStyleSheets);
  }

  if (treeScope.document().hasPendingForcedStyleRecalc())
    return;

  if (!treeScope.document().body() ||
      treeScope.document().hasNodesWithPlaceholderStyle()) {
    treeScope.document().setNeedsStyleRecalc(
        SubtreeStyleChange, StyleChangeReasonForTracing::create(
                                StyleChangeReason::CleanupPlaceholderStyles));
    return;
  }

  if (changedRuleFlags & KeyframesRules)
    ScopedStyleResolver::keyframesRulesAdded(treeScope);

  if (fontsChanged || (changedRuleFlags & FullRecalcRules)) {
    ScopedStyleResolver::invalidationRootForTreeScope(treeScope)
        .setNeedsStyleRecalc(SubtreeStyleChange,
                             StyleChangeReasonForTracing::create(
                                 StyleChangeReason::ActiveStylesheetsUpdate));
    return;
  }

  scheduleInvalidationsForRuleSets(treeScope, changedRuleSets);
}

const MediaQueryEvaluator& StyleEngine::ensureMediaQueryEvaluator() {
  if (!m_mediaQueryEvaluator) {
    if (document().frame())
      m_mediaQueryEvaluator = new MediaQueryEvaluator(document().frame());
    else
      m_mediaQueryEvaluator = new MediaQueryEvaluator("all");
  }
  return *m_mediaQueryEvaluator;
}

bool StyleEngine::mediaQueryAffectedByViewportChange() {
  const MediaQueryEvaluator& evaluator = ensureMediaQueryEvaluator();
  const auto& results =
      m_globalRuleSet.ruleFeatureSet().viewportDependentMediaQueryResults();
  for (unsigned i = 0; i < results.size(); ++i) {
    if (evaluator.eval(results[i]->expression()) != results[i]->result())
      return true;
  }
  return false;
}

bool StyleEngine::mediaQueryAffectedByDeviceChange() {
  const MediaQueryEvaluator& evaluator = ensureMediaQueryEvaluator();
  const auto& results =
      m_globalRuleSet.ruleFeatureSet().deviceDependentMediaQueryResults();
  for (unsigned i = 0; i < results.size(); ++i) {
    if (evaluator.eval(results[i]->expression()) != results[i]->result())
      return true;
  }
  return false;
}

DEFINE_TRACE(StyleEngine) {
  visitor->trace(m_document);
  visitor->trace(m_injectedAuthorStyleSheets);
  visitor->trace(m_inspectorStyleSheet);
  visitor->trace(m_documentStyleSheetCollection);
  visitor->trace(m_styleSheetCollectionMap);
  visitor->trace(m_dirtyTreeScopes);
  visitor->trace(m_activeTreeScopes);
  visitor->trace(m_treeBoundaryCrossingScopes);
  visitor->trace(m_globalRuleSet);
  visitor->trace(m_resolver);
  visitor->trace(m_viewportResolver);
  visitor->trace(m_mediaQueryEvaluator);
  visitor->trace(m_styleInvalidator);
  visitor->trace(m_fontSelector);
  visitor->trace(m_textToSheetCache);
  visitor->trace(m_sheetToTextCache);
  visitor->trace(m_tracker);
  CSSFontSelectorClient::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(StyleEngine) {
  for (auto sheet : m_injectedAuthorStyleSheets) {
    visitor->traceWrappers(sheet);
  }
  visitor->traceWrappers(m_documentStyleSheetCollection);
}

}  // namespace blink
