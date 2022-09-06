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

#include "dawn/native/opengl/TextureGL.h"

#include <limits>

#include "dawn/common/Assert.h"
#include "dawn/common/Constants.h"
#include "dawn/common/Math.h"
#include "dawn/native/EnumMaskIterator.h"
#include "dawn/native/opengl/BufferGL.h"
#include "dawn/native/opengl/CommandBufferGL.h"
#include "dawn/native/opengl/DeviceGL.h"
#include "dawn/native/opengl/UtilsGL.h"

namespace dawn::native::opengl {

namespace {

GLenum TargetForTexture(const TextureDescriptor* descriptor) {
    switch (descriptor->dimension) {
        case wgpu::TextureDimension::e2D:
            if (descriptor->size.depthOrArrayLayers > 1) {
                ASSERT(descriptor->sampleCount == 1);
                return GL_TEXTURE_2D_ARRAY;
            } else {
                if (descriptor->sampleCount > 1) {
                    return GL_TEXTURE_2D_MULTISAMPLE;
                } else {
                    return GL_TEXTURE_2D;
                }
            }
        case wgpu::TextureDimension::e3D:
            ASSERT(descriptor->sampleCount == 1);
            return GL_TEXTURE_3D;

        case wgpu::TextureDimension::e1D:
            break;
    }
    UNREACHABLE();
}

GLenum TargetForTextureViewDimension(wgpu::TextureViewDimension dimension,
                                     uint32_t arrayLayerCount,
                                     uint32_t sampleCount) {
    switch (dimension) {
        case wgpu::TextureViewDimension::e2D:
            return (sampleCount > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        case wgpu::TextureViewDimension::e2DArray:
            if (sampleCount > 1) {
                ASSERT(arrayLayerCount == 1);
                return GL_TEXTURE_2D_MULTISAMPLE;
            }
            ASSERT(sampleCount == 1);
            return GL_TEXTURE_2D_ARRAY;
        case wgpu::TextureViewDimension::Cube:
            ASSERT(sampleCount == 1);
            ASSERT(arrayLayerCount == 6);
            return GL_TEXTURE_CUBE_MAP;
        case wgpu::TextureViewDimension::CubeArray:
            ASSERT(sampleCount == 1);
            ASSERT(arrayLayerCount % 6 == 0);
            return GL_TEXTURE_CUBE_MAP_ARRAY;
        case wgpu::TextureViewDimension::e3D:
            return GL_TEXTURE_3D;

        case wgpu::TextureViewDimension::e1D:
        case wgpu::TextureViewDimension::Undefined:
            break;
    }
    UNREACHABLE();
}

GLuint GenTexture(const OpenGLFunctions& gl) {
    GLuint handle = 0;
    gl.GenTextures(1, &handle);
    return handle;
}

bool RequiresCreatingNewTextureView(const TextureBase* texture,
                                    const TextureViewDescriptor* textureViewDescriptor) {
    constexpr wgpu::TextureUsage kShaderUsageNeedsView =
        wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding;
    constexpr wgpu::TextureUsage kUsageNeedsView =
        kShaderUsageNeedsView | wgpu::TextureUsage::RenderAttachment;
    if ((texture->GetInternalUsage() & kUsageNeedsView) == 0) {
        return false;
    }

    if (texture->GetFormat().format != textureViewDescriptor->format &&
        !texture->GetFormat().HasDepthOrStencil()) {
        // Color format reinterpretation required. Note: Depth/stencil formats don't support
        // reinterpretation.
        return true;
    }

    // Reinterpretation not required. Now, we only need a new view if the view dimension or
    // set of subresources for the shader is different from the base texture.
    if ((texture->GetInternalUsage() & kShaderUsageNeedsView) == 0) {
        return false;
    }

    if (texture->GetArrayLayers() != textureViewDescriptor->arrayLayerCount ||
        (texture->GetArrayLayers() == 1 && texture->GetDimension() == wgpu::TextureDimension::e2D &&
         textureViewDescriptor->dimension == wgpu::TextureViewDimension::e2DArray)) {
        // If the view has a different number of array layers, we need a new view.
        // And, if the original texture is a 2D texture with one array layer, we need a new
        // view to view it as a 2D array texture.
        return true;
    }

    if (texture->GetNumMipLevels() != textureViewDescriptor->mipLevelCount) {
        return true;
    }

    if (ToBackend(texture)->GetGLFormat().format == GL_DEPTH_STENCIL &&
        (texture->GetUsage() & wgpu::TextureUsage::TextureBinding) != 0 &&
        textureViewDescriptor->aspect == wgpu::TextureAspect::StencilOnly) {
        // We need a separate view for one of the depth or stencil planes
        // because each glTextureView needs it's own handle to set
        // GL_DEPTH_STENCIL_TEXTURE_MODE. Choose the stencil aspect for the
        // extra handle since it is likely sampled less often.
        return true;
    }

    switch (textureViewDescriptor->dimension) {
        case wgpu::TextureViewDimension::Cube:
        case wgpu::TextureViewDimension::CubeArray:
            return true;
        default:
            break;
    }

    return false;
}

void AllocateTexture(const OpenGLFunctions& gl,
                     GLenum target,
                     GLsizei samples,
                     GLuint levels,
                     GLenum internalFormat,
                     const Extent3D& size) {
    // glTextureView() requires the value of GL_TEXTURE_IMMUTABLE_FORMAT for origtexture to
    // be GL_TRUE, so the storage of the texture must be allocated with glTexStorage*D.
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTextureView.xhtml
    switch (target) {
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_3D:
            gl.TexStorage3D(target, levels, internalFormat, size.width, size.height,
                            size.depthOrArrayLayers);
            break;
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP:
            gl.TexStorage2D(target, levels, internalFormat, size.width, size.height);
            break;
        case GL_TEXTURE_2D_MULTISAMPLE:
            gl.TexStorage2DMultisample(target, samples, internalFormat, size.width, size.height,
                                       true);
            break;
        default:
            UNREACHABLE();
    }
}

}  // namespace

// Texture

Texture::Texture(Device* device, const TextureDescriptor* descriptor)
    : Texture(device, descriptor, GenTexture(device->gl), TextureState::OwnedInternal) {
    const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;

    uint32_t levels = GetNumMipLevels();

    const GLFormat& glFormat = GetGLFormat();

    gl.BindTexture(mTarget, mHandle);

    AllocateTexture(gl, mTarget, GetSampleCount(), levels, glFormat.internalFormat, GetSize());

    // The texture is not complete if it uses mipmapping and not all levels up to
    // MAX_LEVEL have been defined.
    gl.TexParameteri(mTarget, GL_TEXTURE_MAX_LEVEL, levels - 1);

    if (GetDevice()->IsToggleEnabled(Toggle::NonzeroClearResourcesOnCreationForTesting)) {
        GetDevice()->ConsumedError(
            ClearTexture(GetAllSubresources(), TextureBase::ClearValue::NonZero));
    }
}

void Texture::Touch() {
    mGenID++;
}

uint32_t Texture::GetGenID() const {
    return mGenID;
}

Texture::Texture(Device* device,
                 const TextureDescriptor* descriptor,
                 GLuint handle,
                 TextureState state)
    : TextureBase(device, descriptor, state), mHandle(handle) {
    mTarget = TargetForTexture(descriptor);
}

Texture::~Texture() {}

void Texture::DestroyImpl() {
    TextureBase::DestroyImpl();
    if (GetTextureState() == TextureState::OwnedInternal) {
        ToBackend(GetDevice())->gl.DeleteTextures(1, &mHandle);
        mHandle = 0;
    }
}

GLuint Texture::GetHandle() const {
    return mHandle;
}

GLenum Texture::GetGLTarget() const {
    return mTarget;
}

const GLFormat& Texture::GetGLFormat() const {
    return ToBackend(GetDevice())->GetGLFormat(GetFormat());
}

MaybeError Texture::ClearTexture(const SubresourceRange& range,
                                 TextureBase::ClearValue clearValue) {
    // TODO(crbug.com/dawn/850): initialize the textures with compressed formats.
    if (GetFormat().isCompressed) {
        return {};
    }

    Device* device = ToBackend(GetDevice());
    const OpenGLFunctions& gl = device->gl;

    uint8_t clearColor = (clearValue == TextureBase::ClearValue::Zero) ? 0 : 1;
    float fClearColor = (clearValue == TextureBase::ClearValue::Zero) ? 0.f : 1.f;

    if (GetFormat().isRenderable) {
        if ((range.aspects & (Aspect::Depth | Aspect::Stencil)) != 0) {
            GLfloat depth = fClearColor;
            GLint stencil = clearColor;
            if (range.aspects & Aspect::Depth) {
                gl.DepthMask(GL_TRUE);
            }
            if (range.aspects & Aspect::Stencil) {
                gl.StencilMask(GetStencilMaskFromStencilFormat(GetFormat().format));
            }

            auto DoClear = [&](Aspect aspects) {
                if (aspects == (Aspect::Depth | Aspect::Stencil)) {
                    gl.ClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
                } else if (aspects == Aspect::Depth) {
                    gl.ClearBufferfv(GL_DEPTH, 0, &depth);
                } else if (aspects == Aspect::Stencil) {
                    gl.ClearBufferiv(GL_STENCIL, 0, &stencil);
                } else {
                    UNREACHABLE();
                }
            };

            GLuint framebuffer = 0;
            gl.GenFramebuffers(1, &framebuffer);
            gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
            gl.Disable(GL_SCISSOR_TEST);

            GLenum attachment;
            if (range.aspects == (Aspect::Depth | Aspect::Stencil)) {
                attachment = GL_DEPTH_STENCIL_ATTACHMENT;
            } else if (range.aspects == Aspect::Depth) {
                attachment = GL_DEPTH_ATTACHMENT;
            } else if (range.aspects == Aspect::Stencil) {
                attachment = GL_STENCIL_ATTACHMENT;
            } else {
                UNREACHABLE();
            }

            for (uint32_t level = range.baseMipLevel; level < range.baseMipLevel + range.levelCount;
                 ++level) {
                switch (GetDimension()) {
                    case wgpu::TextureDimension::e2D:
                        if (GetArrayLayers() == 1) {
                            Aspect aspectsToClear = Aspect::None;
                            for (Aspect aspect : IterateEnumMask(range.aspects)) {
                                if (clearValue == TextureBase::ClearValue::Zero &&
                                    IsSubresourceContentInitialized(
                                        SubresourceRange::SingleMipAndLayer(level, 0, aspect))) {
                                    // Skip lazy clears if already initialized.
                                    continue;
                                }
                                aspectsToClear |= aspect;
                            }

                            if (aspectsToClear == Aspect::None) {
                                continue;
                            }

                            gl.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment, GetGLTarget(),
                                                    GetHandle(), static_cast<GLint>(level));
                            DoClear(aspectsToClear);
                        } else {
                            for (uint32_t layer = range.baseArrayLayer;
                                 layer < range.baseArrayLayer + range.layerCount; ++layer) {
                                Aspect aspectsToClear = Aspect::None;
                                for (Aspect aspect : IterateEnumMask(range.aspects)) {
                                    if (clearValue == TextureBase::ClearValue::Zero &&
                                        IsSubresourceContentInitialized(
                                            SubresourceRange::SingleMipAndLayer(level, layer,
                                                                                aspect))) {
                                        // Skip lazy clears if already initialized.
                                        continue;
                                    }
                                    aspectsToClear |= aspect;
                                }

                                if (aspectsToClear == Aspect::None) {
                                    continue;
                                }

                                gl.FramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment,
                                                           GetHandle(), static_cast<GLint>(level),
                                                           static_cast<GLint>(layer));
                                DoClear(aspectsToClear);
                            }
                        }
                        break;

                    case wgpu::TextureDimension::e1D:
                    case wgpu::TextureDimension::e3D:
                        UNREACHABLE();
                }
            }

            gl.Enable(GL_SCISSOR_TEST);
            gl.DeleteFramebuffers(1, &framebuffer);
        } else {
            ASSERT(range.aspects == Aspect::Color);

            // For gl.ClearBufferiv/uiv calls
            constexpr std::array<GLuint, 4> kClearColorDataUint0 = {0u, 0u, 0u, 0u};
            constexpr std::array<GLuint, 4> kClearColorDataUint1 = {1u, 1u, 1u, 1u};
            std::array<GLuint, 4> clearColorData;
            clearColorData.fill((clearValue == TextureBase::ClearValue::Zero) ? 0u : 1u);

            // For gl.ClearBufferfv calls
            constexpr std::array<GLfloat, 4> kClearColorDataFloat0 = {0.f, 0.f, 0.f, 0.f};
            constexpr std::array<GLfloat, 4> kClearColorDataFloat1 = {1.f, 1.f, 1.f, 1.f};
            std::array<GLfloat, 4> fClearColorData;
            fClearColorData.fill((clearValue == TextureBase::ClearValue::Zero) ? 0.f : 1.f);

            static constexpr uint32_t MAX_TEXEL_SIZE = 16;
            const TexelBlockInfo& blockInfo = GetFormat().GetAspectInfo(Aspect::Color).block;
            ASSERT(blockInfo.byteSize <= MAX_TEXEL_SIZE);

            // For gl.ClearTexSubImage calls
            constexpr std::array<GLbyte, MAX_TEXEL_SIZE> kClearColorDataBytes0 = {
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            constexpr std::array<GLbyte, MAX_TEXEL_SIZE> kClearColorDataBytes255 = {
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

            wgpu::TextureComponentType baseType = GetFormat().GetAspectInfo(Aspect::Color).baseType;

            const GLFormat& glFormat = GetGLFormat();
            for (uint32_t level = range.baseMipLevel; level < range.baseMipLevel + range.levelCount;
                 ++level) {
                Extent3D mipSize = GetMipLevelSingleSubresourcePhysicalSize(level);
                for (uint32_t layer = range.baseArrayLayer;
                     layer < range.baseArrayLayer + range.layerCount; ++layer) {
                    if (clearValue == TextureBase::ClearValue::Zero &&
                        IsSubresourceContentInitialized(
                            SubresourceRange::SingleMipAndLayer(level, layer, Aspect::Color))) {
                        // Skip lazy clears if already initialized.
                        continue;
                    }
                    if (gl.IsAtLeastGL(4, 4)) {
                        gl.ClearTexSubImage(mHandle, static_cast<GLint>(level), 0, 0,
                                            static_cast<GLint>(layer), mipSize.width,
                                            mipSize.height, mipSize.depthOrArrayLayers,
                                            glFormat.format, glFormat.type,
                                            clearValue == TextureBase::ClearValue::Zero
                                                ? kClearColorDataBytes0.data()
                                                : kClearColorDataBytes255.data());
                        continue;
                    }

                    GLuint framebuffer = 0;
                    gl.GenFramebuffers(1, &framebuffer);
                    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);

                    GLenum attachment = GL_COLOR_ATTACHMENT0;
                    gl.DrawBuffers(1, &attachment);

                    gl.Disable(GL_SCISSOR_TEST);
                    gl.ColorMask(true, true, true, true);

                    auto DoClear = [&]() {
                        switch (baseType) {
                            case wgpu::TextureComponentType::Float: {
                                gl.ClearBufferfv(GL_COLOR, 0,
                                                 clearValue == TextureBase::ClearValue::Zero
                                                     ? kClearColorDataFloat0.data()
                                                     : kClearColorDataFloat1.data());
                                break;
                            }
                            case wgpu::TextureComponentType::Uint: {
                                gl.ClearBufferuiv(GL_COLOR, 0,
                                                  clearValue == TextureBase::ClearValue::Zero
                                                      ? kClearColorDataUint0.data()
                                                      : kClearColorDataUint1.data());
                                break;
                            }
                            case wgpu::TextureComponentType::Sint: {
                                gl.ClearBufferiv(GL_COLOR, 0,
                                                 reinterpret_cast<const GLint*>(
                                                     clearValue == TextureBase::ClearValue::Zero
                                                         ? kClearColorDataUint0.data()
                                                         : kClearColorDataUint1.data()));
                                break;
                            }

                            case wgpu::TextureComponentType::DepthComparison:
                                UNREACHABLE();
                        }
                    };

                    if (GetArrayLayers() == 1) {
                        switch (GetDimension()) {
                            case wgpu::TextureDimension::e1D:
                                UNREACHABLE();
                            case wgpu::TextureDimension::e2D:
                                gl.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, attachment,
                                                        GetGLTarget(), GetHandle(), level);
                                DoClear();
                                break;
                            case wgpu::TextureDimension::e3D:
                                uint32_t depth = GetMipLevelSingleSubresourceVirtualSize(level)
                                                     .depthOrArrayLayers;
                                for (GLint z = 0; z < static_cast<GLint>(depth); ++z) {
                                    gl.FramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment,
                                                               GetHandle(), level, z);
                                    DoClear();
                                }
                                break;
                        }

                    } else {
                        ASSERT(GetDimension() == wgpu::TextureDimension::e2D);
                        gl.FramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, attachment, GetHandle(),
                                                   level, layer);
                        DoClear();
                    }

                    gl.Enable(GL_SCISSOR_TEST);
                    gl.DeleteFramebuffers(1, &framebuffer);
                    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                }
            }
        }
    } else {
        ASSERT(range.aspects == Aspect::Color);

        // create temp buffer with clear color to copy to the texture image
        const TexelBlockInfo& blockInfo = GetFormat().GetAspectInfo(Aspect::Color).block;
        ASSERT(kTextureBytesPerRowAlignment % blockInfo.byteSize == 0);

        Extent3D largestMipSize = GetMipLevelSingleSubresourcePhysicalSize(range.baseMipLevel);
        uint32_t bytesPerRow =
            Align((largestMipSize.width / blockInfo.width) * blockInfo.byteSize, 4);

        // Make sure that we are not rounding
        ASSERT(bytesPerRow % blockInfo.byteSize == 0);
        ASSERT(largestMipSize.height % blockInfo.height == 0);

        uint64_t bufferSize64 = static_cast<uint64_t>(bytesPerRow) *
                                (largestMipSize.height / blockInfo.height) *
                                largestMipSize.depthOrArrayLayers;
        if (bufferSize64 > std::numeric_limits<size_t>::max()) {
            return DAWN_OUT_OF_MEMORY_ERROR("Unable to allocate buffer.");
        }
        size_t bufferSize = static_cast<size_t>(bufferSize64);

        dawn::native::BufferDescriptor descriptor = {};
        descriptor.mappedAtCreation = true;
        descriptor.usage = wgpu::BufferUsage::CopySrc;
        descriptor.size = bufferSize;

        // We don't count the lazy clear of srcBuffer because it is an internal buffer.
        // TODO(natlee@microsoft.com): use Dynamic Uploader here for temp buffer
        Ref<Buffer> srcBuffer;
        DAWN_TRY_ASSIGN(srcBuffer, Buffer::CreateInternalBuffer(device, &descriptor, false));

        // Fill the buffer with clear color
        memset(srcBuffer->GetMappedRange(0, bufferSize), clearColor, bufferSize);
        srcBuffer->Unmap();

        gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, srcBuffer->GetHandle());
        for (uint32_t level = range.baseMipLevel; level < range.baseMipLevel + range.levelCount;
             ++level) {
            TextureCopy textureCopy;
            textureCopy.texture = this;
            textureCopy.mipLevel = level;
            textureCopy.origin = {};
            textureCopy.aspect = Aspect::Color;

            TextureDataLayout dataLayout;
            dataLayout.offset = 0;
            dataLayout.bytesPerRow = bytesPerRow;
            dataLayout.rowsPerImage = largestMipSize.height;

            Extent3D mipSize = GetMipLevelSingleSubresourcePhysicalSize(level);

            for (uint32_t layer = range.baseArrayLayer;
                 layer < range.baseArrayLayer + range.layerCount; ++layer) {
                if (clearValue == TextureBase::ClearValue::Zero &&
                    IsSubresourceContentInitialized(
                        SubresourceRange::SingleMipAndLayer(level, layer, Aspect::Color))) {
                    // Skip lazy clears if already initialized.
                    continue;
                }

                textureCopy.origin.z = layer;
                DoTexSubImage(ToBackend(GetDevice())->gl, textureCopy, 0, dataLayout, mipSize);
            }
        }
        gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    if (clearValue == TextureBase::ClearValue::Zero) {
        SetIsSubresourceContentInitialized(true, range);
        device->IncrementLazyClearCountForTesting();
    }
    Touch();
    return {};
}

