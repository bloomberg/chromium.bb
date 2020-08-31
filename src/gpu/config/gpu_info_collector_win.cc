// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

// C system before C++ system.
#include <stddef.h>
#include <stdint.h>

// This has to be included before windows.h.
#include "third_party/re2/src/re2/re2.h"

#include <windows.h>

#include <d3d11.h>
#include <d3d11_3.h>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "base/file_version_info_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_native_library.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "gpu/config/gpu_util.h"
#include "third_party/vulkan_headers/include/vulkan/vulkan.h"
#include "ui/gl/direct_composition_surface_win.h"

namespace gpu {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum D3D12FeatureLevel in
// \tools\metrics\histograms\enums.xml
enum class D3D12FeatureLevel {
  kD3DFeatureLevelUnknown = 0,
  kD3DFeatureLevel_12_0 = 1,
  kD3DFeatureLevel_12_1 = 2,
  kD3DFeatureLevel_11_0 = 3,
  kD3DFeatureLevel_11_1 = 4,
  kMaxValue = kD3DFeatureLevel_11_1,
};

inline D3D12FeatureLevel ConvertToHistogramFeatureLevel(
    uint32_t d3d_feature_level) {
  switch (d3d_feature_level) {
    case 0:
      return D3D12FeatureLevel::kD3DFeatureLevelUnknown;
    case D3D_FEATURE_LEVEL_12_0:
      return D3D12FeatureLevel::kD3DFeatureLevel_12_0;
    case D3D_FEATURE_LEVEL_12_1:
      return D3D12FeatureLevel::kD3DFeatureLevel_12_1;
    case D3D_FEATURE_LEVEL_11_0:
      return D3D12FeatureLevel::kD3DFeatureLevel_11_0;
    case D3D_FEATURE_LEVEL_11_1:
      return D3D12FeatureLevel::kD3DFeatureLevel_11_1;
    default:
      NOTREACHED();
      return D3D12FeatureLevel::kD3DFeatureLevelUnknown;
  }
}

OverlaySupport FlagsToOverlaySupport(bool overlays_supported, UINT flags) {
  if (flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING)
    return OverlaySupport::kScaling;
  if (flags & DXGI_OVERLAY_SUPPORT_FLAG_DIRECT)
    return OverlaySupport::kDirect;
  if (overlays_supported)
    return OverlaySupport::kSoftware;

  return OverlaySupport::kNone;
}

}  // namespace

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OFFICIAL_BUILD)
// This function has a real implementation for official builds that can
// be found in src/third_party/amd.
bool GetAMDSwitchableInfo(bool* is_switchable,
                          uint32_t* active_vendor_id,
                          uint32_t* active_device_id);
#else
bool GetAMDSwitchableInfo(bool* is_switchable,
                          uint32_t* active_vendor_id,
                          uint32_t* active_device_id) {
  return false;
}
#endif

// This has to be called after a context is created, active GPU is identified,
// and GPU driver bug workarounds are computed again. Otherwise the workaround
// |disable_direct_composition| may not be correctly applied.
// Also, this has to be called after falling back to SwiftShader decision is
// finalized because this function depends on GL is ANGLE's GLES or not.
void CollectHardwareOverlayInfo(OverlayInfo* overlay_info) {
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE) {
    overlay_info->direct_composition =
        gl::DirectCompositionSurfaceWin::IsDirectCompositionSupported();
    overlay_info->supports_overlays =
        gl::DirectCompositionSurfaceWin::AreOverlaysSupported();
    overlay_info->nv12_overlay_support = FlagsToOverlaySupport(
        overlay_info->supports_overlays,
        gl::DirectCompositionSurfaceWin::GetOverlaySupportFlags(
            DXGI_FORMAT_NV12));
    overlay_info->yuy2_overlay_support = FlagsToOverlaySupport(
        overlay_info->supports_overlays,
        gl::DirectCompositionSurfaceWin::GetOverlaySupportFlags(
            DXGI_FORMAT_YUY2));
  }
}

