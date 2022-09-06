// Copyright 2017 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dawn/native/opengl/CommandBufferGL.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "dawn/native/BindGroup.h"
#include "dawn/native/BindGroupTracker.h"
#include "dawn/native/CommandEncoder.h"
#include "dawn/native/Commands.h"
#include "dawn/native/ExternalTexture.h"
#include "dawn/native/RenderBundle.h"
#include "dawn/native/VertexFormat.h"
#include "dawn/native/opengl/BufferGL.h"
#include "dawn/native/opengl/ComputePipelineGL.h"
#include "dawn/native/opengl/DeviceGL.h"
#include "dawn/native/opengl/Forward.h"
#include "dawn/native/opengl/PersistentPipelineStateGL.h"
#include "dawn/native/opengl/PipelineLayoutGL.h"
#include "dawn/native/opengl/RenderPipelineGL.h"
#include "dawn/native/opengl/SamplerGL.h"
#include "dawn/native/opengl/TextureGL.h"
#include "dawn/native/opengl/UtilsGL.h"

namespace dawn::native::opengl {

namespace {

GLenum IndexFormatType(wgpu::IndexFormat format) {
    switch (format) {
        case wgpu::IndexFormat::Uint16:
            return GL_UNSIGNED_SHORT;
        case wgpu::IndexFormat::Uint32:
            return GL_UNSIGNED_INT;
        case wgpu::IndexFormat::Undefined:
            break;
    }
    UNREACHABLE();
}

GLenum VertexFormatType(wgpu::VertexFormat format) {
    switch (format) {
        case wgpu::VertexFormat::Uint8x2:
        case wgpu::VertexFormat::Uint8x4:
        case wgpu::VertexFormat::Unorm8x2:
        case wgpu::VertexFormat::Unorm8x4:
            return GL_UNSIGNED_BYTE;
        case wgpu::VertexFormat::Sint8x2:
        case wgpu::VertexFormat::Sint8x4:
        case wgpu::VertexFormat::Snorm8x2:
        case wgpu::VertexFormat::Snorm8x4:
            return GL_BYTE;
        case wgpu::VertexFormat::Uint16x2:
        case wgpu::VertexFormat::Uint16x4:
        case wgpu::VertexFormat::Unorm16x2:
        case wgpu::VertexFormat::Unorm16x4:
            return GL_UNSIGNED_SHORT;
        case wgpu::VertexFormat::Sint16x2:
        case wgpu::VertexFormat::Sint16x4:
        case wgpu::VertexFormat::Snorm16x2:
        case wgpu::VertexFormat::Snorm16x4:
            return GL_SHORT;
        case wgpu::VertexFormat::Float16x2:
        case wgpu::VertexFormat::Float16x4:
            return GL_HALF_FLOAT;
        case wgpu::VertexFormat::Float32:
        case wgpu::VertexFormat::Float32x2:
        case wgpu::VertexFormat::Float32x3:
        case wgpu::VertexFormat::Float32x4:
            return GL_FLOAT;
        case wgpu::VertexFormat::Uint32:
        case wgpu::VertexFormat::Uint32x2:
        case wgpu::VertexFormat::Uint32x3:
        case wgpu::VertexFormat::Uint32x4:
            return GL_UNSIGNED_INT;
        case wgpu::VertexFormat::Sint32:
        case wgpu::VertexFormat::Sint32x2:
        case wgpu::VertexFormat::Sint32x3:
        case wgpu::VertexFormat::Sint32x4:
            return GL_INT;
        default:
            UNREACHABLE();
    }
}

GLboolean VertexFormatIsNormalized(wgpu::VertexFormat format) {
    switch (format) {
        case wgpu::VertexFormat::Unorm8x2:
        case wgpu::VertexFormat::Unorm8x4:
        case wgpu::VertexFormat::Snorm8x2:
        case wgpu::VertexFormat::Snorm8x4:
        case wgpu::VertexFormat::Unorm16x2:
        case wgpu::VertexFormat::Unorm16x4:
        case wgpu::VertexFormat::Snorm16x2:
        case wgpu::VertexFormat::Snorm16x4:
            return GL_TRUE;
        default:
            return GL_FALSE;
    }
}

bool VertexFormatIsInt(wgpu::VertexFormat format) {
    switch (format) {
        case wgpu::VertexFormat::Uint8x2:
        case wgpu::VertexFormat::Uint8x4:
        case wgpu::VertexFormat::Sint8x2:
        case wgpu::VertexFormat::Sint8x4:
        case wgpu::VertexFormat::Uint16x2:
        case wgpu::VertexFormat::Uint16x4:
        case wgpu::VertexFormat::Sint16x2:
        case wgpu::VertexFormat::Sint16x4:
        case wgpu::VertexFormat::Uint32:
        case wgpu::VertexFormat::Uint32x2:
        case wgpu::VertexFormat::Uint32x3:
        case wgpu::VertexFormat::Uint32x4:
        case wgpu::VertexFormat::Sint32:
        case wgpu::VertexFormat::Sint32x2:
        case wgpu::VertexFormat::Sint32x3:
        case wgpu::VertexFormat::Sint32x4:
            return true;
        default:
            return false;
    }
}

// Vertex buffers and index buffers are implemented as part of an OpenGL VAO that
// corresponds to a VertexState. On the contrary in Dawn they are part of the global state.
// This means that we have to re-apply these buffers on a VertexState change.
class VertexStateBufferBindingTracker {
  public:
    void OnSetIndexBuffer(BufferBase* buffer) {
        mIndexBufferDirty = true;
        mIndexBuffer = ToBackend(buffer);
    }

    void OnSetVertexBuffer(VertexBufferSlot slot, BufferBase* buffer, uint64_t offset) {
        mVertexBuffers[slot] = ToBackend(buffer);
        mVertexBufferOffsets[slot] = offset;
        mDirtyVertexBuffers.set(slot);
    }

    void OnSetPipeline(RenderPipelineBase* pipeline) {
        if (mLastPipeline == pipeline) {
            return;
        }

        mIndexBufferDirty = true;
        mDirtyVertexBuffers |= pipeline->GetVertexBufferSlotsUsed();

        mLastPipeline = pipeline;
    }

    void Apply(const OpenGLFunctions& gl) {
        if (mIndexBufferDirty && mIndexBuffer != nullptr) {
            gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer->GetHandle());
            mIndexBufferDirty = false;
        }

