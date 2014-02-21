/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2012 Apple Inc. All rights reserved.
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
#include "core/css/CSSStyleSheet.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8PerIsolateData.h"
#include "core/css/CSSCharsetRule.h"
#include "core/css/CSSImportRule.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/CSSRuleList.h"
#include "core/css/MediaList.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/Node.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

class StyleSheetCSSRuleList FINAL : public CSSRuleList {
public:
    StyleSheetCSSRuleList(CSSStyleSheet* sheet) : m_styleSheet(sheet) { }

private:
    virtual void ref() OVERRIDE { m_styleSheet->ref(); }
    virtual void deref() OVERRIDE { m_styleSheet->deref(); }

    virtual unsigned length() const OVERRIDE { return m_styleSheet->length(); }
    virtual CSSRule* item(unsigned index) const OVERRIDE { return m_styleSheet->item(index); }

    virtual CSSStyleSheet* styleSheet() const OVERRIDE { return m_styleSheet; }

    CSSStyleSheet* m_styleSheet;
};

#if !ASSERT_DISABLED
static bool isAcceptableCSSStyleSheetParent(Node* parentNode)
{
    // Only these nodes can be parents of StyleSheets, and they need to call clearOwnerNode() when moved out of document.
    return !parentNode
        || parentNode->isDocumentNode()
        || parentNode->hasTagName(HTMLNames::linkTag)
        || parentNode->hasTagName(HTMLNames::styleTag)
        || parentNode->hasTagName(SVGNames::styleTag)
        || parentNode->nodeType() == Node::PROCESSING_INSTRUCTION_NODE;
}
#endif

PassRefPtr<CSSStyleSheet> CSSStyleSheet::create(PassRefPtrWillBeRawPtr<StyleSheetContents> sheet, CSSImportRule* ownerRule)
{
    return adoptRef(new CSSStyleSheet(sheet, ownerRule));
}

PassRefPtr<CSSStyleSheet> CSSStyleSheet::create(PassRefPtrWillBeRawPtr<StyleSheetContents> sheet, Node* ownerNode)
{
    return adoptRef(new CSSStyleSheet(sheet, ownerNode, false, TextPosition::minimumPosition()));
}

PassRefPtr<CSSStyleSheet> CSSStyleSheet::createInline(PassRefPtrWillBeRawPtr<StyleSheetContents> sheet, Node* ownerNode, const TextPosition& startPosition)
{
    ASSERT(sheet);
    return adoptRef(new CSSStyleSheet(sheet, ownerNode, true, startPosition));
}

PassRefPtr<CSSStyleSheet> CSSStyleSheet::createInline(Node* ownerNode, const KURL& baseURL, const TextPosition& startPosition, const String& encoding)
{
    CSSParserContext parserContext(ownerNode->document(), 0, baseURL, encoding);
    RefPtrWillBeRawPtr<StyleSheetContents> sheet = StyleSheetContents::create(baseURL.string(), parserContext);
    return adoptRef(new CSSStyleSheet(sheet.release(), ownerNode, true, startPosition));
}

CSSStyleSheet::CSSStyleSheet(PassRefPtrWillBeRawPtr<StyleSheetContents> contents, CSSImportRule* ownerRule)
    : m_contents(contents)
    , m_isInlineStylesheet(false)
    , m_isDisabled(false)
    , m_ownerNode(0)
    , m_ownerRule(ownerRule)
    , m_startPosition(TextPosition::minimumPosition())
    , m_loadCompleted(false)
{
    m_contents->registerClient(this);
}

CSSStyleSheet::CSSStyleSheet(PassRefPtrWillBeRawPtr<StyleSheetContents> contents, Node* ownerNode, bool isInlineStylesheet, const TextPosition& startPosition)
    : m_contents(contents)
    , m_isInlineStylesheet(isInlineStylesheet)
    , m_isDisabled(false)
    , m_ownerNode(ownerNode)
    , m_ownerRule(0)
    , m_startPosition(startPosition)
    , m_loadCompleted(false)
{
    ASSERT(isAcceptableCSSStyleSheetParent(ownerNode));
    m_contents->registerClient(this);
}

CSSStyleSheet::~CSSStyleSheet()
{
    // For style rules outside the document, .parentStyleSheet can become null even if the style rule
    // is still observable from JavaScript. This matches the behavior of .parentNode for nodes, but
    // it's not ideal because it makes the CSSOM's behavior depend on the timing of garbage collection.
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->setParentStyleSheet(0);
    }

    if (m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper->clearParentStyleSheet();

    m_contents->unregisterClient(this);
}

