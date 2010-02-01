// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/src/service_resolver.h"

#include "base/logging.h"
#include "base/pe_image.h"
#include "base/scoped_ptr.h"

namespace {
#pragma pack(push, 1)

const ULONG kMmovR10EcxMovEax = 0xB8D18B4C;
const USHORT kSyscall = 0x050F;
const BYTE kRetNp = 0xC3;

// Service code for 64 bit systems.
struct ServiceEntry {
  // This struct contains roughly the following code:
  // 00 mov     r10,rcx
  // 03 mov     eax,52h
  // 08 syscall
  // 0a ret
  // 0b xchg    ax,ax
  // 0e xchg    ax,ax

  ULONG mov_r10_rcx_mov_eax;  // = 4C 8B D1 B8
  ULONG service_id;
  USHORT syscall;             // = 0F 05
  BYTE ret;                   // = C3
  BYTE pad;                   // = 66
  USHORT xchg_ax_ax1;         // = 66 90
  USHORT xchg_ax_ax2;         // = 66 90
};

// We don't have an internal thunk for x64.
struct ServiceFullThunk {
  ServiceEntry original;
};

#pragma pack(pop)

// Simple utility function to write to a buffer on the child, if the memery has
// write protection attributes.
// Arguments:
// child_process (in): process to write to.
// address (out): memory position on the child to write to.
// buffer (in): local buffer with the data to write .
// length (in): number of bytes to write.
// Returns true on success.
bool WriteProtectedChildMemory(HANDLE child_process,
                               void* address,
                               const void* buffer,
                               size_t length) {
  // first, remove the protections
  DWORD old_protection;
  if (!::VirtualProtectEx(child_process, address, length,
                          PAGE_WRITECOPY, &old_protection))
    return false;

  SIZE_T written;
  bool ok = ::WriteProcessMemory(child_process, address, buffer, length,
                                 &written) && (length == written);

  // always attempt to restore the original protection
  if (!::VirtualProtectEx(child_process, address, length,
                          old_protection, &old_protection))
    return false;

  return ok;
}

};  // namespace

namespace sandbox {

NTSTATUS ServiceResolverThunk::Setup(const void* target_module,
                                     const void* interceptor_module,
                                     const char* target_name,
                                     const char* interceptor_name,
                                     const void* interceptor_entry_point,
                                     void* thunk_storage,
                                     size_t storage_bytes,
                                     size_t* storage_used) {
  NTSTATUS ret = Init(target_module, interceptor_module, target_name,
                      interceptor_name, interceptor_entry_point,
                      thunk_storage, storage_bytes);
  if (!NT_SUCCESS(ret))
    return ret;

  size_t thunk_bytes = GetThunkSize();
  scoped_array<char> thunk_buffer(new char[thunk_bytes]);
  ServiceFullThunk* thunk = reinterpret_cast<ServiceFullThunk*>(
                                thunk_buffer.get());

  if (!IsFunctionAService(&thunk->original))
    return STATUS_UNSUCCESSFUL;

  ret = PerformPatch(thunk, thunk_storage);

  if (NULL != storage_used)
    *storage_used = thunk_bytes;

  return ret;
}

NTSTATUS ServiceResolverThunk::ResolveInterceptor(
    const void* interceptor_module,
    const char* interceptor_name,
    const void** address) {
  // After all, we are using a locally mapped version of the exe, so the
  // action is the same as for a target function.
  return ResolveTarget(interceptor_module, interceptor_name,
                       const_cast<void**>(address));
}

// In this case all the work is done from the parent, so resolve is
// just a simple GetProcAddress.
NTSTATUS ServiceResolverThunk::ResolveTarget(const void* module,
                                             const char* function_name,
                                             void** address) {
  DCHECK(address);
  if (NULL == module)
    return STATUS_UNSUCCESSFUL;

  PEImage module_image(module);
  *address = module_image.GetProcAddress(function_name);

  if (NULL == *address)
    return STATUS_UNSUCCESSFUL;

  return STATUS_SUCCESS;
}

size_t ServiceResolverThunk::GetThunkSize() const {
  return sizeof(ServiceFullThunk);
}

bool ServiceResolverThunk::IsFunctionAService(void* local_thunk) const {
  ServiceEntry function_code;
  SIZE_T read;
  if (!::ReadProcessMemory(process_, target_, &function_code,
                           sizeof(function_code), &read))
    return false;

  if (sizeof(function_code) != read)
    return false;

  if (kMmovR10EcxMovEax != function_code.mov_r10_rcx_mov_eax ||
      kSyscall != function_code.syscall || kRetNp != function_code.ret)
    return false;

  // Save the verified code.
  memcpy(local_thunk, &function_code, sizeof(function_code));

  return true;
}

NTSTATUS ServiceResolverThunk::PerformPatch(void* local_thunk,
                                            void* remote_thunk) {
  ServiceFullThunk* full_local_thunk = reinterpret_cast<ServiceFullThunk*>(
                                           local_thunk);
  ServiceFullThunk* full_remote_thunk = reinterpret_cast<ServiceFullThunk*>(
                                            remote_thunk);

  // Patch the original code.
  ServiceEntry local_service;
  DCHECK_GE(GetInternalThunkSize(), sizeof(local_service));
  if (!SetInternalThunk(&local_service, sizeof(local_service), NULL,
                        interceptor_))
    return STATUS_UNSUCCESSFUL;

  // Copy the local thunk buffer to the child.
  SIZE_T actual;
  if (!::WriteProcessMemory(process_, remote_thunk, local_thunk,
                            sizeof(ServiceFullThunk), &actual))
    return STATUS_UNSUCCESSFUL;

  if (sizeof(ServiceFullThunk) != actual)
    return STATUS_UNSUCCESSFUL;

  // And now change the function to intercept, on the child.
  if (NULL != ntdll_base_) {
    // Running a unit test.
    if (!::WriteProcessMemory(process_, target_, &local_service,
                              sizeof(local_service), &actual))
      return STATUS_UNSUCCESSFUL;
  } else {
    if (!WriteProtectedChildMemory(process_, target_, &local_service,
                                   sizeof(local_service)))
      return STATUS_UNSUCCESSFUL;
  }

  return STATUS_SUCCESS;
}

bool Wow64ResolverThunk::IsFunctionAService(void* local_thunk) const {
  NOTREACHED();
  return false;
}

bool Win2kResolverThunk::IsFunctionAService(void* local_thunk) const {
  NOTREACHED();
  return false;
}

}  // namespace sandbox
