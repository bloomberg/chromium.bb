// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/serialized_structs.h"

#include "base/pickle.h"
#include "base/platform_file.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/shared_impl/var.h"

#if defined(OS_NACL)
#include <unistd.h>
#endif

namespace ppapi {
namespace proxy {

SerializedFontDescription::SerializedFontDescription()
    : face(),
      family(0),
      size(0),
      weight(0),
      italic(PP_FALSE),
      small_caps(PP_FALSE),
      letter_spacing(0),
      word_spacing(0) {
}

SerializedFontDescription::~SerializedFontDescription() {}

void SerializedFontDescription::SetFromPPFontDescription(
    const PP_FontDescription_Dev& desc) {
  StringVar* string_var = StringVar::FromPPVar(desc.face);
  face = string_var ? string_var->value() : std::string();

  family = desc.family;
  size = desc.size;
  weight = desc.weight;
  italic = desc.italic;
  small_caps = desc.small_caps;
  letter_spacing = desc.letter_spacing;
  word_spacing = desc.word_spacing;
}

void SerializedFontDescription::SetToPPFontDescription(
    PP_FontDescription_Dev* desc) const {
  desc->face = StringVar::StringToPPVar(face);
  desc->family = static_cast<PP_FontFamily_Dev>(family);
  desc->size = size;
  desc->weight = static_cast<PP_FontWeight_Dev>(weight);
  desc->italic = italic;
  desc->small_caps = small_caps;
  desc->letter_spacing = letter_spacing;
  desc->word_spacing = word_spacing;
}

PPBFlash_DrawGlyphs_Params::PPBFlash_DrawGlyphs_Params()
    : instance(0),
      font_desc(),
      color(0) {
  clip.point.x = 0;
  clip.point.y = 0;
  clip.size.height = 0;
  clip.size.width = 0;
  position.x = 0;
  position.y = 0;
  allow_subpixel_aa = PP_FALSE;
}

PPBFlash_DrawGlyphs_Params::~PPBFlash_DrawGlyphs_Params() {}

SerializedHandle::SerializedHandle()
    : type_(INVALID),
      shm_handle_(base::SharedMemory::NULLHandle()),
      size_(0),
      descriptor_(IPC::InvalidPlatformFileForTransit()) {
}

SerializedHandle::SerializedHandle(Type type_param)
    : type_(type_param),
      shm_handle_(base::SharedMemory::NULLHandle()),
      size_(0),
      descriptor_(IPC::InvalidPlatformFileForTransit()) {
}

SerializedHandle::SerializedHandle(const base::SharedMemoryHandle& handle,
                                   uint32_t size)
    : type_(SHARED_MEMORY),
      shm_handle_(handle),
      size_(size),
      descriptor_(IPC::InvalidPlatformFileForTransit()) {
}

SerializedHandle::SerializedHandle(
    Type type,
    const IPC::PlatformFileForTransit& socket_descriptor)
    : type_(type),
      shm_handle_(base::SharedMemory::NULLHandle()),
      size_(0),
      descriptor_(socket_descriptor) {
}

bool SerializedHandle::IsHandleValid() const {
  if (type_ == SHARED_MEMORY)
    return base::SharedMemory::IsHandleValid(shm_handle_);
  else if (type_ == SOCKET || type_ == CHANNEL_HANDLE)
    return !(IPC::InvalidPlatformFileForTransit() == descriptor_);
  return false;
}

void SerializedHandle::Close() {
  if (IsHandleValid()) {
    switch (type_) {
      case INVALID:
        NOTREACHED();
        break;
      case SHARED_MEMORY:
        base::SharedMemory::CloseHandle(shm_handle_);
        break;
      case SOCKET:
      case CHANNEL_HANDLE:
        base::PlatformFile file =
            IPC::PlatformFileForTransitToPlatformFile(descriptor_);
#if !defined(OS_NACL)
        base::ClosePlatformFile(file);
#else
        close(file);
#endif
        break;
      // No default so the compiler will warn us if a new type is added.
    }
  }
  *this = SerializedHandle();
}

// static
bool SerializedHandle::WriteHeader(const Header& hdr, Pickle* pickle) {
  if (!pickle->WriteInt(hdr.type))
    return false;
  if (hdr.type == SHARED_MEMORY) {
    if (!pickle->WriteUInt32(hdr.size))
      return false;
  }
  return true;
}

// static
bool SerializedHandle::ReadHeader(PickleIterator* iter, Header* hdr) {
  *hdr = Header(INVALID, 0);
  int type = 0;
  if (!iter->ReadInt(&type))
    return false;
  bool valid_type = false;
  switch (type) {
    case SHARED_MEMORY: {
      uint32_t size = 0;
      if (!iter->ReadUInt32(&size))
        return false;
      hdr->size = size;
      valid_type = true;
      break;
    }
    case SOCKET:
    case CHANNEL_HANDLE:
    case INVALID:
      valid_type = true;
      break;
    // No default so the compiler will warn us if a new type is added.
  }
  if (valid_type)
    hdr->type = Type(type);
  return valid_type;
}

}  // namespace proxy
}  // namespace ppapi