bool CollectDriverInfoD3D(GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectDriverInfoD3D");

  Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
  HRESULT hr = ::CreateDXGIFactory(IID_PPV_ARGS(&dxgi_factory));
  if (FAILED(hr))
    return false;

  bool found_amd = false;
  bool found_intel = false;
  bool found_nvidia = false;

  UINT i;
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  for (i = 0; SUCCEEDED(dxgi_factory->EnumAdapters(i, &dxgi_adapter)); i++) {
    DXGI_ADAPTER_DESC desc;
    dxgi_adapter->GetDesc(&desc);

    GPUInfo::GPUDevice device;
    device.vendor_id = desc.VendorId;
    device.device_id = desc.DeviceId;
    device.sub_sys_id = desc.SubSysId;
    device.revision = desc.Revision;

    LARGE_INTEGER umd_version;
    hr = dxgi_adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),
                                             &umd_version);
    if (SUCCEEDED(hr)) {
      device.driver_version = base::StringPrintf(
          "%d.%d.%d.%d", HIWORD(umd_version.HighPart),
          LOWORD(umd_version.HighPart), HIWORD(umd_version.LowPart),
          LOWORD(umd_version.LowPart));
    } else {
      DLOG(ERROR) << "Unable to retrieve the umd version of adapter: "
                  << desc.Description << " HR: " << std::hex << hr;
    }
    switch (device.vendor_id) {
      case 0x8086:
        found_intel = true;
        break;
      case 0x1002:
        found_amd = true;
        break;
      case 0x10de:
        found_nvidia = true;
        break;
      default:
        break;
    }

    if (i == 0) {
      gpu_info->gpu = device;
    } else {
      gpu_info->secondary_gpus.push_back(device);
    }
  }

  if (found_intel && base::win::GetVersion() < base::win::Version::WIN10) {
    // Since Windows 10 (and Windows 8.1 on some systems), switchable graphics
    // platforms are managed by Windows and each adapter is accessible as
    // separate devices.
    // See https://msdn.microsoft.com/en-us/windows/dn265501(v=vs.80)
    if (found_amd) {
      bool is_amd_switchable = false;
      uint32_t active_vendor = 0, active_device = 0;
      GetAMDSwitchableInfo(&is_amd_switchable, &active_vendor, &active_device);
      gpu_info->amd_switchable = is_amd_switchable;
    } else if (found_nvidia) {
      // nvd3d9wrap.dll is loaded into all processes when Optimus is enabled.
      HMODULE nvd3d9wrap = GetModuleHandleW(L"nvd3d9wrap.dll");
      gpu_info->optimus = nvd3d9wrap != nullptr;
    }
  }

  return i > 0;
}

// DirectX 12 are included with Windows 10 and Server 2016.
void GetGpuSupportedD3D12Version(Dx12VulkanVersionInfo* info) {
  TRACE_EVENT0("gpu", "GetGpuSupportedD3D12Version");
  info->supports_dx12 = false;
  info->d3d12_feature_level = 0;

  base::ScopedNativeLibrary d3d12_library(
      base::FilePath(FILE_PATH_LITERAL("d3d12.dll")));
  if (!d3d12_library.is_valid())
    return;

  // The order of feature levels to attempt to create in D3D CreateDevice
  const D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0};

  PFN_D3D12_CREATE_DEVICE D3D12CreateDevice =
      reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(
          d3d12_library.GetFunctionPointer("D3D12CreateDevice"));
  if (D3D12CreateDevice) {
    // For the default adapter only. (*pAdapter == nullptr)
    // Check to see if the adapter supports Direct3D 12, but don't create the
    // actual device yet. (**ppDevice == nullptr)
    for (auto level : feature_levels) {
      if (SUCCEEDED(D3D12CreateDevice(nullptr, level, _uuidof(ID3D12Device),
                                      nullptr))) {
        info->d3d12_feature_level = level;
        info->supports_dx12 = (level >= D3D_FEATURE_LEVEL_12_0) ? true : false;
        break;
      }
    }
  }
}

