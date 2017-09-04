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

#include "core/css/TreeScopeStyleSheetCollection.h"

#include "core/css/ActiveStyleSheets.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleEngine.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetCandidate.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLStyleElement.h"

namespace blink {

TreeScopeStyleSheetCollection::TreeScopeStyleSheetCollection(
    TreeScope& tree_scope)
    : tree_scope_(tree_scope) {}

void TreeScopeStyleSheetCollection::AddStyleSheetCandidateNode(Node& node) {
  if (node.isConnected())
    style_sheet_candidate_nodes_.Add(&node);
}

bool TreeScopeStyleSheetCollection::MediaQueryAffectingValueChanged() {
  bool needs_active_style_update = false;
  for (const auto& active_sheet : active_author_style_sheets_) {
    if (active_sheet.first->MediaQueries())
      needs_active_style_update = true;
    StyleSheetContents* contents = active_sheet.first->Contents();
    if (contents->HasMediaQueries())
      contents->ClearRuleSet();
  }
  return needs_active_style_update;
}

void TreeScopeStyleSheetCollection::ApplyActiveStyleSheetChanges(
    StyleSheetCollection& new_collection) {
  GetDocument().GetStyleEngine().ApplyRuleSetChanges(
      GetTreeScope(), ActiveAuthorStyleSheets(),
      new_collection.ActiveAuthorStyleSheets());
  new_collection.Swap(*this);
}

bool TreeScopeStyleSheetCollection::HasStyleSheets() const {
  for (Node* node : style_sheet_candidate_nodes_) {
    StyleSheetCandidate candidate(*node);
    if (candidate.Sheet() || candidate.IsEnabledAndLoading())
      return true;
  }
  return false;
}

void TreeScopeStyleSheetCollection::UpdateStyleSheetList() {
  if (!sheet_list_dirty_)
    return;

  HeapVector<Member<StyleSheet>> new_list;
  for (Node* node : style_sheet_candidate_nodes_) {
    StyleSheetCandidate candidate(*node);
    DCHECK(!candidate.IsXSL());
    if (candidate.IsImport())
      continue;
    if (candidate.IsEnabledAndLoading())
      continue;
    if (StyleSheet* sheet = candidate.Sheet())
      new_list.push_back(sheet);
  }
  SwapSheetsForSheetList(new_list);
}

DEFINE_TRACE(TreeScopeStyleSheetCollection) {
  visitor->Trace(tree_scope_);
  visitor->Trace(style_sheet_candidate_nodes_);
  StyleSheetCollection::Trace(visitor);
}

}  // namespace blink
