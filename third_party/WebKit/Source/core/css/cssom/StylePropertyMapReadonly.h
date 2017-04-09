// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StylePropertyMapReadonly_h
#define StylePropertyMapReadonly_h

#include "bindings/core/v8/CSSStyleValueOrCSSStyleValueSequence.h"
#include "bindings/core/v8/CSSStyleValueOrCSSStyleValueSequenceOrString.h"
#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class CORE_EXPORT StylePropertyMapReadonly
    : public GarbageCollectedFinalized<StylePropertyMapReadonly>,
      public ScriptWrappable,
      public PairIterable<String, CSSStyleValueOrCSSStyleValueSequence> {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(StylePropertyMapReadonly);

 public:
  typedef std::pair<String, CSSStyleValueOrCSSStyleValueSequence>
      StylePropertyMapEntry;

  virtual ~StylePropertyMapReadonly() {}

  virtual CSSStyleValue* get(const String& property_name, ExceptionState&);
  virtual CSSStyleValueVector getAll(const String& property_name,
                                     ExceptionState&);
  virtual bool has(const String& property_name, ExceptionState&);

  virtual Vector<String> getProperties() = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  StylePropertyMapReadonly() = default;

  virtual CSSStyleValueVector GetAllInternal(CSSPropertyID) = 0;
  virtual CSSStyleValueVector GetAllInternal(
      AtomicString custom_property_name) = 0;

  virtual HeapVector<StylePropertyMapEntry> GetIterationEntries() = 0;
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;
};

}  // namespace blink

#endif
