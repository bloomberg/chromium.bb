/**
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

#include "core/css/StyleSheetList.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLStyleElement.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

using namespace HTMLNames;

StyleSheetList::StyleSheetList(TreeScope* tree_scope)
    : tree_scope_(tree_scope) {}

inline const HeapVector<TraceWrapperMember<StyleSheet>>&
StyleSheetList::StyleSheets() const {
  return GetDocument()->GetStyleEngine().StyleSheetsForStyleSheetList(
      *tree_scope_);
}

unsigned StyleSheetList::length() {
  return StyleSheets().size();
}

StyleSheet* StyleSheetList::item(unsigned index) {
  const HeapVector<TraceWrapperMember<StyleSheet>>& sheets = StyleSheets();
  return index < sheets.size() ? sheets[index].Get() : nullptr;
}

HTMLStyleElement* StyleSheetList::GetNamedItem(const AtomicString& name) const {
  // IE also supports retrieving a stylesheet by name, using the name/id of the
  // <style> tag (this is consistent with all the other collections) ### Bad
  // implementation because returns a single element (are IDs always unique?)
  // and doesn't look for name attribute. But unicity of stylesheet ids is good
  // practice anyway ;)
  // FIXME: We should figure out if we should change this or fix the spec.
  Element* element = tree_scope_->getElementById(name);
  return isHTMLStyleElement(element) ? toHTMLStyleElement(element) : nullptr;
}

CSSStyleSheet* StyleSheetList::AnonymousNamedGetter(const AtomicString& name) {
  if (GetDocument()) {
    UseCounter::Count(*GetDocument(),
                      WebFeature::kStyleSheetListAnonymousNamedGetter);
  }
  HTMLStyleElement* item = GetNamedItem(name);
  if (!item)
    return nullptr;
  return item->sheet();
}

DEFINE_TRACE(StyleSheetList) {
  visitor->Trace(tree_scope_);
}

}  // namespace blink
