/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
bool checkMandatoryFeatures(const vkt::Context& context)
{
	if (!context.isInstanceFunctionalitySupported("VK_KHR_get_physical_device_properties2"))
		TCU_THROW(NotSupportedError, "Extension VK_KHR_get_physical_device_properties2 is not present");

	VkPhysicalDevice					physicalDevice		= context.getPhysicalDevice();
	const InstanceInterface&			vki					= context.getInstanceInterface();
	const vector<VkExtensionProperties>	deviceExtensions	= enumerateDeviceExtensionProperties(vki, physicalDevice, DE_NULL);

	tcu::TestLog& log = context.getTestContext().getLog();
	vk::VkPhysicalDeviceFeatures2 coreFeatures;
	deMemset(&coreFeatures, 0, sizeof(coreFeatures));
	coreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	void** nextPtr = &coreFeatures.pNext;

	vk::VkPhysicalDevice16BitStorageFeaturesKHR physicalDevice16BitStorageFeaturesKHR;
	deMemset(&physicalDevice16BitStorageFeaturesKHR, 0, sizeof(physicalDevice16BitStorageFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_16bit_storage")) )
	{
		physicalDevice16BitStorageFeaturesKHR.sType = getStructureType<VkPhysicalDevice16BitStorageFeaturesKHR>();
		*nextPtr = &physicalDevice16BitStorageFeaturesKHR;
		nextPtr  = &physicalDevice16BitStorageFeaturesKHR.pNext;
	}

	vk::VkPhysicalDevice4444FormatsFeaturesEXT physicalDevice4444FormatsFeaturesEXT;
	deMemset(&physicalDevice4444FormatsFeaturesEXT, 0, sizeof(physicalDevice4444FormatsFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_4444_formats")) )
	{
		physicalDevice4444FormatsFeaturesEXT.sType = getStructureType<VkPhysicalDevice4444FormatsFeaturesEXT>();
		*nextPtr = &physicalDevice4444FormatsFeaturesEXT;
		nextPtr  = &physicalDevice4444FormatsFeaturesEXT.pNext;
	}

	vk::VkPhysicalDevice8BitStorageFeaturesKHR physicalDevice8BitStorageFeaturesKHR;
	deMemset(&physicalDevice8BitStorageFeaturesKHR, 0, sizeof(physicalDevice8BitStorageFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_8bit_storage")) )
	{
		physicalDevice8BitStorageFeaturesKHR.sType = getStructureType<VkPhysicalDevice8BitStorageFeaturesKHR>();
		*nextPtr = &physicalDevice8BitStorageFeaturesKHR;
		nextPtr  = &physicalDevice8BitStorageFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceAccelerationStructureFeaturesKHR physicalDeviceAccelerationStructureFeaturesKHR;
	deMemset(&physicalDeviceAccelerationStructureFeaturesKHR, 0, sizeof(physicalDeviceAccelerationStructureFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		physicalDeviceAccelerationStructureFeaturesKHR.sType = getStructureType<VkPhysicalDeviceAccelerationStructureFeaturesKHR>();
		*nextPtr = &physicalDeviceAccelerationStructureFeaturesKHR;
		nextPtr  = &physicalDeviceAccelerationStructureFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceBorderColorSwizzleFeaturesEXT physicalDeviceBorderColorSwizzleFeaturesEXT;
	deMemset(&physicalDeviceBorderColorSwizzleFeaturesEXT, 0, sizeof(physicalDeviceBorderColorSwizzleFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_border_color_swizzle")) )
	{
		physicalDeviceBorderColorSwizzleFeaturesEXT.sType = getStructureType<VkPhysicalDeviceBorderColorSwizzleFeaturesEXT>();
		*nextPtr = &physicalDeviceBorderColorSwizzleFeaturesEXT;
		nextPtr  = &physicalDeviceBorderColorSwizzleFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceBufferDeviceAddressFeaturesKHR physicalDeviceBufferDeviceAddressFeaturesKHR;
	deMemset(&physicalDeviceBufferDeviceAddressFeaturesKHR, 0, sizeof(physicalDeviceBufferDeviceAddressFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_buffer_device_address")) || context.contextSupports(vk::ApiVersion(1, 1, 0)) )
	{
		physicalDeviceBufferDeviceAddressFeaturesKHR.sType = getStructureType<VkPhysicalDeviceBufferDeviceAddressFeaturesKHR>();
		*nextPtr = &physicalDeviceBufferDeviceAddressFeaturesKHR;
		nextPtr  = &physicalDeviceBufferDeviceAddressFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceColorWriteEnableFeaturesEXT physicalDeviceColorWriteEnableFeaturesEXT;
	deMemset(&physicalDeviceColorWriteEnableFeaturesEXT, 0, sizeof(physicalDeviceColorWriteEnableFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_color_write_enable")) )
	{
		physicalDeviceColorWriteEnableFeaturesEXT.sType = getStructureType<VkPhysicalDeviceColorWriteEnableFeaturesEXT>();
		*nextPtr = &physicalDeviceColorWriteEnableFeaturesEXT;
		nextPtr  = &physicalDeviceColorWriteEnableFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceConditionalRenderingFeaturesEXT physicalDeviceConditionalRenderingFeaturesEXT;
	deMemset(&physicalDeviceConditionalRenderingFeaturesEXT, 0, sizeof(physicalDeviceConditionalRenderingFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_conditional_rendering")) )
	{
		physicalDeviceConditionalRenderingFeaturesEXT.sType = getStructureType<VkPhysicalDeviceConditionalRenderingFeaturesEXT>();
		*nextPtr = &physicalDeviceConditionalRenderingFeaturesEXT;
		nextPtr  = &physicalDeviceConditionalRenderingFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceDepthClipEnableFeaturesEXT physicalDeviceDepthClipEnableFeaturesEXT;
	deMemset(&physicalDeviceDepthClipEnableFeaturesEXT, 0, sizeof(physicalDeviceDepthClipEnableFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_depth_clip_enable")) )
	{
		physicalDeviceDepthClipEnableFeaturesEXT.sType = getStructureType<VkPhysicalDeviceDepthClipEnableFeaturesEXT>();
		*nextPtr = &physicalDeviceDepthClipEnableFeaturesEXT;
		nextPtr  = &physicalDeviceDepthClipEnableFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeaturesEXT;
	deMemset(&physicalDeviceDescriptorIndexingFeaturesEXT, 0, sizeof(physicalDeviceDescriptorIndexingFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) || context.contextSupports(vk::ApiVersion(1, 1, 0)) )
	{
		physicalDeviceDescriptorIndexingFeaturesEXT.sType = getStructureType<VkPhysicalDeviceDescriptorIndexingFeaturesEXT>();
		*nextPtr = &physicalDeviceDescriptorIndexingFeaturesEXT;
		nextPtr  = &physicalDeviceDescriptorIndexingFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceExtendedDynamicState2FeaturesEXT physicalDeviceExtendedDynamicState2FeaturesEXT;
	deMemset(&physicalDeviceExtendedDynamicState2FeaturesEXT, 0, sizeof(physicalDeviceExtendedDynamicState2FeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_extended_dynamic_state2")) )
	{
		physicalDeviceExtendedDynamicState2FeaturesEXT.sType = getStructureType<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>();
		*nextPtr = &physicalDeviceExtendedDynamicState2FeaturesEXT;
		nextPtr  = &physicalDeviceExtendedDynamicState2FeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceFragmentDensityMapFeaturesEXT physicalDeviceFragmentDensityMapFeaturesEXT;
	deMemset(&physicalDeviceFragmentDensityMapFeaturesEXT, 0, sizeof(physicalDeviceFragmentDensityMapFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_fragment_density_map")) )
	{
		physicalDeviceFragmentDensityMapFeaturesEXT.sType = getStructureType<VkPhysicalDeviceFragmentDensityMapFeaturesEXT>();
		*nextPtr = &physicalDeviceFragmentDensityMapFeaturesEXT;
		nextPtr  = &physicalDeviceFragmentDensityMapFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT physicalDeviceFragmentShaderInterlockFeaturesEXT;
	deMemset(&physicalDeviceFragmentShaderInterlockFeaturesEXT, 0, sizeof(physicalDeviceFragmentShaderInterlockFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_fragment_shader_interlock")) )
	{
		physicalDeviceFragmentShaderInterlockFeaturesEXT.sType = getStructureType<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>();
		*nextPtr = &physicalDeviceFragmentShaderInterlockFeaturesEXT;
		nextPtr  = &physicalDeviceFragmentShaderInterlockFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeaturesEXT;
	deMemset(&physicalDeviceGlobalPriorityQueryFeaturesEXT, 0, sizeof(physicalDeviceGlobalPriorityQueryFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_global_priority_query")) )
	{
		physicalDeviceGlobalPriorityQueryFeaturesEXT.sType = getStructureType<VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT>();
		*nextPtr = &physicalDeviceGlobalPriorityQueryFeaturesEXT;
		nextPtr  = &physicalDeviceGlobalPriorityQueryFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceHostQueryResetFeaturesEXT physicalDeviceHostQueryResetFeaturesEXT;
	deMemset(&physicalDeviceHostQueryResetFeaturesEXT, 0, sizeof(physicalDeviceHostQueryResetFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_host_query_reset")) )
	{
		physicalDeviceHostQueryResetFeaturesEXT.sType = getStructureType<VkPhysicalDeviceHostQueryResetFeaturesEXT>();
		*nextPtr = &physicalDeviceHostQueryResetFeaturesEXT;
		nextPtr  = &physicalDeviceHostQueryResetFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceImagelessFramebufferFeaturesKHR physicalDeviceImagelessFramebufferFeaturesKHR;
	deMemset(&physicalDeviceImagelessFramebufferFeaturesKHR, 0, sizeof(physicalDeviceImagelessFramebufferFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_imageless_framebuffer")) )
	{
		physicalDeviceImagelessFramebufferFeaturesKHR.sType = getStructureType<VkPhysicalDeviceImagelessFramebufferFeaturesKHR>();
		*nextPtr = &physicalDeviceImagelessFramebufferFeaturesKHR;
		nextPtr  = &physicalDeviceImagelessFramebufferFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceIndexTypeUint8FeaturesEXT physicalDeviceIndexTypeUint8FeaturesEXT;
	deMemset(&physicalDeviceIndexTypeUint8FeaturesEXT, 0, sizeof(physicalDeviceIndexTypeUint8FeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_index_type_uint8")) )
	{
		physicalDeviceIndexTypeUint8FeaturesEXT.sType = getStructureType<VkPhysicalDeviceIndexTypeUint8FeaturesEXT>();
		*nextPtr = &physicalDeviceIndexTypeUint8FeaturesEXT;
		nextPtr  = &physicalDeviceIndexTypeUint8FeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceInlineUniformBlockFeaturesEXT physicalDeviceInlineUniformBlockFeaturesEXT;
	deMemset(&physicalDeviceInlineUniformBlockFeaturesEXT, 0, sizeof(physicalDeviceInlineUniformBlockFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_inline_uniform_block")) )
	{
		physicalDeviceInlineUniformBlockFeaturesEXT.sType = getStructureType<VkPhysicalDeviceInlineUniformBlockFeaturesEXT>();
		*nextPtr = &physicalDeviceInlineUniformBlockFeaturesEXT;
		nextPtr  = &physicalDeviceInlineUniformBlockFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceLineRasterizationFeaturesEXT physicalDeviceLineRasterizationFeaturesEXT;
	deMemset(&physicalDeviceLineRasterizationFeaturesEXT, 0, sizeof(physicalDeviceLineRasterizationFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_line_rasterization")) )
	{
		physicalDeviceLineRasterizationFeaturesEXT.sType = getStructureType<VkPhysicalDeviceLineRasterizationFeaturesEXT>();
		*nextPtr = &physicalDeviceLineRasterizationFeaturesEXT;
		nextPtr  = &physicalDeviceLineRasterizationFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceMemoryPriorityFeaturesEXT physicalDeviceMemoryPriorityFeaturesEXT;
	deMemset(&physicalDeviceMemoryPriorityFeaturesEXT, 0, sizeof(physicalDeviceMemoryPriorityFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_memory_priority")) )
	{
		physicalDeviceMemoryPriorityFeaturesEXT.sType = getStructureType<VkPhysicalDeviceMemoryPriorityFeaturesEXT>();
		*nextPtr = &physicalDeviceMemoryPriorityFeaturesEXT;
		nextPtr  = &physicalDeviceMemoryPriorityFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceMultiDrawFeaturesEXT physicalDeviceMultiDrawFeaturesEXT;
	deMemset(&physicalDeviceMultiDrawFeaturesEXT, 0, sizeof(physicalDeviceMultiDrawFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_multi_draw")) )
	{
		physicalDeviceMultiDrawFeaturesEXT.sType = getStructureType<VkPhysicalDeviceMultiDrawFeaturesEXT>();
		*nextPtr = &physicalDeviceMultiDrawFeaturesEXT;
		nextPtr  = &physicalDeviceMultiDrawFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures;
	deMemset(&physicalDeviceMultiviewFeatures, 0, sizeof(physicalDeviceMultiviewFeatures));

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) || isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_multiview")) )
	{
		physicalDeviceMultiviewFeatures.sType = getStructureType<VkPhysicalDeviceMultiviewFeatures>();
		*nextPtr = &physicalDeviceMultiviewFeatures;
		nextPtr  = &physicalDeviceMultiviewFeatures.pNext;
	}

	vk::VkPhysicalDeviceMutableDescriptorTypeFeaturesVALVE physicalDeviceMutableDescriptorTypeFeaturesVALVE;
	deMemset(&physicalDeviceMutableDescriptorTypeFeaturesVALVE, 0, sizeof(physicalDeviceMutableDescriptorTypeFeaturesVALVE));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_VALVE_mutable_descriptor_type")) )
	{
		physicalDeviceMutableDescriptorTypeFeaturesVALVE.sType = getStructureType<VkPhysicalDeviceMutableDescriptorTypeFeaturesVALVE>();
		*nextPtr = &physicalDeviceMutableDescriptorTypeFeaturesVALVE;
		nextPtr  = &physicalDeviceMutableDescriptorTypeFeaturesVALVE.pNext;
	}

	vk::VkPhysicalDevicePerformanceQueryFeaturesKHR physicalDevicePerformanceQueryFeaturesKHR;
	deMemset(&physicalDevicePerformanceQueryFeaturesKHR, 0, sizeof(physicalDevicePerformanceQueryFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_performance_query")) )
	{
		physicalDevicePerformanceQueryFeaturesKHR.sType = getStructureType<VkPhysicalDevicePerformanceQueryFeaturesKHR>();
		*nextPtr = &physicalDevicePerformanceQueryFeaturesKHR;
		nextPtr  = &physicalDevicePerformanceQueryFeaturesKHR.pNext;
	}

	vk::VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT physicalDevicePipelineCreationCacheControlFeaturesEXT;
	deMemset(&physicalDevicePipelineCreationCacheControlFeaturesEXT, 0, sizeof(physicalDevicePipelineCreationCacheControlFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_pipeline_creation_cache_control")) )
	{
		physicalDevicePipelineCreationCacheControlFeaturesEXT.sType = getStructureType<VkPhysicalDevicePipelineCreationCacheControlFeaturesEXT>();
		*nextPtr = &physicalDevicePipelineCreationCacheControlFeaturesEXT;
		nextPtr  = &physicalDevicePipelineCreationCacheControlFeaturesEXT.pNext;
	}

	vk::VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR physicalDevicePipelineExecutablePropertiesFeaturesKHR;
	deMemset(&physicalDevicePipelineExecutablePropertiesFeaturesKHR, 0, sizeof(physicalDevicePipelineExecutablePropertiesFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_pipeline_executable_properties")) )
	{
		physicalDevicePipelineExecutablePropertiesFeaturesKHR.sType = getStructureType<VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR>();
		*nextPtr = &physicalDevicePipelineExecutablePropertiesFeaturesKHR;
		nextPtr  = &physicalDevicePipelineExecutablePropertiesFeaturesKHR.pNext;
	}

	vk::VkPhysicalDevicePresentIdFeaturesKHR physicalDevicePresentIdFeaturesKHR;
	deMemset(&physicalDevicePresentIdFeaturesKHR, 0, sizeof(physicalDevicePresentIdFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_present_id")) )
	{
		physicalDevicePresentIdFeaturesKHR.sType = getStructureType<VkPhysicalDevicePresentIdFeaturesKHR>();
		*nextPtr = &physicalDevicePresentIdFeaturesKHR;
		nextPtr  = &physicalDevicePresentIdFeaturesKHR.pNext;
	}

	vk::VkPhysicalDevicePresentWaitFeaturesKHR physicalDevicePresentWaitFeaturesKHR;
	deMemset(&physicalDevicePresentWaitFeaturesKHR, 0, sizeof(physicalDevicePresentWaitFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_present_wait")) )
	{
		physicalDevicePresentWaitFeaturesKHR.sType = getStructureType<VkPhysicalDevicePresentWaitFeaturesKHR>();
		*nextPtr = &physicalDevicePresentWaitFeaturesKHR;
		nextPtr  = &physicalDevicePresentWaitFeaturesKHR.pNext;
	}

	vk::VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT physicalDevicePrimitiveTopologyListRestartFeaturesEXT;
	deMemset(&physicalDevicePrimitiveTopologyListRestartFeaturesEXT, 0, sizeof(physicalDevicePrimitiveTopologyListRestartFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_primitive_topology_list_restart")) )
	{
		physicalDevicePrimitiveTopologyListRestartFeaturesEXT.sType = getStructureType<VkPhysicalDevicePrimitiveTopologyListRestartFeaturesEXT>();
		*nextPtr = &physicalDevicePrimitiveTopologyListRestartFeaturesEXT;
		nextPtr  = &physicalDevicePrimitiveTopologyListRestartFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceRayQueryFeaturesKHR physicalDeviceRayQueryFeaturesKHR;
	deMemset(&physicalDeviceRayQueryFeaturesKHR, 0, sizeof(physicalDeviceRayQueryFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_ray_query")) )
	{
		physicalDeviceRayQueryFeaturesKHR.sType = getStructureType<VkPhysicalDeviceRayQueryFeaturesKHR>();
		*nextPtr = &physicalDeviceRayQueryFeaturesKHR;
		nextPtr  = &physicalDeviceRayQueryFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceRayTracingPipelineFeaturesKHR physicalDeviceRayTracingPipelineFeaturesKHR;
	deMemset(&physicalDeviceRayTracingPipelineFeaturesKHR, 0, sizeof(physicalDeviceRayTracingPipelineFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_ray_tracing_pipeline")) )
	{
		physicalDeviceRayTracingPipelineFeaturesKHR.sType = getStructureType<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>();
		*nextPtr = &physicalDeviceRayTracingPipelineFeaturesKHR;
		nextPtr  = &physicalDeviceRayTracingPipelineFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceSamplerYcbcrConversionFeaturesKHR physicalDeviceSamplerYcbcrConversionFeaturesKHR;
	deMemset(&physicalDeviceSamplerYcbcrConversionFeaturesKHR, 0, sizeof(physicalDeviceSamplerYcbcrConversionFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_sampler_ycbcr_conversion")) )
	{
		physicalDeviceSamplerYcbcrConversionFeaturesKHR.sType = getStructureType<VkPhysicalDeviceSamplerYcbcrConversionFeaturesKHR>();
		*nextPtr = &physicalDeviceSamplerYcbcrConversionFeaturesKHR;
		nextPtr  = &physicalDeviceSamplerYcbcrConversionFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceScalarBlockLayoutFeaturesEXT physicalDeviceScalarBlockLayoutFeaturesEXT;
	deMemset(&physicalDeviceScalarBlockLayoutFeaturesEXT, 0, sizeof(physicalDeviceScalarBlockLayoutFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_scalar_block_layout")) )
	{
		physicalDeviceScalarBlockLayoutFeaturesEXT.sType = getStructureType<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT>();
		*nextPtr = &physicalDeviceScalarBlockLayoutFeaturesEXT;
		nextPtr  = &physicalDeviceScalarBlockLayoutFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR physicalDeviceSeparateDepthStencilLayoutsFeaturesKHR;
	deMemset(&physicalDeviceSeparateDepthStencilLayoutsFeaturesKHR, 0, sizeof(physicalDeviceSeparateDepthStencilLayoutsFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_separate_depth_stencil_layouts")) )
	{
		physicalDeviceSeparateDepthStencilLayoutsFeaturesKHR.sType = getStructureType<VkPhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR>();
		*nextPtr = &physicalDeviceSeparateDepthStencilLayoutsFeaturesKHR;
		nextPtr  = &physicalDeviceSeparateDepthStencilLayoutsFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT physicalDeviceShaderAtomicFloat2FeaturesEXT;
	deMemset(&physicalDeviceShaderAtomicFloat2FeaturesEXT, 0, sizeof(physicalDeviceShaderAtomicFloat2FeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_atomic_float2")) )
	{
		physicalDeviceShaderAtomicFloat2FeaturesEXT.sType = getStructureType<VkPhysicalDeviceShaderAtomicFloat2FeaturesEXT>();
		*nextPtr = &physicalDeviceShaderAtomicFloat2FeaturesEXT;
		nextPtr  = &physicalDeviceShaderAtomicFloat2FeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceShaderAtomicFloatFeaturesEXT physicalDeviceShaderAtomicFloatFeaturesEXT;
	deMemset(&physicalDeviceShaderAtomicFloatFeaturesEXT, 0, sizeof(physicalDeviceShaderAtomicFloatFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_atomic_float")) )
	{
		physicalDeviceShaderAtomicFloatFeaturesEXT.sType = getStructureType<VkPhysicalDeviceShaderAtomicFloatFeaturesEXT>();
		*nextPtr = &physicalDeviceShaderAtomicFloatFeaturesEXT;
		nextPtr  = &physicalDeviceShaderAtomicFloatFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceShaderAtomicInt64FeaturesKHR physicalDeviceShaderAtomicInt64FeaturesKHR;
	deMemset(&physicalDeviceShaderAtomicInt64FeaturesKHR, 0, sizeof(physicalDeviceShaderAtomicInt64FeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_atomic_int64")) )
	{
		physicalDeviceShaderAtomicInt64FeaturesKHR.sType = getStructureType<VkPhysicalDeviceShaderAtomicInt64FeaturesKHR>();
		*nextPtr = &physicalDeviceShaderAtomicInt64FeaturesKHR;
		nextPtr  = &physicalDeviceShaderAtomicInt64FeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceShaderClockFeaturesKHR physicalDeviceShaderClockFeaturesKHR;
	deMemset(&physicalDeviceShaderClockFeaturesKHR, 0, sizeof(physicalDeviceShaderClockFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_clock")) )
	{
		physicalDeviceShaderClockFeaturesKHR.sType = getStructureType<VkPhysicalDeviceShaderClockFeaturesKHR>();
		*nextPtr = &physicalDeviceShaderClockFeaturesKHR;
		nextPtr  = &physicalDeviceShaderClockFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT physicalDeviceShaderDemoteToHelperInvocationFeaturesEXT;
	deMemset(&physicalDeviceShaderDemoteToHelperInvocationFeaturesEXT, 0, sizeof(physicalDeviceShaderDemoteToHelperInvocationFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_demote_to_helper_invocation")) )
	{
		physicalDeviceShaderDemoteToHelperInvocationFeaturesEXT.sType = getStructureType<VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT>();
		*nextPtr = &physicalDeviceShaderDemoteToHelperInvocationFeaturesEXT;
		nextPtr  = &physicalDeviceShaderDemoteToHelperInvocationFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceShaderFloat16Int8Features physicalDeviceShaderFloat16Int8Features;
	deMemset(&physicalDeviceShaderFloat16Int8Features, 0, sizeof(physicalDeviceShaderFloat16Int8Features));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_float16_int8")) )
	{
		physicalDeviceShaderFloat16Int8Features.sType = getStructureType<VkPhysicalDeviceShaderFloat16Int8Features>();
		*nextPtr = &physicalDeviceShaderFloat16Int8Features;
		nextPtr  = &physicalDeviceShaderFloat16Int8Features.pNext;
	}

	vk::VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT physicalDeviceShaderImageAtomicInt64FeaturesEXT;
	deMemset(&physicalDeviceShaderImageAtomicInt64FeaturesEXT, 0, sizeof(physicalDeviceShaderImageAtomicInt64FeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_image_atomic_int64")) )
	{
		physicalDeviceShaderImageAtomicInt64FeaturesEXT.sType = getStructureType<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT>();
		*nextPtr = &physicalDeviceShaderImageAtomicInt64FeaturesEXT;
		nextPtr  = &physicalDeviceShaderImageAtomicInt64FeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceShaderIntegerDotProductFeaturesKHR physicalDeviceShaderIntegerDotProductFeaturesKHR;
	deMemset(&physicalDeviceShaderIntegerDotProductFeaturesKHR, 0, sizeof(physicalDeviceShaderIntegerDotProductFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_integer_dot_product")) )
	{
		physicalDeviceShaderIntegerDotProductFeaturesKHR.sType = getStructureType<VkPhysicalDeviceShaderIntegerDotProductFeaturesKHR>();
		*nextPtr = &physicalDeviceShaderIntegerDotProductFeaturesKHR;
		nextPtr  = &physicalDeviceShaderIntegerDotProductFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR physicalDeviceShaderSubgroupExtendedTypesFeaturesKHR;
	deMemset(&physicalDeviceShaderSubgroupExtendedTypesFeaturesKHR, 0, sizeof(physicalDeviceShaderSubgroupExtendedTypesFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_subgroup_extended_types")) )
	{
		physicalDeviceShaderSubgroupExtendedTypesFeaturesKHR.sType = getStructureType<VkPhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR>();
		*nextPtr = &physicalDeviceShaderSubgroupExtendedTypesFeaturesKHR;
		nextPtr  = &physicalDeviceShaderSubgroupExtendedTypesFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR physicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR;
	deMemset(&physicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR, 0, sizeof(physicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_subgroup_uniform_control_flow")) )
	{
		physicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR.sType = getStructureType<VkPhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR>();
		*nextPtr = &physicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR;
		nextPtr  = &physicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceShaderTerminateInvocationFeaturesKHR physicalDeviceShaderTerminateInvocationFeaturesKHR;
	deMemset(&physicalDeviceShaderTerminateInvocationFeaturesKHR, 0, sizeof(physicalDeviceShaderTerminateInvocationFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_terminate_invocation")) )
	{
		physicalDeviceShaderTerminateInvocationFeaturesKHR.sType = getStructureType<VkPhysicalDeviceShaderTerminateInvocationFeaturesKHR>();
		*nextPtr = &physicalDeviceShaderTerminateInvocationFeaturesKHR;
		nextPtr  = &physicalDeviceShaderTerminateInvocationFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceSubgroupSizeControlFeaturesEXT physicalDeviceSubgroupSizeControlFeaturesEXT;
	deMemset(&physicalDeviceSubgroupSizeControlFeaturesEXT, 0, sizeof(physicalDeviceSubgroupSizeControlFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_subgroup_size_control")) )
	{
		physicalDeviceSubgroupSizeControlFeaturesEXT.sType = getStructureType<VkPhysicalDeviceSubgroupSizeControlFeaturesEXT>();
		*nextPtr = &physicalDeviceSubgroupSizeControlFeaturesEXT;
		nextPtr  = &physicalDeviceSubgroupSizeControlFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceSynchronization2FeaturesKHR physicalDeviceSynchronization2FeaturesKHR;
	deMemset(&physicalDeviceSynchronization2FeaturesKHR, 0, sizeof(physicalDeviceSynchronization2FeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_synchronization2")) )
	{
		physicalDeviceSynchronization2FeaturesKHR.sType = getStructureType<VkPhysicalDeviceSynchronization2FeaturesKHR>();
		*nextPtr = &physicalDeviceSynchronization2FeaturesKHR;
		nextPtr  = &physicalDeviceSynchronization2FeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT physicalDeviceTexelBufferAlignmentFeaturesEXT;
	deMemset(&physicalDeviceTexelBufferAlignmentFeaturesEXT, 0, sizeof(physicalDeviceTexelBufferAlignmentFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_texel_buffer_alignment")) )
	{
		physicalDeviceTexelBufferAlignmentFeaturesEXT.sType = getStructureType<VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT>();
		*nextPtr = &physicalDeviceTexelBufferAlignmentFeaturesEXT;
		nextPtr  = &physicalDeviceTexelBufferAlignmentFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceTextureCompressionASTCHDRFeaturesEXT physicalDeviceTextureCompressionASTCHDRFeaturesEXT;
	deMemset(&physicalDeviceTextureCompressionASTCHDRFeaturesEXT, 0, sizeof(physicalDeviceTextureCompressionASTCHDRFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_texture_compression_astc_hdr")) )
	{
		physicalDeviceTextureCompressionASTCHDRFeaturesEXT.sType = getStructureType<VkPhysicalDeviceTextureCompressionASTCHDRFeaturesEXT>();
		*nextPtr = &physicalDeviceTextureCompressionASTCHDRFeaturesEXT;
		nextPtr  = &physicalDeviceTextureCompressionASTCHDRFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceTimelineSemaphoreFeaturesKHR physicalDeviceTimelineSemaphoreFeaturesKHR;
	deMemset(&physicalDeviceTimelineSemaphoreFeaturesKHR, 0, sizeof(physicalDeviceTimelineSemaphoreFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_timeline_semaphore")) )
	{
		physicalDeviceTimelineSemaphoreFeaturesKHR.sType = getStructureType<VkPhysicalDeviceTimelineSemaphoreFeaturesKHR>();
		*nextPtr = &physicalDeviceTimelineSemaphoreFeaturesKHR;
		nextPtr  = &physicalDeviceTimelineSemaphoreFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceTransformFeedbackFeaturesEXT physicalDeviceTransformFeedbackFeaturesEXT;
	deMemset(&physicalDeviceTransformFeedbackFeaturesEXT, 0, sizeof(physicalDeviceTransformFeedbackFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_transform_feedback")) )
	{
		physicalDeviceTransformFeedbackFeaturesEXT.sType = getStructureType<VkPhysicalDeviceTransformFeedbackFeaturesEXT>();
		*nextPtr = &physicalDeviceTransformFeedbackFeaturesEXT;
		nextPtr  = &physicalDeviceTransformFeedbackFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR physicalDeviceUniformBufferStandardLayoutFeaturesKHR;
	deMemset(&physicalDeviceUniformBufferStandardLayoutFeaturesKHR, 0, sizeof(physicalDeviceUniformBufferStandardLayoutFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_uniform_buffer_standard_layout")) )
	{
		physicalDeviceUniformBufferStandardLayoutFeaturesKHR.sType = getStructureType<VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR>();
		*nextPtr = &physicalDeviceUniformBufferStandardLayoutFeaturesKHR;
		nextPtr  = &physicalDeviceUniformBufferStandardLayoutFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceVariablePointersFeatures physicalDeviceVariablePointersFeatures;
	deMemset(&physicalDeviceVariablePointersFeatures, 0, sizeof(physicalDeviceVariablePointersFeatures));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_variable_pointers")) )
	{
		physicalDeviceVariablePointersFeatures.sType = getStructureType<VkPhysicalDeviceVariablePointersFeatures>();
		*nextPtr = &physicalDeviceVariablePointersFeatures;
		nextPtr  = &physicalDeviceVariablePointersFeatures.pNext;
	}

	vk::VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT physicalDeviceVertexAttributeDivisorFeaturesEXT;
	deMemset(&physicalDeviceVertexAttributeDivisorFeaturesEXT, 0, sizeof(physicalDeviceVertexAttributeDivisorFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_vertex_attribute_divisor")) )
	{
		physicalDeviceVertexAttributeDivisorFeaturesEXT.sType = getStructureType<VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT>();
		*nextPtr = &physicalDeviceVertexAttributeDivisorFeaturesEXT;
		nextPtr  = &physicalDeviceVertexAttributeDivisorFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT physicalDeviceVertexInputDynamicStateFeaturesEXT;
	deMemset(&physicalDeviceVertexInputDynamicStateFeaturesEXT, 0, sizeof(physicalDeviceVertexInputDynamicStateFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_vertex_input_dynamic_state")) )
	{
		physicalDeviceVertexInputDynamicStateFeaturesEXT.sType = getStructureType<VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT>();
		*nextPtr = &physicalDeviceVertexInputDynamicStateFeaturesEXT;
		nextPtr  = &physicalDeviceVertexInputDynamicStateFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceVulkan11Features physicalDeviceVulkan11Features;
	deMemset(&physicalDeviceVulkan11Features, 0, sizeof(physicalDeviceVulkan11Features));

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) )
	{
		physicalDeviceVulkan11Features.sType = getStructureType<VkPhysicalDeviceVulkan11Features>();
		*nextPtr = &physicalDeviceVulkan11Features;
		nextPtr  = &physicalDeviceVulkan11Features.pNext;
	}

	vk::VkPhysicalDeviceVulkan12Features physicalDeviceVulkan12Features;
	deMemset(&physicalDeviceVulkan12Features, 0, sizeof(physicalDeviceVulkan12Features));

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) )
	{
		physicalDeviceVulkan12Features.sType = getStructureType<VkPhysicalDeviceVulkan12Features>();
		*nextPtr = &physicalDeviceVulkan12Features;
		nextPtr  = &physicalDeviceVulkan12Features.pNext;
	}

	vk::VkPhysicalDeviceVulkanMemoryModelFeaturesKHR physicalDeviceVulkanMemoryModelFeaturesKHR;
	deMemset(&physicalDeviceVulkanMemoryModelFeaturesKHR, 0, sizeof(physicalDeviceVulkanMemoryModelFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_vulkan_memory_model")) )
	{
		physicalDeviceVulkanMemoryModelFeaturesKHR.sType = getStructureType<VkPhysicalDeviceVulkanMemoryModelFeaturesKHR>();
		*nextPtr = &physicalDeviceVulkanMemoryModelFeaturesKHR;
		nextPtr  = &physicalDeviceVulkanMemoryModelFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR;
	deMemset(&physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR, 0, sizeof(physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_workgroup_memory_explicit_layout")) )
	{
		physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR.sType = getStructureType<VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR>();
		*nextPtr = &physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR;
		nextPtr  = &physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR.pNext;
	}

	vk::VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT physicalDeviceYcbcr2Plane444FormatsFeaturesEXT;
	deMemset(&physicalDeviceYcbcr2Plane444FormatsFeaturesEXT, 0, sizeof(physicalDeviceYcbcr2Plane444FormatsFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_ycbcr_2plane_444_formats")) )
	{
		physicalDeviceYcbcr2Plane444FormatsFeaturesEXT.sType = getStructureType<VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT>();
		*nextPtr = &physicalDeviceYcbcr2Plane444FormatsFeaturesEXT;
		nextPtr  = &physicalDeviceYcbcr2Plane444FormatsFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceYcbcrImageArraysFeaturesEXT physicalDeviceYcbcrImageArraysFeaturesEXT;
	deMemset(&physicalDeviceYcbcrImageArraysFeaturesEXT, 0, sizeof(physicalDeviceYcbcrImageArraysFeaturesEXT));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_ycbcr_image_arrays")) )
	{
		physicalDeviceYcbcrImageArraysFeaturesEXT.sType = getStructureType<VkPhysicalDeviceYcbcrImageArraysFeaturesEXT>();
		*nextPtr = &physicalDeviceYcbcrImageArraysFeaturesEXT;
		nextPtr  = &physicalDeviceYcbcrImageArraysFeaturesEXT.pNext;
	}

	vk::VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR physicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR;
	deMemset(&physicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR, 0, sizeof(physicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR));

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_zero_initialize_workgroup_memory")) )
	{
		physicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR.sType = getStructureType<VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR>();
		*nextPtr = &physicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR;
		nextPtr  = &physicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR.pNext;
	}

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &coreFeatures);
	bool result = true;

	{
		if ( coreFeatures.features.robustBufferAccess == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature robustBufferAccess not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( coreFeatures.features.shaderSampledImageArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( coreFeatures.features.shaderStorageBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_8bit_storage")) )
	{
		if ( physicalDevice8BitStorageFeaturesKHR.storageBuffer8BitAccess == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature storageBuffer8BitAccess not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) )
	{
		if ( physicalDeviceVulkan11Features.multiview == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature multiview not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) )
	{
		if ( physicalDeviceMultiviewFeatures.multiview == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature multiview not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_multiview")) )
	{
		if ( physicalDeviceMultiviewFeatures.multiview == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature multiview not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_variable_pointers")) )
	{
		if ( physicalDeviceVariablePointersFeatures.variablePointersStorageBuffer == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature variablePointersStorageBuffer not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderUniformTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderStorageTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderSampledImageArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderStorageBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderUniformTexelBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingSampledImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingSampledImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingUniformTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUniformTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingUpdateUnusedWhilePending == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUpdateUnusedWhilePending not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingPartiallyBound == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingPartiallyBound not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.runtimeDescriptorArray == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature runtimeDescriptorArray not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_inline_uniform_block")) )
	{
		if ( physicalDeviceInlineUniformBlockFeaturesEXT.inlineUniformBlock == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature inlineUniformBlock not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_inline_uniform_block")) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_descriptor_indexing")) )
	{
		if ( physicalDeviceInlineUniformBlockFeaturesEXT.descriptorBindingInlineUniformBlockUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingInlineUniformBlockUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_scalar_block_layout")) )
	{
		if ( physicalDeviceScalarBlockLayoutFeaturesEXT.scalarBlockLayout == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature scalarBlockLayout not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_uniform_buffer_standard_layout")) )
	{
		if ( physicalDeviceUniformBufferStandardLayoutFeaturesKHR.uniformBufferStandardLayout == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature uniformBufferStandardLayout not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_pipeline_executable_properties")) )
	{
		if ( physicalDevicePipelineExecutablePropertiesFeaturesKHR.pipelineExecutableInfo == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature pipelineExecutableInfo not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_subgroup_size_control")) )
	{
		if ( physicalDeviceSubgroupSizeControlFeaturesEXT.subgroupSizeControl == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature subgroupSizeControl not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_subgroup_size_control")) )
	{
		if ( physicalDeviceSubgroupSizeControlFeaturesEXT.computeFullSubgroups == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature computeFullSubgroups not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) )
	{
		if ( physicalDeviceVulkan12Features.subgroupBroadcastDynamicId == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature subgroupBroadcastDynamicId not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_subgroup_extended_types")) )
	{
		if ( physicalDeviceShaderSubgroupExtendedTypesFeaturesKHR.shaderSubgroupExtendedTypes == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSubgroupExtendedTypes not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) )
	{
		if ( physicalDeviceVulkan12Features.shaderSubgroupExtendedTypes == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSubgroupExtendedTypes not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_imageless_framebuffer")) )
	{
		if ( physicalDeviceImagelessFramebufferFeaturesKHR.imagelessFramebuffer == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature imagelessFramebuffer not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) )
	{
		if ( physicalDeviceVulkan12Features.imagelessFramebuffer == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature imagelessFramebuffer not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) )
	{
		if ( physicalDeviceVulkan12Features.uniformBufferStandardLayout == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature uniformBufferStandardLayout not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_separate_depth_stencil_layouts")) )
	{
		if ( physicalDeviceSeparateDepthStencilLayoutsFeaturesKHR.separateDepthStencilLayouts == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature separateDepthStencilLayouts not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_pipeline_creation_cache_control")) )
	{
		if ( physicalDevicePipelineCreationCacheControlFeaturesEXT.pipelineCreationCacheControl == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature pipelineCreationCacheControl not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) )
	{
		if ( physicalDeviceVulkan12Features.separateDepthStencilLayouts == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature separateDepthStencilLayouts not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_host_query_reset")) )
	{
		if ( physicalDeviceHostQueryResetFeaturesEXT.hostQueryReset == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature hostQueryReset not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) )
	{
		if ( physicalDeviceVulkan12Features.hostQueryReset == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature hostQueryReset not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_timeline_semaphore")) )
	{
		if ( physicalDeviceTimelineSemaphoreFeaturesKHR.timelineSemaphore == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature timelineSemaphore not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) )
	{
		if ( physicalDeviceVulkan12Features.timelineSemaphore == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature timelineSemaphore not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( coreFeatures.features.shaderSampledImageArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( coreFeatures.features.shaderStorageBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.shaderUniformTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.shaderStorageTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.shaderSampledImageArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.shaderStorageBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.shaderUniformTexelBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingSampledImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingSampledImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingStorageImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingStorageBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingUniformTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUniformTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingStorageTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingUpdateUnusedWhilePending == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUpdateUnusedWhilePending not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingPartiallyBound == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingPartiallyBound not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceVulkan12Features.descriptorIndexing )
	{
		if ( physicalDeviceVulkan12Features.runtimeDescriptorArray == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature runtimeDescriptorArray not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_texture_compression_astc_hdr")) )
	{
		if ( physicalDeviceTextureCompressionASTCHDRFeaturesEXT.textureCompressionASTC_HDR == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature textureCompressionASTC_HDR not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_depth_clip_enable")) )
	{
		if ( physicalDeviceDepthClipEnableFeaturesEXT.depthClipEnable == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature depthClipEnable not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_memory_priority")) )
	{
		if ( physicalDeviceMemoryPriorityFeaturesEXT.memoryPriority == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature memoryPriority not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_ycbcr_image_arrays")) )
	{
		if ( physicalDeviceYcbcrImageArraysFeaturesEXT.ycbcrImageArrays == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature ycbcrImageArrays not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_index_type_uint8")) )
	{
		if ( physicalDeviceIndexTypeUint8FeaturesEXT.indexTypeUint8 == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature indexTypeUint8 not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_demote_to_helper_invocation")) )
	{
		if ( physicalDeviceShaderDemoteToHelperInvocationFeaturesEXT.shaderDemoteToHelperInvocation == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderDemoteToHelperInvocation not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_texel_buffer_alignment")) )
	{
		if ( physicalDeviceTexelBufferAlignmentFeaturesEXT.texelBufferAlignment == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature texelBufferAlignment not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_vulkan_memory_model")) )
	{
		if ( physicalDeviceVulkanMemoryModelFeaturesKHR.vulkanMemoryModel == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature vulkanMemoryModel not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_buffer_device_address")) )
	{
		if ( physicalDeviceBufferDeviceAddressFeaturesKHR.bufferDeviceAddress == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature bufferDeviceAddress not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_performance_query")) )
	{
		if ( physicalDevicePerformanceQueryFeaturesKHR.performanceCounterQueryPools == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature performanceCounterQueryPools not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_transform_feedback")) )
	{
		if ( physicalDeviceTransformFeedbackFeaturesEXT.transformFeedback == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature transformFeedback not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_conditional_rendering")) )
	{
		if ( physicalDeviceConditionalRenderingFeaturesEXT.conditionalRendering == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature conditionalRendering not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_vertex_attribute_divisor")) )
	{
		if ( physicalDeviceVertexAttributeDivisorFeaturesEXT.vertexAttributeInstanceRateDivisor == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature vertexAttributeInstanceRateDivisor not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_fragment_density_map")) )
	{
		if ( physicalDeviceFragmentDensityMapFeaturesEXT.fragmentDensityMap == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature fragmentDensityMap not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_clock")) )
	{
		if ( physicalDeviceShaderClockFeaturesKHR.shaderSubgroupClock == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSubgroupClock not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_atomic_int64")) )
	{
		if ( physicalDeviceShaderAtomicInt64FeaturesKHR.shaderBufferInt64Atomics == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderBufferInt64Atomics not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && physicalDeviceShaderAtomicInt64FeaturesKHR.shaderBufferInt64Atomics )
	{
		if ( physicalDeviceVulkan12Features.shaderBufferInt64Atomics == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderBufferInt64Atomics not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_16bit_storage")) )
	{
		if ( physicalDevice16BitStorageFeaturesKHR.storageBuffer16BitAccess == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature storageBuffer16BitAccess not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_float16_int8")) )
	{
		if ( ( physicalDeviceShaderFloat16Int8Features.shaderFloat16 == VK_FALSE ) && ( physicalDeviceShaderFloat16Int8Features.shaderInt8 == VK_FALSE ) )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderFloat16 or shaderInt8 not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_fragment_shader_interlock")) )
	{
		if ( ( physicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderSampleInterlock == VK_FALSE ) && ( physicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderPixelInterlock == VK_FALSE ) && ( physicalDeviceFragmentShaderInterlockFeaturesEXT.fragmentShaderShadingRateInterlock == VK_FALSE ) )
		{
			log << tcu::TestLog::Message << "Mandatory feature fragmentShaderSampleInterlock or fragmentShaderPixelInterlock or fragmentShaderShadingRateInterlock not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_line_rasterization")) )
	{
		if ( ( physicalDeviceLineRasterizationFeaturesEXT.rectangularLines == VK_FALSE ) && ( physicalDeviceLineRasterizationFeaturesEXT.bresenhamLines == VK_FALSE ) && ( physicalDeviceLineRasterizationFeaturesEXT.smoothLines == VK_FALSE ) && ( physicalDeviceLineRasterizationFeaturesEXT.stippledRectangularLines == VK_FALSE ) && ( physicalDeviceLineRasterizationFeaturesEXT.stippledBresenhamLines == VK_FALSE ) && ( physicalDeviceLineRasterizationFeaturesEXT.stippledSmoothLines == VK_FALSE ) )
		{
			log << tcu::TestLog::Message << "Mandatory feature rectangularLines or bresenhamLines or smoothLines or stippledRectangularLines or stippledBresenhamLines or stippledSmoothLines not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_synchronization2")) )
	{
		if ( physicalDeviceSynchronization2FeaturesKHR.synchronization2 == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature synchronization2 not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_sampler_ycbcr_conversion")) )
	{
		if ( physicalDeviceSamplerYcbcrConversionFeaturesKHR.samplerYcbcrConversion == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature samplerYcbcrConversion not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_draw_indirect_count")) )
	{
		if ( physicalDeviceVulkan12Features.drawIndirectCount == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature drawIndirectCount not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_sampler_mirror_clamp_to_edge")) )
	{
		if ( physicalDeviceVulkan12Features.samplerMirrorClampToEdge == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature samplerMirrorClampToEdge not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_sampler_filter_minmax")) )
	{
		if ( physicalDeviceVulkan12Features.samplerFilterMinmax == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature samplerFilterMinmax not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_viewport_index_layer")) )
	{
		if ( physicalDeviceVulkan12Features.shaderOutputViewportIndex == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderOutputViewportIndex not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_viewport_index_layer")) )
	{
		if ( physicalDeviceVulkan12Features.shaderOutputLayer == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderOutputLayer not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_terminate_invocation")) )
	{
		if ( physicalDeviceShaderTerminateInvocationFeaturesKHR.shaderTerminateInvocation == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderTerminateInvocation not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_image_atomic_int64")) )
	{
		if ( physicalDeviceShaderImageAtomicInt64FeaturesEXT.shaderImageInt64Atomics == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderImageInt64Atomics not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_image_atomic_int64")) )
	{
		if ( coreFeatures.features.shaderInt64 == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderInt64 not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( physicalDeviceShaderAtomicInt64FeaturesKHR.shaderBufferInt64Atomics )
	{
		if ( coreFeatures.features.shaderInt64 == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderInt64 not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( physicalDeviceVulkan12Features.shaderBufferInt64Atomics )
	{
		if ( coreFeatures.features.shaderInt64 == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderInt64 not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( physicalDeviceShaderAtomicInt64FeaturesKHR.shaderSharedInt64Atomics )
	{
		if ( coreFeatures.features.shaderInt64 == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderInt64 not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( physicalDeviceVulkan12Features.shaderSharedInt64Atomics )
	{
		if ( coreFeatures.features.shaderInt64 == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderInt64 not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_4444_formats")) )
	{
		if ( physicalDevice4444FormatsFeaturesEXT.formatA4R4G4B4 == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature formatA4R4G4B4 not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_zero_initialize_workgroup_memory")) )
	{
		if ( physicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR.shaderZeroInitializeWorkgroupMemory == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderZeroInitializeWorkgroupMemory not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_workgroup_memory_explicit_layout")) )
	{
		if ( ( physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR.workgroupMemoryExplicitLayout == VK_FALSE ) && ( physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR.workgroupMemoryExplicitLayoutScalarBlockLayout == VK_FALSE ) && ( physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR.workgroupMemoryExplicitLayout8BitAccess == VK_FALSE ) && ( physicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR.workgroupMemoryExplicitLayout16BitAccess == VK_FALSE ) )
		{
			log << tcu::TestLog::Message << "Mandatory feature workgroupMemoryExplicitLayout or workgroupMemoryExplicitLayoutScalarBlockLayout or workgroupMemoryExplicitLayout8BitAccess or workgroupMemoryExplicitLayout16BitAccess not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature accelerationStructure not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceAccelerationStructureFeaturesKHR.descriptorBindingAccelerationStructureUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingAccelerationStructureUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( coreFeatures.features.shaderSampledImageArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( coreFeatures.features.shaderStorageBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.shaderUniformTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.shaderStorageTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.shaderSampledImageArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.shaderStorageBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.shaderUniformTexelBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingSampledImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingSampledImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingStorageImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingStorageBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingUniformTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUniformTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingStorageTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingUpdateUnusedWhilePending == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUpdateUnusedWhilePending not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.descriptorBindingPartiallyBound == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingPartiallyBound not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.runtimeDescriptorArray == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature runtimeDescriptorArray not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 2, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceVulkan12Features.bufferDeviceAddress == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature bufferDeviceAddress not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceBufferDeviceAddressFeaturesKHR.bufferDeviceAddress == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature bufferDeviceAddress not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderUniformTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderStorageTexelBufferArrayDynamicIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageTexelBufferArrayDynamicIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderSampledImageArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSampledImageArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderStorageBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderStorageBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.shaderUniformTexelBufferArrayNonUniformIndexing == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderUniformTexelBufferArrayNonUniformIndexing not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingSampledImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingSampledImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageImageUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageImageUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingUniformTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUniformTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingStorageTexelBufferUpdateAfterBind == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingStorageTexelBufferUpdateAfterBind not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingUpdateUnusedWhilePending == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingUpdateUnusedWhilePending not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.descriptorBindingPartiallyBound == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature descriptorBindingPartiallyBound not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( context.contextSupports(vk::ApiVersion(1, 1, 0)) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_acceleration_structure")) )
	{
		if ( physicalDeviceDescriptorIndexingFeaturesEXT.runtimeDescriptorArray == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature runtimeDescriptorArray not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_ray_tracing_pipeline")) )
	{
		if ( ( physicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline == VK_FALSE ) && ( physicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect == VK_FALSE ) )
		{
			log << tcu::TestLog::Message << "Mandatory feature rayTracingPipeline or rayTracingPipelineTraceRaysIndirect not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_ray_tracing_pipeline")) && isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_ray_query")) )
	{
		if ( physicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature rayTraversalPrimitiveCulling not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_ray_query")) )
	{
		if ( physicalDeviceRayQueryFeaturesKHR.rayQuery == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature rayQuery not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_vertex_input_dynamic_state")) )
	{
		if ( physicalDeviceVertexInputDynamicStateFeaturesEXT.vertexInputDynamicState == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature vertexInputDynamicState not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_color_write_enable")) )
	{
		if ( physicalDeviceColorWriteEnableFeaturesEXT.colorWriteEnable == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature colorWriteEnable not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_ycbcr_2plane_444_formats")) )
	{
		if ( physicalDeviceYcbcr2Plane444FormatsFeaturesEXT.ycbcr2plane444Formats == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature ycbcr2plane444Formats not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_extended_dynamic_state2")) )
	{
		if ( physicalDeviceExtendedDynamicState2FeaturesEXT.extendedDynamicState2 == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature extendedDynamicState2 not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_VALVE_mutable_descriptor_type")) )
	{
		if ( physicalDeviceMutableDescriptorTypeFeaturesVALVE.mutableDescriptorType == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature mutableDescriptorType not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_global_priority_query")) )
	{
		if ( physicalDeviceGlobalPriorityQueryFeaturesEXT.globalPriorityQuery == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature globalPriorityQuery not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_image_atomic_int64")) && physicalDeviceShaderImageAtomicInt64FeaturesEXT.sparseImageInt64Atomics )
	{
		if ( physicalDeviceShaderImageAtomicInt64FeaturesEXT.shaderImageInt64Atomics == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderImageInt64Atomics not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_atomic_float")) && physicalDeviceShaderAtomicFloatFeaturesEXT.sparseImageFloat32Atomics )
	{
		if ( physicalDeviceShaderAtomicFloatFeaturesEXT.shaderImageFloat32Atomics == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderImageFloat32Atomics not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_atomic_float")) && physicalDeviceShaderAtomicFloatFeaturesEXT.sparseImageFloat32AtomicAdd )
	{
		if ( physicalDeviceShaderAtomicFloatFeaturesEXT.shaderImageFloat32AtomicAdd == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderImageFloat32AtomicAdd not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_multi_draw")) )
	{
		if ( physicalDeviceMultiDrawFeaturesEXT.multiDraw == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature multiDraw not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_subgroup_uniform_control_flow")) )
	{
		if ( physicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR.shaderSubgroupUniformControlFlow == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderSubgroupUniformControlFlow not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_present_id")) )
	{
		if ( physicalDevicePresentIdFeaturesKHR.presentId == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature presentId not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_present_wait")) )
	{
		if ( physicalDevicePresentWaitFeaturesKHR.presentWait == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature presentWait not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_shader_atomic_float2")) && physicalDeviceShaderAtomicFloat2FeaturesEXT.sparseImageFloat32AtomicMinMax )
	{
		if ( physicalDeviceShaderAtomicFloat2FeaturesEXT.shaderImageFloat32AtomicMinMax == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderImageFloat32AtomicMinMax not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_KHR_shader_integer_dot_product")) )
	{
		if ( physicalDeviceShaderIntegerDotProductFeaturesKHR.shaderIntegerDotProduct == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature shaderIntegerDotProduct not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_primitive_topology_list_restart")) )
	{
		if ( physicalDevicePrimitiveTopologyListRestartFeaturesEXT.primitiveTopologyListRestart == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature primitiveTopologyListRestart not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	if ( isExtensionSupported(deviceExtensions, RequiredExtension("VK_EXT_border_color_swizzle")) )
	{
		if ( physicalDeviceBorderColorSwizzleFeaturesEXT.borderColorSwizzle == VK_FALSE )
		{
			log << tcu::TestLog::Message << "Mandatory feature borderColorSwizzle not supported" << tcu::TestLog::EndMessage;
			result = false;
		}
	}

	return result;
}

