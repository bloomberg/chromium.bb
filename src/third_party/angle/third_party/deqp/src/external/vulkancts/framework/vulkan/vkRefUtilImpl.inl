/* WARNING: This is auto-generated file. Do not modify, since changes will
 * be lost! Modify the generating script instead.
 */
namespace refdetails
{

template<>
void Deleter<VkDeviceMemory>::operator() (VkDeviceMemory obj) const
{
	m_deviceIface->freeMemory(m_device, obj, m_allocator);
}

template<>
void Deleter<VkFence>::operator() (VkFence obj) const
{
	m_deviceIface->destroyFence(m_device, obj, m_allocator);
}

template<>
void Deleter<VkSemaphore>::operator() (VkSemaphore obj) const
{
	m_deviceIface->destroySemaphore(m_device, obj, m_allocator);
}

template<>
void Deleter<VkEvent>::operator() (VkEvent obj) const
{
	m_deviceIface->destroyEvent(m_device, obj, m_allocator);
}

template<>
void Deleter<VkQueryPool>::operator() (VkQueryPool obj) const
{
	m_deviceIface->destroyQueryPool(m_device, obj, m_allocator);
}

template<>
void Deleter<VkBuffer>::operator() (VkBuffer obj) const
{
	m_deviceIface->destroyBuffer(m_device, obj, m_allocator);
}

template<>
void Deleter<VkBufferView>::operator() (VkBufferView obj) const
{
	m_deviceIface->destroyBufferView(m_device, obj, m_allocator);
}

template<>
void Deleter<VkImage>::operator() (VkImage obj) const
{
	m_deviceIface->destroyImage(m_device, obj, m_allocator);
}

template<>
void Deleter<VkImageView>::operator() (VkImageView obj) const
{
	m_deviceIface->destroyImageView(m_device, obj, m_allocator);
}

template<>
void Deleter<VkShaderModule>::operator() (VkShaderModule obj) const
{
	m_deviceIface->destroyShaderModule(m_device, obj, m_allocator);
}

template<>
void Deleter<VkPipelineCache>::operator() (VkPipelineCache obj) const
{
	m_deviceIface->destroyPipelineCache(m_device, obj, m_allocator);
}

template<>
void Deleter<VkPipeline>::operator() (VkPipeline obj) const
{
	m_deviceIface->destroyPipeline(m_device, obj, m_allocator);
}

template<>
void Deleter<VkPipelineLayout>::operator() (VkPipelineLayout obj) const
{
	m_deviceIface->destroyPipelineLayout(m_device, obj, m_allocator);
}

template<>
void Deleter<VkSampler>::operator() (VkSampler obj) const
{
	m_deviceIface->destroySampler(m_device, obj, m_allocator);
}

template<>
void Deleter<VkDescriptorSetLayout>::operator() (VkDescriptorSetLayout obj) const
{
	m_deviceIface->destroyDescriptorSetLayout(m_device, obj, m_allocator);
}

template<>
void Deleter<VkDescriptorPool>::operator() (VkDescriptorPool obj) const
{
	m_deviceIface->destroyDescriptorPool(m_device, obj, m_allocator);
}

template<>
void Deleter<VkFramebuffer>::operator() (VkFramebuffer obj) const
{
	m_deviceIface->destroyFramebuffer(m_device, obj, m_allocator);
}

template<>
void Deleter<VkRenderPass>::operator() (VkRenderPass obj) const
{
	m_deviceIface->destroyRenderPass(m_device, obj, m_allocator);
}

template<>
void Deleter<VkCommandPool>::operator() (VkCommandPool obj) const
{
	m_deviceIface->destroyCommandPool(m_device, obj, m_allocator);
}

template<>
void Deleter<VkSamplerYcbcrConversion>::operator() (VkSamplerYcbcrConversion obj) const
{
	m_deviceIface->destroySamplerYcbcrConversion(m_device, obj, m_allocator);
}

template<>
void Deleter<VkDescriptorUpdateTemplate>::operator() (VkDescriptorUpdateTemplate obj) const
{
	m_deviceIface->destroyDescriptorUpdateTemplate(m_device, obj, m_allocator);
}

template<>
void Deleter<VkSwapchainKHR>::operator() (VkSwapchainKHR obj) const
{
	m_deviceIface->destroySwapchainKHR(m_device, obj, m_allocator);
}

template<>
void Deleter<VkIndirectCommandsLayoutNVX>::operator() (VkIndirectCommandsLayoutNVX obj) const
{
	m_deviceIface->destroyIndirectCommandsLayoutNVX(m_device, obj, m_allocator);
}

template<>
void Deleter<VkObjectTableNVX>::operator() (VkObjectTableNVX obj) const
{
	m_deviceIface->destroyObjectTableNVX(m_device, obj, m_allocator);
}

template<>
void Deleter<VkValidationCacheEXT>::operator() (VkValidationCacheEXT obj) const
{
	m_deviceIface->destroyValidationCacheEXT(m_device, obj, m_allocator);
}

} // refdetails

