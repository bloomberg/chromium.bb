//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RendererVk.h:
//    Defines the class interface for RendererVk.
//

#ifndef LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_
#define LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_

#include <vulkan/vulkan.h>
#include <memory>

#include "common/angleutils.h"
#include "libANGLE/BlobCache.h"
#include "libANGLE/Caps.h"
#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/FeaturesVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "libANGLE/renderer/vulkan/vk_internal_shaders.h"

namespace egl
{
class AttributeMap;
class BlobCache;
}

namespace rx
{
class DisplayVk;
class FramebufferVk;

namespace vk
{
struct Format;
}

class RendererVk : angle::NonCopyable
{
  public:
    RendererVk();
    ~RendererVk();

    angle::Result initialize(DisplayVk *displayVk,
                             const egl::AttributeMap &attribs,
                             const char *wsiName);
    void onDestroy(vk::Context *context);

    void markDeviceLost();
    bool isDeviceLost() const;

    std::string getVendorString() const;
    std::string getRendererDescription() const;

    VkInstance getInstance() const { return mInstance; }
    VkPhysicalDevice getPhysicalDevice() const { return mPhysicalDevice; }
    const VkPhysicalDeviceProperties &getPhysicalDeviceProperties() const
    {
        return mPhysicalDeviceProperties;
    }
    const VkPhysicalDeviceFeatures &getPhysicalDeviceFeatures() const
    {
        return mPhysicalDeviceFeatures;
    }
    VkQueue getQueue() const { return mQueue; }
    VkDevice getDevice() const { return mDevice; }

    angle::Result selectPresentQueueForSurface(DisplayVk *displayVk,
                                               VkSurfaceKHR surface,
                                               uint32_t *presentQueueOut);

    angle::Result finish(vk::Context *context);
    angle::Result flush(vk::Context *context,
                        const vk::Semaphore &waitSemaphore,
                        const vk::Semaphore &signalSemaphore);

    const vk::CommandPool &getCommandPool() const;

    const gl::Caps &getNativeCaps() const;
    const gl::TextureCapsMap &getNativeTextureCaps() const;
    const gl::Extensions &getNativeExtensions() const;
    const gl::Limitations &getNativeLimitations() const;
    uint32_t getMaxActiveTextures();

    Serial getCurrentQueueSerial() const { return mCurrentQueueSerial; }
    Serial getLastCompletedQueueSerial() const { return mLastCompletedQueueSerial; }

    bool isSerialInUse(Serial serial) const;

    template <typename T>
    void releaseObject(Serial resourceSerial, T *object)
    {
        if (!isSerialInUse(resourceSerial))
        {
            object->destroy(mDevice);
        }
        else
        {
            object->dumpResources(resourceSerial, &mGarbage);
        }
    }

    uint32_t getQueueFamilyIndex() const { return mCurrentQueueFamilyIndex; }

    const vk::MemoryProperties &getMemoryProperties() const { return mMemoryProperties; }

    // TODO(jmadill): We could pass angle::FormatID here.
    const vk::Format &getFormat(GLenum internalFormat) const
    {
        return mFormatTable[internalFormat];
    }

    const vk::Format &getFormat(angle::FormatID formatID) const { return mFormatTable[formatID]; }

    angle::Result getCompatibleRenderPass(vk::Context *context,
                                          const vk::RenderPassDesc &desc,
                                          vk::RenderPass **renderPassOut);
    angle::Result getRenderPassWithOps(vk::Context *context,
                                       const vk::RenderPassDesc &desc,
                                       const vk::AttachmentOpsArray &ops,
                                       vk::RenderPass **renderPassOut);

    // For getting a vk::Pipeline and checking the pipeline cache.
    angle::Result getPipeline(vk::Context *context,
                              const vk::ShaderAndSerial &vertexShader,
                              const vk::ShaderAndSerial &fragmentShader,
                              const vk::PipelineLayout &pipelineLayout,
                              const vk::PipelineDesc &pipelineDesc,
                              const gl::AttributesMask &activeAttribLocationsMask,
                              vk::PipelineAndSerial **pipelineOut);

