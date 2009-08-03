/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <signal.h>
#include <string.h>

#include "native_client/src/include/nacl_platform.h"

#include "native_client/src/trusted/platform/nacl_host_desc.h"

#include "native_client/src/trusted/plugin/srpc/plugin.h"
#include "native_client/src/trusted/plugin/srpc/shared_memory.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/internal_errno.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"


namespace nacl_srpc {

int SharedMemory::number_alive_counter = 0;

bool SharedMemory::RpcRead(void *obj, SrpcParams *params) {
  SharedMemory* shared_memory = reinterpret_cast<SharedMemory*>(obj);
  uint32_t offset;
  uint32_t len;

  // TODO(gregoryd) - the old code was able to handle both ints and doubles
  // it should be handled in the marshalling code,
  // otherwise the call will be rejected
  if (params->Input(0)->tag == NACL_SRPC_ARG_TYPE_DOUBLE) {
    offset = static_cast<uint32_t>(params->Input(0)->u.dval);
  } else {
    offset = static_cast<uint32_t>(params->Input(0)->u.ival);
  }

  if (params->Input(1)->tag == NACL_SRPC_ARG_TYPE_DOUBLE) {
    len = static_cast<uint32_t>(params->Input(1)->u.dval);
  } else {
    len = static_cast<uint32_t>(params->Input(1)->u.ival);
  }

  // Ensure we will access valid addresses.
  if (NULL == shared_memory->map_addr_) {
    params->SetExceptionInfo("Shared memory not mapped");
    return false;
  }
  if (offset + len < offset) {
    params->SetExceptionInfo("Offset + length overflows");
    return false;
  }
  if (offset + len > shared_memory->size_) {
    params->SetExceptionInfo("Offset + length overlaps end of shared memory");
    return false;
  }


  // UTF-8 encoding may result in a 2x size increase at the most.
  // TODO(sehr): should we do a pre-scan to get the real size?
  uint32_t utf8_buffer_len = 2 * len;
  if ((utf8_buffer_len < len) ||
      (utf8_buffer_len + 1 < utf8_buffer_len)) {
    // Unsigned overflow.
    params->SetExceptionInfo("len too large to hold requested UTF8 chars");
    return false;
  }

  char* ret_string = reinterpret_cast<char*>(malloc(utf8_buffer_len + 1));
  if (NULL == ret_string) {
    params->SetExceptionInfo("out of memory");
    return false;
  }

  unsigned char* shm_addr =
    reinterpret_cast<unsigned char*>(shared_memory->map_addr_) + offset;
  unsigned char* out = reinterpret_cast<unsigned char*>(ret_string);
  // NPAPI wants length to be the number of bytes, not UTF-8 characters.
  for (unsigned int i = 0; i < len; ++i) {
    unsigned char c = *shm_addr;
    if (c < 128) {
      // Code results in a one byte encoding
      *out = c;
      ++out;
    } else {
      // Code results in a two byte encoding
      out[0] = 0xc0 | (c >> 6);
      out[1] = 0x80 | (c & 0x3f);
      out += 2;
    }
    ++shm_addr;
  }
  // Terminate the result string with 0.
  *out = 0;

  params->Output(0)->tag = NACL_SRPC_ARG_TYPE_STRING;
  params->Output(0)->u.sval = ret_string;

  return true;
}

bool SharedMemory::RpcWrite(void *obj, SrpcParams *params) {
  SharedMemory* shared_memory = reinterpret_cast<SharedMemory*>(obj);
  uint32_t offset;
  uint32_t len;

  // TODO(gregoryd) - the old code was able to handle both ints and doubles
  // it should be handled in the marshalling code,
  // otherwise the call will be rejected
  if (params->Input(0)->tag == NACL_SRPC_ARG_TYPE_DOUBLE) {
    offset = static_cast<uint32_t>(params->Input(0)->u.dval);
  } else {
    offset = static_cast<uint32_t>(params->Input(0)->u.ival);
  }

  if (params->Input(1)->tag == NACL_SRPC_ARG_TYPE_DOUBLE) {
    len = static_cast<uint32_t>(params->Input(1)->u.dval);
  } else {
    len = static_cast<uint32_t>(params->Input(1)->u.ival);
  }

  // Ensure we will access valid addresses.
  if (NULL == shared_memory->map_addr_) {
    params->SetExceptionInfo("Shared memory not mapped");
    return false;
  }
  if (offset + len < offset) {
    params->SetExceptionInfo("Offset + length overflows");
    return false;
  }
  if (offset + len > shared_memory->size_) {
    params->SetExceptionInfo("Offset + length overlaps end of shared memory");
    return false;
  }

  // The input is a JavaScript string, which must consist of UFT-8
  // characters with character codes between 0 and 255 inclusive.
  NaClSrpcArg *str_param = params->Input(2);
  const unsigned char* str =
    reinterpret_cast<unsigned const char*>(str_param->u.sval);
  const unsigned int utf_bytes = strlen(str_param->u.sval);
  unsigned char* shm_addr =
    reinterpret_cast<unsigned char*>(shared_memory->map_addr_) + offset;

  for (unsigned int i = 0; i < len; ++i) {
    unsigned char c1 = str[0];
    unsigned char c2 = 0;

    // Check that we are still pointing into the JavaScript string we were
    // passed.
    if (i >= utf_bytes) {
      return false;
    }
    // Process the byte in the string as UTF-8 characters.
    if (c1 & 0x80) {
      c2 = str[1];
      // Assert two byte encoding.
      // The first character must contain 110xxxxxb and the
      // second must contain 10xxxxxxb.
      if (((c1 & 0xc3) != c1) || ((c2 & 0xbf) != c2)) {
        params->SetExceptionInfo("Bad utf8 character value");
        return false;
      }
      *shm_addr = (c1 << 6) | (c2 & 0x3f);
      str += 2;
    } else {
      // One-byte encoding.
      *shm_addr = c1;
      ++str;
    }
    ++shm_addr;
  }
  return true;
}

void SharedMemory::LoadMethods() {
  PortableHandle::AddMethodToMap(RpcRead,
    "read", METHOD_CALL, "ii", "s");
  PortableHandle::AddMethodToMap(RpcWrite,
    "write", METHOD_CALL, "iis", "");
}

bool SharedMemory::Init(struct PortableHandleInitializer* init_info) {
  struct SharedMemoryInitializer *shm_init_info =
      static_cast<SharedMemoryInitializer*>(init_info);
  struct nacl_abi_stat st;
  bool allocated_memory = false;

  if (NULL == shm_init_info->desc_) {
    // Creating a new object by size
    size_t size = static_cast<size_t>(shm_init_info->length_);
    // The Map virtual function rounds up to the nearest AllocPage.
    size_t rounded_size = NaClRoundAllocPage(size);
    NaClHandle handle = nacl::CreateMemoryObject(rounded_size);
    if (nacl::kInvalidHandle == handle) {
      return false;
    }

    struct NaClDescImcShm *imc_desc =
      reinterpret_cast<struct NaClDescImcShm*>(malloc(sizeof(*imc_desc)));
    if (NULL == imc_desc) {
      nacl::Close(handle);
      return false;
    }
    struct NaClDesc* desc = reinterpret_cast<struct NaClDesc*>(imc_desc);

    dprintf(("SharedMemory::Init(%p, 0x%08x)\n",
             static_cast<void *>(shm_init_info->plugin_),
             static_cast<unsigned>(shm_init_info->length_)));
    NaClDescImcShmCtor(imc_desc, handle, shm_init_info->length_);
    shm_init_info->desc_ = desc;
    // Now allocate the object through the canonical factory and return.
    allocated_memory = true;
  }

  if (!DescBasedHandle::Init(init_info)) {
    if (allocated_memory) {
      NaClDescUnref(shm_init_info->desc_);
    }
    return false;
  }

  // Set size from stat call.
  int rval = desc()->vtbl->Fstat(desc(), plugin_->effp_, &st);
  if (0 == rval) {
    void* map_addr = NULL;
    size_t size = st.nacl_abi_st_size;
    // When probing by VirtualAlloc/mmap, use the same granularity
    // as the Map virtual function (64KB).
    size_t rounded_size = NaClRoundAllocPage(size);
    // Set the object size.
    size_ = rounded_size;
    dprintf(("SharedMemory::Init: size 0x%08x\n", (unsigned) rounded_size));

    // Find an address range to map the object into.
    const int kMaxTries = 10;
    int tries = 0;
    do {
      ++tries;
#if NACL_WINDOWS
      map_addr = VirtualAlloc(NULL, rounded_size, MEM_RESERVE, PAGE_READWRITE);
      if (NULL == map_addr ||!VirtualFree(map_addr, 0, MEM_RELEASE)) {
        continue;
      }
#else
      map_addr = mmap(NULL,
                      rounded_size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS,
                      0,
                      0);
      dprintf(("SharedMemory::Init: trying addr %p %d\n",
               static_cast<void *>(map_addr), errno));
      if (MAP_FAILED == map_addr || munmap(map_addr, size)) {
        map_addr = NULL;
        continue;
      }
#endif
      dprintf(("SharedMemory::Init: trying addr %p\n",
               static_cast<void *>(map_addr)));
      rval = desc()->vtbl->Map(desc(),
                               plugin_->effp_,
                               map_addr,
                               rounded_size,
                               NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                               NACL_ABI_MAP_SHARED,
                               0);
      dprintf(("SharedMemory::Init: result 0x%08x\n", rval));
      if (!NaClIsNegErrno(rval)) {
        map_addr = reinterpret_cast<void*>(rval);
        break;
      }
    } while (NULL == map_addr && tries < kMaxTries);
    dprintf(("SharedMemory::Init: addr %p\n", static_cast<void *>(map_addr)));
    map_addr_ = map_addr;
  } else {
    dprintf(("SharedMemory::Init: Fstat failed\n"));
    map_addr_ = NULL;
  }

  if (NULL == map_addr_) {
    return false;
  }

  LoadMethods();
  return true;
}

SharedMemory::SharedMemory() : handle_(0),
                               map_addr_(NULL),
                               size_(0) {
  dprintf(("SharedMemory::SharedMemory(%p, %d)\n",
           static_cast<void *>(this),
           ++number_alive_counter));
}

SharedMemory::~SharedMemory() {
  dprintf(("SharedMemory::~SharedMemory(%p, %d)\n",
           static_cast<void *>(this),
           --number_alive_counter));

  // Invalidates are called by Firefox in abitrary order.  Hence, the plugin
  // could have been freed/trashed before we get invalidated.  Therefore, we
  // cannot use the effp_ member of the plugin object.
  // TODO(sehr): fix the resulting address space leak.
  // Free the memory that was mapped to the descriptor.
  // shared_memory->desc_->vtbl->Unmap(shared_memory->desc_,
  //                                   shared_memory->plugin_->effp_,
  //                                   shared_memory->map_addr_,
  //                                   shared_memory->size_);
  // After invalidation, the browser does not respect reference counting,
  // so we shut down here what we can and prevent attempts to shut down
  // other linked structures in Deallocate.

  // Free the memory that was mapped to the descriptor.
  // if (desc() && plugin_) {
  //   desc()->vtbl->Unmap(desc(),
  //     plugin_->effp_,
  //     map_addr_,
  //     size_);
  // }
  // TODO(sehr): is there a missing NaClDescUnref here?
  // TODO(gregoryd): in addition, should we unref the descriptor if it was
  // constructed during the initialization of this object?
}

}  // namespace nacl_srpc
