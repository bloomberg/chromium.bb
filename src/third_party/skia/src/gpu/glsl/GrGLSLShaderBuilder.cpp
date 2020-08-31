/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/glsl/GrGLSLShaderBuilder.h"

#include "src/gpu/GrShaderCaps.h"
#include "src/gpu/GrShaderVar.h"
#include "src/gpu/GrSwizzle.h"
#include "src/gpu/glsl/GrGLSLBlend.h"
#include "src/gpu/glsl/GrGLSLColorSpaceXformHelper.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"

GrGLSLShaderBuilder::GrGLSLShaderBuilder(GrGLSLProgramBuilder* program)
    : fProgramBuilder(program)
    , fInputs(GrGLSLProgramBuilder::kVarsPerBlock)
    , fOutputs(GrGLSLProgramBuilder::kVarsPerBlock)
    , fFeaturesAddedMask(0)
    , fCodeIndex(kCode)
    , fFinalized(false) {
    // We push back some dummy pointers which will later become our header
    for (int i = 0; i <= kCode; i++) {
        fShaderStrings.push_back();
    }

    this->main() = "void main() {";
}

void GrGLSLShaderBuilder::declAppend(const GrShaderVar& var) {
    SkString tempDecl;
    var.appendDecl(fProgramBuilder->shaderCaps(), &tempDecl);
    this->codeAppendf("%s;", tempDecl.c_str());
}

void GrGLSLShaderBuilder::declareGlobal(const GrShaderVar& v) {
    v.appendDecl(this->getProgramBuilder()->shaderCaps(), &this->definitions());
    this->definitions().append(";");
}

void GrGLSLShaderBuilder::emitFunction(GrSLType returnType,
                                       const char* name,
                                       int argCnt,
                                       const GrShaderVar* args,
                                       const char* body,
                                       SkString* outName) {
    this->functions().append(GrGLSLTypeString(returnType));
    fProgramBuilder->nameVariable(outName, '\0', name);
    this->functions().appendf(" %s", outName->c_str());
    this->functions().append("(");
    for (int i = 0; i < argCnt; ++i) {
        args[i].appendDecl(fProgramBuilder->shaderCaps(), &this->functions());
        if (i < argCnt - 1) {
            this->functions().append(", ");
        }
    }
    this->functions().append(") {\n");
    this->functions().append(body);
    this->functions().append("}\n\n");
}

static inline void append_texture_swizzle(SkString* out, GrSwizzle swizzle) {
    if (swizzle != GrSwizzle::RGBA()) {
        out->appendf(".%s", swizzle.asString().c_str());
    }
}

void GrGLSLShaderBuilder::appendTextureLookup(SkString* out,
                                              SamplerHandle samplerHandle,
                                              const char* coordName) const {
    const char* sampler = fProgramBuilder->samplerVariable(samplerHandle);
    out->appendf("sample(%s, %s)", sampler, coordName);
    append_texture_swizzle(out, fProgramBuilder->samplerSwizzle(samplerHandle));
}

void GrGLSLShaderBuilder::appendTextureLookup(SamplerHandle samplerHandle,
                                              const char* coordName,
                                              GrGLSLColorSpaceXformHelper* colorXformHelper) {
    SkString lookup;
    this->appendTextureLookup(&lookup, samplerHandle, coordName);
    this->appendColorGamutXform(lookup.c_str(), colorXformHelper);
}

void GrGLSLShaderBuilder::appendTextureLookupAndBlend(
        const char* dst,
        SkBlendMode mode,
        SamplerHandle samplerHandle,
        const char* coordName,
        GrGLSLColorSpaceXformHelper* colorXformHelper) {
    if (!dst) {
        dst = "half4(1)";
    }
    SkString lookup;
    // This works around an issue in SwiftShader where the texture lookup is messed up
    // if we use blend_modulate instead of simply operator * on dst and the sampled result.
    // At this time it's unknown if the same problem exists for other modes.
    if (mode == SkBlendMode::kModulate) {
        this->codeAppend("(");
        this->appendTextureLookup(&lookup, samplerHandle, coordName);
        this->appendColorGamutXform(lookup.c_str(), colorXformHelper);
        this->codeAppendf(" * %s)", dst);
    } else {
        this->codeAppendf("%s(", GrGLSLBlend::BlendFuncName(mode));
        this->appendTextureLookup(&lookup, samplerHandle, coordName);
        this->appendColorGamutXform(lookup.c_str(), colorXformHelper);
        this->codeAppendf(", %s)", dst);
    }
}

