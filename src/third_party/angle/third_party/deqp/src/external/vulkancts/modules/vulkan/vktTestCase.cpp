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

#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkDebugReportUtil.hpp"

#include "tcuCommandLine.hpp"

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

vector<string> getValidationLayers (const vector<VkLayerProperties>& supportedLayers)
{
	static const char*	s_magicLayer		= "VK_LAYER_LUNARG_standard_validation";
	static const char*	s_defaultLayers[]	=
	{
		"VK_LAYER_GOOGLE_threading",
		"VK_LAYER_LUNARG_parameter_validation",
		"VK_LAYER_LUNARG_device_limits",
		"VK_LAYER_LUNARG_object_tracker",
		"VK_LAYER_LUNARG_image",
		"VK_LAYER_LUNARG_core_validation",
		"VK_LAYER_LUNARG_swapchain",
		"VK_LAYER_GOOGLE_unique_objects"
	};

	vector<string>		enabledLayers;

	if (isLayerSupported(supportedLayers, RequiredLayer(s_magicLayer)))
		enabledLayers.push_back(s_magicLayer);
	else
	{
		for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_defaultLayers); ++ndx)
		{
			if (isLayerSupported(supportedLayers, RequiredLayer(s_defaultLayers[ndx])))
				enabledLayers.push_back(s_defaultLayers[ndx]);
		}
	}

	return enabledLayers;
}

vector<string> getValidationLayers (const PlatformInterface& vkp)
{
	return getValidationLayers(enumerateInstanceLayerProperties(vkp));
}

vector<string> getValidationLayers (const InstanceInterface& vki, VkPhysicalDevice physicalDevice)
{
	return getValidationLayers(enumerateDeviceLayerProperties(vki, physicalDevice));
}

