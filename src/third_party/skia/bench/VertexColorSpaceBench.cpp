/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Benchmark.h"

#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrGeometryProcessor.h"
#include "GrMemoryPool.h"
#include "GrRenderTargetContext.h"
#include "GrRenderTargetContextPriv.h"
#include "SkColorSpacePriv.h"
#include "SkGr.h"
#include "SkHalf.h"
#include "SkString.h"
#include "glsl/GrGLSLColorSpaceXformHelper.h"
#include "glsl/GrGLSLFragmentShaderBuilder.h"
#include "glsl/GrGLSLGeometryProcessor.h"
#include "glsl/GrGLSLVarying.h"
#include "glsl/GrGLSLVertexGeoBuilder.h"
#include "ops/GrMeshDrawOp.h"

namespace {

enum Mode {
    kBaseline_Mode,  // Do the wrong thing, but quickly.
    kFloat_Mode,     // Transform colors on CPU, use float4 attributes.
    kHalf_Mode,      // Transform colors on CPU, use half4 attributes.
    kShader_Mode,    // Use ubyte4 attributes, transform colors on GPU (vertex shader).
    kShort_Mode,     // Transform on CPU, use short4 (4.12) attributes with a bit of shader math.
};

class GP : public GrGeometryProcessor {
public:
    GP(Mode mode, sk_sp<GrColorSpaceXform> colorSpaceXform)
            : INHERITED(kVertexColorSpaceBenchGP_ClassID)
            , fMode(mode)
            , fColorSpaceXform(std::move(colorSpaceXform)) {
        fInPosition = {"inPosition", kFloat2_GrVertexAttribType, kFloat2_GrSLType};
        switch (fMode) {
            case kBaseline_Mode:
            case kShader_Mode:
                fInColor = {"inColor", kUByte4_norm_GrVertexAttribType, kHalf4_GrSLType};
                break;
            case kFloat_Mode:
                fInColor = {"inColor", kFloat4_GrVertexAttribType, kHalf4_GrSLType};
                break;
            case kHalf_Mode:
                fInColor = {"inColor", kHalf4_GrVertexAttribType, kHalf4_GrSLType};
                break;
            case kShort_Mode:
                fInColor = {"inColor", kShort4_GrVertexAttribType, kFloat4_GrSLType};
                break;
        }
        this->setVertexAttributeCnt(2);
    }
    const char* name() const override { return "VertexColorXformGP"; }

    GrGLSLPrimitiveProcessor* createGLSLInstance(const GrShaderCaps&) const override {
        class GLSLGP : public GrGLSLGeometryProcessor {
        public:
            void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) override {
                const GP& gp = args.fGP.cast<GP>();
                GrGLSLVertexBuilder* vertBuilder = args.fVertBuilder;
                GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
                GrGLSLVaryingHandler* varyingHandler = args.fVaryingHandler;
                GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;

                varyingHandler->emitAttributes(gp);

                // Setup color
                GrGLSLVarying varying(kHalf4_GrSLType);
                varyingHandler->addVarying("color", &varying);
                vertBuilder->codeAppendf("half4 color = %s;", gp.fInColor.name());

                if (kShader_Mode == gp.fMode) {
                    fColorSpaceHelper.emitCode(uniformHandler, gp.fColorSpaceXform.get(),
                                               kVertex_GrShaderFlag);
                    SkString xformedColor;
                    vertBuilder->appendColorGamutXform(&xformedColor, "color", &fColorSpaceHelper);
                    vertBuilder->codeAppendf("color = %s;", xformedColor.c_str());
                    vertBuilder->codeAppend("color = half4(color.rgb * color.a, color.a);");
                } else if (kShort_Mode == gp.fMode) {
                    vertBuilder->codeAppend("color = color * (1 / 4096.0);");
                }

                vertBuilder->codeAppendf("%s = color;", varying.vsOut());
                fragBuilder->codeAppendf("%s = %s;", args.fOutputColor, varying.fsIn());

                // Position
                this->writeOutputPosition(args.fVertBuilder, gpArgs, gp.fInPosition.name());

                // Coverage
                fragBuilder->codeAppendf("%s = half4(1);", args.fOutputCoverage);
            }
            void setData(const GrGLSLProgramDataManager& pdman,
                         const GrPrimitiveProcessor& primProc,
                         FPCoordTransformIter&&) override {
                const GP& gp = primProc.cast<GP>();
                fColorSpaceHelper.setData(pdman, gp.fColorSpaceXform.get());
            }

            GrGLSLColorSpaceXformHelper fColorSpaceHelper;
        };
        return new GLSLGP();
    }

