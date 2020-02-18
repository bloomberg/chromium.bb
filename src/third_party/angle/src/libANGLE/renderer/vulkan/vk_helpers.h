//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_helpers:
//   Helper utilitiy classes that manage Vulkan resources.

#ifndef LIBANGLE_RENDERER_VULKAN_VK_HELPERS_H_
#define LIBANGLE_RENDERER_VULKAN_VK_HELPERS_H_

#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace gl
{
class ImageIndex;
}  // namespace gl

namespace rx
{
namespace vk
{
constexpr VkBufferUsageFlags kVertexBufferUsageFlags =
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
constexpr VkBufferUsageFlags kIndexBufferUsageFlags =
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
constexpr size_t kVertexBufferAlignment = 4;
constexpr size_t kIndexBufferAlignment  = 4;

constexpr VkBufferUsageFlags kStagingBufferFlags =
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
constexpr size_t kStagingBufferSize = 1024 * 16;

struct TextureUnit final
{
    TextureVk *texture;
    SamplerVk *sampler;
};

// A dynamic buffer is conceptually an infinitely long buffer. Each time you write to the buffer,
// you will always write to a previously unused portion. After a series of writes, you must flush
// the buffer data to the device. Buffer lifetime currently assumes that each new allocation will
// last as long or longer than each prior allocation.
//
// Dynamic buffers are used to implement a variety of data streaming operations in Vulkan, such
// as for immediate vertex array and element array data, uniform updates, and other dynamic data.
//
// Internally dynamic buffers keep a collection of VkBuffers. When we write past the end of a
// currently active VkBuffer we keep it until it is no longer in use. We then mark it available
// for future allocations in a free list.
class BufferHelper;
class DynamicBuffer : angle::NonCopyable
{
  public:
    DynamicBuffer();
    DynamicBuffer(DynamicBuffer &&other);
    ~DynamicBuffer();

    // Init is called after the buffer creation so that the alignment can be specified later.
    void init(RendererVk *renderer,
              VkBufferUsageFlags usage,
              size_t alignment,
              size_t initialSize,
              bool hostVisible);

    // This call will allocate a new region at the end of the buffer. It internally may trigger
    // a new buffer to be created (which is returned in the optional parameter
    // `newBufferAllocatedOut`).  The new region will be in the returned buffer at given offset. If
    // a memory pointer is given, the buffer will be automatically map()ed.
    angle::Result allocate(ContextVk *contextVk,
                           size_t sizeInBytes,
                           uint8_t **ptrOut,
                           VkBuffer *bufferOut,
                           VkDeviceSize *offsetOut,
                           bool *newBufferAllocatedOut);

    // After a sequence of writes, call flush to ensure the data is visible to the device.
    angle::Result flush(ContextVk *contextVk);

    // After a sequence of writes, call invalidate to ensure the data is visible to the host.
    angle::Result invalidate(ContextVk *contextVk);

    // This releases resources when they might currently be in use.
    void release(ContextVk *contextVk);
    void release(DisplayVk *display, std::vector<GarbageObjectBase> *garbageQueue);

    // This releases all the buffers that have been allocated since this was last called.
    void releaseInFlightBuffers(ContextVk *contextVk);

    // This frees resources immediately.
    void destroy(VkDevice device);

    BufferHelper *getCurrentBuffer() { return mBuffer; }

    void updateAlignment(RendererVk *renderer, size_t alignment);

    // For testing only!
    void setMinimumSizeForTesting(size_t minSize);

  private:
    void reset();
    angle::Result allocateNewBuffer(ContextVk *contextVk);
    void releaseBufferListToContext(ContextVk *contextVk, std::vector<BufferHelper *> *buffers);
    void releaseBufferListToDisplay(DisplayVk *display,
                                    std::vector<GarbageObjectBase> *garbageQueue,
                                    std::vector<BufferHelper *> *buffers);
    void destroyBufferList(VkDevice device, std::vector<BufferHelper *> *buffers);

    VkBufferUsageFlags mUsage;
    bool mHostVisible;
    size_t mInitialSize;
    BufferHelper *mBuffer;
    uint32_t mNextAllocationOffset;
    uint32_t mLastFlushOrInvalidateOffset;
    size_t mSize;
    size_t mAlignment;

    std::vector<BufferHelper *> mInFlightBuffers;
    std::vector<BufferHelper *> mBufferFreeList;
};

// Uses DescriptorPool to allocate descriptor sets as needed. If a descriptor pool becomes full, we
// allocate new pools internally as needed. RendererVk takes care of the lifetime of the discarded
// pools. Note that we used a fixed layout for descriptor pools in ANGLE. Uniform buffers must
// use set zero and combined Image Samplers must use set 1. We conservatively count each new set
// using the maximum number of descriptor sets and buffers with each allocation. Currently: 2
// (Vertex/Fragment) uniform buffers and 64 (MAX_ACTIVE_TEXTURES) image/samplers.

// Shared handle to a descriptor pool. Each helper is allocated from the dynamic descriptor pool.
// Can be used to share descriptor pools between multiple ProgramVks and the ContextVk.
class DescriptorPoolHelper
{
  public:
    DescriptorPoolHelper();
    ~DescriptorPoolHelper();

