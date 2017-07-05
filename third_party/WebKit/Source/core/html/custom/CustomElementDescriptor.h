// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDescriptor_h
#define CustomElementDescriptor_h

#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashTableDeletedValueType.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

// Describes what elements a custom element definition applies to.
// https://html.spec.whatwg.org/multipage/scripting.html#custom-elements-core-concepts
//
// There are two kinds of definitions:
//
// [Autonomous] These have their own tag name. In that case "name"
// (the definition name) and local name (the tag name) are identical.
//
// [Customized built-in] The name is still a valid custom element
// name; but the local name will be a built-in element name, or an
// unknown element name that is *not* a valid custom element name.
//
// CustomElementDescriptor used when the kind of custom element
// definition is known, and generally the difference is important. For
// example, a definition for "my-element", "my-element" must not be
// applied to an element <button is="my-element">.
class CORE_EXPORT CustomElementDescriptor final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  CustomElementDescriptor() {}

  CustomElementDescriptor(const AtomicString& name,
                          const AtomicString& local_name)
      : name_(name), local_name_(local_name) {}

  explicit CustomElementDescriptor(WTF::HashTableDeletedValueType value)
      : name_(value) {}

  bool IsHashTableDeletedValue() const {
    return name_.IsHashTableDeletedValue();
  }

  bool operator==(const CustomElementDescriptor& other) const {
    return name_ == other.name_ && local_name_ == other.local_name_;
  }

  const AtomicString& GetName() const { return name_; }
  const AtomicString& LocalName() const { return local_name_; }

  bool Matches(const Element& element) const {
    return LocalName() == element.localName() &&
           (IsAutonomous() ||
            GetName() == element.getAttribute(HTMLNames::isAttr)) &&
           element.namespaceURI() == HTMLNames::xhtmlNamespaceURI;
  }

  bool IsAutonomous() const { return name_ == local_name_; }

 private:
  AtomicString name_;
  AtomicString local_name_;
};

}  // namespace blink

#endif  // CustomElementDescriptor_h
