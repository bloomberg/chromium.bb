/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/GrRenderTarget.h"
#include "src/gpu/GrShaderCaps.h"
#include "src/gpu/gl/GrGLGpu.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/gpu/glsl/GrGLSLUniformHandler.h"
#include "src/gpu/glsl/GrGLSLVarying.h"

const char* GrGLSLFragmentShaderBuilder::kDstColorName = "_dstColor";

uint8_t GrGLSLFragmentShaderBuilder::KeyForSurfaceOrigin(GrSurfaceOrigin origin) {
    SkASSERT(kTopLeft_GrSurfaceOrigin == origin || kBottomLeft_GrSurfaceOrigin == origin);
    return origin + 1;

    static_assert(0 == kTopLeft_GrSurfaceOrigin);
    static_assert(1 == kBottomLeft_GrSurfaceOrigin);
}

GrGLSLFragmentShaderBuilder::GrGLSLFragmentShaderBuilder(GrGLSLProgramBuilder* program)
        : GrGLSLShaderBuilder(program) {
    fSubstageIndices.push_back(0);
}

SkString GrGLSLFPFragmentBuilder::writeProcessorFunction(GrGLSLFragmentProcessor* fp,
                                                         GrGLSLFragmentProcessor::EmitArgs& args) {
    this->onBeforeChildProcEmitCode();
    this->nextStage();

    // An FP's function signature is theoretically always main(half4 color, float2 _coords).
    // However, if it is only sampled by a chain of uniform matrix expressions (or legacy coord
    // transforms), the value that would have been passed to _coords is lifted to the vertex shader
    // and stored in a unique varying. In that case it uses that variable and does not have a
    // second actual argument for _coords.
    // FIXME: An alternative would be to have all FP functions have a float2 argument, and the
    // parent FP invokes it with the varying reference when it's been lifted to the vertex shader.
    size_t paramCount = 2;
    GrShaderVar params[] = { GrShaderVar(args.fInputColor, kHalf4_GrSLType),
                             GrShaderVar(args.fSampleCoord, kFloat2_GrSLType) };

    if (!args.fFp.isSampledWithExplicitCoords()) {
        // Sampled with a uniform matrix expression and/or a legacy coord transform. The actual
        // transformation code is emitted in the vertex shader, so this only has to access it.
        // Add a float2 _coords variable that maps to the associated varying and replaces the
        // absent 2nd argument to the fp's function.
        paramCount = 1;

        if (args.fFp.referencesSampleCoords()) {
            const GrShaderVar& varying = args.fTransformedCoords[0];
            switch(varying.getType()) {
                case kFloat2_GrSLType:
                    // Just point the local coords to the varying
                    args.fSampleCoord = varying.getName().c_str();
                    break;
                case kFloat3_GrSLType:
                    // Must perform the perspective divide in the frag shader based on the varying,
                    // and since we won't actually have a function parameter for local coords, add
                    // it as a local variable.
                    this->codeAppendf("float2 %s = %s.xy / %s.z;\n", args.fSampleCoord,
                                      varying.getName().c_str(), varying.getName().c_str());
                    break;
                default:
                    SkDEBUGFAILF("Unexpected varying type for coord: %s %d\n",
                                 varying.getName().c_str(), (int) varying.getType());
                    break;
            }
        }
    } // else the function keeps its two arguments

    fp->emitCode(args);

    SkString funcName = this->getMangledFunctionName(args.fFp.name());
    this->emitFunction(kHalf4_GrSLType, funcName.c_str(), {params, paramCount},
                       this->code().c_str());
    this->deleteStage();
    this->onAfterChildProcEmitCode();
    return funcName;
}

const char* GrGLSLFragmentShaderBuilder::dstColor() {
    SkDEBUGCODE(fHasReadDstColorThisStage_DebugOnly = true;)

    const GrShaderCaps* shaderCaps = fProgramBuilder->shaderCaps();
    if (shaderCaps->fbFetchSupport()) {
        this->addFeature(1 << kFramebufferFetch_GLSLPrivateFeature,
                         shaderCaps->fbFetchExtensionString());

        // Some versions of this extension string require declaring custom color output on ES 3.0+
        const char* fbFetchColorName = "sk_LastFragColor";
        if (shaderCaps->fbFetchNeedsCustomOutput()) {
            this->enableCustomOutput();
            fCustomColorOutput->setTypeModifier(GrShaderVar::TypeModifier::InOut);
            fbFetchColorName = DeclaredColorOutputName();
            // Set the dstColor to an intermediate variable so we don't override it with the output
            this->codeAppendf("half4 %s = %s;", kDstColorName, fbFetchColorName);
        } else {
            return fbFetchColorName;
        }
    }
    return kDstColorName;
}

