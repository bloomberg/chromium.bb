/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef StyleSheetList_h
#define StyleSheetList_h

#include "core/css/CSSStyleSheet.h"
#include "core/dom/TreeScope.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Vector.h"

namespace blink {

class HTMLStyleElement;
class StyleSheet;

class CORE_EXPORT StyleSheetList final
    : public GarbageCollected<StyleSheetList>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static StyleSheetList* Create(TreeScope* tree_scope) {
    return new StyleSheetList(tree_scope);
  }

  unsigned length();
  StyleSheet* item(unsigned index);

  HTMLStyleElement* GetNamedItem(const AtomicString&) const;

  Document* GetDocument() const {
    return tree_scope_ ? &tree_scope_->GetDocument() : nullptr;
  }

  CSSStyleSheet* AnonymousNamedGetter(const AtomicString&);

  void Trace(blink::Visitor*);

 private:
  explicit StyleSheetList(TreeScope*);
  const HeapVector<TraceWrapperMember<StyleSheet>>& StyleSheets() const;

  Member<TreeScope> tree_scope_;
};

}  // namespace blink

#endif  // StyleSheetList_h
