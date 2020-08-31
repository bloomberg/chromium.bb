#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for Vulkan function pointers."""

import filecmp
import optparse
import os
import platform
import sys
from os import path
from string import Template
from subprocess import call

vulkan_reg_path = path.join(path.dirname(__file__), "..", "..", "third_party",
                            "vulkan_headers", "registry")
sys.path.append(vulkan_reg_path)
from reg import Registry

registry = Registry()
registry.loadFile(open(path.join(vulkan_reg_path, "vk.xml")))

VULKAN_UNASSOCIATED_FUNCTIONS = [
  {
    'functions': [
      # vkGetInstanceProcAddr belongs here but is handled specially.
      # vkEnumerateInstanceVersion belongs here but is handled specially.
      'vkCreateInstance',
      'vkEnumerateInstanceExtensionProperties',
      'vkEnumerateInstanceLayerProperties',
    ]
  }
]

VULKAN_INSTANCE_FUNCTIONS = [
  {
    'functions': [
      'vkCreateDevice',
      'vkDestroyInstance',
      'vkEnumerateDeviceExtensionProperties',
      'vkEnumerateDeviceLayerProperties',
      'vkEnumeratePhysicalDevices',
      'vkGetDeviceProcAddr',
      'vkGetPhysicalDeviceFeatures',
      'vkGetPhysicalDeviceFormatProperties',
      'vkGetPhysicalDeviceMemoryProperties',
      'vkGetPhysicalDeviceProperties',
      'vkGetPhysicalDeviceQueueFamilyProperties',
    ]
  },
  {
    'ifdef': 'DCHECK_IS_ON()',
    'extension': 'VK_EXT_DEBUG_REPORT_EXTENSION_NAME',
    'functions': [
      'vkCreateDebugReportCallbackEXT',
      'vkDestroyDebugReportCallbackEXT',
    ]
  },
  {
    'extension': 'VK_KHR_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkDestroySurfaceKHR',
      'vkGetPhysicalDeviceSurfaceCapabilitiesKHR',
      'vkGetPhysicalDeviceSurfaceFormatsKHR',
      'vkGetPhysicalDeviceSurfaceSupportKHR',
    ]
  },
  {
    'ifdef': 'defined(USE_VULKAN_XLIB)',
    'extension': 'VK_KHR_XLIB_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateXlibSurfaceKHR',
      'vkGetPhysicalDeviceXlibPresentationSupportKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_WIN)',
    'extension': 'VK_KHR_WIN32_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateWin32SurfaceKHR',
      'vkGetPhysicalDeviceWin32PresentationSupportKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_ANDROID)',
    'extension': 'VK_KHR_ANDROID_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateAndroidSurfaceKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME',
    'functions': [
      'vkCreateImagePipeSurfaceFUCHSIA',
    ]
  },
  {
  'min_api_version': 'VK_API_VERSION_1_1',
    'functions': [
      'vkGetPhysicalDeviceImageFormatProperties2',
    ]
  },
  {
    # vkGetPhysicalDeviceFeatures2() is defined in Vulkan 1.1 or suffixed in the
    # VK_KHR_get_physical_device_properties2 extension.
    'min_api_version': 'VK_API_VERSION_1_1',
    'extension': 'VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME',
    'extension_suffix': 'KHR',
    'functions': [
      'vkGetPhysicalDeviceFeatures2',
    ]
  },
]

