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

#include "core/css/StyleEngine.h"

#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/DocumentStyleSheetCollector.h"
#include "core/css/FontFaceCache.h"
#include "core/css/ShadowTreeStyleSheetCollection.h"
#include "core/css/StyleChangeReason.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/invalidation/InvalidationSet.h"
#include "core/css/resolver/ScopedStyleResolver.h"
#include "core/css/resolver/StyleRuleUsageTracker.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/Element.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/ShadowRoot.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/html_names.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "core/svg/SVGStyleElement.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontSelector.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

using namespace HTMLNames;

StyleEngine::StyleEngine(Document& document)
    : document_(&document),
      is_master_(!document.IsHTMLImport()),
      document_style_sheet_collection_(
          DocumentStyleSheetCollection::Create(document)) {
  if (document.GetFrame()) {
    // We don't need to create CSSFontSelector for imported document or
    // HTMLTemplateElement's document, because those documents have no frame.
    font_selector_ = CSSFontSelector::Create(&document);
    font_selector_->RegisterForInvalidationCallbacks(this);
  }
  if (document.IsInMainFrame())
    viewport_resolver_ = ViewportStyleResolver::Create(document);
  if (IsMaster())
    global_rule_set_ = CSSGlobalRuleSet::Create();
}

StyleEngine::~StyleEngine() {}

inline Document* StyleEngine::Master() {
  if (IsMaster())
    return document_;
  HTMLImportsController* import = GetDocument().ImportsController();
  // Document::ImportsController() can return null while executing its
  // destructor.
  if (!import)
    return nullptr;
  return import->Master();
}

TreeScopeStyleSheetCollection& StyleEngine::EnsureStyleSheetCollectionFor(
    TreeScope& tree_scope) {
  if (tree_scope == document_)
    return GetDocumentStyleSheetCollection();

  StyleSheetCollectionMap::AddResult result =
      style_sheet_collection_map_.insert(&tree_scope, nullptr);
  if (result.is_new_entry) {
    result.stored_value->value =
        new ShadowTreeStyleSheetCollection(ToShadowRoot(tree_scope));
  }
  return *result.stored_value->value.Get();
}

TreeScopeStyleSheetCollection* StyleEngine::StyleSheetCollectionFor(
    TreeScope& tree_scope) {
  if (tree_scope == document_)
    return &GetDocumentStyleSheetCollection();

  StyleSheetCollectionMap::iterator it =
      style_sheet_collection_map_.find(&tree_scope);
  if (it == style_sheet_collection_map_.end())
    return nullptr;
  return it->value.Get();
}

const HeapVector<TraceWrapperMember<StyleSheet>>&
StyleEngine::StyleSheetsForStyleSheetList(TreeScope& tree_scope) {
  DCHECK(Master());
  TreeScopeStyleSheetCollection& collection =
      EnsureStyleSheetCollectionFor(tree_scope);
  if (Master()->IsActive()) {
    if (all_tree_scopes_dirty_) {
      // If all tree scopes are dirty, update all of active style. Otherwise, we
      // would have to mark all tree scopes explicitly dirty for stylesheet list
      // or repeatedly update the stylesheet list on styleSheets access. Note
      // that this can only happen once if we kDidLayoutWithPendingSheets in
      // Document::UpdateStyleAndLayoutTreeIgnoringPendingStyleSheets.
      UpdateActiveStyle();
    } else {
      collection.UpdateStyleSheetList();
    }
  }
  return collection.StyleSheetsForStyleSheetList();
}

WebStyleSheetId StyleEngine::AddUserSheet(StyleSheetContents* sheet) {
  user_style_sheets_.push_back(
      std::make_pair(++user_sheets_id_count_,
                     CSSStyleSheet::Create(sheet, *document_)));

  MarkUserStyleDirty();
  return user_sheets_id_count_;
}

void StyleEngine::RemoveUserSheet(WebStyleSheetId sheet_id) {
  for (size_t i = 0; i < user_style_sheets_.size(); ++i) {
    if (user_style_sheets_[i].first == sheet_id) {
      user_style_sheets_.EraseAt(i);
      MarkUserStyleDirty();
    }
  }
}

CSSStyleSheet& StyleEngine::EnsureInspectorStyleSheet() {
  if (inspector_style_sheet_)
    return *inspector_style_sheet_;

  StyleSheetContents* contents =
      StyleSheetContents::Create(CSSParserContext::Create(*document_));
  inspector_style_sheet_ = CSSStyleSheet::Create(contents, *document_);
  MarkDocumentDirty();
  // TODO(rune@opera.com): Making the active stylesheets up-to-date here is
  // required by some inspector tests, at least. I theory this should not be
  // necessary. Need to investigate to figure out if/why.
  UpdateActiveStyle();
  return *inspector_style_sheet_;
}

void StyleEngine::AddPendingSheet(StyleEngineContext& context) {
  pending_script_blocking_stylesheets_++;

  context.AddingPendingSheet(GetDocument());
  if (context.AddedPendingSheetBeforeBody()) {
    pending_render_blocking_stylesheets_++;
  } else {
    pending_body_stylesheets_++;
    GetDocument().DidAddPendingStylesheetInBody();
  }
}

// This method is called whenever a top-level stylesheet has finished loading.
void StyleEngine::RemovePendingSheet(Node& style_sheet_candidate_node,
                                     const StyleEngineContext& context) {
  if (style_sheet_candidate_node.isConnected())
    SetNeedsActiveStyleUpdate(style_sheet_candidate_node.GetTreeScope());

  if (context.AddedPendingSheetBeforeBody()) {
    DCHECK_GT(pending_render_blocking_stylesheets_, 0);
    pending_render_blocking_stylesheets_--;
  } else {
    DCHECK_GT(pending_body_stylesheets_, 0);
    pending_body_stylesheets_--;
    if (!pending_body_stylesheets_)
      GetDocument().DidRemoveAllPendingBodyStylesheets();
  }

  // Make sure we knew this sheet was pending, and that our count isn't out of
  // sync.
  DCHECK_GT(pending_script_blocking_stylesheets_, 0);

  pending_script_blocking_stylesheets_--;
  if (pending_script_blocking_stylesheets_)
    return;

  GetDocument().DidRemoveAllPendingStylesheet();
}