    bool valid() { return mDescriptorPool.valid(); }

    bool hasCapacity(uint32_t descriptorSetCount) const;
    angle::Result init(Context *context,
                       const std::vector<VkDescriptorPoolSize> &poolSizes,
                       uint32_t maxSets);
    void destroy(VkDevice device);
    void release(ContextVk *contextVk);

    angle::Result allocateSets(ContextVk *contextVk,
                               const VkDescriptorSetLayout *descriptorSetLayout,
                               uint32_t descriptorSetCount,
                               VkDescriptorSet *descriptorSetsOut);

    void updateSerial(Serial serial) { mMostRecentSerial = serial; }

    Serial getSerial() const { return mMostRecentSerial; }

  private:
    uint32_t mFreeDescriptorSets;
    DescriptorPool mDescriptorPool;
    Serial mMostRecentSerial;
};

using RefCountedDescriptorPoolHelper  = RefCounted<DescriptorPoolHelper>;
using RefCountedDescriptorPoolBinding = BindingPointer<DescriptorPoolHelper>;

class DynamicDescriptorPool final : angle::NonCopyable
{
  public:
    DynamicDescriptorPool();
    ~DynamicDescriptorPool();

    // The DynamicDescriptorPool only handles one pool size at this time.
    // Note that setSizes[i].descriptorCount is expected to be the number of descriptors in
    // an individual set.  The pool size will be calculated accordingly.
    angle::Result init(ContextVk *contextVk,
                       const VkDescriptorPoolSize *setSizes,
                       uint32_t setSizeCount);
    void destroy(VkDevice device);
    void release(ContextVk *contextVk);

    // We use the descriptor type to help count the number of free sets.
    // By convention, sets are indexed according to the constants in vk_cache_utils.h.
    ANGLE_INLINE angle::Result allocateSets(ContextVk *contextVk,
                                            const VkDescriptorSetLayout *descriptorSetLayout,
                                            uint32_t descriptorSetCount,
                                            RefCountedDescriptorPoolBinding *bindingOut,
                                            VkDescriptorSet *descriptorSetsOut)
    {
        bool ignoreNewPoolAllocated;
        return allocateSetsAndGetInfo(contextVk, descriptorSetLayout, descriptorSetCount,
                                      bindingOut, descriptorSetsOut, &ignoreNewPoolAllocated);
    }

    // We use the descriptor type to help count the number of free sets.
    // By convention, sets are indexed according to the constants in vk_cache_utils.h.
    angle::Result allocateSetsAndGetInfo(ContextVk *contextVk,
                                         const VkDescriptorSetLayout *descriptorSetLayout,
                                         uint32_t descriptorSetCount,
                                         RefCountedDescriptorPoolBinding *bindingOut,
                                         VkDescriptorSet *descriptorSetsOut,
                                         bool *newPoolAllocatedOut);

    // For testing only!
    void setMaxSetsPerPoolForTesting(uint32_t maxSetsPerPool);

  private:
    angle::Result allocateNewPool(ContextVk *contextVk);

    uint32_t mMaxSetsPerPool;
    size_t mCurrentPoolIndex;
    std::vector<RefCountedDescriptorPoolHelper *> mDescriptorPools;
    std::vector<VkDescriptorPoolSize> mPoolSizes;
};

template <typename Pool>
class DynamicallyGrowingPool : angle::NonCopyable
{
  public:
    DynamicallyGrowingPool();
    virtual ~DynamicallyGrowingPool();

    bool isValid() { return mPoolSize > 0; }

  protected:
    angle::Result initEntryPool(Context *contextVk, uint32_t poolSize);
    void destroyEntryPool();

    // Checks to see if any pool is already free, in which case it sets it as current pool and
    // returns true.
    bool findFreeEntryPool(ContextVk *contextVk);

    // Allocates a new entry and initializes it with the given pool.
    angle::Result allocateNewEntryPool(ContextVk *contextVk, Pool &&pool);

    // Called by the implementation whenever an entry is freed.
    void onEntryFreed(ContextVk *contextVk, size_t poolIndex);

    // The pool size, to know when a pool is completely freed.
    uint32_t mPoolSize;

    std::vector<Pool> mPools;

    struct PoolStats
    {
        // A count corresponding to each pool indicating how many of its allocated entries
        // have been freed. Once that value reaches mPoolSize for each pool, that pool is considered
        // free and reusable.  While keeping a bitset would allow allocation of each index, the
        // slight runtime overhead of finding free indices is not worth the slight memory overhead
        // of creating new pools when unnecessary.
        uint32_t freedCount;
        // The serial of the renderer is stored on each object free to make sure no
        // new allocations are made from the pool until it's not in use.
        Serial serial;
    };
    std::vector<PoolStats> mPoolStats;

