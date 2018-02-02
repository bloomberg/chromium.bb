// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/EffectModel.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/animation/KeyframeEffectOptions.h"
#include "platform/runtime_enabled_features.h"

namespace blink {
EffectModel::CompositeOperation EffectModel::ExtractCompositeOperation(
    const KeyframeEffectOptions& options) {
  // We have to be careful to keep backwards compatible behavior here; no valid
  // value of KeyframeEffectOptions should throw, even if it is unsupported in
  // Chrome currently. Values we do not support should just be turned into
  // kCompositeReplace.
  //
  // See http://crbug.com/806139.
  if (RuntimeEnabledFeatures::CSSAdditiveAnimationsEnabled() &&
      options.composite() == "add") {
    return kCompositeAdd;
  }
  return kCompositeReplace;
}

bool EffectModel::StringToCompositeOperation(String composite_string,
                                             CompositeOperation& result,
                                             ExceptionState* exception_state) {
  // TODO(crbug.com/788440): Once all CompositeOperations are supported we can
  // just DCHECK the input and directly convert it instead of handling failure.
  if (composite_string == "add") {
    result = kCompositeAdd;
    return true;
  }
  if (composite_string == "replace") {
    result = kCompositeReplace;
    return true;
  }
  if (exception_state) {
    exception_state->ThrowTypeError("Invalid composite value: '" +
                                    composite_string + "'");
  }
  return false;
}

String EffectModel::CompositeOperationToString(CompositeOperation composite) {
  switch (composite) {
    case EffectModel::kCompositeAdd:
      return "add";
    case EffectModel::kCompositeReplace:
      return "replace";
    default:
      NOTREACHED();
      return "";
  }
}
}  // namespace blink
