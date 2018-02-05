// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSURLImageValue_h
#define CSSURLImageValue_h

#include "base/macros.h"
#include "core/css/cssom/CSSStyleImageValue.h"

namespace blink {

class ScriptState;
class CSSImageValue;

class CORE_EXPORT CSSURLImageValue final : public CSSStyleImageValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSURLImageValue* Create(ScriptState*,
                                  const AtomicString& url,
                                  ExceptionState&);

  static CSSURLImageValue* FromCSSValue(const CSSImageValue&);

  const String& url() const;

  // CSSStyleImageValue
  WTF::Optional<IntSize> IntrinsicSize() const final;

  // CanvasImageSource
  ResourceStatus Status() const final;
  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               AccelerationHint,
                                               const FloatSize&) final;
  bool IsAccelerated() const final;

  // CSSStyleValue
  StyleValueType GetType() const final { return kURLImageType; }
  const CSSValue* ToCSSValue() const final;

  virtual void Trace(blink::Visitor*);

 private:
  explicit CSSURLImageValue(const CSSImageValue& value) : value_(value) {}

  scoped_refptr<Image> GetImage() const;

  Member<const CSSImageValue> value_;
  DISALLOW_COPY_AND_ASSIGN(CSSURLImageValue);
};

}  // namespace blink

#endif  // CSSResourceValue_h