    // Index into mPools indicating pool we are currently allocating from.
    size_t mCurrentPool;
    // Index inside mPools[mCurrentPool] indicating which index can be allocated next.
    uint32_t mCurrentFreeEntry;
};

// DynamicQueryPool allocates indices out of QueryPool as needed.  Once a QueryPool is exhausted,
// another is created.  The query pools live permanently, but are recycled as indices get freed.

// These are arbitrary default sizes for query pools.
constexpr uint32_t kDefaultOcclusionQueryPoolSize = 64;
constexpr uint32_t kDefaultTimestampQueryPoolSize = 64;

class QueryHelper;

class DynamicQueryPool final : public DynamicallyGrowingPool<QueryPool>
{
  public:
    DynamicQueryPool();
    ~DynamicQueryPool() override;

    angle::Result init(ContextVk *contextVk, VkQueryType type, uint32_t poolSize);
    void destroy(VkDevice device);

    angle::Result allocateQuery(ContextVk *contextVk, QueryHelper *queryOut);
    void freeQuery(ContextVk *contextVk, QueryHelper *query);

    // Special allocator that doesn't work with QueryHelper, which is a CommandGraphResource.
    // Currently only used with RendererVk::GpuEventQuery.
    angle::Result allocateQuery(ContextVk *contextVk, size_t *poolIndex, uint32_t *queryIndex);
    void freeQuery(ContextVk *contextVk, size_t poolIndex, uint32_t queryIndex);

    const QueryPool *getQueryPool(size_t index) const { return &mPools[index]; }

  private:
    angle::Result allocateNewPool(ContextVk *contextVk);

    // Information required to create new query pools
    VkQueryType mQueryType;
};

// Queries in vulkan are identified by the query pool and an index for a query within that pool.
// Unlike other pools, such as descriptor pools where an allocation returns an independent object
// from the pool, the query allocations are not done through a Vulkan function and are only an
// integer index.
//
// Furthermore, to support arbitrarily large number of queries, DynamicQueryPool creates query pools
// of a fixed size as needed and allocates indices within those pools.
//
// The QueryHelper class below keeps the pool and index pair together.
class QueryHelper final
{
  public:
    QueryHelper();
    ~QueryHelper();

    void init(const DynamicQueryPool *dynamicQueryPool,
              const size_t queryPoolIndex,
              uint32_t query);
    void deinit();

    const QueryPool *getQueryPool() const
    {
        return mDynamicQueryPool ? mDynamicQueryPool->getQueryPool(mQueryPoolIndex) : nullptr;
    }
    uint32_t getQuery() const { return mQuery; }

    // Used only by DynamicQueryPool.
    size_t getQueryPoolIndex() const { return mQueryPoolIndex; }

    void beginQuery(ContextVk *contextVk);
    void endQuery(ContextVk *contextVk);
    void writeTimestamp(ContextVk *contextVk);

    Serial getStoredQueueSerial() { return mMostRecentSerial; }
    bool hasPendingWork(ContextVk *contextVk);

  private:
    const DynamicQueryPool *mDynamicQueryPool;
    size_t mQueryPoolIndex;
    uint32_t mQuery;
    Serial mMostRecentSerial;
};

// DynamicSemaphorePool allocates semaphores as needed.  It uses a std::vector
// as a pool to allocate many semaphores at once.  The pools live permanently,
// but are recycled as semaphores get freed.

// These are arbitrary default sizes for semaphore pools.
constexpr uint32_t kDefaultSemaphorePoolSize = 64;

class SemaphoreHelper;

class DynamicSemaphorePool final : public DynamicallyGrowingPool<std::vector<Semaphore>>
{
  public:
    DynamicSemaphorePool();
    ~DynamicSemaphorePool() override;

    angle::Result init(ContextVk *contextVk, uint32_t poolSize);
    void destroy(VkDevice device);

    bool isValid() { return mPoolSize > 0; }

    // autoFree can be used to allocate a semaphore that's expected to be freed at the end of the
    // frame.  This renders freeSemaphore unnecessary and saves an eventual search.
    angle::Result allocateSemaphore(ContextVk *contextVk, SemaphoreHelper *semaphoreOut);
    void freeSemaphore(ContextVk *contextVk, SemaphoreHelper *semaphore);

  private:
    angle::Result allocateNewPool(ContextVk *contextVk);
};

// Semaphores that are allocated from the semaphore pool are encapsulated in a helper object,
// keeping track of where in the pool they are allocated from.
class SemaphoreHelper final : angle::NonCopyable
{
  public:
    SemaphoreHelper();
    ~SemaphoreHelper();

    SemaphoreHelper(SemaphoreHelper &&other);
    SemaphoreHelper &operator=(SemaphoreHelper &&other);

    void init(const size_t semaphorePoolIndex, const Semaphore *semaphore);
    void deinit();

    const Semaphore *getSemaphore() const { return mSemaphore; }

    // Used only by DynamicSemaphorePool.
    size_t getSemaphorePoolIndex() const { return mSemaphorePoolIndex; }

