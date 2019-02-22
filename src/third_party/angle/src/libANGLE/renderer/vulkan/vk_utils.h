//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_utils:
//    Helper functions for the Vulkan Renderer.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VK_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_VK_UTILS_H_

#include <limits>

#include <vulkan/vulkan.h>

#include "common/Optional.h"
#include "common/PackedEnums.h"
#include "common/debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/Observer.h"
#include "libANGLE/renderer/renderer_utils.h"

#define ANGLE_GL_OBJECTS_X(PROC) \
    PROC(Buffer)                 \
    PROC(Context)                \
    PROC(Framebuffer)            \
    PROC(Program)                \
    PROC(Texture)                \
    PROC(VertexArray)

#define ANGLE_PRE_DECLARE_OBJECT(OBJ) class OBJ;

namespace egl
{
class Display;
}

namespace gl
{
struct Box;
class DrawCallParams;
struct Extents;
struct RasterizerState;
struct Rectangle;
struct SwizzleState;
struct VertexAttribute;
class VertexBinding;

ANGLE_GL_OBJECTS_X(ANGLE_PRE_DECLARE_OBJECT);
}  // namespace gl

#define ANGLE_PRE_DECLARE_VK_OBJECT(OBJ) class OBJ##Vk;

namespace rx
{
class CommandGraphResource;
class DisplayVk;
class RenderTargetVk;
class RendererVk;
class RenderPassCache;
}  // namespace rx

namespace angle
{
egl::Error ToEGL(Result result, rx::DisplayVk *displayVk, EGLint errorCode);
}  // namespace angle

namespace rx
{
ANGLE_GL_OBJECTS_X(ANGLE_PRE_DECLARE_VK_OBJECT);

const char *VulkanResultString(VkResult result);
// Verify that validation layers are available.
bool GetAvailableValidationLayers(const std::vector<VkLayerProperties> &layerProps,
                                  bool mustHaveLayers,
                                  const char *const **enabledLayerNames,
                                  uint32_t *enabledLayerCount);

uint32_t GetImageLayerCount(gl::TextureType textureType);

extern const char *g_VkLoaderLayersPathEnv;
extern const char *g_VkICDPathEnv;

enum class TextureDimension
{
    TEX_2D,
    TEX_CUBE,
    TEX_3D,
    TEX_2D_ARRAY,
};

namespace vk
{
struct Format;

// Abstracts error handling. Implemented by both ContextVk for GL and DisplayVk for EGL errors.
class Context : angle::NonCopyable
{
  public:
    Context(RendererVk *renderer);
    virtual ~Context();

    virtual void handleError(VkResult result, const char *file, unsigned int line) = 0;
    VkDevice getDevice() const;
    RendererVk *getRenderer() const { return mRenderer; }

