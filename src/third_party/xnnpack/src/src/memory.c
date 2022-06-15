// Copyright 2021 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

// Include first for the platform detection macros.
#include "xnnpack/common.h"

#if XNN_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <stddef.h>
#include <stdint.h>
#include <xnnpack.h>

#include "xnnpack/allocator.h"
#include "xnnpack/log.h"
#include "xnnpack/math.h"
#include "xnnpack/params.h"

// Helpers to allocate/mmap and release memory used by both code and weights cache.

// Maps `size` bytes of memory, returns pointer to allocation, NULL if failed.
static void* allocate_buffer(size_t size) {
  xnn_log_debug("allocating buffer of size %zu", size);
  assert(size == round_up_po2(size, xnn_params.page_size));
#if XNN_PLATFORM_WINDOWS
  void* p = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (p == NULL) {
    xnn_log_error("failed to allocate %zu bytes for code/weights buffer, error code: %" PRIu32,
                  size, (uint32_t) GetLastError());
    return NULL;
  }
#else
  void* p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) {
    xnn_log_error("failed to allocate %zu bytes for code/weights buffer, error code: %d", size, errno);
    return NULL;
  }
#endif
  return p;
}

// Releases memory previously mapped by `allocate_buffer`, returns xnn_status_success on success.
static enum xnn_status release_memory(void* start, size_t capacity) {
#if XNN_PLATFORM_WINDOWS
  // We only decommited any unused capacity, so we release all of it now.
  if (!VirtualFree(start, 0, MEM_RELEASE)) {
    xnn_log_error("failed to release code/weights buffer, error code: %" PRIu32, (uint32_t) GetLastError());
    return xnn_status_invalid_state;
  }
#else
  if (munmap(start, capacity) == -1) {
    xnn_log_error("failed to release code/weights buffer, error code: %d", errno);
    return xnn_status_invalid_state;
  }
#endif
  return xnn_status_success;
}

enum xnn_status xnn_allocate_code_memory(struct xnn_code_buffer* buf, size_t size) {
  memset(buf, 0, sizeof(struct xnn_code_buffer));
  size_t page_aligned_size = round_up_po2(size, xnn_params.page_size);
  buf->start = allocate_buffer(page_aligned_size);
  if (buf->start == NULL) {
    return xnn_status_out_of_memory;
  }

  buf->size = 0;
  buf->capacity = page_aligned_size;
  return xnn_status_success;
}

// Releases unused memory. Will write the new capacity to `capacity`.
static enum xnn_status release_unused_memory(size_t size, void* start, size_t* capacity) {
  // Release all unused pages.
  const size_t page_aligned_size = round_up_po2(size, xnn_params.page_size);
  const uint8_t* mem_start = (uint8_t*) start;
  const uint8_t* unused_start = mem_start + page_aligned_size;
  assert(*capacity >= page_aligned_size);
  const size_t unused_capacity = *capacity - page_aligned_size;

  xnn_log_debug("releasing memory, start %p, used: %zu, capacity: %zu, unused %zu", mem_start, size, *capacity,
                unused_capacity);

  if (unused_capacity != 0) {
    // Free unused pages.
    #if XNN_PLATFORM_WINDOWS
      // We cannot selectively release pages inside the region of pages, so just decommit them.
      if (!VirtualFree((void*) unused_start, unused_capacity, MEM_DECOMMIT)) {
        xnn_log_error("failed to unmap code/weights buffer, error code: %" PRIu32, (uint32_t) GetLastError());
        return xnn_status_invalid_state;
      }
      *capacity = page_aligned_size;
    #elif !XNN_PLATFORM_WEB
      // Web does not support partial unmapping.
      if (munmap((void*) unused_start, unused_capacity) == -1) {
        xnn_log_error("failed to unmap code/weights buffer, error code: %d", errno);
        return xnn_status_invalid_state;
      }
      *capacity = page_aligned_size;
    #else
      if (unused_capacity == *capacity) {
        if (munmap((void*) unused_start, unused_capacity) == -1) {
          xnn_log_error("failed to unmap code/weights buffer, error code: %d", errno);
          return xnn_status_invalid_state;
        } else {
          *capacity = 0;
        }
      }
    #endif
  }

  return xnn_status_success;
}

enum xnn_memory_permission {
  xnn_memory_permission_read_only,
  xnn_memory_permission_read_execute,
};

static enum xnn_status set_memory_permission(void* start, size_t size, enum xnn_memory_permission permission) {
  #if XNN_PLATFORM_WINDOWS
    DWORD old = 0, prot = 0;
    switch (permission) {
      case xnn_memory_permission_read_only:
        prot = PAGE_READONLY;
        break;
      case xnn_memory_permission_read_execute:
        prot = PAGE_EXECUTE_READ;
        break;
      default:
        XNN_UNREACHABLE;
    }
    if (!VirtualProtect(start, size, prot, &old)) {
      xnn_log_error(
        "failed to set memory permission (%d), error code: %" PRIu32, permission, (uint32_t) GetLastError());
      return xnn_status_invalid_state;
    }
  #elif XNN_PLATFORM_WEB
    // Memory protection not supported on Web.
    return xnn_status_success;
  #else
    int prot = 0;
    switch (permission) {
      case xnn_memory_permission_read_only:
        prot = PROT_READ;
        break;
      case xnn_memory_permission_read_execute:
        prot = PROT_READ | PROT_EXEC;
        break;
      default:
        XNN_UNREACHABLE;
    }
    if (mprotect(start, size, prot) == -1) {
      xnn_log_error("failed to set memory permission (%d), error code: %d", permission, errno);
      return xnn_status_invalid_state;
    }
  #endif
  return xnn_status_success;
}

