/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan test case base classes
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkDebugReportUtil.hpp"
#include "vkDeviceFeatures.hpp"
#include "vkDeviceProperties.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "deSTLUtil.hpp"
#include "deMemory.h"

#include <set>

namespace vkt
{

// Default device utilities

using std::vector;
using std::string;
using std::set;
using namespace vk;

namespace
{

vector<string> filterExtensions (const vector<VkExtensionProperties>& extensions)
{
	vector<string>	enabledExtensions;
	bool			khrBufferDeviceAddress	= false;

	const char*		extensionGroups[]		=
	{
		"VK_KHR_",
		"VK_EXT_",
		"VK_KHX_",
		"VK_NV_cooperative_matrix",
		"VK_NV_shading_rate_image",
		"VK_NV_ray_tracing",
		"VK_AMD_mixed_attachment_samples",
		"VK_AMD_shader_fragment_mask",
		"VK_AMD_buffer_marker",
		"VK_AMD_shader_explicit_vertex_parameter",
		"VK_AMD_shader_image_load_store_lod",
		"VK_AMD_shader_trinary_minmax",
	};

	for (size_t extNdx = 0; extNdx < extensions.size(); extNdx++)
	{
		if (deStringEqual(extensions[extNdx].extensionName, "VK_KHR_buffer_device_address"))
		{
			khrBufferDeviceAddress = true;
			break;
		}
	}

	for (size_t extNdx = 0; extNdx < extensions.size(); extNdx++)
	{
		// VK_EXT_buffer_device_address is deprecated and must not be enabled if VK_KHR_buffer_device_address is enabled
		if (khrBufferDeviceAddress && deStringEqual(extensions[extNdx].extensionName, "VK_EXT_buffer_device_address"))
			continue;

		for (int extGroupNdx = 0; extGroupNdx < DE_LENGTH_OF_ARRAY(extensionGroups); extGroupNdx++)
		{
			if (deStringBeginsWith(extensions[extNdx].extensionName, extensionGroups[extGroupNdx]))
				enabledExtensions.push_back(extensions[extNdx].extensionName);
		}
	}

	return enabledExtensions;
}

vector<string> addExtensions (const vector<string>& a, const vector<const char*>& b)
{
	vector<string>	res		(a);

	for (vector<const char*>::const_iterator bIter = b.begin(); bIter != b.end(); ++bIter)
	{
		if (!de::contains(res.begin(), res.end(), string(*bIter)))
			res.push_back(string(*bIter));
	}

	return res;
}

vector<string> removeExtensions (const vector<string>& a, const vector<const char*>& b)
{
	vector<string>	res;
	set<string>		removeExts	(b.begin(), b.end());

	for (vector<string>::const_iterator aIter = a.begin(); aIter != a.end(); ++aIter)
	{
		if (!de::contains(removeExts, *aIter))
			res.push_back(*aIter);
	}

	return res;
}

vector<string> addCoreInstanceExtensions (const vector<string>& extensions, deUint32 instanceVersion)
{
	vector<const char*> coreExtensions;
	getCoreInstanceExtensions(instanceVersion, coreExtensions);
	return addExtensions(extensions, coreExtensions);
}

vector<string> addCoreDeviceExtensions(const vector<string>& extensions, deUint32 instanceVersion)
{
	vector<const char*> coreExtensions;
	getCoreDeviceExtensions(instanceVersion, coreExtensions);
	return addExtensions(extensions, coreExtensions);
}

deUint32 getTargetInstanceVersion (const PlatformInterface& vkp)
{
	deUint32 version = pack(ApiVersion(1, 0, 0));

	if (vkp.enumerateInstanceVersion(&version) != VK_SUCCESS)
		TCU_THROW(InternalError, "Enumerate instance version error");
	return version;
}

std::pair<deUint32, deUint32> determineDeviceVersions(const PlatformInterface& vkp, deUint32 apiVersion, const tcu::CommandLine& cmdLine)
{
	Move<VkInstance>						preinstance				= createDefaultInstance(vkp, apiVersion);
	InstanceDriver							preinterface			(vkp, preinstance.get());

	const vector<VkPhysicalDevice>			devices					= enumeratePhysicalDevices(preinterface, preinstance.get());
	deUint32								lowestDeviceVersion		= 0xFFFFFFFFu;
	for (deUint32 deviceNdx = 0u; deviceNdx < devices.size(); ++deviceNdx)
	{
		const VkPhysicalDeviceProperties	props					= getPhysicalDeviceProperties(preinterface, devices[deviceNdx]);
		if (props.apiVersion < lowestDeviceVersion)
			lowestDeviceVersion = props.apiVersion;
	}

	const vk::VkPhysicalDevice				choosenDevice			= chooseDevice(preinterface, *preinstance, cmdLine);
	const VkPhysicalDeviceProperties		props					= getPhysicalDeviceProperties(preinterface, choosenDevice);
	const deUint32							choosenDeviceVersion	= props.apiVersion;

	return std::make_pair(choosenDeviceVersion, lowestDeviceVersion);
}


Move<VkInstance> createInstance (const PlatformInterface& vkp, deUint32 apiVersion, const vector<string>& enabledExtensions, const tcu::CommandLine& cmdLine)
{
	const bool								isValidationEnabled	= cmdLine.isValidationEnabled();
	vector<const char*>						enabledLayers;

	// \note Extensions in core are not explicitly enabled even though
	//		 they are in the extension list advertised to tests.
	vector<const char*>						coreExtensions;
	getCoreInstanceExtensions(apiVersion, coreExtensions);
	vector<string>							nonCoreExtensions	(removeExtensions(enabledExtensions, coreExtensions));

	if (isValidationEnabled)
	{
		if (!isDebugReportSupported(vkp))
			TCU_THROW(NotSupportedError, "VK_EXT_debug_report is not supported");

		enabledLayers = vkt::getValidationLayers(vkp);
		if (enabledLayers.empty())
			TCU_THROW(NotSupportedError, "No validation layers found");
	}

	return createDefaultInstance(vkp, apiVersion, vector<string>(begin(enabledLayers), end(enabledLayers)), nonCoreExtensions);
}

static deUint32 findQueueFamilyIndexWithCaps (const InstanceInterface& vkInstance, VkPhysicalDevice physicalDevice, VkQueueFlags requiredCaps)
{
	const vector<VkQueueFamilyProperties>	queueProps	= getPhysicalDeviceQueueFamilyProperties(vkInstance, physicalDevice);

	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if ((queueProps[queueNdx].queueFlags & requiredCaps) == requiredCaps)
			return (deUint32)queueNdx;
	}

