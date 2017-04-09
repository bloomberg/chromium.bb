// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePropertyMap_h
#define StylePropertyMap_h

#include "core/css/cssom/StylePropertyMapReadonly.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT StylePropertyMap : public StylePropertyMapReadonly {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(StylePropertyMap);

 public:
  void set(const String& property_name,
           CSSStyleValueOrCSSStyleValueSequenceOrString& item,
           ExceptionState&);
  void append(const String& property_name,
              CSSStyleValueOrCSSStyleValueSequenceOrString& item,
              ExceptionState&);
  void remove(const String& property_name, ExceptionState&);

  virtual void set(CSSPropertyID,
                   CSSStyleValueOrCSSStyleValueSequenceOrString& item,
                   ExceptionState&) = 0;
  virtual void append(CSSPropertyID,
                      CSSStyleValueOrCSSStyleValueSequenceOrString& item,
                      ExceptionState&) = 0;
  virtual void remove(CSSPropertyID, ExceptionState&) = 0;

 protected:
  StylePropertyMap() {}

};

}  // namespace blink

#endif