  protected:
    RendererVk *const mRenderer;
};

VkImageAspectFlags GetDepthStencilAspectFlags(const angle::Format &format);
VkImageAspectFlags GetFormatAspectFlags(const angle::Format &format);
VkImageAspectFlags GetDepthStencilAspectFlagsForCopy(bool copyDepth, bool copyStencil);

template <typename T>
struct ImplTypeHelper;

// clang-format off
#define ANGLE_IMPL_TYPE_HELPER_GL(OBJ) \
template<>                             \
struct ImplTypeHelper<gl::OBJ>         \
{                                      \
    using ImplType = OBJ##Vk;          \
};
// clang-format on

ANGLE_GL_OBJECTS_X(ANGLE_IMPL_TYPE_HELPER_GL)

template <>
struct ImplTypeHelper<egl::Display>
{
    using ImplType = DisplayVk;
};

template <typename T>
using GetImplType = typename ImplTypeHelper<T>::ImplType;

template <typename T>
GetImplType<T> *GetImpl(const T *glObject)
{
    return GetImplAs<GetImplType<T>>(glObject);
}

// Unimplemented handle types:
// Instance
// PhysicalDevice
// Device
// Queue
// Event
// QueryPool
// BufferView
// DescriptorSet
// PipelineCache

#define ANGLE_HANDLE_TYPES_X(FUNC) \
    FUNC(Semaphore)                \
    FUNC(CommandBuffer)            \
    FUNC(Fence)                    \
    FUNC(DeviceMemory)             \
    FUNC(Buffer)                   \
    FUNC(Image)                    \
    FUNC(ImageView)                \
    FUNC(ShaderModule)             \
    FUNC(PipelineLayout)           \
    FUNC(RenderPass)               \
    FUNC(Pipeline)                 \
    FUNC(DescriptorSetLayout)      \
    FUNC(Sampler)                  \
    FUNC(DescriptorPool)           \
    FUNC(Framebuffer)              \
    FUNC(CommandPool)              \
    FUNC(QueryPool)

#define ANGLE_COMMA_SEP_FUNC(TYPE) TYPE,

enum class HandleType
{
    Invalid,
    ANGLE_HANDLE_TYPES_X(ANGLE_COMMA_SEP_FUNC)
};

#undef ANGLE_COMMA_SEP_FUNC

#define ANGLE_PRE_DECLARE_CLASS_FUNC(TYPE) class TYPE;
ANGLE_HANDLE_TYPES_X(ANGLE_PRE_DECLARE_CLASS_FUNC)
#undef ANGLE_PRE_DECLARE_CLASS_FUNC

// Returns the HandleType of a Vk Handle.
template <typename T>
struct HandleTypeHelper;

// clang-format off
#define ANGLE_HANDLE_TYPE_HELPER_FUNC(TYPE)                     \
template<> struct HandleTypeHelper<TYPE>                        \
{                                                               \
    constexpr static HandleType kHandleType = HandleType::TYPE; \
};
// clang-format on

ANGLE_HANDLE_TYPES_X(ANGLE_HANDLE_TYPE_HELPER_FUNC)

#undef ANGLE_HANDLE_TYPE_HELPER_FUNC

class GarbageObject final
{
  public:
    template <typename ObjectT>
    GarbageObject(Serial serial, const ObjectT &object)
        : mSerial(serial),
          mHandleType(HandleTypeHelper<ObjectT>::kHandleType),
          mHandle(reinterpret_cast<VkDevice>(object.getHandle()))
    {
    }

    GarbageObject();
    GarbageObject(const GarbageObject &other);
    GarbageObject &operator=(const GarbageObject &other);

    bool destroyIfComplete(VkDevice device, Serial completedSerial);
    void destroy(VkDevice device);

  private:
    // TODO(jmadill): Since many objects will have the same serial, it might be more efficient to
    // store the serial outside of the garbage object itself. We could index ranges of garbage
    // objects in the Renderer, using a circular buffer.
    Serial mSerial;
    HandleType mHandleType;
    VkDevice mHandle;
};

template <typename DerivedT, typename HandleT>
class WrappedObject : angle::NonCopyable
{
  public:
    HandleT getHandle() const { return mHandle; }
    bool valid() const { return (mHandle != VK_NULL_HANDLE); }

    const HandleT *ptr() const { return &mHandle; }

    void dumpResources(Serial serial, std::vector<GarbageObject> *garbageQueue)
    {
        if (valid())
        {
            garbageQueue->emplace_back(serial, *static_cast<DerivedT *>(this));
            mHandle = VK_NULL_HANDLE;
        }
    }

  protected:
    WrappedObject() : mHandle(VK_NULL_HANDLE) {}
    ~WrappedObject() { ASSERT(!valid()); }

    WrappedObject(WrappedObject &&other) : mHandle(other.mHandle)
    {
        other.mHandle = VK_NULL_HANDLE;
    }

    // Only works to initialize empty objects, since we don't have the device handle.
    WrappedObject &operator=(WrappedObject &&other)
    {
        ASSERT(!valid());
        std::swap(mHandle, other.mHandle);
        return *this;
    }

    HandleT mHandle;
};

class MemoryProperties final : angle::NonCopyable
{
  public:
    MemoryProperties();

    void init(VkPhysicalDevice physicalDevice);
    angle::Result findCompatibleMemoryIndex(Context *context,
                                            const VkMemoryRequirements &memoryRequirements,
                                            VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                            VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                            uint32_t *indexOut) const;
    void destroy();

  private:
    VkPhysicalDeviceMemoryProperties mMemoryProperties;
};

class CommandPool final : public WrappedObject<CommandPool, VkCommandPool>
{
  public:
    CommandPool();

    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkCommandPoolCreateInfo &createInfo);
};

// Helper class that wraps a Vulkan command buffer.
class CommandBuffer : public WrappedObject<CommandBuffer, VkCommandBuffer>
{
  public:
    CommandBuffer();

    VkCommandBuffer releaseHandle();