        for (VertexBufferSlot slot :
             IterateBitSet(mDirtyVertexBuffers & mLastPipeline->GetVertexBufferSlotsUsed())) {
            for (VertexAttributeLocation location :
                 IterateBitSet(ToBackend(mLastPipeline)->GetAttributesUsingVertexBuffer(slot))) {
                const VertexAttributeInfo& attribute = mLastPipeline->GetAttribute(location);

                GLuint attribIndex = static_cast<GLuint>(static_cast<uint8_t>(location));
                GLuint buffer = mVertexBuffers[slot]->GetHandle();
                uint64_t offset = mVertexBufferOffsets[slot];

                const VertexBufferInfo& vertexBuffer = mLastPipeline->GetVertexBuffer(slot);
                uint32_t components = GetVertexFormatInfo(attribute.format).componentCount;
                GLenum formatType = VertexFormatType(attribute.format);

                GLboolean normalized = VertexFormatIsNormalized(attribute.format);
                gl.BindBuffer(GL_ARRAY_BUFFER, buffer);
                if (VertexFormatIsInt(attribute.format)) {
                    gl.VertexAttribIPointer(
                        attribIndex, components, formatType, vertexBuffer.arrayStride,
                        reinterpret_cast<void*>(static_cast<intptr_t>(offset + attribute.offset)));
                } else {
                    gl.VertexAttribPointer(
                        attribIndex, components, formatType, normalized, vertexBuffer.arrayStride,
                        reinterpret_cast<void*>(static_cast<intptr_t>(offset + attribute.offset)));
                }
            }
        }

        mDirtyVertexBuffers.reset();
    }

  private:
    bool mIndexBufferDirty = false;
    Buffer* mIndexBuffer = nullptr;

    ityp::bitset<VertexBufferSlot, kMaxVertexBuffers> mDirtyVertexBuffers;
    ityp::array<VertexBufferSlot, Buffer*, kMaxVertexBuffers> mVertexBuffers;
    ityp::array<VertexBufferSlot, uint64_t, kMaxVertexBuffers> mVertexBufferOffsets;

    RenderPipelineBase* mLastPipeline = nullptr;
};

class BindGroupTracker : public BindGroupTrackerBase<false, uint64_t> {
  public:
    void OnSetPipeline(RenderPipeline* pipeline) {
        BindGroupTrackerBase::OnSetPipeline(pipeline);
        mPipeline = pipeline;
    }

    void OnSetPipeline(ComputePipeline* pipeline) {
        BindGroupTrackerBase::OnSetPipeline(pipeline);
        mPipeline = pipeline;
    }

    void Apply(const OpenGLFunctions& gl) {
        BeforeApply();
        for (BindGroupIndex index : IterateBitSet(mDirtyBindGroupsObjectChangedOrIsDynamic)) {
            ApplyBindGroup(gl, index, mBindGroups[index], mDynamicOffsetCounts[index],
                           mDynamicOffsets[index].data());
        }
        AfterApply();
    }

  private:
    void ApplyBindGroup(const OpenGLFunctions& gl,
                        BindGroupIndex index,
                        BindGroupBase* group,
                        uint32_t dynamicOffsetCount,
                        uint64_t* dynamicOffsets) {
        const auto& indices = ToBackend(mPipelineLayout)->GetBindingIndexInfo()[index];
        uint32_t currentDynamicOffsetIndex = 0;

        for (BindingIndex bindingIndex{0}; bindingIndex < group->GetLayout()->GetBindingCount();
             ++bindingIndex) {
            const BindingInfo& bindingInfo = group->GetLayout()->GetBindingInfo(bindingIndex);

            if (bindingInfo.bindingType == BindingInfoType::Texture) {
                TextureView* view = ToBackend(group->GetBindingAsTextureView(bindingIndex));
                view->CopyIfNeeded();
            }
        }

        for (BindingIndex bindingIndex{0}; bindingIndex < group->GetLayout()->GetBindingCount();
             ++bindingIndex) {
            const BindingInfo& bindingInfo = group->GetLayout()->GetBindingInfo(bindingIndex);

            switch (bindingInfo.bindingType) {
                case BindingInfoType::Buffer: {
                    BufferBinding binding = group->GetBindingAsBufferBinding(bindingIndex);
                    GLuint buffer = ToBackend(binding.buffer)->GetHandle();
                    GLuint index = indices[bindingIndex];
                    GLuint offset = binding.offset;

                    if (bindingInfo.buffer.hasDynamicOffset) {
                        offset += dynamicOffsets[currentDynamicOffsetIndex];
                        ++currentDynamicOffsetIndex;
                    }

                    GLenum target;
                    switch (bindingInfo.buffer.type) {
                        case wgpu::BufferBindingType::Uniform:
                            target = GL_UNIFORM_BUFFER;
                            break;
                        case wgpu::BufferBindingType::Storage:
                        case kInternalStorageBufferBinding:
                        case wgpu::BufferBindingType::ReadOnlyStorage:
                            target = GL_SHADER_STORAGE_BUFFER;
                            break;
                        case wgpu::BufferBindingType::Undefined:
                            UNREACHABLE();
                    }

                    gl.BindBufferRange(target, index, buffer, offset, binding.size);
                    break;
                }

                case BindingInfoType::Sampler: {
                    Sampler* sampler = ToBackend(group->GetBindingAsSampler(bindingIndex));
                    GLuint samplerIndex = indices[bindingIndex];

                    for (PipelineGL::SamplerUnit unit :
                         mPipeline->GetTextureUnitsForSampler(samplerIndex)) {
                        // Only use filtering for certain texture units, because int
                        // and uint texture are only complete without filtering
                        if (unit.shouldUseFiltering) {
                            gl.BindSampler(unit.unit, sampler->GetFilteringHandle());
                        } else {
                            gl.BindSampler(unit.unit, sampler->GetNonFilteringHandle());
                        }
                    }
                    break;
                }

                case BindingInfoType::Texture: {
                    TextureView* view = ToBackend(group->GetBindingAsTextureView(bindingIndex));
                    GLuint handle = view->GetHandle();
                    GLenum target = view->GetGLTarget();
                    GLuint viewIndex = indices[bindingIndex];

                    for (auto unit : mPipeline->GetTextureUnitsForTextureView(viewIndex)) {
                        gl.ActiveTexture(GL_TEXTURE0 + unit);
                        gl.BindTexture(target, handle);
                        if (ToBackend(view->GetTexture())->GetGLFormat().format ==
                            GL_DEPTH_STENCIL) {
                            Aspect aspect = view->GetAspects();
                            ASSERT(HasOneBit(aspect));
                            switch (aspect) {
                                case Aspect::None:
                                case Aspect::Color:
                                case Aspect::CombinedDepthStencil:
                                case Aspect::Plane0:
                                case Aspect::Plane1:
                                    UNREACHABLE();
                                case Aspect::Depth:
                                    gl.TexParameteri(target, GL_DEPTH_STENCIL_TEXTURE_MODE,
                                                     GL_DEPTH_COMPONENT);
                                    break;
                                case Aspect::Stencil:
                                    gl.TexParameteri(target, GL_DEPTH_STENCIL_TEXTURE_MODE,
                                                     GL_STENCIL_INDEX);
                                    break;
                            }
                        }
                    }
                    break;
                }

                case BindingInfoType::StorageTexture: {
                    TextureView* view = ToBackend(group->GetBindingAsTextureView(bindingIndex));
                    Texture* texture = ToBackend(view->GetTexture());
                    GLuint handle = texture->GetHandle();
                    GLuint imageIndex = indices[bindingIndex];

                    GLenum access;
                    switch (bindingInfo.storageTexture.access) {
                        case wgpu::StorageTextureAccess::WriteOnly:
                            access = GL_WRITE_ONLY;
                            break;
                        case wgpu::StorageTextureAccess::Undefined:
                            UNREACHABLE();
                    }

                    // OpenGL ES only supports either binding a layer or the entire
                    // texture in glBindImageTexture().
                    GLboolean isLayered;
                    if (view->GetLayerCount() == 1) {
                        isLayered = GL_FALSE;
                    } else if (texture->GetArrayLayers() == view->GetLayerCount()) {
                        isLayered = GL_TRUE;
                    } else {
                        UNREACHABLE();
                    }

                    gl.BindImageTexture(imageIndex, handle, view->GetBaseMipLevel(), isLayered,
                                        view->GetBaseArrayLayer(), access,
                                        texture->GetGLFormat().internalFormat);
                    texture->Touch();
                    break;
                }

                case BindingInfoType::ExternalTexture:
                    UNREACHABLE();
                    break;
            }
        }
    }

