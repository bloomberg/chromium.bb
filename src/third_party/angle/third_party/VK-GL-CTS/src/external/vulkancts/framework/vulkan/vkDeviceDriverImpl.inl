/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

PFN_vkVoidFunction DeviceDriver::getDeviceProcAddr (VkDevice device, const char* pName) const
{
	return m_vk.getDeviceProcAddr(device, pName);
}

void DeviceDriver::destroyDevice (VkDevice device, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyDevice(device, pAllocator);
}

void DeviceDriver::getDeviceQueue (VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) const
{
	m_vk.getDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
}

VkResult DeviceDriver::queueSubmit (VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) const
{
	return m_vk.queueSubmit(queue, submitCount, pSubmits, fence);
}

VkResult DeviceDriver::queueWaitIdle (VkQueue queue) const
{
	return m_vk.queueWaitIdle(queue);
}

VkResult DeviceDriver::deviceWaitIdle (VkDevice device) const
{
	return m_vk.deviceWaitIdle(device);
}

VkResult DeviceDriver::allocateMemory (VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) const
{
	return m_vk.allocateMemory(device, pAllocateInfo, pAllocator, pMemory);
}

void DeviceDriver::freeMemory (VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.freeMemory(device, memory, pAllocator);
}

VkResult DeviceDriver::mapMemory (VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) const
{
	return m_vk.mapMemory(device, memory, offset, size, flags, ppData);
}

void DeviceDriver::unmapMemory (VkDevice device, VkDeviceMemory memory) const
{
	m_vk.unmapMemory(device, memory);
}

