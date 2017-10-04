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

#include "core/css/ShadowTreeStyleSheetCollection.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleChangeReason.h"
#include "core/css/StyleEngine.h"
#include "core/css/StyleSheetCandidate.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"
#include "core/dom/ShadowRoot.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html_names.h"

namespace blink {

using namespace HTMLNames;

ShadowTreeStyleSheetCollection::ShadowTreeStyleSheetCollection(
    ShadowRoot& shadow_root)
    : TreeScopeStyleSheetCollection(shadow_root) {}

void ShadowTreeStyleSheetCollection::CollectStyleSheets(
    StyleEngine& master_engine,
    StyleSheetCollection& collection) {
  for (Node* n : style_sheet_candidate_nodes_) {
    StyleSheetCandidate candidate(*n);
    DCHECK(!candidate.IsXSL());

    StyleSheet* sheet = candidate.Sheet();
    if (!sheet)
      continue;

    collection.AppendSheetForList(sheet);
    if (candidate.CanBeActivated(g_null_atom)) {
      CSSStyleSheet* css_sheet = ToCSSStyleSheet(sheet);
      collection.AppendActiveStyleSheet(
          std::make_pair(css_sheet, master_engine.RuleSetForSheet(*css_sheet)));
    }
  }
}

void ShadowTreeStyleSheetCollection::UpdateActiveStyleSheets(
    StyleEngine& master_engine) {
  // StyleSheetCollection is GarbageCollected<>, allocate it on the heap.
  StyleSheetCollection* collection = StyleSheetCollection::Create();
  CollectStyleSheets(master_engine, *collection);
  ApplyActiveStyleSheetChanges(*collection);
}

}  // namespace blink