bool BadAMDVulkanDriverVersion() {
  // Both 32-bit and 64-bit dll are broken. If 64-bit doesn't exist,
  // 32-bit dll will be used to detect the AMD Vulkan driver.
  const base::FilePath kAmdDriver64(FILE_PATH_LITERAL("amdvlk64.dll"));
  const base::FilePath kAmdDriver32(FILE_PATH_LITERAL("amdvlk32.dll"));
  std::unique_ptr<FileVersionInfoWin> file_version_info =
      FileVersionInfoWin::CreateFileVersionInfoWin(kAmdDriver64);
  if (!file_version_info) {
    file_version_info =
        FileVersionInfoWin::CreateFileVersionInfoWin(kAmdDriver32);
    if (!file_version_info)
      return false;
  }
  base::Version amd_version = file_version_info->GetFileVersion();

  // From the Canary crash logs, the broken amdvlk64.dll versions
  // are 1.0.39.0, 1.0.51.0 and 1.0.54.0. In the manual test, version
  // 9.2.10.1 dated 12/6/2017 works and version 1.0.54.0 dated 11/2/1017
  // crashes. All version numbers small than 1.0.54.0 will be marked as
  // broken.
  const base::Version kBadAMDVulkanDriverVersion("1.0.54.0");
  // CompareTo() returns -1, 0, 1 for <, ==, >.
  if (amd_version.CompareTo(kBadAMDVulkanDriverVersion) != 1)
    return true;

  return false;
}

bool InitVulkan(base::NativeLibrary* vulkan_library,
                PFN_vkGetInstanceProcAddr* vkGetInstanceProcAddr,
                PFN_vkCreateInstance* vkCreateInstance,
                uint32_t* vulkan_version) {
  *vulkan_version = 0;

  *vulkan_library =
      base::LoadNativeLibrary(base::FilePath(L"vulkan-1.dll"), nullptr);

  if (!(*vulkan_library)) {
    return false;
  }

  *vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(*vulkan_library,
                                                "vkGetInstanceProcAddr"));

  if (*vkGetInstanceProcAddr) {
    *vulkan_version = VK_MAKE_VERSION(1, 0, 0);
    PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
    vkEnumerateInstanceVersion =
        reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            (*vkGetInstanceProcAddr)(nullptr, "vkEnumerateInstanceVersion"));

    // If the vkGetInstanceProcAddr returns nullptr for
    // vkEnumerateInstanceVersion, it is a Vulkan 1.0 implementation.
    if (!vkEnumerateInstanceVersion) {
      return false;
    }

    // Return value can be VK_SUCCESS or VK_ERROR_OUT_OF_HOST_MEMORY.
    if (vkEnumerateInstanceVersion(vulkan_version) != VK_SUCCESS) {
      return false;
    }

    // The minimum version required for Vulkan to be enabled is 1.1.0.
    // No further queries will be called for early versions. They are unstable
    // and might cause crashes.
    if (*vulkan_version < VK_MAKE_VERSION(1, 1, 0)) {
      return false;
    }

    *vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        (*vkGetInstanceProcAddr)(nullptr, "vkCreateInstance"));

    if (*vkCreateInstance)
      return true;
  }

  // From the crash reports, unloading the library here might cause a crash in
  // the Vulkan loader or in the Vulkan driver. To work around it, don't
  // explicitly unload the DLL. Instead, GPU process shutdown will unload all
  // loaded DLLs.
  // base::UnloadNativeLibrary(*vulkan_library);
  return false;
}

