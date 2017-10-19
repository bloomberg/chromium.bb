/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
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

#ifndef CSSCounterValue_h
#define CSSCounterValue_h

#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSStringValue.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

namespace cssvalue {

class CSSCounterValue : public CSSValue {
 public:
  static CSSCounterValue* Create(CSSCustomIdentValue* identifier,
                                 CSSIdentifierValue* list_style,
                                 CSSStringValue* separator) {
    return new CSSCounterValue(identifier, list_style, separator);
  }

  String Identifier() const { return identifier_->Value(); }
  CSSValueID ListStyle() const { return list_style_->GetValueID(); }
  String Separator() const { return separator_->Value(); }

  bool Equals(const CSSCounterValue& other) const {
    return Identifier() == other.Identifier() &&
           ListStyle() == other.ListStyle() && Separator() == other.Separator();
  }

  String CustomCSSText() const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  CSSCounterValue(CSSCustomIdentValue* identifier,
                  CSSIdentifierValue* list_style,
                  CSSStringValue* separator)
      : CSSValue(kCounterClass),
        identifier_(identifier),
        list_style_(list_style),
        separator_(separator) {}

  Member<CSSCustomIdentValue> identifier_;  // string
  Member<CSSIdentifierValue> list_style_;   // ident
  Member<CSSStringValue> separator_;        // string
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSCounterValue, IsCounterValue());

}  // namespace cssvalue

}  // namespace blink

#endif  // CSSCounterValue_h
