/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */

inline VkAllocationCallbacks makeAllocationCallbacks (void* pUserData, PFN_vkAllocationFunction pfnAllocation, PFN_vkReallocationFunction pfnReallocation, PFN_vkFreeFunction pfnFree, PFN_vkInternalAllocationNotification pfnInternalAllocation, PFN_vkInternalFreeNotification pfnInternalFree)
{
	VkAllocationCallbacks res;
	res.pUserData				= pUserData;
	res.pfnAllocation			= pfnAllocation;
	res.pfnReallocation			= pfnReallocation;
	res.pfnFree					= pfnFree;
	res.pfnInternalAllocation	= pfnInternalAllocation;
	res.pfnInternalFree			= pfnInternalFree;
	return res;
}

inline VkExtent3D makeExtent3D (deUint32 width, deUint32 height, deUint32 depth)
{
	VkExtent3D res;
	res.width	= width;
	res.height	= height;
	res.depth	= depth;
	return res;
}

inline VkMemoryRequirements makeMemoryRequirements (VkDeviceSize size, VkDeviceSize alignment, deUint32 memoryTypeBits)
{
	VkMemoryRequirements res;
	res.size			= size;
	res.alignment		= alignment;
	res.memoryTypeBits	= memoryTypeBits;
	return res;
}

inline VkSparseMemoryBind makeSparseMemoryBind (VkDeviceSize resourceOffset, VkDeviceSize size, VkDeviceMemory memory, VkDeviceSize memoryOffset, VkSparseMemoryBindFlags flags)
{
	VkSparseMemoryBind res;
	res.resourceOffset	= resourceOffset;
	res.size			= size;
	res.memory			= memory;
	res.memoryOffset	= memoryOffset;
	res.flags			= flags;
	return res;
}

inline VkSparseBufferMemoryBindInfo makeSparseBufferMemoryBindInfo (VkBuffer buffer, deUint32 bindCount, const VkSparseMemoryBind* pBinds)
{
	VkSparseBufferMemoryBindInfo res;
	res.buffer		= buffer;
	res.bindCount	= bindCount;
	res.pBinds		= pBinds;
	return res;
}

inline VkSparseImageOpaqueMemoryBindInfo makeSparseImageOpaqueMemoryBindInfo (VkImage image, deUint32 bindCount, const VkSparseMemoryBind* pBinds)
{
	VkSparseImageOpaqueMemoryBindInfo res;
	res.image		= image;
	res.bindCount	= bindCount;
	res.pBinds		= pBinds;
	return res;
}

inline VkImageSubresource makeImageSubresource (VkImageAspectFlags aspectMask, deUint32 mipLevel, deUint32 arrayLayer)
{
	VkImageSubresource res;
	res.aspectMask	= aspectMask;
	res.mipLevel	= mipLevel;
	res.arrayLayer	= arrayLayer;
	return res;
}

inline VkOffset3D makeOffset3D (deInt32 x, deInt32 y, deInt32 z)
{
	VkOffset3D res;
	res.x	= x;
	res.y	= y;
	res.z	= z;
	return res;
}

inline VkSparseImageMemoryBindInfo makeSparseImageMemoryBindInfo (VkImage image, deUint32 bindCount, const VkSparseImageMemoryBind* pBinds)
{
	VkSparseImageMemoryBindInfo res;
	res.image		= image;
	res.bindCount	= bindCount;
	res.pBinds		= pBinds;
	return res;
}

inline VkSubresourceLayout makeSubresourceLayout (VkDeviceSize offset, VkDeviceSize size, VkDeviceSize rowPitch, VkDeviceSize arrayPitch, VkDeviceSize depthPitch)
{
	VkSubresourceLayout res;
	res.offset		= offset;
	res.size		= size;
	res.rowPitch	= rowPitch;
	res.arrayPitch	= arrayPitch;
	res.depthPitch	= depthPitch;
	return res;
}

inline VkComponentMapping makeComponentMapping (VkComponentSwizzle r, VkComponentSwizzle g, VkComponentSwizzle b, VkComponentSwizzle a)
{
	VkComponentMapping res;
	res.r	= r;
	res.g	= g;
	res.b	= b;
	res.a	= a;
	return res;
}