    PipelineGL* mPipeline = nullptr;
};

void ResolveMultisampledRenderTargets(const OpenGLFunctions& gl,
                                      const BeginRenderPassCmd* renderPass) {
    ASSERT(renderPass != nullptr);

    GLuint readFbo = 0;
    GLuint writeFbo = 0;

    for (ColorAttachmentIndex i :
         IterateBitSet(renderPass->attachmentState->GetColorAttachmentsMask())) {
        if (renderPass->colorAttachments[i].resolveTarget != nullptr) {
            if (readFbo == 0) {
                ASSERT(writeFbo == 0);
                gl.GenFramebuffers(1, &readFbo);
                gl.GenFramebuffers(1, &writeFbo);
            }

            TextureView* colorView = ToBackend(renderPass->colorAttachments[i].view.Get());

            gl.BindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
            colorView->BindToFramebuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0);

            TextureView* resolveView =
                ToBackend(renderPass->colorAttachments[i].resolveTarget.Get());
            gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFbo);
            resolveView->BindToFramebuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0);
            gl.BlitFramebuffer(0, 0, renderPass->width, renderPass->height, 0, 0, renderPass->width,
                               renderPass->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            ToBackend(resolveView->GetTexture())->Touch();
        }
    }

    gl.DeleteFramebuffers(1, &readFbo);
    gl.DeleteFramebuffers(1, &writeFbo);
}

// OpenGL SPEC requires the source/destination region must be a region that is contained
// within srcImage/dstImage. Here the size of the image refers to the virtual size, while
// Dawn validates texture copy extent with the physical size, so we need to re-calculate the
// texture copy extent to ensure it should fit in the virtual size of the subresource.
Extent3D ComputeTextureCopyExtent(const TextureCopy& textureCopy, const Extent3D& copySize) {
    Extent3D validTextureCopyExtent = copySize;
    const TextureBase* texture = textureCopy.texture.Get();
    Extent3D virtualSizeAtLevel =
        texture->GetMipLevelSingleSubresourceVirtualSize(textureCopy.mipLevel);
    ASSERT(textureCopy.origin.x <= virtualSizeAtLevel.width);
    ASSERT(textureCopy.origin.y <= virtualSizeAtLevel.height);
    if (copySize.width > virtualSizeAtLevel.width - textureCopy.origin.x) {
        ASSERT(texture->GetFormat().isCompressed);
        validTextureCopyExtent.width = virtualSizeAtLevel.width - textureCopy.origin.x;
    }
    if (copySize.height > virtualSizeAtLevel.height - textureCopy.origin.y) {
        ASSERT(texture->GetFormat().isCompressed);
        validTextureCopyExtent.height = virtualSizeAtLevel.height - textureCopy.origin.y;
    }

    return validTextureCopyExtent;
}

bool TextureFormatIsSnorm(wgpu::TextureFormat format) {
    return format == wgpu::TextureFormat::RGBA8Snorm || format == wgpu::TextureFormat::RG8Snorm ||
           format == wgpu::TextureFormat::R8Snorm;
}
}  // namespace

CommandBuffer::CommandBuffer(CommandEncoder* encoder, const CommandBufferDescriptor* descriptor)
    : CommandBufferBase(encoder, descriptor) {}

