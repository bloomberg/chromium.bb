/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/GrShaderCaps.h"
#include "src/gpu/effects/GrBezierEffect.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLGeometryProcessor.h"
#include "src/gpu/glsl/GrGLSLProgramDataManager.h"
#include "src/gpu/glsl/GrGLSLUniformHandler.h"
#include "src/gpu/glsl/GrGLSLVarying.h"
#include "src/gpu/glsl/GrGLSLVertexGeoBuilder.h"

class GrGLConicEffect : public GrGLSLGeometryProcessor {
public:
    GrGLConicEffect(const GrGeometryProcessor&);

    void onEmitCode(EmitArgs&, GrGPArgs*) override;

    static inline void GenKey(const GrGeometryProcessor&,
                              const GrShaderCaps&,
                              GrProcessorKeyBuilder*);

    void setData(const GrGLSLProgramDataManager& pdman,
                 const GrShaderCaps& shaderCaps,
                 const GrGeometryProcessor& geomProc) override {
        const GrConicEffect& ce = geomProc.cast<GrConicEffect>();

        SetTransform(pdman, shaderCaps,  fViewMatrixUniform,  ce.viewMatrix(), &fViewMatrix);
        SetTransform(pdman, shaderCaps, fLocalMatrixUniform, ce.localMatrix(), &fLocalMatrix);

        if (ce.color() != fColor) {
            pdman.set4fv(fColorUniform, 1, ce.color().vec());
            fColor = ce.color();
        }

        if (ce.coverageScale() != 0xff && ce.coverageScale() != fCoverageScale) {
            pdman.set1f(fCoverageScaleUniform, GrNormalizeByteToFloat(ce.coverageScale()));
            fCoverageScale = ce.coverageScale();
        }
    }

private:
    SkMatrix fViewMatrix;
    SkMatrix fLocalMatrix;
    SkPMColor4f fColor;
    uint8_t fCoverageScale;
    UniformHandle fColorUniform;
    UniformHandle fCoverageScaleUniform;
    UniformHandle fViewMatrixUniform;
    UniformHandle fLocalMatrixUniform;

    using INHERITED = GrGLSLGeometryProcessor;
};

GrGLConicEffect::GrGLConicEffect(const GrGeometryProcessor& processor)
        : fViewMatrix(SkMatrix::InvalidMatrix())
        , fLocalMatrix(SkMatrix::InvalidMatrix())
        , fColor(SK_PMColor4fILLEGAL)
        , fCoverageScale(0xff) {}