	TCU_THROW(NotSupportedError, "No matching queue found");
}

Move<VkDevice> createDefaultDevice (const PlatformInterface&			vkp,
									VkInstance							instance,
									const InstanceInterface&			vki,
									VkPhysicalDevice					physicalDevice,
									const deUint32						apiVersion,
									deUint32							queueIndex,
									deUint32							sparseQueueIndex,
									const VkPhysicalDeviceFeatures2&	enabledFeatures,
									const vector<string>&				enabledExtensions,
									const tcu::CommandLine&				cmdLine)
{
	VkDeviceQueueCreateInfo		queueInfo[2];
	VkDeviceCreateInfo			deviceInfo;
	vector<const char*>			enabledLayers;
	vector<const char*>			extensionPtrs;
	const float					queuePriority	= 1.0f;
	const deUint32				numQueues = (enabledFeatures.features.sparseBinding && (queueIndex != sparseQueueIndex)) ? 2 : 1;

	deMemset(&queueInfo,	0, sizeof(queueInfo));
	deMemset(&deviceInfo,	0, sizeof(deviceInfo));

	if (cmdLine.isValidationEnabled())
	{
		enabledLayers = vkt::getValidationLayers(vki, physicalDevice);
		if (enabledLayers.empty())
			TCU_THROW(NotSupportedError, "No validation layers found");
	}

	// \note Extensions in core are not explicitly enabled even though
	//		 they are in the extension list advertised to tests.
	vector<const char*> coreExtensions;
	getCoreDeviceExtensions(apiVersion, coreExtensions);
	vector<string>	nonCoreExtensions(removeExtensions(enabledExtensions, coreExtensions));

	extensionPtrs.resize(nonCoreExtensions.size());

	for (size_t ndx = 0; ndx < nonCoreExtensions.size(); ++ndx)
		extensionPtrs[ndx] = nonCoreExtensions[ndx].c_str();

	queueInfo[0].sType						= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo[0].pNext						= DE_NULL;
	queueInfo[0].flags						= (VkDeviceQueueCreateFlags)0u;
	queueInfo[0].queueFamilyIndex			= queueIndex;
	queueInfo[0].queueCount					= 1u;
	queueInfo[0].pQueuePriorities			= &queuePriority;

	if (numQueues > 1)
	{
		queueInfo[1].sType						= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo[1].pNext						= DE_NULL;
		queueInfo[1].flags						= (VkDeviceQueueCreateFlags)0u;
		queueInfo[1].queueFamilyIndex			= sparseQueueIndex;
		queueInfo[1].queueCount					= 1u;
		queueInfo[1].pQueuePriorities			= &queuePriority;
	}

	// VK_KHR_get_physical_device_properties2 is used if enabledFeatures.pNext != 0
	deviceInfo.sType						= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext						= enabledFeatures.pNext ? &enabledFeatures : DE_NULL;
	deviceInfo.queueCreateInfoCount			= numQueues;
	deviceInfo.pQueueCreateInfos			= queueInfo;
	deviceInfo.enabledExtensionCount		= (deUint32)extensionPtrs.size();
	deviceInfo.ppEnabledExtensionNames		= (extensionPtrs.empty() ? DE_NULL : &extensionPtrs[0]);
	deviceInfo.enabledLayerCount			= (deUint32)enabledLayers.size();
	deviceInfo.ppEnabledLayerNames			= (enabledLayers.empty() ? DE_NULL : enabledLayers.data());
	deviceInfo.pEnabledFeatures				= enabledFeatures.pNext ? DE_NULL : &enabledFeatures.features;

	return createDevice(vkp, instance, vki, physicalDevice, &deviceInfo);
};

} // anonymous

