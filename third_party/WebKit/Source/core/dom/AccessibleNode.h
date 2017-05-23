// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AccessibleNode_h
#define AccessibleNode_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class AXObjectCache;
class Element;
class QualifiedName;

// All of the properties of AccessibleNode that have type "string".
// TODO(dmazzoni): Add similar enums for all of the properties with
// type bool, float, reference, and reference list.
enum class AOMStringProperty {
  kAutocomplete,
  kChecked,
  kCurrent,
  kInvalid,
  kKeyShortcuts,
  kLabel,
  kLive,
  kOrientation,
  kPlaceholder,
  kRelevant,
  kRole,
  kRoleDescription,
  kSort,
  kValueText
};

// All of the properties of AccessibleNode that have type "boolean".
enum class AOMBooleanProperty {
  kAtomic,
  kBusy,
  kDisabled,
  kExpanded,
  kHidden,
  kModal,
  kMultiline,
  kMultiselectable,
  kReadOnly,
  kRequired,
  kSelected
};

// All of the properties of AccessibleNode that have an unsigned integer type.
enum class AOMUIntProperty {
  kColIndex,
  kColSpan,
  kLevel,
  kPosInSet,
  kRowIndex,
  kRowSpan,
};

// All of the properties of AccessibleNode that have a signed integer type.
// (These all allow the value -1.)
enum class AOMIntProperty { kColCount, kRowCount, kSetSize };

// All of the properties of AccessibleNode that have a floating-point type.
enum class AOMFloatProperty { kValueMax, kValueMin, kValueNow };