    // This is used for normal pool allocated command buffers. It reset the handle.
    void destroy(VkDevice device);

    // This is used in conjunction with VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT.
    void destroy(VkDevice device, const CommandPool &commandPool);

    angle::Result init(Context *context, const VkCommandBufferAllocateInfo &createInfo);
    void blitImage(const Image &srcImage,
                   VkImageLayout srcImageLayout,
                   const Image &dstImage,
                   VkImageLayout dstImageLayout,
                   uint32_t regionCount,
                   VkImageBlit *pRegions,
                   VkFilter filter);
    using WrappedObject::operator=;

    angle::Result begin(Context *context, const VkCommandBufferBeginInfo &info);

    angle::Result end(Context *context);
    angle::Result reset(Context *context);

    void pipelineBarrier(VkPipelineStageFlags srcStageMask,
                         VkPipelineStageFlags dstStageMask,
                         VkDependencyFlags dependencyFlags,
                         uint32_t memoryBarrierCount,
                         const VkMemoryBarrier *memoryBarriers,
                         uint32_t bufferMemoryBarrierCount,
                         const VkBufferMemoryBarrier *bufferMemoryBarriers,
                         uint32_t imageMemoryBarrierCount,
                         const VkImageMemoryBarrier *imageMemoryBarriers);

    void clearColorImage(const Image &image,
                         VkImageLayout imageLayout,
                         const VkClearColorValue &color,
                         uint32_t rangeCount,
                         const VkImageSubresourceRange *ranges);
    void clearDepthStencilImage(const Image &image,
                                VkImageLayout imageLayout,
                                const VkClearDepthStencilValue &depthStencil,
                                uint32_t rangeCount,
                                const VkImageSubresourceRange *ranges);

    void clearAttachments(uint32_t attachmentCount,
                          const VkClearAttachment *attachments,
                          uint32_t rectCount,
                          const VkClearRect *rects);

    void copyBuffer(const Buffer &srcBuffer,
                    const Buffer &destBuffer,
                    uint32_t regionCount,
                    const VkBufferCopy *regions);

    void copyBuffer(const VkBuffer &srcBuffer,
                    const VkBuffer &destBuffer,
                    uint32_t regionCount,
                    const VkBufferCopy *regions);

    void copyBufferToImage(VkBuffer srcBuffer,
                           const Image &dstImage,
                           VkImageLayout dstImageLayout,
                           uint32_t regionCount,
                           const VkBufferImageCopy *regions);
    void copyImageToBuffer(const Image &srcImage,
                           VkImageLayout srcImageLayout,
                           VkBuffer dstBuffer,
                           uint32_t regionCount,
                           const VkBufferImageCopy *regions);
    void copyImage(const Image &srcImage,
                   VkImageLayout srcImageLayout,
                   const Image &dstImage,
                   VkImageLayout dstImageLayout,
                   uint32_t regionCount,
                   const VkImageCopy *regions);

    void beginRenderPass(const VkRenderPassBeginInfo &beginInfo, VkSubpassContents subpassContents);
    void endRenderPass();

    void draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex,
              uint32_t firstInstance)
    {
        ASSERT(valid());
        vkCmdDraw(mHandle, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     int32_t vertexOffset,
                     uint32_t firstInstance)
    {
        ASSERT(valid());
        vkCmdDrawIndexed(mHandle, indexCount, instanceCount, firstIndex, vertexOffset,
                         firstInstance);
    }

    void bindPipeline(VkPipelineBindPoint pipelineBindPoint, const Pipeline &pipeline);
    void bindVertexBuffers(uint32_t firstBinding,
                           uint32_t bindingCount,
                           const VkBuffer *buffers,
                           const VkDeviceSize *offsets);
    void bindIndexBuffer(const VkBuffer &buffer, VkDeviceSize offset, VkIndexType indexType);
    void bindDescriptorSets(VkPipelineBindPoint bindPoint,
                            const PipelineLayout &layout,
                            uint32_t firstSet,
                            uint32_t descriptorSetCount,
                            const VkDescriptorSet *descriptorSets,
                            uint32_t dynamicOffsetCount,
                            const uint32_t *dynamicOffsets);

    void executeCommands(uint32_t commandBufferCount, const CommandBuffer *commandBuffers);
    void updateBuffer(const vk::Buffer &buffer,
                      VkDeviceSize dstOffset,
                      VkDeviceSize dataSize,
                      const void *data);
    void pushConstants(const PipelineLayout &layout,
                       VkShaderStageFlags flag,
                       uint32_t offset,
                       uint32_t size,
                       const void *data);

    void resetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);
    void beginQuery(VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags);
    void endQuery(VkQueryPool queryPool, uint32_t query);
};