void GrGLConicEffect::onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) {
    GrGLSLVertexBuilder* vertBuilder = args.fVertBuilder;
    const GrConicEffect& gp = args.fGeomProc.cast<GrConicEffect>();
    GrGLSLVaryingHandler* varyingHandler = args.fVaryingHandler;
    GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;

    // emit attributes
    varyingHandler->emitAttributes(gp);

    GrGLSLVarying v(kFloat4_GrSLType);
    varyingHandler->addVarying("ConicCoeffs", &v);
    vertBuilder->codeAppendf("%s = %s;", v.vsOut(), gp.inConicCoeffs().name());

    GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
    // Setup pass through color
    fragBuilder->codeAppendf("half4 %s;", args.fOutputColor);
    this->setupUniformColor(fragBuilder, uniformHandler, args.fOutputColor, &fColorUniform);

    // Setup position
    WriteOutputPosition(vertBuilder,
                        uniformHandler,
                        *args.fShaderCaps,
                        gpArgs,
                        gp.inPosition().name(),
                        gp.viewMatrix(),
                        &fViewMatrixUniform);
    if (gp.usesLocalCoords()) {
        WriteLocalCoord(vertBuilder,
                        uniformHandler,
                        *args.fShaderCaps,
                        gpArgs,
                        gp.inPosition().asShaderVar(),
                        gp.localMatrix(),
                        &fLocalMatrixUniform);
    }

    // TODO: we should check on the number of bits float and half provide and use the smallest one
    // that suffices. Additionally we should assert that the upstream code only lets us get here if
    // either float or half provides the required number of bits.

    GrShaderVar edgeAlpha("edgeAlpha", kHalf_GrSLType, 0);
    GrShaderVar dklmdx("dklmdx", kFloat3_GrSLType, 0);
    GrShaderVar dklmdy("dklmdy", kFloat3_GrSLType, 0);
    GrShaderVar dfdx("dfdx", kFloat_GrSLType, 0);
    GrShaderVar dfdy("dfdy", kFloat_GrSLType, 0);
    GrShaderVar gF("gF", kFloat2_GrSLType, 0);
    GrShaderVar gFM("gFM", kFloat_GrSLType, 0);
    GrShaderVar func("func", kFloat_GrSLType, 0);

    fragBuilder->declAppend(edgeAlpha);
    fragBuilder->declAppend(dklmdx);
    fragBuilder->declAppend(dklmdy);
    fragBuilder->declAppend(dfdx);
    fragBuilder->declAppend(dfdy);
    fragBuilder->declAppend(gF);
    fragBuilder->declAppend(gFM);
    fragBuilder->declAppend(func);

    fragBuilder->codeAppendf("%s = dFdx(%s.xyz);", dklmdx.c_str(), v.fsIn());
    fragBuilder->codeAppendf("%s = dFdy(%s.xyz);", dklmdy.c_str(), v.fsIn());
    fragBuilder->codeAppendf("%s = 2.0 * %s.x * %s.x - %s.y * %s.z - %s.z * %s.y;",
                             dfdx.c_str(),
                             v.fsIn(), dklmdx.c_str(),
                             v.fsIn(), dklmdx.c_str(),
                             v.fsIn(), dklmdx.c_str());
    fragBuilder->codeAppendf("%s = 2.0 * %s.x * %s.x - %s.y * %s.z - %s.z * %s.y;",
                             dfdy.c_str(),
                             v.fsIn(), dklmdy.c_str(),
                             v.fsIn(), dklmdy.c_str(),
                             v.fsIn(), dklmdy.c_str());
    fragBuilder->codeAppendf("%s = float2(%s, %s);", gF.c_str(), dfdx.c_str(),
                             dfdy.c_str());
    fragBuilder->codeAppendf("%s = sqrt(dot(%s, %s));",
                             gFM.c_str(), gF.c_str(), gF.c_str());
    fragBuilder->codeAppendf("%s = %s.x*%s.x - %s.y*%s.z;",
                             func.c_str(), v.fsIn(), v.fsIn(), v.fsIn(), v.fsIn());
    fragBuilder->codeAppendf("%s = abs(%s);", func.c_str(), func.c_str());
    fragBuilder->codeAppendf("%s = half(%s / %s);",
                             edgeAlpha.c_str(), func.c_str(), gFM.c_str());
    fragBuilder->codeAppendf("%s = max(1.0 - %s, 0.0);",
                             edgeAlpha.c_str(), edgeAlpha.c_str());
    // Add line below for smooth cubic ramp
    // fragBuilder->codeAppend("edgeAlpha = edgeAlpha*edgeAlpha*(3.0-2.0*edgeAlpha);");

    // TODO should we really be doing this?
    if (gp.coverageScale() != 0xff) {
        const char* coverageScale;
        fCoverageScaleUniform = uniformHandler->addUniform(nullptr,
                                                           kFragment_GrShaderFlag,
                                                           kFloat_GrSLType,
                                                           "Coverage",
                                                           &coverageScale);
        fragBuilder->codeAppendf("half4 %s = half4(half(%s) * %s);",
                                 args.fOutputCoverage, coverageScale, edgeAlpha.c_str());
    } else {
        fragBuilder->codeAppendf("half4 %s = half4(%s);", args.fOutputCoverage, edgeAlpha.c_str());
    }
}

void GrGLConicEffect::GenKey(const GrGeometryProcessor& gp,
                             const GrShaderCaps& shaderCaps,
                             GrProcessorKeyBuilder* b) {
    const GrConicEffect& ce = gp.cast<GrConicEffect>();
    uint32_t key = ce.isAntiAliased() ? (ce.isFilled() ? 0x0 : 0x1) : 0x2;
    key |= 0xff != ce.coverageScale() ? 0x8 : 0x0;
    key |= ce.usesLocalCoords() ? 0x10 : 0x0;
    key = AddMatrixKeys(shaderCaps,
                        key,
                        ce.viewMatrix(),
                        ce.usesLocalCoords() ? ce.localMatrix() : SkMatrix::I());
    b->add32(key);
}

//////////////////////////////////////////////////////////////////////////////

constexpr GrGeometryProcessor::Attribute GrConicEffect::kAttributes[];

GrConicEffect::~GrConicEffect() {}

void GrConicEffect::getGLSLProcessorKey(const GrShaderCaps& caps,
                                        GrProcessorKeyBuilder* b) const {
    GrGLConicEffect::GenKey(*this, caps, b);
}

GrGLSLGeometryProcessor* GrConicEffect::createGLSLInstance(const GrShaderCaps&) const {
    return new GrGLConicEffect(*this);
}