  private:
    size_t mSemaphorePoolIndex;
    const Semaphore *mSemaphore;
};

// This class' responsibility is to create index buffers needed to support line loops in Vulkan.
// In the setup phase of drawing, the createIndexBuffer method should be called with the
// current draw call parameters. If an element array buffer is bound for an indexed draw, use
// createIndexBufferFromElementArrayBuffer.
//
// If the user wants to draw a loop between [v1, v2, v3], we will create an indexed buffer with
// these indexes: [0, 1, 2, 3, 0] to emulate the loop.
class LineLoopHelper final : angle::NonCopyable
{
  public:
    LineLoopHelper(RendererVk *renderer);
    ~LineLoopHelper();

    angle::Result getIndexBufferForDrawArrays(ContextVk *contextVk,
                                              uint32_t clampedVertexCount,
                                              GLint firstVertex,
                                              vk::BufferHelper **bufferOut,
                                              VkDeviceSize *offsetOut);

    angle::Result getIndexBufferForElementArrayBuffer(ContextVk *contextVk,
                                                      BufferVk *elementArrayBufferVk,
                                                      gl::DrawElementsType glIndexType,
                                                      int indexCount,
                                                      intptr_t elementArrayOffset,
                                                      vk::BufferHelper **bufferOut,
                                                      VkDeviceSize *bufferOffsetOut,
                                                      uint32_t *indexCountOut);

    angle::Result streamIndices(ContextVk *contextVk,
                                gl::DrawElementsType glIndexType,
                                GLsizei indexCount,
                                const uint8_t *srcPtr,
                                vk::BufferHelper **bufferOut,
                                VkDeviceSize *bufferOffsetOut,
                                uint32_t *indexCountOut);

    void release(ContextVk *contextVk);
    void destroy(VkDevice device);

    static void Draw(uint32_t count, vk::CommandBuffer *commandBuffer);

  private:
    DynamicBuffer mDynamicIndexBuffer;
};

class FramebufferHelper;

class BufferHelper final : public CommandGraphResource
{
  public:
    BufferHelper();
    ~BufferHelper() override;

    angle::Result init(ContextVk *contextVk,
                       const VkBufferCreateInfo &createInfo,
                       VkMemoryPropertyFlags memoryPropertyFlags);
    void destroy(VkDevice device);

    void release(ContextVk *contextVk);
    void release(DisplayVk *display, std::vector<GarbageObjectBase> *garbageQueue);

    bool valid() const { return mBuffer.valid(); }
    const Buffer &getBuffer() const { return mBuffer; }
    const DeviceMemory &getDeviceMemory() const { return mDeviceMemory; }
    VkDeviceSize getSize() const { return mSize; }

    // Helpers for setting the graph dependencies *and* setting the appropriate barrier.  These are
    // made for dependencies to non-buffer resources, as only one of two resources participating in
    // the dependency would require a memory barrier.  Note that onWrite takes read access flags
    // too, as output buffers could be read as well.
    void onRead(CommandGraphResource *reader, VkAccessFlags readAccessType)
    {
        addReadDependency(reader);
        onReadAccess(reader, readAccessType);
    }
    void onWrite(ContextVk *contextVk,
                 CommandGraphResource *writer,
                 VkAccessFlags readAccessType,
                 VkAccessFlags writeAccessType)
    {
        addWriteDependency(writer);
        onWriteAccess(contextVk, readAccessType, writeAccessType);
    }
    // Helper for setting a graph dependency between two buffers.  This is a specialized function as
    // both buffers may incur a memory barrier.  Using |onRead| followed by |onWrite| between the
    // buffers is impossible as it would result in a command graph loop.
    void onReadByBuffer(ContextVk *contextVk,
                        BufferHelper *reader,
                        VkAccessFlags readAccessType,
                        VkAccessFlags writeAccessType)
    {
        addReadDependency(reader);
        onReadAccess(reader, readAccessType);
        reader->onWriteAccess(contextVk, 0, writeAccessType);
    }
    // Helper for setting a barrier when different parts of the same buffer is being read from and
    // written to.
    void onSelfReadWrite(ContextVk *contextVk,
                         VkAccessFlags readAccessType,
                         VkAccessFlags writeAccessType)
    {
        if (mCurrentReadAccess || mCurrentWriteAccess)
        {
            finishCurrentCommands(contextVk);
        }
        onWriteAccess(contextVk, readAccessType, writeAccessType);
    }
    // Set write access mask when the buffer is modified externally, e.g. by host.  There is no
    // graph resource to create a dependency to.
    void onExternalWrite(VkAccessFlags writeAccessType) { mCurrentWriteAccess |= writeAccessType; }

    // Also implicitly sets up the correct barriers.
    angle::Result copyFromBuffer(ContextVk *contextVk,
                                 const Buffer &buffer,
                                 VkAccessFlags bufferAccessType,
                                 const VkBufferCopy &copyRegion);

    // Note: currently only one view is allowed.  If needs be, multiple views can be created
    // based on format.
    angle::Result initBufferView(ContextVk *contextVk, const Format &format);

    const BufferView &getBufferView() const
    {
        ASSERT(mBufferView.valid());
        return mBufferView;
    }

    const Format &getViewFormat() const
    {
        ASSERT(mViewFormat);
        return *mViewFormat;
    }

