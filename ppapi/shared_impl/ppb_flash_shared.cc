// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_flash_shared.h"

namespace ppapi {

PPB_Flash_Shared::PPB_Flash_Shared() {
}

PPB_Flash_Shared::~PPB_Flash_Shared() {
}

void PPB_Flash_Shared::FreeDirContents(PP_Instance instance,
                                       PP_DirContents_Dev* contents) {
  for (int32_t i = 0; i < contents->count; ++i)
    delete[] contents->entries[i].name;
  delete[] contents->entries;
  delete contents;
}

// static
bool PPB_Flash_Shared::IsValidClipboardType(
    PP_Flash_Clipboard_Type clipboard_type) {
  return clipboard_type == PP_FLASH_CLIPBOARD_TYPE_STANDARD ||
         clipboard_type == PP_FLASH_CLIPBOARD_TYPE_SELECTION;
}

// static
bool PPB_Flash_Shared::IsValidClipboardFormat(
    PP_Flash_Clipboard_Format format) {
  // Purposely excludes |PP_FLASH_CLIPBOARD_FORMAT_INVALID|.
  return format == PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT ||
         format == PP_FLASH_CLIPBOARD_FORMAT_HTML ||
         format == PP_FLASH_CLIPBOARD_FORMAT_RTF;
}

}  // namespace ppapi
