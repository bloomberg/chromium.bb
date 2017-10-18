// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationTestHelper_h
#define AnimationTestHelper_h

#include "core/animation/InterpolableValue.h"
#include "core/animation/Interpolation.h"
#include "core/animation/LegacyStyleInterpolation.h"
#include "platform/wtf/text/StringView.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8.h"

namespace blink {

class Document;
class Element;

void SetV8ObjectPropertyAsString(v8::Isolate*,
                                 v8::Local<v8::Object>,
                                 const StringView& name,
                                 const StringView& value);
void SetV8ObjectPropertyAsNumber(v8::Isolate*,
                                 v8::Local<v8::Object>,
                                 const StringView& name,
                                 double value);

// Ensures that a set of interpolations actually computes and caches their
// internal interpolated value, so that tests can retrieve them.
//
// All members of the ActiveInterpolations must be instances of
// InvalidatableInterpolation.
void EnsureInterpolatedValueCached(const ActiveInterpolations&,
                                   Document&,
                                   Element*);

class SampleTestInterpolation : public LegacyStyleInterpolation {
 public:
  static RefPtr<LegacyStyleInterpolation> Create(
      std::unique_ptr<InterpolableValue> start,
      std::unique_ptr<InterpolableValue> end) {
    return WTF::AdoptRef(
        new SampleTestInterpolation(std::move(start), std::move(end)));
  }

 private:
  SampleTestInterpolation(std::unique_ptr<InterpolableValue> start,
                          std::unique_ptr<InterpolableValue> end)
      : LegacyStyleInterpolation(std::move(start),
                                 std::move(end),
                                 CSSPropertyBackgroundColor) {}
};

}  // namespace blink

#endif  // AnimationTestHelper_h
