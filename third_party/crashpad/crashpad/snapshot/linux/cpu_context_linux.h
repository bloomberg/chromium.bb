// Copyright 2017 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_SNAPSHOT_LINUX_SNAPSHOT_CPU_CONTEXT_LINUX_H_
#define CRASHPAD_SNAPSHOT_LINUX_SNAPSHOT_CPU_CONTEXT_LINUX_H_

#include "build/build_config.h"
#include "snapshot/cpu_context.h"
#include "snapshot/linux/signal_context.h"
#include "util/linux/thread_info.h"

namespace crashpad {
namespace internal {

#if defined(ARCH_CPU_X86_FAMILY) || DOXYGEN

//! \{
//! \brief Initializes a CPUContextX86 structure from native context structures
//!     on Linux.
//!
//! \param[in] thread_context The native thread context.
//! \param[in] float_context The native float context.
//! \param[out] context The CPUContextX86 structure to initialize.
void InitializeCPUContextX86(const ThreadContext::t32_t& thread_context,
                             const FloatContext::f32_t& float_context,
                             CPUContextX86* context);

void InitializeCPUContextX86(const SignalThreadContext32& thread_context,
                             const SignalFloatContext32& float_context,
                             CPUContextX86* context);
//! \}

//! \brief Initializes GPR and debug state in a CPUContextX86 from a native
//!     signal context structure on Linux.
//!
//! Floating point state is not initialized. Debug registers are initialized to
//! zero.
//!
//! \param[in] thread_context The native thread context.
//! \param[out] context The CPUContextX86 structure to initialize.
void InitializeCPUContextX86_NoFloatingPoint(
    const SignalThreadContext32& thread_context,
    CPUContextX86* context);

// \{
//! \brief Initializes a CPUContextX86_64 structure from native context
//!     structures on Linux.
//!
//! \param[in] thread_context The native thread context.
//! \param[in] float_context The native float context.
//! \param[out] context The CPUContextX86_64 structure to initialize.
void InitializeCPUContextX86_64(const ThreadContext::t64_t& thread_context,
                                const FloatContext::f64_t& float_context,
                                CPUContextX86_64* context);

void InitializeCPUContextX86_64(const SignalThreadContext64& thread_context,
                                const SignalFloatContext64& float_context,
                                CPUContextX86_64* context);
//! \}
#else
#error Port.  // TODO(jperaza): ARM
#endif  // ARCH_CPU_X86_FAMILY || DOXYGEN

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_SNAPSHOT_CPU_CONTEXT_LINUX_H_