#if XNN_PLATFORM_JIT
enum xnn_status xnn_finalize_code_memory(struct xnn_code_buffer* buf) {
  enum xnn_status status;
  status = release_unused_memory(buf->size, buf->start, &buf->capacity);
  if (status != xnn_status_success) {
    return status;
  }

  if (buf->capacity == 0) {
    return xnn_status_success;
  }

  // Flush icache, do it before changing permissions due to bugs on older ARM64 kernels.
  #if (XNN_ARCH_ARM || XNN_ARCH_ARM64) && XNN_PLATFORM_JIT
    // iOS toolchain doesn't support this, use sys_icache_invalidate, when we support iOS.
    __builtin___clear_cache(buf->start, (void*) ((uint8_t*) buf->start + buf->capacity));
  #endif  // (XNN_ARCH_ARM || XNN_ARCH_ARM64) && !XNN_PLATFORM_IOS

  // Set permissions to RX (no write).
  #if XNN_PLATFORM_WINDOWS
    DWORD old = 0;
    if (!VirtualProtect(buf->start, buf->size, PAGE_EXECUTE_READ, &old)) {
      xnn_log_error("failed to make code buffer read+execute, error code: %" PRIu32, (uint32_t) GetLastError());
      return xnn_status_invalid_state;
    }
  #else
    if (mprotect(buf->start, buf->size, PROT_READ | PROT_EXEC) == -1) {
      xnn_log_error("failed to make code buffer read+execute, error code: %d", errno);
      return xnn_status_invalid_state;
    }
  #endif
  return set_memory_permission(buf->start, buf->size, xnn_memory_permission_read_execute);
}
#endif  // XNN_PLATFORM_JIT

enum xnn_status xnn_release_code_memory(struct xnn_code_buffer* buf) {
  if (buf->capacity == 0) {
    return xnn_status_success;
  }
  const enum xnn_status status = release_memory(buf->start, buf->capacity);
  if (status != xnn_status_success) {
    return status;
  }
  memset(buf, 0, sizeof(struct xnn_code_buffer));
  return xnn_status_success;
}

enum xnn_status xnn_reserve_code_memory(struct xnn_code_buffer* buf, size_t n) {
  if (buf->size + n <= buf->capacity) {
    return xnn_status_success;
  }
  xnn_log_debug("reserving code memory of size %zu", n);

  // TODO(zhin): use mremap
  size_t size = buf->size;
  struct xnn_code_buffer new_code_buffer;
  enum xnn_status status = xnn_allocate_code_memory(&new_code_buffer, buf->size + n);
  if (status != xnn_status_success) {
    return status;
  }
  memcpy(new_code_buffer.start, buf->start, size);
  new_code_buffer.size = size;

  // Release old code_buffer.
  status = xnn_release_code_memory(buf);
  if (status != xnn_status_success) {
    return status;
  }
  // Copy over all the new code_buffer information.
  memcpy(buf, &new_code_buffer, sizeof(struct xnn_code_buffer));
  return xnn_status_success;
}

enum xnn_status xnn_allocate_weights_memory(struct xnn_weights_buffer* buf, size_t size) {
  memset(buf, 0, sizeof(struct xnn_weights_buffer));
  size_t page_aligned_size = round_up_po2(size, xnn_params.page_size);
  buf->start = allocate_buffer(page_aligned_size);
  if (buf->start == NULL) {
    return xnn_status_out_of_memory;
  }

  buf->size = 0;
  buf->capacity = page_aligned_size;
  return xnn_status_success;
}

enum xnn_status xnn_release_weights_memory(struct xnn_weights_buffer* buf) {
  if (buf->capacity == 0) {
    return xnn_status_success;
  }
  enum xnn_status status = release_memory(buf->start, buf->capacity);
  if (status != xnn_status_success) {
    return status;
  }
  memset(buf, 0, sizeof(struct xnn_code_buffer));
  return xnn_status_success;
}

enum xnn_status xnn_reserve_weights_memory(struct xnn_weights_buffer* buf, size_t n) {
  if (buf->size + n <= buf->capacity) {
    return xnn_status_success;
  }

  xnn_log_debug("reserving weights memory of size %zu", n);
  // TODO(zhin): use mremap
  size_t size = buf->size;
  struct xnn_weights_buffer new_weights_buffer;
  enum xnn_status status = xnn_allocate_weights_memory(&new_weights_buffer, buf->size + n);
  if (status != xnn_status_success) {
    return status;
  }
  memcpy(new_weights_buffer.start, buf->start, size);
  new_weights_buffer.size = size;

  // Release old weights_buffer.
  status = xnn_release_weights_memory(buf);
  if (status != xnn_status_success) {
    return status;
  }
  // Copy over all the new weights_buffer information.
  memcpy(buf, &new_weights_buffer, sizeof(struct xnn_weights_buffer));
  return xnn_status_success;
}

enum xnn_status xnn_finalize_weights_memory(struct xnn_weights_buffer* buf) {
  enum xnn_status status;
  status = release_unused_memory(buf->size, buf->start, &buf->capacity);
  if (status != xnn_status_success) {
    return status;
  }

  if (buf->capacity == 0) {
    return xnn_status_success;
  }

  return set_memory_permission(buf->start, buf->size, xnn_memory_permission_read_only);
}