void StyleEngine::SetNeedsActiveStyleUpdate(TreeScope& tree_scope) {
  if (GetDocument().IsActive() || !IsMaster())
    MarkTreeScopeDirty(tree_scope);
}

void StyleEngine::AddStyleSheetCandidateNode(Node& node) {
  if (!node.isConnected() || GetDocument().IsDetached())
    return;

  DCHECK(!IsXSLStyleSheet(node));
  TreeScope& tree_scope = node.GetTreeScope();
  EnsureStyleSheetCollectionFor(tree_scope).AddStyleSheetCandidateNode(node);

  SetNeedsActiveStyleUpdate(tree_scope);
  if (tree_scope != document_)
    active_tree_scopes_.insert(&tree_scope);
}

void StyleEngine::RemoveStyleSheetCandidateNode(
    Node& node,
    ContainerNode& insertion_point) {
  DCHECK(!IsXSLStyleSheet(node));
  DCHECK(insertion_point.isConnected());

  ShadowRoot* shadow_root = node.ContainingShadowRoot();
  if (!shadow_root)
    shadow_root = insertion_point.ContainingShadowRoot();

  TreeScope& tree_scope =
      shadow_root ? *ToTreeScope(shadow_root) : ToTreeScope(GetDocument());
  TreeScopeStyleSheetCollection* collection =
      StyleSheetCollectionFor(tree_scope);
  // After detaching document, collection could be null. In the case,
  // we should not update anything. Instead, just return.
  if (!collection)
    return;
  collection->RemoveStyleSheetCandidateNode(node);

  SetNeedsActiveStyleUpdate(tree_scope);
}

void StyleEngine::ModifiedStyleSheetCandidateNode(Node& node) {
  if (node.isConnected())
    SetNeedsActiveStyleUpdate(node.GetTreeScope());
}

void StyleEngine::MediaQueriesChangedInScope(TreeScope& tree_scope) {
  if (ScopedStyleResolver* resolver = tree_scope.GetScopedStyleResolver())
    resolver->SetNeedsAppendAllSheets();
  SetNeedsActiveStyleUpdate(tree_scope);
}

void StyleEngine::WatchedSelectorsChanged() {
  DCHECK(IsMaster());
  DCHECK(global_rule_set_);
  global_rule_set_->InitWatchedSelectorsRuleSet(GetDocument());
  // TODO(rune@opera.com): Should be able to use RuleSetInvalidation here.
  GetDocument().SetNeedsStyleRecalc(
      kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                               StyleChangeReason::kDeclarativeContent));
}

bool StyleEngine::ShouldUpdateDocumentStyleSheetCollection() const {
  return all_tree_scopes_dirty_ || document_scope_dirty_;
}

bool StyleEngine::ShouldUpdateShadowTreeStyleSheetCollection() const {
  return all_tree_scopes_dirty_ || !dirty_tree_scopes_.IsEmpty();
}

void StyleEngine::MediaQueryAffectingValueChanged(
    UnorderedTreeScopeSet& tree_scopes) {
  for (TreeScope* tree_scope : tree_scopes) {
    DCHECK(tree_scope != document_);
    ShadowTreeStyleSheetCollection* collection =
        ToShadowTreeStyleSheetCollection(StyleSheetCollectionFor(*tree_scope));
    DCHECK(collection);
    if (collection->MediaQueryAffectingValueChanged())
      SetNeedsActiveStyleUpdate(*tree_scope);
  }
}

void StyleEngine::MediaQueryAffectingValueChanged() {
  if (GetDocumentStyleSheetCollection().MediaQueryAffectingValueChanged())
    SetNeedsActiveStyleUpdate(GetDocument());
  MediaQueryAffectingValueChanged(active_tree_scopes_);
  if (resolver_)
    resolver_->UpdateMediaType();
}

void StyleEngine::UpdateActiveStyleSheetsInImport(
    StyleEngine& master_engine,
    DocumentStyleSheetCollector& parent_collector) {
  if (!RuntimeEnabledFeatures::HTMLImportsStyleApplicationEnabled())
    return;

  DCHECK(!IsMaster());
  HeapVector<Member<StyleSheet>> sheets_for_list;
  ImportedDocumentStyleSheetCollector subcollector(parent_collector,
                                                   sheets_for_list);
  GetDocumentStyleSheetCollection().CollectStyleSheets(master_engine,
                                                       subcollector);
  GetDocumentStyleSheetCollection().SwapSheetsForSheetList(sheets_for_list);

  // all_tree_scopes_dirty_ should only be set on main documents, never html
  // imports.
  DCHECK(!all_tree_scopes_dirty_);
  // Mark false for consistency. It is never checked for import documents.
  document_scope_dirty_ = false;
}

void StyleEngine::UpdateActiveStyleSheetsInShadow(
    TreeScope* tree_scope,
    UnorderedTreeScopeSet& tree_scopes_removed) {
  DCHECK_NE(tree_scope, document_);
  ShadowTreeStyleSheetCollection* collection =
      ToShadowTreeStyleSheetCollection(StyleSheetCollectionFor(*tree_scope));
  DCHECK(collection);
  collection->UpdateActiveStyleSheets(*this);
  if (!collection->HasStyleSheetCandidateNodes()) {
    tree_scopes_removed.insert(tree_scope);
    // When removing TreeScope from ActiveTreeScopes,
    // its resolver should be destroyed by invoking resetAuthorStyle.
    DCHECK(!tree_scope->GetScopedStyleResolver());
  }
}

void StyleEngine::UpdateActiveUserStyleSheets() {
  DCHECK(user_style_dirty_);

  ActiveStyleSheetVector new_active_sheets;
  for (auto& sheet : user_style_sheets_) {
    if (RuleSet* rule_set = RuleSetForSheet(*sheet.second))
      new_active_sheets.push_back(std::make_pair(sheet.second, rule_set));
  }

  ApplyRuleSetChanges(*document_, active_user_style_sheets_, new_active_sheets,
                      kInvalidateAllScopes);

  new_active_sheets.swap(active_user_style_sheets_);
}

