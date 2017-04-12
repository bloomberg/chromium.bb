/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef PageRuleCollector_h
#define PageRuleCollector_h

#include "core/css/resolver/MatchResult.h"
#include "platform/wtf/Vector.h"

namespace blink {

class StyleRulePage;

class PageRuleCollector {
  STACK_ALLOCATED();

 public:
  PageRuleCollector(const ComputedStyle* root_element_style, int page_index);

  void MatchPageRules(RuleSet* rules);
  const MatchResult& MatchedResult() { return result_; }

 private:
  bool IsLeftPage(const ComputedStyle* root_element_style,
                  int page_index) const;
  bool IsRightPage(const ComputedStyle* root_element_style,
                   int page_index) const {
    return !IsLeftPage(root_element_style, page_index);
  }
  bool IsFirstPage(int page_index) const;
  String PageName(int page_index) const;

  void MatchPageRulesForList(HeapVector<Member<StyleRulePage>>& matched_rules,
                             const HeapVector<Member<StyleRulePage>>& rules,
                             bool is_left_page,
                             bool is_first_page,
                             const String& page_name);

  const bool is_left_page_;
  const bool is_first_page_;
  const String page_name_;

  MatchResult result_;
};

}  // namespace blink

#endif  // PageRuleCollector_h