void CSSStyleSheet::willMutateRules()
{
    InspectorInstrumentation::willMutateRules(this);
    // If we are the only client it is safe to mutate.
    if (m_contents->hasOneClient() && !m_contents->isInMemoryCache()) {
        m_contents->clearRuleSet();
        if (m_contents->maybeCacheable())
            StyleEngine::removeSheet(m_contents.get());
        m_contents->setMutable();
        return;
    }
    // Only cacheable stylesheets should have multiple clients.
    ASSERT(m_contents->isCacheable());

    // Copy-on-write.
    m_contents->unregisterClient(this);
    m_contents = m_contents->copy();
    m_contents->registerClient(this);

    m_contents->setMutable();

    // Any existing CSSOM wrappers need to be connected to the copied child rules.
    reattachChildRuleCSSOMWrappers();
}

void CSSStyleSheet::didMutateRules()
{
    ASSERT(m_contents->isMutable());
    ASSERT(m_contents->hasOneClient());

    InspectorInstrumentation::didMutateRules(this);
    didMutate(PartialRuleUpdate);
}

void CSSStyleSheet::didMutate(StyleSheetUpdateType updateType)
{
    Document* owner = ownerDocument();
    if (!owner)
        return;

    // Need FullStyleUpdate when insertRule or deleteRule,
    // because StyleSheetCollection::analyzeStyleSheetChange cannot detect partial rule update.
    StyleResolverUpdateMode updateMode = updateType != PartialRuleUpdate ? AnalyzedStyleUpdate : FullStyleUpdate;
    owner->modifiedStyleSheet(this, RecalcStyleDeferred, updateMode);
}

void CSSStyleSheet::reattachChildRuleCSSOMWrappers()
{
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (!m_childRuleCSSOMWrappers[i])
            continue;
        m_childRuleCSSOMWrappers[i]->reattach(m_contents->ruleAt(i));
    }
}

void CSSStyleSheet::setDisabled(bool disabled)
{
    if (disabled == m_isDisabled)
        return;
    m_isDisabled = disabled;

    didMutate();
}

void CSSStyleSheet::setMediaQueries(PassRefPtr<MediaQuerySet> mediaQueries)
{
    m_mediaQueries = mediaQueries;
    if (m_mediaCSSOMWrapper && m_mediaQueries)
        m_mediaCSSOMWrapper->reattach(m_mediaQueries.get());

    // Add warning message to inspector whenever dpi/dpcm values are used for "screen" media.
    reportMediaQueryWarningIfNeeded(ownerDocument(), m_mediaQueries.get());
}

unsigned CSSStyleSheet::length() const
{
    return m_contents->ruleCount();
}

CSSRule* CSSStyleSheet::item(unsigned index)
{
    unsigned ruleCount = length();
    if (index >= ruleCount)
        return 0;

    if (m_childRuleCSSOMWrappers.isEmpty())
        m_childRuleCSSOMWrappers.grow(ruleCount);
    ASSERT(m_childRuleCSSOMWrappers.size() == ruleCount);

    RefPtr<CSSRule>& cssRule = m_childRuleCSSOMWrappers[index];
    if (!cssRule) {
        if (index == 0 && m_contents->hasCharsetRule()) {
            ASSERT(!m_contents->ruleAt(0));
            cssRule = CSSCharsetRule::create(this, m_contents->encodingFromCharsetRule());
        } else
            cssRule = m_contents->ruleAt(index)->createCSSOMWrapper(this);
    }
    return cssRule.get();
}

bool CSSStyleSheet::canAccessRules() const
{
    if (m_isInlineStylesheet)
        return true;
    KURL baseURL = m_contents->baseURL();
    if (baseURL.isEmpty())
        return true;
    Document* document = ownerDocument();
    if (!document)
        return true;
    if (document->securityOrigin()->canRequest(baseURL))
        return true;
    return false;
}

PassRefPtr<CSSRuleList> CSSStyleSheet::rules()
{
    if (!canAccessRules())
        return nullptr;
    // IE behavior.
    RefPtr<StaticCSSRuleList> nonCharsetRules = StaticCSSRuleList::create();
    unsigned ruleCount = length();
    for (unsigned i = 0; i < ruleCount; ++i) {
        CSSRule* rule = item(i);
        if (rule->type() == CSSRule::CHARSET_RULE)
            continue;
        nonCharsetRules->rules().append(rule);
    }
    return nonCharsetRules.release();
}

