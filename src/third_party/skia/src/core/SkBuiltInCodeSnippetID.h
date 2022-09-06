/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkBuiltInCodeSnippetID_DEFINED
#define SkBuiltInCodeSnippetID_DEFINED

#include "include/core/SkTypes.h"

// TODO: this needs to be expanded into a more flexible dictionary (esp. for user-supplied SkSL)
enum class SkBuiltInCodeSnippetID : uint8_t {
    // This isn't just a signal for a failure during paintparams key creation. It also actually
    // implements the default behavior for an erroneous draw. Currently it just draws solid
    // magenta.
    kError,

    // SkShader code snippets
    kSolidColorShader,
    kLinearGradientShader4,
    kLinearGradientShader8,
    kRadialGradientShader4,
    kRadialGradientShader8,
    kSweepGradientShader4,
    kSweepGradientShader8,
    kConicalGradientShader4,
    kConicalGradientShader8,

    kLocalMatrixShader,
    kImageShader,
    kBlendShader,     // aka ComposeShader

    // BlendMode code snippets
    kFixedFunctionBlender,
    kShaderBasedBlender,

    kLast = kShaderBasedBlender
};
static constexpr int kBuiltInCodeSnippetIDCount = static_cast<int>(SkBuiltInCodeSnippetID::kLast)+1;

#endif // SkBuiltInCodeSnippetID_DEFINED
