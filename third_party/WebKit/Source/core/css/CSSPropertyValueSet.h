/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef CSSPropertyValueSet_h
#define CSSPropertyValueSet_h

#include "base/macros.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPropertyValue.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/parser/AtRuleDescriptors.h"
#include "core/css/parser/CSSParserMode.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ImmutableCSSPropertyValueSet;
class MutableCSSPropertyValueSet;
enum class SecureContextMode;

class CORE_EXPORT CSSPropertyValueSet
    : public GarbageCollectedFinalized<CSSPropertyValueSet> {
  friend class PropertyReference;

 public:
  void FinalizeGarbageCollectedObject();

  class PropertyReference {
    STACK_ALLOCATED();

   public:
    PropertyReference(const CSSPropertyValueSet& property_set, unsigned index)
        : property_set_(&property_set), index_(index) {}

    CSSPropertyID Id() const {
      return static_cast<CSSPropertyID>(
          PropertyMetadata().Property().PropertyID());
    }
    const CSSProperty& Property() const {
      return PropertyMetadata().Property();
    }
    CSSPropertyID ShorthandID() const {
      return PropertyMetadata().ShorthandID();
    }

    bool IsImportant() const { return PropertyMetadata().important_; }
    bool IsInherited() const { return PropertyMetadata().inherited_; }
    bool IsImplicit() const { return PropertyMetadata().implicit_; }

    const CSSValue& Value() const { return PropertyValue(); }

    // FIXME: Remove this.
    CSSPropertyValue ToCSSPropertyValue() const {
      return CSSPropertyValue(PropertyMetadata(), PropertyValue());
    }

    const CSSPropertyValueMetadata& PropertyMetadata() const;

   private:
    const CSSValue& PropertyValue() const;

    Member<const CSSPropertyValueSet> property_set_;
    unsigned index_;
  };

  unsigned PropertyCount() const;
  bool IsEmpty() const;
  PropertyReference PropertyAt(unsigned index) const {
    return PropertyReference(*this, index);
  }

  template <typename T>  // CSSPropertyID, AtRuleDescriptorID or AtomicString
  int FindPropertyIndex(T property) const;

  bool HasProperty(CSSPropertyID property) const {
    return FindPropertyIndex(property) != -1;
  }

  template <typename T>  // CSSPropertyID, AtRuleDescriptorID or AtomicString
  const CSSValue* GetPropertyCSSValue(T property) const;

  template <typename T>  // CSSPropertyID, AtRuleDescriptorID or AtomicString
  String GetPropertyValue(T property) const;

  template <typename T>  // CSSPropertyID, AtRuleDescriptorID or AtomicString
  bool PropertyIsImportant(T property) const;

  bool ShorthandIsImportant(CSSPropertyID) const;
  bool ShorthandIsImportant(AtomicString custom_property_name) const;

  CSSPropertyID GetPropertyShorthand(CSSPropertyID) const;
  bool IsPropertyImplicit(CSSPropertyID) const;

  CSSParserMode CssParserMode() const {
    return static_cast<CSSParserMode>(css_parser_mode_);
  }

  MutableCSSPropertyValueSet* MutableCopy() const;
  ImmutableCSSPropertyValueSet* ImmutableCopyIfNeeded() const;

  MutableCSSPropertyValueSet* CopyPropertiesInSet(
      const Vector<const CSSProperty*>&) const;

  String AsText() const;

  bool IsMutable() const { return is_mutable_; }

  bool HasFailedOrCanceledSubresources() const;

  static unsigned AverageSizeInBytes();

#ifndef NDEBUG
  void ShowStyle();
#endif

  bool PropertyMatches(CSSPropertyID, const CSSValue&) const;

  void Trace(blink::Visitor*);
  void TraceAfterDispatch(blink::Visitor* visitor) {}

 protected:
  enum { kMaxArraySize = (1 << 28) - 1 };

  CSSPropertyValueSet(CSSParserMode css_parser_mode)
      : css_parser_mode_(css_parser_mode), is_mutable_(true), array_size_(0) {}

  CSSPropertyValueSet(CSSParserMode css_parser_mode,
                      unsigned immutable_array_size)
      : css_parser_mode_(css_parser_mode), is_mutable_(false) {
    // Avoid min()/max() from std here in the header, because that would require
    // inclusion of <algorithm>, which is slow to compile.
    if (immutable_array_size < unsigned(kMaxArraySize))
      array_size_ = immutable_array_size;
    else
      array_size_ = unsigned(kMaxArraySize);
  }

  static uint16_t GetConvertedCSSPropertyID(CSSPropertyID);
  static uint16_t GetConvertedCSSPropertyID(const AtomicString&);
  static uint16_t GetConvertedCSSPropertyID(AtRuleDescriptorID);
  static bool IsPropertyMatch(const CSSPropertyValueMetadata&,
                              const CSSValue&,
                              uint16_t id,
                              CSSPropertyID);
  static bool IsPropertyMatch(const CSSPropertyValueMetadata&,
                              const CSSValue&,
                              uint16_t id,
                              const AtomicString& custom_property_name);
  static bool IsPropertyMatch(const CSSPropertyValueMetadata&,
                              const CSSValue&,
                              uint16_t id,
                              AtRuleDescriptorID);

  unsigned css_parser_mode_ : 3;
  mutable unsigned is_mutable_ : 1;
  unsigned array_size_ : 28;

  friend class PropertySetCSSStyleDeclaration;
  DISALLOW_COPY_AND_ASSIGN(CSSPropertyValueSet);
};

}  // namespace blink

#endif  // CSSPropertyValueSet_h