void Texture::EnsureSubresourceContentInitialized(const SubresourceRange& range) {
    if (!GetDevice()->IsToggleEnabled(Toggle::LazyClearResourceOnFirstUse)) {
        return;
    }
    if (!IsSubresourceContentInitialized(range)) {
        GetDevice()->ConsumedError(ClearTexture(range, TextureBase::ClearValue::Zero));
    }
}

// TextureView

TextureView::TextureView(TextureBase* texture, const TextureViewDescriptor* descriptor)
    : TextureViewBase(texture, descriptor), mOwnsHandle(false) {
    mTarget = TargetForTextureViewDimension(descriptor->dimension, descriptor->arrayLayerCount,
                                            texture->GetSampleCount());

    // Texture could be destroyed by the time we make a view.
    if (GetTexture()->GetTextureState() == Texture::TextureState::Destroyed) {
        return;
    }

    if (!RequiresCreatingNewTextureView(texture, descriptor)) {
        mHandle = ToBackend(texture)->GetHandle();
    } else {
        const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;
        if (gl.IsAtLeastGL(4, 3)) {
            mHandle = GenTexture(gl);
            const Texture* textureGL = ToBackend(texture);
            gl.TextureView(mHandle, mTarget, textureGL->GetHandle(), GetInternalFormat(),
                           descriptor->baseMipLevel, descriptor->mipLevelCount,
                           descriptor->baseArrayLayer, descriptor->arrayLayerCount);
            mOwnsHandle = true;
        } else {
            // Simulate glTextureView() with texture-to-texture copies.
            mUseCopy = true;
            mHandle = 0;
        }
    }
}