void GrGLSLShaderBuilder::appendColorGamutXform(SkString* out,
                                                const char* srcColor,
                                                GrGLSLColorSpaceXformHelper* colorXformHelper) {
    if (!colorXformHelper || colorXformHelper->isNoop()) {
        *out = srcColor;
        return;
    }

    GrGLSLUniformHandler* uniformHandler = fProgramBuilder->uniformHandler();

    // We define up to three helper functions, to keep things clearer. One for the source transfer
    // function, one for the (inverse) destination transfer function, and one for the gamut xform.
    // Any combination of these may be present, although some configurations are much more likely.

    auto emitTFFunc = [=](const char* name, GrGLSLProgramDataManager::UniformHandle uniform,
                          TFKind kind) {
        const GrShaderVar gTFArgs[] = { GrShaderVar("x", kHalf_GrSLType) };
        const char* coeffs = uniformHandler->getUniformCStr(uniform);
        SkString body;
        // Temporaries to make evaluation line readable. We always use the sRGBish names, so the
        // PQ and HLG math is confusing.
        body.appendf("half G = %s[0];", coeffs);
        body.appendf("half A = %s[1];", coeffs);
        body.appendf("half B = %s[2];", coeffs);
        body.appendf("half C = %s[3];", coeffs);
        body.appendf("half D = %s[4];", coeffs);
        body.appendf("half E = %s[5];", coeffs);
        body.appendf("half F = %s[6];", coeffs);
        body.append("half s = sign(x);");
        body.append("x = abs(x);");
        switch (kind) {
            case TFKind::sRGBish_TF:
                body.append("x = (x < D) ? (C * x) + F : pow(A * x + B, G) + E;");
                break;
            case TFKind::PQish_TF:
                body.append("x = pow(max(A + B * pow(x, C), 0) / (D + E * pow(x, C)), F);");
                break;
            case TFKind::HLGish_TF:
                body.append("x = (x*A <= 1) ? pow(x*A, B) : exp((x-E)*C) + D;");
                break;
            case TFKind::HLGinvish_TF:
                body.append("x = (x <= 1) ? A * pow(x, B) : C * log(x - D) + E;");
                break;
            default:
                SkASSERT(false);
                break;
        }
        body.append("return s * x;");
        SkString funcName;
        this->emitFunction(kHalf_GrSLType, name, SK_ARRAY_COUNT(gTFArgs), gTFArgs, body.c_str(),
                           &funcName);
        return funcName;
    };

    SkString srcTFFuncName;
    if (colorXformHelper->applySrcTF()) {
        srcTFFuncName = emitTFFunc("src_tf", colorXformHelper->srcTFUniform(),
                                   colorXformHelper->srcTFKind());
    }

    SkString dstTFFuncName;
    if (colorXformHelper->applyDstTF()) {
        dstTFFuncName = emitTFFunc("dst_tf", colorXformHelper->dstTFUniform(),
                                   colorXformHelper->dstTFKind());
    }

    SkString gamutXformFuncName;
    if (colorXformHelper->applyGamutXform()) {
        const GrShaderVar gGamutXformArgs[] = { GrShaderVar("color", kHalf4_GrSLType) };
        const char* xform = uniformHandler->getUniformCStr(colorXformHelper->gamutXformUniform());
        SkString body;
        body.appendf("color.rgb = (%s * color.rgb);", xform);
        body.append("return color;");
        this->emitFunction(kHalf4_GrSLType, "gamut_xform", SK_ARRAY_COUNT(gGamutXformArgs),
                           gGamutXformArgs, body.c_str(), &gamutXformFuncName);
    }

    // Now define a wrapper function that applies all the intermediate steps
    {
        // Some GPUs require full float to get results that are as accurate as expected/required.
        // Most GPUs work just fine with half float. Strangely, the GPUs that have this bug
        // (Mali G series) only require us to promote the type of a few temporaries here --
        // the helper functions above can always be written to use half.
        bool useFloat = fProgramBuilder->shaderCaps()->colorSpaceMathNeedsFloat();

        const GrShaderVar gColorXformArgs[] = {
                GrShaderVar("color", useFloat ? kFloat4_GrSLType : kHalf4_GrSLType)};
        SkString body;
        if (colorXformHelper->applyUnpremul()) {
            body.appendf("%s nonZeroAlpha = max(color.a, 0.0001);", useFloat ? "float" : "half");
            body.appendf("color = %s(color.rgb / nonZeroAlpha, nonZeroAlpha);",
                         useFloat ? "float4" : "half4");
        }
        if (colorXformHelper->applySrcTF()) {
            body.appendf("color.r = %s(half(color.r));", srcTFFuncName.c_str());
            body.appendf("color.g = %s(half(color.g));", srcTFFuncName.c_str());
            body.appendf("color.b = %s(half(color.b));", srcTFFuncName.c_str());
        }
        if (colorXformHelper->applyGamutXform()) {
            body.appendf("color = %s(half4(color));", gamutXformFuncName.c_str());
        }
        if (colorXformHelper->applyDstTF()) {
            body.appendf("color.r = %s(half(color.r));", dstTFFuncName.c_str());
            body.appendf("color.g = %s(half(color.g));", dstTFFuncName.c_str());
            body.appendf("color.b = %s(half(color.b));", dstTFFuncName.c_str());
        }
        if (colorXformHelper->applyPremul()) {
            body.append("color.rgb *= color.a;");
        }
        body.append("return half4(color);");
        SkString colorXformFuncName;
        this->emitFunction(kHalf4_GrSLType, "color_xform", SK_ARRAY_COUNT(gColorXformArgs),
                           gColorXformArgs, body.c_str(), &colorXformFuncName);
        out->appendf("%s(%s)", colorXformFuncName.c_str(), srcColor);
    }
}