void StyleEngine::UpdateActiveStyleSheets() {
  if (!NeedsActiveStyleSheetUpdate())
    return;

  DCHECK(IsMaster());
  DCHECK(!GetDocument().InStyleRecalc());
  DCHECK(GetDocument().IsActive());

  TRACE_EVENT0("blink,blink_style", "StyleEngine::updateActiveStyleSheets");

  if (user_style_dirty_)
    UpdateActiveUserStyleSheets();

  if (ShouldUpdateDocumentStyleSheetCollection())
    GetDocumentStyleSheetCollection().UpdateActiveStyleSheets(*this);

  if (ShouldUpdateShadowTreeStyleSheetCollection()) {
    UnorderedTreeScopeSet tree_scopes_removed;

    if (all_tree_scopes_dirty_) {
      for (TreeScope* tree_scope : active_tree_scopes_)
        UpdateActiveStyleSheetsInShadow(tree_scope, tree_scopes_removed);
    } else {
      for (TreeScope* tree_scope : dirty_tree_scopes_)
        UpdateActiveStyleSheetsInShadow(tree_scope, tree_scopes_removed);
    }
    for (TreeScope* tree_scope : tree_scopes_removed)
      active_tree_scopes_.erase(tree_scope);
  }

  probe::activeStyleSheetsUpdated(document_);

  dirty_tree_scopes_.clear();
  document_scope_dirty_ = false;
  all_tree_scopes_dirty_ = false;
  tree_scopes_removed_ = false;
  user_style_dirty_ = false;
}

void StyleEngine::UpdateViewport() {
  if (viewport_resolver_)
    viewport_resolver_->UpdateViewport(GetDocumentStyleSheetCollection());
}

bool StyleEngine::NeedsActiveStyleUpdate() const {
  return (viewport_resolver_ && viewport_resolver_->NeedsUpdate()) ||
         NeedsActiveStyleSheetUpdate() ||
         (global_rule_set_ && global_rule_set_->IsDirty());
}

void StyleEngine::UpdateActiveStyle() {
  DCHECK(GetDocument().IsActive());
  UpdateViewport();
  UpdateActiveStyleSheets();
  UpdateGlobalRuleSet();
}

const ActiveStyleSheetVector StyleEngine::ActiveStyleSheetsForInspector() {
  if (GetDocument().IsActive())
    UpdateActiveStyle();

  if (active_tree_scopes_.IsEmpty())
    return GetDocumentStyleSheetCollection().ActiveAuthorStyleSheets();

  ActiveStyleSheetVector active_style_sheets;

  active_style_sheets.AppendVector(
      GetDocumentStyleSheetCollection().ActiveAuthorStyleSheets());
  for (TreeScope* tree_scope : active_tree_scopes_) {
    if (TreeScopeStyleSheetCollection* collection =
            style_sheet_collection_map_.at(tree_scope))
      active_style_sheets.AppendVector(collection->ActiveAuthorStyleSheets());
  }

  // FIXME: Inspector needs a vector which has all active stylesheets.
  // However, creating such a large vector might cause performance regression.
  // Need to implement some smarter solution.
  return active_style_sheets;
}

void StyleEngine::ShadowRootRemovedFromDocument(ShadowRoot* shadow_root) {
  style_sheet_collection_map_.erase(shadow_root);
  active_tree_scopes_.erase(shadow_root);
  dirty_tree_scopes_.erase(shadow_root);
  tree_scopes_removed_ = true;
  ResetAuthorStyle(*shadow_root);
}

void StyleEngine::AddTreeBoundaryCrossingScope(const TreeScope& tree_scope) {
  tree_boundary_crossing_scopes_.Add(&tree_scope.RootNode());
}

void StyleEngine::ResetAuthorStyle(TreeScope& tree_scope) {
  tree_boundary_crossing_scopes_.Remove(&tree_scope.RootNode());

  ScopedStyleResolver* scoped_resolver = tree_scope.GetScopedStyleResolver();
  if (!scoped_resolver)
    return;

  global_rule_set_->MarkDirty();
  if (tree_scope.RootNode().IsDocumentNode()) {
    scoped_resolver->ResetAuthorStyle();
    return;
  }

  tree_scope.ClearScopedStyleResolver();
}

void StyleEngine::SetRuleUsageTracker(StyleRuleUsageTracker* tracker) {
  tracker_ = tracker;

  if (resolver_)
    resolver_->SetRuleUsageTracker(tracker_);
}

RuleSet* StyleEngine::RuleSetForSheet(CSSStyleSheet& sheet) {
  if (!sheet.MatchesMediaQueries(EnsureMediaQueryEvaluator()))
    return nullptr;

  AddRuleFlags add_rule_flags = kRuleHasNoSpecialState;
  if (document_->GetSecurityOrigin()->CanRequest(sheet.BaseURL()))
    add_rule_flags = kRuleHasDocumentSecurityOrigin;
  return &sheet.Contents()->EnsureRuleSet(*media_query_evaluator_,
                                          add_rule_flags);
}

void StyleEngine::CreateResolver() {
  resolver_ = StyleResolver::Create(*document_);
  resolver_->SetRuleUsageTracker(tracker_);
}

void StyleEngine::ClearResolvers() {
  DCHECK(!GetDocument().InStyleRecalc());
  DCHECK(IsMaster() || !resolver_);

  GetDocument().ClearScopedStyleResolver();
  for (TreeScope* tree_scope : active_tree_scopes_)
    tree_scope->ClearScopedStyleResolver();

  if (resolver_) {
    TRACE_EVENT1("blink", "StyleEngine::clearResolver", "frame",
                 GetDocument().GetFrame());
    resolver_->Dispose();
    resolver_.Clear();
  }
}

