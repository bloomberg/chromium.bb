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
#include "core/dom/DocumentStyleSheetCollection.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleInvalidationAnalysis.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ProcessingInstruction.h"
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

DocumentStyleSheetCollection::DocumentStyleSheetCollection(Document* document)
    : m_document(document)
    , m_pendingStylesheets(0)
    , m_injectedStyleSheetCacheValid(false)
    , m_needsUpdateActiveStylesheetsOnStyleRecalc(false)
    , m_usesSiblingRules(false)
    , m_usesSiblingRulesOverride(false)
    , m_usesFirstLineRules(false)
    , m_usesFirstLetterRules(false)
    , m_usesBeforeAfterRules(false)
    , m_usesBeforeAfterRulesOverride(false)
    , m_usesRemUnits(false)
    , m_collectionForDocument(document)
{
}

DocumentStyleSheetCollection::~DocumentStyleSheetCollection()
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

const Vector<RefPtr<StyleSheet> >& DocumentStyleSheetCollection::styleSheetsForStyleSheetList()
{
    return m_collectionForDocument.styleSheetsForStyleSheetList();
}

const Vector<RefPtr<CSSStyleSheet> >& DocumentStyleSheetCollection::activeAuthorStyleSheets() const
{
    return m_collectionForDocument.activeAuthorStyleSheets();
}

void DocumentStyleSheetCollection::combineCSSFeatureFlags(const RuleFeatureSet& features)
{
    // Delay resetting the flags until after next style recalc since unapplying the style may not work without these set (this is true at least with before/after).
    m_usesSiblingRules = m_usesSiblingRules || features.usesSiblingRules();
    m_usesFirstLineRules = m_usesFirstLineRules || features.usesFirstLineRules();
    m_usesBeforeAfterRules = m_usesBeforeAfterRules || features.usesBeforeAfterRules();
}

void DocumentStyleSheetCollection::resetCSSFeatureFlags(const RuleFeatureSet& features)
{
    m_usesSiblingRules = features.usesSiblingRules();
    m_usesFirstLineRules = features.usesFirstLineRules();
    m_usesBeforeAfterRules = features.usesBeforeAfterRules();
}

CSSStyleSheet* DocumentStyleSheetCollection::pageUserSheet()
{
    if (m_pageUserSheet)
        return m_pageUserSheet.get();

    Page* owningPage = m_document->page();
    if (!owningPage)
        return 0;

    String userSheetText = owningPage->userStyleSheet();
    if (userSheetText.isEmpty())
        return 0;

    // Parse the sheet and cache it.
    m_pageUserSheet = CSSStyleSheet::createInline(m_document, m_document->settings()->userStyleSheetLocation());
    m_pageUserSheet->contents()->setIsUserStyleSheet(true);
    m_pageUserSheet->contents()->parseString(userSheetText);
    return m_pageUserSheet.get();
}

void DocumentStyleSheetCollection::clearPageUserSheet()
{
    if (m_pageUserSheet) {
        RefPtr<StyleSheet> removedSheet = m_pageUserSheet;
        m_pageUserSheet = 0;
        m_document->removedStyleSheet(removedSheet.get());
    }
}

void DocumentStyleSheetCollection::updatePageUserSheet()
{
    clearPageUserSheet();
    // FIXME: Why is this immediately and not defer?
    if (StyleSheet* addedSheet = pageUserSheet())
        m_document->addedStyleSheet(addedSheet, RecalcStyleImmediately);
}

const Vector<RefPtr<CSSStyleSheet> >& DocumentStyleSheetCollection::injectedUserStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedUserStyleSheets;
}

const Vector<RefPtr<CSSStyleSheet> >& DocumentStyleSheetCollection::injectedAuthorStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedAuthorStyleSheets;
}

void DocumentStyleSheetCollection::updateInjectedStyleSheetCache() const
{
    if (m_injectedStyleSheetCacheValid)
        return;
    m_injectedStyleSheetCacheValid = true;
    m_injectedUserStyleSheets.clear();
    m_injectedAuthorStyleSheets.clear();

    Page* owningPage = m_document->page();
    if (!owningPage)
        return;

    const PageGroup& pageGroup = owningPage->group();
    const UserStyleSheetVector& sheets = pageGroup.userStyleSheets();
    for (unsigned i = 0; i < sheets.size(); ++i) {
        const UserStyleSheet* sheet = sheets[i].get();
        if (sheet->injectedFrames() == InjectInTopFrameOnly && m_document->ownerElement())
            continue;
        if (!UserContentURLPattern::matchesPatterns(m_document->url(), sheet->whitelist(), sheet->blacklist()))
            continue;
        RefPtr<CSSStyleSheet> groupSheet = CSSStyleSheet::createInline(const_cast<Document*>(m_document), sheet->url());
        bool isUserStyleSheet = sheet->level() == UserStyleUserLevel;
        if (isUserStyleSheet)
            m_injectedUserStyleSheets.append(groupSheet);
        else
            m_injectedAuthorStyleSheets.append(groupSheet);
        groupSheet->contents()->setIsUserStyleSheet(isUserStyleSheet);
        groupSheet->contents()->parseString(sheet->source());
    }
}

