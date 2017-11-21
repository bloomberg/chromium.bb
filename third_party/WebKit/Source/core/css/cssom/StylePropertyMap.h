// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePropertyMap_h
#define StylePropertyMap_h

#include "bindings/core/v8/v8_update_function.h"
#include "core/css/cssom/StylePropertyMapReadonly.h"

namespace blink {

class ExceptionState;
class ExecutionContext;

class CORE_EXPORT StylePropertyMap : public StylePropertyMapReadonly {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(StylePropertyMap);

 public:
  void set(const ExecutionContext*,
           const String& property_name,
           CSSStyleValueOrCSSStyleValueSequenceOrString& item,
           ExceptionState&);
  void append(const ExecutionContext*,
              const String& property_name,
              CSSStyleValueOrCSSStyleValueSequenceOrString& item,
              ExceptionState&);
  void remove(const String& property_name, ExceptionState&);
  void update(const String&, const V8UpdateFunction*) {}

  virtual void set(const ExecutionContext*,
                   CSSPropertyID,
                   CSSStyleValueOrCSSStyleValueSequenceOrString& item,
                   ExceptionState&) = 0;
  virtual void append(const ExecutionContext*,
                      CSSPropertyID,
                      CSSStyleValueOrCSSStyleValueSequenceOrString& item,
                      ExceptionState&) = 0;
  virtual void remove(CSSPropertyID, ExceptionState&) = 0;

 protected:
  StylePropertyMap() {}

};

}  // namespace blink

#endif
