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

#include <memory>
#include <utility>
#include "core/CoreExport.h"
#include "core/css/ActiveStyleSheets.h"
#include "core/css/CSSFontSelectorClient.h"
#include "core/css/CSSGlobalRuleSet.h"
#include "core/css/invalidation/StyleInvalidator.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverStats.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/StyleEngineContext.h"
#include "core/dom/TreeOrderedList.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebDocument.h"

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
        : scope_(&engine.ignore_pending_stylesheets_, true) {}

   private:
    AutoReset<bool> scope_;
  };

  friend class IgnoringPendingStylesheet;

  static StyleEngine* Create(Document& document) {
    return new StyleEngine(document);
  }

  ~StyleEngine();

  const HeapVector<TraceWrapperMember<StyleSheet>>&
  StyleSheetsForStyleSheetList(TreeScope&);

  const HeapVector<
      std::pair<WebStyleSheetId, TraceWrapperMember<CSSStyleSheet>>>&
  InjectedAuthorStyleSheets() const {
    return injected_author_style_sheets_;
  }
  CSSStyleSheet* InspectorStyleSheet() const { return inspector_style_sheet_; }

  const ActiveStyleSheetVector ActiveStyleSheetsForInspector();

  bool NeedsActiveStyleUpdate() const;
  void SetNeedsActiveStyleUpdate(TreeScope&);
  void AddStyleSheetCandidateNode(Node&);
  void RemoveStyleSheetCandidateNode(Node&, ContainerNode& insertion_point);
  void ModifiedStyleSheetCandidateNode(Node&);
  void MediaQueriesChangedInScope(TreeScope&);
  void WatchedSelectorsChanged();
  void InitialViewportChanged();
  void ViewportRulesChanged();
  void HtmlImportAddedOrRemoved();

  WebStyleSheetId InjectAuthorSheet(StyleSheetContents* author_sheet);
  void RemoveInjectedAuthorSheet(WebStyleSheetId author_sheet_id);
  CSSStyleSheet& EnsureInspectorStyleSheet();
  RuleSet* WatchedSelectorsRuleSet() {
    DCHECK(IsMaster());
    DCHECK(global_rule_set_);
    return global_rule_set_->WatchedSelectorsRuleSet();
  }
  bool HasStyleSheets() const {
    return GetDocumentStyleSheetCollection().HasStyleSheets();
  }

  RuleSet* RuleSetForSheet(CSSStyleSheet&);
  void MediaQueryAffectingValueChanged();
  void UpdateActiveStyleSheetsInImport(
      StyleEngine& master_engine,
      DocumentStyleSheetCollector& parent_collector);
  void UpdateActiveStyle();
  void MarkAllTreeScopesDirty() { all_tree_scopes_dirty_ = true; }

  String PreferredStylesheetSetName() const {
    return preferred_stylesheet_set_name_;
  }
  String SelectedStylesheetSetName() const {
    return selected_stylesheet_set_name_;
  }
  void SetPreferredStylesheetSetNameIfNotSet(const String&);
  void SetSelectedStylesheetSetName(const String&);
  void SetHttpDefaultStyle(const String&);

  void AddPendingSheet(StyleEngineContext&);
  void RemovePendingSheet(Node& style_sheet_candidate_node,
                          const StyleEngineContext&);

  bool HasPendingScriptBlockingSheets() const {
    return pending_script_blocking_stylesheets_ > 0;
  }
  bool HasPendingRenderBlockingSheets() const {
    return pending_render_blocking_stylesheets_ > 0;
  }
  bool HaveScriptBlockingStylesheetsLoaded() const {
    return !HasPendingScriptBlockingSheets() || ignore_pending_stylesheets_;
  }
  bool HaveRenderBlockingStylesheetsLoaded() const {
    return !HasPendingRenderBlockingSheets() || ignore_pending_stylesheets_;
  }
  bool IgnoringPendingStylesheets() const {
    return ignore_pending_stylesheets_;
  }

  unsigned MaxDirectAdjacentSelectors() const {
    return GetRuleFeatureSet().MaxDirectAdjacentSelectors();
  }
  bool UsesSiblingRules() const {
    return GetRuleFeatureSet().UsesSiblingRules();
  }
  bool UsesFirstLineRules() const {
    return GetRuleFeatureSet().UsesFirstLineRules();
  }
  bool UsesWindowInactiveSelector() const {
    return GetRuleFeatureSet().UsesWindowInactiveSelector();
  }

  bool UsesRemUnits() const { return uses_rem_units_; }
  void SetUsesRemUnit(bool uses_rem_units) { uses_rem_units_ = uses_rem_units; }

  void ResetCSSFeatureFlags(const RuleFeatureSet&);

  void ShadowRootRemovedFromDocument(ShadowRoot*);
  void AddTreeBoundaryCrossingScope(const TreeScope&);
  const TreeOrderedList& TreeBoundaryCrossingScopes() const {
    return tree_boundary_crossing_scopes_;
  }
  void ResetAuthorStyle(TreeScope&);

  StyleResolver* Resolver() const { return resolver_; }

  void SetRuleUsageTracker(StyleRuleUsageTracker*);

  StyleResolver& EnsureResolver() {
    UpdateActiveStyle();
    if (!resolver_)
      CreateResolver();
    return *resolver_;
  }

  bool HasResolver() const { return resolver_; }

  StyleInvalidator& GetStyleInvalidator() { return style_invalidator_; }
  bool MediaQueryAffectedByViewportChange();
  bool MediaQueryAffectedByDeviceChange();
  bool HasViewportDependentMediaQueries() const {
    DCHECK(IsMaster());
    DCHECK(global_rule_set_);
    return !global_rule_set_->GetRuleFeatureSet()
                .ViewportDependentMediaQueryResults()
                .IsEmpty();
  }

  CSSFontSelector* FontSelector() { return font_selector_; }
  void SetFontSelector(CSSFontSelector*);

  void RemoveFontFaceRules(const HeapVector<Member<const StyleRuleFontFace>>&);
  void ClearFontCache();
  // updateGenericFontFamilySettings is used from WebSettingsImpl.
  void UpdateGenericFontFamilySettings();

  void DidDetach();

  CSSStyleSheet* CreateSheet(Element&,
                             const String& text,
                             TextPosition start_position,
                             StyleEngineContext&);

  void CollectScopedStyleFeaturesTo(RuleFeatureSet&) const;
  void EnsureUAStyleForFullscreen();
  void EnsureUAStyleForElement(const Element&);

  void PlatformColorsChanged();

  bool HasRulesForId(const AtomicString& id) const;
  void ClassChangedForElement(const SpaceSplitString& changed_classes,
                              Element&);
  void ClassChangedForElement(const SpaceSplitString& old_classes,
                              const SpaceSplitString& new_classes,
                              Element&);
  void AttributeChangedForElement(const QualifiedName& attribute_name,
                                  Element&);
  void IdChangedForElement(const AtomicString& old_id,
                           const AtomicString& new_id,
                           Element&);
  void PseudoStateChangedForElement(CSSSelector::PseudoType, Element&);
  void ScheduleSiblingInvalidationsForElement(Element&,
                                              ContainerNode& scheduling_parent,
                                              unsigned min_direct_adjacent);
  void ScheduleInvalidationsForInsertedSibling(Element* before_element,
                                               Element& inserted_element);
  void ScheduleInvalidationsForRemovedSibling(Element* before_element,
                                              Element& removed_element,
                                              Element& after_element);
  void ScheduleNthPseudoInvalidations(ContainerNode&);
  void ScheduleInvalidationsForRuleSets(TreeScope&,
                                        const HeapHashSet<Member<RuleSet>>&);

  void ElementWillBeRemoved(Element& element) {
    style_invalidator_.RescheduleSiblingInvalidationsAsDescendants(element);
  }

  unsigned StyleForElementCount() const { return style_for_element_count_; }
  void IncStyleForElementCount() { style_for_element_count_++; }

  StyleResolverStats* Stats() { return style_resolver_stats_.get(); }
  void SetStatsEnabled(bool);

  PassRefPtr<ComputedStyle> FindSharedStyle(const ElementResolveContext&);

  void ApplyRuleSetChanges(TreeScope&,
                           const ActiveStyleSheetVector& old_style_sheets,
                           const ActiveStyleSheetVector& new_style_sheets);

  DECLARE_VIRTUAL_TRACE();
  DECLARE_TRACE_WRAPPERS();

 private:
  // CSSFontSelectorClient implementation.
  void FontsNeedUpdate(CSSFontSelector*) override;

 private:
  StyleEngine(Document&);
  bool NeedsActiveStyleSheetUpdate() const {
    return all_tree_scopes_dirty_ || tree_scopes_removed_ ||
           document_scope_dirty_ || dirty_tree_scopes_.size();
  }

  TreeScopeStyleSheetCollection& EnsureStyleSheetCollectionFor(TreeScope&);
  TreeScopeStyleSheetCollection* StyleSheetCollectionFor(TreeScope&);
  bool ShouldUpdateDocumentStyleSheetCollection() const;
  bool ShouldUpdateShadowTreeStyleSheetCollection() const;

  void MarkDocumentDirty();
  void MarkTreeScopeDirty(TreeScope&);

  bool IsMaster() const { return is_master_; }
  Document* Master();
  Document& GetDocument() const { return *document_; }

  typedef HeapHashSet<Member<TreeScope>> UnorderedTreeScopeSet;

  void MediaQueryAffectingValueChanged(UnorderedTreeScopeSet&);
  const RuleFeatureSet& GetRuleFeatureSet() const {
    DCHECK(IsMaster());
    DCHECK(global_rule_set_);
    return global_rule_set_->GetRuleFeatureSet();
  }

  void CreateResolver();
  void ClearResolvers();

  CSSStyleSheet* ParseSheet(Element&,
                            const String& text,
                            TextPosition start_position);

  const DocumentStyleSheetCollection& GetDocumentStyleSheetCollection() const {
    DCHECK(document_style_sheet_collection_);
    return *document_style_sheet_collection_;
  }

  DocumentStyleSheetCollection& GetDocumentStyleSheetCollection() {
    DCHECK(document_style_sheet_collection_);
    return *document_style_sheet_collection_;
  }

  void UpdateActiveStyleSheetsInShadow(
      TreeScope*,
      UnorderedTreeScopeSet& tree_scopes_removed);

  bool ShouldSkipInvalidationFor(const Element&) const;
  void ScheduleRuleSetInvalidationsForElement(
      Element&,
      const HeapHashSet<Member<RuleSet>>&);
  void ScheduleTypeRuleSetInvalidations(ContainerNode&,
                                        const HeapHashSet<Member<RuleSet>>&);
  void InvalidateSlottedElements(HTMLSlotElement&);

  void UpdateViewport();
  void UpdateActiveStyleSheets();
  void UpdateGlobalRuleSet() {
    DCHECK(!NeedsActiveStyleSheetUpdate());
    if (global_rule_set_)
      global_rule_set_->Update(GetDocument());
  }
  const MediaQueryEvaluator& EnsureMediaQueryEvaluator();
  void UpdateStyleSheetList(TreeScope&);

  Member<Document> document_;
  bool is_master_;

  // Track the number of currently loading top-level stylesheets needed for
  // layout.  Sheets loaded using the @import directive are not included in this
  // count.  We use this count of pending sheets to detect when we can begin
  // attaching elements and when it is safe to execute scripts.
  int pending_script_blocking_stylesheets_ = 0;
  int pending_render_blocking_stylesheets_ = 0;
  int pending_body_stylesheets_ = 0;

  HeapVector<std::pair<WebStyleSheetId, TraceWrapperMember<CSSStyleSheet>>>
      injected_author_style_sheets_;
  Member<CSSStyleSheet> inspector_style_sheet_;

  TraceWrapperMember<DocumentStyleSheetCollection>
      document_style_sheet_collection_;

  Member<StyleRuleUsageTracker> tracker_;

  using StyleSheetCollectionMap =
      HeapHashMap<WeakMember<TreeScope>,
                  Member<ShadowTreeStyleSheetCollection>>;
  StyleSheetCollectionMap style_sheet_collection_map_;

  bool document_scope_dirty_ = true;
  bool all_tree_scopes_dirty_ = false;
  bool tree_scopes_removed_ = false;
  UnorderedTreeScopeSet dirty_tree_scopes_;
  UnorderedTreeScopeSet active_tree_scopes_;
  TreeOrderedList tree_boundary_crossing_scopes_;

  String preferred_stylesheet_set_name_;
  String selected_stylesheet_set_name_;

  bool uses_rem_units_ = false;
  bool ignore_pending_stylesheets_ = false;

  Member<StyleResolver> resolver_;
  Member<ViewportStyleResolver> viewport_resolver_;
  Member<MediaQueryEvaluator> media_query_evaluator_;
  Member<CSSGlobalRuleSet> global_rule_set_;
  StyleInvalidator style_invalidator_;

  Member<CSSFontSelector> font_selector_;

  HeapHashMap<AtomicString, WeakMember<StyleSheetContents>>
      text_to_sheet_cache_;
  HeapHashMap<WeakMember<StyleSheetContents>, AtomicString>
      sheet_to_text_cache_;

  std::unique_ptr<StyleResolverStats> style_resolver_stats_;
  unsigned style_for_element_count_ = 0;

  WebStyleSheetId injected_author_sheets_id_count_ = 0;

  friend class StyleEngineTest;
};

}  // namespace blink

#endif