class DefaultDevice
{
public:
																	DefaultDevice							(const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine);
																	~DefaultDevice							(void);

	VkInstance														getInstance								(void) const { return *m_instance;										}
	const InstanceInterface&										getInstanceInterface					(void) const { return m_instanceInterface;								}
	deUint32														getMaximumFrameworkVulkanVersion		(void) const { return m_maximumFrameworkVulkanVersion;					}
	deUint32														getAvailableInstanceVersion				(void) const { return m_availableInstanceVersion;						}
	deUint32														getUsedInstanceVersion					(void) const { return m_usedInstanceVersion;							}
	const vector<string>&											getInstanceExtensions					(void) const { return m_instanceExtensions;								}

	VkPhysicalDevice												getPhysicalDevice						(void) const { return m_physicalDevice;									}
	deUint32														getDeviceVersion						(void) const { return m_deviceVersion;									}

	bool															isDeviceFeatureInitialized				(VkStructureType sType) const { return m_deviceFeatures.isDeviceFeatureInitialized(sType);		}
	const VkPhysicalDeviceFeatures&									getDeviceFeatures						(void) const { return m_deviceFeatures.getCoreFeatures2().features;		}
	const VkPhysicalDeviceFeatures2&								getDeviceFeatures2						(void) const { return m_deviceFeatures.getCoreFeatures2();				}
	const VkPhysicalDeviceVulkan11Features&							getVulkan11Features						(void) const { return m_deviceFeatures.getVulkan11Features(); }
	const VkPhysicalDeviceVulkan12Features&							getVulkan12Features						(void) const { return m_deviceFeatures.getVulkan12Features(); }

#include "vkDeviceFeaturesForDefaultDeviceDefs.inl"

	bool															isDevicePropertyInitialized				(VkStructureType sType) const { return m_devicePropertiesFull.isDevicePropertyInitialized(sType);	}
	const VkPhysicalDeviceProperties&								getDeviceProperties						(void) const { return m_deviceProperties;									}
	const VkPhysicalDeviceProperties2&								getDeviceProperties2					(void) const { return m_devicePropertiesFull.getCoreProperties2();			}

#include "vkDevicePropertiesForDefaultDeviceDefs.inl"

