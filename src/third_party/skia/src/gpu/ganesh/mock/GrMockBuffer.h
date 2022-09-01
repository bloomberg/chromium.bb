/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrMockBuffer_DEFINED
#define GrMockBuffer_DEFINED

#include "src/gpu/ganesh/GrCaps.h"
#include "src/gpu/ganesh/GrGpuBuffer.h"
#include "src/gpu/ganesh/mock/GrMockGpu.h"

class GrMockBuffer : public GrGpuBuffer {
public:
    GrMockBuffer(GrMockGpu* gpu, size_t sizeInBytes, GrGpuBufferType type,
                 GrAccessPattern accessPattern,
                 std::string_view label)
            : INHERITED(gpu, sizeInBytes, type, accessPattern, label) {
        this->registerWithCache(SkBudgeted::kYes);
    }

private:
    void onMap() override {
        if (GrCaps::kNone_MapFlags != this->getGpu()->caps()->mapBufferFlags()) {
            fMapPtr = sk_malloc_throw(this->size());
        }
    }
    void onUnmap() override { sk_free(fMapPtr); }
    bool onUpdateData(const void* src, size_t srcSizeInBytes) override { return true; }

    using INHERITED = GrGpuBuffer;
};

#endif
