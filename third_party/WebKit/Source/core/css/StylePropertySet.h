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

#ifndef StylePropertySet_h
#define StylePropertySet_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSProperty.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/parser/CSSParserMode.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSStyleDeclaration;
class ImmutableStylePropertySet;
class MutableStylePropertySet;
class PropertyRegistry;
class StyleSheetContents;

class CORE_EXPORT StylePropertySet
    : public GarbageCollectedFinalized<StylePropertySet> {
  WTF_MAKE_NONCOPYABLE(StylePropertySet);
  friend class PropertyReference;

 public:
  void FinalizeGarbageCollectedObject();

  class PropertyReference {
    STACK_ALLOCATED();

   public:
    PropertyReference(const StylePropertySet& property_set, unsigned index)
        : property_set_(&property_set), index_(index) {}

    CSSPropertyID Id() const {
      return static_cast<CSSPropertyID>(PropertyMetadata().property_id_);
    }
    CSSPropertyID ShorthandID() const {
      return PropertyMetadata().ShorthandID();
    }

    bool IsImportant() const { return PropertyMetadata().important_; }
    bool IsInherited() const { return PropertyMetadata().inherited_; }
    bool IsImplicit() const { return PropertyMetadata().implicit_; }

    const CSSValue& Value() const { return PropertyValue(); }

    // FIXME: Remove this.
    CSSProperty ToCSSProperty() const {
      return CSSProperty(PropertyMetadata(), PropertyValue());
    }

    const StylePropertyMetadata& PropertyMetadata() const;

   private:
    const CSSValue& PropertyValue() const;

    Member<const StylePropertySet> property_set_;
    unsigned index_;
  };

  unsigned PropertyCount() const;
  bool IsEmpty() const;
  PropertyReference PropertyAt(unsigned index) const {
    return PropertyReference(*this, index);
  }

  template <typename T>  // CSSPropertyID or AtomicString
  int FindPropertyIndex(T property) const;

  bool HasProperty(CSSPropertyID property) const {
    return FindPropertyIndex(property) != -1;
  }

  template <typename T>  // CSSPropertyID or AtomicString
  const CSSValue* GetPropertyCSSValue(T property) const;

  template <typename T>  // CSSPropertyID or AtomicString
  String GetPropertyValue(T property) const;

  template <typename T>  // CSSPropertyID or AtomicString
  bool PropertyIsImportant(T property) const;

  bool ShorthandIsImportant(CSSPropertyID) const;
  bool ShorthandIsImportant(AtomicString custom_property_name) const;

  CSSPropertyID GetPropertyShorthand(CSSPropertyID) const;
  bool IsPropertyImplicit(CSSPropertyID) const;

  CSSParserMode CssParserMode() const {
    return static_cast<CSSParserMode>(css_parser_mode_);
  }

  MutableStylePropertySet* MutableCopy() const;
  ImmutableStylePropertySet* ImmutableCopyIfNeeded() const;

  MutableStylePropertySet* CopyPropertiesInSet(
      const Vector<CSSPropertyID>&) const;

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

  StylePropertySet(CSSParserMode css_parser_mode)
      : css_parser_mode_(css_parser_mode), is_mutable_(true), array_size_(0) {}

  StylePropertySet(CSSParserMode css_parser_mode, unsigned immutable_array_size)
      : css_parser_mode_(css_parser_mode), is_mutable_(false) {
    // Avoid min()/max() from std here in the header, because that would require
    // inclusion of <algorithm>, which is slow to compile.
    if (immutable_array_size < unsigned(kMaxArraySize))
      array_size_ = immutable_array_size;
    else
      array_size_ = unsigned(kMaxArraySize);
  }

  unsigned css_parser_mode_ : 3;
  mutable unsigned is_mutable_ : 1;
  unsigned array_size_ : 28;

  friend class PropertySetCSSStyleDeclaration;
};

// Used for lazily parsing properties.
class CSSLazyPropertyParser
    : public GarbageCollectedFinalized<CSSLazyPropertyParser> {
  WTF_MAKE_NONCOPYABLE(CSSLazyPropertyParser);

 public:
  CSSLazyPropertyParser() {}
  virtual ~CSSLazyPropertyParser() {}
  virtual StylePropertySet* ParseProperties() = 0;
  virtual void Trace(blink::Visitor*);
};

class CORE_EXPORT ImmutableStylePropertySet : public StylePropertySet {
 public:
  ~ImmutableStylePropertySet();
  static ImmutableStylePropertySet* Create(const CSSProperty* properties,
                                           unsigned count,
                                           CSSParserMode);

  unsigned PropertyCount() const { return array_size_; }

  const Member<const CSSValue>* ValueArray() const;
  const StylePropertyMetadata* MetadataArray() const;

  template <typename T>  // CSSPropertyID or AtomicString
  int FindPropertyIndex(T property) const;

  void TraceAfterDispatch(blink::Visitor*);

  void* operator new(std::size_t, void* location) { return location; }

  void* storage_;

 private:
  ImmutableStylePropertySet(const CSSProperty*, unsigned count, CSSParserMode);
};

