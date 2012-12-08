// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_SERIALIZED_STRUCTS_H_
#define PPAPI_PROXY_SERIALIZED_STRUCTS_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/host_resource.h"

class Pickle;
struct PP_FontDescription_Dev;
struct PP_BrowserFont_Trusted_Description;

namespace ppapi {
namespace proxy {

// PP_FontDescription_Dev/PP_BrowserFontDescription (same definition, different
// names) has to be redefined with a string in place of the PP_Var used for the
// face name.
struct PPAPI_PROXY_EXPORT SerializedFontDescription {
  SerializedFontDescription();
  ~SerializedFontDescription();

  // Converts a PP_FontDescription_Dev to a SerializedFontDescription.
  //
  // The reference of |face| owned by the PP_FontDescription_Dev will be
  // unchanged and the caller is responsible for freeing it.
  void SetFromPPFontDescription(const PP_FontDescription_Dev& desc);
  void SetFromPPBrowserFontDescription(
      const PP_BrowserFont_Trusted_Description& desc);

  // Converts to a PP_FontDescription_Dev. The face name will have one ref
  // assigned to it. The caller is responsible for freeing it.
  void SetToPPFontDescription(PP_FontDescription_Dev* desc) const;
  void SetToPPBrowserFontDescription(
      PP_BrowserFont_Trusted_Description* desc) const;

  std::string face;
  int32_t family;
  uint32_t size;
  int32_t weight;
  PP_Bool italic;
  PP_Bool small_caps;
  int32_t letter_spacing;
  int32_t word_spacing;
};

struct SerializedDirEntry {
  std::string name;
  bool is_dir;
};

struct PPBFlash_DrawGlyphs_Params {
  PPBFlash_DrawGlyphs_Params();
  ~PPBFlash_DrawGlyphs_Params();

  PP_Instance instance;
  ppapi::HostResource image_data;
  SerializedFontDescription font_desc;
  uint32_t color;
  PP_Point position;
  PP_Rect clip;
  float transformation[3][3];
  PP_Bool allow_subpixel_aa;
  std::vector<uint16_t> glyph_indices;
  std::vector<PP_Point> glyph_advances;
};

struct PPBURLLoader_UpdateProgress_Params {
  PP_Instance instance;
  ppapi::HostResource resource;
  int64_t bytes_sent;
  int64_t total_bytes_to_be_sent;
  int64_t bytes_received;
  int64_t total_bytes_to_be_received;
};

// We put all our handles in a unified structure to make it easy to translate
// them in NaClIPCAdapter for use in NaCl.
class PPAPI_PROXY_EXPORT SerializedHandle {
 public:
  enum Type { INVALID, SHARED_MEMORY, SOCKET, CHANNEL_HANDLE, FILE };
  struct Header {
    Header() : type(INVALID), size(0) {}
    Header(Type type_arg, uint32_t size_arg)
        : type(type_arg), size(size_arg) {
    }
    Type type;
    uint32_t size;
  };

  SerializedHandle();
  // Create an invalid handle of the given type.
  explicit SerializedHandle(Type type);

  // Create a shared memory handle.
  SerializedHandle(const base::SharedMemoryHandle& handle, uint32_t size);

  // Create a socket, channel or file handle.
  SerializedHandle(const Type type,
                   const IPC::PlatformFileForTransit& descriptor);

  Type type() const { return type_; }
  bool is_shmem() const { return type_ == SHARED_MEMORY; }
  bool is_socket() const { return type_ == SOCKET; }
  bool is_channel_handle() const { return type_ == CHANNEL_HANDLE; }
  bool is_file() const { return type_ == FILE; }
  const base::SharedMemoryHandle& shmem() const {
    DCHECK(is_shmem());
    return shm_handle_;
  }
  uint32_t size() const {
    DCHECK(is_shmem());
    return size_;
  }
  const IPC::PlatformFileForTransit& descriptor() const {
    DCHECK(is_socket() || is_channel_handle() || is_file());
    return descriptor_;
  }
  void set_shmem(const base::SharedMemoryHandle& handle, uint32_t size) {
    type_ = SHARED_MEMORY;
    shm_handle_ = handle;
    size_ = size;

    descriptor_ = IPC::InvalidPlatformFileForTransit();
  }
  void set_socket(const IPC::PlatformFileForTransit& socket) {
    type_ = SOCKET;
    descriptor_ = socket;

    shm_handle_ = base::SharedMemory::NULLHandle();
    size_ = 0;
  }
  void set_channel_handle(const IPC::PlatformFileForTransit& descriptor) {
    type_ = CHANNEL_HANDLE;

    descriptor_ = descriptor;
    shm_handle_ = base::SharedMemory::NULLHandle();
    size_ = 0;
  }
  void set_file_handle(const IPC::PlatformFileForTransit& descriptor) {
    type_ = FILE;

    descriptor_ = descriptor;
    shm_handle_ = base::SharedMemory::NULLHandle();
    size_ = 0;
  }
  void set_null_shmem() {
    set_shmem(base::SharedMemory::NULLHandle(), 0);
  }
  void set_null_socket() {
    set_socket(IPC::InvalidPlatformFileForTransit());
  }
  void set_null_channel_handle() {
    set_channel_handle(IPC::InvalidPlatformFileForTransit());
  }
  void set_null_file_handle() {
    set_file_handle(IPC::InvalidPlatformFileForTransit());
  }
  bool IsHandleValid() const;

  Header header() const {
    return Header(type_, size_);
  }

  // Closes the handle and sets it to invalid.
  void Close();

  // Write/Read a Header, which contains all the data except the handle. This
  // allows us to write the handle in a platform-specific way, as is necessary
  // in NaClIPCAdapter to share handles with NaCl from Windows.
  static bool WriteHeader(const Header& hdr, Pickle* pickle);
  static bool ReadHeader(PickleIterator* iter, Header* hdr);

 private:
  // The kind of handle we're holding.
  Type type_;

  // We hold more members than we really need; we can't easily use a union,
  // because we hold non-POD types. But these types are pretty light-weight. If
  // we add more complex things later, we should come up with a more memory-
  // efficient strategy.
  // These are valid if type == SHARED_MEMORY.
  base::SharedMemoryHandle shm_handle_;
  uint32_t size_;

  // This is valid if type == SOCKET || type == CHANNEL_HANDLE.
  IPC::PlatformFileForTransit descriptor_;
};

struct PPPDecryptor_Buffer {
  ppapi::HostResource resource;
  uint32_t size;
  base::SharedMemoryHandle handle;
};

#if defined(OS_WIN)
typedef HANDLE ImageHandle;
#elif defined(OS_MACOSX) || defined(OS_ANDROID)
typedef base::SharedMemoryHandle ImageHandle;
#else
// On X Windows this is a SysV shared memory key.
typedef int ImageHandle;
#endif

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_SERIALIZED_STRUCTS_H_