bool InitVulkanInstanceProc(
    const VkInstance& vk_instance,
    const PFN_vkGetInstanceProcAddr& vkGetInstanceProcAddr,
    PFN_vkEnumeratePhysicalDevices* vkEnumeratePhysicalDevices,
    PFN_vkEnumerateDeviceExtensionProperties*
        vkEnumerateDeviceExtensionProperties) {

  *vkEnumeratePhysicalDevices =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          vkGetInstanceProcAddr(vk_instance, "vkEnumeratePhysicalDevices"));

  *vkEnumerateDeviceExtensionProperties =
      reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkEnumerateDeviceExtensionProperties"));

  if ((*vkEnumeratePhysicalDevices) &&
      (*vkEnumerateDeviceExtensionProperties)) {
    return true;
  }
  return false;
}

void GetGpuSupportedVulkanVersionAndExtensions(
    Dx12VulkanVersionInfo* info,
    const std::vector<const char*>& requested_vulkan_extensions,
    std::vector<bool>* extension_support) {
  TRACE_EVENT0("gpu", "GetGpuSupportedVulkanVersionAndExtensions");

  base::NativeLibrary vulkan_library;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkCreateInstance vkCreateInstance;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
  VkInstance vk_instance = VK_NULL_HANDLE;
  uint32_t physical_device_count = 0;
  info->supports_vulkan = false;
  info->vulkan_version = 0;

  // Skip if the system has an older AMD Vulkan driver amdvlk64.dll or
  // amdvlk32.dll which crashes when vkCreateInstance() is called. This bug has
  // been fixed in the latest AMD driver.
  if (BadAMDVulkanDriverVersion()) {
    return;
  }

  // Only supports a version >= 1.1.0.
  if (!InitVulkan(&vulkan_library, &vkGetInstanceProcAddr, &vkCreateInstance,
                  &info->vulkan_version)) {
    return;
  }

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

  const std::vector<const char*> enabled_instance_extensions = {
      "VK_KHR_surface", "VK_KHR_win32_surface"};

  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = enabled_instance_extensions.size();
  create_info.ppEnabledExtensionNames = enabled_instance_extensions.data();

  // Get the Vulkan API version supported in the GPU driver
  int highest_minor_version = VK_VERSION_MINOR(info->vulkan_version);
  for (int minor_version = highest_minor_version; minor_version >= 1;
       --minor_version) {
    app_info.apiVersion = VK_MAKE_VERSION(1, minor_version, 0);
    VkResult result = vkCreateInstance(&create_info, nullptr, &vk_instance);
    if (result == VK_SUCCESS && vk_instance &&
        InitVulkanInstanceProc(vk_instance, vkGetInstanceProcAddr,
                               &vkEnumeratePhysicalDevices,
                               &vkEnumerateDeviceExtensionProperties)) {
      result = vkEnumeratePhysicalDevices(vk_instance, &physical_device_count,
                                          nullptr);
      if (result == VK_SUCCESS && physical_device_count > 0) {
        info->supports_vulkan = true;
        info->vulkan_version = app_info.apiVersion;
        break;
      } else {
        // Skip destroy here. GPU process shutdown will unload all loaded DLLs.
        // vkDestroyInstance(vk_instance, nullptr);
        vk_instance = VK_NULL_HANDLE;
      }
    }
  }

  // Check whether the requested_vulkan_extensions are supported
  if (info->supports_vulkan) {
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(vk_instance, &physical_device_count,
                               physical_devices.data());

    // physical_devices[0]: Only query the default device for now
    uint32_t property_count;
    vkEnumerateDeviceExtensionProperties(physical_devices[0], nullptr,
                                         &property_count, nullptr);

    std::vector<VkExtensionProperties> extension_properties(property_count);
    if (property_count > 0) {
      vkEnumerateDeviceExtensionProperties(physical_devices[0], nullptr,
                                           &property_count,
                                           extension_properties.data());
    }

    for (size_t i = 0; i < requested_vulkan_extensions.size(); ++i) {
      for (size_t p = 0; p < property_count; ++p) {
        if (strcmp(requested_vulkan_extensions[i],
                   extension_properties[p].extensionName) == 0) {
          (*extension_support)[i] = true;
          break;
        }
      }
    }
  } else {
    info->vulkan_version = VK_MAKE_VERSION(1, 0, 0);
  }

  // From the crash reports, calling the following two functions might cause a
  // crash in the Vulkan loader or in the Vulkan driver. To work around it,
  // don't explicitly unload the DLL. Instead, GPU process shutdown will unload
  // all loaded DLLs.
  // if (vk_instance) {
  //   vkDestroyInstance(vk_instance, nullptr);
  // }
  // base::UnloadNativeLibrary(vulkan_library);
}

