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

  // Returns the given string property if the Element has an AccessibleNode,
  // otherwise returns the equivalent ARIA attribute.
  static const AtomicString& GetPropertyOrARIAAttribute(Element*,
                                                        AOMStringProperty);

  AtomicString autocomplete() const;
  void setAutocomplete(const AtomicString&);

  AtomicString checked() const;
  void setChecked(const AtomicString&);

  AtomicString current() const;
  void setCurrent(const AtomicString&);

  AtomicString invalid() const;
  void setInvalid(const AtomicString&);

  AtomicString keyShortcuts() const;
  void setKeyShortcuts(const AtomicString&);

  AtomicString label() const;
  void setLabel(const AtomicString&);

  AtomicString live() const;
  void setLive(const AtomicString&);

  AtomicString orientation() const;
  void setOrientation(const AtomicString&);

  AtomicString placeholder() const;
  void setPlaceholder(const AtomicString&);

  AtomicString relevant() const;
  void setRelevant(const AtomicString&);

  AtomicString role() const;
  void setRole(const AtomicString&);

  AtomicString roleDescription() const;
  void setRoleDescription(const AtomicString&);

  AtomicString sort() const;
  void setSort(const AtomicString&);

  AtomicString valueText() const;
  void setValueText(const AtomicString&);

  DECLARE_VIRTUAL_TRACE();

 private:
  void SetStringProperty(AOMStringProperty, const AtomicString&);
  void NotifyAttributeChanged(const blink::QualifiedName&);
  AXObjectCache* GetAXObjectCache();

  Vector<std::pair<AOMStringProperty, AtomicString>> string_properties_;

  // This object's owner Element.
  Member<Element> element_;
};

}  // namespace blink

#endif  // AccessibleNode_h
