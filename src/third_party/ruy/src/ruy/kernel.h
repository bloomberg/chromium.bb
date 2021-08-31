/* Copyright 2019 Google LLC. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef RUY_RUY_KERNEL_H_
#define RUY_RUY_KERNEL_H_

#include "ruy/platform.h"

// IWYU pragma: begin_exports
#if RUY_PLATFORM_NEON
#include "ruy/kernel_arm.h"
#elif RUY_PLATFORM_X86
#include "ruy/kernel_x86.h"
#else
#include "ruy/kernel_common.h"
#endif
// IWYU pragma: end_exports

#endif  // RUY_RUY_KERNEL_H_
