// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_trace_event_impl.h"

#include "base/debug/trace_event.h"
#include "ppapi/thunk/thunk.h"


namespace ppapi {

// PPB_Trace_Event_Dev is a shared implementation because Trace Events can be
// sent from either the plugin process or renderer process depending on whether
// the plugin is in- or out-of-process.  Also, for NaCl plugins these functions
// will be executed from untrusted code and handled appropriately by tracing
// functionality in the IRT.

// static
void* TraceEventImpl::GetCategoryEnabled(const char* category_name) {
  // This casting is here because all mem_t return types in Pepper are void* and
  // non-const.  All mem_t parameters are const void* so there is no way to
  // return a pointer type to the caller without some const_cast.  The pointer
  // type the tracing system works with is normally unsigned char*.
  return const_cast<void*>(static_cast<const void*>(
      base::debug::TraceLog::GetInstance()->GetCategoryEnabled(category_name)));
}

// static
void TraceEventImpl::AddTraceEvent(int8_t phase,
                                      const void* category_enabled,
                                      const char* name,
                                      uint64_t id,
                                      uint32_t num_args,
                                      const char* arg_names[],
                                      const uint8_t arg_types[],
                                      const uint64_t arg_values[],
                                      uint8_t flags) {
  base::debug::TraceLog::GetInstance()->AddTraceEvent(phase,
      static_cast<const unsigned char*>(category_enabled), name, id, num_args,
      arg_names, arg_types,
      // This cast is necessary for LP64 systems, where uint64_t is defined as
      // an unsigned long int, but trace_event internals are hermetic and
      // accepts an |unsigned long long*|.  The pointer types are compatible but
      // the compiler throws an error without an explicit cast.
      reinterpret_cast<const unsigned long long*>(arg_values), flags);
}

// static
void TraceEventImpl::SetThreadName(const char* thread_name) {
  base::PlatformThread::SetName(thread_name);
}

namespace {

const PPB_Trace_Event_Dev g_ppb_trace_event_thunk = {
  &TraceEventImpl::GetCategoryEnabled,
  &TraceEventImpl::AddTraceEvent,
  &TraceEventImpl::SetThreadName,
};

}  // namespace ppapi

}  // namespace

namespace ppapi {
namespace thunk {

const PPB_Trace_Event_Dev_0_1* GetPPB_Trace_Event_Dev_0_1_Thunk() {
  return &g_ppb_trace_event_thunk;
}

}  // namespace thunk
}  // namespace ppapi
