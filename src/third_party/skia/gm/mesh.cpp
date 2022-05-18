/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm/gm.h"
#include "include/core/SkBlender.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkData.h"
#include "include/core/SkMesh.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkGradientShader.h"
#include "include/gpu/GrDirectContext.h"
#include "src/core/SkCanvasPriv.h"
#include "src/core/SkMeshPriv.h"

#include <memory>

namespace skiagm {
class MeshGM : public skiagm::GM {
public:
    MeshGM() {}

protected:
    using Attribute = SkMeshSpecification::Attribute;
    using Varying   = SkMeshSpecification::Varying;

    SkISize onISize() override { return {435, 1180}; }

    void onOnceBeforeDraw() override {
        {
            static const Attribute kAttributes[]{
                    {Attribute::Type::kFloat4,       8, SkString{"xuyv"}},
                    {Attribute::Type::kUByte4_unorm, 4, SkString{"brag"}},
            };
            static const Varying kVaryings[]{
                    {Varying::Type::kHalf4,  SkString{"color"}},
                    {Varying::Type::kFloat2, SkString{"uv"}   },
            };
            static constexpr char kVS[] = R"(
                    half4 unswizzle_color(half4 color) { return color.garb; }

                    float2 main(in Attributes attributes, out Varyings varyings) {
                        varyings.color = unswizzle_color(attributes.brag);
                        varyings.uv    = attributes.xuyv.yw;
                        return attributes.xuyv.xz;
                    }
            )";
            static constexpr char kFS[] = R"(
                    float2 main(in Varyings varyings, out float4 color) {
                        color = varyings.color;
                        return varyings.uv;
                    }
            )";
            auto[spec, error] =
                    SkMeshSpecification::Make(SkMakeSpan(kAttributes, SK_ARRAY_COUNT(kAttributes)),
                                              sizeof(ColorVertex),
                                              SkMakeSpan(kVaryings, SK_ARRAY_COUNT(kVaryings)),
                                              SkString(kVS),
                                              SkString(kFS));
            if (!spec) {
                SkDebugf("%s\n", error.c_str());
            }
            fSpecWithColor = std::move(spec);
        }
        {
            static const Attribute kAttributes[]{
                    {Attribute::Type::kFloat4, 0, SkString{"xuyv"}},
            };
            static const Varying kVaryings[]{
                    {Varying::Type::kFloat2, SkString{"vux2"}},
            };
            static constexpr char kVS[] = R"(
                    float2 main(in Attributes a, out Varyings v) {
                        v.vux2 = 2*a.xuyv.wy;
                        return a.xuyv.xz;
                    }
            )";
            static constexpr char kFS[] = R"(
                    float2 helper(in float2 vux2) { return vux2.yx/2; }
                    float2 main(in Varyings varyings) {
                        return helper(varyings.vux2);
                    }
            )";
            auto[spec, error] =
                    SkMeshSpecification::Make(SkMakeSpan(kAttributes, SK_ARRAY_COUNT(kAttributes)),
                                              sizeof(NoColorVertex),
                                              SkMakeSpan(kVaryings, SK_ARRAY_COUNT(kVaryings)),
                                              SkString(kVS),
                                              SkString(kFS));
            if (!spec) {
                SkDebugf("%s\n", error.c_str());
            }
            fSpecWithNoColor = std::move(spec);
        }

        static constexpr SkColor kColors[] = {SK_ColorTRANSPARENT, SK_ColorWHITE};
        fShader = SkGradientShader::MakeRadial({10, 10},
                                               3,
                                               kColors,
                                               nullptr,
                                               2,
                                               SkTileMode::kMirror);
    }

    DrawResult onGpuSetup(GrDirectContext* context, SkString* string) override {
        this->ensureBuffers();
        if (!context || context->abandoned()) {
            return DrawResult::kOk;
        }

        fColorVB        = SkMesh::MakeVertexBuffer(context, CpuVBAsData(fColorVB));
        fColorIndexedVB = SkMesh::MakeVertexBuffer(context, CpuVBAsData(fColorIndexedVB));
        fIB[1]          = SkMesh::MakeIndexBuffer (context, CpuIBAsData(fIB[0]));
        if (!fColorVB || !fColorIndexedVB || !fIB[1]) {
            return DrawResult::kFail;
        }
        return DrawResult::kOk;
    }

    void onGpuTeardown() override {
        // Destroy the GPU buffers and recreate on CPU
        fColorVB        = nullptr;
        fColorIndexedVB = nullptr;
        fIB[1]          = nullptr;
        this->ensureBuffers();
    }

    SkString onShortName() override { return SkString("custommesh"); }

    DrawResult onDraw(SkCanvas* canvas, SkString*) override {
        int i = 0;
        for (const sk_sp<SkBlender>& blender : {SkBlender::Mode(SkBlendMode::kDst),
                                                SkBlender::Mode(SkBlendMode::kSrc),
                                                SkBlender::Mode(SkBlendMode::kSaturation)}) {
            canvas->save();
            for (uint8_t alpha  : {0xFF , 0x40})
            for (bool    colors : {false, true})
            for (bool    shader : {false, true}) {
                SkMesh mesh;
                // Rather than pile onto the combinatorics we draw every other test case indexed.
                if ((i & 1) == 0) {
                    if (colors) {
                        mesh = SkMesh::Make(fSpecWithColor,
                                            SkMesh::Mode::kTriangleStrip,
                                            fColorVB,
                                            /*vertexCount= */ 4,
                                            /*vertexOffset=*/ 0,
                                            kRect);
                    } else {
                        mesh = SkMesh::Make(fSpecWithNoColor,
                                            SkMesh::Mode::kTriangleStrip,
                                            fNoColorVB,
                                            /*vertexCount=*/4,
                                            kNoColorOffset,
                                            kRect);
                    }
                } else {
                    // Alternate between CPU and GPU-backend index buffers.
                    auto ib = (i%4 == 0) ? fIB[0] : fIB[1];
                    if (colors) {
                        mesh = SkMesh::MakeIndexed(fSpecWithColor,
                                                   SkMesh::Mode::kTriangles,
                                                   fColorIndexedVB,
                                                   /*vertexCount=*/ 6,
                                                   kColorIndexedOffset,
                                                   std::move(ib),
                                                   /*indexCount=*/6,
                                                   kIndexOffset,
                                                   kRect);
                    } else {
                        mesh = SkMesh::MakeIndexed(fSpecWithNoColor,
                                                   SkMesh::Mode::kTriangles,
                                                   fNoColorIndexedVB,
                                                   /*vertexCount=*/ 6,
                                                   /*vertexOffset=*/0,
                                                   std::move(ib),
                                                   /*indexCount=*/6,
                                                   kIndexOffset,
                                                   kRect);
                    }
                }
                SkPaint paint;
                paint.setColor(SK_ColorGREEN);
                paint.setShader(shader ? fShader : nullptr);
                paint.setAlpha(alpha);

                SkCanvasPriv::DrawMesh(canvas, mesh, blender, paint);

                canvas->translate(0, 150);
                ++i;
            }
            canvas->restore();
            canvas->translate(150, 0);
        }
        return DrawResult::kOk;
    }

