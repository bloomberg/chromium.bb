// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_LONGHAND_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_LONGHAND_H_

#include "third_party/blink/renderer/core/css/properties/css_property.h"

#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/scoped_css_value.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSValue;
class CSSParserContext;
class CSSParserLocalContext;
class CSSParserTokenRange;

class Longhand : public CSSProperty {
 public:
  // Parses and consumes a longhand property value from the token range.
  // Returns nullptr if the input is invalid.
  virtual const CSSValue* ParseSingleValue(CSSParserTokenRange&,
                                           const CSSParserContext&,
                                           const CSSParserLocalContext&) const {
    return nullptr;
  }
  virtual void ApplyInitial(StyleResolverState&) const { NOTREACHED(); }
  virtual void ApplyInherit(StyleResolverState&) const { NOTREACHED(); }
  // Properties which take tree-scoped references should override this method to
  // handle the TreeScope during application.
  virtual void ApplyValue(StyleResolverState& state,
                          const ScopedCSSValue& scoped_value) const {
    ApplyValue(state, scoped_value.GetCSSValue());
  }
  virtual void ApplyValue(StyleResolverState&, const CSSValue&) const {
    NOTREACHED();
  }
  void ApplyUnset(StyleResolverState& state) const {
    if (state.IsInheritedForUnset(*this))
      ApplyInherit(state);
    else
      ApplyInitial(state);
  }
  virtual const blink::Color ColorIncludingFallback(bool, const ComputedStyle&)
      const {
    NOTREACHED();
    return Color();
  }
  virtual const CSSValue* InitialValue() const {
    return CSSInitialValue::Create();
  }

 protected:
  constexpr Longhand(CSSPropertyID id, Flags flags, char repetition_separator)
      : CSSProperty(id, flags | kLonghand, repetition_separator) {}
};

template <>
struct DowncastTraits<Longhand> {
  static bool AllowFrom(const CSSProperty& longhand) {
    return longhand.IsLonghand();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_LONGHAND_H_