	VkDevice														getDevice								(void) const { return *m_device;											}
	const DeviceInterface&											getDeviceInterface						(void) const { return m_deviceInterface;									}
	const vector<string>&											getDeviceExtensions						(void) const { return m_deviceExtensions;									}
	deUint32														getUsedApiVersion						(void) const { return m_usedApiVersion;										}
	deUint32														getUniversalQueueFamilyIndex			(void) const { return m_universalQueueFamilyIndex;							}
	VkQueue															getUniversalQueue						(void) const;
	deUint32														getSparseQueueFamilyIndex				(void) const { return m_sparseQueueFamilyIndex;								}
	VkQueue															getSparseQueue							(void) const;

private:

	const deUint32						m_maximumFrameworkVulkanVersion;
	const deUint32						m_availableInstanceVersion;
	const deUint32						m_usedInstanceVersion;

	const std::pair<deUint32, deUint32> m_deviceVersions;
	const deUint32						m_usedApiVersion;

	const vector<string>				m_instanceExtensions;
	const Unique<VkInstance>			m_instance;
	const InstanceDriver				m_instanceInterface;

	const VkPhysicalDevice				m_physicalDevice;
	const deUint32						m_deviceVersion;

	const vector<string>				m_deviceExtensions;
	const DeviceFeatures				m_deviceFeatures;

	const deUint32						m_universalQueueFamilyIndex;
	const deUint32						m_sparseQueueFamilyIndex;
	const VkPhysicalDeviceProperties	m_deviceProperties;
	const DeviceProperties				m_devicePropertiesFull;

	const Unique<VkDevice>				m_device;
	const DeviceDriver					m_deviceInterface;
};

static deUint32 sanitizeApiVersion(deUint32 v)
{
	return VK_MAKE_VERSION( VK_VERSION_MAJOR(v), VK_VERSION_MINOR(v), 0 );
}

DefaultDevice::DefaultDevice (const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine)
	: m_maximumFrameworkVulkanVersion	(VK_API_MAX_FRAMEWORK_VERSION)
	, m_availableInstanceVersion		(getTargetInstanceVersion(vkPlatform))
	, m_usedInstanceVersion				(sanitizeApiVersion(deMinu32(m_availableInstanceVersion, m_maximumFrameworkVulkanVersion)))
	, m_deviceVersions					(determineDeviceVersions(vkPlatform, m_usedInstanceVersion, cmdLine))
	, m_usedApiVersion					(sanitizeApiVersion(deMinu32(m_usedInstanceVersion, m_deviceVersions.first)))

	, m_instanceExtensions				(addCoreInstanceExtensions(filterExtensions(enumerateInstanceExtensionProperties(vkPlatform, DE_NULL)), m_usedApiVersion))
	, m_instance						(createInstance(vkPlatform, m_usedApiVersion, m_instanceExtensions, cmdLine))

	, m_instanceInterface				(vkPlatform, *m_instance)
	, m_physicalDevice					(chooseDevice(m_instanceInterface, *m_instance, cmdLine))
	, m_deviceVersion					(getPhysicalDeviceProperties(m_instanceInterface, m_physicalDevice).apiVersion)

	, m_deviceExtensions				(addCoreDeviceExtensions(filterExtensions(enumerateDeviceExtensionProperties(m_instanceInterface, m_physicalDevice, DE_NULL)), m_usedApiVersion))
	, m_deviceFeatures					(m_instanceInterface, m_usedApiVersion, m_physicalDevice, m_instanceExtensions, m_deviceExtensions)
	, m_universalQueueFamilyIndex		(findQueueFamilyIndexWithCaps(m_instanceInterface, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))
	, m_sparseQueueFamilyIndex			(m_deviceFeatures.getCoreFeatures2().features.sparseBinding ? findQueueFamilyIndexWithCaps(m_instanceInterface, m_physicalDevice, VK_QUEUE_SPARSE_BINDING_BIT) : 0)
	, m_deviceProperties				(getPhysicalDeviceProperties(m_instanceInterface, m_physicalDevice))
	, m_devicePropertiesFull			(m_instanceInterface, m_usedApiVersion, m_physicalDevice, m_instanceExtensions, m_deviceExtensions)
	, m_device							(createDefaultDevice(vkPlatform, *m_instance, m_instanceInterface, m_physicalDevice, m_usedApiVersion, m_universalQueueFamilyIndex, m_sparseQueueFamilyIndex, m_deviceFeatures.getCoreFeatures2(), m_deviceExtensions, cmdLine))
	, m_deviceInterface					(vkPlatform, *m_instance, *m_device)
{
	DE_ASSERT(m_deviceVersions.first == m_deviceVersion);
}