    angle::Result map(ContextVk *contextVk, uint8_t **ptrOut)
    {
        if (!mMappedMemory)
        {
            ANGLE_TRY(mapImpl(contextVk));
        }
        *ptrOut = mMappedMemory;
        return angle::Result::Continue;
    }
    void unmap(VkDevice device);

    // After a sequence of writes, call flush to ensure the data is visible to the device.
    angle::Result flush(ContextVk *contextVk, VkDeviceSize offset, VkDeviceSize size);

    // After a sequence of writes, call invalidate to ensure the data is visible to the host.
    angle::Result invalidate(ContextVk *contextVk, VkDeviceSize offset, VkDeviceSize size);

  private:
    angle::Result mapImpl(ContextVk *contextVk);
    bool needsOnReadBarrier(VkAccessFlags readAccessType,
                            VkAccessFlags *barrierSrcOut,
                            VkAccessFlags *barrierDstOut)
    {
        bool needsBarrier =
            mCurrentWriteAccess != 0 && (mCurrentReadAccess & readAccessType) != readAccessType;

        *barrierSrcOut = mCurrentWriteAccess;
        *barrierDstOut = readAccessType;

        mCurrentReadAccess |= readAccessType;
        return needsBarrier;
    }
    void onReadAccess(CommandGraphResource *reader, VkAccessFlags readAccessType)
    {
        VkAccessFlags barrierSrc, barrierDst;
        if (needsOnReadBarrier(readAccessType, &barrierSrc, &barrierDst))
        {
            reader->addGlobalMemoryBarrier(barrierSrc, barrierDst,
                                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }
    }
    bool needsOnWriteBarrier(VkAccessFlags readAccessType,
                             VkAccessFlags writeAccessType,
                             VkAccessFlags *barrierSrcOut,
                             VkAccessFlags *barrierDstOut);
    void onWriteAccess(ContextVk *contextVk,
                       VkAccessFlags readAccessType,
                       VkAccessFlags writeAccessType);

    // Vulkan objects.
    Buffer mBuffer;
    BufferView mBufferView;
    DeviceMemory mDeviceMemory;

    // Cached properties.
    VkMemoryPropertyFlags mMemoryPropertyFlags;
    VkDeviceSize mSize;
    uint8_t *mMappedMemory;
    const Format *mViewFormat;

    // For memory barriers.
    VkFlags mCurrentWriteAccess;
    VkFlags mCurrentReadAccess;
};

// Imagine an image going through a few layout transitions:
//
//           srcStage 1    dstStage 2          srcStage 2     dstStage 3
//  Layout 1 ------Transition 1-----> Layout 2 ------Transition 2------> Layout 3
//           srcAccess 1  dstAccess 2          srcAccess 2   dstAccess 3
//   \_________________  ___________________/
//                     \/
//               A transition
//
// Every transition requires 6 pieces of information: from/to layouts, src/dst stage masks and
// src/dst access masks.  At the moment we decide to transition the image to Layout 2 (i.e.
// Transition 1), we need to have Layout 1, srcStage 1 and srcAccess 1 stored as history of the
// image.  To perform the transition, we need to know Layout 2, dstStage 2 and dstAccess 2.
// Additionally, we need to know srcStage 2 and srcAccess 2 to retain them for the next transition.
//
// That is, with the history kept, on every new transition we need 5 pieces of new information:
// layout/dstStage/dstAccess to transition into the layout, and srcStage/srcAccess for the future
// transition out from it.  Given the small number of possible combinations of these values, an
// enum is used were each value encapsulates these 5 pieces of information:
//
//                       +--------------------------------+
//           srcStage 1  | dstStage 2          srcStage 2 |   dstStage 3
//  Layout 1 ------Transition 1-----> Layout 2 ------Transition 2------> Layout 3
//           srcAccess 1 |dstAccess 2          srcAccess 2|  dstAccess 3
//                       +---------------  ---------------+
//                                       \/
//                                 One enum value
//
// Note that, while generally dstStage for the to-transition and srcStage for the from-transition
// are the same, they may occasionally be BOTTOM_OF_PIPE and TOP_OF_PIPE respectively.
enum class ImageLayout
{
    Undefined                  = 0,
    ExternalPreInitialized     = 1,
    TransferSrc                = 2,
    TransferDst                = 3,
    ComputeShaderReadOnly      = 4,
    ComputeShaderWrite         = 5,
    AllGraphicsShadersReadOnly = 6,
    AllGraphicsShadersWrite    = 7,
    ColorAttachment            = 8,
    DepthStencilAttachment     = 9,
    Present                    = 10,

    InvalidEnum = 11,
    EnumCount   = 11,
};

class ImageHelper final : public CommandGraphResource
{
  public:
    ImageHelper();
    ImageHelper(ImageHelper &&other);
    ~ImageHelper() override;

    void initStagingBuffer(RendererVk *renderer,
                           const vk::Format &format,
                           VkBufferUsageFlags usageFlags,
                           size_t initialSize);