MaybeError CommandBuffer::Execute() {
    const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;

    auto LazyClearSyncScope = [](const SyncScopeResourceUsage& scope) {
        for (size_t i = 0; i < scope.textures.size(); i++) {
            Texture* texture = ToBackend(scope.textures[i]);

            // Clear subresources that are not render attachments. Render attachments will be
            // cleared in RecordBeginRenderPass by setting the loadop to clear when the texture
            // subresource has not been initialized before the render pass.
            scope.textureUsages[i].Iterate(
                [&](const SubresourceRange& range, wgpu::TextureUsage usage) {
                    if (usage & ~wgpu::TextureUsage::RenderAttachment) {
                        texture->EnsureSubresourceContentInitialized(range);
                    }
                });
        }

        for (BufferBase* bufferBase : scope.buffers) {
            ToBackend(bufferBase)->EnsureDataInitialized();
        }
    };

    size_t nextComputePassNumber = 0;
    size_t nextRenderPassNumber = 0;

    Command type;
    while (mCommands.NextCommandId(&type)) {
        switch (type) {
            case Command::BeginComputePass: {
                mCommands.NextCommand<BeginComputePassCmd>();
                for (const SyncScopeResourceUsage& scope :
                     GetResourceUsages().computePasses[nextComputePassNumber].dispatchUsages) {
                    LazyClearSyncScope(scope);
                }
                DAWN_TRY(ExecuteComputePass());

                nextComputePassNumber++;
                break;
            }

            case Command::BeginRenderPass: {
                auto* cmd = mCommands.NextCommand<BeginRenderPassCmd>();
                LazyClearSyncScope(GetResourceUsages().renderPasses[nextRenderPassNumber]);
                LazyClearRenderPassAttachments(cmd);
                DAWN_TRY(ExecuteRenderPass(cmd));

                nextRenderPassNumber++;
                break;
            }

            case Command::CopyBufferToBuffer: {
                CopyBufferToBufferCmd* copy = mCommands.NextCommand<CopyBufferToBufferCmd>();
                if (copy->size == 0) {
                    // Skip no-op copies.
                    break;
                }

                ToBackend(copy->source)->EnsureDataInitialized();
                ToBackend(copy->destination)
                    ->EnsureDataInitializedAsDestination(copy->destinationOffset, copy->size);

                gl.BindBuffer(GL_PIXEL_PACK_BUFFER, ToBackend(copy->source)->GetHandle());
                gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, ToBackend(copy->destination)->GetHandle());
                gl.CopyBufferSubData(GL_PIXEL_PACK_BUFFER, GL_PIXEL_UNPACK_BUFFER,
                                     copy->sourceOffset, copy->destinationOffset, copy->size);

                gl.BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
                gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                break;
            }

            case Command::CopyBufferToTexture: {
                CopyBufferToTextureCmd* copy = mCommands.NextCommand<CopyBufferToTextureCmd>();
                if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                    copy->copySize.depthOrArrayLayers == 0) {
                    // Skip no-op copies.
                    continue;
                }
                auto& src = copy->source;
                auto& dst = copy->destination;
                Buffer* buffer = ToBackend(src.buffer.Get());

                DAWN_INVALID_IF(
                    dst.aspect == Aspect::Stencil,
                    "Copies to stencil textures are unsupported on the OpenGL backend.");

                ASSERT(dst.aspect == Aspect::Color);

                buffer->EnsureDataInitialized();
                SubresourceRange range = GetSubresourcesAffectedByCopy(dst, copy->copySize);
                if (IsCompleteSubresourceCopiedTo(dst.texture.Get(), copy->copySize,
                                                  dst.mipLevel)) {
                    dst.texture->SetIsSubresourceContentInitialized(true, range);
                } else {
                    ToBackend(dst.texture)->EnsureSubresourceContentInitialized(range);
                }

                gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer->GetHandle());

                TextureDataLayout dataLayout;
                dataLayout.offset = 0;
                dataLayout.bytesPerRow = src.bytesPerRow;
                dataLayout.rowsPerImage = src.rowsPerImage;

                DoTexSubImage(gl, dst, reinterpret_cast<void*>(src.offset), dataLayout,
                              copy->copySize);
                gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                ToBackend(dst.texture)->Touch();
                break;
            }

            case Command::CopyTextureToBuffer: {
                CopyTextureToBufferCmd* copy = mCommands.NextCommand<CopyTextureToBufferCmd>();
                if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                    copy->copySize.depthOrArrayLayers == 0) {
                    // Skip no-op copies.
                    continue;
                }
                auto& src = copy->source;
                auto& dst = copy->destination;
                auto& copySize = copy->copySize;
                Texture* texture = ToBackend(src.texture.Get());
                Buffer* buffer = ToBackend(dst.buffer.Get());
                const Format& formatInfo = texture->GetFormat();
                const GLFormat& format = texture->GetGLFormat();
                GLenum target = texture->GetGLTarget();

                // TODO(crbug.com/dawn/667): Implement validation in WebGPU/Compat to
                // avoid this codepath. OpenGL does not support readback from non-renderable
                // texture formats.
                if (formatInfo.isCompressed ||
                    (TextureFormatIsSnorm(formatInfo.format) &&
                     GetDevice()->IsToggleEnabled(Toggle::DisableSnormRead))) {
                    UNREACHABLE();
                }

                buffer->EnsureDataInitializedAsDestination(copy);

                ASSERT(texture->GetDimension() != wgpu::TextureDimension::e1D);
                SubresourceRange subresources = GetSubresourcesAffectedByCopy(src, copy->copySize);
                texture->EnsureSubresourceContentInitialized(subresources);
                // The only way to move data from a texture to a buffer in GL is via
                // glReadPixels with a pack buffer. Create a temporary FBO for the copy.
                gl.BindTexture(target, texture->GetHandle());

                GLuint readFBO = 0;
                gl.GenFramebuffers(1, &readFBO);
                gl.BindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);

                const TexelBlockInfo& blockInfo = formatInfo.GetAspectInfo(src.aspect).block;

                gl.BindBuffer(GL_PIXEL_PACK_BUFFER, buffer->GetHandle());
                gl.PixelStorei(GL_PACK_ROW_LENGTH, dst.bytesPerRow / blockInfo.byteSize);

                GLenum glAttachment;
                GLenum glFormat;
                GLenum glType;
                switch (src.aspect) {
                    case Aspect::Color:
                        glAttachment = GL_COLOR_ATTACHMENT0;
                        glFormat = format.format;
                        glType = format.type;
                        break;
                    case Aspect::Depth:
                        glAttachment = GL_DEPTH_ATTACHMENT;
                        glFormat = GL_DEPTH_COMPONENT;
                        glType = GL_FLOAT;
                        break;
                    case Aspect::Stencil:
                        glAttachment = GL_STENCIL_ATTACHMENT;
                        glFormat = GL_STENCIL_INDEX;
                        glType = GL_UNSIGNED_BYTE;
                        break;

                    case Aspect::CombinedDepthStencil:
                    case Aspect::None:
                    case Aspect::Plane0:
                    case Aspect::Plane1:
                        UNREACHABLE();
                }

                uint8_t* offset = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(dst.offset));
                switch (texture->GetDimension()) {
                    case wgpu::TextureDimension::e2D: {
                        if (texture->GetArrayLayers() == 1) {
                            gl.FramebufferTexture2D(GL_READ_FRAMEBUFFER, glAttachment, target,
                                                    texture->GetHandle(), src.mipLevel);
                            gl.ReadPixels(src.origin.x, src.origin.y, copySize.width,
                                          copySize.height, glFormat, glType, offset);
                            break;
                        }
                        // Implementation for 2D array is the same as 3D.
                        [[fallthrough]];
                    }

                    case wgpu::TextureDimension::e3D: {
                        const uint64_t bytesPerImage = dst.bytesPerRow * dst.rowsPerImage;
                        for (uint32_t z = 0; z < copySize.depthOrArrayLayers; ++z) {
                            gl.FramebufferTextureLayer(GL_READ_FRAMEBUFFER, glAttachment,
                                                       texture->GetHandle(), src.mipLevel,
                                                       src.origin.z + z);
                            gl.ReadPixels(src.origin.x, src.origin.y, copySize.width,
                                          copySize.height, glFormat, glType, offset);

                            offset += bytesPerImage;
                        }
                        break;
                    }

                    case wgpu::TextureDimension::e1D:
                        UNREACHABLE();
                }

                gl.PixelStorei(GL_PACK_ROW_LENGTH, 0);

                gl.BindBuffer(GL_PIXEL_PACK_BUFFER, 0);
                gl.DeleteFramebuffers(1, &readFBO);
                break;
            }

            case Command::CopyTextureToTexture: {
                CopyTextureToTextureCmd* copy = mCommands.NextCommand<CopyTextureToTextureCmd>();
                if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                    copy->copySize.depthOrArrayLayers == 0) {
                    // Skip no-op copies.
                    continue;
                }
                auto& src = copy->source;
                auto& dst = copy->destination;

                // TODO(crbug.com/dawn/817): add workaround for the case that imageExtentSrc
                // is not equal to imageExtentDst. For example when copySize fits in the virtual
                // size of the source image but does not fit in the one of the destination
                // image.
                Extent3D copySize = ComputeTextureCopyExtent(dst, copy->copySize);
                Texture* srcTexture = ToBackend(src.texture.Get());
                Texture* dstTexture = ToBackend(dst.texture.Get());

                SubresourceRange srcRange = GetSubresourcesAffectedByCopy(src, copy->copySize);
                SubresourceRange dstRange = GetSubresourcesAffectedByCopy(dst, copy->copySize);

                srcTexture->EnsureSubresourceContentInitialized(srcRange);
                if (IsCompleteSubresourceCopiedTo(dstTexture, copySize, dst.mipLevel)) {
                    dstTexture->SetIsSubresourceContentInitialized(true, dstRange);
                } else {
                    dstTexture->EnsureSubresourceContentInitialized(dstRange);
                }
                CopyImageSubData(gl, src.aspect, srcTexture->GetHandle(), srcTexture->GetGLTarget(),
                                 src.mipLevel, src.origin, dstTexture->GetHandle(),
                                 dstTexture->GetGLTarget(), dst.mipLevel, dst.origin, copySize);
                ToBackend(dst.texture)->Touch();
                break;
            }

            case Command::ClearBuffer: {
                ClearBufferCmd* cmd = mCommands.NextCommand<ClearBufferCmd>();
                if (cmd->size == 0) {
                    // Skip no-op fills.
                    break;
                }
                Buffer* dstBuffer = ToBackend(cmd->buffer.Get());

                bool clearedToZero =
                    dstBuffer->EnsureDataInitializedAsDestination(cmd->offset, cmd->size);

                if (!clearedToZero) {
                    const std::vector<uint8_t> clearValues(cmd->size, 0u);
                    gl.BindBuffer(GL_ARRAY_BUFFER, dstBuffer->GetHandle());
                    gl.BufferSubData(GL_ARRAY_BUFFER, cmd->offset, cmd->size, clearValues.data());
                }

                break;
            }

            case Command::ResolveQuerySet: {
                // TODO(crbug.com/dawn/434): Resolve non-precise occlusion query.
                SkipCommand(&mCommands, type);
                break;
            }

            case Command::WriteTimestamp: {
                return DAWN_UNIMPLEMENTED_ERROR("WriteTimestamp unimplemented");
            }

            case Command::InsertDebugMarker:
            case Command::PopDebugGroup:
            case Command::PushDebugGroup: {
                // Due to lack of linux driver support for GL_EXT_debug_marker
                // extension these functions are skipped.
                SkipCommand(&mCommands, type);
                break;
            }

            case Command::WriteBuffer: {
                WriteBufferCmd* write = mCommands.NextCommand<WriteBufferCmd>();
                uint64_t offset = write->offset;
                uint64_t size = write->size;
                if (size == 0) {
                    continue;
                }

                Buffer* dstBuffer = ToBackend(write->buffer.Get());
                uint8_t* data = mCommands.NextData<uint8_t>(size);
                dstBuffer->EnsureDataInitializedAsDestination(offset, size);

                gl.BindBuffer(GL_ARRAY_BUFFER, dstBuffer->GetHandle());
                gl.BufferSubData(GL_ARRAY_BUFFER, offset, size, data);
                break;
            }

            default:
                UNREACHABLE();
        }
    }

    return {};
}