class Image final : public WrappedObject<Image, VkImage>
{
  public:
    Image();

    // Use this method if the lifetime of the image is not controlled by ANGLE. (SwapChain)
    void setHandle(VkImage handle);

    // Called on shutdown when the helper class *doesn't* own the handle to the image resource.
    void reset();

    // Called on shutdown when the helper class *does* own the handle to the image resource.
    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkImageCreateInfo &createInfo);

    void getMemoryRequirements(VkDevice device, VkMemoryRequirements *requirementsOut) const;
    angle::Result bindMemory(Context *context, const DeviceMemory &deviceMemory);

    void getSubresourceLayout(VkDevice device,
                              VkImageAspectFlagBits aspectMask,
                              uint32_t mipLevel,
                              uint32_t arrayLayer,
                              VkSubresourceLayout *outSubresourceLayout) const;
};

class ImageView final : public WrappedObject<ImageView, VkImageView>
{
  public:
    ImageView();
    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkImageViewCreateInfo &createInfo);
};

class Semaphore final : public WrappedObject<Semaphore, VkSemaphore>
{
  public:
    Semaphore();
    void destroy(VkDevice device);

    angle::Result init(Context *context);
};

class Framebuffer final : public WrappedObject<Framebuffer, VkFramebuffer>
{
  public:
    Framebuffer();
    void destroy(VkDevice device);

    // Use this method only in necessary cases. (RenderPass)
    void setHandle(VkFramebuffer handle);

    angle::Result init(Context *context, const VkFramebufferCreateInfo &createInfo);
};

class DeviceMemory final : public WrappedObject<DeviceMemory, VkDeviceMemory>
{
  public:
    DeviceMemory();
    void destroy(VkDevice device);

    angle::Result allocate(Context *context, const VkMemoryAllocateInfo &allocInfo);
    angle::Result map(Context *context,
                      VkDeviceSize offset,
                      VkDeviceSize size,
                      VkMemoryMapFlags flags,
                      uint8_t **mapPointer) const;
    void unmap(VkDevice device) const;
};

class RenderPass final : public WrappedObject<RenderPass, VkRenderPass>
{
  public:
    RenderPass();
    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkRenderPassCreateInfo &createInfo);
};

enum class StagingUsage
{
    Read,
    Write,
    Both,
};

class Buffer final : public WrappedObject<Buffer, VkBuffer>
{
  public:
    Buffer();
    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkBufferCreateInfo &createInfo);
    angle::Result bindMemory(Context *context, const DeviceMemory &deviceMemory);
    void getMemoryRequirements(VkDevice device, VkMemoryRequirements *memoryRequirementsOut);
};

class ShaderModule final : public WrappedObject<ShaderModule, VkShaderModule>
{
  public:
    ShaderModule();
    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkShaderModuleCreateInfo &createInfo);
};

class PipelineLayout final : public WrappedObject<PipelineLayout, VkPipelineLayout>
{
  public:
    PipelineLayout();
    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkPipelineLayoutCreateInfo &createInfo);
};

class PipelineCache final : public WrappedObject<PipelineCache, VkPipelineCache>
{
  public:
    PipelineCache();
    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkPipelineCacheCreateInfo &createInfo);
    angle::Result getCacheData(Context *context, size_t *cacheSize, void *cacheData);
};

class Pipeline final : public WrappedObject<Pipeline, VkPipeline>
{
  public:
    Pipeline();
    void destroy(VkDevice device);

    angle::Result initGraphics(Context *context,
                               const VkGraphicsPipelineCreateInfo &createInfo,
                               const PipelineCache &pipelineCacheVk);
};

class DescriptorSetLayout final : public WrappedObject<DescriptorSetLayout, VkDescriptorSetLayout>
{
  public:
    DescriptorSetLayout();
    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkDescriptorSetLayoutCreateInfo &createInfo);
};

class DescriptorPool final : public WrappedObject<DescriptorPool, VkDescriptorPool>
{
  public:
    DescriptorPool();
    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkDescriptorPoolCreateInfo &createInfo);

    angle::Result allocateDescriptorSets(Context *context,
                                         const VkDescriptorSetAllocateInfo &allocInfo,
                                         VkDescriptorSet *descriptorSetsOut);
    angle::Result freeDescriptorSets(Context *context,
                                     uint32_t descriptorSetCount,
                                     const VkDescriptorSet *descriptorSets);
};

