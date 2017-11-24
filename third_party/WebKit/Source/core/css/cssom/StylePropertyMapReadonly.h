// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePropertyMapReadonly_h
#define StylePropertyMapReadonly_h

#include "base/macros.h"
#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/css_style_value_or_css_style_value_sequence.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CORE_EXPORT StylePropertyMapReadonly
    : public ScriptWrappable,
      public PairIterable<String, CSSStyleValueOrCSSStyleValueSequence> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  typedef std::pair<String, CSSStyleValueOrCSSStyleValueSequence>
      StylePropertyMapEntry;

  virtual ~StylePropertyMapReadonly() = default;

  CSSStyleValue* get(const String& property_name, ExceptionState&);
  CSSStyleValueVector getAll(const String& property_name, ExceptionState&);
  bool has(const String& property_name, ExceptionState&);

  virtual Vector<String> getProperties() = 0;

 protected:
  StylePropertyMapReadonly() = default;

  virtual const CSSValue* GetProperty(CSSPropertyID) = 0;
  virtual const CSSValue* GetCustomProperty(AtomicString) = 0;

  virtual HeapVector<StylePropertyMapEntry> GetIterationEntries() = 0;
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StylePropertyMapReadonly);
};

}  // namespace blink

#endif