MaybeError CommandBuffer::ExecuteComputePass() {
    const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;
    ComputePipeline* lastPipeline = nullptr;
    BindGroupTracker bindGroupTracker = {};

    Command type;
    while (mCommands.NextCommandId(&type)) {
        switch (type) {
            case Command::EndComputePass: {
                mCommands.NextCommand<EndComputePassCmd>();
                return {};
            }

            case Command::Dispatch: {
                DispatchCmd* dispatch = mCommands.NextCommand<DispatchCmd>();
                bindGroupTracker.Apply(gl);

                gl.DispatchCompute(dispatch->x, dispatch->y, dispatch->z);
                gl.MemoryBarrier(GL_ALL_BARRIER_BITS);
                break;
            }

            case Command::DispatchIndirect: {
                DispatchIndirectCmd* dispatch = mCommands.NextCommand<DispatchIndirectCmd>();
                bindGroupTracker.Apply(gl);

                uint64_t indirectBufferOffset = dispatch->indirectOffset;
                Buffer* indirectBuffer = ToBackend(dispatch->indirectBuffer.Get());

                gl.BindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirectBuffer->GetHandle());
                gl.DispatchComputeIndirect(static_cast<GLintptr>(indirectBufferOffset));
                gl.MemoryBarrier(GL_ALL_BARRIER_BITS);
                break;
            }

            case Command::SetComputePipeline: {
                SetComputePipelineCmd* cmd = mCommands.NextCommand<SetComputePipelineCmd>();
                lastPipeline = ToBackend(cmd->pipeline).Get();
                lastPipeline->ApplyNow();

                bindGroupTracker.OnSetPipeline(lastPipeline);
                break;
            }

            case Command::SetBindGroup: {
                SetBindGroupCmd* cmd = mCommands.NextCommand<SetBindGroupCmd>();
                uint32_t* dynamicOffsets = nullptr;
                if (cmd->dynamicOffsetCount > 0) {
                    dynamicOffsets = mCommands.NextData<uint32_t>(cmd->dynamicOffsetCount);
                }
                bindGroupTracker.OnSetBindGroup(cmd->index, cmd->group.Get(),
                                                cmd->dynamicOffsetCount, dynamicOffsets);
                break;
            }

            case Command::InsertDebugMarker:
            case Command::PopDebugGroup:
            case Command::PushDebugGroup: {
                // Due to lack of linux driver support for GL_EXT_debug_marker
                // extension these functions are skipped.
                SkipCommand(&mCommands, type);
                break;
            }

            case Command::WriteTimestamp: {
                return DAWN_UNIMPLEMENTED_ERROR("WriteTimestamp unimplemented");
            }

            default:
                UNREACHABLE();
        }
    }

    // EndComputePass should have been called
    UNREACHABLE();
}