vector<string> filterExtensions (const vector<VkExtensionProperties>& extensions)
{
	vector<string>	enabledExtensions;
	const char*		extensionGroups[] =
	{
		"VK_KHR_",
		"VK_EXT_",
		"VK_KHX_"
	};

	for (size_t extNdx = 0; extNdx < extensions.size(); extNdx++)
	{
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
	vector<string>							enabledLayers;

	// \note Extensions in core are not explicitly enabled even though
	//		 they are in the extension list advertised to tests.
	vector<const char*>						coreExtensions;
	getCoreInstanceExtensions(apiVersion, coreExtensions);
	vector<string>							nonCoreExtensions	(removeExtensions(enabledExtensions, coreExtensions));

	if (isValidationEnabled)
	{
		if (!isDebugReportSupported(vkp))
			TCU_THROW(NotSupportedError, "VK_EXT_debug_report is not supported");

		enabledLayers = getValidationLayers(vkp);
		if (enabledLayers.empty())
			TCU_THROW(NotSupportedError, "No validation layers found");
	}

	return createDefaultInstance(vkp, apiVersion, enabledLayers, nonCoreExtensions);
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
	vector<string>				enabledLayers;
	vector<const char*>			layerPtrs;
	vector<const char*>			extensionPtrs;
	const float					queuePriority	= 1.0f;
	const deUint32				numQueues = (enabledFeatures.features.sparseBinding && (queueIndex != sparseQueueIndex)) ? 2 : 1;

	deMemset(&queueInfo,	0, sizeof(queueInfo));
	deMemset(&deviceInfo,	0, sizeof(deviceInfo));

	if (cmdLine.isValidationEnabled())
	{
		enabledLayers = getValidationLayers(vki, physicalDevice);
		if (enabledLayers.empty())
			TCU_THROW(NotSupportedError, "No validation layers found");
	}

	layerPtrs.resize(enabledLayers.size());

	for (size_t ndx = 0; ndx < enabledLayers.size(); ++ndx)
		layerPtrs[ndx] = enabledLayers[ndx].c_str();

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
	deviceInfo.enabledLayerCount			= (deUint32)layerPtrs.size();
	deviceInfo.ppEnabledLayerNames			= (layerPtrs.empty() ? DE_NULL : &layerPtrs[0]);
	deviceInfo.pEnabledFeatures				= enabledFeatures.pNext ? DE_NULL : &enabledFeatures.features;

	return createDevice(vkp, instance, vki, physicalDevice, &deviceInfo);
};

bool isPhysicalDeviceFeatures2Supported (const deUint32 version, const vector<string>& instanceExtensions)
{
	return isInstanceExtensionSupported(version, instanceExtensions, "VK_KHR_get_physical_device_properties2");
}

class DeviceFeatures
{
public:
	VkPhysicalDeviceFeatures2						coreFeatures;
	VkPhysicalDeviceSamplerYcbcrConversionFeatures	samplerYCbCrConversionFeatures;
	VkPhysicalDevice16BitStorageFeatures			sixteenBitStorageFeatures;
	VkPhysicalDeviceVariablePointerFeatures			variablePointerFeatures;

	DeviceFeatures (const InstanceInterface&	vki,
					const deUint32				apiVersion,
					const VkPhysicalDevice&		physicalDevice,
					const vector<string>&		instanceExtensions,
					const vector<string>&		deviceExtensions)
	{
		deMemset(&coreFeatures, 0, sizeof(coreFeatures));
		deMemset(&samplerYCbCrConversionFeatures, 0, sizeof(samplerYCbCrConversionFeatures));
		deMemset(&sixteenBitStorageFeatures, 0, sizeof(sixteenBitStorageFeatures));
		deMemset(&variablePointerFeatures, 0, sizeof(variablePointerFeatures));

		coreFeatures.sType						= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		samplerYCbCrConversionFeatures.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
		sixteenBitStorageFeatures.sType			= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR;
		variablePointerFeatures.sType			= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES_KHR;

		if (isPhysicalDeviceFeatures2Supported(apiVersion, instanceExtensions))
		{
			void** nextPtr = &coreFeatures.pNext;

			if (de::contains(deviceExtensions.begin(), deviceExtensions.end(), "VK_KHR_sampler_ycbcr_conversion"))
			{
				*nextPtr	= &samplerYCbCrConversionFeatures;
				nextPtr		= &samplerYCbCrConversionFeatures.pNext;
			}
			if (de::contains(deviceExtensions.begin(), deviceExtensions.end(), "VK_KHR_16bit_storage"))
			{
				*nextPtr	= &sixteenBitStorageFeatures;
				nextPtr		= &sixteenBitStorageFeatures.pNext;
			}
			if (de::contains(deviceExtensions.begin(), deviceExtensions.end(), "VK_KHR_variable_pointers"))
			{
				*nextPtr	= &variablePointerFeatures;
				nextPtr		= &variablePointerFeatures.pNext;
			}

			vki.getPhysicalDeviceFeatures2(physicalDevice, &coreFeatures);
		}
		else
			coreFeatures.features = getPhysicalDeviceFeatures(vki, physicalDevice);

		// Disable robustness by default, as it has an impact on performance on some HW.
		coreFeatures.features.robustBufferAccess = false;
	}
};

} // anonymous

class DefaultDevice
{
public:
															DefaultDevice						(const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine);
															~DefaultDevice						(void);

	VkInstance												getInstance							(void) const	{ return *m_instance;										}
	const InstanceInterface&								getInstanceInterface				(void) const	{ return m_instanceInterface;								}
	deUint32												getAvailableInstanceVersion			(void) const	{ return m_availableInstanceVersion;						}
	const vector<string>&									getInstanceExtensions				(void) const	{ return m_instanceExtensions;								}