inline VkImageSubresourceRange makeImageSubresourceRange (VkImageAspectFlags aspectMask, deUint32 baseMipLevel, deUint32 levelCount, deUint32 baseArrayLayer, deUint32 layerCount)
{
	VkImageSubresourceRange res;
	res.aspectMask		= aspectMask;
	res.baseMipLevel	= baseMipLevel;
	res.levelCount		= levelCount;
	res.baseArrayLayer	= baseArrayLayer;
	res.layerCount		= layerCount;
	return res;
}

inline VkSpecializationMapEntry makeSpecializationMapEntry (deUint32 constantID, deUint32 offset, deUintptr size)
{
	VkSpecializationMapEntry res;
	res.constantID	= constantID;
	res.offset		= offset;
	res.size		= size;
	return res;
}

inline VkSpecializationInfo makeSpecializationInfo (deUint32 mapEntryCount, const VkSpecializationMapEntry* pMapEntries, deUintptr dataSize, const void* pData)
{
	VkSpecializationInfo res;
	res.mapEntryCount	= mapEntryCount;
	res.pMapEntries		= pMapEntries;
	res.dataSize		= dataSize;
	res.pData			= pData;
	return res;
}

inline VkVertexInputBindingDescription makeVertexInputBindingDescription (deUint32 binding, deUint32 stride, VkVertexInputRate inputRate)
{
	VkVertexInputBindingDescription res;
	res.binding		= binding;
	res.stride		= stride;
	res.inputRate	= inputRate;
	return res;
}

inline VkVertexInputAttributeDescription makeVertexInputAttributeDescription (deUint32 location, deUint32 binding, VkFormat format, deUint32 offset)
{
	VkVertexInputAttributeDescription res;
	res.location	= location;
	res.binding		= binding;
	res.format		= format;
	res.offset		= offset;
	return res;
}

inline VkViewport makeViewport (float x, float y, float width, float height, float minDepth, float maxDepth)
{
	VkViewport res;
	res.x			= x;
	res.y			= y;
	res.width		= width;
	res.height		= height;
	res.minDepth	= minDepth;
	res.maxDepth	= maxDepth;
	return res;
}

inline VkOffset2D makeOffset2D (deInt32 x, deInt32 y)
{
	VkOffset2D res;
	res.x	= x;
	res.y	= y;
	return res;
}

inline VkExtent2D makeExtent2D (deUint32 width, deUint32 height)
{
	VkExtent2D res;
	res.width	= width;
	res.height	= height;
	return res;
}

inline VkStencilOpState makeStencilOpState (VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp, deUint32 compareMask, deUint32 writeMask, deUint32 reference)
{
	VkStencilOpState res;
	res.failOp		= failOp;
	res.passOp		= passOp;
	res.depthFailOp	= depthFailOp;
	res.compareOp	= compareOp;
	res.compareMask	= compareMask;
	res.writeMask	= writeMask;
	res.reference	= reference;
	return res;
}

inline VkPipelineColorBlendAttachmentState makePipelineColorBlendAttachmentState (VkBool32 blendEnable, VkBlendFactor srcColorBlendFactor, VkBlendFactor dstColorBlendFactor, VkBlendOp colorBlendOp, VkBlendFactor srcAlphaBlendFactor, VkBlendFactor dstAlphaBlendFactor, VkBlendOp alphaBlendOp, VkColorComponentFlags colorWriteMask)
{
	VkPipelineColorBlendAttachmentState res;
	res.blendEnable			= blendEnable;
	res.srcColorBlendFactor	= srcColorBlendFactor;
	res.dstColorBlendFactor	= dstColorBlendFactor;
	res.colorBlendOp		= colorBlendOp;
	res.srcAlphaBlendFactor	= srcAlphaBlendFactor;
	res.dstAlphaBlendFactor	= dstAlphaBlendFactor;
	res.alphaBlendOp		= alphaBlendOp;
	res.colorWriteMask		= colorWriteMask;
	return res;
}

inline VkPushConstantRange makePushConstantRange (VkShaderStageFlags stageFlags, deUint32 offset, deUint32 size)
{
	VkPushConstantRange res;
	res.stageFlags	= stageFlags;
	res.offset		= offset;
	res.size		= size;
	return res;
}