GrConicEffect::GrConicEffect(const SkPMColor4f& color, const SkMatrix& viewMatrix, uint8_t coverage,
                             const SkMatrix& localMatrix, bool usesLocalCoords)
        : INHERITED(kGrConicEffect_ClassID)
        , fColor(color)
        , fViewMatrix(viewMatrix)
        , fLocalMatrix(viewMatrix)
        , fUsesLocalCoords(usesLocalCoords)
        , fCoverageScale(coverage) {
    this->setVertexAttributes(kAttributes, SK_ARRAY_COUNT(kAttributes));
}

//////////////////////////////////////////////////////////////////////////////

GR_DEFINE_GEOMETRY_PROCESSOR_TEST(GrConicEffect);

#if GR_TEST_UTILS
GrGeometryProcessor* GrConicEffect::TestCreate(GrProcessorTestData* d) {
    GrColor color = GrRandomColor(d->fRandom);
    SkMatrix viewMatrix = GrTest::TestMatrix(d->fRandom);
    SkMatrix localMatrix = GrTest::TestMatrix(d->fRandom);
    bool usesLocalCoords = d->fRandom->nextBool();
    return GrConicEffect::Make(d->allocator(),
                               SkPMColor4f::FromBytes_RGBA(color),
                               viewMatrix,
                               *d->caps(),
                               localMatrix,
                               usesLocalCoords);
}
#endif

//////////////////////////////////////////////////////////////////////////////
// Quad
//////////////////////////////////////////////////////////////////////////////

class GrGLQuadEffect : public GrGLSLGeometryProcessor {
public:
    GrGLQuadEffect(const GrGeometryProcessor&);

    void onEmitCode(EmitArgs&, GrGPArgs*) override;

    static inline void GenKey(const GrGeometryProcessor&,
                              const GrShaderCaps&,
                              GrProcessorKeyBuilder*);

    void setData(const GrGLSLProgramDataManager& pdman,
                 const GrShaderCaps& shaderCaps,
                 const GrGeometryProcessor& geomProc) override {
        const GrQuadEffect& qe = geomProc.cast<GrQuadEffect>();

        SetTransform(pdman, shaderCaps,  fViewMatrixUniform,  qe.viewMatrix(), &fViewMatrix);
        SetTransform(pdman, shaderCaps, fLocalMatrixUniform, qe.localMatrix(), &fLocalMatrix);

        if (qe.color() != fColor) {
            pdman.set4fv(fColorUniform, 1, qe.color().vec());
            fColor = qe.color();
        }

        if (qe.coverageScale() != 0xff && qe.coverageScale() != fCoverageScale) {
            pdman.set1f(fCoverageScaleUniform, GrNormalizeByteToFloat(qe.coverageScale()));
            fCoverageScale = qe.coverageScale();
        }
    }

private:
    SkMatrix fViewMatrix;
    SkMatrix fLocalMatrix;
    SkPMColor4f fColor;
    uint8_t fCoverageScale;

    UniformHandle fColorUniform;
    UniformHandle fCoverageScaleUniform;
    UniformHandle fViewMatrixUniform;
    UniformHandle fLocalMatrixUniform;

    using INHERITED = GrGLSLGeometryProcessor;
};

GrGLQuadEffect::GrGLQuadEffect(const GrGeometryProcessor& processor)
        : fViewMatrix(SkMatrix::InvalidMatrix())
        , fLocalMatrix(SkMatrix::InvalidMatrix())
        , fColor(SK_PMColor4fILLEGAL)
        , fCoverageScale(0xff) {}