class Sampler final : public WrappedObject<Sampler, VkSampler>
{
  public:
    Sampler();
    void destroy(VkDevice device);
    angle::Result init(Context *context, const VkSamplerCreateInfo &createInfo);
};

class Fence final : public WrappedObject<Fence, VkFence>
{
  public:
    Fence();
    void destroy(VkDevice fence);
    using WrappedObject::operator=;

    angle::Result init(Context *context, const VkFenceCreateInfo &createInfo);
    VkResult getStatus(VkDevice device) const;
};

// Similar to StagingImage, for Buffers.
class StagingBuffer final : angle::NonCopyable
{
  public:
    StagingBuffer();
    void destroy(VkDevice device);

    angle::Result init(Context *context, VkDeviceSize size, StagingUsage usage);

    Buffer &getBuffer() { return mBuffer; }
    const Buffer &getBuffer() const { return mBuffer; }
    DeviceMemory &getDeviceMemory() { return mDeviceMemory; }
    const DeviceMemory &getDeviceMemory() const { return mDeviceMemory; }
    size_t getSize() const { return mSize; }

    void dumpResources(Serial serial, std::vector<GarbageObject> *garbageQueue);

  private:
    Buffer mBuffer;
    DeviceMemory mDeviceMemory;
    size_t mSize;
};

class QueryPool final : public WrappedObject<QueryPool, VkQueryPool>
{
  public:
    QueryPool();
    void destroy(VkDevice device);

    angle::Result init(Context *context, const VkQueryPoolCreateInfo &createInfo);
    angle::Result getResults(Context *context,
                             uint32_t firstQuery,
                             uint32_t queryCount,
                             size_t dataSize,
                             void *data,
                             VkDeviceSize stride,
                             VkQueryResultFlags flags) const;
};

template <typename ObjT>
class ObjectAndSerial final : angle::NonCopyable
{
  public:
    ObjectAndSerial() {}

    ObjectAndSerial(ObjT &&object, Serial serial) : mObject(std::move(object)), mSerial(serial) {}

    ObjectAndSerial(ObjectAndSerial &&other)
        : mObject(std::move(other.mObject)), mSerial(std::move(other.mSerial))
    {
    }
    ObjectAndSerial &operator=(ObjectAndSerial &&other)
    {
        mObject = std::move(other.mObject);
        mSerial = std::move(other.mSerial);
        return *this;
    }

    Serial getSerial() const { return mSerial; }
    void updateSerial(Serial newSerial) { mSerial = newSerial; }

    const ObjT &get() const { return mObject; }
    ObjT &get() { return mObject; }

    bool valid() const { return mObject.valid(); }

    void destroy(VkDevice device)
    {
        mObject.destroy(device);
        mSerial = Serial();
    }

  private:
    ObjT mObject;
    Serial mSerial;
};

angle::Result AllocateBufferMemory(vk::Context *context,
                                   VkMemoryPropertyFlags requestedMemoryPropertyFlags,
                                   VkMemoryPropertyFlags *memoryPropertyFlagsOut,
                                   Buffer *buffer,
                                   DeviceMemory *deviceMemoryOut);

struct BufferAndMemory final : angle::NonCopyable
{
    BufferAndMemory();
    BufferAndMemory(Buffer &&buffer, DeviceMemory &&deviceMemory);
    BufferAndMemory(BufferAndMemory &&other);
    BufferAndMemory &operator=(BufferAndMemory &&other);

    Buffer buffer;
    DeviceMemory memory;
};

angle::Result AllocateImageMemory(vk::Context *context,
                                  VkMemoryPropertyFlags memoryPropertyFlags,
                                  Image *image,
                                  DeviceMemory *deviceMemoryOut);

using ShaderAndSerial = ObjectAndSerial<ShaderModule>;

// TODO(jmadill): Use gl::ShaderType when possible. http://anglebug.com/2522
enum class ShaderType
{
    VertexShader,
    FragmentShader,
    EnumCount,
    InvalidEnum = EnumCount,
};

template <typename T>
using ShaderMap = angle::PackedEnumMap<ShaderType, T>;

using ShaderBitSet = angle::PackedEnumBitSet<ShaderType>;

