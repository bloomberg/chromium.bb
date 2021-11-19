/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_ResourceProvider_DEFINED
#define skgpu_ResourceProvider_DEFINED

#include "experimental/graphite/include/private/GraphiteTypesPriv.h"
#include "experimental/graphite/src/CommandBuffer.h"
#include "experimental/graphite/src/RenderPipeline.h"
#include "experimental/graphite/src/RenderPipelineDesc.h"
#include "include/core/SkSize.h"
#include "src/core/SkLRUCache.h"

namespace skgpu {

class Buffer;
class CommandBuffer;
class Gpu;
class RenderPipeline;
class Texture;
class TextureInfo;

class ResourceProvider {
public:
    virtual ~ResourceProvider();

    virtual sk_sp<CommandBuffer> createCommandBuffer() = 0;

    sk_sp<RenderPipeline> findOrCreateRenderPipeline(const RenderPipelineDesc&);

    sk_sp<Texture> findOrCreateTexture(SkISize, const TextureInfo&);

    sk_sp<Buffer> findOrCreateBuffer(size_t size, BufferType type, PrioritizeGpuReads);

protected:
    ResourceProvider(const Gpu* gpu);

    const Gpu* fGpu;

private:
    virtual sk_sp<RenderPipeline> onCreateRenderPipeline(const RenderPipelineDesc&) = 0;
    virtual sk_sp<Texture> createTexture(SkISize, const TextureInfo&) = 0;
    virtual sk_sp<Buffer> createBuffer(size_t size, BufferType type, PrioritizeGpuReads) = 0;

    class RenderPipelineCache {
    public:
        RenderPipelineCache(ResourceProvider* resourceProvider);
        ~RenderPipelineCache();

        void release();
        sk_sp<RenderPipeline> refPipeline(const RenderPipelineDesc&);

    private:
        struct Entry;

        struct DescHash {
            uint32_t operator()(const RenderPipelineDesc& desc) const {
                return SkOpts::hash_fn(desc.asKey(), desc.keyLength(), 0);
            }
        };

        SkLRUCache<const RenderPipelineDesc, std::unique_ptr<Entry>, DescHash> fMap;

        ResourceProvider* fResourceProvider;
    };

    // Cache of RenderPipelines
    std::unique_ptr<RenderPipelineCache> fRenderPipelineCache;
};

} // namespace skgpu

#endif // skgpu_ResourceProvider_DEFINED