    angle::Result init(Context *context,
                       gl::TextureType textureType,
                       const VkExtent3D &extents,
                       const Format &format,
                       GLint samples,
                       VkImageUsageFlags usage,
                       uint32_t mipLevels,
                       uint32_t layerCount);
    angle::Result initExternal(Context *context,
                               gl::TextureType textureType,
                               const VkExtent3D &extents,
                               const Format &format,
                               GLint samples,
                               VkImageUsageFlags usage,
                               ImageLayout initialLayout,
                               const void *externalImageCreateInfo,
                               uint32_t mipLevels,
                               uint32_t layerCount);
    angle::Result initMemory(Context *context,
                             const MemoryProperties &memoryProperties,
                             VkMemoryPropertyFlags flags);
    angle::Result initExternalMemory(Context *context,
                                     const MemoryProperties &memoryProperties,
                                     const VkMemoryRequirements &memoryRequirements,
                                     const void *extraAllocationInfo,
                                     uint32_t currentQueueFamilyIndex,
                                     VkMemoryPropertyFlags flags);
    angle::Result initLayerImageView(Context *context,
                                     gl::TextureType textureType,
                                     VkImageAspectFlags aspectMask,
                                     const gl::SwizzleState &swizzleMap,
                                     ImageView *imageViewOut,
                                     uint32_t baseMipLevel,
                                     uint32_t levelCount,
                                     uint32_t baseArrayLayer,
                                     uint32_t layerCount);
    angle::Result initImageView(Context *context,
                                gl::TextureType textureType,
                                VkImageAspectFlags aspectMask,
                                const gl::SwizzleState &swizzleMap,
                                ImageView *imageViewOut,
                                uint32_t baseMipLevel,
                                uint32_t levelCount);
    // Create a 2D[Array] for staging purposes.  Used by:
    //
    // - TextureVk::copySubImageImplWithDraw
    // - FramebufferVk::readPixelsImpl
    //
    angle::Result init2DStaging(Context *context,
                                const MemoryProperties &memoryProperties,
                                const gl::Extents &glExtents,
                                const Format &format,
                                VkImageUsageFlags usage,
                                uint32_t layerCount);

    void releaseImage(ContextVk *contextVk);
    void releaseImage(DisplayVk *display, std::vector<GarbageObjectBase> *garbageQueue);

    void releaseStagingBuffer(ContextVk *contextVk);
    void releaseStagingBuffer(DisplayVk *display, std::vector<GarbageObjectBase> *garbageQueue);

    bool valid() const { return mImage.valid(); }

    VkImageAspectFlags getAspectFlags() const;
    void destroy(VkDevice device);
    void dumpResources(Serial serial, std::vector<GarbageObject> *garbageQueue);

    void init2DWeakReference(VkImage handle,
                             const gl::Extents &glExtents,
                             const Format &format,
                             GLint samples);
    void resetImageWeakReference();

    const Image &getImage() const { return mImage; }
    const DeviceMemory &getDeviceMemory() const { return mDeviceMemory; }

    const VkExtent3D &getExtents() const { return mExtents; }
    uint32_t getLayerCount() const { return mLayerCount; }
    uint32_t getLevelCount() const { return mLevelCount; }
    const Format &getFormat() const { return *mFormat; }
    GLint getSamples() const { return mSamples; }

    VkImageLayout getCurrentLayout() const;

    // Helper function to calculate the extents of a render target created for a certain mip of the
    // image.
    gl::Extents getLevelExtents2D(uint32_t level) const;

    // Clear either color or depth/stencil based on image format.
    void clear(const VkClearValue &value,
               uint32_t mipLevel,
               uint32_t baseArrayLayer,
               uint32_t layerCount,
               vk::CommandBuffer *commandBuffer);

    gl::Extents getSize(const gl::ImageIndex &index) const;

    static void Copy(ImageHelper *srcImage,
                     ImageHelper *dstImage,
                     const gl::Offset &srcOffset,
                     const gl::Offset &dstOffset,
                     const gl::Extents &copySize,
                     const VkImageSubresourceLayers &srcSubresources,
                     const VkImageSubresourceLayers &dstSubresources,
                     vk::CommandBuffer *commandBuffer);

    angle::Result generateMipmapsWithBlit(ContextVk *contextVk, GLuint maxLevel);

    // Resolve this image into a destination image.  This image should be in the TransferSrc layout.
    // The destination image is automatically transitioned into TransferDst.
    void resolve(ImageHelper *dest, const VkImageResolve &region, vk::CommandBuffer *commandBuffer);

    // Data staging
    void removeStagedUpdates(ContextVk *contextVk, const gl::ImageIndex &index);

    angle::Result stageSubresourceUpdate(ContextVk *contextVk,
                                         const gl::ImageIndex &index,
                                         const gl::Extents &glExtents,
                                         const gl::Offset &offset,
                                         const gl::InternalFormat &formatInfo,
                                         const gl::PixelUnpackState &unpack,
                                         GLenum type,
                                         const uint8_t *pixels,
                                         const vk::Format &vkFormat);

    angle::Result stageSubresourceUpdateAndGetData(ContextVk *contextVk,
                                                   size_t allocationSize,
                                                   const gl::ImageIndex &imageIndex,
                                                   const gl::Extents &glExtents,
                                                   const gl::Offset &offset,
                                                   uint8_t **destData);

