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

#include "core/html/HTMLContentElement.h"

#include "core/HTMLNames.h"
#include "core/css/SelectorChecker.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/ElementShadowV0.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/ShadowRoot.h"
#include "core/frame/UseCounter.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

using namespace HTMLNames;

HTMLContentElement* HTMLContentElement::Create(
    Document& document,
    HTMLContentSelectFilter* filter) {
  return new HTMLContentElement(document, filter);
}

inline HTMLContentElement::HTMLContentElement(Document& document,
                                              HTMLContentSelectFilter* filter)
    : V0InsertionPoint(contentTag, document),
      should_parse_select_(false),
      is_valid_selector_(true),
      filter_(filter) {
  UseCounter::Count(document, WebFeature::kHTMLContentElement);
}

HTMLContentElement::~HTMLContentElement() {}

DEFINE_TRACE(HTMLContentElement) {
  visitor->Trace(filter_);
  V0InsertionPoint::Trace(visitor);
}

void HTMLContentElement::ParseSelect() {
  DCHECK(should_parse_select_);

  selector_list_ = CSSParser::ParseSelector(
      CSSParserContext::Create(GetDocument()), nullptr, select_);
  should_parse_select_ = false;
  is_valid_selector_ = ValidateSelect();
  if (!is_valid_selector_)
    selector_list_ = CSSSelectorList();
}

void HTMLContentElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == selectAttr) {
    if (ShadowRoot* root = ContainingShadowRoot()) {
      if (!root->IsV1() && root->Owner())
        root->Owner()->V0().WillAffectSelector();
    }
    should_parse_select_ = true;
    select_ = params.new_value;
  } else {
    V0InsertionPoint::ParseAttribute(params);
  }
}

static inline bool IncludesDisallowedPseudoClass(const CSSSelector& selector) {
  if (selector.GetPseudoType() == CSSSelector::kPseudoNot) {
    const CSSSelector* sub_selector = selector.SelectorList()->First();
    return sub_selector->Match() == CSSSelector::kPseudoClass;
  }
  return selector.Match() == CSSSelector::kPseudoClass;
}

bool HTMLContentElement::ValidateSelect() const {
  DCHECK(!should_parse_select_);

  if (select_.IsNull() || select_.IsEmpty())
    return true;

  if (!selector_list_.IsValid())
    return false;

  for (const CSSSelector* selector = selector_list_.First(); selector;
       selector = selector_list_.Next(*selector)) {
    if (!selector->IsCompound())
      return false;
    for (const CSSSelector* sub_selector = selector; sub_selector;
         sub_selector = sub_selector->TagHistory()) {
      if (IncludesDisallowedPseudoClass(*sub_selector))
        return false;
    }
  }
  return true;
}

// TODO(esprehn): element should really be const, but matching a selector is not
// const for some SelectorCheckingModes (mainly ResolvingStyle) where it sets
// dynamic restyle flags on elements.
bool HTMLContentElement::MatchSelector(Element& element) const {
  SelectorChecker::Init init;
  init.mode = SelectorChecker::kQueryingRules;
  SelectorChecker checker(init);
  SelectorChecker::SelectorCheckingContext context(
      &element, SelectorChecker::kVisitedMatchDisabled);
  for (const CSSSelector* selector = SelectorList().First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    context.selector = selector;
    if (checker.Match(context))
      return true;
  }
  return false;
}

}  // namespace blink