void StyleEngine::DidDetach() {
  ClearResolvers();
  if (global_rule_set_)
    global_rule_set_->Dispose();
  global_rule_set_ = nullptr;
  tree_boundary_crossing_scopes_.Clear();
  dirty_tree_scopes_.clear();
  active_tree_scopes_.clear();
  viewport_resolver_ = nullptr;
  media_query_evaluator_ = nullptr;
  if (font_selector_)
    font_selector_->GetFontFaceCache()->ClearAll();
  font_selector_ = nullptr;
}

void StyleEngine::ClearFontCache() {
  if (font_selector_)
    font_selector_->GetFontFaceCache()->ClearCSSConnected();
  if (resolver_)
    resolver_->InvalidateMatchedPropertiesCache();
}

void StyleEngine::UpdateGenericFontFamilySettings() {
  // FIXME: we should not update generic font family settings when
  // document is inactive.
  DCHECK(GetDocument().IsActive());

  if (!font_selector_)
    return;

  font_selector_->UpdateGenericFontFamilySettings(*document_);
  if (resolver_)
    resolver_->InvalidateMatchedPropertiesCache();
  FontCache::GetFontCache()->InvalidateShapeCache();
}

void StyleEngine::RemoveFontFaceRules(
    const HeapVector<Member<const StyleRuleFontFace>>& font_face_rules) {
  if (!font_selector_)
    return;

  FontFaceCache* cache = font_selector_->GetFontFaceCache();
  for (const auto& rule : font_face_rules)
    cache->Remove(rule);
  if (resolver_)
    resolver_->InvalidateMatchedPropertiesCache();
}

void StyleEngine::MarkTreeScopeDirty(TreeScope& scope) {
  if (scope == document_) {
    MarkDocumentDirty();
    return;
  }

  TreeScopeStyleSheetCollection* collection = StyleSheetCollectionFor(scope);
  DCHECK(collection);
  collection->MarkSheetListDirty();
  dirty_tree_scopes_.insert(&scope);
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void StyleEngine::MarkDocumentDirty() {
  document_scope_dirty_ = true;
  document_style_sheet_collection_->MarkSheetListDirty();
  if (RuntimeEnabledFeatures::CSSViewportEnabled())
    ViewportRulesChanged();
  if (GetDocument().ImportLoader())
    GetDocument().MasterDocument().GetStyleEngine().MarkDocumentDirty();
  else
    GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

void StyleEngine::MarkUserStyleDirty() {
  user_style_dirty_ = true;
  GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
}

CSSStyleSheet* StyleEngine::CreateSheet(Element& element,
                                        const String& text,
                                        TextPosition start_position,
                                        StyleEngineContext& context) {
  DCHECK(element.GetDocument() == GetDocument());
  CSSStyleSheet* style_sheet = nullptr;

  AddPendingSheet(context);

  AtomicString text_content(text);

  auto result = text_to_sheet_cache_.insert(text_content, nullptr);
  StyleSheetContents* contents = result.stored_value->value;
  if (result.is_new_entry || !contents ||
      !contents->IsCacheableForStyleElement()) {
    result.stored_value->value = nullptr;
    style_sheet = ParseSheet(element, text, start_position);
    if (style_sheet->Contents()->IsCacheableForStyleElement()) {
      result.stored_value->value = style_sheet->Contents();
      sheet_to_text_cache_.insert(style_sheet->Contents(), text_content);
    }
  } else {
    DCHECK(contents);
    DCHECK(contents->IsCacheableForStyleElement());
    DCHECK(contents->HasSingleOwnerDocument());
    contents->SetIsUsedFromTextCache();
    style_sheet =
        CSSStyleSheet::CreateInline(contents, element, start_position);
  }

  DCHECK(style_sheet);
  if (!element.IsInShadowTree()) {
    String title = element.title();
    if (!title.IsEmpty()) {
      style_sheet->SetTitle(title);
      SetPreferredStylesheetSetNameIfNotSet(title);
    }
  }
  return style_sheet;
}

CSSStyleSheet* StyleEngine::ParseSheet(Element& element,
                                       const String& text,
                                       TextPosition start_position) {
  CSSStyleSheet* style_sheet = nullptr;
  style_sheet = CSSStyleSheet::CreateInline(element, NullURL(), start_position,
                                            GetDocument().Encoding());
  style_sheet->Contents()->ParseStringAtPosition(text, start_position);
  return style_sheet;
}

void StyleEngine::CollectScopedStyleFeaturesTo(RuleFeatureSet& features) const {
  HeapHashSet<Member<const StyleSheetContents>>
      visited_shared_style_sheet_contents;
  if (GetDocument().GetScopedStyleResolver()) {
    GetDocument().GetScopedStyleResolver()->CollectFeaturesTo(
        features, visited_shared_style_sheet_contents);
  }
  for (TreeScope* tree_scope : active_tree_scopes_) {
    if (ScopedStyleResolver* resolver = tree_scope->GetScopedStyleResolver()) {
      resolver->CollectFeaturesTo(features,
                                  visited_shared_style_sheet_contents);
    }
  }
}

void StyleEngine::FontsNeedUpdate(FontSelector*) {
  if (!GetDocument().IsActive())
    return;

  if (resolver_)
    resolver_->InvalidateMatchedPropertiesCache();
  GetDocument().SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(StyleChangeReason::kFonts));
  probe::fontsUpdated(document_);
}

void StyleEngine::SetFontSelector(CSSFontSelector* font_selector) {
  if (font_selector_)
    font_selector_->UnregisterForInvalidationCallbacks(this);
  font_selector_ = font_selector;
  if (font_selector_)
    font_selector_->RegisterForInvalidationCallbacks(this);
}

void StyleEngine::PlatformColorsChanged() {
  if (resolver_)
    resolver_->InvalidateMatchedPropertiesCache();
  GetDocument().SetNeedsStyleRecalc(
      kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                               StyleChangeReason::kPlatformColorChange));
}

bool StyleEngine::ShouldSkipInvalidationFor(const Element& element) const {
  if (GetDocument().GetStyleChangeType() >= kSubtreeStyleChange)
    return true;
  if (!element.InActiveDocument())
    return true;
  if (!element.parentNode())
    return true;
  return element.parentNode()->GetStyleChangeType() >= kSubtreeStyleChange;
}

