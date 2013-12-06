// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These helpers allow to avoid the use of an #ifdef when the code can
// compile without them. Thanks to compiler optimizations, the final generated
// binary should look the same when using these.

#ifndef SANDBOX_LINUX_SERVICES_BUILD_CONFIG_FUNCTIONS_H_
#define SANDBOX_LINUX_SERVICES_BUILD_CONFIG_FUNCTIONS_H_

#include "build/build_config.h"

namespace sandbox {

namespace {

inline bool IsASANBuild() {
#if defined(ADDRESS_SANITIZER)
  return true;
#else
  return false;
#endif
}

inline bool IsLinux() {
#if defined(OS_LINUX)
  return true;
#else
  return false;
#endif
}

inline bool IsChromeOS() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

inline bool IsAndroid() {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureX86_64() {
#if defined(ARCH_CPU_X86_64)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureI386() {
#if defined(ARCH_CPU_X86)
  return true;
#else
  return false;
#endif
}

inline bool IsArchitectureARM() {
#if defined(ARCH_CPU_ARM_FAMILY)
  return true;
#else
  return false;
#endif
}

inline bool IsUsingToolKitGtk() {
#if defined(TOOLKIT_GTK)
  return true;
#else
  return false;
#endif
}

}  // namespace.

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SERVICES_BUILD_CONFIG_FUNCTIONS_H_
