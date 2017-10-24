/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SVGStringListTearOff_h
#define SVGStringListTearOff_h

#include "core/svg/SVGStringList.h"
#include "core/svg/properties/SVGPropertyTearOff.h"

namespace blink {

class SVGStringListTearOff : public SVGPropertyTearOff<SVGStringList> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SVGStringListTearOff* Create(
      SVGStringList* target,
      SVGElement* context_element,
      PropertyIsAnimValType property_is_anim_val,
      const QualifiedName& attribute_name) {
    return new SVGStringListTearOff(target, context_element,
                                    property_is_anim_val, attribute_name);
  }

  // SVGStringList DOM interface:

  // WebIDL requires "unsigned long" type instead of size_t.
  unsigned long length() { return Target()->length(); }

  void clear(ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return;
    }
    Target()->clear();
    CommitChange();
  }

  String initialize(const String& item, ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return String();
    }
    Target()->Initialize(item);
    CommitChange();
    return item;
  }

  String getItem(unsigned long index, ExceptionState& exception_state) {
    return Target()->GetItem(index, exception_state);
  }

  String insertItemBefore(const String& item,
                          unsigned long index,
                          ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return String();
    }
    Target()->InsertItemBefore(item, index);
    CommitChange();
    return item;
  }

  String replaceItem(const String& item,
                     unsigned long index,
                     ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return String();
    }
    Target()->ReplaceItem(item, index, exception_state);
    CommitChange();
    return item;
  }

  bool AnonymousIndexedSetter(unsigned index,
                              const String& item,
                              ExceptionState& exception_state) {
    replaceItem(item, index, exception_state);
    return true;
  }

  String removeItem(unsigned long index, ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return String();
    }
    String removed_item = Target()->RemoveItem(index, exception_state);
    CommitChange();
    return removed_item;
  }

  String appendItem(const String& item, ExceptionState& exception_state) {
    if (IsImmutable()) {
      ThrowReadOnly(exception_state);
      return String();
    }
    Target()->AppendItem(item);
    CommitChange();
    return item;
  }

 protected:
  SVGStringListTearOff(SVGStringList*,
                       SVGElement*,
                       PropertyIsAnimValType,
                       const QualifiedName&);
};

}  // namespace blink

#endif  // SVGStringListTearOff_h