VkResult DeviceDriver::flushMappedMemoryRanges (VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) const
{
	return m_vk.flushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

VkResult DeviceDriver::invalidateMappedMemoryRanges (VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) const
{
	return m_vk.invalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

void DeviceDriver::getDeviceMemoryCommitment (VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) const
{
	m_vk.getDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
}

VkResult DeviceDriver::bindBufferMemory (VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) const
{
	return m_vk.bindBufferMemory(device, buffer, memory, memoryOffset);
}

VkResult DeviceDriver::bindImageMemory (VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) const
{
	return m_vk.bindImageMemory(device, image, memory, memoryOffset);
}

void DeviceDriver::getBufferMemoryRequirements (VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) const
{
	m_vk.getBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

void DeviceDriver::getImageMemoryRequirements (VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) const
{
	m_vk.getImageMemoryRequirements(device, image, pMemoryRequirements);
}

void DeviceDriver::getImageSparseMemoryRequirements (VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) const
{
	m_vk.getImageSparseMemoryRequirements(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

VkResult DeviceDriver::queueBindSparse (VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) const
{
	return m_vk.queueBindSparse(queue, bindInfoCount, pBindInfo, fence);
}

VkResult DeviceDriver::createFence (VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) const
{
	return m_vk.createFence(device, pCreateInfo, pAllocator, pFence);
}

void DeviceDriver::destroyFence (VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyFence(device, fence, pAllocator);
}

VkResult DeviceDriver::resetFences (VkDevice device, uint32_t fenceCount, const VkFence* pFences) const
{
	return m_vk.resetFences(device, fenceCount, pFences);
}

VkResult DeviceDriver::getFenceStatus (VkDevice device, VkFence fence) const
{
	return m_vk.getFenceStatus(device, fence);
}

VkResult DeviceDriver::waitForFences (VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) const
{
	return m_vk.waitForFences(device, fenceCount, pFences, waitAll, timeout);
}

VkResult DeviceDriver::createSemaphore (VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) const
{
	return m_vk.createSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
}

void DeviceDriver::destroySemaphore (VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroySemaphore(device, semaphore, pAllocator);
}

VkResult DeviceDriver::createEvent (VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) const
{
	return m_vk.createEvent(device, pCreateInfo, pAllocator, pEvent);
}

void DeviceDriver::destroyEvent (VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyEvent(device, event, pAllocator);
}

VkResult DeviceDriver::getEventStatus (VkDevice device, VkEvent event) const
{
	return m_vk.getEventStatus(device, event);
}

VkResult DeviceDriver::setEvent (VkDevice device, VkEvent event) const
{
	return m_vk.setEvent(device, event);
}

VkResult DeviceDriver::resetEvent (VkDevice device, VkEvent event) const
{
	return m_vk.resetEvent(device, event);
}

VkResult DeviceDriver::createQueryPool (VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) const
{
	return m_vk.createQueryPool(device, pCreateInfo, pAllocator, pQueryPool);
}

void DeviceDriver::destroyQueryPool (VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyQueryPool(device, queryPool, pAllocator);
}

VkResult DeviceDriver::getQueryPoolResults (VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) const
{
	return m_vk.getQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
}

VkResult DeviceDriver::createBuffer (VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) const
{
	return m_vk.createBuffer(device, pCreateInfo, pAllocator, pBuffer);
}

void DeviceDriver::destroyBuffer (VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyBuffer(device, buffer, pAllocator);
}

VkResult DeviceDriver::createBufferView (VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) const
{
	return m_vk.createBufferView(device, pCreateInfo, pAllocator, pView);
}

void DeviceDriver::destroyBufferView (VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyBufferView(device, bufferView, pAllocator);
}

VkResult DeviceDriver::createImage (VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) const
{
	return m_vk.createImage(device, pCreateInfo, pAllocator, pImage);
}

void DeviceDriver::destroyImage (VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyImage(device, image, pAllocator);
}

void DeviceDriver::getImageSubresourceLayout (VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) const
{
	m_vk.getImageSubresourceLayout(device, image, pSubresource, pLayout);
}

VkResult DeviceDriver::createImageView (VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) const
{
	return m_vk.createImageView(device, pCreateInfo, pAllocator, pView);
}

void DeviceDriver::destroyImageView (VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyImageView(device, imageView, pAllocator);
}

VkResult DeviceDriver::createShaderModule (VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) const
{
	return m_vk.createShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
}

void DeviceDriver::destroyShaderModule (VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyShaderModule(device, shaderModule, pAllocator);
}

VkResult DeviceDriver::createPipelineCache (VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) const
{
	return m_vk.createPipelineCache(device, pCreateInfo, pAllocator, pPipelineCache);
}

void DeviceDriver::destroyPipelineCache (VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyPipelineCache(device, pipelineCache, pAllocator);
}

VkResult DeviceDriver::getPipelineCacheData (VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) const
{
	return m_vk.getPipelineCacheData(device, pipelineCache, pDataSize, pData);
}

VkResult DeviceDriver::mergePipelineCaches (VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) const
{
	return m_vk.mergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches);
}

VkResult DeviceDriver::createGraphicsPipelines (VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) const
{
	return m_vk.createGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

VkResult DeviceDriver::createComputePipelines (VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) const
{
	return m_vk.createComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

void DeviceDriver::destroyPipeline (VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyPipeline(device, pipeline, pAllocator);
}

VkResult DeviceDriver::createPipelineLayout (VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) const
{
	return m_vk.createPipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
}

void DeviceDriver::destroyPipelineLayout (VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyPipelineLayout(device, pipelineLayout, pAllocator);
}

VkResult DeviceDriver::createSampler (VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) const
{
	return m_vk.createSampler(device, pCreateInfo, pAllocator, pSampler);
}

void DeviceDriver::destroySampler (VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroySampler(device, sampler, pAllocator);
}

VkResult DeviceDriver::createDescriptorSetLayout (VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) const
{
	return m_vk.createDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
}

void DeviceDriver::destroyDescriptorSetLayout (VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
}

VkResult DeviceDriver::createDescriptorPool (VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) const
{
	return m_vk.createDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
}

void DeviceDriver::destroyDescriptorPool (VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyDescriptorPool(device, descriptorPool, pAllocator);
}

VkResult DeviceDriver::resetDescriptorPool (VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) const
{
	return m_vk.resetDescriptorPool(device, descriptorPool, flags);
}

VkResult DeviceDriver::allocateDescriptorSets (VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) const
{
	return m_vk.allocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
}

VkResult DeviceDriver::freeDescriptorSets (VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) const
{
	return m_vk.freeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
}

void DeviceDriver::updateDescriptorSets (VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) const
{
	m_vk.updateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

VkResult DeviceDriver::createFramebuffer (VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) const
{
	return m_vk.createFramebuffer(device, pCreateInfo, pAllocator, pFramebuffer);
}

void DeviceDriver::destroyFramebuffer (VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyFramebuffer(device, framebuffer, pAllocator);
}

VkResult DeviceDriver::createRenderPass (VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) const
{
	return m_vk.createRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
}

void DeviceDriver::destroyRenderPass (VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyRenderPass(device, renderPass, pAllocator);
}

void DeviceDriver::getRenderAreaGranularity (VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) const
{
	m_vk.getRenderAreaGranularity(device, renderPass, pGranularity);
}

VkResult DeviceDriver::createCommandPool (VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) const
{
	return m_vk.createCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
}

void DeviceDriver::destroyCommandPool (VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyCommandPool(device, commandPool, pAllocator);
}

VkResult DeviceDriver::resetCommandPool (VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) const
{
	return m_vk.resetCommandPool(device, commandPool, flags);
}

VkResult DeviceDriver::allocateCommandBuffers (VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) const
{
	return m_vk.allocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
}

void DeviceDriver::freeCommandBuffers (VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) const
{
	m_vk.freeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
}

VkResult DeviceDriver::beginCommandBuffer (VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) const
{
	return m_vk.beginCommandBuffer(commandBuffer, pBeginInfo);
}

VkResult DeviceDriver::endCommandBuffer (VkCommandBuffer commandBuffer) const
{
	return m_vk.endCommandBuffer(commandBuffer);
}

VkResult DeviceDriver::resetCommandBuffer (VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) const
{
	return m_vk.resetCommandBuffer(commandBuffer, flags);
}

void DeviceDriver::cmdBindPipeline (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) const
{
	m_vk.cmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

void DeviceDriver::cmdSetViewport (VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) const
{
	m_vk.cmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
}

void DeviceDriver::cmdSetScissor (VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) const
{
	m_vk.cmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
}

void DeviceDriver::cmdSetLineWidth (VkCommandBuffer commandBuffer, float lineWidth) const
{
	m_vk.cmdSetLineWidth(commandBuffer, lineWidth);
}

void DeviceDriver::cmdSetDepthBias (VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) const
{
	m_vk.cmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

void DeviceDriver::cmdSetBlendConstants (VkCommandBuffer commandBuffer, const float blendConstants[4]) const
{
	m_vk.cmdSetBlendConstants(commandBuffer, blendConstants);
}

void DeviceDriver::cmdSetDepthBounds (VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) const
{
	m_vk.cmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
}

void DeviceDriver::cmdSetStencilCompareMask (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) const
{
	m_vk.cmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
}

void DeviceDriver::cmdSetStencilWriteMask (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) const
{
	m_vk.cmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
}

void DeviceDriver::cmdSetStencilReference (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) const
{
	m_vk.cmdSetStencilReference(commandBuffer, faceMask, reference);
}

void DeviceDriver::cmdBindDescriptorSets (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) const
{
	m_vk.cmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}

void DeviceDriver::cmdBindIndexBuffer (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) const
{
	m_vk.cmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

void DeviceDriver::cmdBindVertexBuffers (VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) const
{
	m_vk.cmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

void DeviceDriver::cmdDraw (VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const
{
	m_vk.cmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void DeviceDriver::cmdDrawIndexed (VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) const
{
	m_vk.cmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void DeviceDriver::cmdDrawIndirect (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) const
{
	m_vk.cmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

void DeviceDriver::cmdDrawIndexedIndirect (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) const
{
	m_vk.cmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

void DeviceDriver::cmdDispatch (VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const
{
	m_vk.cmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void DeviceDriver::cmdDispatchIndirect (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) const
{
	m_vk.cmdDispatchIndirect(commandBuffer, buffer, offset);
}

void DeviceDriver::cmdCopyBuffer (VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) const
{
	m_vk.cmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

void DeviceDriver::cmdCopyImage (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) const
{
	m_vk.cmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

void DeviceDriver::cmdBlitImage (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) const
{
	m_vk.cmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}

void DeviceDriver::cmdCopyBufferToImage (VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) const
{
	m_vk.cmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

void DeviceDriver::cmdCopyImageToBuffer (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) const
{
	m_vk.cmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}

void DeviceDriver::cmdUpdateBuffer (VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) const
{
	m_vk.cmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
}

void DeviceDriver::cmdFillBuffer (VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) const
{
	m_vk.cmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
}

void DeviceDriver::cmdClearColorImage (VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) const
{
	m_vk.cmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

void DeviceDriver::cmdClearDepthStencilImage (VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) const
{
	m_vk.cmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

void DeviceDriver::cmdClearAttachments (VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) const
{
	m_vk.cmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}

void DeviceDriver::cmdResolveImage (VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) const
{
	m_vk.cmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

void DeviceDriver::cmdSetEvent (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) const
{
	m_vk.cmdSetEvent(commandBuffer, event, stageMask);
}

void DeviceDriver::cmdResetEvent (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) const
{
	m_vk.cmdResetEvent(commandBuffer, event, stageMask);
}

void DeviceDriver::cmdWaitEvents (VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) const
{
	m_vk.cmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

void DeviceDriver::cmdPipelineBarrier (VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) const
{
	m_vk.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}

void DeviceDriver::cmdBeginQuery (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) const
{
	m_vk.cmdBeginQuery(commandBuffer, queryPool, query, flags);
}

void DeviceDriver::cmdEndQuery (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) const
{
	m_vk.cmdEndQuery(commandBuffer, queryPool, query);
}

void DeviceDriver::cmdResetQueryPool (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) const
{
	m_vk.cmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
}

void DeviceDriver::cmdWriteTimestamp (VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) const
{
	m_vk.cmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
}

void DeviceDriver::cmdCopyQueryPoolResults (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) const
{
	m_vk.cmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}

void DeviceDriver::cmdPushConstants (VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) const
{
	m_vk.cmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
}

void DeviceDriver::cmdBeginRenderPass (VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) const
{
	m_vk.cmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

void DeviceDriver::cmdNextSubpass (VkCommandBuffer commandBuffer, VkSubpassContents contents) const
{
	m_vk.cmdNextSubpass(commandBuffer, contents);
}

void DeviceDriver::cmdEndRenderPass (VkCommandBuffer commandBuffer) const
{
	m_vk.cmdEndRenderPass(commandBuffer);
}

void DeviceDriver::cmdExecuteCommands (VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) const
{
	m_vk.cmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
}

VkResult DeviceDriver::bindBufferMemory2 (VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) const
{
	return m_vk.bindBufferMemory2(device, bindInfoCount, pBindInfos);
}

VkResult DeviceDriver::bindImageMemory2 (VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) const
{
	return m_vk.bindImageMemory2(device, bindInfoCount, pBindInfos);
}

void DeviceDriver::getDeviceGroupPeerMemoryFeatures (VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) const
{
	m_vk.getDeviceGroupPeerMemoryFeatures(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
}

void DeviceDriver::cmdSetDeviceMask (VkCommandBuffer commandBuffer, uint32_t deviceMask) const
{
	m_vk.cmdSetDeviceMask(commandBuffer, deviceMask);
}

void DeviceDriver::cmdDispatchBase (VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const
{
	m_vk.cmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
}

void DeviceDriver::getImageMemoryRequirements2 (VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) const
{
	m_vk.getImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
}

void DeviceDriver::getBufferMemoryRequirements2 (VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) const
{
	m_vk.getBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
}

void DeviceDriver::getImageSparseMemoryRequirements2 (VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) const
{
	m_vk.getImageSparseMemoryRequirements2(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

void DeviceDriver::trimCommandPool (VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) const
{
	m_vk.trimCommandPool(device, commandPool, flags);
}

void DeviceDriver::getDeviceQueue2 (VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) const
{
	m_vk.getDeviceQueue2(device, pQueueInfo, pQueue);
}

VkResult DeviceDriver::createSamplerYcbcrConversion (VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) const
{
	return m_vk.createSamplerYcbcrConversion(device, pCreateInfo, pAllocator, pYcbcrConversion);
}

void DeviceDriver::destroySamplerYcbcrConversion (VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroySamplerYcbcrConversion(device, ycbcrConversion, pAllocator);
}

VkResult DeviceDriver::createDescriptorUpdateTemplate (VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) const
{
	return m_vk.createDescriptorUpdateTemplate(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
}

void DeviceDriver::destroyDescriptorUpdateTemplate (VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, pAllocator);
}

void DeviceDriver::updateDescriptorSetWithTemplate (VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) const
{
	m_vk.updateDescriptorSetWithTemplate(device, descriptorSet, descriptorUpdateTemplate, pData);
}

void DeviceDriver::getDescriptorSetLayoutSupport (VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) const
{
	m_vk.getDescriptorSetLayoutSupport(device, pCreateInfo, pSupport);
}

void DeviceDriver::cmdDrawIndirectCount (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) const
{
	m_vk.cmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

void DeviceDriver::cmdDrawIndexedIndirectCount (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) const
{
	m_vk.cmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

VkResult DeviceDriver::createRenderPass2 (VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) const
{
	return m_vk.createRenderPass2(device, pCreateInfo, pAllocator, pRenderPass);
}

void DeviceDriver::cmdBeginRenderPass2 (VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) const
{
	m_vk.cmdBeginRenderPass2(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
}

void DeviceDriver::cmdNextSubpass2 (VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) const
{
	m_vk.cmdNextSubpass2(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}

void DeviceDriver::cmdEndRenderPass2 (VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) const
{
	m_vk.cmdEndRenderPass2(commandBuffer, pSubpassEndInfo);
}

void DeviceDriver::resetQueryPool (VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) const
{
	m_vk.resetQueryPool(device, queryPool, firstQuery, queryCount);
}

VkResult DeviceDriver::getSemaphoreCounterValue (VkDevice device, VkSemaphore semaphore, uint64_t* pValue) const
{
	return m_vk.getSemaphoreCounterValue(device, semaphore, pValue);
}

VkResult DeviceDriver::waitSemaphores (VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) const
{
	return m_vk.waitSemaphores(device, pWaitInfo, timeout);
}

VkResult DeviceDriver::signalSemaphore (VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) const
{
	return m_vk.signalSemaphore(device, pSignalInfo);
}

VkDeviceAddress DeviceDriver::getBufferDeviceAddress (VkDevice device, const VkBufferDeviceAddressInfo* pInfo) const
{
	return m_vk.getBufferDeviceAddress(device, pInfo);
}

uint64_t DeviceDriver::getBufferOpaqueCaptureAddress (VkDevice device, const VkBufferDeviceAddressInfo* pInfo) const
{
	return m_vk.getBufferOpaqueCaptureAddress(device, pInfo);
}

uint64_t DeviceDriver::getDeviceMemoryOpaqueCaptureAddress (VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) const
{
	return m_vk.getDeviceMemoryOpaqueCaptureAddress(device, pInfo);
}

VkResult DeviceDriver::createSwapchainKHR (VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) const
{
	return m_vk.createSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
}

void DeviceDriver::destroySwapchainKHR (VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroySwapchainKHR(device, swapchain, pAllocator);
}

VkResult DeviceDriver::getSwapchainImagesKHR (VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) const
{
	return m_vk.getSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}

VkResult DeviceDriver::acquireNextImageKHR (VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) const
{
	return m_vk.acquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

VkResult DeviceDriver::queuePresentKHR (VkQueue queue, const VkPresentInfoKHR* pPresentInfo) const
{
	return m_vk.queuePresentKHR(queue, pPresentInfo);
}

VkResult DeviceDriver::getDeviceGroupPresentCapabilitiesKHR (VkDevice device, VkDeviceGroupPresentCapabilitiesKHR* pDeviceGroupPresentCapabilities) const
{
	return m_vk.getDeviceGroupPresentCapabilitiesKHR(device, pDeviceGroupPresentCapabilities);
}

VkResult DeviceDriver::getDeviceGroupSurfacePresentModesKHR (VkDevice device, VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR* pModes) const
{
	return m_vk.getDeviceGroupSurfacePresentModesKHR(device, surface, pModes);
}

VkResult DeviceDriver::acquireNextImage2KHR (VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo, uint32_t* pImageIndex) const
{
	return m_vk.acquireNextImage2KHR(device, pAcquireInfo, pImageIndex);
}

VkResult DeviceDriver::createSharedSwapchainsKHR (VkDevice device, uint32_t swapchainCount, const VkSwapchainCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchains) const
{
	return m_vk.createSharedSwapchainsKHR(device, swapchainCount, pCreateInfos, pAllocator, pSwapchains);
}

void DeviceDriver::cmdBeginRenderingKHR (VkCommandBuffer commandBuffer, const VkRenderingInfoKHR* pRenderingInfo) const
{
	m_vk.cmdBeginRenderingKHR(commandBuffer, pRenderingInfo);
}

void DeviceDriver::cmdEndRenderingKHR (VkCommandBuffer commandBuffer) const
{
	m_vk.cmdEndRenderingKHR(commandBuffer);
}

VkResult DeviceDriver::getMemoryFdKHR (VkDevice device, const VkMemoryGetFdInfoKHR* pGetFdInfo, int* pFd) const
{
	return m_vk.getMemoryFdKHR(device, pGetFdInfo, pFd);
}

VkResult DeviceDriver::getMemoryFdPropertiesKHR (VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, int fd, VkMemoryFdPropertiesKHR* pMemoryFdProperties) const
{
	return m_vk.getMemoryFdPropertiesKHR(device, handleType, fd, pMemoryFdProperties);
}

VkResult DeviceDriver::importSemaphoreFdKHR (VkDevice device, const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) const
{
	return m_vk.importSemaphoreFdKHR(device, pImportSemaphoreFdInfo);
}

VkResult DeviceDriver::getSemaphoreFdKHR (VkDevice device, const VkSemaphoreGetFdInfoKHR* pGetFdInfo, int* pFd) const
{
	return m_vk.getSemaphoreFdKHR(device, pGetFdInfo, pFd);
}

void DeviceDriver::cmdPushDescriptorSetKHR (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) const
{
	m_vk.cmdPushDescriptorSetKHR(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites);
}

void DeviceDriver::cmdPushDescriptorSetWithTemplateKHR (VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData) const
{
	m_vk.cmdPushDescriptorSetWithTemplateKHR(commandBuffer, descriptorUpdateTemplate, layout, set, pData);
}

VkResult DeviceDriver::getSwapchainStatusKHR (VkDevice device, VkSwapchainKHR swapchain) const
{
	return m_vk.getSwapchainStatusKHR(device, swapchain);
}

VkResult DeviceDriver::importFenceFdKHR (VkDevice device, const VkImportFenceFdInfoKHR* pImportFenceFdInfo) const
{
	return m_vk.importFenceFdKHR(device, pImportFenceFdInfo);
}

VkResult DeviceDriver::getFenceFdKHR (VkDevice device, const VkFenceGetFdInfoKHR* pGetFdInfo, int* pFd) const
{
	return m_vk.getFenceFdKHR(device, pGetFdInfo, pFd);
}

VkResult DeviceDriver::acquireProfilingLockKHR (VkDevice device, const VkAcquireProfilingLockInfoKHR* pInfo) const
{
	return m_vk.acquireProfilingLockKHR(device, pInfo);
}

void DeviceDriver::releaseProfilingLockKHR (VkDevice device) const
{
	m_vk.releaseProfilingLockKHR(device);
}

void DeviceDriver::cmdSetFragmentShadingRateKHR (VkCommandBuffer commandBuffer, const VkExtent2D* pFragmentSize, const VkFragmentShadingRateCombinerOpKHR combinerOps[2]) const
{
	m_vk.cmdSetFragmentShadingRateKHR(commandBuffer, pFragmentSize, combinerOps);
}

VkResult DeviceDriver::waitForPresentKHR (VkDevice device, VkSwapchainKHR swapchain, uint64_t presentId, uint64_t timeout) const
{
	return m_vk.waitForPresentKHR(device, swapchain, presentId, timeout);
}

VkResult DeviceDriver::createDeferredOperationKHR (VkDevice device, const VkAllocationCallbacks* pAllocator, VkDeferredOperationKHR* pDeferredOperation) const
{
	return m_vk.createDeferredOperationKHR(device, pAllocator, pDeferredOperation);
}

void DeviceDriver::destroyDeferredOperationKHR (VkDevice device, VkDeferredOperationKHR operation, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyDeferredOperationKHR(device, operation, pAllocator);
}

uint32_t DeviceDriver::getDeferredOperationMaxConcurrencyKHR (VkDevice device, VkDeferredOperationKHR operation) const
{
	return m_vk.getDeferredOperationMaxConcurrencyKHR(device, operation);
}

VkResult DeviceDriver::getDeferredOperationResultKHR (VkDevice device, VkDeferredOperationKHR operation) const
{
	return m_vk.getDeferredOperationResultKHR(device, operation);
}

VkResult DeviceDriver::deferredOperationJoinKHR (VkDevice device, VkDeferredOperationKHR operation) const
{
	return m_vk.deferredOperationJoinKHR(device, operation);
}

VkResult DeviceDriver::getPipelineExecutablePropertiesKHR (VkDevice device, const VkPipelineInfoKHR* pPipelineInfo, uint32_t* pExecutableCount, VkPipelineExecutablePropertiesKHR* pProperties) const
{
	return m_vk.getPipelineExecutablePropertiesKHR(device, pPipelineInfo, pExecutableCount, pProperties);
}

VkResult DeviceDriver::getPipelineExecutableStatisticsKHR (VkDevice device, const VkPipelineExecutableInfoKHR* pExecutableInfo, uint32_t* pStatisticCount, VkPipelineExecutableStatisticKHR* pStatistics) const
{
	return m_vk.getPipelineExecutableStatisticsKHR(device, pExecutableInfo, pStatisticCount, pStatistics);
}

VkResult DeviceDriver::getPipelineExecutableInternalRepresentationsKHR (VkDevice device, const VkPipelineExecutableInfoKHR* pExecutableInfo, uint32_t* pInternalRepresentationCount, VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations) const
{
	return m_vk.getPipelineExecutableInternalRepresentationsKHR(device, pExecutableInfo, pInternalRepresentationCount, pInternalRepresentations);
}

void DeviceDriver::cmdSetEvent2KHR (VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfoKHR* pDependencyInfo) const
{
	m_vk.cmdSetEvent2KHR(commandBuffer, event, pDependencyInfo);
}

void DeviceDriver::cmdResetEvent2KHR (VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2KHR stageMask) const
{
	m_vk.cmdResetEvent2KHR(commandBuffer, event, stageMask);
}

void DeviceDriver::cmdWaitEvents2KHR (VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfoKHR* pDependencyInfos) const
{
	m_vk.cmdWaitEvents2KHR(commandBuffer, eventCount, pEvents, pDependencyInfos);
}

void DeviceDriver::cmdPipelineBarrier2KHR (VkCommandBuffer commandBuffer, const VkDependencyInfoKHR* pDependencyInfo) const
{
	m_vk.cmdPipelineBarrier2KHR(commandBuffer, pDependencyInfo);
}

void DeviceDriver::cmdWriteTimestamp2KHR (VkCommandBuffer commandBuffer, VkPipelineStageFlags2KHR stage, VkQueryPool queryPool, uint32_t query) const
{
	m_vk.cmdWriteTimestamp2KHR(commandBuffer, stage, queryPool, query);
}

VkResult DeviceDriver::queueSubmit2KHR (VkQueue queue, uint32_t submitCount, const VkSubmitInfo2KHR* pSubmits, VkFence fence) const
{
	return m_vk.queueSubmit2KHR(queue, submitCount, pSubmits, fence);
}

void DeviceDriver::cmdWriteBufferMarker2AMD (VkCommandBuffer commandBuffer, VkPipelineStageFlags2KHR stage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker) const
{
	m_vk.cmdWriteBufferMarker2AMD(commandBuffer, stage, dstBuffer, dstOffset, marker);
}

void DeviceDriver::getQueueCheckpointData2NV (VkQueue queue, uint32_t* pCheckpointDataCount, VkCheckpointData2NV* pCheckpointData) const
{
	m_vk.getQueueCheckpointData2NV(queue, pCheckpointDataCount, pCheckpointData);
}

void DeviceDriver::cmdCopyBuffer2KHR (VkCommandBuffer commandBuffer, const VkCopyBufferInfo2KHR* pCopyBufferInfo) const
{
	m_vk.cmdCopyBuffer2KHR(commandBuffer, pCopyBufferInfo);
}

void DeviceDriver::cmdCopyImage2KHR (VkCommandBuffer commandBuffer, const VkCopyImageInfo2KHR* pCopyImageInfo) const
{
	m_vk.cmdCopyImage2KHR(commandBuffer, pCopyImageInfo);
}

void DeviceDriver::cmdCopyBufferToImage2KHR (VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2KHR* pCopyBufferToImageInfo) const
{
	m_vk.cmdCopyBufferToImage2KHR(commandBuffer, pCopyBufferToImageInfo);
}

void DeviceDriver::cmdCopyImageToBuffer2KHR (VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2KHR* pCopyImageToBufferInfo) const
{
	m_vk.cmdCopyImageToBuffer2KHR(commandBuffer, pCopyImageToBufferInfo);
}

void DeviceDriver::cmdBlitImage2KHR (VkCommandBuffer commandBuffer, const VkBlitImageInfo2KHR* pBlitImageInfo) const
{
	m_vk.cmdBlitImage2KHR(commandBuffer, pBlitImageInfo);
}

void DeviceDriver::cmdResolveImage2KHR (VkCommandBuffer commandBuffer, const VkResolveImageInfo2KHR* pResolveImageInfo) const
{
	m_vk.cmdResolveImage2KHR(commandBuffer, pResolveImageInfo);
}

void DeviceDriver::getDeviceBufferMemoryRequirementsKHR (VkDevice device, const VkDeviceBufferMemoryRequirementsKHR* pInfo, VkMemoryRequirements2* pMemoryRequirements) const
{
	m_vk.getDeviceBufferMemoryRequirementsKHR(device, pInfo, pMemoryRequirements);
}

void DeviceDriver::getDeviceImageMemoryRequirementsKHR (VkDevice device, const VkDeviceImageMemoryRequirementsKHR* pInfo, VkMemoryRequirements2* pMemoryRequirements) const
{
	m_vk.getDeviceImageMemoryRequirementsKHR(device, pInfo, pMemoryRequirements);
}

void DeviceDriver::getDeviceImageSparseMemoryRequirementsKHR (VkDevice device, const VkDeviceImageMemoryRequirementsKHR* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) const
{
	m_vk.getDeviceImageSparseMemoryRequirementsKHR(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

VkResult DeviceDriver::debugMarkerSetObjectTagEXT (VkDevice device, const VkDebugMarkerObjectTagInfoEXT* pTagInfo) const
{
	return m_vk.debugMarkerSetObjectTagEXT(device, pTagInfo);
}

VkResult DeviceDriver::debugMarkerSetObjectNameEXT (VkDevice device, const VkDebugMarkerObjectNameInfoEXT* pNameInfo) const
{
	return m_vk.debugMarkerSetObjectNameEXT(device, pNameInfo);
}

void DeviceDriver::cmdDebugMarkerBeginEXT (VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo) const
{
	m_vk.cmdDebugMarkerBeginEXT(commandBuffer, pMarkerInfo);
}

void DeviceDriver::cmdDebugMarkerEndEXT (VkCommandBuffer commandBuffer) const
{
	m_vk.cmdDebugMarkerEndEXT(commandBuffer);
}

void DeviceDriver::cmdDebugMarkerInsertEXT (VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo) const
{
	m_vk.cmdDebugMarkerInsertEXT(commandBuffer, pMarkerInfo);
}

void DeviceDriver::cmdBindTransformFeedbackBuffersEXT (VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes) const
{
	m_vk.cmdBindTransformFeedbackBuffersEXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
}

void DeviceDriver::cmdBeginTransformFeedbackEXT (VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) const
{
	m_vk.cmdBeginTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

void DeviceDriver::cmdEndTransformFeedbackEXT (VkCommandBuffer commandBuffer, uint32_t firstCounterBuffer, uint32_t counterBufferCount, const VkBuffer* pCounterBuffers, const VkDeviceSize* pCounterBufferOffsets) const
{
	m_vk.cmdEndTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets);
}

void DeviceDriver::cmdBeginQueryIndexedEXT (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags, uint32_t index) const
{
	m_vk.cmdBeginQueryIndexedEXT(commandBuffer, queryPool, query, flags, index);
}

void DeviceDriver::cmdEndQueryIndexedEXT (VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, uint32_t index) const
{
	m_vk.cmdEndQueryIndexedEXT(commandBuffer, queryPool, query, index);
}

void DeviceDriver::cmdDrawIndirectByteCountEXT (VkCommandBuffer commandBuffer, uint32_t instanceCount, uint32_t firstInstance, VkBuffer counterBuffer, VkDeviceSize counterBufferOffset, uint32_t counterOffset, uint32_t vertexStride) const
{
	m_vk.cmdDrawIndirectByteCountEXT(commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride);
}

VkResult DeviceDriver::createCuModuleNVX (VkDevice device, const VkCuModuleCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCuModuleNVX* pModule) const
{
	return m_vk.createCuModuleNVX(device, pCreateInfo, pAllocator, pModule);
}

VkResult DeviceDriver::createCuFunctionNVX (VkDevice device, const VkCuFunctionCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCuFunctionNVX* pFunction) const
{
	return m_vk.createCuFunctionNVX(device, pCreateInfo, pAllocator, pFunction);
}

void DeviceDriver::destroyCuModuleNVX (VkDevice device, VkCuModuleNVX module, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyCuModuleNVX(device, module, pAllocator);
}

void DeviceDriver::destroyCuFunctionNVX (VkDevice device, VkCuFunctionNVX function, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyCuFunctionNVX(device, function, pAllocator);
}

void DeviceDriver::cmdCuLaunchKernelNVX (VkCommandBuffer commandBuffer, const VkCuLaunchInfoNVX* pLaunchInfo) const
{
	m_vk.cmdCuLaunchKernelNVX(commandBuffer, pLaunchInfo);
}

uint32_t DeviceDriver::getImageViewHandleNVX (VkDevice device, const VkImageViewHandleInfoNVX* pInfo) const
{
	return m_vk.getImageViewHandleNVX(device, pInfo);
}

VkResult DeviceDriver::getImageViewAddressNVX (VkDevice device, VkImageView imageView, VkImageViewAddressPropertiesNVX* pProperties) const
{
	return m_vk.getImageViewAddressNVX(device, imageView, pProperties);
}

void DeviceDriver::cmdDrawIndirectCountAMD (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) const
{
	m_vk.cmdDrawIndirectCountAMD(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

void DeviceDriver::cmdDrawIndexedIndirectCountAMD (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) const
{
	m_vk.cmdDrawIndexedIndirectCountAMD(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

VkResult DeviceDriver::getShaderInfoAMD (VkDevice device, VkPipeline pipeline, VkShaderStageFlagBits shaderStage, VkShaderInfoTypeAMD infoType, size_t* pInfoSize, void* pInfo) const
{
	return m_vk.getShaderInfoAMD(device, pipeline, shaderStage, infoType, pInfoSize, pInfo);
}

void DeviceDriver::cmdBeginConditionalRenderingEXT (VkCommandBuffer commandBuffer, const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) const
{
	m_vk.cmdBeginConditionalRenderingEXT(commandBuffer, pConditionalRenderingBegin);
}

void DeviceDriver::cmdEndConditionalRenderingEXT (VkCommandBuffer commandBuffer) const
{
	m_vk.cmdEndConditionalRenderingEXT(commandBuffer);
}

void DeviceDriver::cmdSetViewportWScalingNV (VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportWScalingNV* pViewportWScalings) const
{
	m_vk.cmdSetViewportWScalingNV(commandBuffer, firstViewport, viewportCount, pViewportWScalings);
}

VkResult DeviceDriver::displayPowerControlEXT (VkDevice device, VkDisplayKHR display, const VkDisplayPowerInfoEXT* pDisplayPowerInfo) const
{
	return m_vk.displayPowerControlEXT(device, display, pDisplayPowerInfo);
}

VkResult DeviceDriver::registerDeviceEventEXT (VkDevice device, const VkDeviceEventInfoEXT* pDeviceEventInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) const
{
	return m_vk.registerDeviceEventEXT(device, pDeviceEventInfo, pAllocator, pFence);
}

VkResult DeviceDriver::registerDisplayEventEXT (VkDevice device, VkDisplayKHR display, const VkDisplayEventInfoEXT* pDisplayEventInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) const
{
	return m_vk.registerDisplayEventEXT(device, display, pDisplayEventInfo, pAllocator, pFence);
}

VkResult DeviceDriver::getSwapchainCounterEXT (VkDevice device, VkSwapchainKHR swapchain, VkSurfaceCounterFlagBitsEXT counter, uint64_t* pCounterValue) const
{
	return m_vk.getSwapchainCounterEXT(device, swapchain, counter, pCounterValue);
}

VkResult DeviceDriver::getRefreshCycleDurationGOOGLE (VkDevice device, VkSwapchainKHR swapchain, VkRefreshCycleDurationGOOGLE* pDisplayTimingProperties) const
{
	return m_vk.getRefreshCycleDurationGOOGLE(device, swapchain, pDisplayTimingProperties);
}

VkResult DeviceDriver::getPastPresentationTimingGOOGLE (VkDevice device, VkSwapchainKHR swapchain, uint32_t* pPresentationTimingCount, VkPastPresentationTimingGOOGLE* pPresentationTimings) const
{
	return m_vk.getPastPresentationTimingGOOGLE(device, swapchain, pPresentationTimingCount, pPresentationTimings);
}

void DeviceDriver::cmdSetDiscardRectangleEXT (VkCommandBuffer commandBuffer, uint32_t firstDiscardRectangle, uint32_t discardRectangleCount, const VkRect2D* pDiscardRectangles) const
{
	m_vk.cmdSetDiscardRectangleEXT(commandBuffer, firstDiscardRectangle, discardRectangleCount, pDiscardRectangles);
}

void DeviceDriver::setHdrMetadataEXT (VkDevice device, uint32_t swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata) const
{
	m_vk.setHdrMetadataEXT(device, swapchainCount, pSwapchains, pMetadata);
}

VkResult DeviceDriver::setDebugUtilsObjectNameEXT (VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo) const
{
	return m_vk.setDebugUtilsObjectNameEXT(device, pNameInfo);
}

VkResult DeviceDriver::setDebugUtilsObjectTagEXT (VkDevice device, const VkDebugUtilsObjectTagInfoEXT* pTagInfo) const
{
	return m_vk.setDebugUtilsObjectTagEXT(device, pTagInfo);
}

void DeviceDriver::queueBeginDebugUtilsLabelEXT (VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) const
{
	m_vk.queueBeginDebugUtilsLabelEXT(queue, pLabelInfo);
}

void DeviceDriver::queueEndDebugUtilsLabelEXT (VkQueue queue) const
{
	m_vk.queueEndDebugUtilsLabelEXT(queue);
}

void DeviceDriver::queueInsertDebugUtilsLabelEXT (VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) const
{
	m_vk.queueInsertDebugUtilsLabelEXT(queue, pLabelInfo);
}

void DeviceDriver::cmdBeginDebugUtilsLabelEXT (VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) const
{
	m_vk.cmdBeginDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
}

void DeviceDriver::cmdEndDebugUtilsLabelEXT (VkCommandBuffer commandBuffer) const
{
	m_vk.cmdEndDebugUtilsLabelEXT(commandBuffer);
}

void DeviceDriver::cmdInsertDebugUtilsLabelEXT (VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) const
{
	m_vk.cmdInsertDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
}

void DeviceDriver::cmdSetSampleLocationsEXT (VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo) const
{
	m_vk.cmdSetSampleLocationsEXT(commandBuffer, pSampleLocationsInfo);
}

VkResult DeviceDriver::getImageDrmFormatModifierPropertiesEXT (VkDevice device, VkImage image, VkImageDrmFormatModifierPropertiesEXT* pProperties) const
{
	return m_vk.getImageDrmFormatModifierPropertiesEXT(device, image, pProperties);
}

VkResult DeviceDriver::createValidationCacheEXT (VkDevice device, const VkValidationCacheCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkValidationCacheEXT* pValidationCache) const
{
	return m_vk.createValidationCacheEXT(device, pCreateInfo, pAllocator, pValidationCache);
}

void DeviceDriver::destroyValidationCacheEXT (VkDevice device, VkValidationCacheEXT validationCache, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyValidationCacheEXT(device, validationCache, pAllocator);
}

VkResult DeviceDriver::mergeValidationCachesEXT (VkDevice device, VkValidationCacheEXT dstCache, uint32_t srcCacheCount, const VkValidationCacheEXT* pSrcCaches) const
{
	return m_vk.mergeValidationCachesEXT(device, dstCache, srcCacheCount, pSrcCaches);
}

VkResult DeviceDriver::getValidationCacheDataEXT (VkDevice device, VkValidationCacheEXT validationCache, size_t* pDataSize, void* pData) const
{
	return m_vk.getValidationCacheDataEXT(device, validationCache, pDataSize, pData);
}

void DeviceDriver::cmdBindShadingRateImageNV (VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout) const
{
	m_vk.cmdBindShadingRateImageNV(commandBuffer, imageView, imageLayout);
}

void DeviceDriver::cmdSetViewportShadingRatePaletteNV (VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkShadingRatePaletteNV* pShadingRatePalettes) const
{
	m_vk.cmdSetViewportShadingRatePaletteNV(commandBuffer, firstViewport, viewportCount, pShadingRatePalettes);
}

void DeviceDriver::cmdSetCoarseSampleOrderNV (VkCommandBuffer commandBuffer, VkCoarseSampleOrderTypeNV sampleOrderType, uint32_t customSampleOrderCount, const VkCoarseSampleOrderCustomNV* pCustomSampleOrders) const
{
	m_vk.cmdSetCoarseSampleOrderNV(commandBuffer, sampleOrderType, customSampleOrderCount, pCustomSampleOrders);
}

VkResult DeviceDriver::createAccelerationStructureNV (VkDevice device, const VkAccelerationStructureCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureNV* pAccelerationStructure) const
{
	return m_vk.createAccelerationStructureNV(device, pCreateInfo, pAllocator, pAccelerationStructure);
}

void DeviceDriver::destroyAccelerationStructureNV (VkDevice device, VkAccelerationStructureNV accelerationStructure, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyAccelerationStructureNV(device, accelerationStructure, pAllocator);
}

void DeviceDriver::getAccelerationStructureMemoryRequirementsNV (VkDevice device, const VkAccelerationStructureMemoryRequirementsInfoNV* pInfo, VkMemoryRequirements2KHR* pMemoryRequirements) const
{
	m_vk.getAccelerationStructureMemoryRequirementsNV(device, pInfo, pMemoryRequirements);
}

VkResult DeviceDriver::bindAccelerationStructureMemoryNV (VkDevice device, uint32_t bindInfoCount, const VkBindAccelerationStructureMemoryInfoNV* pBindInfos) const
{
	return m_vk.bindAccelerationStructureMemoryNV(device, bindInfoCount, pBindInfos);
}

void DeviceDriver::cmdBuildAccelerationStructureNV (VkCommandBuffer commandBuffer, const VkAccelerationStructureInfoNV* pInfo, VkBuffer instanceData, VkDeviceSize instanceOffset, VkBool32 update, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkBuffer scratch, VkDeviceSize scratchOffset) const
{
	m_vk.cmdBuildAccelerationStructureNV(commandBuffer, pInfo, instanceData, instanceOffset, update, dst, src, scratch, scratchOffset);
}

void DeviceDriver::cmdCopyAccelerationStructureNV (VkCommandBuffer commandBuffer, VkAccelerationStructureNV dst, VkAccelerationStructureNV src, VkCopyAccelerationStructureModeKHR mode) const
{
	m_vk.cmdCopyAccelerationStructureNV(commandBuffer, dst, src, mode);
}

void DeviceDriver::cmdTraceRaysNV (VkCommandBuffer commandBuffer, VkBuffer raygenShaderBindingTableBuffer, VkDeviceSize raygenShaderBindingOffset, VkBuffer missShaderBindingTableBuffer, VkDeviceSize missShaderBindingOffset, VkDeviceSize missShaderBindingStride, VkBuffer hitShaderBindingTableBuffer, VkDeviceSize hitShaderBindingOffset, VkDeviceSize hitShaderBindingStride, VkBuffer callableShaderBindingTableBuffer, VkDeviceSize callableShaderBindingOffset, VkDeviceSize callableShaderBindingStride, uint32_t width, uint32_t height, uint32_t depth) const
{
	m_vk.cmdTraceRaysNV(commandBuffer, raygenShaderBindingTableBuffer, raygenShaderBindingOffset, missShaderBindingTableBuffer, missShaderBindingOffset, missShaderBindingStride, hitShaderBindingTableBuffer, hitShaderBindingOffset, hitShaderBindingStride, callableShaderBindingTableBuffer, callableShaderBindingOffset, callableShaderBindingStride, width, height, depth);
}

VkResult DeviceDriver::createRayTracingPipelinesNV (VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoNV* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) const
{
	return m_vk.createRayTracingPipelinesNV(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

VkResult DeviceDriver::getRayTracingShaderGroupHandlesKHR (VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) const
{
	return m_vk.getRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VkResult DeviceDriver::getRayTracingShaderGroupHandlesNV (VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) const
{
	return m_vk.getRayTracingShaderGroupHandlesNV(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VkResult DeviceDriver::getAccelerationStructureHandleNV (VkDevice device, VkAccelerationStructureNV accelerationStructure, size_t dataSize, void* pData) const
{
	return m_vk.getAccelerationStructureHandleNV(device, accelerationStructure, dataSize, pData);
}

void DeviceDriver::cmdWriteAccelerationStructuresPropertiesNV (VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureNV* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) const
{
	m_vk.cmdWriteAccelerationStructuresPropertiesNV(commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery);
}

VkResult DeviceDriver::compileDeferredNV (VkDevice device, VkPipeline pipeline, uint32_t shader) const
{
	return m_vk.compileDeferredNV(device, pipeline, shader);
}

VkResult DeviceDriver::getMemoryHostPointerPropertiesEXT (VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, const void* pHostPointer, VkMemoryHostPointerPropertiesEXT* pMemoryHostPointerProperties) const
{
	return m_vk.getMemoryHostPointerPropertiesEXT(device, handleType, pHostPointer, pMemoryHostPointerProperties);
}

void DeviceDriver::cmdWriteBufferMarkerAMD (VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkBuffer dstBuffer, VkDeviceSize dstOffset, uint32_t marker) const
{
	m_vk.cmdWriteBufferMarkerAMD(commandBuffer, pipelineStage, dstBuffer, dstOffset, marker);
}

VkResult DeviceDriver::getCalibratedTimestampsEXT (VkDevice device, uint32_t timestampCount, const VkCalibratedTimestampInfoEXT* pTimestampInfos, uint64_t* pTimestamps, uint64_t* pMaxDeviation) const
{
	return m_vk.getCalibratedTimestampsEXT(device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation);
}

void DeviceDriver::cmdDrawMeshTasksNV (VkCommandBuffer commandBuffer, uint32_t taskCount, uint32_t firstTask) const
{
	m_vk.cmdDrawMeshTasksNV(commandBuffer, taskCount, firstTask);
}

void DeviceDriver::cmdDrawMeshTasksIndirectNV (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) const
{
	m_vk.cmdDrawMeshTasksIndirectNV(commandBuffer, buffer, offset, drawCount, stride);
}

void DeviceDriver::cmdDrawMeshTasksIndirectCountNV (VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) const
{
	m_vk.cmdDrawMeshTasksIndirectCountNV(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

void DeviceDriver::cmdSetExclusiveScissorNV (VkCommandBuffer commandBuffer, uint32_t firstExclusiveScissor, uint32_t exclusiveScissorCount, const VkRect2D* pExclusiveScissors) const
{
	m_vk.cmdSetExclusiveScissorNV(commandBuffer, firstExclusiveScissor, exclusiveScissorCount, pExclusiveScissors);
}

void DeviceDriver::cmdSetCheckpointNV (VkCommandBuffer commandBuffer, const void* pCheckpointMarker) const
{
	m_vk.cmdSetCheckpointNV(commandBuffer, pCheckpointMarker);
}

void DeviceDriver::getQueueCheckpointDataNV (VkQueue queue, uint32_t* pCheckpointDataCount, VkCheckpointDataNV* pCheckpointData) const
{
	m_vk.getQueueCheckpointDataNV(queue, pCheckpointDataCount, pCheckpointData);
}

VkResult DeviceDriver::initializePerformanceApiINTEL (VkDevice device, const VkInitializePerformanceApiInfoINTEL* pInitializeInfo) const
{
	return m_vk.initializePerformanceApiINTEL(device, pInitializeInfo);
}

void DeviceDriver::uninitializePerformanceApiINTEL (VkDevice device) const
{
	m_vk.uninitializePerformanceApiINTEL(device);
}

VkResult DeviceDriver::cmdSetPerformanceMarkerINTEL (VkCommandBuffer commandBuffer, const VkPerformanceMarkerInfoINTEL* pMarkerInfo) const
{
	return m_vk.cmdSetPerformanceMarkerINTEL(commandBuffer, pMarkerInfo);
}

VkResult DeviceDriver::cmdSetPerformanceStreamMarkerINTEL (VkCommandBuffer commandBuffer, const VkPerformanceStreamMarkerInfoINTEL* pMarkerInfo) const
{
	return m_vk.cmdSetPerformanceStreamMarkerINTEL(commandBuffer, pMarkerInfo);
}

VkResult DeviceDriver::cmdSetPerformanceOverrideINTEL (VkCommandBuffer commandBuffer, const VkPerformanceOverrideInfoINTEL* pOverrideInfo) const
{
	return m_vk.cmdSetPerformanceOverrideINTEL(commandBuffer, pOverrideInfo);
}

VkResult DeviceDriver::acquirePerformanceConfigurationINTEL (VkDevice device, const VkPerformanceConfigurationAcquireInfoINTEL* pAcquireInfo, VkPerformanceConfigurationINTEL* pConfiguration) const
{
	return m_vk.acquirePerformanceConfigurationINTEL(device, pAcquireInfo, pConfiguration);
}

VkResult DeviceDriver::releasePerformanceConfigurationINTEL (VkDevice device, VkPerformanceConfigurationINTEL configuration) const
{
	return m_vk.releasePerformanceConfigurationINTEL(device, configuration);
}

VkResult DeviceDriver::queueSetPerformanceConfigurationINTEL (VkQueue queue, VkPerformanceConfigurationINTEL configuration) const
{
	return m_vk.queueSetPerformanceConfigurationINTEL(queue, configuration);
}

VkResult DeviceDriver::getPerformanceParameterINTEL (VkDevice device, VkPerformanceParameterTypeINTEL parameter, VkPerformanceValueINTEL* pValue) const
{
	return m_vk.getPerformanceParameterINTEL(device, parameter, pValue);
}

void DeviceDriver::setLocalDimmingAMD (VkDevice device, VkSwapchainKHR swapChain, VkBool32 localDimmingEnable) const
{
	m_vk.setLocalDimmingAMD(device, swapChain, localDimmingEnable);
}

VkDeviceAddress DeviceDriver::getBufferDeviceAddressEXT (VkDevice device, const VkBufferDeviceAddressInfo* pInfo) const
{
	return m_vk.getBufferDeviceAddressEXT(device, pInfo);
}

void DeviceDriver::cmdSetLineStippleEXT (VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) const
{
	m_vk.cmdSetLineStippleEXT(commandBuffer, lineStippleFactor, lineStipplePattern);
}

void DeviceDriver::cmdSetCullModeEXT (VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) const
{
	m_vk.cmdSetCullModeEXT(commandBuffer, cullMode);
}

void DeviceDriver::cmdSetFrontFaceEXT (VkCommandBuffer commandBuffer, VkFrontFace frontFace) const
{
	m_vk.cmdSetFrontFaceEXT(commandBuffer, frontFace);
}

void DeviceDriver::cmdSetPrimitiveTopologyEXT (VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) const
{
	m_vk.cmdSetPrimitiveTopologyEXT(commandBuffer, primitiveTopology);
}

void DeviceDriver::cmdSetViewportWithCountEXT (VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) const
{
	m_vk.cmdSetViewportWithCountEXT(commandBuffer, viewportCount, pViewports);
}

void DeviceDriver::cmdSetScissorWithCountEXT (VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) const
{
	m_vk.cmdSetScissorWithCountEXT(commandBuffer, scissorCount, pScissors);
}

void DeviceDriver::cmdBindVertexBuffers2EXT (VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) const
{
	m_vk.cmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

void DeviceDriver::cmdSetDepthTestEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) const
{
	m_vk.cmdSetDepthTestEnableEXT(commandBuffer, depthTestEnable);
}

void DeviceDriver::cmdSetDepthWriteEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) const
{
	m_vk.cmdSetDepthWriteEnableEXT(commandBuffer, depthWriteEnable);
}

void DeviceDriver::cmdSetDepthCompareOpEXT (VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) const
{
	m_vk.cmdSetDepthCompareOpEXT(commandBuffer, depthCompareOp);
}

void DeviceDriver::cmdSetDepthBoundsTestEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) const
{
	m_vk.cmdSetDepthBoundsTestEnableEXT(commandBuffer, depthBoundsTestEnable);
}

void DeviceDriver::cmdSetStencilTestEnableEXT (VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) const
{
	m_vk.cmdSetStencilTestEnableEXT(commandBuffer, stencilTestEnable);
}

void DeviceDriver::cmdSetStencilOpEXT (VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) const
{
	m_vk.cmdSetStencilOpEXT(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
}

void DeviceDriver::getGeneratedCommandsMemoryRequirementsNV (VkDevice device, const VkGeneratedCommandsMemoryRequirementsInfoNV* pInfo, VkMemoryRequirements2* pMemoryRequirements) const
{
	m_vk.getGeneratedCommandsMemoryRequirementsNV(device, pInfo, pMemoryRequirements);
}

void DeviceDriver::cmdPreprocessGeneratedCommandsNV (VkCommandBuffer commandBuffer, const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo) const
{
	m_vk.cmdPreprocessGeneratedCommandsNV(commandBuffer, pGeneratedCommandsInfo);
}

void DeviceDriver::cmdExecuteGeneratedCommandsNV (VkCommandBuffer commandBuffer, VkBool32 isPreprocessed, const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo) const
{
	m_vk.cmdExecuteGeneratedCommandsNV(commandBuffer, isPreprocessed, pGeneratedCommandsInfo);
}

void DeviceDriver::cmdBindPipelineShaderGroupNV (VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline, uint32_t groupIndex) const
{
	m_vk.cmdBindPipelineShaderGroupNV(commandBuffer, pipelineBindPoint, pipeline, groupIndex);
}

VkResult DeviceDriver::createIndirectCommandsLayoutNV (VkDevice device, const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkIndirectCommandsLayoutNV* pIndirectCommandsLayout) const
{
	return m_vk.createIndirectCommandsLayoutNV(device, pCreateInfo, pAllocator, pIndirectCommandsLayout);
}

void DeviceDriver::destroyIndirectCommandsLayoutNV (VkDevice device, VkIndirectCommandsLayoutNV indirectCommandsLayout, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyIndirectCommandsLayoutNV(device, indirectCommandsLayout, pAllocator);
}

VkResult DeviceDriver::createPrivateDataSlotEXT (VkDevice device, const VkPrivateDataSlotCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlotEXT* pPrivateDataSlot) const
{
	return m_vk.createPrivateDataSlotEXT(device, pCreateInfo, pAllocator, pPrivateDataSlot);
}

void DeviceDriver::destroyPrivateDataSlotEXT (VkDevice device, VkPrivateDataSlotEXT privateDataSlot, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyPrivateDataSlotEXT(device, privateDataSlot, pAllocator);
}

VkResult DeviceDriver::setPrivateDataEXT (VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlotEXT privateDataSlot, uint64_t data) const
{
	return m_vk.setPrivateDataEXT(device, objectType, objectHandle, privateDataSlot, data);
}

void DeviceDriver::getPrivateDataEXT (VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlotEXT privateDataSlot, uint64_t* pData) const
{
	m_vk.getPrivateDataEXT(device, objectType, objectHandle, privateDataSlot, pData);
}

void DeviceDriver::cmdSetFragmentShadingRateEnumNV (VkCommandBuffer commandBuffer, VkFragmentShadingRateNV shadingRate, const VkFragmentShadingRateCombinerOpKHR combinerOps[2]) const
{
	m_vk.cmdSetFragmentShadingRateEnumNV(commandBuffer, shadingRate, combinerOps);
}

void DeviceDriver::cmdSetVertexInputEXT (VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) const
{
	m_vk.cmdSetVertexInputEXT(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions);
}

VkResult DeviceDriver::getDeviceSubpassShadingMaxWorkgroupSizeHUAWEI (VkDevice device, VkRenderPass renderpass, VkExtent2D* pMaxWorkgroupSize) const
{
	return m_vk.getDeviceSubpassShadingMaxWorkgroupSizeHUAWEI(device, renderpass, pMaxWorkgroupSize);
}

void DeviceDriver::cmdSubpassShadingHUAWEI (VkCommandBuffer commandBuffer) const
{
	m_vk.cmdSubpassShadingHUAWEI(commandBuffer);
}

void DeviceDriver::cmdBindInvocationMaskHUAWEI (VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout imageLayout) const
{
	m_vk.cmdBindInvocationMaskHUAWEI(commandBuffer, imageView, imageLayout);
}

VkResult DeviceDriver::getMemoryRemoteAddressNV (VkDevice device, const VkMemoryGetRemoteAddressInfoNV* pMemoryGetRemoteAddressInfo, VkRemoteAddressNV* pAddress) const
{
	return m_vk.getMemoryRemoteAddressNV(device, pMemoryGetRemoteAddressInfo, pAddress);
}

void DeviceDriver::cmdSetPatchControlPointsEXT (VkCommandBuffer commandBuffer, uint32_t patchControlPoints) const
{
	m_vk.cmdSetPatchControlPointsEXT(commandBuffer, patchControlPoints);
}

void DeviceDriver::cmdSetRasterizerDiscardEnableEXT (VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) const
{
	m_vk.cmdSetRasterizerDiscardEnableEXT(commandBuffer, rasterizerDiscardEnable);
}

void DeviceDriver::cmdSetDepthBiasEnableEXT (VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) const
{
	m_vk.cmdSetDepthBiasEnableEXT(commandBuffer, depthBiasEnable);
}

void DeviceDriver::cmdSetLogicOpEXT (VkCommandBuffer commandBuffer, VkLogicOp logicOp) const
{
	m_vk.cmdSetLogicOpEXT(commandBuffer, logicOp);
}

void DeviceDriver::cmdSetPrimitiveRestartEnableEXT (VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) const
{
	m_vk.cmdSetPrimitiveRestartEnableEXT(commandBuffer, primitiveRestartEnable);
}

void DeviceDriver::cmdSetColorWriteEnableEXT (VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) const
{
	m_vk.cmdSetColorWriteEnableEXT(commandBuffer, attachmentCount, pColorWriteEnables);
}

void DeviceDriver::cmdDrawMultiEXT (VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawInfoEXT* pVertexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride) const
{
	m_vk.cmdDrawMultiEXT(commandBuffer, drawCount, pVertexInfo, instanceCount, firstInstance, stride);
}

void DeviceDriver::cmdDrawMultiIndexedEXT (VkCommandBuffer commandBuffer, uint32_t drawCount, const VkMultiDrawIndexedInfoEXT* pIndexInfo, uint32_t instanceCount, uint32_t firstInstance, uint32_t stride, const int32_t* pVertexOffset) const
{
	m_vk.cmdDrawMultiIndexedEXT(commandBuffer, drawCount, pIndexInfo, instanceCount, firstInstance, stride, pVertexOffset);
}

void DeviceDriver::setDeviceMemoryPriorityEXT (VkDevice device, VkDeviceMemory memory, float priority) const
{
	m_vk.setDeviceMemoryPriorityEXT(device, memory, priority);
}

VkResult DeviceDriver::createAccelerationStructureKHR (VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure) const
{
	return m_vk.createAccelerationStructureKHR(device, pCreateInfo, pAllocator, pAccelerationStructure);
}

void DeviceDriver::destroyAccelerationStructureKHR (VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyAccelerationStructureKHR(device, accelerationStructure, pAllocator);
}

void DeviceDriver::cmdBuildAccelerationStructuresKHR (VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos) const
{
	m_vk.cmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

void DeviceDriver::cmdBuildAccelerationStructuresIndirectKHR (VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkDeviceAddress* pIndirectDeviceAddresses, const uint32_t* pIndirectStrides, const uint32_t* const* ppMaxPrimitiveCounts) const
{
	m_vk.cmdBuildAccelerationStructuresIndirectKHR(commandBuffer, infoCount, pInfos, pIndirectDeviceAddresses, pIndirectStrides, ppMaxPrimitiveCounts);
}

VkResult DeviceDriver::buildAccelerationStructuresKHR (VkDevice device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos) const
{
	return m_vk.buildAccelerationStructuresKHR(device, deferredOperation, infoCount, pInfos, ppBuildRangeInfos);
}

VkResult DeviceDriver::copyAccelerationStructureKHR (VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureInfoKHR* pInfo) const
{
	return m_vk.copyAccelerationStructureKHR(device, deferredOperation, pInfo);
}

VkResult DeviceDriver::copyAccelerationStructureToMemoryKHR (VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) const
{
	return m_vk.copyAccelerationStructureToMemoryKHR(device, deferredOperation, pInfo);
}

VkResult DeviceDriver::copyMemoryToAccelerationStructureKHR (VkDevice device, VkDeferredOperationKHR deferredOperation, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) const
{
	return m_vk.copyMemoryToAccelerationStructureKHR(device, deferredOperation, pInfo);
}

VkResult DeviceDriver::writeAccelerationStructuresPropertiesKHR (VkDevice device, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, size_t dataSize, void* pData, size_t stride) const
{
	return m_vk.writeAccelerationStructuresPropertiesKHR(device, accelerationStructureCount, pAccelerationStructures, queryType, dataSize, pData, stride);
}

void DeviceDriver::cmdCopyAccelerationStructureKHR (VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureInfoKHR* pInfo) const
{
	m_vk.cmdCopyAccelerationStructureKHR(commandBuffer, pInfo);
}

void DeviceDriver::cmdCopyAccelerationStructureToMemoryKHR (VkCommandBuffer commandBuffer, const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) const
{
	m_vk.cmdCopyAccelerationStructureToMemoryKHR(commandBuffer, pInfo);
}

void DeviceDriver::cmdCopyMemoryToAccelerationStructureKHR (VkCommandBuffer commandBuffer, const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) const
{
	m_vk.cmdCopyMemoryToAccelerationStructureKHR(commandBuffer, pInfo);
}

VkDeviceAddress DeviceDriver::getAccelerationStructureDeviceAddressKHR (VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) const
{
	return m_vk.getAccelerationStructureDeviceAddressKHR(device, pInfo);
}

void DeviceDriver::cmdWriteAccelerationStructuresPropertiesKHR (VkCommandBuffer commandBuffer, uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) const
{
	m_vk.cmdWriteAccelerationStructuresPropertiesKHR(commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery);
}

void DeviceDriver::getDeviceAccelerationStructureCompatibilityKHR (VkDevice device, const VkAccelerationStructureVersionInfoKHR* pVersionInfo, VkAccelerationStructureCompatibilityKHR* pCompatibility) const
{
	m_vk.getDeviceAccelerationStructureCompatibilityKHR(device, pVersionInfo, pCompatibility);
}

void DeviceDriver::getAccelerationStructureBuildSizesKHR (VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo) const
{
	m_vk.getAccelerationStructureBuildSizesKHR(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

void DeviceDriver::cmdTraceRaysKHR (VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth) const
{
	m_vk.cmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}

VkResult DeviceDriver::createRayTracingPipelinesKHR (VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) const
{
	return m_vk.createRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

VkResult DeviceDriver::getRayTracingCaptureReplayShaderGroupHandlesKHR (VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) const
{
	return m_vk.getRayTracingCaptureReplayShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

void DeviceDriver::cmdTraceRaysIndirectKHR (VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, VkDeviceAddress indirectDeviceAddress) const
{
	m_vk.cmdTraceRaysIndirectKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, indirectDeviceAddress);
}

VkDeviceSize DeviceDriver::getRayTracingShaderGroupStackSizeKHR (VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) const
{
	return m_vk.getRayTracingShaderGroupStackSizeKHR(device, pipeline, group, groupShader);
}

void DeviceDriver::cmdSetRayTracingPipelineStackSizeKHR (VkCommandBuffer commandBuffer, uint32_t pipelineStackSize) const
{
	m_vk.cmdSetRayTracingPipelineStackSizeKHR(commandBuffer, pipelineStackSize);
}

VkResult DeviceDriver::getAndroidHardwareBufferPropertiesANDROID (VkDevice device, const struct pt::AndroidHardwareBufferPtr buffer, VkAndroidHardwareBufferPropertiesANDROID* pProperties) const
{
	return m_vk.getAndroidHardwareBufferPropertiesANDROID(device, buffer, pProperties);
}

VkResult DeviceDriver::getMemoryAndroidHardwareBufferANDROID (VkDevice device, const VkMemoryGetAndroidHardwareBufferInfoANDROID* pInfo, struct pt::AndroidHardwareBufferPtr* pBuffer) const
{
	return m_vk.getMemoryAndroidHardwareBufferANDROID(device, pInfo, pBuffer);
}

VkResult DeviceDriver::createVideoSessionKHR (VkDevice device, const VkVideoSessionCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkVideoSessionKHR* pVideoSession) const
{
	return m_vk.createVideoSessionKHR(device, pCreateInfo, pAllocator, pVideoSession);
}

void DeviceDriver::destroyVideoSessionKHR (VkDevice device, VkVideoSessionKHR videoSession, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyVideoSessionKHR(device, videoSession, pAllocator);
}

VkResult DeviceDriver::getVideoSessionMemoryRequirementsKHR (VkDevice device, VkVideoSessionKHR videoSession, uint32_t* pVideoSessionMemoryRequirementsCount, VkVideoGetMemoryPropertiesKHR* pVideoSessionMemoryRequirements) const
{
	return m_vk.getVideoSessionMemoryRequirementsKHR(device, videoSession, pVideoSessionMemoryRequirementsCount, pVideoSessionMemoryRequirements);
}

VkResult DeviceDriver::bindVideoSessionMemoryKHR (VkDevice device, VkVideoSessionKHR videoSession, uint32_t videoSessionBindMemoryCount, const VkVideoBindMemoryKHR* pVideoSessionBindMemories) const
{
	return m_vk.bindVideoSessionMemoryKHR(device, videoSession, videoSessionBindMemoryCount, pVideoSessionBindMemories);
}

VkResult DeviceDriver::createVideoSessionParametersKHR (VkDevice device, const VkVideoSessionParametersCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkVideoSessionParametersKHR* pVideoSessionParameters) const
{
	return m_vk.createVideoSessionParametersKHR(device, pCreateInfo, pAllocator, pVideoSessionParameters);
}

VkResult DeviceDriver::updateVideoSessionParametersKHR (VkDevice device, VkVideoSessionParametersKHR videoSessionParameters, const VkVideoSessionParametersUpdateInfoKHR* pUpdateInfo) const
{
	return m_vk.updateVideoSessionParametersKHR(device, videoSessionParameters, pUpdateInfo);
}

void DeviceDriver::destroyVideoSessionParametersKHR (VkDevice device, VkVideoSessionParametersKHR videoSessionParameters, const VkAllocationCallbacks* pAllocator) const
{
	m_vk.destroyVideoSessionParametersKHR(device, videoSessionParameters, pAllocator);
}

void DeviceDriver::cmdBeginVideoCodingKHR (VkCommandBuffer commandBuffer, const VkVideoBeginCodingInfoKHR* pBeginInfo) const
{
	m_vk.cmdBeginVideoCodingKHR(commandBuffer, pBeginInfo);
}

void DeviceDriver::cmdEndVideoCodingKHR (VkCommandBuffer commandBuffer, const VkVideoEndCodingInfoKHR* pEndCodingInfo) const
{
	m_vk.cmdEndVideoCodingKHR(commandBuffer, pEndCodingInfo);
}

void DeviceDriver::cmdControlVideoCodingKHR (VkCommandBuffer commandBuffer, const VkVideoCodingControlInfoKHR* pCodingControlInfo) const
{
	m_vk.cmdControlVideoCodingKHR(commandBuffer, pCodingControlInfo);
}

void DeviceDriver::cmdDecodeVideoKHR (VkCommandBuffer commandBuffer, const VkVideoDecodeInfoKHR* pFrameInfo) const
{
	m_vk.cmdDecodeVideoKHR(commandBuffer, pFrameInfo);
}

void DeviceDriver::cmdEncodeVideoKHR (VkCommandBuffer commandBuffer, const VkVideoEncodeInfoKHR* pEncodeInfo) const
{
	m_vk.cmdEncodeVideoKHR(commandBuffer, pEncodeInfo);
}

VkResult DeviceDriver::getMemoryZirconHandleFUCHSIA (VkDevice device, const VkMemoryGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo, pt::zx_handle_t* pZirconHandle) const
{
	return m_vk.getMemoryZirconHandleFUCHSIA(device, pGetZirconHandleInfo, pZirconHandle);
}

VkResult DeviceDriver::getMemoryZirconHandlePropertiesFUCHSIA (VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, pt::zx_handle_t zirconHandle, VkMemoryZirconHandlePropertiesFUCHSIA* pMemoryZirconHandleProperties) const
{
	return m_vk.getMemoryZirconHandlePropertiesFUCHSIA(device, handleType, zirconHandle, pMemoryZirconHandleProperties);
}

VkResult DeviceDriver::importSemaphoreZirconHandleFUCHSIA (VkDevice device, const VkImportSemaphoreZirconHandleInfoFUCHSIA* pImportSemaphoreZirconHandleInfo) const
{
	return m_vk.importSemaphoreZirconHandleFUCHSIA(device, pImportSemaphoreZirconHandleInfo);
}

VkResult DeviceDriver::getSemaphoreZirconHandleFUCHSIA (VkDevice device, const VkSemaphoreGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo, pt::zx_handle_t* pZirconHandle) const
{
	return m_vk.getSemaphoreZirconHandleFUCHSIA(device, pGetZirconHandleInfo, pZirconHandle);
}

VkResult DeviceDriver::getMemoryWin32HandleKHR (VkDevice device, const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo, pt::Win32Handle* pHandle) const
{
	return m_vk.getMemoryWin32HandleKHR(device, pGetWin32HandleInfo, pHandle);
}

VkResult DeviceDriver::getMemoryWin32HandlePropertiesKHR (VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, pt::Win32Handle handle, VkMemoryWin32HandlePropertiesKHR* pMemoryWin32HandleProperties) const
{
	return m_vk.getMemoryWin32HandlePropertiesKHR(device, handleType, handle, pMemoryWin32HandleProperties);
}

VkResult DeviceDriver::importSemaphoreWin32HandleKHR (VkDevice device, const VkImportSemaphoreWin32HandleInfoKHR* pImportSemaphoreWin32HandleInfo) const
{
	return m_vk.importSemaphoreWin32HandleKHR(device, pImportSemaphoreWin32HandleInfo);
}

VkResult DeviceDriver::getSemaphoreWin32HandleKHR (VkDevice device, const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo, pt::Win32Handle* pHandle) const
{
	return m_vk.getSemaphoreWin32HandleKHR(device, pGetWin32HandleInfo, pHandle);
}

VkResult DeviceDriver::importFenceWin32HandleKHR (VkDevice device, const VkImportFenceWin32HandleInfoKHR* pImportFenceWin32HandleInfo) const
{
	return m_vk.importFenceWin32HandleKHR(device, pImportFenceWin32HandleInfo);
}

VkResult DeviceDriver::getFenceWin32HandleKHR (VkDevice device, const VkFenceGetWin32HandleInfoKHR* pGetWin32HandleInfo, pt::Win32Handle* pHandle) const
{
	return m_vk.getFenceWin32HandleKHR(device, pGetWin32HandleInfo, pHandle);
}

VkResult DeviceDriver::getMemoryWin32HandleNV (VkDevice device, VkDeviceMemory memory, VkExternalMemoryHandleTypeFlagsNV handleType, pt::Win32Handle* pHandle) const
{
	return m_vk.getMemoryWin32HandleNV(device, memory, handleType, pHandle);
}

VkResult DeviceDriver::acquireFullScreenExclusiveModeEXT (VkDevice device, VkSwapchainKHR swapchain) const
{
	return m_vk.acquireFullScreenExclusiveModeEXT(device, swapchain);
}

VkResult DeviceDriver::releaseFullScreenExclusiveModeEXT (VkDevice device, VkSwapchainKHR swapchain) const
{
	return m_vk.releaseFullScreenExclusiveModeEXT(device, swapchain);
}

VkResult DeviceDriver::getDeviceGroupSurfacePresentModes2EXT (VkDevice device, const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, VkDeviceGroupPresentModeFlagsKHR* pModes) const
{
	return m_vk.getDeviceGroupSurfacePresentModes2EXT(device, pSurfaceInfo, pModes);
}