unsigned CSSStyleSheet::insertRule(const String& ruleString, unsigned index, ExceptionState& exceptionState)
{
    ASSERT(m_childRuleCSSOMWrappers.isEmpty() || m_childRuleCSSOMWrappers.size() == m_contents->ruleCount());

    if (index > length()) {
        exceptionState.throwDOMException(IndexSizeError, "The index provided (" + String::number(index) + ") is larger than the maximum index (" + String::number(length()) + ").");
        return 0;
    }
    CSSParserContext context(m_contents->parserContext(), UseCounter::getFrom(this));
    BisonCSSParser p(context);
    RefPtrWillBeRawPtr<StyleRuleBase> rule = p.parseRule(m_contents.get(), ruleString);

    if (!rule) {
        exceptionState.throwDOMException(SyntaxError, "Failed to parse the rule '" + ruleString + "'.");
        return 0;
    }
    RuleMutationScope mutationScope(this);

    bool success = m_contents->wrapperInsertRule(rule, index);
    if (!success) {
        exceptionState.throwDOMException(HierarchyRequestError, "Failed to insert the rule.");
        return 0;
    }
    if (!m_childRuleCSSOMWrappers.isEmpty())
        m_childRuleCSSOMWrappers.insert(index, RefPtr<CSSRule>());

    return index;
}

unsigned CSSStyleSheet::insertRule(const String& rule, ExceptionState& exceptionState)
{
    UseCounter::countDeprecation(callingExecutionContext(V8PerIsolateData::mainThreadIsolate()), UseCounter::CSSStyleSheetInsertRuleOptionalArg);
    return insertRule(rule, 0, exceptionState);
}

void CSSStyleSheet::deleteRule(unsigned index, ExceptionState& exceptionState)
{
    ASSERT(m_childRuleCSSOMWrappers.isEmpty() || m_childRuleCSSOMWrappers.size() == m_contents->ruleCount());

    if (index >= length()) {
        exceptionState.throwDOMException(IndexSizeError, "The index provided (" + String::number(index) + ") is larger than the maximum index (" + String::number(length() - 1) + ").");
        return;
    }
    RuleMutationScope mutationScope(this);

    m_contents->wrapperDeleteRule(index);

    if (!m_childRuleCSSOMWrappers.isEmpty()) {
        if (m_childRuleCSSOMWrappers[index])
            m_childRuleCSSOMWrappers[index]->setParentStyleSheet(0);
        m_childRuleCSSOMWrappers.remove(index);
    }
}

int CSSStyleSheet::addRule(const String& selector, const String& style, int index, ExceptionState& exceptionState)
{
    StringBuilder text;
    text.append(selector);
    text.appendLiteral(" { ");
    text.append(style);
    if (!style.isEmpty())
        text.append(' ');
    text.append('}');
    insertRule(text.toString(), index, exceptionState);

    // As per Microsoft documentation, always return -1.
    return -1;
}

int CSSStyleSheet::addRule(const String& selector, const String& style, ExceptionState& exceptionState)
{
    return addRule(selector, style, length(), exceptionState);
}


PassRefPtr<CSSRuleList> CSSStyleSheet::cssRules()
{
    if (!canAccessRules())
        return nullptr;
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = adoptPtr(new StyleSheetCSSRuleList(this));
    return m_ruleListCSSOMWrapper.get();
}

String CSSStyleSheet::href() const
{
    return m_contents->originalURL();
}

KURL CSSStyleSheet::baseURL() const
{
    return m_contents->baseURL();
}

bool CSSStyleSheet::isLoading() const
{
    return m_contents->isLoading();
}

MediaList* CSSStyleSheet::media() const
{
    if (!m_mediaQueries)
        return 0;

    if (!m_mediaCSSOMWrapper)
        m_mediaCSSOMWrapper = MediaList::create(m_mediaQueries.get(), const_cast<CSSStyleSheet*>(this));
    return m_mediaCSSOMWrapper.get();
}

CSSStyleSheet* CSSStyleSheet::parentStyleSheet() const
{
    return m_ownerRule ? m_ownerRule->parentStyleSheet() : 0;
}

Document* CSSStyleSheet::ownerDocument() const
{
    const CSSStyleSheet* root = this;
    while (root->parentStyleSheet())
        root = root->parentStyleSheet();
    return root->ownerNode() ? &root->ownerNode()->document() : 0;
}

void CSSStyleSheet::clearChildRuleCSSOMWrappers()
{
    m_childRuleCSSOMWrappers.clear();
}

bool CSSStyleSheet::sheetLoaded()
{
    ASSERT(m_ownerNode);
    m_loadCompleted = m_ownerNode->sheetLoaded();
    return m_loadCompleted;
}

void CSSStyleSheet::startLoadingDynamicSheet()
{
    m_loadCompleted = false;
    m_ownerNode->startLoadingDynamicSheet();
}

}