    angle::Result stageSubresourceUpdateFromFramebuffer(const gl::Context *context,
                                                        const gl::ImageIndex &index,
                                                        const gl::Rectangle &sourceArea,
                                                        const gl::Offset &dstOffset,
                                                        const gl::Extents &dstExtent,
                                                        const gl::InternalFormat &formatInfo,
                                                        FramebufferVk *framebufferVk);

    void stageSubresourceUpdateFromImage(vk::ImageHelper *image,
                                         const gl::ImageIndex &index,
                                         const gl::Offset &destOffset,
                                         const gl::Extents &glExtents,
                                         const VkImageType imageType);

    // Stage a clear operation to a clear value based on WebGL requirements.
    void stageSubresourceRobustClear(const gl::ImageIndex &index, const angle::Format &format);

    // Stage a clear operation to a clear value that initializes emulated channels to the desired
    // values.
    void stageSubresourceEmulatedClear(const gl::ImageIndex &index, const angle::Format &format);

    // If the image has emulated channels, we clear them once so as not to leave garbage on those
    // channels.
    void stageClearIfEmulatedFormat(const gl::ImageIndex &index, const Format &format);

    // This will use the underlying dynamic buffer to allocate some memory to be used as a src or
    // dst.
    angle::Result allocateStagingMemory(ContextVk *contextVk,
                                        size_t sizeInBytes,
                                        uint8_t **ptrOut,
                                        VkBuffer *handleOut,
                                        VkDeviceSize *offsetOut,
                                        bool *newBufferAllocatedOut);

    // Flushes staged updates to a range of levels and layers from start to (but not including) end.
    // Due to the nature of updates (done wholly to a VkImageSubresourceLayers), some unsolicited
    // layers may also be updated.
    angle::Result flushStagedUpdates(ContextVk *contextVk,
                                     uint32_t levelStart,
                                     uint32_t levelEnd,
                                     uint32_t layerStart,
                                     uint32_t layerEnd,
                                     vk::CommandBuffer *commandBuffer);
    // Creates a command buffer and flushes all staged updates.  This is used for one-time
    // initialization of resources that we don't expect to accumulate further staged updates, such
    // as with renderbuffers or surface images.
    angle::Result flushAllStagedUpdates(ContextVk *contextVk);

    bool hasStagedUpdates() const { return !mSubresourceUpdates.empty(); }

    // changeLayout automatically skips the layout change if it's unnecessary.  This function can be
    // used to prevent creating a command graph node and subsequently a command buffer for the sole
    // purpose of performing a transition (which may then not be issued).
    bool isLayoutChangeNecessary(ImageLayout newLayout) const;

    void changeLayout(VkImageAspectFlags aspectMask,
                      ImageLayout newLayout,
                      vk::CommandBuffer *commandBuffer);

    bool isQueueChangeNeccesary(uint32_t newQueueFamilyIndex) const
    {
        return mCurrentQueueFamilyIndex != newQueueFamilyIndex;
    }

    void changeLayoutAndQueue(VkImageAspectFlags aspectMask,
                              ImageLayout newLayout,
                              uint32_t newQueueFamilyIndex,
                              vk::CommandBuffer *commandBuffer);

  private:
    void forceChangeLayoutAndQueue(VkImageAspectFlags aspectMask,
                                   ImageLayout newLayout,
                                   uint32_t newQueueFamilyIndex,
                                   vk::CommandBuffer *commandBuffer);

    void stageSubresourceClear(const gl::ImageIndex &index,
                               const angle::Format &format,
                               const VkClearColorValue &colorValue,
                               const VkClearDepthStencilValue &depthStencilValue);

    void clearColor(const VkClearColorValue &color,
                    uint32_t baseMipLevel,
                    uint32_t levelCount,
                    uint32_t baseArrayLayer,
                    uint32_t layerCount,
                    vk::CommandBuffer *commandBuffer);

    void clearDepthStencil(VkImageAspectFlags imageAspectFlags,
                           VkImageAspectFlags clearAspectFlags,
                           const VkClearDepthStencilValue &depthStencil,
                           uint32_t baseMipLevel,
                           uint32_t levelCount,
                           uint32_t baseArrayLayer,
                           uint32_t layerCount,
                           vk::CommandBuffer *commandBuffer);

    struct SubresourceUpdate
    {
        SubresourceUpdate();
        SubresourceUpdate(VkBuffer bufferHandle, const VkBufferImageCopy &copyRegion);
        SubresourceUpdate(vk::ImageHelper *image, const VkImageCopy &copyRegion);
        SubresourceUpdate(const VkClearValue &clearValue, const gl::ImageIndex &imageIndex);
        SubresourceUpdate(const SubresourceUpdate &other);

        void release(ContextVk *contextVk);
        void release(DisplayVk *display, std::vector<GarbageObjectBase> *garbageQueue);

        const VkImageSubresourceLayers &dstSubresource() const
        {
            ASSERT(updateSource == UpdateSource::Buffer || updateSource == UpdateSource::Image);
            return updateSource == UpdateSource::Buffer ? buffer.copyRegion.imageSubresource
                                                        : image.copyRegion.dstSubresource;
        }
        bool isUpdateToLayerLevel(uint32_t layerIndex, uint32_t levelIndex) const;

