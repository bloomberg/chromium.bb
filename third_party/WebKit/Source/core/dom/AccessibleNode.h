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

  // Returns the value of the given boolean property if the
  // Element has an AccessibleNode. Sets |isNull| if the property and
  // attribute are not present.
  static bool GetProperty(Element*, AOMBooleanProperty, bool& is_null);

  // Returns the value of the given string property if the
  // Element has an AccessibleNode, otherwise returns the equivalent
  // ARIA attribute.
  static const AtomicString& GetPropertyOrARIAAttribute(Element*,
                                                        AOMStringProperty);

  // Returns the value of the given boolean property if the
  // Element has an AccessibleNode, otherwise returns the equivalent
  // ARIA attribute. Sets |isNull| if the property and attribute are not
  // present.
  static bool GetPropertyOrARIAAttribute(Element*,
                                         AOMBooleanProperty,
                                         bool& is_null);

  bool atomic(bool&) const;
  void setAtomic(bool, bool is_null);

  AtomicString autocomplete() const;
  void setAutocomplete(const AtomicString&);

  bool busy(bool&) const;
  void setBusy(bool, bool is_null);

  AtomicString checked() const;
  void setChecked(const AtomicString&);

  AtomicString current() const;
  void setCurrent(const AtomicString&);

  bool disabled(bool&) const;
  void setDisabled(bool, bool is_null);

  bool expanded(bool&) const;
  void setExpanded(bool, bool is_null);

  bool hidden(bool&) const;
  void setHidden(bool, bool is_null);

  AtomicString invalid() const;
  void setInvalid(const AtomicString&);

  AtomicString keyShortcuts() const;
  void setKeyShortcuts(const AtomicString&);

  AtomicString label() const;
  void setLabel(const AtomicString&);

  AtomicString live() const;
  void setLive(const AtomicString&);

  bool modal(bool&) const;
  void setModal(bool, bool is_null);

  bool multiline(bool&) const;
  void setMultiline(bool, bool is_null);

  bool multiselectable(bool&) const;
  void setMultiselectable(bool, bool is_null);

  AtomicString orientation() const;
  void setOrientation(const AtomicString&);

  AtomicString placeholder() const;
  void setPlaceholder(const AtomicString&);

  bool readOnly(bool&) const;
  void setReadOnly(bool, bool is_null);

  AtomicString relevant() const;
  void setRelevant(const AtomicString&);

  bool required(bool&) const;
  void setRequired(bool, bool is_null);

  AtomicString role() const;
  void setRole(const AtomicString&);

  AtomicString roleDescription() const;
  void setRoleDescription(const AtomicString&);

  bool selected(bool&) const;
  void setSelected(bool, bool is_null);

  AtomicString sort() const;
  void setSort(const AtomicString&);

  AtomicString valueText() const;
  void setValueText(const AtomicString&);

  DECLARE_VIRTUAL_TRACE();

 private:
  void SetStringProperty(AOMStringProperty, const AtomicString&);
  void SetBooleanProperty(AOMBooleanProperty, bool value, bool is_null);
  void NotifyAttributeChanged(const blink::QualifiedName&);
  AXObjectCache* GetAXObjectCache();

  Vector<std::pair<AOMStringProperty, AtomicString>> string_properties_;
  Vector<std::pair<AOMBooleanProperty, bool>> boolean_properties_;

  // This object's owner Element.
  Member<Element> element_;
};

}  // namespace blink

#endif  // AccessibleNode_h
