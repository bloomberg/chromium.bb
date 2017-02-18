/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#ifndef StyleEngine_h
#define StyleEngine_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/TraceWrapperMember.h"
#include "core/CoreExport.h"
#include "core/css/ActiveStyleSheets.h"
#include "core/css/CSSFontSelectorClient.h"
#include "core/css/CSSGlobalRuleSet.h"
#include "core/css/invalidation/StyleInvalidator.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentOrderedList.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/StyleEngineContext.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/AutoReset.h"
#include "wtf/ListHashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class CSSFontSelector;
class CSSStyleSheet;
class MediaQueryEvaluator;
class Node;
class RuleFeatureSet;
class ShadowTreeStyleSheetCollection;
class StyleRuleFontFace;
class StyleRuleUsageTracker;
class StyleSheetContents;
class ViewportStyleResolver;

class CORE_EXPORT StyleEngine final
    : public GarbageCollectedFinalized<StyleEngine>,
      public CSSFontSelectorClient,
      public TraceWrapperBase {
  USING_GARBAGE_COLLECTED_MIXIN(StyleEngine);

 public:
  class IgnoringPendingStylesheet {
    DISALLOW_NEW();

   public:
    IgnoringPendingStylesheet(StyleEngine& engine)
        : m_scope(&engine.m_ignorePendingStylesheets, true) {}

   private:
    AutoReset<bool> m_scope;
  };

  friend class IgnoringPendingStylesheet;

  static StyleEngine* create(Document& document) {
    return new StyleEngine(document);
  }

  ~StyleEngine();

  const HeapVector<TraceWrapperMember<StyleSheet>>&
  styleSheetsForStyleSheetList(TreeScope&);

  const HeapVector<TraceWrapperMember<CSSStyleSheet>>&
  injectedAuthorStyleSheets() const {
    return m_injectedAuthorStyleSheets;
  }
  CSSStyleSheet* inspectorStyleSheet() const { return m_inspectorStyleSheet; }

  const ActiveStyleSheetVector activeStyleSheetsForInspector();

  bool needsActiveStyleUpdate() const;
  void setNeedsActiveStyleUpdate(TreeScope&);
  void addStyleSheetCandidateNode(Node&);
  void removeStyleSheetCandidateNode(Node&, ContainerNode& insertionPoint);
  void modifiedStyleSheetCandidateNode(Node&);
  void mediaQueriesChangedInScope(TreeScope&);
  void watchedSelectorsChanged();
  void initialViewportChanged();
  void viewportRulesChanged();
  void htmlImportAddedOrRemoved();

  void injectAuthorSheet(StyleSheetContents* authorSheet);
  CSSStyleSheet& ensureInspectorStyleSheet();
  RuleSet* watchedSelectorsRuleSet() {
    return m_globalRuleSet.watchedSelectorsRuleSet();
  }

  RuleSet* ruleSetForSheet(CSSStyleSheet&);
  void mediaQueryAffectingValueChanged();
  void updateStyleSheetsInImport(StyleEngine& masterEngine,
                                 DocumentStyleSheetCollector& parentCollector);
  void updateActiveStyle();
  void markAllTreeScopesDirty() { m_allTreeScopesDirty = true; }

  String preferredStylesheetSetName() const {
    return m_preferredStylesheetSetName;
  }
  String selectedStylesheetSetName() const {
    return m_selectedStylesheetSetName;
  }
  void setPreferredStylesheetSetNameIfNotSet(const String&);
  void setSelectedStylesheetSetName(const String&);
  void setHttpDefaultStyle(const String&);

  void addPendingSheet(StyleEngineContext&);
  void removePendingSheet(Node& styleSheetCandidateNode,
                          const StyleEngineContext&);

  bool hasPendingScriptBlockingSheets() const {
    return m_pendingScriptBlockingStylesheets > 0;
  }
  bool hasPendingRenderBlockingSheets() const {
    return m_pendingRenderBlockingStylesheets > 0;
  }
  bool haveScriptBlockingStylesheetsLoaded() const {
    return !hasPendingScriptBlockingSheets() || m_ignorePendingStylesheets;
  }
  bool haveRenderBlockingStylesheetsLoaded() const {
    return !hasPendingRenderBlockingSheets() || m_ignorePendingStylesheets;
  }
  bool ignoringPendingStylesheets() const { return m_ignorePendingStylesheets; }

  unsigned maxDirectAdjacentSelectors() const {
    return ruleFeatureSet().maxDirectAdjacentSelectors();
  }
  bool usesSiblingRules() const { return ruleFeatureSet().usesSiblingRules(); }
  bool usesFirstLineRules() const {
    return ruleFeatureSet().usesFirstLineRules();
  }
  bool usesWindowInactiveSelector() const {
    return ruleFeatureSet().usesWindowInactiveSelector();
  }

  bool usesRemUnits() const { return m_usesRemUnits; }
  void setUsesRemUnit(bool usesRemUnits) { m_usesRemUnits = usesRemUnits; }

  void resetCSSFeatureFlags(const RuleFeatureSet&);

  void shadowRootRemovedFromDocument(ShadowRoot*);
  void addTreeBoundaryCrossingScope(const TreeScope&);
  const DocumentOrderedList& treeBoundaryCrossingScopes() const {
    return m_treeBoundaryCrossingScopes;
  }
  void resetAuthorStyle(TreeScope&);

  StyleResolver* resolver() const { return m_resolver; }

  void setRuleUsageTracker(StyleRuleUsageTracker*);

  StyleResolver& ensureResolver() {
    updateActiveStyle();
    if (!m_resolver)
      createResolver();
    return *m_resolver;
  }

  bool hasResolver() const { return m_resolver; }

  StyleInvalidator& styleInvalidator() { return m_styleInvalidator; }
  bool mediaQueryAffectedByViewportChange();
  bool mediaQueryAffectedByDeviceChange();
  bool hasViewportDependentMediaQueries() const {
    return !m_globalRuleSet.ruleFeatureSet()
                .viewportDependentMediaQueryResults()
                .isEmpty();
  }

  CSSFontSelector* fontSelector() { return m_fontSelector; }
  void setFontSelector(CSSFontSelector*);

  void removeFontFaceRules(const HeapVector<Member<const StyleRuleFontFace>>&);
  void clearFontCache();
  // updateGenericFontFamilySettings is used from WebSettingsImpl.
  void updateGenericFontFamilySettings();

  void didDetach();

  CSSStyleSheet* createSheet(Element&,
                             const String& text,
                             TextPosition startPosition,
                             StyleEngineContext&);

  void collectScopedStyleFeaturesTo(RuleFeatureSet&) const;
  void ensureUAStyleForFullscreen();
  void ensureUAStyleForElement(const Element&);

  void platformColorsChanged();

  bool hasRulesForId(const AtomicString& id) const;
  void classChangedForElement(const SpaceSplitString& changedClasses, Element&);
  void classChangedForElement(const SpaceSplitString& oldClasses,
                              const SpaceSplitString& newClasses,
                              Element&);
  void attributeChangedForElement(const QualifiedName& attributeName, Element&);
  void idChangedForElement(const AtomicString& oldId,
                           const AtomicString& newId,
                           Element&);
  void pseudoStateChangedForElement(CSSSelector::PseudoType, Element&);
  void scheduleSiblingInvalidationsForElement(Element&,
                                              ContainerNode& schedulingParent,
                                              unsigned minDirectAdjacent);
  void scheduleInvalidationsForInsertedSibling(Element* beforeElement,
                                               Element& insertedElement);
  void scheduleInvalidationsForRemovedSibling(Element* beforeElement,
                                              Element& removedElement,
                                              Element& afterElement);
  void scheduleNthPseudoInvalidations(ContainerNode&);
  void scheduleInvalidationsForRuleSets(TreeScope&,
                                        const HeapHashSet<Member<RuleSet>>&);

  void elementWillBeRemoved(Element& element) {
    m_styleInvalidator.rescheduleSiblingInvalidationsAsDescendants(element);
  }

  unsigned styleForElementCount() const { return m_styleForElementCount; }
  void incStyleForElementCount() { m_styleForElementCount++; }

  StyleResolverStats* stats() { return m_styleResolverStats.get(); }
  void setStatsEnabled(bool);

  PassRefPtr<ComputedStyle> findSharedStyle(const ElementResolveContext&);

  void applyRuleSetChanges(TreeScope&,
                           const ActiveStyleSheetVector& oldStyleSheets,
                           const ActiveStyleSheetVector& newStyleSheets);

  DECLARE_VIRTUAL_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  // CSSFontSelectorClient implementation.
  void fontsNeedUpdate(CSSFontSelector*) override;

 private:
  StyleEngine(Document&);
  bool needsActiveStyleSheetUpdate() const {
    return m_allTreeScopesDirty || m_documentScopeDirty ||
           m_dirtyTreeScopes.size();
  }

  TreeScopeStyleSheetCollection* ensureStyleSheetCollectionFor(TreeScope&);
  TreeScopeStyleSheetCollection* styleSheetCollectionFor(TreeScope&);
  bool shouldUpdateDocumentStyleSheetCollection() const;
  bool shouldUpdateShadowTreeStyleSheetCollection() const;

  void markDocumentDirty();
  void markTreeScopeDirty(TreeScope&);

  bool isMaster() const { return m_isMaster; }
  Document* master();
  Document& document() const { return *m_document; }

  typedef HeapHashSet<Member<TreeScope>> UnorderedTreeScopeSet;

  void mediaQueryAffectingValueChanged(UnorderedTreeScopeSet&);
  const RuleFeatureSet& ruleFeatureSet() const {
    return m_globalRuleSet.ruleFeatureSet();
  }

  void createResolver();
  void clearResolvers();

  CSSStyleSheet* parseSheet(Element&,
                            const String& text,
                            TextPosition startPosition);

  const DocumentStyleSheetCollection& documentStyleSheetCollection() const {
    DCHECK(m_documentStyleSheetCollection);
    return *m_documentStyleSheetCollection;
  }

  DocumentStyleSheetCollection& documentStyleSheetCollection() {
    DCHECK(m_documentStyleSheetCollection);
    return *m_documentStyleSheetCollection;
  }

  void updateActiveStyleSheetsInShadow(
      TreeScope*,
      UnorderedTreeScopeSet& treeScopesRemoved);

  bool shouldSkipInvalidationFor(const Element&) const;
  void scheduleRuleSetInvalidationsForElement(
      Element&,
      const HeapHashSet<Member<RuleSet>>&);
  void scheduleTypeRuleSetInvalidations(ContainerNode&,
                                        const HeapHashSet<Member<RuleSet>>&);
  void invalidateSlottedElements(HTMLSlotElement&);

  void updateViewport();
  void updateActiveStyleSheets();
  void updateGlobalRuleSet() {
    DCHECK(!needsActiveStyleSheetUpdate());
    m_globalRuleSet.update(document());
  }
  const MediaQueryEvaluator& ensureMediaQueryEvaluator();

  Member<Document> m_document;
  bool m_isMaster;

  // Track the number of currently loading top-level stylesheets needed for
  // layout.  Sheets loaded using the @import directive are not included in this
  // count.  We use this count of pending sheets to detect when we can begin
  // attaching elements and when it is safe to execute scripts.
  int m_pendingScriptBlockingStylesheets = 0;
  int m_pendingRenderBlockingStylesheets = 0;
  int m_pendingBodyStylesheets = 0;

  HeapVector<TraceWrapperMember<CSSStyleSheet>> m_injectedAuthorStyleSheets;
  Member<CSSStyleSheet> m_inspectorStyleSheet;

  TraceWrapperMember<DocumentStyleSheetCollection>
      m_documentStyleSheetCollection;

  Member<StyleRuleUsageTracker> m_tracker;

  using StyleSheetCollectionMap =
      HeapHashMap<WeakMember<TreeScope>,
                  Member<ShadowTreeStyleSheetCollection>>;
  StyleSheetCollectionMap m_styleSheetCollectionMap;

  bool m_documentScopeDirty = true;
  bool m_allTreeScopesDirty = false;
  UnorderedTreeScopeSet m_dirtyTreeScopes;
  UnorderedTreeScopeSet m_activeTreeScopes;
  DocumentOrderedList m_treeBoundaryCrossingScopes;

  String m_preferredStylesheetSetName;
  String m_selectedStylesheetSetName;

  CSSGlobalRuleSet m_globalRuleSet;

  bool m_usesRemUnits = false;
  bool m_ignorePendingStylesheets = false;

  Member<StyleResolver> m_resolver;
  Member<ViewportStyleResolver> m_viewportResolver;
  Member<MediaQueryEvaluator> m_mediaQueryEvaluator;
  StyleInvalidator m_styleInvalidator;

  Member<CSSFontSelector> m_fontSelector;

  HeapHashMap<AtomicString, WeakMember<StyleSheetContents>> m_textToSheetCache;
  HeapHashMap<WeakMember<StyleSheetContents>, AtomicString> m_sheetToTextCache;

  std::unique_ptr<StyleResolverStats> m_styleResolverStats;
  unsigned m_styleForElementCount = 0;

  friend class StyleEngineTest;
};

}  // namespace blink

#endif