        enum class UpdateSource
        {
            Clear,
            Buffer,
            Image,
        };
        struct ClearUpdate
        {
            VkClearValue value;
            uint32_t levelIndex;
            uint32_t layerIndex;
            uint32_t layerCount;
        };
        struct BufferUpdate
        {
            VkBuffer bufferHandle;
            VkBufferImageCopy copyRegion;
        };
        struct ImageUpdate
        {
            vk::ImageHelper *image;
            VkImageCopy copyRegion;
        };

        UpdateSource updateSource;
        union
        {
            ClearUpdate clear;
            BufferUpdate buffer;
            ImageUpdate image;
        };
    };

    // Vulkan objects.
    Image mImage;
    DeviceMemory mDeviceMemory;

    // Image properties.
    VkExtent3D mExtents;
    const Format *mFormat;
    GLint mSamples;

    // Current state.
    ImageLayout mCurrentLayout;
    uint32_t mCurrentQueueFamilyIndex;

    // Cached properties.
    uint32_t mLayerCount;
    uint32_t mLevelCount;

    // Staging buffer
    vk::DynamicBuffer mStagingBuffer;
    std::vector<SubresourceUpdate> mSubresourceUpdates;
};

class FramebufferHelper : public CommandGraphResource
{
  public:
    FramebufferHelper();
    ~FramebufferHelper() override;

    angle::Result init(ContextVk *contextVk, const VkFramebufferCreateInfo &createInfo);
    void release(ContextVk *contextVk);

    bool valid() { return mFramebuffer.valid(); }

    const Framebuffer &getFramebuffer() const
    {
        ASSERT(mFramebuffer.valid());
        return mFramebuffer;
    }

    Framebuffer &getFramebuffer()
    {
        ASSERT(mFramebuffer.valid());
        return mFramebuffer;
    }

  private:
    // Vulkan object.
    Framebuffer mFramebuffer;
};

// A special command graph resource to hold resource dependencies for dispatch calls.  It's the
// equivalent of FramebufferHelper, though it doesn't contain a Vulkan object.
class DispatchHelper : public CommandGraphResource
{
  public:
    DispatchHelper();
    ~DispatchHelper() override;
};

class ShaderProgramHelper : angle::NonCopyable
{
  public:
    ShaderProgramHelper();
    ~ShaderProgramHelper();

    bool valid() const;
    void destroy(VkDevice device);
    void release(ContextVk *contextVk);

    bool isGraphicsProgram() const
    {
        ASSERT(mShaders[gl::ShaderType::Vertex].valid() !=
               mShaders[gl::ShaderType::Compute].valid());
        return mShaders[gl::ShaderType::Vertex].valid();
    }

    vk::ShaderAndSerial &getShader(gl::ShaderType shaderType) { return mShaders[shaderType].get(); }

    void setShader(gl::ShaderType shaderType, RefCounted<ShaderAndSerial> *shader);

    // For getting a vk::Pipeline and from the pipeline cache.
    ANGLE_INLINE angle::Result getGraphicsPipeline(
        Context *context,
        RenderPassCache *renderPassCache,
        const PipelineCache &pipelineCache,
        Serial currentQueueSerial,
        const PipelineLayout &pipelineLayout,
        const GraphicsPipelineDesc &pipelineDesc,
        const gl::AttributesMask &activeAttribLocationsMask,
        const gl::ComponentTypeMask &programAttribsTypeMask,
        const vk::GraphicsPipelineDesc **descPtrOut,
        PipelineHelper **pipelineOut)
    {
        // Pull in a compatible RenderPass.
        vk::RenderPass *compatibleRenderPass = nullptr;
        ANGLE_TRY(renderPassCache->getCompatibleRenderPass(
            context, currentQueueSerial, pipelineDesc.getRenderPassDesc(), &compatibleRenderPass));

        ShaderModule *vertexShader   = &mShaders[gl::ShaderType::Vertex].get().get();
        ShaderModule *fragmentShader = mShaders[gl::ShaderType::Fragment].valid()
                                           ? &mShaders[gl::ShaderType::Fragment].get().get()
                                           : nullptr;

        return mGraphicsPipelines.getPipeline(context, pipelineCache, *compatibleRenderPass,
                                              pipelineLayout, activeAttribLocationsMask,
                                              programAttribsTypeMask, vertexShader, fragmentShader,
                                              pipelineDesc, descPtrOut, pipelineOut);
    }

    angle::Result getComputePipeline(Context *context,
                                     const PipelineLayout &pipelineLayout,
                                     PipelineAndSerial **pipelineOut);

  private:
    gl::ShaderMap<BindingPointer<ShaderAndSerial>> mShaders;
    GraphicsPipelineCache mGraphicsPipelines;

    // We should probably use PipelineHelper here so we can remove PipelineAndSerial.
    PipelineAndSerial mComputePipeline;
};
}  // namespace vk
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VK_HELPERS_H_