Move<VkInstance> createInstance (const PlatformInterface& vk, const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkInstance object = 0;
	VK_CHECK(vk.createInstance(pCreateInfo, pAllocator, &object));
	return Move<VkInstance>(check<VkInstance>(object), Deleter<VkInstance>(vk, object, pAllocator));
}

Move<VkDevice> createDevice (const PlatformInterface& vkp, VkInstance instance, const InstanceInterface& vk, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkDevice object = 0;
	VK_CHECK(vk.createDevice(physicalDevice, pCreateInfo, pAllocator, &object));
	return Move<VkDevice>(check<VkDevice>(object), Deleter<VkDevice>(vkp, instance, object, pAllocator));
}

Move<VkDeviceMemory> allocateMemory (const DeviceInterface& vk, VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkDeviceMemory object = 0;
	VK_CHECK(vk.allocateMemory(device, pAllocateInfo, pAllocator, &object));
	return Move<VkDeviceMemory>(check<VkDeviceMemory>(object), Deleter<VkDeviceMemory>(vk, device, pAllocator));
}

Move<VkFence> createFence (const DeviceInterface& vk, VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkFence object = 0;
	VK_CHECK(vk.createFence(device, pCreateInfo, pAllocator, &object));
	return Move<VkFence>(check<VkFence>(object), Deleter<VkFence>(vk, device, pAllocator));
}

Move<VkSemaphore> createSemaphore (const DeviceInterface& vk, VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSemaphore object = 0;
	VK_CHECK(vk.createSemaphore(device, pCreateInfo, pAllocator, &object));
	return Move<VkSemaphore>(check<VkSemaphore>(object), Deleter<VkSemaphore>(vk, device, pAllocator));
}

Move<VkEvent> createEvent (const DeviceInterface& vk, VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkEvent object = 0;
	VK_CHECK(vk.createEvent(device, pCreateInfo, pAllocator, &object));
	return Move<VkEvent>(check<VkEvent>(object), Deleter<VkEvent>(vk, device, pAllocator));
}

Move<VkQueryPool> createQueryPool (const DeviceInterface& vk, VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkQueryPool object = 0;
	VK_CHECK(vk.createQueryPool(device, pCreateInfo, pAllocator, &object));
	return Move<VkQueryPool>(check<VkQueryPool>(object), Deleter<VkQueryPool>(vk, device, pAllocator));
}

Move<VkBuffer> createBuffer (const DeviceInterface& vk, VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkBuffer object = 0;
	VK_CHECK(vk.createBuffer(device, pCreateInfo, pAllocator, &object));
	return Move<VkBuffer>(check<VkBuffer>(object), Deleter<VkBuffer>(vk, device, pAllocator));
}

Move<VkBufferView> createBufferView (const DeviceInterface& vk, VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkBufferView object = 0;
	VK_CHECK(vk.createBufferView(device, pCreateInfo, pAllocator, &object));
	return Move<VkBufferView>(check<VkBufferView>(object), Deleter<VkBufferView>(vk, device, pAllocator));
}

Move<VkImage> createImage (const DeviceInterface& vk, VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkImage object = 0;
	VK_CHECK(vk.createImage(device, pCreateInfo, pAllocator, &object));
	return Move<VkImage>(check<VkImage>(object), Deleter<VkImage>(vk, device, pAllocator));
}

Move<VkImageView> createImageView (const DeviceInterface& vk, VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkImageView object = 0;
	VK_CHECK(vk.createImageView(device, pCreateInfo, pAllocator, &object));
	return Move<VkImageView>(check<VkImageView>(object), Deleter<VkImageView>(vk, device, pAllocator));
}

Move<VkShaderModule> createShaderModule (const DeviceInterface& vk, VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkShaderModule object = 0;
	VK_CHECK(vk.createShaderModule(device, pCreateInfo, pAllocator, &object));
	return Move<VkShaderModule>(check<VkShaderModule>(object), Deleter<VkShaderModule>(vk, device, pAllocator));
}

Move<VkPipelineCache> createPipelineCache (const DeviceInterface& vk, VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkPipelineCache object = 0;
	VK_CHECK(vk.createPipelineCache(device, pCreateInfo, pAllocator, &object));
	return Move<VkPipelineCache>(check<VkPipelineCache>(object), Deleter<VkPipelineCache>(vk, device, pAllocator));
}

