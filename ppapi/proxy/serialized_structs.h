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

struct PPAPI_PROXY_EXPORT PPBFlash_DrawGlyphs_Params {
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