void GrGLSLShaderBuilder::appendColorGamutXform(const char* srcColor,
                                                GrGLSLColorSpaceXformHelper* colorXformHelper) {
    SkString xform;
    this->appendColorGamutXform(&xform, srcColor, colorXformHelper);
    this->codeAppend(xform.c_str());
}

bool GrGLSLShaderBuilder::addFeature(uint32_t featureBit, const char* extensionName) {
    if (featureBit & fFeaturesAddedMask) {
        return false;
    }
    this->extensions().appendf("#extension %s: require\n", extensionName);
    fFeaturesAddedMask |= featureBit;
    return true;
}

void GrGLSLShaderBuilder::appendDecls(const VarArray& vars, SkString* out) const {
    for (const auto& v : vars.items()) {
        v.appendDecl(fProgramBuilder->shaderCaps(), out);
        out->append(";\n");
    }
}

void GrGLSLShaderBuilder::addLayoutQualifier(const char* param, InterfaceQualifier interface) {
    SkASSERT(fProgramBuilder->shaderCaps()->generation() >= k330_GrGLSLGeneration ||
             fProgramBuilder->shaderCaps()->mustEnableAdvBlendEqs());
    fLayoutParams[interface].push_back() = param;
}

void GrGLSLShaderBuilder::compileAndAppendLayoutQualifiers() {
    static const char* interfaceQualifierNames[] = {
        "in",
        "out"
    };

    for (int interface = 0; interface <= kLastInterfaceQualifier; ++interface) {
        const SkTArray<SkString>& params = fLayoutParams[interface];
        if (params.empty()) {
            continue;
        }
        this->layoutQualifiers().appendf("layout(%s", params[0].c_str());
        for (int i = 1; i < params.count(); ++i) {
            this->layoutQualifiers().appendf(", %s", params[i].c_str());
        }
        this->layoutQualifiers().appendf(") %s;\n", interfaceQualifierNames[interface]);
    }

    static_assert(0 == GrGLSLShaderBuilder::kIn_InterfaceQualifier);
    static_assert(1 == GrGLSLShaderBuilder::kOut_InterfaceQualifier);
    static_assert(SK_ARRAY_COUNT(interfaceQualifierNames) == kLastInterfaceQualifier + 1);
}

void GrGLSLShaderBuilder::finalize(uint32_t visibility) {
    SkASSERT(!fFinalized);
    this->compileAndAppendLayoutQualifiers();
    SkASSERT(visibility);
    fProgramBuilder->appendUniformDecls((GrShaderFlags) visibility, &this->uniforms());
    this->appendDecls(fInputs, &this->inputs());
    this->appendDecls(fOutputs, &this->outputs());
    this->onFinalize();
    // append the 'footer' to code
    this->code().append("}");

    for (int i = 0; i <= fCodeIndex; i++) {
        fCompilerString.append(fShaderStrings[i].c_str(), fShaderStrings[i].size());
    }

    fFinalized = true;
}
