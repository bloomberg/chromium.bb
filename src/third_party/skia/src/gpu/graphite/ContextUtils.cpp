/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/ContextUtils.h"

#include <string>
#include "include/private/SkUniquePaintParamsID.h"
#include "src/core/SkBlenderBase.h"
#include "src/core/SkKeyContext.h"
#include "src/core/SkPipelineData.h"
#include "src/core/SkShaderCodeDictionary.h"
#include "src/gpu/graphite/PaintParams.h"
#include "src/gpu/graphite/RecorderPriv.h"
#include "src/gpu/graphite/Renderer.h"
#include "src/gpu/graphite/ResourceProvider.h"
#include "src/gpu/graphite/UniformManager.h"

namespace skgpu::graphite {

std::tuple<SkUniquePaintParamsID, UniformDataCache::Index, TextureDataCache::Index>
ExtractPaintData(Recorder* recorder,
                 SkPipelineDataGatherer* gatherer,
                 SkPaintParamsKeyBuilder* builder,
                 const PaintParams& p) {

    SkDEBUGCODE(gatherer->checkReset());
    SkDEBUGCODE(builder->checkReset());

    SkKeyContext keyContext(recorder);

    p.toKey(keyContext, builder, gatherer);

    SkPaintParamsKey key = builder->lockAsKey();

    auto dict = recorder->priv().resourceProvider()->shaderCodeDictionary();
    UniformDataCache* uniformDataCache = recorder->priv().uniformDataCache();
    TextureDataCache* textureDataCache = recorder->priv().textureDataCache();

    auto entry = dict->findOrCreate(key, gatherer->blendInfo());
    UniformDataCache::Index uniformIndex = uniformDataCache->insert(gatherer->peekUniformData());
    TextureDataCache::Index textureIndex = textureDataCache->insert(gatherer->textureDataBlock());

    gatherer->reset();

    return { entry->uniqueID(), uniformIndex, textureIndex };
}

UniformDataCache::Index ExtractRenderStepData(UniformDataCache* geometryUniformDataCache,
                                              SkPipelineDataGatherer* gatherer,
                                              const RenderStep* step,
                                              const DrawGeometry& geometry) {
    SkDEBUGCODE(gatherer->checkReset());

    step->writeUniforms(geometry, gatherer);

    UniformDataCache::Index uIndex = geometryUniformDataCache->insert(gatherer->peekUniformData());

    gatherer->reset();

    return uIndex;
}

} // namespace skgpu::graphite