    // Queries the descriptor set layout cache. Creates the layout if not present.
    angle::Result getDescriptorSetLayout(
        vk::Context *context,
        const vk::DescriptorSetLayoutDesc &desc,
        vk::BindingPointer<vk::DescriptorSetLayout> *descriptorSetLayoutOut);

    // Queries the pipeline layout cache. Creates the layout if not present.
    angle::Result getPipelineLayout(vk::Context *context,
                                    const vk::PipelineLayoutDesc &desc,
                                    const vk::DescriptorSetLayoutPointerArray &descriptorSetLayouts,
                                    vk::BindingPointer<vk::PipelineLayout> *pipelineLayoutOut);

    angle::Result syncPipelineCacheVk(DisplayVk *displayVk);

    // This should only be called from ResourceVk.
    // TODO(jmadill): Keep in ContextVk to enable threaded rendering.
    vk::CommandGraph *getCommandGraph();

    // Issues a new serial for linked shader modules. Used in the pipeline cache.
    Serial issueShaderSerial();

    vk::ShaderLibrary *getShaderLibrary();
    const FeaturesVk &getFeatures() const { return mFeatures; }

  private:
    angle::Result initializeDevice(DisplayVk *displayVk, uint32_t queueFamilyIndex);
    void ensureCapsInitialized() const;
    angle::Result submitFrame(vk::Context *context,
                              const VkSubmitInfo &submitInfo,
                              vk::CommandBuffer &&commandBuffer);
    angle::Result checkInFlightCommands(vk::Context *context);
    void freeAllInFlightResources();
    angle::Result flushCommandGraph(vk::Context *context, vk::CommandBuffer *commandBatch);
    void initFeatures();
    void initPipelineCacheVkKey();
    angle::Result initPipelineCacheVk(DisplayVk *display);

    mutable bool mCapsInitialized;
    mutable gl::Caps mNativeCaps;
    mutable gl::TextureCapsMap mNativeTextureCaps;
    mutable gl::Extensions mNativeExtensions;
    mutable gl::Limitations mNativeLimitations;
    mutable FeaturesVk mFeatures;

    VkInstance mInstance;
    bool mEnableValidationLayers;
    VkDebugReportCallbackEXT mDebugReportCallback;
    VkPhysicalDevice mPhysicalDevice;
    VkPhysicalDeviceProperties mPhysicalDeviceProperties;
    VkPhysicalDeviceFeatures mPhysicalDeviceFeatures;
    std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;
    VkQueue mQueue;
    uint32_t mCurrentQueueFamilyIndex;
    VkDevice mDevice;
    vk::CommandPool mCommandPool;
    SerialFactory mQueueSerialFactory;
    SerialFactory mShaderSerialFactory;
    Serial mLastCompletedQueueSerial;
    Serial mCurrentQueueSerial;

    bool mDeviceLost;

    struct CommandBatch final : angle::NonCopyable
    {
        CommandBatch();
        ~CommandBatch();
        CommandBatch(CommandBatch &&other);
        CommandBatch &operator=(CommandBatch &&other);

        void destroy(VkDevice device);

        vk::CommandPool commandPool;
        vk::Fence fence;
        Serial serial;
    };

    std::vector<CommandBatch> mInFlightCommands;
    std::vector<vk::GarbageObject> mGarbage;
    vk::MemoryProperties mMemoryProperties;
    vk::FormatTable mFormatTable;

    RenderPassCache mRenderPassCache;
    PipelineCache mPipelineCache;

    vk::PipelineCache mPipelineCacheVk;
    egl::BlobCache::Key mPipelineCacheVkBlobKey;
    uint32_t mPipelineCacheVkUpdateTimeout;

    // See CommandGraph.h for a desription of the Command Graph.
    vk::CommandGraph mCommandGraph;

    // ANGLE uses a PipelineLayout cache to store compatible pipeline layouts.
    PipelineLayoutCache mPipelineLayoutCache;

    // DescriptorSetLayouts are also managed in a cache.
    DescriptorSetLayoutCache mDescriptorSetLayoutCache;

    // Internal shader library.
    vk::ShaderLibrary mShaderLibrary;
};

uint32_t GetUniformBufferDescriptorCount();

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_RENDERERVK_H_