void StyleEngine::ClassChangedForElement(
    const SpaceSplitString& changed_classes,
    Element& element) {
  if (ShouldSkipInvalidationFor(element))
    return;
  InvalidationLists invalidation_lists;
  unsigned changed_size = changed_classes.size();
  const RuleFeatureSet& features = GetRuleFeatureSet();
  for (unsigned i = 0; i < changed_size; ++i) {
    features.CollectInvalidationSetsForClass(invalidation_lists, element,
                                             changed_classes[i]);
  }
  style_invalidator_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                     element);
}

void StyleEngine::ClassChangedForElement(const SpaceSplitString& old_classes,
                                         const SpaceSplitString& new_classes,
                                         Element& element) {
  if (ShouldSkipInvalidationFor(element))
    return;

  if (!old_classes.size()) {
    ClassChangedForElement(new_classes, element);
    return;
  }

  // Class vectors tend to be very short. This is faster than using a hash
  // table.
  BitVector remaining_class_bits;
  remaining_class_bits.EnsureSize(old_classes.size());

  InvalidationLists invalidation_lists;
  const RuleFeatureSet& features = GetRuleFeatureSet();

  for (unsigned i = 0; i < new_classes.size(); ++i) {
    bool found = false;
    for (unsigned j = 0; j < old_classes.size(); ++j) {
      if (new_classes[i] == old_classes[j]) {
        // Mark each class that is still in the newClasses so we can skip doing
        // an n^2 search below when looking for removals. We can't break from
        // this loop early since a class can appear more than once.
        remaining_class_bits.QuickSet(j);
        found = true;
      }
    }
    // Class was added.
    if (!found) {
      features.CollectInvalidationSetsForClass(invalidation_lists, element,
                                               new_classes[i]);
    }
  }

  for (unsigned i = 0; i < old_classes.size(); ++i) {
    if (remaining_class_bits.QuickGet(i))
      continue;
    // Class was removed.
    features.CollectInvalidationSetsForClass(invalidation_lists, element,
                                             old_classes[i]);
  }

  style_invalidator_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                     element);
}

void StyleEngine::AttributeChangedForElement(
    const QualifiedName& attribute_name,
    Element& element) {
  if (ShouldSkipInvalidationFor(element))
    return;

  InvalidationLists invalidation_lists;
  GetRuleFeatureSet().CollectInvalidationSetsForAttribute(
      invalidation_lists, element, attribute_name);
  style_invalidator_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                     element);
}

void StyleEngine::IdChangedForElement(const AtomicString& old_id,
                                      const AtomicString& new_id,
                                      Element& element) {
  if (ShouldSkipInvalidationFor(element))
    return;

  InvalidationLists invalidation_lists;
  const RuleFeatureSet& features = GetRuleFeatureSet();
  if (!old_id.IsEmpty())
    features.CollectInvalidationSetsForId(invalidation_lists, element, old_id);
  if (!new_id.IsEmpty())
    features.CollectInvalidationSetsForId(invalidation_lists, element, new_id);
  style_invalidator_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                     element);
}

void StyleEngine::PseudoStateChangedForElement(
    CSSSelector::PseudoType pseudo_type,
    Element& element) {
  if (ShouldSkipInvalidationFor(element))
    return;

  InvalidationLists invalidation_lists;
  GetRuleFeatureSet().CollectInvalidationSetsForPseudoClass(
      invalidation_lists, element, pseudo_type);
  style_invalidator_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                     element);
}

void StyleEngine::ScheduleSiblingInvalidationsForElement(
    Element& element,
    ContainerNode& scheduling_parent,
    unsigned min_direct_adjacent) {
  DCHECK(min_direct_adjacent);

  InvalidationLists invalidation_lists;

  const RuleFeatureSet& features = GetRuleFeatureSet();

  if (element.HasID()) {
    features.CollectSiblingInvalidationSetForId(invalidation_lists, element,
                                                element.IdForStyleResolution(),
                                                min_direct_adjacent);
  }

  if (element.HasClass()) {
    const SpaceSplitString& class_names = element.ClassNames();
    for (size_t i = 0; i < class_names.size(); i++) {
      features.CollectSiblingInvalidationSetForClass(
          invalidation_lists, element, class_names[i], min_direct_adjacent);
    }
  }

  for (const Attribute& attribute : element.Attributes()) {
    features.CollectSiblingInvalidationSetForAttribute(
        invalidation_lists, element, attribute.GetName(), min_direct_adjacent);
  }

  features.CollectUniversalSiblingInvalidationSet(invalidation_lists,
                                                  min_direct_adjacent);

  style_invalidator_.ScheduleSiblingInvalidationsAsDescendants(
      invalidation_lists, scheduling_parent);
}

void StyleEngine::ScheduleInvalidationsForInsertedSibling(
    Element* before_element,
    Element& inserted_element) {
  unsigned affected_siblings =
      inserted_element.parentNode()->ChildrenAffectedByIndirectAdjacentRules()
          ? UINT_MAX
          : MaxDirectAdjacentSelectors();

  ContainerNode* scheduling_parent =
      inserted_element.ParentElementOrShadowRoot();
  if (!scheduling_parent)
    return;

  ScheduleSiblingInvalidationsForElement(inserted_element, *scheduling_parent,
                                         1);

  for (unsigned i = 1; before_element && i <= affected_siblings;
       i++, before_element =
                ElementTraversal::PreviousSibling(*before_element)) {
    ScheduleSiblingInvalidationsForElement(*before_element, *scheduling_parent,
                                           i);
  }
}

void StyleEngine::ScheduleInvalidationsForRemovedSibling(
    Element* before_element,
    Element& removed_element,
    Element& after_element) {
  unsigned affected_siblings =
      after_element.parentNode()->ChildrenAffectedByIndirectAdjacentRules()
          ? UINT_MAX
          : MaxDirectAdjacentSelectors();

  ContainerNode* scheduling_parent = after_element.ParentElementOrShadowRoot();
  if (!scheduling_parent)
    return;

  ScheduleSiblingInvalidationsForElement(removed_element, *scheduling_parent,
                                         1);

  for (unsigned i = 1; before_element && i <= affected_siblings;
       i++, before_element =
                ElementTraversal::PreviousSibling(*before_element)) {
    ScheduleSiblingInvalidationsForElement(*before_element, *scheduling_parent,
                                           i);
  }
}

