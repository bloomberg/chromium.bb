// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/UseCounterCallback.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"

namespace blink {

void UseCounterCallback(v8::Isolate* isolate,
                        v8::Isolate::UseCounterFeature feature) {
  if (V8PerIsolateData::From(isolate)->IsUseCounterDisabled())
    return;

  UseCounter::Feature blink_feature;
  bool deprecated = false;
  switch (feature) {
    case v8::Isolate::kUseAsm:
      blink_feature = UseCounter::kUseAsm;
      break;
    case v8::Isolate::kBreakIterator:
      blink_feature = UseCounter::kBreakIterator;
      break;
    case v8::Isolate::kLegacyConst:
      blink_feature = UseCounter::kLegacyConst;
      break;
    case v8::Isolate::kSloppyMode:
      blink_feature = UseCounter::kV8SloppyMode;
      break;
    case v8::Isolate::kStrictMode:
      blink_feature = UseCounter::kV8StrictMode;
      break;
    case v8::Isolate::kStrongMode:
      blink_feature = UseCounter::kV8StrongMode;
      break;
    case v8::Isolate::kRegExpPrototypeStickyGetter:
      blink_feature = UseCounter::kV8RegExpPrototypeStickyGetter;
      break;
    case v8::Isolate::kRegExpPrototypeToString:
      blink_feature = UseCounter::kV8RegExpPrototypeToString;
      break;
    case v8::Isolate::kRegExpPrototypeUnicodeGetter:
      blink_feature = UseCounter::kV8RegExpPrototypeUnicodeGetter;
      break;
    case v8::Isolate::kIntlV8Parse:
      blink_feature = UseCounter::kV8IntlV8Parse;
      break;
    case v8::Isolate::kIntlPattern:
      blink_feature = UseCounter::kV8IntlPattern;
      break;
    case v8::Isolate::kIntlResolved:
      blink_feature = UseCounter::kV8IntlResolved;
      break;
    case v8::Isolate::kPromiseChain:
      blink_feature = UseCounter::kV8PromiseChain;
      break;
    case v8::Isolate::kPromiseAccept:
      blink_feature = UseCounter::kV8PromiseAccept;
      break;
    case v8::Isolate::kPromiseDefer:
      blink_feature = UseCounter::kV8PromiseDefer;
      break;
    case v8::Isolate::kHtmlCommentInExternalScript:
      blink_feature = UseCounter::kV8HTMLCommentInExternalScript;
      break;
    case v8::Isolate::kHtmlComment:
      blink_feature = UseCounter::kV8HTMLComment;
      break;
    case v8::Isolate::kSloppyModeBlockScopedFunctionRedefinition:
      blink_feature = UseCounter::kV8SloppyModeBlockScopedFunctionRedefinition;
      break;
    case v8::Isolate::kForInInitializer:
      blink_feature = UseCounter::kV8ForInInitializer;
      break;
    case v8::Isolate::kArrayProtectorDirtied:
      blink_feature = UseCounter::kV8ArrayProtectorDirtied;
      break;
    case v8::Isolate::kArraySpeciesModified:
      blink_feature = UseCounter::kV8ArraySpeciesModified;
      break;
    case v8::Isolate::kArrayPrototypeConstructorModified:
      blink_feature = UseCounter::kV8ArrayPrototypeConstructorModified;
      break;
    case v8::Isolate::kArrayInstanceProtoModified:
      blink_feature = UseCounter::kV8ArrayInstanceProtoModified;
      break;
    case v8::Isolate::kArrayInstanceConstructorModified:
      blink_feature = UseCounter::kV8ArrayInstanceConstructorModified;
      break;
    case v8::Isolate::kLegacyFunctionDeclaration:
      blink_feature = UseCounter::kV8LegacyFunctionDeclaration;
      break;
    case v8::Isolate::kRegExpPrototypeSourceGetter:
      blink_feature = UseCounter::kV8RegExpPrototypeSourceGetter;
      break;
    case v8::Isolate::kRegExpPrototypeOldFlagGetter:
      blink_feature = UseCounter::kV8RegExpPrototypeOldFlagGetter;
      break;
    case v8::Isolate::kDecimalWithLeadingZeroInStrictMode:
      blink_feature = UseCounter::kV8DecimalWithLeadingZeroInStrictMode;
      break;
    case v8::Isolate::kLegacyDateParser:
      blink_feature = UseCounter::kV8LegacyDateParser;
      break;
    case v8::Isolate::kDefineGetterOrSetterWouldThrow:
      blink_feature = UseCounter::kV8DefineGetterOrSetterWouldThrow;
      break;
    case v8::Isolate::kFunctionConstructorReturnedUndefined:
      blink_feature = UseCounter::kV8FunctionConstructorReturnedUndefined;
      break;
    case v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy:
      blink_feature = UseCounter::kV8AssigmentExpressionLHSIsCallInSloppy;
      break;
    case v8::Isolate::kAssigmentExpressionLHSIsCallInStrict:
      blink_feature = UseCounter::kV8AssigmentExpressionLHSIsCallInStrict;
      break;
    case v8::Isolate::kPromiseConstructorReturnedUndefined:
      blink_feature = UseCounter::kV8PromiseConstructorReturnedUndefined;
      break;
    default:
      // This can happen if V8 has added counters that this version of Blink
      // does not know about. It's harmless.
      return;
  }
  if (deprecated) {
    Deprecation::CountDeprecation(CurrentExecutionContext(isolate),
                                  blink_feature);
  } else {
    UseCounter::Count(CurrentExecutionContext(isolate), blink_feature);
  }
}

}  // namespace blink