void RecordGpuSupportedRuntimeVersionHistograms(Dx12VulkanVersionInfo* info) {
  // D3D
  GetGpuSupportedD3D12Version(info);
  UMA_HISTOGRAM_BOOLEAN("GPU.SupportsDX12", info->supports_dx12);
  UMA_HISTOGRAM_ENUMERATION(
      "GPU.D3D12FeatureLevel",
      ConvertToHistogramFeatureLevel(info->d3d12_feature_level));

  // Vulkan
  const std::vector<const char*> vulkan_extensions = {
      "VK_KHR_external_memory_win32", "VK_KHR_external_semaphore_win32",
      "VK_KHR_win32_keyed_mutex"};
  std::vector<bool> extension_support(vulkan_extensions.size(), false);
  GetGpuSupportedVulkanVersionAndExtensions(info, vulkan_extensions,
                                            &extension_support);

  UMA_HISTOGRAM_BOOLEAN("GPU.SupportsVulkan", info->supports_vulkan);
  UMA_HISTOGRAM_ENUMERATION(
      "GPU.VulkanVersion",
      ConvertToHistogramVulkanVersion(info->vulkan_version));

  for (size_t i = 0; i < vulkan_extensions.size(); ++i) {
    std::string name = "GPU.VulkanExtSupport.";
    name.append(vulkan_extensions[i]);
    base::UmaHistogramBoolean(name, extension_support[i]);
  }
}