MaybeError CommandBuffer::ExecuteRenderPass(BeginRenderPassCmd* renderPass) {
    const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;
    GLuint fbo = 0;

    // Create the framebuffer used for this render pass and calls the correct glDrawBuffers
    {
        // TODO(kainino@chromium.org): This is added to possibly work around an issue seen on
        // Windows/Intel. It should break any feedback loop before the clears, even if there
        // shouldn't be any negative effects from this. Investigate whether it's actually
        // needed.
        gl.BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        // TODO(kainino@chromium.org): possible future optimization: create these framebuffers
        // at Framebuffer build time (or maybe CommandBuffer build time) so they don't have to
        // be created and destroyed at draw time.
        gl.GenFramebuffers(1, &fbo);
        gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

        // Mapping from attachmentSlot to GL framebuffer attachment points. Defaults to zero
        // (GL_NONE).
        ityp::array<ColorAttachmentIndex, GLenum, kMaxColorAttachments> drawBuffers = {};

        // Construct GL framebuffer

        ColorAttachmentIndex attachmentCount(uint8_t(0));
        for (ColorAttachmentIndex i :
             IterateBitSet(renderPass->attachmentState->GetColorAttachmentsMask())) {
            TextureView* textureView = ToBackend(renderPass->colorAttachments[i].view.Get());
            GLenum glAttachment = GL_COLOR_ATTACHMENT0 + static_cast<uint8_t>(i);

            // Attach color buffers.
            textureView->BindToFramebuffer(GL_DRAW_FRAMEBUFFER, glAttachment);
            drawBuffers[i] = glAttachment;
            attachmentCount = i;
            attachmentCount++;
        }
        gl.DrawBuffers(static_cast<uint8_t>(attachmentCount), drawBuffers.data());

        if (renderPass->attachmentState->HasDepthStencilAttachment()) {
            TextureView* textureView = ToBackend(renderPass->depthStencilAttachment.view.Get());
            const Format& format = textureView->GetTexture()->GetFormat();

            // Attach depth/stencil buffer.
            GLenum glAttachment = 0;
            if (format.aspects == (Aspect::Depth | Aspect::Stencil)) {
                glAttachment = GL_DEPTH_STENCIL_ATTACHMENT;
            } else if (format.aspects == Aspect::Depth) {
                glAttachment = GL_DEPTH_ATTACHMENT;
            } else if (format.aspects == Aspect::Stencil) {
                glAttachment = GL_STENCIL_ATTACHMENT;
            } else {
                UNREACHABLE();
            }

            textureView->BindToFramebuffer(GL_DRAW_FRAMEBUFFER, glAttachment);
        }
    }

    ASSERT(gl.CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    // Set defaults for dynamic state before executing clears and commands.
    PersistentPipelineState persistentPipelineState;
    persistentPipelineState.SetDefaultState(gl);
    gl.BlendColor(0, 0, 0, 0);
    gl.Viewport(0, 0, renderPass->width, renderPass->height);
    gl.DepthRangef(0.0, 1.0);
    gl.Scissor(0, 0, renderPass->width, renderPass->height);

    // Clear framebuffer attachments as needed
    {
        for (ColorAttachmentIndex index :
             IterateBitSet(renderPass->attachmentState->GetColorAttachmentsMask())) {
            uint8_t i = static_cast<uint8_t>(index);
            auto* attachmentInfo = &renderPass->colorAttachments[index];

            // Load op - color
            if (attachmentInfo->loadOp == wgpu::LoadOp::Clear) {
                gl.ColorMask(true, true, true, true);

                wgpu::TextureComponentType baseType =
                    attachmentInfo->view->GetFormat().GetAspectInfo(Aspect::Color).baseType;
                switch (baseType) {
                    case wgpu::TextureComponentType::Float: {
                        const std::array<float, 4> appliedClearColor =
                            ConvertToFloatColor(attachmentInfo->clearColor);
                        gl.ClearBufferfv(GL_COLOR, i, appliedClearColor.data());
                        break;
                    }
                    case wgpu::TextureComponentType::Uint: {
                        const std::array<uint32_t, 4> appliedClearColor =
                            ConvertToUnsignedIntegerColor(attachmentInfo->clearColor);
                        gl.ClearBufferuiv(GL_COLOR, i, appliedClearColor.data());
                        break;
                    }
                    case wgpu::TextureComponentType::Sint: {
                        const std::array<int32_t, 4> appliedClearColor =
                            ConvertToSignedIntegerColor(attachmentInfo->clearColor);
                        gl.ClearBufferiv(GL_COLOR, i, appliedClearColor.data());
                        break;
                    }

                    case wgpu::TextureComponentType::DepthComparison:
                        UNREACHABLE();
                }
            }

            if (attachmentInfo->storeOp == wgpu::StoreOp::Discard) {
                // TODO(natlee@microsoft.com): call glDiscard to do optimization
            }
        }

        if (renderPass->attachmentState->HasDepthStencilAttachment()) {
            auto* attachmentInfo = &renderPass->depthStencilAttachment;
            const Format& attachmentFormat = attachmentInfo->view->GetTexture()->GetFormat();

            // Load op - depth/stencil
            bool doDepthClear =
                attachmentFormat.HasDepth() && (attachmentInfo->depthLoadOp == wgpu::LoadOp::Clear);
            bool doStencilClear = attachmentFormat.HasStencil() &&
                                  (attachmentInfo->stencilLoadOp == wgpu::LoadOp::Clear);

            if (doDepthClear) {
                gl.DepthMask(GL_TRUE);
            }
            if (doStencilClear) {
                gl.StencilMask(GetStencilMaskFromStencilFormat(attachmentFormat.format));
            }

            if (doDepthClear && doStencilClear) {
                gl.ClearBufferfi(GL_DEPTH_STENCIL, 0, attachmentInfo->clearDepth,
                                 attachmentInfo->clearStencil);
            } else if (doDepthClear) {
                gl.ClearBufferfv(GL_DEPTH, 0, &attachmentInfo->clearDepth);
            } else if (doStencilClear) {
                const GLint clearStencil = attachmentInfo->clearStencil;
                gl.ClearBufferiv(GL_STENCIL, 0, &clearStencil);
            }
        }
    }

    RenderPipeline* lastPipeline = nullptr;
    uint64_t indexBufferBaseOffset = 0;
    GLenum indexBufferFormat;
    uint32_t indexFormatSize;

    VertexStateBufferBindingTracker vertexStateBufferBindingTracker;
    BindGroupTracker bindGroupTracker = {};

    auto DoRenderBundleCommand = [&](CommandIterator* iter, Command type) {
        switch (type) {
            case Command::Draw: {
                DrawCmd* draw = iter->NextCommand<DrawCmd>();
                vertexStateBufferBindingTracker.Apply(gl);
                bindGroupTracker.Apply(gl);

                if (draw->firstInstance > 0) {
                    gl.DrawArraysInstancedBaseInstance(lastPipeline->GetGLPrimitiveTopology(),
                                                       draw->firstVertex, draw->vertexCount,
                                                       draw->instanceCount, draw->firstInstance);
                } else {
                    // This branch is only needed on OpenGL < 4.2
                    gl.DrawArraysInstanced(lastPipeline->GetGLPrimitiveTopology(),
                                           draw->firstVertex, draw->vertexCount,
                                           draw->instanceCount);
                }
                break;
            }

            case Command::DrawIndexed: {
                DrawIndexedCmd* draw = iter->NextCommand<DrawIndexedCmd>();
                vertexStateBufferBindingTracker.Apply(gl);
                bindGroupTracker.Apply(gl);

                if (draw->firstInstance > 0) {
                    gl.DrawElementsInstancedBaseVertexBaseInstance(
                        lastPipeline->GetGLPrimitiveTopology(), draw->indexCount, indexBufferFormat,
                        reinterpret_cast<void*>(draw->firstIndex * indexFormatSize +
                                                indexBufferBaseOffset),
                        draw->instanceCount, draw->baseVertex, draw->firstInstance);
                } else {
                    // This branch is only needed on OpenGL < 4.2; ES < 3.2
                    if (draw->baseVertex != 0) {
                        gl.DrawElementsInstancedBaseVertex(
                            lastPipeline->GetGLPrimitiveTopology(), draw->indexCount,
                            indexBufferFormat,
                            reinterpret_cast<void*>(draw->firstIndex * indexFormatSize +
                                                    indexBufferBaseOffset),
                            draw->instanceCount, draw->baseVertex);
                    } else {
                        // This branch is only needed on OpenGL < 3.2; ES < 3.2
                        gl.DrawElementsInstanced(
                            lastPipeline->GetGLPrimitiveTopology(), draw->indexCount,
                            indexBufferFormat,
                            reinterpret_cast<void*>(draw->firstIndex * indexFormatSize +
                                                    indexBufferBaseOffset),
                            draw->instanceCount);
                    }
                }
                break;
            }

            case Command::DrawIndirect: {
                DrawIndirectCmd* draw = iter->NextCommand<DrawIndirectCmd>();
                vertexStateBufferBindingTracker.Apply(gl);
                bindGroupTracker.Apply(gl);

                uint64_t indirectBufferOffset = draw->indirectOffset;
                Buffer* indirectBuffer = ToBackend(draw->indirectBuffer.Get());

                gl.BindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer->GetHandle());
                gl.DrawArraysIndirect(
                    lastPipeline->GetGLPrimitiveTopology(),
                    reinterpret_cast<void*>(static_cast<intptr_t>(indirectBufferOffset)));
                break;
            }

            case Command::DrawIndexedIndirect: {
                DrawIndexedIndirectCmd* draw = iter->NextCommand<DrawIndexedIndirectCmd>();

                vertexStateBufferBindingTracker.Apply(gl);
                bindGroupTracker.Apply(gl);

                Buffer* indirectBuffer = ToBackend(draw->indirectBuffer.Get());
                ASSERT(indirectBuffer != nullptr);

                gl.BindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer->GetHandle());
                gl.DrawElementsIndirect(
                    lastPipeline->GetGLPrimitiveTopology(), indexBufferFormat,
                    reinterpret_cast<void*>(static_cast<intptr_t>(draw->indirectOffset)));
                break;
            }

            case Command::InsertDebugMarker:
            case Command::PopDebugGroup:
            case Command::PushDebugGroup: {
                // Due to lack of linux driver support for GL_EXT_debug_marker
                // extension these functions are skipped.
                SkipCommand(iter, type);
                break;
            }

            case Command::SetRenderPipeline: {
                SetRenderPipelineCmd* cmd = iter->NextCommand<SetRenderPipelineCmd>();
                lastPipeline = ToBackend(cmd->pipeline).Get();
                lastPipeline->ApplyNow(persistentPipelineState);

                vertexStateBufferBindingTracker.OnSetPipeline(lastPipeline);
                bindGroupTracker.OnSetPipeline(lastPipeline);
                break;
            }

            case Command::SetBindGroup: {
                SetBindGroupCmd* cmd = iter->NextCommand<SetBindGroupCmd>();
                uint32_t* dynamicOffsets = nullptr;
                if (cmd->dynamicOffsetCount > 0) {
                    dynamicOffsets = iter->NextData<uint32_t>(cmd->dynamicOffsetCount);
                }
                bindGroupTracker.OnSetBindGroup(cmd->index, cmd->group.Get(),
                                                cmd->dynamicOffsetCount, dynamicOffsets);
                break;
            }

            case Command::SetIndexBuffer: {
                SetIndexBufferCmd* cmd = iter->NextCommand<SetIndexBufferCmd>();

                indexBufferBaseOffset = cmd->offset;
                indexBufferFormat = IndexFormatType(cmd->format);
                indexFormatSize = IndexFormatSize(cmd->format);
                vertexStateBufferBindingTracker.OnSetIndexBuffer(cmd->buffer.Get());
                break;
            }

            case Command::SetVertexBuffer: {
                SetVertexBufferCmd* cmd = iter->NextCommand<SetVertexBufferCmd>();
                vertexStateBufferBindingTracker.OnSetVertexBuffer(cmd->slot, cmd->buffer.Get(),
                                                                  cmd->offset);
                break;
            }

            default:
                UNREACHABLE();
                break;
        }
    };

    Command type;
    while (mCommands.NextCommandId(&type)) {
        switch (type) {
            case Command::EndRenderPass: {
                mCommands.NextCommand<EndRenderPassCmd>();

                for (ColorAttachmentIndex i :
                     IterateBitSet(renderPass->attachmentState->GetColorAttachmentsMask())) {
                    TextureView* textureView =
                        ToBackend(renderPass->colorAttachments[i].view.Get());
                    ToBackend(textureView->GetTexture())->Touch();
                }
                if (renderPass->attachmentState->HasDepthStencilAttachment()) {
                    TextureView* textureView =
                        ToBackend(renderPass->depthStencilAttachment.view.Get());
                    ToBackend(textureView->GetTexture())->Touch();
                }
                if (renderPass->attachmentState->GetSampleCount() > 1) {
                    ResolveMultisampledRenderTargets(gl, renderPass);
                }
                gl.DeleteFramebuffers(1, &fbo);
                return {};
            }

            case Command::SetStencilReference: {
                SetStencilReferenceCmd* cmd = mCommands.NextCommand<SetStencilReferenceCmd>();
                persistentPipelineState.SetStencilReference(gl, cmd->reference);
                break;
            }

            case Command::SetViewport: {
                SetViewportCmd* cmd = mCommands.NextCommand<SetViewportCmd>();
                if (gl.IsAtLeastGL(4, 1)) {
                    gl.ViewportIndexedf(0, cmd->x, cmd->y, cmd->width, cmd->height);
                } else {
                    // Floating-point viewport coords are unsupported on OpenGL ES, but
                    // truncation is ok because other APIs do not guarantee subpixel precision
                    // either.
                    gl.Viewport(static_cast<int>(cmd->x), static_cast<int>(cmd->y),
                                static_cast<int>(cmd->width), static_cast<int>(cmd->height));
                }
                gl.DepthRangef(cmd->minDepth, cmd->maxDepth);
                break;
            }

            case Command::SetScissorRect: {
                SetScissorRectCmd* cmd = mCommands.NextCommand<SetScissorRectCmd>();
                gl.Scissor(cmd->x, cmd->y, cmd->width, cmd->height);
                break;
            }

            case Command::SetBlendConstant: {
                SetBlendConstantCmd* cmd = mCommands.NextCommand<SetBlendConstantCmd>();
                const std::array<float, 4> blendColor = ConvertToFloatColor(cmd->color);
                gl.BlendColor(blendColor[0], blendColor[1], blendColor[2], blendColor[3]);
                break;
            }

            case Command::ExecuteBundles: {
                ExecuteBundlesCmd* cmd = mCommands.NextCommand<ExecuteBundlesCmd>();
                auto bundles = mCommands.NextData<Ref<RenderBundleBase>>(cmd->count);

                for (uint32_t i = 0; i < cmd->count; ++i) {
                    CommandIterator* iter = bundles[i]->GetCommands();
                    iter->Reset();
                    while (iter->NextCommandId(&type)) {
                        DoRenderBundleCommand(iter, type);
                    }
                }
                break;
            }

            case Command::BeginOcclusionQuery: {
                return DAWN_UNIMPLEMENTED_ERROR("BeginOcclusionQuery unimplemented.");
            }

            case Command::EndOcclusionQuery: {
                return DAWN_UNIMPLEMENTED_ERROR("EndOcclusionQuery unimplemented.");
            }

            case Command::WriteTimestamp:
                return DAWN_UNIMPLEMENTED_ERROR("WriteTimestamp unimplemented");

            default: {
                DoRenderBundleCommand(&mCommands, type);
                break;
            }
        }
    }

    // EndRenderPass should have been called
    UNREACHABLE();
}

