/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// We use this header to include vk_mem_alloc.h to make sure we always include GrVkDefines.h first.
// We need to do this so that the corect defines are setup before we include vulkan.h inside of
// vk_mem_alloc.h

#ifndef GrVulkanMemoryAllocator_DEFINED
#define GrVulkanMemoryAllocator_DEFINED

// We only ever include this from src files which have already included vulkan.
#ifndef VULKAN_CORE_H_
#error "vulkan_core.h has not been included before trying to include the GrVulkanMemoryAllocator"
#endif

// TODO: We currently lock down our API to Vulkan 1.1. When we update Skia to support 1.3 then we
// can remove this macro. We should also update the setting of the API level in the vma createInfo
// struct when we do this
#define VMA_VULKAN_VERSION 1001000

// vk_mem_alloc.h checks to see if VULKAN_H_ has been included before trying to include vulkan.h.
// However, some builds of Skia may not have access to vulkan.h and just have access to
// vulkan_core.h. So we pretend we've already included vulkan.h (if it already hasn't been) which
// will be fine for building internal skia files. If we do fake it out by defining VULKAN_H_ we
// need to make sure to undefine it incase outside client code does later try to include the
// real vulkan.h
#ifndef VULKAN_H_
#define VULKAN_H_
#define GR_NEEDED_TO_DEFINE_VULKAN_H
#endif
#include "vk_mem_alloc.h"
#ifdef GR_NEEDED_TO_DEFINE_VULKAN_H
#undef VULKAN_H_
#endif

#endif