// Accessibility Object Model node
// Explainer: https://github.com/WICG/aom/blob/master/explainer.md
// Spec: https://wicg.github.io/aom/spec/
class CORE_EXPORT AccessibleNode
    : public GarbageCollectedFinalized<AccessibleNode>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AccessibleNode(Element*);
  virtual ~AccessibleNode();

  // Returns the given string property if the Element has an AccessibleNode.
  static const AtomicString& GetProperty(Element*, AOMStringProperty);

  // Returns the value of the given property if the
  // Element has an AccessibleNode. Sets |isNull| if the property and
  // attribute are not present.
  static bool GetProperty(Element*, AOMBooleanProperty, bool& is_null);
  static float GetProperty(Element*, AOMFloatProperty, bool& is_null);
  static int32_t GetProperty(Element*, AOMIntProperty, bool& is_null);
  static uint32_t GetProperty(Element*, AOMUIntProperty, bool& is_null);

  // Returns the value of the given string property if the
  // Element has an AccessibleNode, otherwise returns the equivalent
  // ARIA attribute.
  static const AtomicString& GetPropertyOrARIAAttribute(Element*,
                                                        AOMStringProperty);

  // Returns the value of the given property if the
  // Element has an AccessibleNode, otherwise returns the equivalent
  // ARIA attribute. Sets |isNull| if the property and attribute are not
  // present.
  static bool GetPropertyOrARIAAttribute(Element*,
                                         AOMBooleanProperty,
                                         bool& is_null);
  static float GetPropertyOrARIAAttribute(Element*,
                                          AOMFloatProperty,
                                          bool& is_null);
  static int32_t GetPropertyOrARIAAttribute(Element*,
                                            AOMIntProperty,
                                            bool& is_null);
  static uint32_t GetPropertyOrARIAAttribute(Element*,
                                             AOMUIntProperty,
                                             bool& is_null);

  bool atomic(bool& is_null) const;
  void setAtomic(bool, bool is_null);

  AtomicString autocomplete() const;
  void setAutocomplete(const AtomicString&);

  bool busy(bool& is_null) const;
  void setBusy(bool, bool is_null);

  AtomicString checked() const;
  void setChecked(const AtomicString&);

  int32_t colCount(bool& is_null) const;
  void setColCount(int32_t, bool is_null);

  uint32_t colIndex(bool& is_null) const;
  void setColIndex(uint32_t, bool is_null);

  uint32_t colSpan(bool& is_null) const;
  void setColSpan(uint32_t, bool is_null);

  AtomicString current() const;
  void setCurrent(const AtomicString&);

  bool disabled(bool& is_null) const;
  void setDisabled(bool, bool is_null);

  bool expanded(bool& is_null) const;
  void setExpanded(bool, bool is_null);

  bool hidden(bool& is_null) const;
  void setHidden(bool, bool is_null);

  AtomicString invalid() const;
  void setInvalid(const AtomicString&);

  AtomicString keyShortcuts() const;
  void setKeyShortcuts(const AtomicString&);

  AtomicString label() const;
  void setLabel(const AtomicString&);

  uint32_t level(bool& is_null) const;
  void setLevel(uint32_t, bool is_null);

  AtomicString live() const;
  void setLive(const AtomicString&);

  bool modal(bool& is_null) const;
  void setModal(bool, bool is_null);

  bool multiline(bool& is_null) const;
  void setMultiline(bool, bool is_null);

  bool multiselectable(bool& is_null) const;
  void setMultiselectable(bool, bool is_null);

  AtomicString orientation() const;
  void setOrientation(const AtomicString&);

  AtomicString placeholder() const;
  void setPlaceholder(const AtomicString&);

  uint32_t posInSet(bool& is_null) const;
  void setPosInSet(uint32_t, bool is_null);

  bool readOnly(bool& is_null) const;
  void setReadOnly(bool, bool is_null);

  AtomicString relevant() const;
  void setRelevant(const AtomicString&);

  bool required(bool& is_null) const;
  void setRequired(bool, bool is_null);

  AtomicString role() const;
  void setRole(const AtomicString&);

  AtomicString roleDescription() const;
  void setRoleDescription(const AtomicString&);

  int32_t rowCount(bool& is_null) const;
  void setRowCount(int32_t, bool is_null);

  uint32_t rowIndex(bool& is_null) const;
  void setRowIndex(uint32_t, bool is_null);

  uint32_t rowSpan(bool& is_null) const;
  void setRowSpan(uint32_t, bool is_null);

  bool selected(bool& is_null) const;
  void setSelected(bool, bool is_null);

  int32_t setSize(bool& is_null) const;
  void setSetSize(int32_t, bool is_null);

  AtomicString sort() const;
  void setSort(const AtomicString&);

  float valueMax(bool& is_null) const;
  void setValueMax(float, bool is_null);

  float valueMin(bool& is_null) const;
  void setValueMin(float, bool is_null);

  float valueNow(bool& is_null) const;
  void setValueNow(float, bool is_null);

  AtomicString valueText() const;
  void setValueText(const AtomicString&);

  DECLARE_VIRTUAL_TRACE();

 private:
  void SetStringProperty(AOMStringProperty, const AtomicString&);
  void SetBooleanProperty(AOMBooleanProperty, bool value, bool is_null);
  void SetFloatProperty(AOMFloatProperty, float value, bool is_null);
  void SetUIntProperty(AOMUIntProperty, uint32_t value, bool is_null);
  void SetIntProperty(AOMIntProperty, int32_t value, bool is_null);
  void NotifyAttributeChanged(const blink::QualifiedName&);
  AXObjectCache* GetAXObjectCache();

  Vector<std::pair<AOMStringProperty, AtomicString>> string_properties_;
  Vector<std::pair<AOMBooleanProperty, bool>> boolean_properties_;
  Vector<std::pair<AOMFloatProperty, float>> float_properties_;
  Vector<std::pair<AOMIntProperty, int32_t>> int_properties_;
  Vector<std::pair<AOMUIntProperty, uint32_t>> uint_properties_;

  // This object's owner Element.
  Member<Element> element_;
};

}  // namespace blink

#endif  // AccessibleNode_h