private:
    static sk_sp<const SkData> CpuVBAsData(sk_sp<SkMesh::VertexBuffer> buffer) {
        auto vb = static_cast<SkMeshPriv::VB*>(buffer.get());
        SkASSERT(vb->asData());
        return vb->asData();
    }

    static sk_sp<const SkData> CpuIBAsData(sk_sp<SkMesh::IndexBuffer> buffer) {
        auto ib = static_cast<SkMeshPriv::IB*>(buffer.get());
        SkASSERT(ib->asData());
        return ib->asData();
    }

    void ensureBuffers() {
        if (!fColorVB) {
            fColorVB = SkMesh::MakeVertexBuffer(
                    /*GrDirectContext*=*/nullptr,
                    SkData::MakeWithoutCopy(kColorQuad, sizeof(kColorQuad)));
        }

        if (!fNoColorVB) {
            // Make this one such that the data is offset into the buffer.
            auto data = SkData::MakeUninitialized(sizeof(kNoColorQuad) + kNoColorOffset);
            std::memcpy(SkTAddOffset<void>(data->writable_data(), kNoColorOffset),
                        kNoColorQuad,
                        sizeof(kNoColorQuad));
            fNoColorVB = SkMesh::MakeVertexBuffer(/*GrDirectContext*=*/nullptr, std::move(data));
        }

        if (!fColorIndexedVB) {
            // This buffer also has an offset.
            auto data = SkData::MakeUninitialized(sizeof(kColorIndexedQuad) + kColorIndexedOffset);
            std::memcpy(SkTAddOffset<void>(data->writable_data(), kColorIndexedOffset),
                        kColorIndexedQuad,
                        sizeof(kColorIndexedQuad));
            fColorIndexedVB = SkMesh::MakeVertexBuffer(/*GrDirectContext*=*/nullptr,
                                                       std::move(data));
        }

        if (!fNoColorIndexedVB) {
            fNoColorIndexedVB = SkMesh::MakeVertexBuffer(
                    /*GrDirectContext*=*/nullptr,
                    SkData::MakeWithoutCopy(kNoColorIndexedQuad, sizeof(kNoColorIndexedQuad)));
        }

        if (!fIB[0]) {
            // The index buffer has an offset.
            auto data = SkData::MakeUninitialized(sizeof(kIndices) + kIndexOffset);
            std::memcpy(SkTAddOffset<void>(data->writable_data(), kIndexOffset),
                        kIndices,
                        sizeof(kIndices));
            fIB[0] = SkMesh::MakeIndexBuffer(/*GrDirectContext*=*/nullptr, std::move(data));
        }

        if (!fIB[1]) {
            // On CPU we always use the same CPU-backed index buffer.
            fIB[1] = fIB[0];
        }
    }

    struct ColorVertex {
        uint32_t pad;
        uint32_t brag;
        float    xuyv[4];
    };

    struct NoColorVertex {
        float xuyv[4];
    };

    static constexpr auto kRect = SkRect::MakeLTRB(20, 20, 120, 120);
    static constexpr auto kUV   = SkRect::MakeLTRB( 0,  0,  20,  20);

    static constexpr ColorVertex kColorQuad[] {
            {0, 0x00FFFF00, {kRect.left(),  kUV.left(),  kRect.top(),    kUV.top()   }},
            {0, 0x00FFFFFF, {kRect.right(), kUV.right(), kRect.top(),    kUV.top()   }},
            {0, 0xFFFF00FF, {kRect.left(),  kUV.left(),  kRect.bottom(), kUV.bottom()}},
            {0, 0xFFFFFF00, {kRect.right(), kUV.right(), kRect.bottom(), kUV.bottom()}},
    };

    static constexpr NoColorVertex kNoColorQuad[]{
            {{kRect.left(),  kUV.left(),  kRect.top(),    kUV.top()   }},
            {{kRect.right(), kUV.right(), kRect.top(),    kUV.top()   }},
            {{kRect.left(),  kUV.left(),  kRect.bottom(), kUV.bottom()}},
            {{kRect.right(), kUV.right(), kRect.bottom(), kUV.bottom()}},
    };

    // The indexed quads draw the same as the non-indexed. They just have unused vertices that the
    // index buffer skips over draw with triangles instead of a triangle strip.
    static constexpr ColorVertex kColorIndexedQuad[] {
            {0, 0x00FFFF00, {kRect.left(),  kUV.left(),  kRect.top(),    kUV.top()   }},
            {0, 0x00000000, {        100.f,        0.f,        100.f,    5.f         }}, // unused
            {0, 0x00FFFFFF, {kRect.right(), kUV.right(), kRect.top(),    kUV.top()   }},
            {0, 0x00000000, {        200.f,        10.f,        200.f,   10.f        }}, // unused
            {0, 0xFFFF00FF, {kRect.left(),  kUV.left(),  kRect.bottom(), kUV.bottom()}},
            {0, 0xFFFFFF00, {kRect.right(), kUV.right(), kRect.bottom(), kUV.bottom()}},
    };

    static constexpr NoColorVertex kNoColorIndexedQuad[]{
            {{kRect.left(),  kUV.left(),  kRect.top(),    kUV.top()   }},
            {{        100.f,        0.f,        100.f,    5.f         }}, // unused
            {{kRect.right(), kUV.right(), kRect.top(),    kUV.top()   }},
            {{        200.f,        10.f,        200.f,   10.f        }}, // unused
            {{kRect.left(),  kUV.left(),  kRect.bottom(), kUV.bottom()}},
            {{kRect.right(), kUV.right(), kRect.bottom(), kUV.bottom()}},
    };

    static constexpr uint16_t kIndices[]{0, 2, 4, 2, 5, 4};

    // For some buffers we add an offset to ensure we're exercising drawing from mid-buffer.
    static constexpr size_t kNoColorOffset      = sizeof(NoColorVertex);
    static constexpr size_t kColorIndexedOffset = 2*sizeof(ColorVertex);
    static constexpr size_t kIndexOffset        = 6;

    sk_sp<SkShader> fShader;

    sk_sp<SkMeshSpecification> fSpecWithColor;
    sk_sp<SkMeshSpecification> fSpecWithNoColor;

    // On GPU the first IB is a CPU buffer and the second is a GPU buffer.
    sk_sp<SkMesh::IndexBuffer> fIB[2];

    sk_sp<SkMesh::VertexBuffer> fColorVB;
    sk_sp<SkMesh::VertexBuffer> fNoColorVB;
    sk_sp<SkMesh::VertexBuffer> fColorIndexedVB;
    sk_sp<SkMesh::VertexBuffer> fNoColorIndexedVB;
};

