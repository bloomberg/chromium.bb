/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/css/parser/CSSParserSelector.h"

#include <memory>
#include "core/css/CSSSelectorList.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

CSSParserSelector::CSSParserSelector()
    : selector_(std::make_unique<CSSSelector>()) {}

CSSParserSelector::CSSParserSelector(const QualifiedName& tag_q_name,
                                     bool is_implicit)
    : selector_(std::make_unique<CSSSelector>(tag_q_name, is_implicit)) {}

CSSParserSelector::~CSSParserSelector() {
  if (!tag_history_)
    return;
  Vector<std::unique_ptr<CSSParserSelector>, 16> to_delete;
  std::unique_ptr<CSSParserSelector> selector = std::move(tag_history_);
  while (true) {
    std::unique_ptr<CSSParserSelector> next = std::move(selector->tag_history_);
    to_delete.push_back(std::move(selector));
    if (!next)
      break;
    selector = std::move(next);
  }
}

void CSSParserSelector::AdoptSelectorVector(
    Vector<std::unique_ptr<CSSParserSelector>>& selector_vector) {
  CSSSelectorList* selector_list = new CSSSelectorList(
      CSSSelectorList::AdoptSelectorVector(selector_vector));
  selector_->SetSelectorList(WTF::WrapUnique(selector_list));
}

void CSSParserSelector::SetSelectorList(
    std::unique_ptr<CSSSelectorList> selector_list) {
  selector_->SetSelectorList(std::move(selector_list));
}

bool CSSParserSelector::IsSimple() const {
  if (selector_->SelectorList() ||
      selector_->Match() == CSSSelector::kPseudoElement)
    return false;

  if (!tag_history_)
    return true;

  if (selector_->Match() == CSSSelector::kTag) {
    // We can't check against anyQName() here because namespace may not be
    // g_null_atom.
    // Example:
    //     @namespace "http://www.w3.org/2000/svg";
    //     svg:not(:root) { ...
    if (selector_->TagQName().LocalName() == g_star_atom)
      return tag_history_->IsSimple();
  }

  return false;
}

void CSSParserSelector::AppendTagHistory(
    CSSSelector::RelationType relation,
    std::unique_ptr<CSSParserSelector> selector) {
  CSSParserSelector* end = this;
  while (end->TagHistory())
    end = end->TagHistory();
  end->SetRelation(relation);
  end->SetTagHistory(std::move(selector));
}

std::unique_ptr<CSSParserSelector> CSSParserSelector::ReleaseTagHistory() {
  SetRelation(CSSSelector::kSubSelector);
  return std::move(tag_history_);
}

void CSSParserSelector::PrependTagSelector(const QualifiedName& tag_q_name,
                                           bool is_implicit) {
  std::unique_ptr<CSSParserSelector> second = CSSParserSelector::Create();
  second->selector_ = std::move(selector_);
  second->tag_history_ = std::move(tag_history_);
  tag_history_ = std::move(second);
  selector_ = std::make_unique<CSSSelector>(tag_q_name, is_implicit);
}

bool CSSParserSelector::IsHostPseudoSelector() const {
  return GetPseudoType() == CSSSelector::kPseudoHost ||
         GetPseudoType() == CSSSelector::kPseudoHostContext;
}

}  // namespace blink