	VkPhysicalDevice										getPhysicalDevice					(void) const	{ return m_physicalDevice;									}
	deUint32												getDeviceVersion					(void) const	{ return m_deviceVersion;									}
	const VkPhysicalDeviceFeatures&							getDeviceFeatures					(void) const	{ return m_deviceFeatures.coreFeatures.features;			}
	const VkPhysicalDeviceFeatures2&						getDeviceFeatures2					(void) const	{ return m_deviceFeatures.coreFeatures; }
	const VkPhysicalDeviceSamplerYcbcrConversionFeatures&	getSamplerYCbCrConversionFeatures	(void) const	{ return m_deviceFeatures.samplerYCbCrConversionFeatures;	}
	const VkPhysicalDevice16BitStorageFeatures&				get16BitStorageFeatures				(void) const	{ return m_deviceFeatures.sixteenBitStorageFeatures;		}
	const VkPhysicalDeviceVariablePointerFeatures&			getVariablePointerFeatures			(void) const	{ return m_deviceFeatures.variablePointerFeatures;			}
	VkDevice												getDevice							(void) const	{ return *m_device;											}
	const DeviceInterface&									getDeviceInterface					(void) const	{ return m_deviceInterface;									}
	const VkPhysicalDeviceProperties&						getDeviceProperties					(void) const	{ return m_deviceProperties;								}
	const vector<string>&									getDeviceExtensions					(void) const	{ return m_deviceExtensions;								}

	deUint32												getUsedApiVersion					(void) const	{ return m_usedApiVersion;									}

	deUint32												getUniversalQueueFamilyIndex		(void) const	{ return m_universalQueueFamilyIndex;						}
	VkQueue													getUniversalQueue					(void) const;
	deUint32												getSparseQueueFamilyIndex		(void) const	{ return m_sparseQueueFamilyIndex;					}
	VkQueue													getSparseQueue					(void) const;

private:

	const deUint32						m_availableInstanceVersion;

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

	const Unique<VkDevice>				m_device;
	const DeviceDriver					m_deviceInterface;

};

DefaultDevice::DefaultDevice (const PlatformInterface& vkPlatform, const tcu::CommandLine& cmdLine)
	: m_availableInstanceVersion	(getTargetInstanceVersion(vkPlatform))
	, m_deviceVersions				(determineDeviceVersions(vkPlatform, m_availableInstanceVersion, cmdLine))
	, m_usedApiVersion				(deMinu32(m_availableInstanceVersion, m_deviceVersions.first))

	, m_instanceExtensions			(addCoreInstanceExtensions(filterExtensions(enumerateInstanceExtensionProperties(vkPlatform, DE_NULL)), m_usedApiVersion))
	, m_instance					(createInstance(vkPlatform, m_usedApiVersion, m_instanceExtensions, cmdLine))

	, m_instanceInterface			(vkPlatform, *m_instance)
	, m_physicalDevice				(chooseDevice(m_instanceInterface, *m_instance, cmdLine))
	, m_deviceVersion				(getPhysicalDeviceProperties(m_instanceInterface, m_physicalDevice).apiVersion)

	, m_deviceExtensions			(addCoreDeviceExtensions(filterExtensions(enumerateDeviceExtensionProperties(m_instanceInterface, m_physicalDevice, DE_NULL)), m_usedApiVersion))
	, m_deviceFeatures				(m_instanceInterface, m_usedApiVersion, m_physicalDevice, m_instanceExtensions, m_deviceExtensions)
	, m_universalQueueFamilyIndex	(findQueueFamilyIndexWithCaps(m_instanceInterface, m_physicalDevice, VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))
	, m_sparseQueueFamilyIndex		(m_deviceFeatures.coreFeatures.features.sparseBinding ? findQueueFamilyIndexWithCaps(m_instanceInterface, m_physicalDevice, VK_QUEUE_SPARSE_BINDING_BIT) : 0)
	, m_deviceProperties			(getPhysicalDeviceProperties(m_instanceInterface, m_physicalDevice))
	, m_device						(createDefaultDevice(vkPlatform, *m_instance, m_instanceInterface, m_physicalDevice, m_usedApiVersion, m_universalQueueFamilyIndex, m_sparseQueueFamilyIndex, m_deviceFeatures.coreFeatures, m_deviceExtensions, cmdLine))
	, m_deviceInterface				(vkPlatform, *m_instance, *m_device)
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
	if (!m_deviceFeatures.coreFeatures.features.sparseBinding)
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
	: m_testCtx				(testCtx)
	, m_platformInterface	(platformInterface)
	, m_progCollection		(progCollection)
	, m_device				(new DefaultDevice(m_platformInterface, testCtx.getCommandLine()))
	, m_allocator			(createAllocator(m_device.get()))
{
}

