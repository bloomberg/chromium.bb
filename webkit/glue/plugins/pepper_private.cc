// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "webkit/glue/plugins/pepper_private.h"

#include "base/utf_string_conversions.h"
#include "grit/webkit_strings.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_var.h"
#include "webkit/glue/plugins/ppb_private.h"

namespace pepper {

#if defined(OS_LINUX)
class PrivateFontFile : public Resource {
 public:
  PrivateFontFile(PluginModule* module, int fd) : Resource(module), fd_(fd) {}
  virtual ~PrivateFontFile() {}

  // Resource overrides.
  PrivateFontFile* AsPrivateFontFile() { return this; }

  bool GetFontTable(uint32_t table,
                    void* output,
                    uint32_t* output_length);

 private:
  int fd_;
};
#endif

namespace {

PP_Var GetLocalizedString(PP_ResourceString string_id) {
  std::string rv;
  if (string_id == PP_RESOURCESTRING_PDFGETPASSWORD)
    rv = UTF16ToUTF8(webkit_glue::GetLocalizedString(IDS_PDF_NEED_PASSWORD));

  return StringToPPVar(rv);
}

PP_Resource GetFontFileWithFallback(
    PP_Module module_id,
    const PP_PrivateFontFileDescription* description) {
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

  scoped_refptr<PrivateFontFile> font(new PrivateFontFile(module, fd));

  return font->GetReference();
#else
  // For trusted pepper plugins, this is only needed in Linux since font loading
  // on Windows and Mac works through the renderer sandbox.
  return false;
#endif
}

bool GetFontTableForPrivateFontFile(PP_Resource font_file,
                                    uint32_t table,
                                    void* output,
                                    uint32_t* output_length) {
#if defined(OS_LINUX)
  scoped_refptr<PrivateFontFile> font(
      Resource::GetAs<PrivateFontFile>(font_file));
  if (!font.get())
    return false;
  return font->GetFontTable(table, output, output_length);
#else
  return false;
#endif
}

const PPB_Private ppb_private = {
  &GetLocalizedString,
  &GetFontFileWithFallback,
  &GetFontTableForPrivateFontFile,
};

}  // namespace

// static
const PPB_Private* Private::GetInterface() {
  return &ppb_private;
}

#if defined(OS_LINUX)
bool PrivateFontFile::GetFontTable(uint32_t table,
                                   void* output,
                                   uint32_t* output_length) {
  size_t temp_size = static_cast<size_t>(*output_length);
  bool rv = webkit_glue::GetFontTable(
      fd_, table, static_cast<uint8_t*>(output), &temp_size);
  *output_length = static_cast<uint32_t>(temp_size);
  return rv;
}
#endif

}  // namespace pepper
