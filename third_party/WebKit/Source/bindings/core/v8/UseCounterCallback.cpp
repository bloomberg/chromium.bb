// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/UseCounterCallback.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/frame/Deprecation.h"
#include "core/frame/UseCounter.h"
#include "platform/bindings/V8PerIsolateData.h"

namespace blink {

void UseCounterCallback(v8::Isolate* isolate,
                        v8::Isolate::UseCounterFeature feature) {
  if (V8PerIsolateData::From(isolate)->IsUseCounterDisabled())
    return;

  WebFeature blink_feature;
  bool deprecated = false;
  switch (feature) {
    case v8::Isolate::kUseAsm:
      blink_feature = WebFeature::kUseAsm;
      break;
    case v8::Isolate::kBreakIterator:
      blink_feature = WebFeature::kBreakIterator;
      break;
    case v8::Isolate::kLegacyConst:
      blink_feature = WebFeature::kLegacyConst;
      break;
    case v8::Isolate::kSloppyMode:
      blink_feature = WebFeature::kV8SloppyMode;
      break;
    case v8::Isolate::kStrictMode:
      blink_feature = WebFeature::kV8StrictMode;
      break;
    case v8::Isolate::kStrongMode:
      blink_feature = WebFeature::kV8StrongMode;
      break;
    case v8::Isolate::kRegExpPrototypeStickyGetter:
      blink_feature = WebFeature::kV8RegExpPrototypeStickyGetter;
      break;
    case v8::Isolate::kRegExpPrototypeToString:
      blink_feature = WebFeature::kV8RegExpPrototypeToString;
      break;
    case v8::Isolate::kRegExpPrototypeUnicodeGetter:
      blink_feature = WebFeature::kV8RegExpPrototypeUnicodeGetter;
      break;
    case v8::Isolate::kIntlV8Parse:
      blink_feature = WebFeature::kV8IntlV8Parse;
      break;
    case v8::Isolate::kIntlPattern:
      blink_feature = WebFeature::kV8IntlPattern;
      break;
    case v8::Isolate::kIntlResolved:
      blink_feature = WebFeature::kV8IntlResolved;
      break;
    case v8::Isolate::kPromiseChain:
      blink_feature = WebFeature::kV8PromiseChain;
      break;
    case v8::Isolate::kPromiseAccept:
      blink_feature = WebFeature::kV8PromiseAccept;
      break;
    case v8::Isolate::kPromiseDefer:
      blink_feature = WebFeature::kV8PromiseDefer;
      break;
    case v8::Isolate::kHtmlCommentInExternalScript:
      blink_feature = WebFeature::kV8HTMLCommentInExternalScript;
      break;
    case v8::Isolate::kHtmlComment:
      blink_feature = WebFeature::kV8HTMLComment;
      break;
    case v8::Isolate::kSloppyModeBlockScopedFunctionRedefinition:
      blink_feature = WebFeature::kV8SloppyModeBlockScopedFunctionRedefinition;
      break;
    case v8::Isolate::kForInInitializer:
      blink_feature = WebFeature::kV8ForInInitializer;
      break;
    case v8::Isolate::kArrayProtectorDirtied:
      blink_feature = WebFeature::kV8ArrayProtectorDirtied;
      break;
    case v8::Isolate::kArraySpeciesModified:
      blink_feature = WebFeature::kV8ArraySpeciesModified;
      break;
    case v8::Isolate::kArrayPrototypeConstructorModified:
      blink_feature = WebFeature::kV8ArrayPrototypeConstructorModified;
      break;
    case v8::Isolate::kArrayInstanceProtoModified:
      blink_feature = WebFeature::kV8ArrayInstanceProtoModified;
      break;
    case v8::Isolate::kArrayInstanceConstructorModified:
      blink_feature = WebFeature::kV8ArrayInstanceConstructorModified;
      break;
    case v8::Isolate::kLegacyFunctionDeclaration:
      blink_feature = WebFeature::kV8LegacyFunctionDeclaration;
      break;
    case v8::Isolate::kRegExpPrototypeSourceGetter:
      blink_feature = WebFeature::kV8RegExpPrototypeSourceGetter;
      break;
    case v8::Isolate::kRegExpPrototypeOldFlagGetter:
      blink_feature = WebFeature::kV8RegExpPrototypeOldFlagGetter;
      break;
    case v8::Isolate::kDecimalWithLeadingZeroInStrictMode:
      blink_feature = WebFeature::kV8DecimalWithLeadingZeroInStrictMode;
      break;
    case v8::Isolate::kLegacyDateParser:
      blink_feature = WebFeature::kV8LegacyDateParser;
      break;
    case v8::Isolate::kDefineGetterOrSetterWouldThrow:
      blink_feature = WebFeature::kV8DefineGetterOrSetterWouldThrow;
      break;
    case v8::Isolate::kFunctionConstructorReturnedUndefined:
      blink_feature = WebFeature::kV8FunctionConstructorReturnedUndefined;
      break;
    case v8::Isolate::kAssigmentExpressionLHSIsCallInSloppy:
      blink_feature = WebFeature::kV8AssigmentExpressionLHSIsCallInSloppy;
      break;
    case v8::Isolate::kAssigmentExpressionLHSIsCallInStrict:
      blink_feature = WebFeature::kV8AssigmentExpressionLHSIsCallInStrict;
      break;
    case v8::Isolate::kPromiseConstructorReturnedUndefined:
      blink_feature = WebFeature::kV8PromiseConstructorReturnedUndefined;
      break;
    case v8::Isolate::kConstructorNonUndefinedPrimitiveReturn:
      blink_feature = WebFeature::kV8ConstructorNonUndefinedPrimitiveReturn;
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