DefaultDevice::~DefaultDevice (void)
{
}

VkQueue DefaultDevice::getUniversalQueue (void) const
{
	return getDeviceQueue(m_deviceInterface, *m_device, m_universalQueueFamilyIndex, 0);
}

VkQueue DefaultDevice::getSparseQueue (void) const
{
	if (!m_deviceFeatures.getCoreFeatures2().features.sparseBinding)
		TCU_THROW(NotSupportedError, "Sparse binding not supported.");

	return getDeviceQueue(m_deviceInterface, *m_device, m_sparseQueueFamilyIndex, 0);
}

namespace
{
// Allocator utilities

vk::Allocator* createAllocator (DefaultDevice* device)
{
	const VkPhysicalDeviceMemoryProperties memoryProperties = vk::getPhysicalDeviceMemoryProperties(device->getInstanceInterface(), device->getPhysicalDevice());

	// \todo [2015-07-24 jarkko] support allocator selection/configuration from command line (or compile time)
	return new SimpleAllocator(device->getDeviceInterface(), device->getDevice(), memoryProperties);
}

} // anonymous

// Context

Context::Context (tcu::TestContext&				testCtx,
				  const vk::PlatformInterface&	platformInterface,
				  vk::BinaryCollection&			progCollection)
	: m_testCtx					(testCtx)
	, m_platformInterface		(platformInterface)
	, m_progCollection			(progCollection)
	, m_device					(new DefaultDevice(m_platformInterface, testCtx.getCommandLine()))
	, m_allocator				(createAllocator(m_device.get()))
	, m_resultSetOnValidation	(false)
{
}

Context::~Context (void)
{
}

deUint32								Context::getMaximumFrameworkVulkanVersion	(void) const { return m_device->getMaximumFrameworkVulkanVersion();		}
deUint32								Context::getAvailableInstanceVersion		(void) const { return m_device->getAvailableInstanceVersion();	}
const vector<string>&					Context::getInstanceExtensions				(void) const { return m_device->getInstanceExtensions();		}
vk::VkInstance							Context::getInstance						(void) const { return m_device->getInstance();					}
const vk::InstanceInterface&			Context::getInstanceInterface				(void) const { return m_device->getInstanceInterface();			}
vk::VkPhysicalDevice					Context::getPhysicalDevice					(void) const { return m_device->getPhysicalDevice();			}
deUint32								Context::getDeviceVersion					(void) const { return m_device->getDeviceVersion();				}
const vk::VkPhysicalDeviceFeatures&		Context::getDeviceFeatures					(void) const { return m_device->getDeviceFeatures();			}
const vk::VkPhysicalDeviceFeatures2&	Context::getDeviceFeatures2					(void) const { return m_device->getDeviceFeatures2();			}