DEF_GM(return new MeshGM;)

class MeshColorSpaceGM : public skiagm::GM {
public:
    MeshColorSpaceGM() {}

protected:
    using Attribute = SkMeshSpecification::Attribute;
    using Varying   = SkMeshSpecification::Varying;

    SkISize onISize() override { return {468, 258}; }

    void onOnceBeforeDraw() override {
        static const Attribute kAttributes[]{
                {Attribute::Type::kFloat2, 0, SkString{"pos"}  },
                {Attribute::Type::kFloat4, 8, SkString{"color"}},
        };
        static const Varying kVaryings[]{
                {Varying::Type::kHalf4,  SkString{"color"}},
        };
        static constexpr char kPremulVS[] = R"(
                float2 main(in Attributes attributes, out Varyings varyings) {
                    varyings.color = half4(attributes.color.a*attributes.color.rgb,
                                           attributes.color.a);
                    return attributes.pos;
                }
        )";
        static constexpr char kUnpremulVS[] = R"(
                float2 main(in Attributes attributes, out Varyings varyings) {
                    varyings.color = attributes.color;
                    return attributes.pos;
                }
        )";
        static constexpr char kFS[] = R"(
                void main(in Varyings varyings, out half4 color) {
                    color = varyings.color;
                }
        )";
        for (bool unpremul : {false, true}) {
            auto at = unpremul ? kUnpremul_SkAlphaType : kPremul_SkAlphaType;
            auto vs = unpremul ? kUnpremulVS : kPremulVS;
            for (bool spin : {false, true}) {
                auto cs = SkColorSpace::MakeSRGB();
                if (spin) {
                    cs = cs->makeColorSpin();
                }

                auto [spec, error] = SkMeshSpecification::Make(
                        SkMakeSpan(kAttributes, SK_ARRAY_COUNT(kAttributes)),
                        sizeof(Vertex),
                        SkMakeSpan(kVaryings, SK_ARRAY_COUNT(kVaryings)),
                        SkString(vs),
                        SkString(kFS),
                        std::move(cs),
                        at);
                if (!spec) {
                    SkDebugf("%s\n", error.c_str());
                }
                fSpecs[SpecIndex(unpremul, spin)] = std::move(spec);
            }
        }
        SkPoint pts[]    = {{kRect.fLeft, 0}, {kRect.centerX(), 0}};
        SkColor colors[] = {SK_ColorWHITE,    SK_ColorTRANSPARENT};
        fShader = SkGradientShader::MakeLinear(pts, colors, nullptr, 2, SkTileMode::kMirror);

        fVB = SkMesh::MakeVertexBuffer(nullptr, SkData::MakeWithoutCopy(kQuad, sizeof(kQuad)));
    }

    SkString onShortName() override { return SkString("custommesh_cs"); }

    DrawResult onDraw(SkCanvas* canvas, SkString* error) override {
        // Force an intermediate surface if the canvas is in "legacy" mode
        SkCanvas* c = canvas;
        sk_sp<SkSurface> surface;
        if (!c->imageInfo().colorSpace()) {
            SkImageInfo info = canvas->imageInfo().makeColorSpace(SkColorSpace::MakeSRGB());
            surface = canvas->makeSurface(info);
            if (!surface) {
                // This GM won't work on configs that use a recording canvas.
                return DrawResult::kSkip;
            }
            c = surface->getCanvas();
            c->clear(SK_ColorWHITE);
        }
        for (bool useShader : {false, true})
        for (bool unpremul  : {false, true}) {
            c->save();
            for (bool spin : {false, true}) {
                SkMesh mesh = SkMesh::Make(fSpecs[SpecIndex(unpremul, spin)],
                                           SkMesh::Mode::kTriangleStrip,
                                           fVB,
                                           /*vertexCount=*/ 4,
                                           /*vertexOffset=*/0,
                                           kRect);

                SkPaint paint;
                paint.setShader(useShader ? fShader : nullptr);
                SkBlendMode mode = useShader ? SkBlendMode::kModulate : SkBlendMode::kDst;
                SkCanvasPriv::DrawMesh(c, mesh, SkBlender::Mode(mode), paint);

                c->translate(0, kRect.height() + 10);
            }
            c->restore();
            c->translate(kRect.width() + 10, 0);
            c->save();
        }
        if (surface) {
            surface->draw(canvas, 0, 0);
        }
        return DrawResult::kOk;
    }

private:
    struct Vertex {
        SkPoint   pos;
        SkColor4f color;
    };

    static int SpecIndex(bool spin, bool unpremul) {
        return static_cast<int>(spin) + 2*static_cast<int>(unpremul);
    }

    static constexpr auto kRect = SkRect::MakeLTRB(20, 20, 120, 120);

    static constexpr Vertex kQuad[] {
            {{kRect.left() , kRect.top()   }, {1, 0, 0, 1}},
            {{kRect.right(), kRect.top()   }, {0, 1, 0, 0}},
            {{kRect.left() , kRect.bottom()}, {1, 1, 0, 0}},
            {{kRect.right(), kRect.bottom()}, {0, 0, 1, 1}},
    };

    sk_sp<SkMesh::VertexBuffer> fVB;

    sk_sp<SkMeshSpecification> fSpecs[4];

    sk_sp<SkShader> fShader;
};

DEF_GM(return new MeshColorSpaceGM;)

}  // namespace skiagm