inline VkDescriptorSetLayoutBinding makeDescriptorSetLayoutBinding (deUint32 binding, VkDescriptorType descriptorType, deUint32 descriptorCount, VkShaderStageFlags stageFlags, const VkSampler* pImmutableSamplers)
{
	VkDescriptorSetLayoutBinding res;
	res.binding				= binding;
	res.descriptorType		= descriptorType;
	res.descriptorCount		= descriptorCount;
	res.stageFlags			= stageFlags;
	res.pImmutableSamplers	= pImmutableSamplers;
	return res;
}

inline VkDescriptorPoolSize makeDescriptorPoolSize (VkDescriptorType type, deUint32 descriptorCount)
{
	VkDescriptorPoolSize res;
	res.type			= type;
	res.descriptorCount	= descriptorCount;
	return res;
}

inline VkDescriptorImageInfo makeDescriptorImageInfo (VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
{
	VkDescriptorImageInfo res;
	res.sampler		= sampler;
	res.imageView	= imageView;
	res.imageLayout	= imageLayout;
	return res;
}

inline VkDescriptorBufferInfo makeDescriptorBufferInfo (VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
	VkDescriptorBufferInfo res;
	res.buffer	= buffer;
	res.offset	= offset;
	res.range	= range;
	return res;
}

inline VkAttachmentDescription makeAttachmentDescription (VkAttachmentDescriptionFlags flags, VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkAttachmentLoadOp stencilLoadOp, VkAttachmentStoreOp stencilStoreOp, VkImageLayout initialLayout, VkImageLayout finalLayout)
{
	VkAttachmentDescription res;
	res.flags			= flags;
	res.format			= format;
	res.samples			= samples;
	res.loadOp			= loadOp;
	res.storeOp			= storeOp;
	res.stencilLoadOp	= stencilLoadOp;
	res.stencilStoreOp	= stencilStoreOp;
	res.initialLayout	= initialLayout;
	res.finalLayout		= finalLayout;
	return res;
}

inline VkAttachmentReference makeAttachmentReference (deUint32 attachment, VkImageLayout layout)
{
	VkAttachmentReference res;
	res.attachment	= attachment;
	res.layout		= layout;
	return res;
}

inline VkSubpassDescription makeSubpassDescription (VkSubpassDescriptionFlags flags, VkPipelineBindPoint pipelineBindPoint, deUint32 inputAttachmentCount, const VkAttachmentReference* pInputAttachments, deUint32 colorAttachmentCount, const VkAttachmentReference* pColorAttachments, const VkAttachmentReference* pResolveAttachments, const VkAttachmentReference* pDepthStencilAttachment, deUint32 preserveAttachmentCount, const deUint32* pPreserveAttachments)
{
	VkSubpassDescription res;
	res.flags					= flags;
	res.pipelineBindPoint		= pipelineBindPoint;
	res.inputAttachmentCount	= inputAttachmentCount;
	res.pInputAttachments		= pInputAttachments;
	res.colorAttachmentCount	= colorAttachmentCount;
	res.pColorAttachments		= pColorAttachments;
	res.pResolveAttachments		= pResolveAttachments;
	res.pDepthStencilAttachment	= pDepthStencilAttachment;
	res.preserveAttachmentCount	= preserveAttachmentCount;
	res.pPreserveAttachments	= pPreserveAttachments;
	return res;
}

inline VkSubpassDependency makeSubpassDependency (deUint32 srcSubpass, deUint32 dstSubpass, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkDependencyFlags dependencyFlags)
{
	VkSubpassDependency res;
	res.srcSubpass		= srcSubpass;
	res.dstSubpass		= dstSubpass;
	res.srcStageMask	= srcStageMask;
	res.dstStageMask	= dstStageMask;
	res.srcAccessMask	= srcAccessMask;
	res.dstAccessMask	= dstAccessMask;
	res.dependencyFlags	= dependencyFlags;
	return res;
}

inline VkBufferCopy makeBufferCopy (VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
{
	VkBufferCopy res;
	res.srcOffset	= srcOffset;
	res.dstOffset	= dstOffset;
	res.size		= size;
	return res;
}

inline VkImageSubresourceLayers makeImageSubresourceLayers (VkImageAspectFlags aspectMask, deUint32 mipLevel, deUint32 baseArrayLayer, deUint32 layerCount)
{
	VkImageSubresourceLayers res;
	res.aspectMask		= aspectMask;
	res.mipLevel		= mipLevel;
	res.baseArrayLayer	= baseArrayLayer;
	res.layerCount		= layerCount;
	return res;
}

inline VkClearDepthStencilValue makeClearDepthStencilValue (float depth, deUint32 stencil)
{
	VkClearDepthStencilValue res;
	res.depth	= depth;
	res.stencil	= stencil;
	return res;
}

inline VkDispatchIndirectCommand makeDispatchIndirectCommand (deUint32 x, deUint32 y, deUint32 z)
{
	VkDispatchIndirectCommand res;
	res.x	= x;
	res.y	= y;
	res.z	= z;
	return res;
}

inline VkDrawIndexedIndirectCommand makeDrawIndexedIndirectCommand (deUint32 indexCount, deUint32 instanceCount, deUint32 firstIndex, deInt32 vertexOffset, deUint32 firstInstance)
{
	VkDrawIndexedIndirectCommand res;
	res.indexCount		= indexCount;
	res.instanceCount	= instanceCount;
	res.firstIndex		= firstIndex;
	res.vertexOffset	= vertexOffset;
	res.firstInstance	= firstInstance;
	return res;
}

inline VkDrawIndirectCommand makeDrawIndirectCommand (deUint32 vertexCount, deUint32 instanceCount, deUint32 firstVertex, deUint32 firstInstance)
{
	VkDrawIndirectCommand res;
	res.vertexCount		= vertexCount;
	res.instanceCount	= instanceCount;
	res.firstVertex		= firstVertex;
	res.firstInstance	= firstInstance;
	return res;
}

inline VkInputAttachmentAspectReference makeInputAttachmentAspectReference (deUint32 subpass, deUint32 inputAttachmentIndex, VkImageAspectFlags aspectMask)
{
	VkInputAttachmentAspectReference res;
	res.subpass					= subpass;
	res.inputAttachmentIndex	= inputAttachmentIndex;
	res.aspectMask				= aspectMask;
	return res;
}

inline VkDescriptorUpdateTemplateEntry makeDescriptorUpdateTemplateEntry (deUint32 dstBinding, deUint32 dstArrayElement, deUint32 descriptorCount, VkDescriptorType descriptorType, deUintptr offset, deUintptr stride)
{
	VkDescriptorUpdateTemplateEntry res;
	res.dstBinding		= dstBinding;
	res.dstArrayElement	= dstArrayElement;
	res.descriptorCount	= descriptorCount;
	res.descriptorType	= descriptorType;
	res.offset			= offset;
	res.stride			= stride;
	return res;
}

inline VkExternalMemoryProperties makeExternalMemoryProperties (VkExternalMemoryFeatureFlags externalMemoryFeatures, VkExternalMemoryHandleTypeFlags exportFromImportedHandleTypes, VkExternalMemoryHandleTypeFlags compatibleHandleTypes)
{
	VkExternalMemoryProperties res;
	res.externalMemoryFeatures			= externalMemoryFeatures;
	res.exportFromImportedHandleTypes	= exportFromImportedHandleTypes;
	res.compatibleHandleTypes			= compatibleHandleTypes;
	return res;
}

inline VkConformanceVersion makeConformanceVersion (deUint8 major, deUint8 minor, deUint8 subminor, deUint8 patch)
{
	VkConformanceVersion res;
	res.major		= major;
	res.minor		= minor;
	res.subminor	= subminor;
	res.patch		= patch;
	return res;
}

inline VkSurfaceFormatKHR makeSurfaceFormatKHR (VkFormat format, VkColorSpaceKHR colorSpace)
{
	VkSurfaceFormatKHR res;
	res.format		= format;
	res.colorSpace	= colorSpace;
	return res;
}

inline VkDisplayPlanePropertiesKHR makeDisplayPlanePropertiesKHR (VkDisplayKHR currentDisplay, deUint32 currentStackIndex)
{
	VkDisplayPlanePropertiesKHR res;
	res.currentDisplay		= currentDisplay;
	res.currentStackIndex	= currentStackIndex;
	return res;
}

inline VkPresentRegionKHR makePresentRegionKHR (deUint32 rectangleCount, const VkRectLayerKHR* pRectangles)
{
	VkPresentRegionKHR res;
	res.rectangleCount	= rectangleCount;
	res.pRectangles		= pRectangles;
	return res;
}

inline VkShaderResourceUsageAMD makeShaderResourceUsageAMD (deUint32 numUsedVgprs, deUint32 numUsedSgprs, deUint32 ldsSizePerLocalWorkGroup, deUintptr ldsUsageSizeInBytes, deUintptr scratchMemUsageInBytes)
{
	VkShaderResourceUsageAMD res;
	res.numUsedVgprs				= numUsedVgprs;
	res.numUsedSgprs				= numUsedSgprs;
	res.ldsSizePerLocalWorkGroup	= ldsSizePerLocalWorkGroup;
	res.ldsUsageSizeInBytes			= ldsUsageSizeInBytes;
	res.scratchMemUsageInBytes		= scratchMemUsageInBytes;
	return res;
}

inline VkViewportWScalingNV makeViewportWScalingNV (float xcoeff, float ycoeff)
{
	VkViewportWScalingNV res;
	res.xcoeff	= xcoeff;
	res.ycoeff	= ycoeff;
	return res;
}

inline VkRefreshCycleDurationGOOGLE makeRefreshCycleDurationGOOGLE (deUint64 refreshDuration)
{
	VkRefreshCycleDurationGOOGLE res;
	res.refreshDuration	= refreshDuration;
	return res;
}

inline VkPastPresentationTimingGOOGLE makePastPresentationTimingGOOGLE (deUint32 presentID, deUint64 desiredPresentTime, deUint64 actualPresentTime, deUint64 earliestPresentTime, deUint64 presentMargin)
{
	VkPastPresentationTimingGOOGLE res;
	res.presentID			= presentID;
	res.desiredPresentTime	= desiredPresentTime;
	res.actualPresentTime	= actualPresentTime;
	res.earliestPresentTime	= earliestPresentTime;
	res.presentMargin		= presentMargin;
	return res;
}

inline VkPresentTimeGOOGLE makePresentTimeGOOGLE (deUint32 presentID, deUint64 desiredPresentTime)
{
	VkPresentTimeGOOGLE res;
	res.presentID			= presentID;
	res.desiredPresentTime	= desiredPresentTime;
	return res;
}

inline VkViewportSwizzleNV makeViewportSwizzleNV (VkViewportCoordinateSwizzleNV x, VkViewportCoordinateSwizzleNV y, VkViewportCoordinateSwizzleNV z, VkViewportCoordinateSwizzleNV w)
{
	VkViewportSwizzleNV res;
	res.x	= x;
	res.y	= y;
	res.z	= z;
	res.w	= w;
	return res;
}

inline VkXYColorEXT makeXYColorEXT (float x, float y)
{
	VkXYColorEXT res;
	res.x	= x;
	res.y	= y;
	return res;
}

inline VkSampleLocationEXT makeSampleLocationEXT (float x, float y)
{
	VkSampleLocationEXT res;
	res.x	= x;
	res.y	= y;
	return res;
}

inline VkDrmFormatModifierPropertiesEXT makeDrmFormatModifierPropertiesEXT (deUint64 drmFormatModifier, deUint32 drmFormatModifierPlaneCount, VkFormatFeatureFlags drmFormatModifierTilingFeatures)
{
	VkDrmFormatModifierPropertiesEXT res;
	res.drmFormatModifier				= drmFormatModifier;
	res.drmFormatModifierPlaneCount		= drmFormatModifierPlaneCount;
	res.drmFormatModifierTilingFeatures	= drmFormatModifierTilingFeatures;
	return res;
}

inline VkShadingRatePaletteNV makeShadingRatePaletteNV (deUint32 shadingRatePaletteEntryCount, const VkShadingRatePaletteEntryNV* pShadingRatePaletteEntries)
{
	VkShadingRatePaletteNV res;
	res.shadingRatePaletteEntryCount	= shadingRatePaletteEntryCount;
	res.pShadingRatePaletteEntries		= pShadingRatePaletteEntries;
	return res;
}

inline VkCoarseSampleLocationNV makeCoarseSampleLocationNV (deUint32 pixelX, deUint32 pixelY, deUint32 sample)
{
	VkCoarseSampleLocationNV res;
	res.pixelX	= pixelX;
	res.pixelY	= pixelY;
	res.sample	= sample;
	return res;
}

inline VkCoarseSampleOrderCustomNV makeCoarseSampleOrderCustomNV (VkShadingRatePaletteEntryNV shadingRate, deUint32 sampleCount, deUint32 sampleLocationCount, const VkCoarseSampleLocationNV* pSampleLocations)
{
	VkCoarseSampleOrderCustomNV res;
	res.shadingRate			= shadingRate;
	res.sampleCount			= sampleCount;
	res.sampleLocationCount	= sampleLocationCount;
	res.pSampleLocations	= pSampleLocations;
	return res;
}

inline VkAabbPositionsKHR makeAabbPositionsKHR (float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
{
	VkAabbPositionsKHR res;
	res.minX	= minX;
	res.minY	= minY;
	res.minZ	= minZ;
	res.maxX	= maxX;
	res.maxY	= maxY;
	res.maxZ	= maxZ;
	return res;
}

inline VkVertexInputBindingDivisorDescriptionEXT makeVertexInputBindingDivisorDescriptionEXT (deUint32 binding, deUint32 divisor)
{
	VkVertexInputBindingDivisorDescriptionEXT res;
	res.binding	= binding;
	res.divisor	= divisor;
	return res;
}

inline VkPipelineCreationFeedbackEXT makePipelineCreationFeedbackEXT (VkPipelineCreationFeedbackFlagsEXT flags, deUint64 duration)
{
	VkPipelineCreationFeedbackEXT res;
	res.flags		= flags;
	res.duration	= duration;
	return res;
}

inline VkDrawMeshTasksIndirectCommandNV makeDrawMeshTasksIndirectCommandNV (deUint32 taskCount, deUint32 firstTask)
{
	VkDrawMeshTasksIndirectCommandNV res;
	res.taskCount	= taskCount;
	res.firstTask	= firstTask;
	return res;
}

inline VkBindShaderGroupIndirectCommandNV makeBindShaderGroupIndirectCommandNV (deUint32 groupIndex)
{
	VkBindShaderGroupIndirectCommandNV res;
	res.groupIndex	= groupIndex;
	return res;
}

inline VkBindIndexBufferIndirectCommandNV makeBindIndexBufferIndirectCommandNV (VkDeviceAddress bufferAddress, deUint32 size, VkIndexType indexType)
{
	VkBindIndexBufferIndirectCommandNV res;
	res.bufferAddress	= bufferAddress;
	res.size			= size;
	res.indexType		= indexType;
	return res;
}

inline VkBindVertexBufferIndirectCommandNV makeBindVertexBufferIndirectCommandNV (VkDeviceAddress bufferAddress, deUint32 size, deUint32 stride)
{
	VkBindVertexBufferIndirectCommandNV res;
	res.bufferAddress	= bufferAddress;
	res.size			= size;
	res.stride			= stride;
	return res;
}

inline VkSetStateFlagsIndirectCommandNV makeSetStateFlagsIndirectCommandNV (deUint32 data)
{
	VkSetStateFlagsIndirectCommandNV res;
	res.data	= data;
	return res;
}

inline VkIndirectCommandsStreamNV makeIndirectCommandsStreamNV (VkBuffer buffer, VkDeviceSize offset)
{
	VkIndirectCommandsStreamNV res;
	res.buffer	= buffer;
	res.offset	= offset;
	return res;
}

inline VkAccelerationStructureBuildOffsetInfoKHR makeAccelerationStructureBuildOffsetInfoKHR (deUint32 primitiveCount, deUint32 primitiveOffset, deUint32 firstVertex, deUint32 transformOffset)
{
	VkAccelerationStructureBuildOffsetInfoKHR res;
	res.primitiveCount	= primitiveCount;
	res.primitiveOffset	= primitiveOffset;
	res.firstVertex		= firstVertex;
	res.transformOffset	= transformOffset;
	return res;
}

inline VkStridedBufferRegionKHR makeStridedBufferRegionKHR (VkBuffer buffer, VkDeviceSize offset, VkDeviceSize stride, VkDeviceSize size)
{
	VkStridedBufferRegionKHR res;
	res.buffer	= buffer;
	res.offset	= offset;
	res.stride	= stride;
	res.size	= size;
	return res;
}

inline VkTraceRaysIndirectCommandKHR makeTraceRaysIndirectCommandKHR (deUint32 width, deUint32 height, deUint32 depth)
{
	VkTraceRaysIndirectCommandKHR res;
	res.width	= width;
	res.height	= height;
	res.depth	= depth;
	return res;
}