void GrGLSLFragmentShaderBuilder::enableAdvancedBlendEquationIfNeeded(GrBlendEquation equation) {
    SkASSERT(GrBlendEquationIsAdvanced(equation));

    if (fProgramBuilder->shaderCaps()->mustEnableAdvBlendEqs()) {
        this->addFeature(1 << kBlendEquationAdvanced_GLSLPrivateFeature,
                         "GL_KHR_blend_equation_advanced");
        this->addLayoutQualifier("blend_support_all_equations", kOut_InterfaceQualifier);
    }
}

void GrGLSLFragmentShaderBuilder::enableCustomOutput() {
    if (!fCustomColorOutput) {
        fCustomColorOutput = &fOutputs.emplace_back(DeclaredColorOutputName(), kHalf4_GrSLType,
                                                    GrShaderVar::TypeModifier::Out);
        fProgramBuilder->finalizeFragmentOutputColor(fOutputs.back());
    }
}

void GrGLSLFragmentShaderBuilder::enableSecondaryOutput() {
    SkASSERT(!fHasSecondaryOutput);
    fHasSecondaryOutput = true;
    const GrShaderCaps& caps = *fProgramBuilder->shaderCaps();
    if (const char* extension = caps.secondaryOutputExtensionString()) {
        this->addFeature(1 << kBlendFuncExtended_GLSLPrivateFeature, extension);
    }

    // If the primary output is declared, we must declare also the secondary output
    // and vice versa, since it is not allowed to use a built-in gl_FragColor and a custom
    // output. The condition also co-incides with the condition in which GLES SL 2.0
    // requires the built-in gl_SecondaryFragColorEXT, where as 3.0 requires a custom output.
    if (caps.mustDeclareFragmentShaderOutput()) {
        fOutputs.emplace_back(DeclaredSecondaryColorOutputName(), kHalf4_GrSLType,
                              GrShaderVar::TypeModifier::Out);
        fProgramBuilder->finalizeFragmentSecondaryColor(fOutputs.back());
    }
}

const char* GrGLSLFragmentShaderBuilder::getPrimaryColorOutputName() const {
    return this->hasCustomColorOutput() ? DeclaredColorOutputName() : "sk_FragColor";
}

bool GrGLSLFragmentShaderBuilder::primaryColorOutputIsInOut() const {
    return fCustomColorOutput &&
           fCustomColorOutput->getTypeModifier() == GrShaderVar::TypeModifier::InOut;
}

const char* GrGLSLFragmentShaderBuilder::getSecondaryColorOutputName() const {
    if (this->hasSecondaryOutput()) {
        return (fProgramBuilder->shaderCaps()->mustDeclareFragmentShaderOutput())
                ? DeclaredSecondaryColorOutputName()
                : "gl_SecondaryFragColorEXT";
    }
    return nullptr;
}

GrSurfaceOrigin GrGLSLFragmentShaderBuilder::getSurfaceOrigin() const {
    return fProgramBuilder->origin();
}

void GrGLSLFragmentShaderBuilder::onFinalize() {
    SkASSERT(fProgramBuilder->processorFeatures() == fUsedProcessorFeaturesAllStages_DebugOnly);

    fProgramBuilder->varyingHandler()->getFragDecls(&this->inputs(), &this->outputs());
}

void GrGLSLFragmentShaderBuilder::onBeforeChildProcEmitCode() {
    SkASSERT(fSubstageIndices.count() >= 1);
    fSubstageIndices.push_back(0);
    // second-to-last value in the fSubstageIndices stack is the index of the child proc
    // at that level which is currently emitting code.
    fMangleString.appendf("_c%d", fSubstageIndices[fSubstageIndices.count() - 2]);
}

void GrGLSLFragmentShaderBuilder::onAfterChildProcEmitCode() {
    SkASSERT(fSubstageIndices.count() >= 2);
    fSubstageIndices.pop_back();
    fSubstageIndices.back()++;
    int removeAt = fMangleString.findLastOf('_');
    fMangleString.remove(removeAt, fMangleString.size() - removeAt);
}