Move<VkPipelineLayout> createPipelineLayout (const DeviceInterface& vk, VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkPipelineLayout object = 0;
	VK_CHECK(vk.createPipelineLayout(device, pCreateInfo, pAllocator, &object));
	return Move<VkPipelineLayout>(check<VkPipelineLayout>(object), Deleter<VkPipelineLayout>(vk, device, pAllocator));
}

Move<VkSampler> createSampler (const DeviceInterface& vk, VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSampler object = 0;
	VK_CHECK(vk.createSampler(device, pCreateInfo, pAllocator, &object));
	return Move<VkSampler>(check<VkSampler>(object), Deleter<VkSampler>(vk, device, pAllocator));
}

Move<VkDescriptorSetLayout> createDescriptorSetLayout (const DeviceInterface& vk, VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkDescriptorSetLayout object = 0;
	VK_CHECK(vk.createDescriptorSetLayout(device, pCreateInfo, pAllocator, &object));
	return Move<VkDescriptorSetLayout>(check<VkDescriptorSetLayout>(object), Deleter<VkDescriptorSetLayout>(vk, device, pAllocator));
}

Move<VkDescriptorPool> createDescriptorPool (const DeviceInterface& vk, VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkDescriptorPool object = 0;
	VK_CHECK(vk.createDescriptorPool(device, pCreateInfo, pAllocator, &object));
	return Move<VkDescriptorPool>(check<VkDescriptorPool>(object), Deleter<VkDescriptorPool>(vk, device, pAllocator));
}

Move<VkFramebuffer> createFramebuffer (const DeviceInterface& vk, VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkFramebuffer object = 0;
	VK_CHECK(vk.createFramebuffer(device, pCreateInfo, pAllocator, &object));
	return Move<VkFramebuffer>(check<VkFramebuffer>(object), Deleter<VkFramebuffer>(vk, device, pAllocator));
}

Move<VkRenderPass> createRenderPass (const DeviceInterface& vk, VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkRenderPass object = 0;
	VK_CHECK(vk.createRenderPass(device, pCreateInfo, pAllocator, &object));
	return Move<VkRenderPass>(check<VkRenderPass>(object), Deleter<VkRenderPass>(vk, device, pAllocator));
}

Move<VkCommandPool> createCommandPool (const DeviceInterface& vk, VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkCommandPool object = 0;
	VK_CHECK(vk.createCommandPool(device, pCreateInfo, pAllocator, &object));
	return Move<VkCommandPool>(check<VkCommandPool>(object), Deleter<VkCommandPool>(vk, device, pAllocator));
}

Move<VkSamplerYcbcrConversion> createSamplerYcbcrConversion (const DeviceInterface& vk, VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSamplerYcbcrConversion object = 0;
	VK_CHECK(vk.createSamplerYcbcrConversion(device, pCreateInfo, pAllocator, &object));
	return Move<VkSamplerYcbcrConversion>(check<VkSamplerYcbcrConversion>(object), Deleter<VkSamplerYcbcrConversion>(vk, device, pAllocator));
}

Move<VkDescriptorUpdateTemplate> createDescriptorUpdateTemplate (const DeviceInterface& vk, VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkDescriptorUpdateTemplate object = 0;
	VK_CHECK(vk.createDescriptorUpdateTemplate(device, pCreateInfo, pAllocator, &object));
	return Move<VkDescriptorUpdateTemplate>(check<VkDescriptorUpdateTemplate>(object), Deleter<VkDescriptorUpdateTemplate>(vk, device, pAllocator));
}

Move<VkSwapchainKHR> createSwapchainKHR (const DeviceInterface& vk, VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSwapchainKHR object = 0;
	VK_CHECK(vk.createSwapchainKHR(device, pCreateInfo, pAllocator, &object));
	return Move<VkSwapchainKHR>(check<VkSwapchainKHR>(object), Deleter<VkSwapchainKHR>(vk, device, pAllocator));
}

Move<VkSurfaceKHR> createDisplayPlaneSurfaceKHR (const InstanceInterface& vk, VkInstance instance, const VkDisplaySurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(vk.createDisplayPlaneSurfaceKHR(instance, pCreateInfo, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vk, instance, pAllocator));
}

Move<VkSwapchainKHR> createSharedSwapchainsKHR (const DeviceInterface& vk, VkDevice device, deUint32 swapchainCount, const VkSwapchainCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator)
{
	VkSwapchainKHR object = 0;
	VK_CHECK(vk.createSharedSwapchainsKHR(device, swapchainCount, pCreateInfos, pAllocator, &object));
	return Move<VkSwapchainKHR>(check<VkSwapchainKHR>(object), Deleter<VkSwapchainKHR>(vk, device, pAllocator));
}

Move<VkSurfaceKHR> createXlibSurfaceKHR (const InstanceInterface& vk, VkInstance instance, const VkXlibSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(vk.createXlibSurfaceKHR(instance, pCreateInfo, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vk, instance, pAllocator));
}