Context::~Context (void)
{
}

deUint32								Context::getAvailableInstanceVersion	(void) const { return m_device->getAvailableInstanceVersion();	}
const vector<string>&					Context::getInstanceExtensions			(void) const { return m_device->getInstanceExtensions();		}
vk::VkInstance							Context::getInstance					(void) const { return m_device->getInstance();					}
const vk::InstanceInterface&			Context::getInstanceInterface			(void) const { return m_device->getInstanceInterface();			}
vk::VkPhysicalDevice					Context::getPhysicalDevice				(void) const { return m_device->getPhysicalDevice();			}
deUint32								Context::getDeviceVersion				(void) const { return m_device->getDeviceVersion();				}
const vk::VkPhysicalDeviceFeatures&		Context::getDeviceFeatures				(void) const { return m_device->getDeviceFeatures();			}
const vk::VkPhysicalDeviceFeatures2&	Context::getDeviceFeatures2				(void) const { return m_device->getDeviceFeatures2();			}
const vk::VkPhysicalDeviceSamplerYcbcrConversionFeatures&
										Context::getSamplerYCbCrConversionFeatures
																				(void) const { return m_device->getSamplerYCbCrConversionFeatures();	}
const vk::VkPhysicalDevice16BitStorageFeatures&
										Context::get16BitStorageFeatures		(void) const { return m_device->get16BitStorageFeatures();		}
const vk::VkPhysicalDeviceVariablePointerFeatures&
										Context::getVariablePointerFeatures		(void) const { return m_device->getVariablePointerFeatures();	}
const vk::VkPhysicalDeviceProperties&	Context::getDeviceProperties			(void) const { return m_device->getDeviceProperties();			}
const vector<string>&					Context::getDeviceExtensions			(void) const { return m_device->getDeviceExtensions();			}
vk::VkDevice							Context::getDevice						(void) const { return m_device->getDevice();					}
const vk::DeviceInterface&				Context::getDeviceInterface				(void) const { return m_device->getDeviceInterface();			}
deUint32								Context::getUniversalQueueFamilyIndex	(void) const { return m_device->getUniversalQueueFamilyIndex();	}
vk::VkQueue								Context::getUniversalQueue				(void) const { return m_device->getUniversalQueue();			}
deUint32								Context::getSparseQueueFamilyIndex		(void) const { return m_device->getSparseQueueFamilyIndex();	}
vk::VkQueue								Context::getSparseQueue					(void) const { return m_device->getSparseQueue();				}
vk::Allocator&							Context::getDefaultAllocator			(void) const { return *m_allocator;								}
deUint32								Context::getUsedApiVersion				(void) const { return m_device->getUsedApiVersion();			}
bool									Context::contextSupports				(const deUint32 majorNum, const deUint32 minorNum, const deUint32 patchNum) const
																							{ return m_device->getUsedApiVersion() >= VK_MAKE_VERSION(majorNum, minorNum, patchNum); }
bool									Context::contextSupports				(const ApiVersion version) const
																							{ return m_device->getUsedApiVersion() >= pack(version); }
bool									Context::contextSupports				(const deUint32 requiredApiVersionBits) const
																							{ return m_device->getUsedApiVersion() >= requiredApiVersionBits; }

// TestCase

void TestCase::initPrograms (SourceCollections&) const
{
}

void TestCase::checkSupport (Context&) const
{
}

} // vkt