using AllShaderTypes = angle::AllEnums<vk::ShaderType>;

angle::Result InitShaderAndSerial(Context *context,
                                  ShaderAndSerial *shaderAndSerial,
                                  const uint32_t *shaderCode,
                                  size_t shaderCodeSize);

enum class RecordingMode
{
    Start,
    Append,
};

// Helper class to handle RAII patterns for initialization. Requires that T have a destroy method
// that takes a VkDevice and returns void.
template <typename T>
class Scoped final : angle::NonCopyable
{
  public:
    Scoped(VkDevice device) : mDevice(device) {}
    ~Scoped() { mVar.destroy(mDevice); }

    const T &get() const { return mVar; }
    T &get() { return mVar; }

    T &&release() { return std::move(mVar); }

  private:
    VkDevice mDevice;
    T mVar;
};
}  // namespace vk

namespace gl_vk
{
VkRect2D GetRect(const gl::Rectangle &source);
VkFilter GetFilter(const GLenum filter);
VkSamplerMipmapMode GetSamplerMipmapMode(const GLenum filter);
VkSamplerAddressMode GetSamplerAddressMode(const GLenum wrap);
VkPrimitiveTopology GetPrimitiveTopology(gl::PrimitiveMode mode);
VkCullModeFlags GetCullMode(const gl::RasterizerState &rasterState);
VkFrontFace GetFrontFace(GLenum frontFace, bool invertCullFace);
VkSampleCountFlagBits GetSamples(GLint sampleCount);
VkComponentSwizzle GetSwizzle(const GLenum swizzle);
VkIndexType GetIndexType(GLenum elementType);
void GetOffset(const gl::Offset &glOffset, VkOffset3D *vkOffset);
void GetExtent(const gl::Extents &glExtent, VkExtent3D *vkExtent);
VkImageType GetImageType(gl::TextureType textureType);
VkImageViewType GetImageViewType(gl::TextureType textureType);
VkColorComponentFlags GetColorComponentFlags(bool red, bool green, bool blue, bool alpha);
}  // namespace gl_vk

}  // namespace rx

#define ANGLE_VK_TRY(context, command)                                 \
    {                                                                  \
        auto ANGLE_LOCAL_VAR = command;                                \
        if (ANGLE_UNLIKELY(ANGLE_LOCAL_VAR != VK_SUCCESS))             \
        {                                                              \
            context->handleError(ANGLE_LOCAL_VAR, __FILE__, __LINE__); \
            return angle::Result::Stop();                              \
        }                                                              \
    }                                                                  \
    ANGLE_EMPTY_STATEMENT

#define ANGLE_VK_TRY_ALLOW_OTHER(context, command, acceptable, result)                      \
    {                                                                                       \
        auto ANGLE_LOCAL_VAR = command;                                                     \
        if (ANGLE_UNLIKELY(ANGLE_LOCAL_VAR != VK_SUCCESS && ANGLE_LOCAL_VAR != acceptable)) \
        {                                                                                   \
            context->handleError(ANGLE_LOCAL_VAR, __FILE__, __LINE__);                      \
            return angle::Result::Stop();                                                   \
        }                                                                                   \
        result = ANGLE_LOCAL_VAR == VK_SUCCESS ? angle::Result::Continue()                  \
                                               : angle::Result::Incomplete();               \
    }                                                                                       \
    ANGLE_EMPTY_STATEMENT

#define ANGLE_VK_TRY_ALLOW_INCOMPLETE(context, command, result) \
    ANGLE_VK_TRY_ALLOW_OTHER(context, command, VK_INCOMPLETE, result)

#define ANGLE_VK_TRY_ALLOW_NOT_READY(context, command, result) \
    ANGLE_VK_TRY_ALLOW_OTHER(context, command, VK_NOT_READY, result)

#define ANGLE_VK_CHECK(context, test, error) ANGLE_VK_TRY(context, test ? VK_SUCCESS : error)

#define ANGLE_VK_CHECK_MATH(context, result) \
    ANGLE_VK_CHECK(context, result, VK_ERROR_VALIDATION_FAILED_EXT)

#define ANGLE_VK_CHECK_ALLOC(context, result) \
    ANGLE_VK_CHECK(context, result, VK_ERROR_OUT_OF_HOST_MEMORY)

#endif  // LIBANGLE_RENDERER_VULKAN_VK_UTILS_H_
