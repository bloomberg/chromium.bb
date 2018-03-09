// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePropertyMapReadOnly_h
#define StylePropertyMapReadOnly_h

#include "base/macros.h"
#include "bindings/core/v8/Iterable.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css_property_names.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class CSSProperty;

class CORE_EXPORT StylePropertyMapReadOnly
    : public ScriptWrappable,
      public PairIterable<String, CSSStyleValueVector> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using StylePropertyMapEntry = std::pair<String, CSSStyleValueVector>;

  virtual ~StylePropertyMapReadOnly() = default;

  CSSStyleValue* get(const String& property_name, ExceptionState&);
  CSSStyleValueVector getAll(const String& property_name, ExceptionState&);
  bool has(const String& property_name, ExceptionState&);

  virtual unsigned int size() = 0;

 protected:
  StylePropertyMapReadOnly() = default;

  virtual const CSSValue* GetProperty(CSSPropertyID) = 0;
  virtual const CSSValue* GetCustomProperty(AtomicString) = 0;

  using IterationCallback =
      std::function<void(const AtomicString&, const CSSValue&)>;
  virtual void ForEachProperty(const IterationCallback&) = 0;

  virtual String SerializationForShorthand(const CSSProperty&) = 0;

 private:
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;

  CSSStyleValue* GetShorthandProperty(const CSSProperty&);

 private:
  DISALLOW_COPY_AND_ASSIGN(StylePropertyMapReadOnly);
};

}  // namespace blink

#endif