void StyleEngine::ScheduleNthPseudoInvalidations(ContainerNode& nth_parent) {
  InvalidationLists invalidation_lists;
  GetRuleFeatureSet().CollectNthInvalidationSet(invalidation_lists);
  style_invalidator_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                     nth_parent);
}

void StyleEngine::ScheduleRuleSetInvalidationsForElement(
    Element& element,
    const HeapHashSet<Member<RuleSet>>& rule_sets) {
  AtomicString id;
  const SpaceSplitString* class_names = nullptr;

  if (element.HasID())
    id = element.IdForStyleResolution();
  if (element.HasClass())
    class_names = &element.ClassNames();

  InvalidationLists invalidation_lists;
  for (const auto& rule_set : rule_sets) {
    if (!id.IsNull()) {
      rule_set->Features().CollectInvalidationSetsForId(invalidation_lists,
                                                        element, id);
    }
    if (class_names) {
      unsigned class_name_count = class_names->size();
      for (size_t i = 0; i < class_name_count; i++) {
        rule_set->Features().CollectInvalidationSetsForClass(
            invalidation_lists, element, (*class_names)[i]);
      }
    }
    for (const Attribute& attribute : element.Attributes()) {
      rule_set->Features().CollectInvalidationSetsForAttribute(
          invalidation_lists, element, attribute.GetName());
    }
  }
  style_invalidator_.ScheduleInvalidationSetsForNode(invalidation_lists,
                                                     element);
}

void StyleEngine::ScheduleTypeRuleSetInvalidations(
    ContainerNode& node,
    const HeapHashSet<Member<RuleSet>>& rule_sets) {
  InvalidationLists invalidation_lists;
  for (const auto& rule_set : rule_sets) {
    rule_set->Features().CollectTypeRuleInvalidationSet(invalidation_lists,
                                                        node);
  }
  DCHECK(invalidation_lists.siblings.IsEmpty());
  style_invalidator_.ScheduleInvalidationSetsForNode(invalidation_lists, node);

  if (!node.IsShadowRoot())
    return;

  Element& host = ToShadowRoot(node).host();
  if (host.NeedsStyleRecalc())
    return;

  for (auto& invalidation_set : invalidation_lists.descendants) {
    if (invalidation_set->InvalidatesTagName(host)) {
      host.SetNeedsStyleRecalc(kLocalStyleChange,
                               StyleChangeReasonForTracing::Create(
                                   StyleChangeReason::kStyleSheetChange));
      return;
    }
  }
}

void StyleEngine::InvalidateSlottedElements(HTMLSlotElement& slot) {
  for (auto& node : slot.GetDistributedNodes()) {
    if (node->IsElementNode()) {
      node->SetNeedsStyleRecalc(kLocalStyleChange,
                                StyleChangeReasonForTracing::Create(
                                    StyleChangeReason::kStyleSheetChange));
    }
  }
}

void StyleEngine::ScheduleInvalidationsForRuleSets(
    TreeScope& tree_scope,
    const HeapHashSet<Member<RuleSet>>& rule_sets,
    InvalidationScope invalidation_scope) {
#if DCHECK_IS_ON()
  // Full scope recalcs should be handled while collecting the ruleSets before
  // calling this method.
  for (auto rule_set : rule_sets)
    DCHECK(!rule_set->Features().NeedsFullRecalcForRuleSetInvalidation());
#endif  // DCHECK_IS_ON()

  TRACE_EVENT0("blink,blink_style",
               "StyleEngine::scheduleInvalidationsForRuleSets");

  ScheduleTypeRuleSetInvalidations(tree_scope.RootNode(), rule_sets);

  bool invalidate_slotted = false;
  if (tree_scope.RootNode().IsShadowRoot()) {
    Element& host = ToShadowRoot(tree_scope.RootNode()).host();
    ScheduleRuleSetInvalidationsForElement(host, rule_sets);
    if (host.GetStyleChangeType() >= kSubtreeStyleChange)
      return;
    for (auto rule_set : rule_sets) {
      if (rule_set->HasSlottedRules()) {
        invalidate_slotted = true;
        break;
      }
    }
  }

  Node* stay_within = &tree_scope.RootNode();
  Element* element = ElementTraversal::FirstChild(*stay_within);
  while (element) {
    ScheduleRuleSetInvalidationsForElement(*element, rule_sets);
    if (invalidate_slotted && IsHTMLSlotElement(element))
      InvalidateSlottedElements(ToHTMLSlotElement(*element));

    if (invalidation_scope == kInvalidateAllScopes) {
      ElementShadow* shadow = element->Shadow();
      ShadowRoot* shadow_root = shadow ? &shadow->OldestShadowRoot() : nullptr;
      while (shadow_root) {
        ScheduleInvalidationsForRuleSets(*shadow_root, rule_sets,
                                         kInvalidateAllScopes);
        shadow_root = shadow_root->YoungerShadowRoot();
      }
    }

    if (element->GetStyleChangeType() < kSubtreeStyleChange)
      element = ElementTraversal::Next(*element, stay_within);
    else
      element = ElementTraversal::NextSkippingChildren(*element, stay_within);
  }
}

void StyleEngine::SetStatsEnabled(bool enabled) {
  if (!enabled) {
    style_resolver_stats_ = nullptr;
    return;
  }
  if (!style_resolver_stats_)
    style_resolver_stats_ = StyleResolverStats::Create();
  else
    style_resolver_stats_->Reset();
}

void StyleEngine::SetPreferredStylesheetSetNameIfNotSet(const String& name) {
  DCHECK(!name.IsEmpty());
  if (!preferred_stylesheet_set_name_.IsEmpty())
    return;
  preferred_stylesheet_set_name_ = name;
  // TODO(rune@opera.com): Setting the selected set here is wrong if the set
  // has been previously set by through Document.selectedStylesheetSet. Our
  // current implementation ignores the effect of Document.selectedStylesheetSet
  // and either only collects persistent style, or additionally preferred
  // style when present.
  selected_stylesheet_set_name_ = name;
  MarkDocumentDirty();
}

