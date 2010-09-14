// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "webkit/glue/plugins/pepper_private.h"

#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_strings.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/pepper_image_data.h"
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

struct ResourceImageInfo {
  PP_ResourceImage pp_id;
  int res_id;
};

static const ResourceImageInfo kResourceImageMap[] = {
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTH, IDR_PDF_BUTTON_FTH },
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTH_HOVER, IDR_PDF_BUTTON_FTH_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTH_PRESSED, IDR_PDF_BUTTON_FTH_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTW, IDR_PDF_BUTTON_FTW },
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTW_HOVER, IDR_PDF_BUTTON_FTW_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_FTW_PRESSED, IDR_PDF_BUTTON_FTW_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN, IDR_PDF_BUTTON_ZOOMIN },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_HOVER, IDR_PDF_BUTTON_ZOOMIN_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN_PRESSED, IDR_PDF_BUTTON_ZOOMIN_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT, IDR_PDF_BUTTON_ZOOMOUT },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT_HOVER, IDR_PDF_BUTTON_ZOOMOUT_HOVER },
  { PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMOUT_PRESSED,
      IDR_PDF_BUTTON_ZOOMOUT_PRESSED },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_0, IDR_PDF_THUMBNAIL_0 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_1, IDR_PDF_THUMBNAIL_1 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_2, IDR_PDF_THUMBNAIL_2 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_3, IDR_PDF_THUMBNAIL_3 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_4, IDR_PDF_THUMBNAIL_4 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_5, IDR_PDF_THUMBNAIL_5 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_6, IDR_PDF_THUMBNAIL_6 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_7, IDR_PDF_THUMBNAIL_7 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_8, IDR_PDF_THUMBNAIL_8 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_9, IDR_PDF_THUMBNAIL_9 },
  { PP_RESOURCEIMAGE_PDF_BUTTON_THUMBNAIL_NUM_BACKGROUND,
      IDR_PDF_THUMBNAIL_NUM_BACKGROUND },
};

PP_Var GetLocalizedString(PP_Module module_id, PP_ResourceString string_id) {
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return PP_MakeVoid();

  std::string rv;
  if (string_id == PP_RESOURCESTRING_PDFGETPASSWORD)
    rv = UTF16ToUTF8(webkit_glue::GetLocalizedString(IDS_PDF_NEED_PASSWORD));

  return StringVar::StringToPPVar(module, rv);
}

PP_Resource GetResourceImage(PP_Module module_id, PP_ResourceImage image_id) {
  int res_id = 0;
  for (size_t i = 0; i < arraysize(kResourceImageMap); ++i) {
    if (kResourceImageMap[i].pp_id == image_id) {
      res_id = kResourceImageMap[i].res_id;
      break;
    }
  }
  if (res_id == 0)
    return 0;

  SkBitmap* res_bitmap =
      ResourceBundle::GetSharedInstance().GetBitmapNamed(res_id);

  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return 0;
  scoped_refptr<pepper::ImageData> image_data(new pepper::ImageData(module));
  if (!image_data->Init(PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                        res_bitmap->width(), res_bitmap->height(), false)) {
    return 0;
  }

  ImageDataAutoMapper mapper(image_data);
  if (!mapper.is_valid())
    return 0;

  skia::PlatformCanvas* canvas = image_data->mapped_canvas();
  SkBitmap& ret_bitmap =
      const_cast<SkBitmap&>(canvas->getTopPlatformDevice().accessBitmap(true));
  if (!res_bitmap->copyTo(&ret_bitmap, SkBitmap::kARGB_8888_Config, NULL)) {
    return 0;
  }

  return image_data->GetReference();
}

PP_Resource GetFontFileWithFallback(
    PP_Module module_id,
    const PP_PrivateFontFileDescription* description) {
#if defined(OS_LINUX)
  PluginModule* module = PluginModule::FromPPModule(module_id);
  if (!module)
    return 0;

  int fd = webkit_glue::MatchFontWithFallback(description->face,
                                              description->weight >= 700,
                                              description->italic,
                                              description->charset);
  if (fd == -1)
    return 0;

  scoped_refptr<PrivateFontFile> font(new PrivateFontFile(module, fd));

  return font->GetReference();
#else
  // For trusted pepper plugins, this is only needed in Linux since font loading
  // on Windows and Mac works through the renderer sandbox.
  return 0;
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
  &GetResourceImage,
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
