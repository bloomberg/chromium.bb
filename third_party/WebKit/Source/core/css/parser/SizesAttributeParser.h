// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SizesAttributeParser_h
#define SizesAttributeParser_h

#include "core/CoreExport.h"
#include "core/css/MediaValues.h"
#include "core/css/parser/MediaQueryBlockWatcher.h"
#include "core/css/parser/MediaQueryParser.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT SizesAttributeParser {
  STACK_ALLOCATED();

 public:
  SizesAttributeParser(MediaValues*, const String&);

  float length();

 private:
  bool Parse(CSSParserTokenRange);
  float EffectiveSize();
  bool CalculateLengthInPixels(CSSParserTokenRange, float& result);
  bool MediaConditionMatches(const MediaQuerySet& media_condition);
  float EffectiveSizeDefaultValue();

  scoped_refptr<MediaQuerySet> media_condition_;
  Member<MediaValues> media_values_;
  float length_;
  bool length_was_set_;
  bool is_valid_;
};

}  // namespace blink

#endif