    void getGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder* b) const override {
        b->add32(fMode);
        b->add32(GrColorSpaceXform::XformKey(fColorSpaceXform.get()));
    }

private:
    const GrPrimitiveProcessor::Attribute& onVertexAttribute(int i) const override {
        return IthAttribute(i, fInPosition, fInColor);
    }

    Mode fMode;
    sk_sp<GrColorSpaceXform> fColorSpaceXform;

    Attribute fInPosition;
    Attribute fInColor;

    typedef GrGeometryProcessor INHERITED;
};

class Op : public GrMeshDrawOp {
public:
    DEFINE_OP_CLASS_ID

    const char* name() const override { return "VertColorXformOp"; }

    Op(GrColor color)
            : INHERITED(ClassID())
            , fMode(kBaseline_Mode)
            , fColor(color) {
        this->setBounds(SkRect::MakeWH(100.f, 100.f), HasAABloat::kNo, IsZeroArea::kNo);
    }

    Op(GrColor4f color4f, Mode mode)
            : INHERITED(ClassID())
            , fMode(mode)
            , fColor4f(color4f) {
        SkASSERT(kFloat_Mode == fMode || kHalf_Mode == mode || kShort_Mode == mode);
        this->setBounds(SkRect::MakeWH(100.f, 100.f), HasAABloat::kNo, IsZeroArea::kNo);
    }

    Op(GrColor color, sk_sp<GrColorSpaceXform> colorSpaceXform)
            : INHERITED(ClassID())
            , fMode(kShader_Mode)
            , fColor(color)
            , fColorSpaceXform(std::move(colorSpaceXform)) {
        this->setBounds(SkRect::MakeWH(100.f, 100.f), HasAABloat::kNo, IsZeroArea::kNo);
    }

    FixedFunctionFlags fixedFunctionFlags() const override {
        return FixedFunctionFlags::kNone;
    }

    RequiresDstTexture finalize(const GrCaps&, const GrAppliedClip*) override {
        return RequiresDstTexture::kNo;
    }

private:
    friend class ::GrOpMemoryPool;

    void onPrepareDraws(Target* target) override {
        sk_sp<GrGeometryProcessor> gp(new GP(fMode, fColorSpaceXform));

        size_t vertexStride = sizeof(SkPoint);
        switch (fMode) {
            case kFloat_Mode:
                vertexStride += sizeof(GrColor4f);
                break;
            case kHalf_Mode:
            case kShort_Mode:
                vertexStride += sizeof(uint64_t);
                break;
            default:
                vertexStride += sizeof(uint32_t);
        }
        SkASSERT(vertexStride == gp->debugOnly_vertexStride());

        const int kVertexCount = 1024;
        const GrBuffer* vertexBuffer = nullptr;
        int firstVertex = 0;
        void* verts = target->makeVertexSpace(vertexStride, kVertexCount, &vertexBuffer,
                                              &firstVertex);
        if (!verts) {
            return;
        }

        const float dx = 100.0f / kVertexCount;
        if (kFloat_Mode == fMode) {
            struct V {
                SkPoint fPos;
                GrColor4f fColor;
            };
            SkASSERT(sizeof(V) == vertexStride);
            V* v = (V*)verts;
            for (int i = 0; i < kVertexCount; i += 2) {
                v[i + 0].fPos.set(dx * i, 0.0f);
                v[i + 0].fColor = fColor4f;
                v[i + 1].fPos.set(dx * i, 100.0f);
                v[i + 1].fColor = fColor4f;
            }
        } else if (kHalf_Mode == fMode) {
            struct V {
                SkPoint fPos;
                uint64_t fColor;
            };
            SkASSERT(sizeof(V) == vertexStride);
            uint64_t color;
            Sk4h halfColor = SkFloatToHalf_finite_ftz(Sk4f::Load(&fColor4f));
            color = (uint64_t)halfColor[0] << 48 |
                    (uint64_t)halfColor[1] << 32 |
                    (uint64_t)halfColor[2] << 16 |
                    (uint64_t)halfColor[3] << 0;
            V* v = (V*)verts;
            for (int i = 0; i < kVertexCount; i += 2) {
                v[i + 0].fPos.set(dx * i, 0.0f);
                v[i + 0].fColor = color;
                v[i + 1].fPos.set(dx * i, 100.0f);
                v[i + 1].fColor = color;
            }
        } else if (kShort_Mode == fMode) {
            struct ShortColor { int16_t fRGBA[4]; };
            struct V {
                SkPoint fPos;
                ShortColor fColor;
            };
            SkASSERT(sizeof(V) == vertexStride);
            Sk4i c = Sk4f_round(Sk4f::Load(&fColor4f) * 4096.0f);
            c = Sk4i::Max(-32768, Sk4i::Min(c, 32767));
            ShortColor color;
            for (int i = 0; i < 4; ++i) {
                color.fRGBA[i] = c[i];
            }
            V* v = (V*)verts;
            for (int i = 0; i < kVertexCount; i += 2) {
                v[i + 0].fPos.set(dx * i, 0.0f);
                v[i + 0].fColor = color;
                v[i + 1].fPos.set(dx * i, 100.0f);
                v[i + 1].fColor = color;
            }
        } else {
            struct V {
                SkPoint fPos;
                GrColor fColor;
            };
            SkASSERT(sizeof(V) == vertexStride);
            V* v = (V*)verts;
            for (int i = 0; i < kVertexCount; i += 2) {
                v[i + 0].fPos.set(dx * i, 0.0f);
                v[i + 0].fColor = fColor;
                v[i + 1].fPos.set(dx * i, 100.0f);
                v[i + 1].fColor = fColor;
            }
        }

        GrMesh* mesh = target->allocMesh(GrPrimitiveType::kTriangleStrip);
        mesh->setNonIndexedNonInstanced(kVertexCount);
        mesh->setVertexData(vertexBuffer, firstVertex);
        auto pipe = target->makePipeline(0, GrProcessorSet::MakeEmptySet(),
                                         target->detachAppliedClip());
        target->draw(gp, pipe.fPipeline, pipe.fFixedDynamicState, mesh);
    }

