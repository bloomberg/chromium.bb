/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <errno.h>
#include <signal.h>
#include <string.h>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_platform.h"

#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"

#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/shared_memory.h"
#include "native_client/src/trusted/plugin/utility.h"

#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/internal_errno.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"


namespace {

bool RpcRead(void* obj, plugin::SrpcParams* params) {
  plugin::SharedMemory* shared_memory =
      reinterpret_cast<plugin::SharedMemory*>(obj);
  uint32_t offset;
  uint32_t len;

  // TODO(gregoryd) - the old code was able to handle both ints and doubles
  // it should be handled in the marshalling code,
  // otherwise the call will be rejected
  if (params->ins()[0]->tag == NACL_SRPC_ARG_TYPE_DOUBLE) {
    offset = static_cast<uint32_t>(params->ins()[0]->u.dval);
  } else {
    offset = static_cast<uint32_t>(params->ins()[0]->u.ival);
  }

  if (params->ins()[1]->tag == NACL_SRPC_ARG_TYPE_DOUBLE) {
    len = static_cast<uint32_t>(params->ins()[1]->u.dval);
  } else {
    len = static_cast<uint32_t>(params->ins()[1]->u.ival);
  }

  // Ensure we will access valid addresses.
  if (NULL == shared_memory->shm_addr()) {
    params->set_exception_string("Shared memory not mapped");
    return false;
  }
  if (offset + len < offset) {
    params->set_exception_string("Offset + length overflows");
    return false;
  }
  if (offset + len > shared_memory->shm_size()) {
    params->set_exception_string(
        "Offset + length overlaps end of shared memory");
    return false;
  }

  // TODO(mseaborn): Change this function to use ByteStringAsUTF8().
  // UTF-8 encoding may result in a 2x size increase at the most.
  // TODO(sehr): should we do a pre-scan to get the real size?
  uint32_t utf8_buffer_len = 2 * len;
  if ((utf8_buffer_len < len) ||
      (utf8_buffer_len + 1 < utf8_buffer_len)) {
    // Unsigned overflow.
    params->set_exception_string("len too large to hold requested UTF8 chars");
    return false;
  }

  char* ret_string = reinterpret_cast<char*>(malloc(utf8_buffer_len + 1));
  if (NULL == ret_string) {
    params->set_exception_string("out of memory");
    return false;
  }

  unsigned char* shm_addr =
    reinterpret_cast<unsigned char*>(shared_memory->shm_addr()) + offset;
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

  params->outs()[0]->tag = NACL_SRPC_ARG_TYPE_STRING;
  params->outs()[0]->u.sval.str = ret_string;

  return true;
}

bool RpcWrite(void* obj, plugin::SrpcParams* params) {
  plugin::SharedMemory* shared_memory =
      reinterpret_cast<plugin::SharedMemory*>(obj);
  uint32_t offset;
  uint32_t len;

  // TODO(gregoryd) - the old code was able to handle both ints and doubles
  // it should be handled in the marshalling code,
  // otherwise the call will be rejected
  if (params->ins()[0]->tag == NACL_SRPC_ARG_TYPE_DOUBLE) {
    offset = static_cast<uint32_t>(params->ins()[0]->u.dval);
  } else {
    offset = static_cast<uint32_t>(params->ins()[0]->u.ival);
  }

  if (params->ins()[1]->tag == NACL_SRPC_ARG_TYPE_DOUBLE) {
    len = static_cast<uint32_t>(params->ins()[1]->u.dval);
  } else {
    len = static_cast<uint32_t>(params->ins()[1]->u.ival);
  }

  // Ensure we will access valid addresses.
  if (NULL == shared_memory->shm_addr()) {
    params->set_exception_string("Shared memory not mapped");
    return false;
  }
  if (offset + len < offset) {
    params->set_exception_string("Offset + length overflows");
    return false;
  }
  if (offset + len > shared_memory->shm_size()) {
    params->set_exception_string(
        "Offset + length overlaps end of shared memory");
    return false;
  }

  // The input is a JavaScript string, which must consist of UFT-8
  // characters with character codes between 0 and 255 inclusive.
  NaClSrpcArg* str_param = params->ins()[2];
  const unsigned char* str =
    reinterpret_cast<unsigned const char*>(str_param->u.sval.str);
  uint32_t utf_bytes =
      nacl::saturate_cast<uint32_t>(strlen(str_param->u.sval.str));
  unsigned char* shm_addr =
    reinterpret_cast<unsigned char*>(shared_memory->shm_addr()) + offset;

  // TODO(mseaborn): Change this function to use ByteStringFromUTF8().
  for (unsigned int i = 0; i < len;) {
    unsigned char c1 = str[0];
    unsigned char c2 = 0;

    // Check that we are still pointing into the JavaScript string we were
    // passed.
    if (i >= utf_bytes) {
      return false;
    }
    // Process the byte in the string as UTF-8 characters.
    if (c1 & 0x80) {
      // str[1] will not access out of bounds because sval.str is a
      // NUL-terminated sequence of bytes.  However, NUL would fail the content
      // test just below, so failing here seems a good thing anyway.
      if (i == len - 1) {
        return false;
      }
      c2 = str[1];
      // Assert two byte encoding.
      // The first character must contain 110xxxxxb and the
      // second must contain 10xxxxxxb.
      if (((c1 & 0xc3) != c1) || ((c2 & 0xbf) != c2)) {
        params->set_exception_string("Bad utf8 character value");
        return false;
      }
      *shm_addr = (c1 << 6) | (c2 & 0x3f);
      str += 2;
      i += 2;
    } else {
      // One-byte encoding.
      *shm_addr = c1;
      ++str;
      ++i;
    }
    ++shm_addr;
  }
  return true;
}

}  // namespace

