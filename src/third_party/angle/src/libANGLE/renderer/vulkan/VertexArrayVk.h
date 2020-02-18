//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayVk.h:
//    Defines the class interface for VertexArrayVk, implementing VertexArrayImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_
#define LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_

#include "libANGLE/renderer/VertexArrayImpl.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
class BufferVk;
struct ConversionBuffer;

class VertexArrayVk : public VertexArrayImpl
{
  public:
    VertexArrayVk(ContextVk *contextVk, const gl::VertexArrayState &state);
    ~VertexArrayVk() override;

    void destroy(const gl::Context *context) override;

    angle::Result syncState(const gl::Context *context,
                            const gl::VertexArray::DirtyBits &dirtyBits,
                            gl::VertexArray::DirtyAttribBitsArray *attribBits,
                            gl::VertexArray::DirtyBindingBitsArray *bindingBits) override;

    void updateDefaultAttrib(ContextVk *contextVk,
                             size_t attribIndex,
                             VkBuffer bufferHandle,
                             uint32_t offset);

    angle::Result updateClientAttribs(const gl::Context *context,
                                      GLint firstVertex,
                                      GLsizei vertexOrIndexCount,
                                      GLsizei instanceCount,
                                      gl::DrawElementsType indexTypeOrInvalid,
                                      const void *indices);

    angle::Result handleLineLoop(ContextVk *contextVk,
                                 GLint firstVertex,
                                 GLsizei vertexOrIndexCount,
                                 gl::DrawElementsType indexTypeOrInvalid,
                                 const void *indices,
                                 size_t *indexCountOut);

    const gl::AttribArray<VkBuffer> &getCurrentArrayBufferHandles() const
    {
        return mCurrentArrayBufferHandles;
    }

    const gl::AttribArray<VkDeviceSize> &getCurrentArrayBufferOffsets() const
    {
        return mCurrentArrayBufferOffsets;
    }

    const gl::AttribArray<vk::BufferHelper *> &getCurrentArrayBuffers() const
    {
        return mCurrentArrayBuffers;
    }

    VkDeviceSize getCurrentElementArrayBufferOffset() const
    {
        return mCurrentElementArrayBufferOffset;
    }

    void updateCurrentElementArrayBufferOffset(const GLvoid *offset)
    {
        mCurrentElementArrayBufferOffset = reinterpret_cast<VkDeviceSize>(offset);
    }

    vk::BufferHelper *getCurrentElementArrayBuffer() const { return mCurrentElementArrayBuffer; }

    angle::Result convertIndexBufferGPU(ContextVk *contextVk,
                                        BufferVk *bufferVk,
                                        const void *indices);

    angle::Result convertIndexBufferCPU(ContextVk *contextVk,
                                        gl::DrawElementsType indexType,
                                        size_t indexCount,
                                        const void *sourcePointer);

  private:
    void setDefaultPackedInput(ContextVk *contextVk, size_t attribIndex);

    angle::Result convertVertexBufferGPU(ContextVk *contextVk,
                                         BufferVk *srcBuffer,
                                         const gl::VertexBinding &binding,
                                         size_t attribIndex,
                                         const vk::Format &vertexFormat,
                                         ConversionBuffer *conversion);
    angle::Result convertVertexBufferCPU(ContextVk *contextVk,
                                         BufferVk *srcBuffer,
                                         const gl::VertexBinding &binding,
                                         size_t attribIndex,
                                         const vk::Format &vertexFormat,
                                         ConversionBuffer *conversion);

    angle::Result syncDirtyAttrib(ContextVk *contextVk,
                                  const gl::VertexAttribute &attrib,
                                  const gl::VertexBinding &binding,
                                  size_t attribIndex);

    void syncDirtyBuffer(ContextVk *contextVk,
                         const gl::VertexBinding &binding,
                         size_t bindingIndex);

    gl::AttribArray<VkBuffer> mCurrentArrayBufferHandles;
    gl::AttribArray<VkDeviceSize> mCurrentArrayBufferOffsets;
    gl::AttribArray<vk::BufferHelper *> mCurrentArrayBuffers;
    VkDeviceSize mCurrentElementArrayBufferOffset;
    vk::BufferHelper *mCurrentElementArrayBuffer;

    vk::DynamicBuffer mDynamicVertexData;
    vk::DynamicBuffer mDynamicIndexData;
    vk::DynamicBuffer mTranslatedByteIndexData;

    vk::LineLoopHelper mLineLoopHelper;
    Optional<GLint> mLineLoopBufferFirstIndex;
    Optional<size_t> mLineLoopBufferLastIndex;
    bool mDirtyLineLoopTranslation;

    // Vulkan does not allow binding a null vertex buffer. We use a dummy as a placeholder.
    vk::BufferHelper mTheNullBuffer;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_
