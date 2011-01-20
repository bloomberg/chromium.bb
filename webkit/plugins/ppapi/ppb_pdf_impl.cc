// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_pdf_impl.h"

#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_strings.h"
#include "skia/ext/platform_canvas.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "unicode/usearch.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/var.h"

namespace webkit {
namespace ppapi {

#if defined(OS_LINUX)
class PrivateFontFile : public Resource {
 public:
  PrivateFontFile(PluginInstance* instance, int fd)
      : Resource(instance),
        fd_(fd) {
  }
  virtual ~PrivateFontFile() {
  }

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

PP_Var GetLocalizedString(PP_Instance instance_id,
                          PP_ResourceString string_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return PP_MakeUndefined();

  std::string rv;
  if (string_id == PP_RESOURCESTRING_PDFGETPASSWORD) {
    rv = UTF16ToUTF8(webkit_glue::GetLocalizedString(IDS_PDF_NEED_PASSWORD));
  } else if (string_id == PP_RESOURCESTRING_PDFLOADING) {
    rv = UTF16ToUTF8(webkit_glue::GetLocalizedString(IDS_PDF_PAGE_LOADING));
  } else if (string_id == PP_RESOURCESTRING_PDFLOAD_FAILED) {
    rv = UTF16ToUTF8(webkit_glue::GetLocalizedString(IDS_PDF_PAGE_LOAD_FAILED));
  } else {
    NOTREACHED();
  }

  return StringVar::StringToPPVar(instance->module(), rv);
}

PP_Resource GetResourceImage(PP_Instance instance_id,
                             PP_ResourceImage image_id) {
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

  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;
  scoped_refptr<PPB_ImageData_Impl> image_data(
      new PPB_ImageData_Impl(instance));
  if (!image_data->Init(PPB_ImageData_Impl::GetNativeImageDataFormat(),
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
    PP_Instance instance_id,
    const PP_FontDescription_Dev* description,
    PP_PrivateFontCharset charset) {
#if defined(OS_LINUX)
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<StringVar> face_name(StringVar::FromPPVar(description->face));
  if (!face_name)
    return 0;

  int fd = webkit_glue::MatchFontWithFallback(
      face_name->value().c_str(),
      description->weight >= PP_FONTWEIGHT_BOLD,
      description->italic,
      charset);
  if (fd == -1)
    return 0;

  scoped_refptr<PrivateFontFile> font(new PrivateFontFile(instance, fd));

  return font->GetReference();
#else
  // For trusted PPAPI plugins, this is only needed in Linux since font loading
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

void SearchString(PP_Instance instance,
                  const unsigned short* input_string,
                  const unsigned short* input_term,
                  bool case_sensitive,
                  PP_PrivateFindResult** results,
                  int* count) {
  const char16* string = reinterpret_cast<const char16*>(input_string);
  const char16* term = reinterpret_cast<const char16*>(input_term);

  UErrorCode status = U_ZERO_ERROR;
  UStringSearch* searcher = usearch_open(
      term, -1, string, -1, webkit_glue::GetWebKitLocale().c_str(), 0,
      &status);
  DCHECK(status == U_ZERO_ERROR || status == U_USING_FALLBACK_WARNING ||
         status == U_USING_DEFAULT_WARNING);
  UCollationStrength strength = case_sensitive ? UCOL_TERTIARY : UCOL_PRIMARY;

  UCollator* collator = usearch_getCollator(searcher);
  if (ucol_getStrength(collator) != strength) {
    ucol_setStrength(collator, strength);
    usearch_reset(searcher);
  }

  status = U_ZERO_ERROR;
  int match_start = usearch_first(searcher, &status);
  DCHECK(status == U_ZERO_ERROR);

  std::vector<PP_PrivateFindResult> pp_results;
  while (match_start != USEARCH_DONE) {
    size_t matched_length = usearch_getMatchedLength(searcher);
    PP_PrivateFindResult result;
    result.start_index = match_start;
    result.length = matched_length;
    pp_results.push_back(result);
    match_start = usearch_next(searcher, &status);
    DCHECK(status == U_ZERO_ERROR);
  }

  *count = pp_results.size();
  if (*count) {
    *results = reinterpret_cast<PP_PrivateFindResult*>(
        malloc(*count * sizeof(PP_PrivateFindResult)));
    memcpy(*results, &pp_results[0], *count * sizeof(PP_PrivateFindResult));
  } else {
    *results = NULL;
  }

  usearch_close(searcher);
}

void DidStartLoading(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return;
  instance->delegate()->DidStartLoading();
}

void DidStopLoading(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return;
  instance->delegate()->DidStopLoading();
}

void SetContentRestriction(PP_Instance instance_id, int restrictions) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return;
  instance->delegate()->SetContentRestriction(restrictions);
}

void HistogramPDFPageCount(int count) {
  UMA_HISTOGRAM_COUNTS_10000("PDF.PageCount", count);
}

void UserMetricsRecordAction(PP_Var action) {
  scoped_refptr<StringVar> action_str(StringVar::FromPPVar(action));
  if (action_str)
    webkit_glue::UserMetricsRecordAction(action_str->value());
}

const PPB_PDF ppb_pdf = {
  &GetLocalizedString,
  &GetResourceImage,
  &GetFontFileWithFallback,
  &GetFontTableForPrivateFontFile,
  &SearchString,
  &DidStartLoading,
  &DidStopLoading,
  &SetContentRestriction,
  &HistogramPDFPageCount,
  &UserMetricsRecordAction
};

}  // namespace

// static
const PPB_PDF* PPB_PDF_Impl::GetInterface() {
  return &ppb_pdf;
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

}  // namespace ppapi
}  // namespace webkit