namespace plugin {

void SharedMemory::LoadMethods() {
  AddMethodCall(RpcRead, "read", "ii", "s");
  AddMethodCall(RpcWrite, "write", "iis", "");
}

bool SharedMemory::Init(Plugin* plugin,
                        nacl::DescWrapper* wrapper,
                        off_t length) {
  bool allocated_memory = false;

  if (NULL == wrapper) {
    // Creating a new object by size
    size_t size = static_cast<size_t>(length);
    // The Map virtual function rounds up to the nearest AllocPage.
    size_t rounded_size = NaClRoundAllocPage(size);
    wrapper = plugin->wrapper_factory()->MakeShm(rounded_size);
    if (NULL == wrapper) {
      return false;
    }

    PLUGIN_PRINTF(("SharedMemory::Init(%p, 0x%08x)\n",
                   static_cast<void*>(plugin),
                   static_cast<unsigned>(length)));
    // Now allocate the object through the canonical factory and return.
    allocated_memory = true;
  }

  if (!DescBasedHandle::Init(plugin, wrapper)) {
    if (allocated_memory) {
      // TODO(sehr,mseaborn): use scoped_ptr for management of DescWrappers.
      delete wrapper;
    }
    return false;
  }

  if (0 > wrapper->Map(&addr_, &size_)) {
    // BUG: we are leaking the shared memory object here.
    return false;
  }

  LoadMethods();
  return true;
}

SharedMemory* SharedMemory::New(Plugin* plugin, nacl::DescWrapper* wrapper) {
  PLUGIN_PRINTF(("SharedMemory::New()\n"));

  SharedMemory* shared_memory = new(std::nothrow) SharedMemory();
  if (shared_memory == NULL || !shared_memory->Init(plugin, wrapper, 0)) {
    return NULL;
  }
  return shared_memory;
}

SharedMemory* SharedMemory::New(Plugin* plugin, off_t length) {
  PLUGIN_PRINTF(("SharedMemory::New()\n"));

  SharedMemory* shared_memory = new(std::nothrow) SharedMemory();
  if (shared_memory == NULL || !shared_memory->Init(plugin, NULL, length)) {
    return NULL;
  }
  return shared_memory;
}

SharedMemory::SharedMemory() : handle_(0),
                               addr_(NULL),
                               size_(0) {
  PLUGIN_PRINTF(("SharedMemory::SharedMemory(%p)\n",
                 static_cast<void*>(this)));
}

SharedMemory::~SharedMemory() {
  PLUGIN_PRINTF(("SharedMemory::~SharedMemory(%p)\n",
                 static_cast<void*>(this)));

  // Invalidates are called by Firefox in abitrary order.  Hence, the plugin
  // could have been freed/trashed before we get invalidated.  Therefore, we
  // cannot use the effp_ member of the plugin object.
  // TODO(sehr): fix the resulting address space leak.
  // Free the memory that was mapped to the descriptor.
  // shared_memory->desc_->vtbl->Unmap(shared_memory->desc_,
  //                                   shared_memory->plugin_->effp_,
  //                                   shared_memory->addr_,
  //                                   shared_memory->size_);
  // After invalidation, the browser does not respect reference counting,
  // so we shut down here what we can and prevent attempts to shut down
  // other linked structures in Deallocate.

  // Free the memory that was mapped to the descriptor.
  // if (desc() && plugin_) {
  //   desc()->vtbl->Unmap(desc(),
  //     plugin_->effp_,
  //     addr_,
  //     size_);
  // }
  // TODO(sehr): is there a missing delete desc() here?
  // TODO(gregoryd): in addition, should we unref the descriptor if it was
  // constructed during the initialization of this object?
}

}  // namespace plugin