void DoTexSubImage(const OpenGLFunctions& gl,
                   const TextureCopy& destination,
                   const void* data,
                   const TextureDataLayout& dataLayout,
                   const Extent3D& copySize) {
    Texture* texture = ToBackend(destination.texture.Get());
    ASSERT(texture->GetDimension() != wgpu::TextureDimension::e1D);

    const GLFormat& format = texture->GetGLFormat();
    GLenum target = texture->GetGLTarget();
    data = static_cast<const uint8_t*>(data) + dataLayout.offset;
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(target, texture->GetHandle());
    const TexelBlockInfo& blockInfo = texture->GetFormat().GetAspectInfo(destination.aspect).block;

    uint32_t x = destination.origin.x;
    uint32_t y = destination.origin.y;
    uint32_t z = destination.origin.z;
    if (texture->GetFormat().isCompressed) {
        size_t rowSize = copySize.width / blockInfo.width * blockInfo.byteSize;
        Extent3D virtSize = texture->GetMipLevelSingleSubresourceVirtualSize(destination.mipLevel);
        uint32_t width = std::min(copySize.width, virtSize.width - x);

        // In GLES glPixelStorei() doesn't affect CompressedTexSubImage*D() and
        // GL_UNPACK_COMPRESSED_BLOCK_* isn't defined, so we have to workaround
        // this limitation by copying the compressed texture data once per row.
        // See OpenGL ES 3.2 SPEC Chapter 8.4.1, "Pixel Storage Modes and Pixel
        // Buffer Objects" for more details. For Desktop GL, we use row-by-row
        // copies only for uploads where bytesPerRow is not a multiple of byteSize.
        if (dataLayout.bytesPerRow % blockInfo.byteSize == 0 && gl.GetVersion().IsDesktop()) {
            size_t imageSize =
                rowSize * (copySize.height / blockInfo.height) * copySize.depthOrArrayLayers;

            uint32_t height = std::min(copySize.height, virtSize.height - y);

            gl.PixelStorei(GL_UNPACK_ROW_LENGTH,
                           dataLayout.bytesPerRow / blockInfo.byteSize * blockInfo.width);
            gl.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, blockInfo.byteSize);
            gl.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, blockInfo.width);
            gl.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, blockInfo.height);
            gl.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH, 1);

            if (texture->GetArrayLayers() == 1 &&
                texture->GetDimension() == wgpu::TextureDimension::e2D) {
                gl.CompressedTexSubImage2D(target, destination.mipLevel, x, y, width, height,
                                           format.internalFormat, imageSize, data);
            } else {
                gl.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, dataLayout.rowsPerImage * blockInfo.height);
                gl.CompressedTexSubImage3D(target, destination.mipLevel, x, y, z, width, height,
                                           copySize.depthOrArrayLayers, format.internalFormat,
                                           imageSize, data);
                gl.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
            }

            gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            gl.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, 0);
            gl.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 0);
            gl.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 0);
            gl.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH, 0);
        } else {
            if (texture->GetArrayLayers() == 1 &&
                texture->GetDimension() == wgpu::TextureDimension::e2D) {
                const uint8_t* d = static_cast<const uint8_t*>(data);

                for (; y < destination.origin.y + copySize.height; y += blockInfo.height) {
                    uint32_t height = std::min(blockInfo.height, virtSize.height - y);
                    gl.CompressedTexSubImage2D(target, destination.mipLevel, x, y, width, height,
                                               format.internalFormat, rowSize, d);
                    d += dataLayout.bytesPerRow;
                }
            } else {
                const uint8_t* slice = static_cast<const uint8_t*>(data);

                for (; z < destination.origin.z + copySize.depthOrArrayLayers; ++z) {
                    const uint8_t* d = slice;

                    for (y = destination.origin.y; y < destination.origin.y + copySize.height;
                         y += blockInfo.height) {
                        uint32_t height = std::min(blockInfo.height, virtSize.height - y);
                        gl.CompressedTexSubImage3D(target, destination.mipLevel, x, y, z, width,
                                                   height, 1, format.internalFormat, rowSize, d);
                        d += dataLayout.bytesPerRow;
                    }

                    slice += dataLayout.rowsPerImage * dataLayout.bytesPerRow;
                }
            }
        }
    } else {
        uint32_t width = copySize.width;
        uint32_t height = copySize.height;
        if (dataLayout.bytesPerRow % blockInfo.byteSize == 0) {
            gl.PixelStorei(GL_UNPACK_ROW_LENGTH,
                           dataLayout.bytesPerRow / blockInfo.byteSize * blockInfo.width);
            if (texture->GetArrayLayers() == 1 &&
                texture->GetDimension() == wgpu::TextureDimension::e2D) {
                gl.TexSubImage2D(target, destination.mipLevel, x, y, width, height, format.format,
                                 format.type, data);
            } else {
                gl.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, dataLayout.rowsPerImage * blockInfo.height);
                gl.TexSubImage3D(target, destination.mipLevel, x, y, z, width, height,
                                 copySize.depthOrArrayLayers, format.format, format.type, data);
                gl.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
            }
            gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        } else {
            if (texture->GetArrayLayers() == 1 &&
                texture->GetDimension() == wgpu::TextureDimension::e2D) {
                const uint8_t* d = static_cast<const uint8_t*>(data);
                for (; y < destination.origin.y + height; ++y) {
                    gl.TexSubImage2D(target, destination.mipLevel, x, y, width, 1, format.format,
                                     format.type, d);
                    d += dataLayout.bytesPerRow;
                }
            } else {
                const uint8_t* slice = static_cast<const uint8_t*>(data);
                for (; z < destination.origin.z + copySize.depthOrArrayLayers; ++z) {
                    const uint8_t* d = slice;
                    for (y = destination.origin.y; y < destination.origin.y + height; ++y) {
                        gl.TexSubImage3D(target, destination.mipLevel, x, y, z, width, 1, 1,
                                         format.format, format.type, d);
                        d += dataLayout.bytesPerRow;
                    }
                    slice += dataLayout.rowsPerImage * dataLayout.bytesPerRow;
                }
            }
        }
    }
}

}  // namespace dawn::native::opengl