void StyleEngine::SetSelectedStylesheetSetName(const String& name) {
  selected_stylesheet_set_name_ = name;
  // TODO(rune@opera.com): Setting Document.selectedStylesheetSet currently
  // has no other effect than the ability to read back the set value using
  // the same api. If it did have an effect, we should have marked the
  // document scope dirty and triggered an update of the active stylesheets
  // from here.
}

void StyleEngine::SetHttpDefaultStyle(const String& content) {
  if (!content.IsEmpty())
    SetPreferredStylesheetSetNameIfNotSet(content);
}

void StyleEngine::EnsureUAStyleForFullscreen() {
  DCHECK(IsMaster());
  DCHECK(global_rule_set_);
  if (global_rule_set_->HasFullscreenUAStyle())
    return;
  CSSDefaultStyleSheets::Instance().EnsureDefaultStyleSheetForFullscreen();
  global_rule_set_->MarkDirty();
  UpdateActiveStyle();
}

void StyleEngine::EnsureUAStyleForElement(const Element& element) {
  DCHECK(IsMaster());
  DCHECK(global_rule_set_);
  if (CSSDefaultStyleSheets::Instance().EnsureDefaultStyleSheetsForElement(
          element)) {
    global_rule_set_->MarkDirty();
    UpdateActiveStyle();
  }
}

bool StyleEngine::HasRulesForId(const AtomicString& id) const {
  DCHECK(IsMaster());
  DCHECK(global_rule_set_);
  return global_rule_set_->GetRuleFeatureSet().HasSelectorForId(id);
}

void StyleEngine::InitialViewportChanged() {
  if (viewport_resolver_)
    viewport_resolver_->InitialViewportChanged();
}

void StyleEngine::ViewportRulesChanged() {
  if (viewport_resolver_)
    viewport_resolver_->SetNeedsCollectRules();
}

void StyleEngine::HtmlImportAddedOrRemoved() {
  if (GetDocument().ImportLoader()) {
    GetDocument().MasterDocument().GetStyleEngine().HtmlImportAddedOrRemoved();
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
  if (ScopedStyleResolver* resolver = GetDocument().GetScopedStyleResolver()) {
    MarkDocumentDirty();
    resolver->SetNeedsAppendAllSheets();
    GetDocument().SetNeedsStyleRecalc(
        kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                                 StyleChangeReason::kActiveStylesheetsUpdate));
  }
}

namespace {

enum RuleSetFlags {
  kFontFaceRules = 1 << 0,
  kKeyframesRules = 1 << 1,
  kFullRecalcRules = 1 << 2
};

unsigned GetRuleSetFlags(const HeapHashSet<Member<RuleSet>> rule_sets) {
  unsigned flags = 0;
  for (auto& rule_set : rule_sets) {
    rule_set->CompactRulesIfNeeded();
    if (!rule_set->KeyframesRules().IsEmpty())
      flags |= kKeyframesRules;
    if (!rule_set->FontFaceRules().IsEmpty())
      flags |= kFontFaceRules;
    if (rule_set->NeedsFullRecalcForRuleSetInvalidation())
      flags |= kFullRecalcRules;
  }
  return flags;
}

}  // namespace

void StyleEngine::ApplyRuleSetChanges(
    TreeScope& tree_scope,
    const ActiveStyleSheetVector& old_style_sheets,
    const ActiveStyleSheetVector& new_style_sheets,
    InvalidationScope invalidation_scope) {
  DCHECK(IsMaster());
  DCHECK(global_rule_set_);
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  ActiveSheetsChange change = CompareActiveStyleSheets(
      old_style_sheets, new_style_sheets, changed_rule_sets);
  bool append_all_sheets = false;

  if (invalidation_scope == kInvalidateCurrentScope) {
    if (ScopedStyleResolver* scoped_resolver =
            tree_scope.GetScopedStyleResolver())
      append_all_sheets = scoped_resolver->NeedsAppendAllSheets();

    if (change == kNoActiveSheetsChanged && !append_all_sheets)
      return;
  }

  // With rules added or removed, we need to re-aggregate rule meta data.
  global_rule_set_->MarkDirty();

  unsigned changed_rule_flags = GetRuleSetFlags(changed_rule_sets);
  bool fonts_changed = tree_scope.RootNode().IsDocumentNode() &&
                       (changed_rule_flags & kFontFaceRules);
  unsigned append_start_index = 0;

  // We don't need to clear the font cache if new sheets are appended.
  if (fonts_changed && change == kActiveSheetsChanged)
    ClearFontCache();

  if (invalidation_scope == kInvalidateCurrentScope) {
    // - If all sheets were removed, we remove the ScopedStyleResolver.
    // - If new sheets were appended to existing ones, start appending after the
    //   common prefix.
    // - For other diffs, reset author style and re-add all sheets for the
    //   TreeScope.
    if (tree_scope.GetScopedStyleResolver()) {
      if (new_style_sheets.IsEmpty())
        ResetAuthorStyle(tree_scope);
      else if (change == kActiveSheetsAppended && !append_all_sheets)
        append_start_index = old_style_sheets.size();
      else
        tree_scope.GetScopedStyleResolver()->ResetAuthorStyle();
    }

    if (!new_style_sheets.IsEmpty()) {
      tree_scope.EnsureScopedStyleResolver().AppendActiveStyleSheets(
          append_start_index, new_style_sheets);
    }
  }

  if (tree_scope.GetDocument().HasPendingForcedStyleRecalc())
    return;

  if (!tree_scope.GetDocument().body() ||
      tree_scope.GetDocument().HasNodesWithPlaceholderStyle()) {
    tree_scope.GetDocument().SetNeedsStyleRecalc(
        kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                                 StyleChangeReason::kCleanupPlaceholderStyles));
    return;
  }

  if (changed_rule_sets.IsEmpty())
    return;

  if (changed_rule_flags & kKeyframesRules)
    ScopedStyleResolver::KeyframesRulesAdded(tree_scope);

  Node& invalidation_root =
      ScopedStyleResolver::InvalidationRootForTreeScope(tree_scope);
  if (invalidation_root.GetStyleChangeType() >= kSubtreeStyleChange)
    return;

  if (fonts_changed || (changed_rule_flags & kFullRecalcRules)) {
    invalidation_root.SetNeedsStyleRecalc(
        kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                                 StyleChangeReason::kActiveStylesheetsUpdate));
    return;
  }

  ScheduleInvalidationsForRuleSets(tree_scope, changed_rule_sets,
                                   invalidation_scope);
}

