// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePropertyMap_h
#define StylePropertyMap_h

#include "base/macros.h"
#include "bindings/core/v8/css_style_value_or_string.h"
#include "bindings/core/v8/v8_update_function.h"
#include "core/css/cssom/StylePropertyMapReadOnly.h"

namespace blink {

class ExceptionState;
class ExecutionContext;

class CORE_EXPORT StylePropertyMap : public StylePropertyMapReadOnly {
  DEFINE_WRAPPERTYPEINFO();

 public:
  void set(const ExecutionContext*,
           const String& property_name,
           const HeapVector<CSSStyleValueOrString>& values,
           ExceptionState&);
  void append(const ExecutionContext*,
              const String& property_name,
              const HeapVector<CSSStyleValueOrString>& values,
              ExceptionState&);
  void remove(const String& property_name, ExceptionState&);
  void update(const String&, V8UpdateFunction*, ExceptionState&);

 protected:
  virtual void SetProperty(CSSPropertyID, const CSSValue&) = 0;
  virtual void SetCustomProperty(const AtomicString&, const CSSValue&) = 0;
  virtual void RemoveProperty(CSSPropertyID) = 0;
  virtual void RemoveCustomProperty(const AtomicString&) = 0;

  StylePropertyMap() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(StylePropertyMap);
};

}  // namespace blink

#endif
