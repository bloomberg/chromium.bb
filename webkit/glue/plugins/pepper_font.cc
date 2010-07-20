// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "webkit/glue/plugins/pepper_font.h"

#if defined(OS_LINUX)
#include <unistd.h>
#endif

#include "base/logging.h"
#include "third_party/ppapi/c/ppb_font.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/webkit_glue.h"

namespace pepper {

namespace {

PP_Resource MatchFontWithFallback(PP_Module module_id,
                                  const PP_FontDescription* description) {
#if defined(OS_LINUX)
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return NULL;

  int fd = webkit_glue::MatchFontWithFallback(description->face,
                                              description->weight >= 700,
                                              description->italic,
                                              description->charset);
  if (fd == -1)
    return NULL;

  scoped_refptr<Font> font(new Font(module, fd));

  return font->GetReference();
#else
  // For trusted pepper plugins, this is only needed in Linux since font loading
  // on Windows and Mac works through the renderer sandbox.
  return false;
#endif
}

bool IsFont(PP_Resource resource) {
  return !!Resource::GetAs<Font>(resource);
}

bool GetFontTable(PP_Resource font_id,
                  uint32_t table,
                  void* output,
                  uint32_t* output_length) {
  scoped_refptr<Font> font(Resource::GetAs<Font>(font_id));
  if (!font.get())
    return false;

  return font->GetFontTable(table, output, output_length);
}

const PPB_Font ppb_font = {
  &MatchFontWithFallback,
  &IsFont,
  &GetFontTable,
};

}  // namespace

Font::Font(PluginModule* module, int fd)
    : Resource(module),
      fd_(fd) {
}

Font::~Font() {
#if defined (OS_LINUX)
  close(fd_);
#endif
}

// static
const PPB_Font* Font::GetInterface() {
  return &ppb_font;
}

bool Font::GetFontTable(uint32_t table,
                        void* output,
                        uint32_t* output_length) {
#if defined(OS_LINUX)
  size_t temp_size = static_cast<size_t>(*output_length);
  bool rv = webkit_glue::GetFontTable(
      fd_, table, static_cast<uint8_t*>(output), &temp_size);
  *output_length = static_cast<uint32_t>(temp_size);
  return rv;
#else
  return false;
#endif
}

}  // namespace pepper
