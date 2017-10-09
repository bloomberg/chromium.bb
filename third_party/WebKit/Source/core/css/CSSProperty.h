/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef CSSProperty_h
#define CSSProperty_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSValue.h"
#include "core/css/properties/CSSPropertyAPI.h"
#include "platform/wtf/Allocator.h"

namespace blink {

struct StylePropertyMetadata {
  DISALLOW_NEW();
  StylePropertyMetadata(CSSPropertyID property_id,
                        bool is_set_from_shorthand,
                        int index_in_shorthands_vector,
                        bool important,
                        bool implicit,
                        bool inherited)
      : property_id_(property_id),
        is_set_from_shorthand_(is_set_from_shorthand),
        index_in_shorthands_vector_(index_in_shorthands_vector),
        important_(important),
        implicit_(implicit),
        inherited_(inherited) {}

  CSSPropertyID ShorthandID() const;

  unsigned property_id_ : 10;
  unsigned is_set_from_shorthand_ : 1;
  // If this property was set as part of an ambiguous shorthand, gives the index
  // in the shorthands vector.
  unsigned index_in_shorthands_vector_ : 2;
  unsigned important_ : 1;
  // Whether or not the property was set implicitly as the result of a
  // shorthand.
  unsigned implicit_ : 1;
  unsigned inherited_ : 1;
};

class CSSProperty {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  CSSProperty(CSSPropertyID property_id,
              const CSSValue& value,
              bool important = false,
              bool is_set_from_shorthand = false,
              int index_in_shorthands_vector = 0,
              bool implicit = false)
      : metadata_(property_id,
                  is_set_from_shorthand,
                  index_in_shorthands_vector,
                  important,
                  implicit,
                  CSSPropertyAPI::Get(property_id).IsInherited()),
        value_(value) {}

  // FIXME: Remove this.
  CSSProperty(StylePropertyMetadata metadata, const CSSValue& value)
      : metadata_(metadata), value_(value) {}

  CSSPropertyID Id() const {
    return static_cast<CSSPropertyID>(metadata_.property_id_);
  }
  bool IsSetFromShorthand() const { return metadata_.is_set_from_shorthand_; }
  CSSPropertyID ShorthandID() const { return metadata_.ShorthandID(); }
  bool IsImportant() const { return metadata_.important_; }

  const CSSValue* Value() const { return value_.Get(); }

  static void FilterEnabledCSSPropertiesIntoVector(const CSSPropertyID*,
                                                   size_t length,
                                                   Vector<CSSPropertyID>&);

  const StylePropertyMetadata& Metadata() const { return metadata_; }

  bool operator==(const CSSProperty& other) const;

  DEFINE_INLINE_TRACE() { visitor->Trace(value_); }

 private:
  StylePropertyMetadata metadata_;
  Member<const CSSValue> value_;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::CSSProperty);

#endif  // CSSProperty_h