bool Context::isDeviceFunctionalitySupported (const std::string& extension) const
{
	// check if extension was promoted to core
	deUint32 apiVersion = getUsedApiVersion();
	if (isCoreDeviceExtension(apiVersion, extension))
	{
		// all folowing checks are for vk12 and can be skipped for previous versions
		if (apiVersion < VK_MAKE_VERSION(1, 2, 0))
			return true;

		// handle promoted functionality that was provided under feature bit
		const auto& vk11Features = m_device->getVulkan11Features();
		if (extension == "VK_KHR_multiview")
			return !!vk11Features.multiview;
		if (extension == "VK_KHR_variable_pointers")
			return !!vk11Features.variablePointersStorageBuffer;
		if (extension == "VK_KHR_sampler_ycbcr_conversion")
			return !!vk11Features.samplerYcbcrConversion;
		if (extension == "VK_KHR_shader_draw_parameters")
			return !!vk11Features.shaderDrawParameters;

		const auto& vk12Features = m_device->getVulkan12Features();
		if (extension == "VK_KHR_timeline_semaphore")
			return !!vk12Features.timelineSemaphore;
		if (extension == "VK_KHR_buffer_device_address")
			return !!vk12Features.bufferDeviceAddress;
		if (extension == "VK_EXT_descriptor_indexing")
			return !!vk12Features.descriptorIndexing;
		if (extension == "VK_KHR_draw_indirect_count")
			return !!vk12Features.drawIndirectCount;
		if (extension == "VK_KHR_sampler_mirror_clamp_to_edge")
			return !!vk12Features.samplerMirrorClampToEdge;
		if (extension == "VK_EXT_sampler_filter_minmax")
			return !!vk12Features.samplerFilterMinmax;
		if (extension == "VK_EXT_shader_viewport_index_layer")
			return !!vk12Features.shaderOutputViewportIndex && !!vk12Features.shaderOutputLayer;

		// no feature flags to check
		return true;
	}

	// check if extension is on the lits of extensions for current device
	const auto& extensions = getDeviceExtensions();
	if (de::contains(extensions.begin(), extensions.end(), extension))
	{
		if (extension == "VK_KHR_timeline_semaphore")
			return !!getTimelineSemaphoreFeatures().timelineSemaphore;

		return true;
	}

	return false;
}

bool Context::isInstanceFunctionalitySupported(const std::string& extension) const
{
	// NOTE: current implementation uses isInstanceExtensionSupported but
	// this will change when some instance extensions will be promoted to the
	// core; don't use isInstanceExtensionSupported directly, use this method instead
	return isInstanceExtensionSupported(getUsedApiVersion(), getInstanceExtensions(), extension);
}

#include "vkDeviceFeaturesForContextDefs.inl"

const vk::VkPhysicalDeviceProperties&	Context::getDeviceProperties				(void) const { return m_device->getDeviceProperties();			}
const vk::VkPhysicalDeviceProperties2&	Context::getDeviceProperties2				(void) const { return m_device->getDeviceProperties2();			}

#include "vkDevicePropertiesForContextDefs.inl"

const vector<string>&					Context::getDeviceExtensions				(void) const { return m_device->getDeviceExtensions();			}
vk::VkDevice							Context::getDevice							(void) const { return m_device->getDevice();					}
const vk::DeviceInterface&				Context::getDeviceInterface					(void) const { return m_device->getDeviceInterface();			}
deUint32								Context::getUniversalQueueFamilyIndex		(void) const { return m_device->getUniversalQueueFamilyIndex();	}
vk::VkQueue								Context::getUniversalQueue					(void) const { return m_device->getUniversalQueue();			}
deUint32								Context::getSparseQueueFamilyIndex			(void) const { return m_device->getSparseQueueFamilyIndex();	}
vk::VkQueue								Context::getSparseQueue						(void) const { return m_device->getSparseQueue();				}
vk::Allocator&							Context::getDefaultAllocator				(void) const { return *m_allocator;								}
deUint32								Context::getUsedApiVersion					(void) const { return m_device->getUsedApiVersion();			}
bool									Context::contextSupports					(const deUint32 majorNum, const deUint32 minorNum, const deUint32 patchNum) const
																							{ return m_device->getUsedApiVersion() >= VK_MAKE_VERSION(majorNum, minorNum, patchNum); }
bool									Context::contextSupports					(const ApiVersion version) const
																							{ return m_device->getUsedApiVersion() >= pack(version); }
bool									Context::contextSupports					(const deUint32 requiredApiVersionBits) const
																							{ return m_device->getUsedApiVersion() >= requiredApiVersionBits; }
