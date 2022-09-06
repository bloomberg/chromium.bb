/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/ganesh/dawn/GrDawnBuffer.h"

#include "src/gpu/ganesh/dawn/GrDawnAsyncWait.h"
#include "src/gpu/ganesh/dawn/GrDawnGpu.h"

namespace {
    wgpu::BufferUsage GrGpuBufferTypeToDawnUsageBit(GrGpuBufferType type) {
        switch (type) {
            case GrGpuBufferType::kVertex:
                return wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
            case GrGpuBufferType::kIndex:
                return wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
            case GrGpuBufferType::kXferCpuToGpu:
                return wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;
            case GrGpuBufferType::kXferGpuToCpu:
                return wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
            default:
                SkASSERT(!"buffer type not supported by Dawn");
                return wgpu::BufferUsage::Vertex;
        }
    }
}

// static
sk_sp<GrDawnBuffer> GrDawnBuffer::Make(GrDawnGpu* gpu,
                                       size_t sizeInBytes,
                                       GrGpuBufferType type,
                                       GrAccessPattern pattern,
                                       std::string_view label) {
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = sizeInBytes;
    bufferDesc.usage = GrGpuBufferTypeToDawnUsageBit(type);

    Mappable mappable = Mappable::kNot;
    if (bufferDesc.usage & wgpu::BufferUsage::MapRead) {
        SkASSERT(!SkToBool(bufferDesc.usage & wgpu::BufferUsage::MapWrite));
        mappable = Mappable::kReadOnly;
    } else if (bufferDesc.usage & wgpu::BufferUsage::MapWrite) {
        mappable = Mappable::kWriteOnly;
    }

    wgpu::Buffer buffer;
    void* mapPtr = nullptr;
    if (mappable == Mappable::kNot || mappable == Mappable::kReadOnly) {
        buffer = gpu->device().CreateBuffer(&bufferDesc);
    } else {
        bufferDesc.mappedAtCreation = true;
        buffer = gpu->device().CreateBuffer(&bufferDesc);
        mapPtr = buffer.GetMappedRange();
        if (!mapPtr) {
            SkDebugf("GrDawnBuffer: failed to map buffer at creation\n");
            return nullptr;
        }
    }

    return sk_sp<GrDawnBuffer>(new GrDawnBuffer(
            gpu, sizeInBytes, type, pattern, label, mappable, std::move(buffer), mapPtr));
}

GrDawnBuffer::GrDawnBuffer(GrDawnGpu* gpu,
                           size_t sizeInBytes,
                           GrGpuBufferType type,
                           GrAccessPattern pattern,
                           std::string_view label,
                           Mappable mappable,
                           wgpu::Buffer buffer,
                           void* mapPtr)
        : INHERITED(gpu, sizeInBytes, type, pattern, label)
        , fBuffer(std::move(buffer))
        , fMappable(mappable) {
    fMapPtr = mapPtr;

    // We want to make the blocking map in `onMap` available initially only for read-only buffers,
    // which are not mapped at creation or backed by a staging buffer which gets mapped
    // independently. Note that the blocking map procedure becomes available to both read-only and
    // write-only buffers once they get explicitly unmapped.
    fUnmapped = (mapPtr == nullptr && mappable == Mappable::kReadOnly);
    this->registerWithCache(SkBudgeted::kYes);
}

void GrDawnBuffer::onMap() {
    if (this->wasDestroyed()) {
        return;
    }

    if (fUnmapped) {
        SkASSERT(fMappable != Mappable::kNot);
        if (!this->blockingMap()) {
            SkDebugf("GrDawnBuffer: failed to map buffer\n");
            return;
        }
        fUnmapped = false;
    }

    if (fMappable == Mappable::kNot) {
        GrStagingBufferManager::Slice slice =
                this->getDawnGpu()->stagingBufferManager()->allocateStagingBufferSlice(
                        this->size());
        fStagingBuffer = static_cast<GrDawnBuffer*>(slice.fBuffer)->get();
        fStagingOffset = slice.fOffset;
        fMapPtr = slice.fOffsetMapPtr;
    } else {
        // We always create this buffers mapped or if they've been used on the gpu before we use the
        // async map callback to know when it is safe to reuse them. Thus by the time we get here
        // the buffer should always be mapped.
        SkASSERT(this->isMapped());
    }
}

void GrDawnBuffer::onUnmap() {
    if (this->wasDestroyed()) {
        return;
    }

    if (fMappable == Mappable::kNot) {
        this->getDawnGpu()->getCopyEncoder().CopyBufferToBuffer(fStagingBuffer, fStagingOffset,
                                                                fBuffer, 0, this->size());
    } else {
        fBuffer.Unmap();
        fUnmapped = true;
    }
}

bool GrDawnBuffer::onUpdateData(const void* src, size_t srcSizeInBytes) {
    if (this->wasDestroyed()) {
        return false;
    }

    this->map();
    if (!this->isMapped()) {
        return false;
    }

    memcpy(fMapPtr, src, srcSizeInBytes);
    this->unmap();
    return true;
}

GrDawnGpu* GrDawnBuffer::getDawnGpu() const {
    SkASSERT(!this->wasDestroyed());
    return static_cast<GrDawnGpu*>(this->getGpu());
}

void GrDawnBuffer::mapAsync(MapAsyncCallback callback) {
    SkASSERT(fMappable != Mappable::kNot);
    SkASSERT(!fMapAsyncCallback);
    SkASSERT(!this->isMapped());

    fMapAsyncCallback = std::move(callback);
    fBuffer.MapAsync(
            (fMappable == Mappable::kReadOnly) ? wgpu::MapMode::Read : wgpu::MapMode::Write,
            0,
            wgpu::kWholeMapSize,
            [](WGPUBufferMapAsyncStatus status, void* userData) {
                static_cast<GrDawnBuffer*>(userData)->mapAsyncDone(status);
            },
            this);
}

void GrDawnBuffer::mapAsyncDone(WGPUBufferMapAsyncStatus status) {
    SkASSERT(fMapAsyncCallback);
    auto callback = std::move(fMapAsyncCallback);

    if (status != WGPUBufferMapAsyncStatus_Success) {
        SkDebugf("GrDawnBuffer: failed to map buffer (status: %u)\n", status);
        callback(false);
        return;
    }

    if (fMappable == Mappable::kReadOnly) {
        fMapPtr = const_cast<void*>(fBuffer.GetConstMappedRange());
    } else {
        fMapPtr = fBuffer.GetMappedRange();
    }

    if (this->isMapped()) {
        fUnmapped = false;
    }

    // Run the callback as the last step in this function since the callback can deallocate this
    // GrDawnBuffer.
    callback(this->isMapped());
}

bool GrDawnBuffer::blockingMap() {
    SkASSERT(fMappable != Mappable::kNot);

    GrDawnAsyncWait wait(this->getDawnGpu()->device());
    this->mapAsync([&wait](bool) { wait.signal(); });
    wait.busyWait();

    return this->isMapped();
}