TextureView::~TextureView() {}

void TextureView::DestroyImpl() {
    TextureViewBase::DestroyImpl();
    if (mOwnsHandle) {
        ToBackend(GetDevice())->gl.DeleteTextures(1, &mHandle);
    }
}

GLuint TextureView::GetHandle() const {
    ASSERT(mHandle != 0);
    return mHandle;
}

GLenum TextureView::GetGLTarget() const {
    return mTarget;
}

void TextureView::BindToFramebuffer(GLenum target, GLenum attachment) {
    const OpenGLFunctions& gl = ToBackend(GetDevice())->gl;

    // Use the base texture where possible to minimize the amount of copying required on GLES.
    bool useOwnView = GetFormat().format != GetTexture()->GetFormat().format &&
                      !GetTexture()->GetFormat().HasDepthOrStencil();

    GLuint handle, textarget, mipLevel, arrayLayer;
    if (useOwnView) {
        // Use our own texture handle and target which points to a subset of the texture's
        // subresources.
        handle = GetHandle();
        textarget = GetGLTarget();
        mipLevel = 0;
        arrayLayer = 0;
    } else {
        // Use the texture's handle and target, with the view's base mip level and base array

        handle = ToBackend(GetTexture())->GetHandle();
        textarget = ToBackend(GetTexture())->GetGLTarget();
        mipLevel = GetBaseMipLevel();
        arrayLayer = GetBaseArrayLayer();
    }

    ASSERT(handle != 0);
    if (textarget == GL_TEXTURE_2D_ARRAY || textarget == GL_TEXTURE_3D) {
        gl.FramebufferTextureLayer(target, attachment, handle, mipLevel, arrayLayer);
    } else {
        gl.FramebufferTexture2D(target, attachment, textarget, handle, mipLevel);
    }
}