bool									Context::isDeviceFeatureInitialized			(vk::VkStructureType sType) const
																							{ return m_device->isDeviceFeatureInitialized(sType);	}
bool									Context::isDevicePropertyInitialized		(vk::VkStructureType sType) const
																							{ return m_device->isDevicePropertyInitialized(sType);	}

bool Context::requireDeviceFunctionality (const std::string& required) const
{
	if (!isDeviceFunctionalitySupported(required))
		TCU_THROW(NotSupportedError, required + " is not supported");

	return true;
}

bool Context::requireInstanceFunctionality (const std::string& required) const
{
	if (!isInstanceFunctionalitySupported(required))
		TCU_THROW(NotSupportedError, required + " is not supported");

	return true;
}

struct DeviceCoreFeaturesTable
{
	const char*		featureName;
	const deUint32	featureArrayIndex;
	const deUint32	featureArrayOffset;
};

#define DEVICE_CORE_FEATURE_OFFSET(FEATURE_FIELD_NAME)	DE_OFFSET_OF(VkPhysicalDeviceFeatures, FEATURE_FIELD_NAME)
#define DEVICE_CORE_FEATURE_ENTRY(BITNAME, FIELDNAME)	{ #FIELDNAME, BITNAME, DEVICE_CORE_FEATURE_OFFSET(FIELDNAME) }

const DeviceCoreFeaturesTable	deviceCoreFeaturesTable[] =
{
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_ROBUST_BUFFER_ACCESS							,	robustBufferAccess						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_FULL_DRAW_INDEX_UINT32						,	fullDrawIndexUint32						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY								,	imageCubeArray							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_INDEPENDENT_BLEND								,	independentBlend						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_GEOMETRY_SHADER								,	geometryShader							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TESSELLATION_SHADER							,	tessellationShader						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING							,	sampleRateShading						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DUAL_SRC_BLEND								,	dualSrcBlend							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_LOGIC_OP										,	logicOp									),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_MULTI_DRAW_INDIRECT							,	multiDrawIndirect						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DRAW_INDIRECT_FIRST_INSTANCE					,	drawIndirectFirstInstance				),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DEPTH_CLAMP									,	depthClamp								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DEPTH_BIAS_CLAMP								,	depthBiasClamp							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_FILL_MODE_NON_SOLID							,	fillModeNonSolid						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DEPTH_BOUNDS									,	depthBounds								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_WIDE_LINES									,	wideLines								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_LARGE_POINTS									,	largePoints								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_ALPHA_TO_ONE									,	alphaToOne								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_MULTI_VIEWPORT								,	multiViewport							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SAMPLER_ANISOTROPY							,	samplerAnisotropy						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_ETC2						,	textureCompressionETC2					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_ASTC_LDR					,	textureCompressionASTC_LDR				),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_BC						,	textureCompressionBC					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_OCCLUSION_QUERY_PRECISE						,	occlusionQueryPrecise					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_PIPELINE_STATISTICS_QUERY						,	pipelineStatisticsQuery					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS			,	vertexPipelineStoresAndAtomics			),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS					,	fragmentStoresAndAtomics				),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE	,	shaderTessellationAndGeometryPointSize	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_IMAGE_GATHER_EXTENDED					,	shaderImageGatherExtended				),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_EXTENDED_FORMATS			,	shaderStorageImageExtendedFormats		),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_MULTISAMPLE				,	shaderStorageImageMultisample			),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_READ_WITHOUT_FORMAT		,	shaderStorageImageReadWithoutFormat		),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT		,	shaderStorageImageWriteWithoutFormat	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_UNIFORM_BUFFER_ARRAY_DYNAMIC_INDEXING	,	shaderUniformBufferArrayDynamicIndexing	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_SAMPLED_IMAGE_ARRAY_DYNAMIC_INDEXING	,	shaderSampledImageArrayDynamicIndexing	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_BUFFER_ARRAY_DYNAMIC_INDEXING	,	shaderStorageBufferArrayDynamicIndexing	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_ARRAY_DYNAMIC_INDEXING	,	shaderStorageImageArrayDynamicIndexing	),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_CLIP_DISTANCE							,	shaderClipDistance						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_CULL_DISTANCE							,	shaderCullDistance						),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_FLOAT64								,	shaderFloat64							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_INT64									,	shaderInt64								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_INT16									,	shaderInt16								),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_RESOURCE_RESIDENCY						,	shaderResourceResidency					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_RESOURCE_MIN_LOD						,	shaderResourceMinLod					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_BINDING								,	sparseBinding							),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_BUFFER						,	sparseResidencyBuffer					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE2D						,	sparseResidencyImage2D					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE3D						,	sparseResidencyImage3D					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY2_SAMPLES						,	sparseResidency2Samples					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY4_SAMPLES						,	sparseResidency4Samples					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY8_SAMPLES						,	sparseResidency8Samples					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY16_SAMPLES					,	sparseResidency16Samples				),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_ALIASED						,	sparseResidencyAliased					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_VARIABLE_MULTISAMPLE_RATE						,	variableMultisampleRate					),
	DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_INHERITED_QUERIES								,	inheritedQueries						),
};

