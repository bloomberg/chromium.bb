/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkMeshPriv_DEFINED
#define SkMeshPriv_DEFINED

#include "include/core/SkMesh.h"

#ifdef SK_ENABLE_SKSL
#include "include/core/SkData.h"
#include "include/private/gpu/ganesh/GrTypesPriv.h"
#include "src/core/SkSLTypeShared.h"

#if SK_SUPPORT_GPU
#include "include/gpu/GrDirectContext.h"
#include "src/gpu/ganesh/GrDirectContextPriv.h"
#include "src/gpu/ganesh/GrGpuBuffer.h"
#include "src/gpu/ganesh/GrResourceCache.h"
#include "src/gpu/ganesh/GrResourceProvider.h"
#endif

struct SkMeshSpecificationPriv {
    using Varying   = SkMeshSpecification::Varying;
    using Attribute = SkMeshSpecification::Attribute;
    using ColorType = SkMeshSpecification::ColorType;

    static SkSpan<const Varying> Varyings(const SkMeshSpecification& spec) {
        return SkMakeSpan(spec.fVaryings);
    }

    static const SkSL::Program* VS(const SkMeshSpecification& spec) { return spec.fVS.get(); }
    static const SkSL::Program* FS(const SkMeshSpecification& spec) { return spec.fFS.get(); }

    static int Hash(const SkMeshSpecification& spec) { return spec.fHash; }

    static ColorType GetColorType(const SkMeshSpecification& spec) { return spec.fColorType; }
    static bool HasColors(const SkMeshSpecification& spec) {
        return GetColorType(spec) != ColorType::kNone;
    }

    static SkColorSpace* ColorSpace(const SkMeshSpecification& spec) {
        return spec.fColorSpace.get();
    }

    static SkAlphaType AlphaType(const SkMeshSpecification& spec) { return spec.fAlphaType; }

    static bool HasLocalCoords(const SkMeshSpecification& spec) { return spec.fHasLocalCoords; }

    static SkSLType VaryingTypeAsSLType(Varying::Type type) {
        switch (type) {
            case Varying::Type::kFloat:  return SkSLType::kFloat;
            case Varying::Type::kFloat2: return SkSLType::kFloat2;
            case Varying::Type::kFloat3: return SkSLType::kFloat3;
            case Varying::Type::kFloat4: return SkSLType::kFloat4;
            case Varying::Type::kHalf:   return SkSLType::kHalf;
            case Varying::Type::kHalf2:  return SkSLType::kHalf2;
            case Varying::Type::kHalf3:  return SkSLType::kHalf3;
            case Varying::Type::kHalf4:  return SkSLType::kHalf4;
        }
        SkUNREACHABLE;
    }

    static GrVertexAttribType AttrTypeAsVertexAttribType(Attribute::Type type) {
        switch (type) {
            case Attribute::Type::kFloat:        return kFloat_GrVertexAttribType;
            case Attribute::Type::kFloat2:       return kFloat2_GrVertexAttribType;
            case Attribute::Type::kFloat3:       return kFloat3_GrVertexAttribType;
            case Attribute::Type::kFloat4:       return kFloat4_GrVertexAttribType;
            case Attribute::Type::kUByte4_unorm: return kUByte4_norm_GrVertexAttribType;
        }
        SkUNREACHABLE;
    }

    static SkSLType AttrTypeAsSLType(Attribute::Type type) {
        switch (type) {
            case Attribute::Type::kFloat:        return SkSLType::kFloat;
            case Attribute::Type::kFloat2:       return SkSLType::kFloat2;
            case Attribute::Type::kFloat3:       return SkSLType::kFloat3;
            case Attribute::Type::kFloat4:       return SkSLType::kFloat4;
            case Attribute::Type::kUByte4_unorm: return SkSLType::kHalf4;
        }
        SkUNREACHABLE;
    }
};

struct SkMeshPriv {
    class Buffer {
    public:
        virtual ~Buffer() = 0;

        Buffer() = default;
        Buffer(const Buffer&) = delete;

        Buffer& operator=(const Buffer&) = delete;

        virtual sk_sp<const SkData> asData() const { return nullptr; }

#if SK_SUPPORT_GPU
        virtual sk_sp<const GrGpuBuffer> asGpuBuffer() const { return nullptr; }
#endif

        virtual size_t size() const = 0;
    };

    class IB : public Buffer, public SkMesh::IndexBuffer  {};
    class VB : public Buffer, public SkMesh::VertexBuffer {};

    template <typename Base> class CpuBuffer final : public Base {
    public:
        CpuBuffer()           = default;
        ~CpuBuffer() override = default;

        static sk_sp<Base> Make(sk_sp<const SkData> data);

        sk_sp<const SkData> asData() const override { return fData; }

        size_t size() const override { return fData->size(); }

    private:
        sk_sp<const SkData> fData;
    };

    using CpuIndexBuffer  = CpuBuffer<IB>;
    using CpuVertexBuffer = CpuBuffer<VB>;

#if SK_SUPPORT_GPU
    template <typename Base, GrGpuBufferType> class GpuBuffer final : public Base {
    public:
        GpuBuffer() = default;

        ~GpuBuffer() override;

        static sk_sp<Base> Make(GrDirectContext*, sk_sp<const SkData>);

        sk_sp<const GrGpuBuffer> asGpuBuffer() const override { return fBuffer; }

        size_t size() const override { return fBuffer->size(); }

    private:
        sk_sp<GrGpuBuffer> fBuffer;
        GrDirectContext::DirectContextID fContextID;
    };

    using GpuIndexBuffer  = GpuBuffer<IB, GrGpuBufferType::kIndex >;
    using GpuVertexBuffer = GpuBuffer<VB, GrGpuBufferType::kVertex>;
#endif  // SK_SUPPORT_GPU
};

inline SkMeshPriv::Buffer::~Buffer() = default;

template <typename Base> sk_sp<Base> SkMeshPriv::CpuBuffer<Base>::Make(sk_sp<const SkData> data) {
    SkASSERT(data);
    auto result = new CpuBuffer<Base>;
    result->fData = std::move(data);
    return sk_sp<Base>(result);
}
#if SK_SUPPORT_GPU

template <typename Base, GrGpuBufferType Type> SkMeshPriv::GpuBuffer<Base, Type>::~GpuBuffer() {
    GrResourceCache::ReturnResourceFromThread(std::move(fBuffer), fContextID);
}

template <typename Base, GrGpuBufferType Type>
sk_sp<Base> SkMeshPriv::GpuBuffer<Base, Type>::Make(GrDirectContext* dc,sk_sp<const SkData> data) {
    SkASSERT(dc);
    SkASSERT(data);

    sk_sp<GrGpuBuffer> buffer = dc->priv().resourceProvider()->createBuffer(
            data->data(),
            data->size(),
            Type,
            kStatic_GrAccessPattern);
    if (!buffer) {
        return nullptr;
    }

    auto result = new GpuBuffer;
    result->fBuffer    = std::move(buffer);
    result->fContextID = dc->directContextID();
    return sk_sp<Base>(result);
}

#endif  // SK_SUPPORT_GPU

#endif  // SK_ENABLE_SKSL

#endif