Move<VkSurfaceKHR> createXcbSurfaceKHR (const InstanceInterface& vk, VkInstance instance, const VkXcbSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(vk.createXcbSurfaceKHR(instance, pCreateInfo, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vk, instance, pAllocator));
}

Move<VkSurfaceKHR> createWaylandSurfaceKHR (const InstanceInterface& vk, VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(vk.createWaylandSurfaceKHR(instance, pCreateInfo, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vk, instance, pAllocator));
}

Move<VkSurfaceKHR> createMirSurfaceKHR (const InstanceInterface& vk, VkInstance instance, const VkMirSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(vk.createMirSurfaceKHR(instance, pCreateInfo, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vk, instance, pAllocator));
}

Move<VkSurfaceKHR> createAndroidSurfaceKHR (const InstanceInterface& vk, VkInstance instance, const VkAndroidSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(vk.createAndroidSurfaceKHR(instance, pCreateInfo, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vk, instance, pAllocator));
}

Move<VkSurfaceKHR> createWin32SurfaceKHR (const InstanceInterface& vk, VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(vk.createWin32SurfaceKHR(instance, pCreateInfo, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vk, instance, pAllocator));
}

Move<VkRenderPass> createRenderPass2KHR (const DeviceInterface& vk, VkDevice device, const VkRenderPassCreateInfo2KHR* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkRenderPass object = 0;
	VK_CHECK(vk.createRenderPass2KHR(device, pCreateInfo, pAllocator, &object));
	return Move<VkRenderPass>(check<VkRenderPass>(object), Deleter<VkRenderPass>(vk, device, pAllocator));
}

Move<VkDebugReportCallbackEXT> createDebugReportCallbackEXT (const InstanceInterface& vk, VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkDebugReportCallbackEXT object = 0;
	VK_CHECK(vk.createDebugReportCallbackEXT(instance, pCreateInfo, pAllocator, &object));
	return Move<VkDebugReportCallbackEXT>(check<VkDebugReportCallbackEXT>(object), Deleter<VkDebugReportCallbackEXT>(vk, instance, pAllocator));
}

Move<VkSurfaceKHR> createViSurfaceNN (const InstanceInterface& vk, VkInstance instance, const VkViSurfaceCreateInfoNN* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(vk.createViSurfaceNN(instance, pCreateInfo, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vk, instance, pAllocator));
}

Move<VkIndirectCommandsLayoutNVX> createIndirectCommandsLayoutNVX (const DeviceInterface& vk, VkDevice device, const VkIndirectCommandsLayoutCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkIndirectCommandsLayoutNVX object = 0;
	VK_CHECK(vk.createIndirectCommandsLayoutNVX(device, pCreateInfo, pAllocator, &object));
	return Move<VkIndirectCommandsLayoutNVX>(check<VkIndirectCommandsLayoutNVX>(object), Deleter<VkIndirectCommandsLayoutNVX>(vk, device, pAllocator));
}

Move<VkObjectTableNVX> createObjectTableNVX (const DeviceInterface& vk, VkDevice device, const VkObjectTableCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkObjectTableNVX object = 0;
	VK_CHECK(vk.createObjectTableNVX(device, pCreateInfo, pAllocator, &object));
	return Move<VkObjectTableNVX>(check<VkObjectTableNVX>(object), Deleter<VkObjectTableNVX>(vk, device, pAllocator));
}

Move<VkSurfaceKHR> createIOSSurfaceMVK (const InstanceInterface& vk, VkInstance instance, const VkIOSSurfaceCreateInfoMVK* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(vk.createIOSSurfaceMVK(instance, pCreateInfo, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vk, instance, pAllocator));
}

Move<VkSurfaceKHR> createMacOSSurfaceMVK (const InstanceInterface& vk, VkInstance instance, const VkMacOSSurfaceCreateInfoMVK* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkSurfaceKHR object = 0;
	VK_CHECK(vk.createMacOSSurfaceMVK(instance, pCreateInfo, pAllocator, &object));
	return Move<VkSurfaceKHR>(check<VkSurfaceKHR>(object), Deleter<VkSurfaceKHR>(vk, instance, pAllocator));
}

Move<VkValidationCacheEXT> createValidationCacheEXT (const DeviceInterface& vk, VkDevice device, const VkValidationCacheCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator)
{
	VkValidationCacheEXT object = 0;
	VK_CHECK(vk.createValidationCacheEXT(device, pCreateInfo, pAllocator, &object));
	return Move<VkValidationCacheEXT>(check<VkValidationCacheEXT>(object), Deleter<VkValidationCacheEXT>(vk, device, pAllocator));
}