void GrGLQuadEffect::onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) {
    GrGLSLVertexBuilder* vertBuilder = args.fVertBuilder;
    const GrQuadEffect& gp = args.fGeomProc.cast<GrQuadEffect>();
    GrGLSLVaryingHandler* varyingHandler = args.fVaryingHandler;
    GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;

    // emit attributes
    varyingHandler->emitAttributes(gp);

    GrGLSLVarying v(kHalf4_GrSLType);
    varyingHandler->addVarying("HairQuadEdge", &v);
    vertBuilder->codeAppendf("%s = %s;", v.vsOut(), gp.inHairQuadEdge().name());

    GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
    // Setup pass through color
    fragBuilder->codeAppendf("half4 %s;", args.fOutputColor);
    this->setupUniformColor(fragBuilder, uniformHandler, args.fOutputColor, &fColorUniform);

    // Setup position
    WriteOutputPosition(vertBuilder,
                        uniformHandler,
                        *args.fShaderCaps,
                        gpArgs,
                        gp.inPosition().name(),
                        gp.viewMatrix(),
                        &fViewMatrixUniform);
    if (gp.usesLocalCoords()) {
        WriteLocalCoord(vertBuilder,
                        uniformHandler,
                        *args.fShaderCaps,
                        gpArgs,
                        gp.inPosition().asShaderVar(),
                        gp.localMatrix(),
                        &fLocalMatrixUniform);
    }

    fragBuilder->codeAppendf("half edgeAlpha;");

    fragBuilder->codeAppendf("half2 duvdx = half2(dFdx(%s.xy));", v.fsIn());
    fragBuilder->codeAppendf("half2 duvdy = half2(dFdy(%s.xy));", v.fsIn());
    fragBuilder->codeAppendf("half2 gF = half2(2.0 * %s.x * duvdx.x - duvdx.y,"
                             "               2.0 * %s.x * duvdy.x - duvdy.y);",
                             v.fsIn(), v.fsIn());
    fragBuilder->codeAppendf("edgeAlpha = half(%s.x * %s.x - %s.y);",
                             v.fsIn(), v.fsIn(), v.fsIn());
    fragBuilder->codeAppend("edgeAlpha = sqrt(edgeAlpha * edgeAlpha / dot(gF, gF));");
    fragBuilder->codeAppend("edgeAlpha = max(1.0 - edgeAlpha, 0.0);");
    // Add line below for smooth cubic ramp
    // fragBuilder->codeAppend("edgeAlpha = edgeAlpha*edgeAlpha*(3.0-2.0*edgeAlpha);");

    if (0xff != gp.coverageScale()) {
        const char* coverageScale;
        fCoverageScaleUniform = uniformHandler->addUniform(nullptr,
                                                           kFragment_GrShaderFlag,
                                                           kHalf_GrSLType,
                                                           "Coverage",
                                                           &coverageScale);
        fragBuilder->codeAppendf("half4 %s = half4(%s * edgeAlpha);", args.fOutputCoverage,
                                 coverageScale);
    } else {
        fragBuilder->codeAppendf("half4 %s = half4(edgeAlpha);", args.fOutputCoverage);
    }
}

void GrGLQuadEffect::GenKey(const GrGeometryProcessor& gp,
                            const GrShaderCaps& shaderCaps,
                            GrProcessorKeyBuilder* b) {
    const GrQuadEffect& ce = gp.cast<GrQuadEffect>();
    uint32_t key = ce.isAntiAliased() ? (ce.isFilled() ? 0x0 : 0x1) : 0x2;
    key |= ce.coverageScale() != 0xff ? 0x8 : 0x0;
    key |= ce.usesLocalCoords()? 0x10 : 0x0;
    key = AddMatrixKeys(shaderCaps,
                        key,
                        ce.viewMatrix(),
                        ce.usesLocalCoords() ? ce.localMatrix() : SkMatrix::I());
    b->add32(key);
}

//////////////////////////////////////////////////////////////////////////////

constexpr GrGeometryProcessor::Attribute GrQuadEffect::kAttributes[];

GrQuadEffect::~GrQuadEffect() {}

void GrQuadEffect::getGLSLProcessorKey(const GrShaderCaps& caps,
                                       GrProcessorKeyBuilder* b) const {
    GrGLQuadEffect::GenKey(*this, caps, b);
}

GrGLSLGeometryProcessor* GrQuadEffect::createGLSLInstance(const GrShaderCaps&) const {
    return new GrGLQuadEffect(*this);
}

GrQuadEffect::GrQuadEffect(const SkPMColor4f& color, const SkMatrix& viewMatrix, uint8_t coverage,
                           const SkMatrix& localMatrix, bool usesLocalCoords)
    : INHERITED(kGrQuadEffect_ClassID)
    , fColor(color)
    , fViewMatrix(viewMatrix)
    , fLocalMatrix(localMatrix)
    , fUsesLocalCoords(usesLocalCoords)
    , fCoverageScale(coverage) {
    this->setVertexAttributes(kAttributes, SK_ARRAY_COUNT(kAttributes));
}

//////////////////////////////////////////////////////////////////////////////

GR_DEFINE_GEOMETRY_PROCESSOR_TEST(GrQuadEffect);

#if GR_TEST_UTILS
GrGeometryProcessor* GrQuadEffect::TestCreate(GrProcessorTestData* d) {
    GrColor color = GrRandomColor(d->fRandom);
    SkMatrix viewMatrix = GrTest::TestMatrix(d->fRandom);
    SkMatrix localMatrix = GrTest::TestMatrix(d->fRandom);
    bool usesLocalCoords = d->fRandom->nextBool();
    return GrQuadEffect::Make(d->allocator(),
                              SkPMColor4f::FromBytes_RGBA(color),
                              viewMatrix,
                              *d->caps(),
                              localMatrix,
                              usesLocalCoords);
}
#endif