    Mode fMode;
    GrColor fColor;
    GrColor4f fColor4f;
    sk_sp<GrColorSpaceXform> fColorSpaceXform;

    typedef GrMeshDrawOp INHERITED;
};
}

class VertexColorSpaceBench : public Benchmark {
public:
    VertexColorSpaceBench(Mode mode, const char* name) : fMode(mode) {
        fName = "vertexcolorspace";
        fName.appendf("_%s", name);
    }

    bool isSuitableFor(Backend backend) override { return kGPU_Backend == backend; }
    const char* onGetName() override { return fName.c_str(); }

    void onDraw(int loops, SkCanvas* canvas) override {
        GrContext* context = canvas->getGrContext();
        SkASSERT(context);

        if (kHalf_Mode == fMode &&
            !context->contextPriv().caps()->halfFloatVertexAttributeSupport()) {
            return;
        }

        GrOpMemoryPool* pool = context->contextPriv().opMemoryPool();

        auto p3 = SkColorSpace::MakeRGB(SkColorSpace::kSRGB_RenderTargetGamma,
                                        SkColorSpace::kDCIP3_D65_Gamut);
        auto xform = GrColorSpaceXform::Make(sk_srgb_singleton(), kUnpremul_SkAlphaType,
                                             p3.get(),            kUnpremul_SkAlphaType);

        SkRandom r;
        const int kDrawsPerLoop = 32;

        for (int i = 0; i < loops; ++i) {
            sk_sp<GrRenderTargetContext> rtc(
                    context->contextPriv().makeDeferredRenderTargetContext(SkBackingFit::kApprox,
                    100, 100, kRGBA_8888_GrPixelConfig, p3));
            SkASSERT(rtc);

            for (int j = 0; j < kDrawsPerLoop; ++j) {
                SkColor c = r.nextU();
                std::unique_ptr<GrDrawOp> op = nullptr;

                switch (fMode) {
                    case kBaseline_Mode:
                        op = pool->allocate<Op>(SkColorToPremulGrColor(c));
                        break;
                    case kShader_Mode:
                        op = pool->allocate<Op>(SkColorToUnpremulGrColor(c), xform);
                        break;
                    case kHalf_Mode:
                    case kShort_Mode:
                    case kFloat_Mode: {
                        GrColor4f c4f = GrColor4f::FromGrColor(SkColorToUnpremulGrColor(c));
                        c4f = xform->apply(c4f);
                        op = pool->allocate<Op>(c4f, fMode);
                    }
                }
                rtc->priv().testingOnly_addDrawOp(std::move(op));
            }

            context->flush();
        }
    }

private:
    SkString fName;
    Mode fMode;

    typedef Benchmark INHERITED;
};

DEF_BENCH(return new VertexColorSpaceBench(kBaseline_Mode, "baseline"));
DEF_BENCH(return new VertexColorSpaceBench(kFloat_Mode,    "float"));
DEF_BENCH(return new VertexColorSpaceBench(kHalf_Mode,     "half"));
DEF_BENCH(return new VertexColorSpaceBench(kShort_Mode,    "short"));
DEF_BENCH(return new VertexColorSpaceBench(kShader_Mode,   "shader"));
