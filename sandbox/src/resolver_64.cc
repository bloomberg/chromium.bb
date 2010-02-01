// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/resolver.h"

#include "base/pe_image.h"
#include "sandbox/src/sandbox_nt_util.h"

namespace {

const BYTE kPushRax = 0x50;
const USHORT kMovRax = 0xB848;
const ULONG kMovRspRax = 0x24048948;
const BYTE kRetNp = 0xC3;

#pragma pack(push, 1)
struct InternalThunk {
  // This struct contains roughly the following code:
  // 00 50                    push  rax
  // 01 48b8f0debc9a78563412  mov   rax,123456789ABCDEF0h
  // 0b 48890424              mov   qword ptr [rsp],rax
  // 0f c3                    ret
  //
  // The code modifies rax, but that should not be an issue for the common
  // calling conventions.

  InternalThunk() {
    push_rax = kPushRax;
    mov_rax = kMovRax;
    interceptor_function = 0;
    mov_rsp_rax = kMovRspRax;
    ret = kRetNp;
  };
  BYTE push_rax;        // = 50
  USHORT mov_rax;       // = 48 B8
  ULONG_PTR interceptor_function;
  ULONG mov_rsp_rax;    // = 48 89 04 24
  BYTE ret;             // = C3
};
#pragma pack(pop)

} // namespace.

namespace sandbox {

NTSTATUS ResolverThunk::Init(const void* target_module,
                             const void* interceptor_module,
                             const char* target_name,
                             const char* interceptor_name,
                             const void* interceptor_entry_point,
                             void* thunk_storage,
                             size_t storage_bytes) {
  if (NULL == thunk_storage || 0 == storage_bytes ||
      NULL == target_module || NULL == target_name)
    return STATUS_INVALID_PARAMETER;

  if (storage_bytes < GetThunkSize())
    return STATUS_BUFFER_TOO_SMALL;

  NTSTATUS ret = STATUS_SUCCESS;
  if (NULL == interceptor_entry_point) {
    ret = ResolveInterceptor(interceptor_module, interceptor_name,
                             &interceptor_entry_point);
    if (!NT_SUCCESS(ret))
      return ret;
  }

  ret = ResolveTarget(target_module, target_name, &target_);
  if (!NT_SUCCESS(ret))
    return ret;

  interceptor_ = interceptor_entry_point;

  return ret;
}

size_t ResolverThunk::GetInternalThunkSize() const {
  return sizeof(InternalThunk);
}

bool ResolverThunk::SetInternalThunk(void* storage, size_t storage_bytes,
                                     const void* original_function,
                                     const void* interceptor) {
  if (storage_bytes < sizeof(InternalThunk))
    return false;

  InternalThunk* thunk = new(storage, NT_PLACE) InternalThunk;
  thunk->interceptor_function = reinterpret_cast<ULONG_PTR>(interceptor);

  return true;
}

NTSTATUS ResolverThunk::ResolveInterceptor(const void* interceptor_module,
                                           const char* interceptor_name,
                                           const void** address) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS ResolverThunk::ResolveTarget(const void* module,
                                      const char* function_name,
                                      void** address) {
  return STATUS_NOT_IMPLEMENTED;
}

}  // namespace sandbox