bool CollectD3D11FeatureInfo(D3D_FEATURE_LEVEL* d3d11_feature_level,
                             bool* has_discrete_gpu) {
  Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
  if (FAILED(::CreateDXGIFactory(IID_PPV_ARGS(&dxgi_factory))))
    return false;

  base::ScopedNativeLibrary d3d11_library(
      base::FilePath(FILE_PATH_LITERAL("d3d11.dll")));
  if (!d3d11_library.is_valid())
    return false;
  PFN_D3D11_CREATE_DEVICE D3D11CreateDevice =
      reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
          d3d11_library.GetFunctionPointer("D3D11CreateDevice"));
  if (!D3D11CreateDevice)
    return false;

  // The order of feature levels to attempt to create in D3D CreateDevice
  const D3D_FEATURE_LEVEL kFeatureLevels[] = {
      D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
      D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1};

  bool detected_discrete_gpu = false;
  D3D_FEATURE_LEVEL max_level = D3D_FEATURE_LEVEL_1_0_CORE;
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  for (UINT ii = 0; SUCCEEDED(dxgi_factory->EnumAdapters(ii, &dxgi_adapter));
       ++ii) {
    DXGI_ADAPTER_DESC desc;
    if (SUCCEEDED(dxgi_adapter->GetDesc(&desc)) && desc.VendorId == 0x1414) {
      // Bypass Microsoft software renderer.
      continue;
    }
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    D3D_FEATURE_LEVEL returned_feature_level = D3D_FEATURE_LEVEL_1_0_CORE;
    if (FAILED(D3D11CreateDevice(dxgi_adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN,
                                 /*Software=*/0,
                                 /*Flags=*/0, kFeatureLevels,
                                 _countof(kFeatureLevels), D3D11_SDK_VERSION,
                                 &d3d11_device, &returned_feature_level,
                                 /*ppImmediateContext=*/nullptr))) {
      continue;
    }
    if (returned_feature_level > max_level)
      max_level = returned_feature_level;
    Microsoft::WRL::ComPtr<ID3D11Device3> d3d11_device_3;
    if (FAILED(d3d11_device.As(&d3d11_device_3)))
      continue;
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 data = {};
    if (FAILED(d3d11_device_3->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2,
                                                   &data, sizeof(data)))) {
      continue;
    }
    if (!data.UnifiedMemoryArchitecture)
      detected_discrete_gpu = true;
  }

  if (max_level > D3D_FEATURE_LEVEL_1_0_CORE) {
    *d3d11_feature_level = max_level;
    *has_discrete_gpu = detected_discrete_gpu;
    return true;
  }
  return false;
}

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectGraphicsInfo");

  DCHECK(gpu_info);

  if (!CollectGraphicsInfoGL(gpu_info))
    return false;

  // ANGLE's renderer strings are of the form:
  // ANGLE (<adapter_identifier> Direct3D<version> vs_x_x ps_x_x)
  std::string direct3d_version;
  int vertex_shader_major_version = 0;
  int vertex_shader_minor_version = 0;
  int pixel_shader_major_version = 0;
  int pixel_shader_minor_version = 0;
  if (RE2::FullMatch(gpu_info->gl_renderer,
                     "ANGLE \\(.*\\)") &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " Direct3D(\\w+)",
                        &direct3d_version) &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " vs_(\\d+)_(\\d+)",
                        &vertex_shader_major_version,
                        &vertex_shader_minor_version) &&
      RE2::PartialMatch(gpu_info->gl_renderer,
                        " ps_(\\d+)_(\\d+)",
                        &pixel_shader_major_version,
                        &pixel_shader_minor_version)) {
    gpu_info->vertex_shader_version =
        base::StringPrintf("%d.%d",
                           vertex_shader_major_version,
                           vertex_shader_minor_version);
    gpu_info->pixel_shader_version =
        base::StringPrintf("%d.%d",
                           pixel_shader_major_version,
                           pixel_shader_minor_version);

    DCHECK(!gpu_info->vertex_shader_version.empty());
    // Note: do not reorder, used by UMA_HISTOGRAM below
    enum ShaderModel {
      SHADER_MODEL_UNKNOWN,
      SHADER_MODEL_2_0,
      SHADER_MODEL_3_0,
      SHADER_MODEL_4_0,
      SHADER_MODEL_4_1,
      SHADER_MODEL_5_0,
      NUM_SHADER_MODELS
    };
    ShaderModel shader_model = SHADER_MODEL_UNKNOWN;
    if (gpu_info->vertex_shader_version == "5.0") {
      shader_model = SHADER_MODEL_5_0;
    } else if (gpu_info->vertex_shader_version == "4.1") {
      shader_model = SHADER_MODEL_4_1;
    } else if (gpu_info->vertex_shader_version == "4.0") {
      shader_model = SHADER_MODEL_4_0;
    } else if (gpu_info->vertex_shader_version == "3.0") {
      shader_model = SHADER_MODEL_3_0;
    } else if (gpu_info->vertex_shader_version == "2.0") {
      shader_model = SHADER_MODEL_2_0;
    }
    UMA_HISTOGRAM_ENUMERATION("GPU.D3DShaderModel", shader_model,
                              NUM_SHADER_MODELS);

    // DirectX diagnostics are collected asynchronously because it takes a
    // couple of seconds.
  }
  return true;
}

bool CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  TRACE_EVENT0("gpu", "CollectPreliminaryGraphicsInfo");
  DCHECK(gpu_info);
  // TODO(zmo): we only need to call CollectDriverInfoD3D() if we use ANGLE.
  return CollectDriverInfoD3D(gpu_info);
}

}  // namespace gpu
