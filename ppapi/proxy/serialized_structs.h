// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_SERIALIZED_STRUCTS_H_
#define PPAPI_PROXY_SERIALIZED_STRUCTS_H_

#include <string>
#include <vector>

#include "base/shared_memory.h"
#include "build/build_config.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/serialized_var.h"

struct PP_FontDescription_Dev;

namespace pp {
namespace proxy {

class Dispatcher;

// PP_FontDescript_Dev has to be redefined with a SerializedVar in place of
// the PP_Var used for the face name.
struct SerializedFontDescription {
  SerializedFontDescription();
  ~SerializedFontDescription();

  // Converts a PP_FontDescription_Dev to a SerializedFontDescription.
  //
  // If source_owns_ref is true, the reference owned by the
  // PP_FontDescription_Dev will be unchanged and the caller is responsible for
  // freeing it. When false, the SerializedFontDescription will take ownership
  // of the ref. This is the difference between serializing as an input value
  // (owns_ref = true) and an output value (owns_ref = true).
  void SetFromPPFontDescription(Dispatcher* dispatcher,
                                const PP_FontDescription_Dev& desc,
                                bool source_owns_ref);

  // Converts to a PP_FontDescription_Dev. The face name will have one ref
  // assigned to it on behalf of the caller.
  //
  // If dest_owns_ref is set, the resulting PP_FontDescription_Dev will keep a
  // reference to any strings we made on its behalf even when the
  // SerializedFontDescription goes away. When false, ownership of the ref will
  // stay with the SerializedFontDescription and the PP_FontDescription_Dev
  // will just refer to that one. This is the difference between deserializing
  // as an input value (owns_ref = false) and an output value (owns_ref = true).
  void SetToPPFontDescription(Dispatcher* dispatcher,
                              PP_FontDescription_Dev* desc,
                              bool dest_owns_ref) const;

  pp::proxy::SerializedVar face;
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

// FileRefs are created in a number of places and they include a number of
// return values. This struct encapsulates everything in one place.
struct PPBFileRef_CreateInfo {
  PPBFileRef_CreateInfo();  // Initializes to 0.

  HostResource resource;
  int file_system_type;  // One of PP_FileSystemType_Dev values.
  SerializedVar path;
  SerializedVar name;
};

// Since there are so many parameters, DrawTextAt requires this separate
// structure. This includes everything but the font name. Because the font name
// is a var, it's much more convenient to use the normal way of passing a
// PP_Var.
struct PPBFont_DrawTextAt_Params {
  HostResource font;
  HostResource image_data;
  PP_Bool text_is_rtl;
  PP_Bool override_direction;
  PP_Point position;
  uint32_t color;
  PP_Rect clip;
  bool clip_is_null;
  PP_Bool image_data_is_opaque;
};

struct PPBFlash_DrawGlyphs_Params {
  PPBFlash_DrawGlyphs_Params();
  ~PPBFlash_DrawGlyphs_Params();

  PP_Instance instance;
  HostResource image_data;
  SerializedFontDescription font_desc;
  uint32_t color;
  PP_Point position;
  PP_Rect clip;
  float transformation[3][3];
  std::vector<uint16_t> glyph_indices;
  std::vector<PP_Point> glyph_advances;
};

struct PPBAudio_NotifyAudioStreamCreated_Params {
  pp::proxy::HostResource audio_id;
  int32_t result_code;  // Will be != PP_OK on failure
  IPC::PlatformFileForTransit socket_handle;
  base::SharedMemoryHandle handle;
  int32_t length;
};

struct PPBURLLoader_UpdateProgress_Params {
  PP_Instance instance;
  pp::proxy::HostResource resource;
  int64_t bytes_sent;
  int64_t total_bytes_to_be_sent;
  int64_t bytes_received;
  int64_t total_bytes_to_be_received;
};

#if defined(OS_WIN)
typedef HANDLE ImageHandle;
#elif defined(OS_MACOSX)
typedef base::SharedMemoryHandle ImageHandle;
#else
// On X Windows this is a SysV shared memory key.
typedef int ImageHandle;
#endif

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_SERIALIZED_STRUCTS_H_
