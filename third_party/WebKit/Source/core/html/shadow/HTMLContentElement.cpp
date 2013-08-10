/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/shadow/HTMLContentElement.h"

#include "HTMLNames.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<HTMLContentElement> HTMLContentElement::create(Document* document)
{
    return adoptRef(new HTMLContentElement(contentTag, document));
}

PassRefPtr<HTMLContentElement> HTMLContentElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLContentElement(tagName, document));
}

HTMLContentElement::HTMLContentElement(const QualifiedName& name, Document* document)
    : InsertionPoint(name, document)
    , m_shouldParseSelect(false)
    , m_isValidSelector(true)
{
    ASSERT(hasTagName(contentTag));
    ScriptWrappable::init(this);
}

HTMLContentElement::~HTMLContentElement()
{
}

void HTMLContentElement::parseSelect()
{
    ASSERT(m_shouldParseSelect);

    CSSParser parser(document());
    parser.parseSelector(m_select, m_selectorList);
    m_shouldParseSelect = false;
    m_isValidSelector = validateSelect();
    if (!m_isValidSelector) {
        CSSSelectorList emptyList;
        m_selectorList.adopt(emptyList);
    }
}

void HTMLContentElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == selectAttr) {
        if (ShadowRoot* root = containingShadowRoot())
            root->owner()->willAffectSelector();
        m_shouldParseSelect = true;
        m_select = value;
    } else
        InsertionPoint::parseAttribute(name, value);
}

bool HTMLContentElement::validateSelect() const
{
    ASSERT(!m_shouldParseSelect);

    if (m_select.isNull() || m_select.isEmpty())
        return true;

    if (!m_selectorList.isValid())
        return false;

    for (const CSSSelector* selector = m_selectorList.first(); selector; selector = m_selectorList.next(selector)) {
        if (!selector->isCompound())
            return false;
    }

    return true;
}

static inline bool checkOneSelector(const CSSSelector* selector, const Vector<Node*>& siblings, int nth)
{
    Element* element = toElement(siblings[nth]);
    SelectorChecker selectorChecker(element->document(), SelectorChecker::CollectingRules);
    SelectorChecker::SelectorCheckingContext context(selector, element, SelectorChecker::VisitedMatchEnabled);
    ShadowDOMSiblingTraversalStrategy strategy(siblings, nth);
    PseudoId ignoreDynamicPseudo = NOPSEUDO;
    return selectorChecker.match(context, ignoreDynamicPseudo, strategy) == SelectorChecker::SelectorMatches;
}

bool HTMLContentElement::matchSelector(const Vector<Node*>& siblings, int nth) const
{
    for (const CSSSelector* selector = selectorList().first(); selector; selector = CSSSelectorList::next(selector)) {
        if (checkOneSelector(selector, siblings, nth))
            return true;
    }
    return false;
}

}
