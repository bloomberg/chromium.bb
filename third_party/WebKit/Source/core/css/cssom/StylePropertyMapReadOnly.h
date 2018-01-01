// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePropertyMapReadOnly_h
#define StylePropertyMapReadOnly_h

#include "base/macros.h"
#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/css_style_value_or_css_style_value_sequence.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CORE_EXPORT StylePropertyMapReadOnly
    : public ScriptWrappable,
      public PairIterable<String, CSSStyleValueOrCSSStyleValueSequence> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using StylePropertyMapEntry =
      std::pair<String, CSSStyleValueOrCSSStyleValueSequence>;

  virtual ~StylePropertyMapReadOnly() = default;

  CSSStyleValue* get(const String& property_name, ExceptionState&);
  CSSStyleValueVector getAll(const String& property_name, ExceptionState&);
  bool has(const String& property_name, ExceptionState&);

  Vector<String> getProperties();

 protected:
  StylePropertyMapReadOnly() = default;

  virtual const CSSValue* GetProperty(CSSPropertyID) = 0;
  virtual const CSSValue* GetCustomProperty(AtomicString) = 0;

  using IterationCallback =
      std::function<void(const AtomicString&, const CSSValue&)>;
  virtual void ForEachProperty(const IterationCallback&) = 0;

 private:
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StylePropertyMapReadOnly);
};

}  // namespace blink

#endif
