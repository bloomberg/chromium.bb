/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2012 Apple Computer, Inc.
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

#ifndef CSSRuleList_h
#define CSSRuleList_h

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;

class CSSRuleList : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual unsigned length() const = 0;
  virtual CSSRule* item(unsigned index) const = 0;

  virtual CSSStyleSheet* GetStyleSheet() const = 0;

 protected:
  CSSRuleList() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CSSRuleList);
};

class StaticCSSRuleList final : public CSSRuleList {
 public:
  static StaticCSSRuleList* Create() { return new StaticCSSRuleList(); }

  HeapVector<Member<CSSRule>>& Rules() { return rules_; }

  CSSStyleSheet* GetStyleSheet() const override { return 0; }

  virtual void Trace(blink::Visitor*);

 private:
  StaticCSSRuleList();

  unsigned length() const override { return rules_.size(); }
  CSSRule* item(unsigned index) const override {
    return index < rules_.size() ? rules_[index].Get() : nullptr;
  }

  HeapVector<Member<CSSRule>> rules_;
};

template <class Rule>
class LiveCSSRuleList final : public CSSRuleList {
 public:
  static LiveCSSRuleList* Create(Rule* rule) {
    return new LiveCSSRuleList(rule);
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(rule_);
    CSSRuleList::Trace(visitor);
  }

 private:
  LiveCSSRuleList(Rule* rule) : rule_(rule) {}

  unsigned length() const override { return rule_->length(); }
  CSSRule* item(unsigned index) const override { return rule_->Item(index); }
  CSSStyleSheet* GetStyleSheet() const override {
    return rule_->parentStyleSheet();
  }

  Member<Rule> rule_;
};

}  // namespace blink

#endif  // CSSRuleList_h