void TextureView::CopyIfNeeded() {
    if (!mUseCopy) {
        return;
    }

    const Texture* texture = ToBackend(GetTexture());
    if (mGenID == texture->GetGenID()) {
        return;
    }

    Device* device = ToBackend(GetDevice());
    const OpenGLFunctions& gl = device->gl;
    uint32_t srcLevel = GetBaseMipLevel();
    uint32_t numLevels = GetLevelCount();

    uint32_t width = texture->GetWidth() >> srcLevel;
    uint32_t height = texture->GetHeight() >> srcLevel;
    Extent3D size{width, height, GetLayerCount()};

    if (mHandle == 0) {
        mHandle = GenTexture(gl);
        gl.BindTexture(mTarget, mHandle);
        AllocateTexture(gl, mTarget, texture->GetSampleCount(), numLevels, GetInternalFormat(),
                        size);
        mOwnsHandle = true;
    }

    Origin3D src{0, 0, GetBaseArrayLayer()};
    Origin3D dst{0, 0, 0};
    for (GLuint level = 0; level < numLevels; ++level) {
        CopyImageSubData(gl, GetAspects(), texture->GetHandle(), texture->GetGLTarget(),
                         srcLevel + level, src, mHandle, mTarget, level, dst, size);
    }

    mGenID = texture->GetGenID();
}

GLenum TextureView::GetInternalFormat() const {
    // Depth/stencil don't support reinterpretation, and the aspect is specified at
    // bind time. In that case, we use the base texture format.
    const Format& format =
        GetFormat().HasDepthOrStencil() ? GetTexture()->GetFormat() : GetFormat();
    const GLFormat& glFormat = ToBackend(GetDevice())->GetGLFormat(format);
    return glFormat.internalFormat;
}

}  // namespace dawn::native::opengl
