/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentOrderedList.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "heap/Handle.h"
#include "wtf/FastAllocBase.h"
#include "wtf/ListHashSet.h"
#include "wtf/RefPtr.h"
#include "wtf/TemporaryChange.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class CSSFontSelector;
class CSSStyleSheet;
class FontSelector;
class Node;
class RuleFeatureSet;
class ShadowTreeStyleSheetCollection;
class StyleResolver;
class StyleRuleFontFace;
class StyleSheet;
class StyleSheetCollection;
class StyleSheetContents;
class StyleSheetList;

class StyleResolverChange {
public:
    StyleResolverChange()
        : m_needsRepaint(false)
        , m_needsStyleRecalc(false)
    { }

    bool needsRepaint() const { return m_needsRepaint; }
    bool needsStyleRecalc() const { return m_needsStyleRecalc; }
    void setNeedsRepaint() { m_needsRepaint = true; }
    void setNeedsStyleRecalc() { m_needsStyleRecalc = true; }

private:
    bool m_needsRepaint;
    bool m_needsStyleRecalc;
};

class StyleEngine : public NoBaseWillBeGarbageCollectedFinalized<StyleEngine> {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:

    class IgnoringPendingStylesheet : public TemporaryChange<bool> {
    public:
        IgnoringPendingStylesheet(StyleEngine* engine)
            : TemporaryChange<bool>(engine->m_ignorePendingStylesheets, true)
        {
        }
    };

    friend class IgnoringPendingStylesheet;

    static PassOwnPtrWillBeRawPtr<StyleEngine> create(Document& document) { return adoptPtrWillBeNoop(new StyleEngine(document)); }

    ~StyleEngine();

    void detachFromDocument();

    const WillBeHeapVector<RefPtrWillBeMember<StyleSheet> >& styleSheetsForStyleSheetList(TreeScope&);
    const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >& activeAuthorStyleSheets() const;

    const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >& documentAuthorStyleSheets() const { return m_authorStyleSheets; }
    const WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> >& injectedAuthorStyleSheets() const;

    const WillBeHeapVector<RefPtrWillBeMember<StyleSheet> > activeStyleSheetsForInspector() const;

    void modifiedStyleSheet(StyleSheet*);
    void addStyleSheetCandidateNode(Node*, bool createdByParser);
    void removeStyleSheetCandidateNode(Node*);
    void removeStyleSheetCandidateNode(Node*, ContainerNode* scopingNode, TreeScope&);
    void modifiedStyleSheetCandidateNode(Node*);

    void invalidateInjectedStyleSheetCache();
    void updateInjectedStyleSheetCache() const;

    void addAuthorSheet(PassRefPtrWillBeRawPtr<StyleSheetContents> authorSheet);

    void clearMediaQueryRuleSetStyleSheets();
    void updateStyleSheetsInImport(DocumentStyleSheetCollector& parentCollector);
    bool updateActiveStyleSheets(StyleResolverUpdateMode);

    String preferredStylesheetSetName() const { return m_preferredStylesheetSetName; }
    String selectedStylesheetSetName() const { return m_selectedStylesheetSetName; }
    void setPreferredStylesheetSetName(const String& name) { m_preferredStylesheetSetName = name; }
    void setSelectedStylesheetSetName(const String& name) { m_selectedStylesheetSetName = name; }

    void selectStylesheetSetName(const String& name)
    {
        setPreferredStylesheetSetName(name);
        setSelectedStylesheetSetName(name);
    }

    void addPendingSheet();
    enum RemovePendingSheetNotificationType {
        RemovePendingSheetNotifyImmediately,
        RemovePendingSheetNotifyLater
    };
    void removePendingSheet(Node* styleSheetCandidateNode, RemovePendingSheetNotificationType = RemovePendingSheetNotifyImmediately);

    bool hasPendingSheets() const { return m_pendingStylesheets > 0; }
    bool haveStylesheetsLoaded() const { return !hasPendingSheets() || m_ignorePendingStylesheets; }
    bool ignoringPendingStylesheets() const { return m_ignorePendingStylesheets; }

    unsigned maxDirectAdjacentSelectors() const { return m_maxDirectAdjacentSelectors; }
    bool usesSiblingRules() const { return m_usesSiblingRules || m_usesSiblingRulesOverride; }
    void setUsesSiblingRulesOverride(bool b) { m_usesSiblingRulesOverride = b; }
    bool usesFirstLineRules() const { return m_usesFirstLineRules; }
    bool usesFirstLetterRules() const { return m_usesFirstLetterRules; }
    void setUsesFirstLetterRules(bool b) { m_usesFirstLetterRules = b; }
    bool usesRemUnits() const { return m_usesRemUnits; }
    void setUsesRemUnit(bool b) { m_usesRemUnits = b; }
    bool hasScopedStyleSheet() { return m_documentStyleSheetCollection.scopingNodesForStyleScoped(); }