bool Context::requireDeviceCoreFeature (const DeviceCoreFeature requiredFeature)
{
	const vk::VkPhysicalDeviceFeatures& featuresAvailable		= getDeviceFeatures();
	const vk::VkBool32*					featuresAvailableArray	= (vk::VkBool32*)(&featuresAvailable);
	const deUint32						requiredFeatureIndex	= static_cast<deUint32>(requiredFeature);

	DE_ASSERT(requiredFeatureIndex * sizeof(vk::VkBool32) < sizeof(featuresAvailable));
	DE_ASSERT(deviceCoreFeaturesTable[requiredFeatureIndex].featureArrayIndex * sizeof(vk::VkBool32) == deviceCoreFeaturesTable[requiredFeatureIndex].featureArrayOffset);

	if (featuresAvailableArray[requiredFeatureIndex] == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requested core feature is not supported: " + std::string(deviceCoreFeaturesTable[requiredFeatureIndex].featureName));

	return true;
}

void* Context::getInstanceProcAddr	()
{
	return (void*)m_platformInterface.getGetInstanceProcAddr();
}

bool Context::isBufferDeviceAddressSupported(void) const
{
	return isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") ||
		   isDeviceFunctionalitySupported("VK_EXT_buffer_device_address");
}

// TestCase

void TestCase::initPrograms (SourceCollections&) const
{
}

void TestCase::checkSupport (Context&) const
{
}

void TestCase::delayedInit (void)
{
}

void collectAndReportDebugMessages(vk::DebugReportRecorder &debugReportRecorder, Context& context)
{
	// \note We are not logging INFORMATION and DEBUG messages
	static const vk::VkDebugReportFlagsEXT			errorFlags		= vk::VK_DEBUG_REPORT_ERROR_BIT_EXT;
	static const vk::VkDebugReportFlagsEXT			logFlags		= errorFlags
																	| vk::VK_DEBUG_REPORT_WARNING_BIT_EXT
																	| vk::VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;

	typedef vk::DebugReportRecorder::MessageList	DebugMessages;

	const DebugMessages&	messages	= debugReportRecorder.getMessages();
	tcu::TestLog&			log			= context.getTestContext().getLog();

	if (messages.begin() != messages.end())
	{
		const tcu::ScopedLogSection	section		(log, "DebugMessages", "Debug Messages");
		int							numErrors	= 0;

		for (DebugMessages::const_iterator curMsg = messages.begin(); curMsg != messages.end(); ++curMsg)
		{
			if ((curMsg->flags & logFlags) != 0)
				log << tcu::TestLog::Message << *curMsg << tcu::TestLog::EndMessage;

			if ((curMsg->flags & errorFlags) != 0)
				numErrors += 1;
		}

		debugReportRecorder.clearMessages();

		if (numErrors > 0)
		{
			string errorMsg = de::toString(numErrors) + " API usage errors found";
			context.resultSetOnValidation(true);
			context.getTestContext().setTestResult(QP_TEST_RESULT_INTERNAL_ERROR, errorMsg.c_str());
		}
	}
}

} // vkt