void DocumentStyleSheetCollection::invalidateInjectedStyleSheetCache()
{
    m_injectedStyleSheetCacheValid = false;
    // FIXME: updateInjectedStyleSheetCache is called inside StyleSheetCollection::updateActiveStyleSheets
    // and batch updates lots of sheets so we can't call addedStyleSheet() or removedStyleSheet().
    m_document->styleResolverChanged(DeferRecalcStyle);
}

void DocumentStyleSheetCollection::addAuthorSheet(PassRefPtr<StyleSheetContents> authorSheet)
{
    ASSERT(!authorSheet->isUserStyleSheet());
    m_authorStyleSheets.append(CSSStyleSheet::create(authorSheet, m_document));
    m_document->addedStyleSheet(m_authorStyleSheets.last().get(), RecalcStyleImmediately);
}

void DocumentStyleSheetCollection::addUserSheet(PassRefPtr<StyleSheetContents> userSheet)
{
    ASSERT(userSheet->isUserStyleSheet());
    m_userStyleSheets.append(CSSStyleSheet::create(userSheet, m_document));
    m_document->addedStyleSheet(m_userStyleSheets.last().get(), RecalcStyleImmediately);
}

// This method is called whenever a top-level stylesheet has finished loading.
void DocumentStyleSheetCollection::removePendingSheet(RemovePendingSheetNotificationType notification)
{
    // Make sure we knew this sheet was pending, and that our count isn't out of sync.
    ASSERT(m_pendingStylesheets > 0);

    m_pendingStylesheets--;

    if (m_pendingStylesheets)
        return;

    if (notification == RemovePendingSheetNotifyLater) {
        m_document->setNeedsNotifyRemoveAllPendingStylesheet();
        return;
    }

    // FIXME: We can't call addedStyleSheet or removedStyleSheet here because we don't know
    // what's new. We should track that to tell the style system what changed.
    m_document->didRemoveAllPendingStylesheet();
}

void DocumentStyleSheetCollection::addStyleSheetCandidateNode(Node* node, bool createdByParser)
{
    m_collectionForDocument.addStyleSheetCandidateNode(node, createdByParser);
}

void DocumentStyleSheetCollection::removeStyleSheetCandidateNode(Node* node, ContainerNode* scopingNode)
{
    m_collectionForDocument.removeStyleSheetCandidateNode(node, scopingNode);
}

static bool styleSheetsUseRemUnits(const Vector<RefPtr<CSSStyleSheet> >& sheets)
{
    for (unsigned i = 0; i < sheets.size(); ++i) {
        if (sheets[i]->contents()->usesRemUnits())
            return true;
    }
    return false;
}

bool DocumentStyleSheetCollection::updateActiveStyleSheets(StyleResolverUpdateMode updateMode)
{
    if (m_document->inStyleRecalc()) {
        // SVG <use> element may manage to invalidate style selector in the middle of a style recalc.
        // https://bugs.webkit.org/show_bug.cgi?id=54344
        // FIXME: This should be fixed in SVG and the call site replaced by ASSERT(!m_inStyleRecalc).
        m_needsUpdateActiveStylesheetsOnStyleRecalc = true;
        return false;

    }
    if (!m_document->renderer() || !m_document->attached())
        return false;

    StyleSheetCollection::StyleResolverUpdateType styleResolverUpdateType;
    bool requiresFullStyleRecalc = m_collectionForDocument.updateActiveStyleSheets(this, updateMode, styleResolverUpdateType);
    m_needsUpdateActiveStylesheetsOnStyleRecalc = false;

    if (styleResolverUpdateType != StyleSheetCollection::Reconstruct)
        resetCSSFeatureFlags(m_document->styleResolver()->ruleFeatureSet());

    InspectorInstrumentation::activeStyleSheetsUpdated(m_document, m_collectionForDocument.styleSheetsForStyleSheetList());
    m_usesRemUnits = styleSheetsUseRemUnits(m_collectionForDocument.activeAuthorStyleSheets());
    m_document->notifySeamlessChildDocumentsOfStylesheetUpdate();

    return requiresFullStyleRecalc;
}

}