    void combineCSSFeatureFlags(const RuleFeatureSet&);
    void resetCSSFeatureFlags(const RuleFeatureSet&);

    void didRemoveShadowRoot(ShadowRoot*);
    void appendActiveAuthorStyleSheets();

    StyleResolver* resolver() const
    {
        return m_resolver.get();
    }

    StyleResolver& ensureResolver()
    {
        if (!m_resolver) {
            createResolver();
        } else if (m_resolver->hasPendingAuthorStyleSheets()) {
            m_resolver->appendPendingAuthorStyleSheets();
        }
        return *m_resolver.get();
    }

    bool hasResolver() const { return m_resolver.get(); }
    void clearResolver();
    void clearMasterResolver();

    CSSFontSelector* fontSelector() { return m_fontSelector.get(); }
    void removeFontFaceRules(const WillBeHeapVector<RawPtrWillBeMember<const StyleRuleFontFace> >&);
    void clearFontCache();
    // updateGenericFontFamilySettings is used from WebSettingsImpl.
    void updateGenericFontFamilySettings();

    void didDetach();
    bool shouldClearResolver() const;
    StyleResolverChange resolverChanged(RecalcStyleTime, StyleResolverUpdateMode);
    unsigned resolverAccessCount() const;

    void markDocumentDirty();

    PassRefPtrWillBeRawPtr<CSSStyleSheet> createSheet(Element*, const String& text, TextPosition startPosition, bool createdByParser);
    void removeSheet(StyleSheetContents*);

    void trace(Visitor*);

private:
    StyleEngine(Document&);

    TreeScopeStyleSheetCollection* ensureStyleSheetCollectionFor(TreeScope&);
    TreeScopeStyleSheetCollection* styleSheetCollectionFor(TreeScope&);
    bool shouldUpdateShadowTreeStyleSheetCollection(StyleResolverUpdateMode);

    void markTreeScopeDirty(TreeScope&);

    bool isMaster() const { return m_isMaster; }
    Document* master();

    typedef ListHashSet<TreeScope*, 16> TreeScopeSet;
    static void insertTreeScopeInDocumentOrder(TreeScopeSet&, TreeScope*);
    void clearMediaQueryRuleSetOnTreeScopeStyleSheets(TreeScopeSet treeScopes);

    void createResolver();

    static PassRefPtrWillBeRawPtr<CSSStyleSheet> parseSheet(Element*, const String& text, TextPosition startPosition, bool createdByParser);

    Document& m_document;
    bool m_isMaster;

    // Track the number of currently loading top-level stylesheets needed for rendering.
    // Sheets loaded using the @import directive are not included in this count.
    // We use this count of pending sheets to detect when we can begin attaching
    // elements and when it is safe to execute scripts.
    int m_pendingStylesheets;

    mutable WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> > m_injectedAuthorStyleSheets;
    mutable bool m_injectedStyleSheetCacheValid;

    WillBeHeapVector<RefPtrWillBeMember<CSSStyleSheet> > m_authorStyleSheets;

    DocumentStyleSheetCollection m_documentStyleSheetCollection;
    WillBeHeapHashMap<TreeScope*, OwnPtrWillBeMember<ShadowTreeStyleSheetCollection> > m_styleSheetCollectionMap;

    bool m_documentScopeDirty;
    TreeScopeSet m_dirtyTreeScopes;
    TreeScopeSet m_activeTreeScopes;

    String m_preferredStylesheetSetName;
    String m_selectedStylesheetSetName;

    bool m_usesSiblingRules;
    bool m_usesSiblingRulesOverride;
    bool m_usesFirstLineRules;
    bool m_usesFirstLetterRules;
    bool m_usesRemUnits;
    unsigned m_maxDirectAdjacentSelectors;

    bool m_ignorePendingStylesheets;
    bool m_didCalculateResolver;
    OwnPtrWillBeMember<StyleResolver> m_resolver;

    RefPtr<CSSFontSelector> m_fontSelector;

    WillBeHeapHashMap<AtomicString, RawPtrWillBeMember<StyleSheetContents> > m_textToSheetCache;
    WillBeHeapHashMap<RawPtrWillBeMember<StyleSheetContents>, AtomicString> m_sheetToTextCache;
};

}

#endif