VULKAN_DEVICE_FUNCTIONS = [
  {
    'functions': [
      'vkAllocateCommandBuffers',
      'vkAllocateDescriptorSets',
      'vkAllocateMemory',
      'vkBeginCommandBuffer',
      'vkBindBufferMemory',
      'vkBindImageMemory',
      'vkCmdBeginRenderPass',
      'vkCmdCopyBuffer',
      'vkCmdCopyBufferToImage',
      'vkCmdEndRenderPass',
      'vkCmdExecuteCommands',
      'vkCmdNextSubpass',
      'vkCmdPipelineBarrier',
      'vkCreateBuffer',
      'vkCreateCommandPool',
      'vkCreateDescriptorPool',
      'vkCreateDescriptorSetLayout',
      'vkCreateFence',
      'vkCreateFramebuffer',
      'vkCreateImage',
      'vkCreateImageView',
      'vkCreateRenderPass',
      'vkCreateSampler',
      'vkCreateSemaphore',
      'vkCreateShaderModule',
      'vkDestroyBuffer',
      'vkDestroyCommandPool',
      'vkDestroyDescriptorPool',
      'vkDestroyDescriptorSetLayout',
      'vkDestroyDevice',
      'vkDestroyFence',
      'vkDestroyFramebuffer',
      'vkDestroyImage',
      'vkDestroyImageView',
      'vkDestroyRenderPass',
      'vkDestroySampler',
      'vkDestroySemaphore',
      'vkDestroyShaderModule',
      'vkDeviceWaitIdle',
      'vkFlushMappedMemoryRanges',
      'vkEndCommandBuffer',
      'vkFreeCommandBuffers',
      'vkFreeDescriptorSets',
      'vkFreeMemory',
      'vkInvalidateMappedMemoryRanges',
      'vkGetBufferMemoryRequirements',
      'vkGetDeviceQueue',
      'vkGetFenceStatus',
      'vkGetImageMemoryRequirements',
      'vkMapMemory',
      'vkQueueSubmit',
      'vkQueueWaitIdle',
      'vkResetCommandBuffer',
      'vkResetFences',
      'vkUnmapMemory',
      'vkUpdateDescriptorSets',
      'vkWaitForFences',
    ]
  },
  {
    'min_api_version': 'VK_API_VERSION_1_1',
    'functions': [
      'vkGetDeviceQueue2',
      'vkGetBufferMemoryRequirements2',
      'vkGetImageMemoryRequirements2',
    ]
  },
  {
    'ifdef': 'defined(OS_ANDROID)',
    'extension':
        'VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME',
    'functions': [
      'vkGetAndroidHardwareBufferPropertiesANDROID',
    ]
  },
  {
    'ifdef': 'defined(OS_LINUX) || defined(OS_ANDROID)',
    'extension': 'VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME',
    'functions': [
      'vkGetSemaphoreFdKHR',
      'vkImportSemaphoreFdKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_WIN)',
    'extension': 'VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME',
    'functions': [
      'vkGetSemaphoreWin32HandleKHR',
      'vkImportSemaphoreWin32HandleKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_LINUX) || defined(OS_ANDROID)',
    'extension': 'VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME',
    'functions': [
      'vkGetMemoryFdKHR',
      'vkGetMemoryFdPropertiesKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_WIN)',
    'extension': 'VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME',
    'functions': [
      'vkGetMemoryWin32HandleKHR',
      'vkGetMemoryWin32HandlePropertiesKHR',
    ]
  },
  {
    'ifdef': 'defined(OS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME',
    'functions': [
      'vkImportSemaphoreZirconHandleFUCHSIA',
      'vkGetSemaphoreZirconHandleFUCHSIA',
    ]
  },
  {
    'ifdef': 'defined(OS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME',
    'functions': [
      'vkGetMemoryZirconHandleFUCHSIA',
    ]
  },
  {
    'ifdef': 'defined(OS_FUCHSIA)',
    'extension': 'VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME',
    'functions': [
      'vkCreateBufferCollectionFUCHSIA',
      'vkSetBufferCollectionConstraintsFUCHSIA',
      'vkGetBufferCollectionPropertiesFUCHSIA',
      'vkDestroyBufferCollectionFUCHSIA',
    ]
  },
  {
    'extension': 'VK_KHR_SWAPCHAIN_EXTENSION_NAME',
    'functions': [
      'vkAcquireNextImageKHR',
      'vkCreateSwapchainKHR',
      'vkDestroySwapchainKHR',
      'vkGetSwapchainImagesKHR',
      'vkQueuePresentKHR',
    ]
  }

]

SELF_LOCATION = os.path.dirname(os.path.abspath(__file__))

LICENSE_AND_HEADER = """\
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

"""

def WriteFunctionsInternal(file, functions, gen_content, check_extension=False):
  for group in functions:
    if 'ifdef' in group:
      file.write('#if %s\n' % group['ifdef'])

    extension = group['extension'] if 'extension' in group else ''
    min_api_version = \
        group['min_api_version'] if 'min_api_version' in group else ''

    if not check_extension:
      for func in group['functions']:
        file.write(gen_content(func))
    elif not extension and not min_api_version:
      for func in group['functions']:
        file.write(gen_content(func))
    else:
      if min_api_version:
        file.write('  if (api_version >= %s) {\n' % min_api_version)

        for func in group['functions']:
          file.write(
              gen_content(func))

        file.write('}\n')
        if extension:
          file.write('else ')

      if extension:
        file.write('if (gfx::HasExtension(enabled_extensions, %s)) {\n' %
                   extension)

        extension_suffix = \
            group['extension_suffix'] if 'extension_suffix' in group \
            else ''
        for func in group['functions']:
          file.write(gen_content(func, extension_suffix))

        file.write('}\n')
    if 'ifdef' in group:
      file.write('#endif  // %s\n' % group['ifdef'])
    file.write('\n')

def WriteFunctions(file, functions, template, check_extension=False):
  def gen_content(func, suffix=''):
    return template.substitute({'name': func,'extension_suffix': suffix})
  WriteFunctionsInternal(file, functions, gen_content, check_extension)

def WriteFunctionDeclarations(file, functions):
  template = Template('  VulkanFunction<PFN_${name}> ${name}Fn;\n')
  WriteFunctions(file, functions, template)

def WriteMacros(file, functions):
  def gen_content(func, suffix=''):
    if func not in registry.cmddict:
      # Some fuchsia functions are not in the vulkan registry, so use macro for
      # them.
      template = Template(
          '#define $name gpu::GetVulkanFunctionPointers()->${name}Fn\n')
      return  template.substitute({'name': func, 'extension_suffix' : suffix})
    none_str = lambda s: s if s else ''
    cmd = registry.cmddict[func].elem
    proto = cmd.find('proto')
    params = cmd.findall('param')
    pdecl = none_str(proto.text)
    for elem in proto:
      text = none_str(elem.text)
      tail = none_str(elem.tail)
      pdecl += text + tail
    n = len(params)

    callstat = 'return gpu::GetVulkanFunctionPointers()->%sFn(' % func
    paramdecl = '('
    if n > 0:
      paramnames = (''.join(t for t in p.itertext())
                    for p in params)
      paramdecl += ', '.join(paramnames)
      paramnames = (''.join(p[1].text)
                    for p in params)
      callstat += ', '.join(paramnames)
    else:
        paramdecl += 'void'
    paramdecl += ')'
    callstat += ')'
    pdecl += paramdecl
    return 'ALWAYS_INLINE %s { %s; }\n' % (pdecl, callstat)

  WriteFunctionsInternal(file, functions, gen_content)

def GenerateHeaderFile(file):
  """Generates gpu/vulkan/vulkan_function_pointers.h"""

  file.write(LICENSE_AND_HEADER +
"""

#ifndef GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
#define GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_

#include <vulkan/vulkan.h>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/native_library.h"
#include "build/build_config.h"
#include "ui/gfx/extension_set.h"

#if defined(OS_ANDROID)
#include <vulkan/vulkan_android.h>
#endif

#if defined(OS_FUCHSIA)
#include <zircon/types.h>
// <vulkan/vulkan_fuchsia.h> must be included after <zircon/types.h>
#include <vulkan/vulkan_fuchsia.h>

#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if defined(USE_VULKAN_XLIB)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#endif

#if defined(OS_WIN)
#include <vulkan/vulkan_win32.h>
#endif

namespace gpu {

struct VulkanFunctionPointers;

COMPONENT_EXPORT(VULKAN) VulkanFunctionPointers* GetVulkanFunctionPointers();

struct COMPONENT_EXPORT(VULKAN) VulkanFunctionPointers {
  VulkanFunctionPointers();
  ~VulkanFunctionPointers();

  bool BindUnassociatedFunctionPointers();

  // These functions assume that vkGetInstanceProcAddr has been populated.
  bool BindInstanceFunctionPointers(
      VkInstance vk_instance,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  // These functions assume that vkGetDeviceProcAddr has been populated.
  bool BindDeviceFunctionPointers(
      VkDevice vk_device,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  base::NativeLibrary vulkan_loader_library = nullptr;

  template<typename T>
  class VulkanFunction;
  template <typename R, typename ...Args>
  class VulkanFunction <R(VKAPI_PTR*)(Args...)> {
   public:
    using Fn = R(VKAPI_PTR*)(Args...);

    explicit operator bool() {
      return !!fn_;
    }

    NO_SANITIZE("cfi-icall")
    R operator()(Args... args) {
      return fn_(args...);
    }

    Fn get() const { return fn_; }

   private:
    friend VulkanFunctionPointers;

    Fn operator=(Fn fn) {
      fn_ = fn;
      return fn_;
    }

    Fn fn_ = nullptr;
  };

  // Unassociated functions
  VulkanFunction<PFN_vkEnumerateInstanceVersion> vkEnumerateInstanceVersionFn;
  VulkanFunction<PFN_vkGetInstanceProcAddr> vkGetInstanceProcAddrFn;

""")

  WriteFunctionDeclarations(file, VULKAN_UNASSOCIATED_FUNCTIONS)

  file.write("""\

  // Instance functions
""")

  WriteFunctionDeclarations(file, VULKAN_INSTANCE_FUNCTIONS);

  file.write("""\

  // Device functions
""")

  WriteFunctionDeclarations(file, VULKAN_DEVICE_FUNCTIONS)

  file.write("""\
};

}  // namespace gpu

// Unassociated functions
""")

  WriteMacros(file, [{'functions': [ 'vkGetInstanceProcAddr' ,
                                     'vkEnumerateInstanceVersion']}])
  WriteMacros(file, VULKAN_UNASSOCIATED_FUNCTIONS)

  file.write("""\

// Instance functions
""")

  WriteMacros(file, VULKAN_INSTANCE_FUNCTIONS);

  file.write("""\

// Device functions
""")

  WriteMacros(file, VULKAN_DEVICE_FUNCTIONS)

  file.write("""\

#endif  // GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_""")

def WriteFunctionPointerInitialization(file, proc_addr_function, parent,
                                       functions):
  template = Template("""  ${name}Fn = reinterpret_cast<PFN_${name}>(
    ${get_proc_addr}(${parent}, "${name}${extension_suffix}"));
  if (!${name}Fn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "${name}${extension_suffix}";
    return false;
  }

""")

  # Substitute all values in the template, except name, which is processed in
  # WriteFunctions().
  template = Template(template.substitute({
        'name': '${name}', 'extension_suffix': '${extension_suffix}',
        'get_proc_addr': proc_addr_function, 'parent': parent}))

  WriteFunctions(file, functions, template, check_extension=True)

def WriteUnassociatedFunctionPointerInitialization(file, functions):
  WriteFunctionPointerInitialization(file, 'vkGetInstanceProcAddr', 'nullptr',
                                     functions)

def WriteInstanceFunctionPointerInitialization(file, functions):
  WriteFunctionPointerInitialization(file, 'vkGetInstanceProcAddr',
                                     'vk_instance', functions)

def WriteDeviceFunctionPointerInitialization(file, functions):
  WriteFunctionPointerInitialization(file, 'vkGetDeviceProcAddr', 'vk_device',
                                     functions)

def GenerateSourceFile(file):
  """Generates gpu/vulkan/vulkan_function_pointers.cc"""

  file.write(LICENSE_AND_HEADER +
"""

#include "gpu/vulkan/vulkan_function_pointers.h"

#include "base/logging.h"
#include "base/no_destructor.h"

namespace gpu {

VulkanFunctionPointers* GetVulkanFunctionPointers() {
  static base::NoDestructor<VulkanFunctionPointers> vulkan_function_pointers;
  return vulkan_function_pointers.get();
}

VulkanFunctionPointers::VulkanFunctionPointers() = default;
VulkanFunctionPointers::~VulkanFunctionPointers() = default;

bool VulkanFunctionPointers::BindUnassociatedFunctionPointers() {
  // vkGetInstanceProcAddr must be handled specially since it gets its function
  // pointer through base::GetFunctionPOinterFromNativeLibrary(). Other Vulkan
  // functions don't do this.
  vkGetInstanceProcAddrFn = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(vulkan_loader_library,
                                                "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddrFn)
    return false;

  vkEnumerateInstanceVersionFn =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
          vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
  // vkEnumerateInstanceVersion didn't exist in Vulkan 1.0, so we should
  // proceed even if we fail to get vkEnumerateInstanceVersion pointer.
""")

  WriteUnassociatedFunctionPointerInitialization(
      file, VULKAN_UNASSOCIATED_FUNCTIONS)

  file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindInstanceFunctionPointers(
    VkInstance vk_instance,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
""")

  WriteInstanceFunctionPointerInitialization(file, VULKAN_INSTANCE_FUNCTIONS);

  file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindDeviceFunctionPointers(
    VkDevice vk_device,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
  // Device functions
""")
  WriteDeviceFunctionPointerInitialization(file, VULKAN_DEVICE_FUNCTIONS)

  file.write("""\

  return true;
}

}  // namespace gpu
""")

def main(argv):
  """This is the main function."""

  parser = optparse.OptionParser()
  parser.add_option(
      "--output-dir",
      help="Output directory for generated files. Defaults to this script's "
      "directory.")
  parser.add_option(
      "-c", "--check", action="store_true",
      help="Check if output files match generated files in chromium root "
      "directory. Use this in PRESUBMIT scripts with --output-dir.")

  (options, _) = parser.parse_args(args=argv)

  # Support generating files for PRESUBMIT.
  if options.output_dir:
    output_dir = options.output_dir
  else:
    output_dir = SELF_LOCATION

  def ClangFormat(filename):
    formatter = "clang-format"
    if platform.system() == "Windows":
      formatter += ".bat"
    call([formatter, "-i", "-style=chromium", filename])

  header_file_name = 'vulkan_function_pointers.h'
  header_file = open(
      os.path.join(output_dir, header_file_name), 'w')
  GenerateHeaderFile(header_file)
  header_file.close()
  ClangFormat(header_file.name)

  source_file_name = 'vulkan_function_pointers.cc'
  source_file = open(
      os.path.join(output_dir, source_file_name), 'w')
  GenerateSourceFile(source_file)
  source_file.close()
  ClangFormat(source_file.name)

  check_failed_filenames = []
  if options.check:
    for filename in [header_file_name, source_file_name]:
      if not filecmp.cmp(os.path.join(output_dir, filename),
                         os.path.join(SELF_LOCATION, filename)):
        check_failed_filenames.append(filename)

  if len(check_failed_filenames) > 0:
    print('Please run gpu/vulkan/generate_bindings.py')
    print('Failed check on generated files:')
    for filename in check_failed_filenames:
      print(filename)
    return 1

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
