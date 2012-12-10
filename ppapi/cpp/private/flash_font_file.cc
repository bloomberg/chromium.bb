// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/flash_font_file.h"

#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/private/ppb_flash_font_file.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

// TODO(yzshen): Once PPB_Flash_FontFile gets to the stable channel, we can
// remove the code of using PPB_PDF in this file.
template <> const char* interface_name<PPB_PDF>() {
  return PPB_PDF_INTERFACE;
}

template <> const char* interface_name<PPB_Flash_FontFile_0_1>() {
  return PPB_FLASH_FONTFILE_INTERFACE_0_1;
}

}  // namespace

namespace flash {

FontFile::FontFile() {
}

FontFile::FontFile(const InstanceHandle& instance,
                   const PP_BrowserFont_Trusted_Description* description,
                   PP_PrivateFontCharset charset) {
  if (has_interface<PPB_Flash_FontFile_0_1>()) {
    PassRefFromConstructor(get_interface<PPB_Flash_FontFile_0_1>()->Create(
        instance.pp_instance(), description, charset));
  } else if (has_interface<PPB_PDF>()) {
    // PP_Font_Description_Dev and PP_BrowserFont_Trusted_Description are the
    // same struct so we can call this old interface by casting.
    PassRefFromConstructor(get_interface<PPB_PDF>()->GetFontFileWithFallback(
        instance.pp_instance(),
        reinterpret_cast<const PP_FontDescription_Dev*>(description),
        charset));
  }
}

FontFile::~FontFile() {
}

// static
bool FontFile::IsAvailable() {
  return has_interface<PPB_Flash_FontFile_0_1>() || has_interface<PPB_PDF>();
}

bool FontFile::GetFontTable(uint32_t table,
                            void* output,
                            uint32_t* output_length) {
  if (has_interface<PPB_Flash_FontFile_0_1>()) {
    return !!get_interface<PPB_Flash_FontFile_0_1>()->
        GetFontTable(pp_resource(), table, output, output_length);
  }
  if (has_interface<PPB_PDF>()) {
    return get_interface<PPB_PDF>()->GetFontTableForPrivateFontFile(
        pp_resource(), table, output, output_length);
  }
  return false;
}

}  // namespace flash
}  // namespace pp
