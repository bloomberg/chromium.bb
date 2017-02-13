// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/UseCounterCallback.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"

namespace blink {

void useCounterCallback(v8::Isolate* isolate,
                        v8::Isolate::UseCounterFeature feature) {
  if (V8PerIsolateData::from(isolate)->isUseCounterDisabled())
    return;

  UseCounter::Feature blinkFeature;
  bool deprecated = false;
  switch (feature) {
    case v8::Isolate::kUseAsm:
      blinkFeature = UseCounter::UseAsm;
      break;
    case v8::Isolate::kBreakIterator:
      blinkFeature = UseCounter::BreakIterator;
      break;
    case v8::Isolate::kLegacyConst:
      blinkFeature = UseCounter::LegacyConst;
      break;
    case v8::Isolate::kSloppyMode:
      blinkFeature = UseCounter::V8SloppyMode;
      break;
    case v8::Isolate::kStrictMode:
      blinkFeature = UseCounter::V8StrictMode;
      break;
    case v8::Isolate::kStrongMode:
      blinkFeature = UseCounter::V8StrongMode;
      break;
    case v8::Isolate::kRegExpPrototypeStickyGetter:
      blinkFeature = UseCounter::V8RegExpPrototypeStickyGetter;
      break;
    case v8::Isolate::kRegExpPrototypeToString:
      blinkFeature = UseCounter::V8RegExpPrototypeToString;
      break;
    case v8::Isolate::kRegExpPrototypeUnicodeGetter:
      blinkFeature = UseCounter::V8RegExpPrototypeUnicodeGetter;
      break;
    case v8::Isolate::kIntlV8Parse:
      blinkFeature = UseCounter::V8IntlV8Parse;
      break;
    case v8::Isolate::kIntlPattern:
      blinkFeature = UseCounter::V8IntlPattern;
      break;
    case v8::Isolate::kIntlResolved:
      blinkFeature = UseCounter::V8IntlResolved;
      break;
    case v8::Isolate::kPromiseChain:
      blinkFeature = UseCounter::V8PromiseChain;
      break;
    case v8::Isolate::kPromiseAccept:
      blinkFeature = UseCounter::V8PromiseAccept;
      break;
    case v8::Isolate::kPromiseDefer:
      blinkFeature = UseCounter::V8PromiseDefer;
      break;
    case v8::Isolate::kHtmlCommentInExternalScript:
      blinkFeature = UseCounter::V8HTMLCommentInExternalScript;
      break;
    case v8::Isolate::kHtmlComment:
      blinkFeature = UseCounter::V8HTMLComment;
      break;
    case v8::Isolate::kSloppyModeBlockScopedFunctionRedefinition:
      blinkFeature = UseCounter::V8SloppyModeBlockScopedFunctionRedefinition;
      break;
    case v8::Isolate::kForInInitializer:
      blinkFeature = UseCounter::V8ForInInitializer;
      break;
    case v8::Isolate::kArrayProtectorDirtied:
      blinkFeature = UseCounter::V8ArrayProtectorDirtied;
      break;
    case v8::Isolate::kArraySpeciesModified:
      blinkFeature = UseCounter::V8ArraySpeciesModified;
      break;
    case v8::Isolate::kArrayPrototypeConstructorModified:
      blinkFeature = UseCounter::V8ArrayPrototypeConstructorModified;
      break;
    case v8::Isolate::kArrayInstanceProtoModified:
      blinkFeature = UseCounter::V8ArrayInstanceProtoModified;
      break;
    case v8::Isolate::kArrayInstanceConstructorModified:
      blinkFeature = UseCounter::V8ArrayInstanceConstructorModified;
      break;
    case v8::Isolate::kLegacyFunctionDeclaration:
      blinkFeature = UseCounter::V8LegacyFunctionDeclaration;
      break;
    case v8::Isolate::kRegExpPrototypeSourceGetter:
      blinkFeature = UseCounter::V8RegExpPrototypeSourceGetter;
      break;
    case v8::Isolate::kRegExpPrototypeOldFlagGetter:
      blinkFeature = UseCounter::V8RegExpPrototypeOldFlagGetter;
      break;
    case v8::Isolate::kDecimalWithLeadingZeroInStrictMode:
      blinkFeature = UseCounter::V8DecimalWithLeadingZeroInStrictMode;
      break;
    case v8::Isolate::kLegacyDateParser:
      blinkFeature = UseCounter::V8LegacyDateParser;
      break;
    case v8::Isolate::kDefineGetterOrSetterWouldThrow:
      blinkFeature = UseCounter::V8DefineGetterOrSetterWouldThrow;
      break;
    case v8::Isolate::kFunctionConstructorReturnedUndefined:
      blinkFeature = UseCounter::V8FunctionConstructorReturnedUndefined;
      break;
    case v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy:
      blinkFeature = UseCounter::V8AssigmentExpressionLHSIsCallInSloppy;
      break;
    case v8::Isolate::kAssigmentExpressionLHSIsCallInStrict:
      blinkFeature = UseCounter::V8AssigmentExpressionLHSIsCallInStrict;
      break;
    case v8::Isolate::kPromiseConstructorReturnedUndefined:
      blinkFeature = UseCounter::V8PromiseConstructorReturnedUndefined;
      break;
    default:
      // This can happen if V8 has added counters that this version of Blink
      // does not know about. It's harmless.
      return;
  }
  if (deprecated) {
    Deprecation::countDeprecation(currentExecutionContext(isolate),
                                  blinkFeature);
  } else {
    UseCounter::count(currentExecutionContext(isolate), blinkFeature);
  }
}

}  // namespace blink