const MediaQueryEvaluator& StyleEngine::EnsureMediaQueryEvaluator() {
  if (!media_query_evaluator_) {
    if (GetDocument().GetFrame()) {
      media_query_evaluator_ =
          new MediaQueryEvaluator(GetDocument().GetFrame());
    } else {
      media_query_evaluator_ = new MediaQueryEvaluator("all");
    }
  }
  return *media_query_evaluator_;
}

bool StyleEngine::MediaQueryAffectedByViewportChange() {
  DCHECK(IsMaster());
  DCHECK(global_rule_set_);
  const MediaQueryEvaluator& evaluator = EnsureMediaQueryEvaluator();
  const auto& results = global_rule_set_->GetRuleFeatureSet()
                            .ViewportDependentMediaQueryResults();
  for (unsigned i = 0; i < results.size(); ++i) {
    if (evaluator.Eval(results[i].Expression()) != results[i].Result())
      return true;
  }
  return false;
}

bool StyleEngine::MediaQueryAffectedByDeviceChange() {
  DCHECK(IsMaster());
  DCHECK(global_rule_set_);
  const MediaQueryEvaluator& evaluator = EnsureMediaQueryEvaluator();
  const auto& results =
      global_rule_set_->GetRuleFeatureSet().DeviceDependentMediaQueryResults();
  for (unsigned i = 0; i < results.size(); ++i) {
    if (evaluator.Eval(results[i].Expression()) != results[i].Result())
      return true;
  }
  return false;
}

bool StyleEngine::UpdateRemUnits(const ComputedStyle* old_root_style,
                                 const ComputedStyle* new_root_style) {
  if (!UsesRemUnits())
    return false;
  if (!old_root_style ||
      old_root_style->FontSize() != new_root_style->FontSize()) {
    DCHECK(Resolver());
    // Resolved rem units are stored in the matched properties cache so we need
    // to make sure to invalidate the cache if the documentElement font size
    // changes.
    Resolver()->InvalidateMatchedPropertiesCache();
    return true;
  }
  return false;
}

void StyleEngine::CustomPropertyRegistered() {
  // TODO(timloh): Invalidate only elements with this custom property set
  GetDocument().SetNeedsStyleRecalc(
      kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                               StyleChangeReason::kPropertyRegistration));
  if (resolver_)
    resolver_->InvalidateMatchedPropertiesCache();
}

void StyleEngine::MarkForWhitespaceReattachment() {
  for (auto element : whitespace_reattach_set_) {
    if (!element->GetLayoutObject())
      continue;
    element->SetChildNeedsReattachLayoutTree();
    element->MarkAncestorsWithChildNeedsReattachLayoutTree();
  }
}

void StyleEngine::NodeWillBeRemoved(Node& node) {
  if (node.IsElementNode()) {
    style_invalidator_.RescheduleSiblingInvalidationsAsDescendants(
        ToElement(node));
  }

  // Mark closest ancestor with with LayoutObject to have all whitespace
  // children being considered for re-attachment during the layout tree build.

  LayoutObject* layout_object = node.GetLayoutObject();
  // The removed node does not have a layout object. No sibling whitespace nodes
  // will change rendering.
  if (!layout_object)
    return;
  // Floating or out-of-flow elements do not affect whitespace siblings.
  if (layout_object->IsFloatingOrOutOfFlowPositioned())
    return;
  layout_object = layout_object->Parent();
  while (layout_object->IsAnonymous())
    layout_object = layout_object->Parent();
  DCHECK(layout_object);
  DCHECK(layout_object->GetNode());
  if (layout_object->GetNode()->IsElementNode()) {
    whitespace_reattach_set_.insert(ToElement(layout_object->GetNode()));
    GetDocument().ScheduleLayoutTreeUpdateIfNeeded();
  }
}

void StyleEngine::CollectMatchingUserRules(
    ElementRuleCollector& collector) const {
  for (unsigned i = 0; i < active_user_style_sheets_.size(); ++i) {
    DCHECK(active_user_style_sheets_[i].second);
    collector.CollectMatchingRules(
        MatchRequest(active_user_style_sheets_[i].second, nullptr,
                     active_user_style_sheets_[i].first, i));
  }
}

void StyleEngine::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(user_style_sheets_);
  visitor->Trace(active_user_style_sheets_);
  visitor->Trace(inspector_style_sheet_);
  visitor->Trace(document_style_sheet_collection_);
  visitor->Trace(style_sheet_collection_map_);
  visitor->Trace(dirty_tree_scopes_);
  visitor->Trace(active_tree_scopes_);
  visitor->Trace(tree_boundary_crossing_scopes_);
  visitor->Trace(resolver_);
  visitor->Trace(viewport_resolver_);
  visitor->Trace(media_query_evaluator_);
  visitor->Trace(global_rule_set_);
  visitor->Trace(style_invalidator_);
  visitor->Trace(whitespace_reattach_set_);
  visitor->Trace(font_selector_);
  visitor->Trace(text_to_sheet_cache_);
  visitor->Trace(sheet_to_text_cache_);
  visitor->Trace(tracker_);
  FontSelectorClient::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(StyleEngine) {
  for (const auto& sheet : user_style_sheets_) {
    visitor->TraceWrappers(sheet.second);
  }
  visitor->TraceWrappers(document_style_sheet_collection_);
}

}  // namespace blink
