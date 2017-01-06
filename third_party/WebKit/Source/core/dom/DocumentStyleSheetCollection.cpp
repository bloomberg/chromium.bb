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

#include "core/dom/DocumentStyleSheetCollection.h"

#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentStyleSheetCollector.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/StyleSheetCandidate.h"

namespace blink {

DocumentStyleSheetCollection::DocumentStyleSheetCollection(TreeScope& treeScope)
    : TreeScopeStyleSheetCollection(treeScope) {
  DCHECK_EQ(treeScope.rootNode(), treeScope.rootNode().document());
}

void DocumentStyleSheetCollection::collectStyleSheetsFromCandidates(
    StyleEngine& masterEngine,
    DocumentStyleSheetCollector& collector) {
  for (Node* n : m_styleSheetCandidateNodes) {
    StyleSheetCandidate candidate(*n);

    DCHECK(!candidate.isXSL());
    if (candidate.isImport()) {
      Document* document = candidate.importedDocument();
      if (!document)
        continue;
      if (collector.hasVisited(document))
        continue;
      collector.willVisit(document);

      document->styleEngine().updateStyleSheetsInImport(masterEngine,
                                                        collector);
      continue;
    }

    if (candidate.isEnabledAndLoading())
      continue;

    StyleSheet* sheet = candidate.sheet();
    if (!sheet)
      continue;

    collector.appendSheetForList(sheet);
    if (!candidate.canBeActivated(
            document().styleEngine().preferredStylesheetSetName()))
      continue;

    CSSStyleSheet* cssSheet = toCSSStyleSheet(sheet);
    collector.appendActiveStyleSheet(
        std::make_pair(cssSheet, masterEngine.ruleSetForSheet(*cssSheet)));
  }
}

void DocumentStyleSheetCollection::collectStyleSheets(
    StyleEngine& masterEngine,
    DocumentStyleSheetCollector& collector) {
  for (auto& sheet : document().styleEngine().injectedAuthorStyleSheets()) {
    collector.appendActiveStyleSheet(std::make_pair(
        sheet, document().styleEngine().ruleSetForSheet(*sheet)));
  }
  collectStyleSheetsFromCandidates(masterEngine, collector);
  if (CSSStyleSheet* inspectorSheet =
          document().styleEngine().inspectorStyleSheet()) {
    collector.appendActiveStyleSheet(std::make_pair(
        inspectorSheet,
        document().styleEngine().ruleSetForSheet(*inspectorSheet)));
  }
}

void DocumentStyleSheetCollection::updateActiveStyleSheets(
    StyleEngine& masterEngine) {
  // StyleSheetCollection is GarbageCollected<>, allocate it on the heap.
  StyleSheetCollection* collection = StyleSheetCollection::create();
  ActiveDocumentStyleSheetCollector collector(*collection);
  collectStyleSheets(masterEngine, collector);
  applyActiveStyleSheetChanges(*collection);
}

void DocumentStyleSheetCollection::collectViewportRules(
    ViewportStyleResolver& viewportResolver) {
  for (Node* node : m_styleSheetCandidateNodes) {
    StyleSheetCandidate candidate(*node);

    if (candidate.isImport())
      continue;
    StyleSheet* sheet = candidate.sheet();
    if (!sheet)
      continue;
    if (!candidate.canBeActivated(
            document().styleEngine().preferredStylesheetSetName()))
      continue;
    viewportResolver.collectViewportRulesFromAuthorSheet(
        *toCSSStyleSheet(sheet));
  }
}

}  // namespace blink