inline const Member<const CSSValue>* ImmutableStylePropertySet::ValueArray()
    const {
  return reinterpret_cast<const Member<const CSSValue>*>(
      const_cast<const void**>(&(this->storage_)));
}

inline const StylePropertyMetadata* ImmutableStylePropertySet::MetadataArray()
    const {
  return reinterpret_cast<const StylePropertyMetadata*>(
      &reinterpret_cast<const char*>(
          &(this->storage_))[array_size_ * sizeof(Member<CSSValue>)]);
}

DEFINE_TYPE_CASTS(ImmutableStylePropertySet,
                  StylePropertySet,
                  set,
                  !set->IsMutable(),
                  !set.IsMutable());

class CORE_EXPORT MutableStylePropertySet : public StylePropertySet {
 public:
  ~MutableStylePropertySet() {}
  static MutableStylePropertySet* Create(CSSParserMode);
  static MutableStylePropertySet* Create(const CSSProperty* properties,
                                         unsigned count);

  unsigned PropertyCount() const { return property_vector_.size(); }

  // Returns whether this style set was changed.
  bool AddParsedProperties(const HeapVector<CSSProperty, 256>&);
  bool AddRespectingCascade(const CSSProperty&);

  struct SetResult {
    bool did_parse;
    bool did_change;
  };
  // These expand shorthand properties into multiple properties.
  SetResult SetProperty(CSSPropertyID unresolved_property,
                        const String& value,
                        bool important = false,
                        StyleSheetContents* context_style_sheet = 0);
  SetResult SetProperty(const AtomicString& custom_property_name,
                        const PropertyRegistry*,
                        const String& value,
                        bool important,
                        StyleSheetContents* context_style_sheet,
                        bool is_animation_tainted);
  void SetProperty(CSSPropertyID, const CSSValue&, bool important = false);

  // These do not. FIXME: This is too messy, we can do better.
  bool SetProperty(CSSPropertyID,
                   CSSValueID identifier,
                   bool important = false);
  bool SetProperty(const CSSProperty&, CSSProperty* slot = 0);

  template <typename T>  // CSSPropertyID or AtomicString
  bool RemoveProperty(T property, String* return_text = 0);
  bool RemovePropertiesInSet(const CSSPropertyID* set, unsigned length);
  void RemoveEquivalentProperties(const StylePropertySet*);
  void RemoveEquivalentProperties(const CSSStyleDeclaration*);

  void MergeAndOverrideOnConflict(const StylePropertySet*);

  void Clear();
  void ParseDeclarationList(const String& style_declaration,
                            StyleSheetContents* context_style_sheet);

  CSSStyleDeclaration* EnsureCSSStyleDeclaration();

  template <typename T>  // CSSPropertyID or AtomicString
  int FindPropertyIndex(T property) const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  explicit MutableStylePropertySet(CSSParserMode);
  explicit MutableStylePropertySet(const StylePropertySet&);
  MutableStylePropertySet(const CSSProperty* properties, unsigned count);

  bool RemovePropertyAtIndex(int, String* return_text);

  bool RemoveShorthandProperty(CSSPropertyID);
  bool RemoveShorthandProperty(const AtomicString& custom_property_name) {
    return false;
  }
  CSSProperty* FindCSSPropertyWithID(
      CSSPropertyID,
      const AtomicString& custom_property_name = g_null_atom);
  Member<PropertySetCSSStyleDeclaration> cssom_wrapper_;

  friend class StylePropertySet;

  HeapVector<CSSProperty, 4> property_vector_;
};

DEFINE_TYPE_CASTS(MutableStylePropertySet,
                  StylePropertySet,
                  set,
                  set->IsMutable(),
                  set.IsMutable());

inline MutableStylePropertySet* ToMutableStylePropertySet(
    const Persistent<StylePropertySet>& set) {
  return ToMutableStylePropertySet(set.Get());
}

inline MutableStylePropertySet* ToMutableStylePropertySet(
    const Member<StylePropertySet>& set) {
  return ToMutableStylePropertySet(set.Get());
}

inline const StylePropertyMetadata&
StylePropertySet::PropertyReference::PropertyMetadata() const {
  if (property_set_->IsMutable())
    return ToMutableStylePropertySet(*property_set_)
        .property_vector_.at(index_)
        .Metadata();
  return ToImmutableStylePropertySet(*property_set_).MetadataArray()[index_];
}

inline const CSSValue& StylePropertySet::PropertyReference::PropertyValue()
    const {
  if (property_set_->IsMutable())
    return *ToMutableStylePropertySet(*property_set_)
                .property_vector_.at(index_)
                .Value();
  return *ToImmutableStylePropertySet(*property_set_).ValueArray()[index_];
}

inline unsigned StylePropertySet::PropertyCount() const {
  if (is_mutable_)
    return ToMutableStylePropertySet(this)->property_vector_.size();
  return array_size_;
}

inline bool StylePropertySet::IsEmpty() const {
  return !PropertyCount();
}

template <typename T>
inline int StylePropertySet::FindPropertyIndex(T property) const {
  if (is_mutable_)
    return ToMutableStylePropertySet(this)->FindPropertyIndex(property);
  return ToImmutableStylePropertySet(this)->FindPropertyIndex(property);
}

}  // namespace blink

#endif  // StylePropertySet_h
