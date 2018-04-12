// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_engine.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>

#include "base/auto_reset.h"
#include "base/i18n/encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gin/array_buffer.h"
#include "gin/public/gin_embedders.h"
#include "gin/public/isolate_holder.h"
#include "pdf/draw_utils.h"
#include "pdf/pdf_transform.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_read.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "pdf/url_loader_wrapper_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/cpp/dev/memory_dev.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/pdf.h"
#include "ppapi/cpp/trusted/browser_font_trusted.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_dictionary.h"
#include "printing/units.h"
#include "third_party/pdfium/public/cpp/fpdf_deleters.h"
#include "third_party/pdfium/public/fpdf_annot.h"
#include "third_party/pdfium/public/fpdf_attachment.h"
#include "third_party/pdfium/public/fpdf_catalog.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdf_ext.h"
#include "third_party/pdfium/public/fpdf_flatten.h"
#include "third_party/pdfium/public/fpdf_ppo.h"
#include "third_party/pdfium/public/fpdf_save.h"
#include "third_party/pdfium/public/fpdf_searchex.h"
#include "third_party/pdfium/public/fpdf_sysfontinfo.h"
#include "third_party/pdfium/public/fpdf_transformpage.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "v8/include/v8.h"

using printing::ConvertUnit;
using printing::ConvertUnitDouble;
using printing::kPointsPerInch;
using printing::kPixelsPerInch;

namespace chrome_pdf {

static_assert(static_cast<int>(PDFEngine::FormType::kNone) == FORMTYPE_NONE,
              "None form types must match");
static_assert(static_cast<int>(PDFEngine::FormType::kAcroForm) ==
                  FORMTYPE_ACRO_FORM,
              "AcroForm form types must match");
static_assert(static_cast<int>(PDFEngine::FormType::kXFAFull) ==
                  FORMTYPE_XFA_FULL,
              "XFA full form types must match");
static_assert(static_cast<int>(PDFEngine::FormType::kXFAForeground) ==
                  FORMTYPE_XFA_FOREGROUND,
              "XFA foreground form types must match");
static_assert(static_cast<int>(PDFEngine::FormType::kCount) == FORMTYPE_COUNT,
              "Form type counts must match");

namespace {

const int32_t kPageShadowTop = 3;
const int32_t kPageShadowBottom = 7;
const int32_t kPageShadowLeft = 5;
const int32_t kPageShadowRight = 5;

const int32_t kPageSeparatorThickness = 4;
const int32_t kHighlightColorR = 153;
const int32_t kHighlightColorG = 193;
const int32_t kHighlightColorB = 218;

const uint32_t kPendingPageColor = 0xFFEEEEEE;

const uint32_t kFormHighlightColor = 0xFFE4DD;
const int32_t kFormHighlightAlpha = 100;

constexpr int kMaxPasswordTries = 3;

constexpr base::TimeDelta kTouchLongPressTimeout =
    base::TimeDelta::FromMilliseconds(300);

// Windows has native panning capabilities. No need to use our own.
#if defined(OS_WIN)
constexpr bool kViewerImplementedPanning = false;
#else
constexpr bool kViewerImplementedPanning = true;
#endif

// See Table 3.20 in
// http://www.adobe.com/devnet/acrobat/pdfs/pdf_reference_1-7.pdf
const uint32_t kPDFPermissionPrintLowQualityMask = 1 << 2;
const uint32_t kPDFPermissionPrintHighQualityMask = 1 << 11;
const uint32_t kPDFPermissionCopyMask = 1 << 4;
const uint32_t kPDFPermissionCopyAccessibleMask = 1 << 9;

const int32_t kLoadingTextVerticalOffset = 50;

// The maximum amount of time we'll spend doing a paint before we give back
// control of the thread.
constexpr base::TimeDelta kMaxProgressivePaintTime =
    base::TimeDelta::FromMilliseconds(300);

// The maximum amount of time we'll spend doing the first paint. This is less
// than the above to keep things smooth if the user is scrolling quickly. This
// is set to 250 ms to give enough time for most PDFs to render, while avoiding
// adding too much latency to the display of the final image when the user
// stops scrolling.
// Setting a higher value has minimal benefit (scrolling at less than 4 fps will
// never be a great experience) and there is some cost, since when the user
// stops scrolling the in-progress painting has to complete or timeout before
// the final painting can start.
// The scrollbar will always be responsive since it is managed by a separate
// process.
constexpr base::TimeDelta kMaxInitialProgressivePaintTime =
    base::TimeDelta::FromMilliseconds(250);

// Flag to turn edit mode tracking on.
// Do not flip until form saving is completely functional.
constexpr bool kIsEditModeTracked = false;

PDFiumEngine* g_engine_for_fontmapper = nullptr;

std::vector<uint32_t> GetPageNumbersFromPrintPageNumberRange(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count) {
  std::vector<uint32_t> page_numbers;
  for (uint32_t index = 0; index < page_range_count; ++index) {
    for (uint32_t page_number = page_ranges[index].first_page_number;
         page_number <= page_ranges[index].last_page_number; ++page_number) {
      page_numbers.push_back(page_number);
    }
  }
  return page_numbers;
}

#if defined(OS_LINUX)

PP_Instance g_last_instance_id;

// TODO(npm): Move font stuff to another file to reduce the size of this one
PP_BrowserFont_Trusted_Weight WeightToBrowserFontTrustedWeight(int weight) {
  static_assert(PP_BROWSERFONT_TRUSTED_WEIGHT_100 == 0,
                "PP_BrowserFont_Trusted_Weight min");
  static_assert(PP_BROWSERFONT_TRUSTED_WEIGHT_900 == 8,
                "PP_BrowserFont_Trusted_Weight max");
  const int kMinimumWeight = 100;
  const int kMaximumWeight = 900;
  int normalized_weight =
      std::min(std::max(weight, kMinimumWeight), kMaximumWeight);
  normalized_weight = (normalized_weight / 100) - 1;
  return static_cast<PP_BrowserFont_Trusted_Weight>(normalized_weight);
}

// This list is for CPWL_FontMap::GetDefaultFontByCharset().
// We pretend to have these font natively and let the browser (or underlying
// fontconfig) to pick the proper font on the system.
void EnumFonts(FPDF_SYSFONTINFO* sysfontinfo, void* mapper) {
  FPDF_AddInstalledFont(mapper, "Arial", FXFONT_DEFAULT_CHARSET);

  const FPDF_CharsetFontMap* font_map = FPDF_GetDefaultTTFMap();
  for (; font_map->charset != -1; ++font_map) {
    FPDF_AddInstalledFont(mapper, font_map->fontname, font_map->charset);
  }
}

void* MapFont(FPDF_SYSFONTINFO*,
              int weight,
              int italic,
              int charset,
              int pitch_family,
              const char* face,
              int* exact) {
  // Do not attempt to map fonts if pepper is not initialized (for privet local
  // printing).
  // TODO(noamsml): Real font substitution (http://crbug.com/391978)
  if (!pp::Module::Get())
    return nullptr;

  pp::BrowserFontDescription description;

  // Pretend the system does not have the Symbol font to force a fallback to
  // the built in Symbol font in CFX_FontMapper::FindSubstFont().
  if (strcmp(face, "Symbol") == 0)
    return nullptr;

  if (pitch_family & FXFONT_FF_FIXEDPITCH) {
    description.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE);
  } else if (pitch_family & FXFONT_FF_ROMAN) {
    description.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_SERIF);
  }

  static const struct {
    const char* pdf_name;
    const char* face;
    bool bold;
    bool italic;
  } kPdfFontSubstitutions[] = {
      {"Courier", "Courier New", false, false},
      {"Courier-Bold", "Courier New", true, false},
      {"Courier-BoldOblique", "Courier New", true, true},
      {"Courier-Oblique", "Courier New", false, true},
      {"Helvetica", "Arial", false, false},
      {"Helvetica-Bold", "Arial", true, false},
      {"Helvetica-BoldOblique", "Arial", true, true},
      {"Helvetica-Oblique", "Arial", false, true},
      {"Times-Roman", "Times New Roman", false, false},
      {"Times-Bold", "Times New Roman", true, false},
      {"Times-BoldItalic", "Times New Roman", true, true},
      {"Times-Italic", "Times New Roman", false, true},

      // MS P?(Mincho|Gothic) are the most notable fonts in Japanese PDF files
      // without embedding the glyphs. Sometimes the font names are encoded
      // in Japanese Windows's locale (CP932/Shift_JIS) without space.
      // Most Linux systems don't have the exact font, but for outsourcing
      // fontconfig to find substitutable font in the system, we pass ASCII
      // font names to it.
      {"MS-PGothic", "MS PGothic", false, false},
      {"MS-Gothic", "MS Gothic", false, false},
      {"MS-PMincho", "MS PMincho", false, false},
      {"MS-Mincho", "MS Mincho", false, false},
      // MS PGothic in Shift_JIS encoding.
      {"\x82\x6C\x82\x72\x82\x6F\x83\x53\x83\x56\x83\x62\x83\x4E", "MS PGothic",
       false, false},
      // MS Gothic in Shift_JIS encoding.
      {"\x82\x6C\x82\x72\x83\x53\x83\x56\x83\x62\x83\x4E", "MS Gothic", false,
       false},
      // MS PMincho in Shift_JIS encoding.
      {"\x82\x6C\x82\x72\x82\x6F\x96\xBE\x92\xA9", "MS PMincho", false, false},
      // MS Mincho in Shift_JIS encoding.
      {"\x82\x6C\x82\x72\x96\xBE\x92\xA9", "MS Mincho", false, false},
  };

  // Similar logic exists in PDFium's CFX_FolderFontInfo::FindFont().
  if (charset == FXFONT_ANSI_CHARSET && (pitch_family & FXFONT_FF_FIXEDPITCH))
    face = "Courier New";

  // Map from the standard PDF fonts to TrueType font names.
  size_t i;
  for (i = 0; i < arraysize(kPdfFontSubstitutions); ++i) {
    if (strcmp(face, kPdfFontSubstitutions[i].pdf_name) == 0) {
      description.set_face(kPdfFontSubstitutions[i].face);
      if (kPdfFontSubstitutions[i].bold)
        description.set_weight(PP_BROWSERFONT_TRUSTED_WEIGHT_BOLD);
      if (kPdfFontSubstitutions[i].italic)
        description.set_italic(true);
      break;
    }
  }

  if (i == arraysize(kPdfFontSubstitutions)) {
    // Convert to UTF-8 before calling set_face().
    std::string face_utf8;
    if (base::IsStringUTF8(face)) {
      face_utf8 = face;
    } else {
      std::string encoding;
      if (base::DetectEncoding(face, &encoding)) {
        // ConvertToUtf8AndNormalize() clears |face_utf8| on failure.
        base::ConvertToUtf8AndNormalize(face, encoding, &face_utf8);
      }
    }

    if (face_utf8.empty())
      return nullptr;

    description.set_face(face_utf8);
    description.set_weight(WeightToBrowserFontTrustedWeight(weight));
    description.set_italic(italic > 0);
  }

  if (!pp::PDF::IsAvailable()) {
    NOTREACHED();
    return nullptr;
  }

  if (g_engine_for_fontmapper)
    g_engine_for_fontmapper->FontSubstituted();

  PP_Resource font_resource = pp::PDF::GetFontFileWithFallback(
      pp::InstanceHandle(g_last_instance_id),
      &description.pp_font_description(),
      static_cast<PP_PrivateFontCharset>(charset));
  long res_id = font_resource;
  return reinterpret_cast<void*>(res_id);
}

unsigned long GetFontData(FPDF_SYSFONTINFO*,
                          void* font_id,
                          unsigned int table,
                          unsigned char* buffer,
                          unsigned long buf_size) {
  if (!pp::PDF::IsAvailable()) {
    NOTREACHED();
    return 0;
  }

  uint32_t size = buf_size;
  long res_id = reinterpret_cast<long>(font_id);
  if (!pp::PDF::GetFontTableForPrivateFontFile(res_id, table, buffer, &size))
    return 0;
  return size;
}

void DeleteFont(FPDF_SYSFONTINFO*, void* font_id) {
  long res_id = reinterpret_cast<long>(font_id);
  pp::Module::Get()->core()->ReleaseResource(res_id);
}

FPDF_SYSFONTINFO g_font_info = {1,           0, EnumFonts, MapFont,   0,
                                GetFontData, 0, 0,         DeleteFont};
#else
struct FPDF_SYSFONTINFO_WITHMETRICS : public FPDF_SYSFONTINFO {
  explicit FPDF_SYSFONTINFO_WITHMETRICS(FPDF_SYSFONTINFO* sysfontinfo) {
    version = sysfontinfo->version;
    default_sysfontinfo = sysfontinfo;
  }

  ~FPDF_SYSFONTINFO_WITHMETRICS() {
    FPDF_FreeDefaultSystemFontInfo(default_sysfontinfo);
  }

  FPDF_SYSFONTINFO* default_sysfontinfo;
};

FPDF_SYSFONTINFO_WITHMETRICS* g_font_info = nullptr;

void* MapFontWithMetrics(FPDF_SYSFONTINFO* sysfontinfo,
                         int weight,
                         int italic,
                         int charset,
                         int pitch_family,
                         const char* face,
                         int* exact) {
  auto* fontinfo_with_metrics =
      static_cast<FPDF_SYSFONTINFO_WITHMETRICS*>(sysfontinfo);
  if (!fontinfo_with_metrics->default_sysfontinfo->MapFont)
    return nullptr;
  void* mapped_font = fontinfo_with_metrics->default_sysfontinfo->MapFont(
      fontinfo_with_metrics->default_sysfontinfo, weight, italic, charset,
      pitch_family, face, exact);
  if (mapped_font && g_engine_for_fontmapper)
    g_engine_for_fontmapper->FontSubstituted();
  return mapped_font;
}

void DeleteFont(FPDF_SYSFONTINFO* sysfontinfo, void* font_id) {
  auto* fontinfo_with_metrics =
      static_cast<FPDF_SYSFONTINFO_WITHMETRICS*>(sysfontinfo);
  if (!fontinfo_with_metrics->default_sysfontinfo->DeleteFont)
    return;
  fontinfo_with_metrics->default_sysfontinfo->DeleteFont(
      fontinfo_with_metrics->default_sysfontinfo, font_id);
}

void EnumFonts(FPDF_SYSFONTINFO* sysfontinfo, void* mapper) {
  auto* fontinfo_with_metrics =
      static_cast<FPDF_SYSFONTINFO_WITHMETRICS*>(sysfontinfo);
  if (!fontinfo_with_metrics->default_sysfontinfo->EnumFonts)
    return;
  fontinfo_with_metrics->default_sysfontinfo->EnumFonts(
      fontinfo_with_metrics->default_sysfontinfo, mapper);
}

unsigned long GetFaceName(FPDF_SYSFONTINFO* sysfontinfo,
                          void* hFont,
                          char* buffer,
                          unsigned long buffer_size) {
  auto* fontinfo_with_metrics =
      static_cast<FPDF_SYSFONTINFO_WITHMETRICS*>(sysfontinfo);
  if (!fontinfo_with_metrics->default_sysfontinfo->GetFaceName)
    return 0;
  return fontinfo_with_metrics->default_sysfontinfo->GetFaceName(
      fontinfo_with_metrics->default_sysfontinfo, hFont, buffer, buffer_size);
}

void* GetFont(FPDF_SYSFONTINFO* sysfontinfo, const char* face) {
  auto* fontinfo_with_metrics =
      static_cast<FPDF_SYSFONTINFO_WITHMETRICS*>(sysfontinfo);
  if (!fontinfo_with_metrics->default_sysfontinfo->GetFont)
    return nullptr;
  return fontinfo_with_metrics->default_sysfontinfo->GetFont(
      fontinfo_with_metrics->default_sysfontinfo, face);
}

int GetFontCharset(FPDF_SYSFONTINFO* sysfontinfo, void* hFont) {
  auto* fontinfo_with_metrics =
      static_cast<FPDF_SYSFONTINFO_WITHMETRICS*>(sysfontinfo);
  if (!fontinfo_with_metrics->default_sysfontinfo->GetFontCharset)
    return 0;
  return fontinfo_with_metrics->default_sysfontinfo->GetFontCharset(
      fontinfo_with_metrics->default_sysfontinfo, hFont);
}

unsigned long GetFontData(FPDF_SYSFONTINFO* sysfontinfo,
                          void* hFont,
                          unsigned int table,
                          unsigned char* buffer,
                          unsigned long buf_size) {
  auto* fontinfo_with_metrics =
      static_cast<FPDF_SYSFONTINFO_WITHMETRICS*>(sysfontinfo);
  if (!fontinfo_with_metrics->default_sysfontinfo->GetFontData)
    return 0;
  return fontinfo_with_metrics->default_sysfontinfo->GetFontData(
      fontinfo_with_metrics->default_sysfontinfo, hFont, table, buffer,
      buf_size);
}

void Release(FPDF_SYSFONTINFO* sysfontinfo) {
  auto* fontinfo_with_metrics =
      static_cast<FPDF_SYSFONTINFO_WITHMETRICS*>(sysfontinfo);
  if (!fontinfo_with_metrics->default_sysfontinfo->Release)
    return;
  fontinfo_with_metrics->default_sysfontinfo->Release(
      fontinfo_with_metrics->default_sysfontinfo);
}
#endif  // defined(OS_LINUX)

PDFiumEngine* g_engine_for_unsupported = nullptr;

void Unsupported_Handler(UNSUPPORT_INFO*, int type) {
  if (!g_engine_for_unsupported) {
    NOTREACHED();
    return;
  }

  g_engine_for_unsupported->UnsupportedFeature(type);
}

UNSUPPORT_INFO g_unsupported_info = {1, Unsupported_Handler};

// Set the destination page size and content area in points based on source
// page rotation and orientation.
//
// |rotated| True if source page is rotated 90 degree or 270 degree.
// |is_src_page_landscape| is true if the source page orientation is landscape.
// |page_size| has the actual destination page size in points.
// |content_rect| has the actual destination page printable area values in
// points.
void SetPageSizeAndContentRect(bool rotated,
                               bool is_src_page_landscape,
                               pp::Size* page_size,
                               pp::Rect* content_rect) {
  bool is_dst_page_landscape = page_size->width() > page_size->height();
  bool page_orientation_mismatched =
      is_src_page_landscape != is_dst_page_landscape;
  bool rotate_dst_page = rotated ^ page_orientation_mismatched;
  if (rotate_dst_page) {
    page_size->SetSize(page_size->height(), page_size->width());
    content_rect->SetRect(content_rect->y(), content_rect->x(),
                          content_rect->height(), content_rect->width());
  }
}

template <class S>
bool IsAboveOrDirectlyLeftOf(const S& lhs, const S& rhs) {
  return lhs.y() < rhs.y() || (lhs.y() == rhs.y() && lhs.x() < rhs.x());
}

int CalculateCenterForZoom(int center, int length, double zoom) {
  int adjusted_center =
      static_cast<int>(center * zoom) - static_cast<int>(length * zoom / 2);
  return std::max(adjusted_center, 0);
}

// This formats a string with special 0xfffe end-of-line hyphens the same way
// as Adobe Reader. When a hyphen is encountered, the next non-CR/LF whitespace
// becomes CR+LF and the hyphen is erased. If there is no whitespace between
// two hyphens, the latter hyphen is erased and ignored.
void FormatStringWithHyphens(base::string16* text) {
  // First pass marks all the hyphen positions.
  struct HyphenPosition {
    HyphenPosition() : position(0), next_whitespace_position(0) {}
    size_t position;
    size_t next_whitespace_position;  // 0 for none
  };
  std::vector<HyphenPosition> hyphen_positions;
  HyphenPosition current_hyphen_position;
  bool current_hyphen_position_is_valid = false;
  const base::char16 kPdfiumHyphenEOL = 0xfffe;

  for (size_t i = 0; i < text->size(); ++i) {
    const base::char16& current_char = (*text)[i];
    if (current_char == kPdfiumHyphenEOL) {
      if (current_hyphen_position_is_valid)
        hyphen_positions.push_back(current_hyphen_position);
      current_hyphen_position = HyphenPosition();
      current_hyphen_position.position = i;
      current_hyphen_position_is_valid = true;
    } else if (base::IsUnicodeWhitespace(current_char)) {
      if (current_hyphen_position_is_valid) {
        if (current_char != L'\r' && current_char != L'\n')
          current_hyphen_position.next_whitespace_position = i;
        hyphen_positions.push_back(current_hyphen_position);
        current_hyphen_position_is_valid = false;
      }
    }
  }
  if (current_hyphen_position_is_valid)
    hyphen_positions.push_back(current_hyphen_position);

  // With all the hyphen positions, do the search and replace.
  while (!hyphen_positions.empty()) {
    static const base::char16 kCr[] = {L'\r', L'\0'};
    const HyphenPosition& position = hyphen_positions.back();
    if (position.next_whitespace_position != 0) {
      (*text)[position.next_whitespace_position] = L'\n';
      text->insert(position.next_whitespace_position, kCr);
    }
    text->erase(position.position, 1);
    hyphen_positions.pop_back();
  }

  // Adobe Reader also get rid of trailing spaces right before a CRLF.
  static const base::char16 kSpaceCrCn[] = {L' ', L'\r', L'\n', L'\0'};
  static const base::char16 kCrCn[] = {L'\r', L'\n', L'\0'};
  base::ReplaceSubstringsAfterOffset(text, 0, kSpaceCrCn, kCrCn);
}

// Replace CR/LF with just LF on POSIX.
void FormatStringForOS(base::string16* text) {
#if defined(OS_POSIX)
  static const base::char16 kCr[] = {L'\r', L'\0'};
  static const base::char16 kBlank[] = {L'\0'};
  base::ReplaceChars(*text, kCr, kBlank, text);
#elif defined(OS_WIN)
  // Do nothing
#else
  NOTIMPLEMENTED();
#endif
}

// Returns true if |cur| is a character to break on.
// For double clicks, look for work breaks.
// For triple clicks, look for line breaks.
// The actual algorithm used in Blink is much more complicated, so do a simple
// approximation.
bool FindMultipleClickBoundary(bool is_double_click, base::char16 cur) {
  if (!is_double_click)
    return cur == '\n';

  // Deal with ASCII characters.
  if (base::IsAsciiAlpha(cur) || base::IsAsciiDigit(cur) || cur == '_')
    return false;
  if (cur < 128)
    return true;

  static constexpr base::char16 kZeroWidthSpace = 0x200B;
  if (cur == kZeroWidthSpace)
    return true;

  return false;
}

std::string GetDocumentMetadata(FPDF_DOCUMENT doc, const std::string& key) {
  size_t size = FPDF_GetMetaText(doc, key.c_str(), nullptr, 0);
  if (size == 0)
    return std::string();

  base::string16 value;
  PDFiumAPIStringBufferSizeInBytesAdapter<base::string16> string_adapter(
      &value, size, false);
  string_adapter.Close(
      FPDF_GetMetaText(doc, key.c_str(), string_adapter.GetData(), size));
  return base::UTF16ToUTF8(value);
}

gin::IsolateHolder* g_isolate_holder = nullptr;

void SetUpV8() {
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kNonStrictMode,
                                 gin::IsolateHolder::kStableV8Extras,
                                 gin::ArrayBufferAllocator::SharedInstance());
  DCHECK(!g_isolate_holder);
  g_isolate_holder = new gin::IsolateHolder(base::ThreadTaskRunnerHandle::Get(),
                                            gin::IsolateHolder::kSingleThread);
  g_isolate_holder->isolate()->Enter();
}

void TearDownV8() {
  g_isolate_holder->isolate()->Exit();
  delete g_isolate_holder;
  g_isolate_holder = nullptr;
}

int GetBlockForJpeg(void* param,
                    unsigned long pos,
                    unsigned char* buf,
                    unsigned long size) {
  std::vector<uint8_t>* data_vector = static_cast<std::vector<uint8_t>*>(param);
  if (pos + size < pos || pos + size > data_vector->size())
    return 0;
  memcpy(buf, data_vector->data() + pos, size);
  return 1;
}

std::string WideStringToString(FPDF_WIDESTRING wide_string) {
  return base::UTF16ToUTF8(reinterpret_cast<const base::char16*>(wide_string));
}

// Returns true if the given |area| and |form_type| combination from
// PDFiumEngine::GetCharIndex() indicates it is a form text area.
bool IsFormTextArea(PDFiumPage::Area area, int form_type) {
  if (form_type == FPDF_FORMFIELD_UNKNOWN)
    return false;

  DCHECK_EQ(area, PDFiumPage::FormTypeToArea(form_type));
  return area == PDFiumPage::FORM_TEXT_AREA;
}

// Checks whether or not focus is in an editable form text area given the
// form field annotation flags and form type.
bool CheckIfEditableFormTextArea(int flags, int form_type) {
  if (!!(flags & FPDF_FORMFLAG_READONLY))
    return false;
  if (form_type == FPDF_FORMFIELD_TEXTFIELD)
    return true;
  if (form_type == FPDF_FORMFIELD_COMBOBOX &&
      (!!(flags & FPDF_FORMFLAG_CHOICE_EDIT))) {
    return true;
  }
  return false;
}

bool IsLinkArea(PDFiumPage::Area area) {
  return area == PDFiumPage::WEBLINK_AREA || area == PDFiumPage::DOCLINK_AREA;
}

// Normalize a MouseInputEvent. For Mac, this means transforming ctrl + left
// button down events into a right button down events.
pp::MouseInputEvent NormalizeMouseEvent(pp::Instance* instance,
                                        const pp::MouseInputEvent& event) {
  pp::MouseInputEvent normalized_event = event;
#if defined(OS_MACOSX)
  uint32_t modifiers = event.GetModifiers();
  if ((modifiers & PP_INPUTEVENT_MODIFIER_CONTROLKEY) &&
      event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_LEFT &&
      event.GetType() == PP_INPUTEVENT_TYPE_MOUSEDOWN) {
    uint32_t new_modifiers = modifiers & ~PP_INPUTEVENT_MODIFIER_CONTROLKEY;
    normalized_event = pp::MouseInputEvent(
        instance, PP_INPUTEVENT_TYPE_MOUSEDOWN, event.GetTimeStamp(),
        new_modifiers, PP_INPUTEVENT_MOUSEBUTTON_RIGHT, event.GetPosition(), 1,
        event.GetMovement());
  }
#endif
  return normalized_event;
}

// These values are intended for the JS to handle, and it doesn't have access
// to the PDFDEST_VIEW_* defines.
std::string ConvertViewIntToViewString(unsigned long view_int) {
  switch (view_int) {
    case PDFDEST_VIEW_XYZ:
      return "XYZ";
    case PDFDEST_VIEW_FIT:
      return "Fit";
    case PDFDEST_VIEW_FITH:
      return "FitH";
    case PDFDEST_VIEW_FITV:
      return "FitV";
    case PDFDEST_VIEW_FITR:
      return "FitR";
    case PDFDEST_VIEW_FITB:
      return "FitB";
    case PDFDEST_VIEW_FITBH:
      return "FitBH";
    case PDFDEST_VIEW_FITBV:
      return "FitBV";
    case PDFDEST_VIEW_UNKNOWN_MODE:
      return "";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

bool InitializeSDK() {
  SetUpV8();

  FPDF_LIBRARY_CONFIG config;
  config.version = 2;
  config.m_pUserFontPaths = nullptr;
  config.m_pIsolate = v8::Isolate::GetCurrent();
  config.m_v8EmbedderSlot = gin::kEmbedderPDFium;
  FPDF_InitLibraryWithConfig(&config);

#if defined(OS_LINUX)
  // Font loading doesn't work in the renderer sandbox in Linux.
  FPDF_SetSystemFontInfo(&g_font_info);
#else
  g_font_info =
      new FPDF_SYSFONTINFO_WITHMETRICS(FPDF_GetDefaultSystemFontInfo());
  g_font_info->Release = Release;
  g_font_info->EnumFonts = EnumFonts;
  // Set new MapFont that calculates metrics
  g_font_info->MapFont = MapFontWithMetrics;
  g_font_info->GetFont = GetFont;
  g_font_info->GetFaceName = GetFaceName;
  g_font_info->GetFontCharset = GetFontCharset;
  g_font_info->GetFontData = GetFontData;
  g_font_info->DeleteFont = DeleteFont;
  FPDF_SetSystemFontInfo(g_font_info);
#endif

  FSDK_SetUnSpObjProcessHandler(&g_unsupported_info);

  return true;
}

void ShutdownSDK() {
  FPDF_DestroyLibrary();
#if !defined(OS_LINUX)
  delete g_font_info;
#endif
  TearDownV8();
}

std::unique_ptr<PDFEngine> PDFEngine::Create(PDFEngine::Client* client) {
  return std::make_unique<PDFiumEngine>(client);
}

PDFiumEngine::PDFiumEngine(PDFEngine::Client* client)
    : client_(client),
      current_zoom_(1.0),
      current_rotation_(0),
      doc_(nullptr),
      form_(nullptr),
      defer_page_unload_(false),
      selecting_(false),
      mouse_down_state_(PDFiumPage::NONSELECTABLE_AREA,
                        PDFiumPage::LinkTarget()),
      in_form_text_area_(false),
      editable_form_text_area_(false),
      mouse_left_button_down_(false),
      mouse_middle_button_down_(false),
      permissions_(0),
      permissions_handler_revision_(-1),
      fpdf_availability_(nullptr),
      last_page_mouse_down_(-1),
      most_visible_page_(-1),
      called_do_document_action_(false),
      render_grayscale_(false),
      render_annots_(true),
      edit_mode_(false) {
  find_factory_.Initialize(this);
  password_factory_.Initialize(this);

  file_access_.m_FileLen = 0;
  file_access_.m_GetBlock = &GetBlock;
  file_access_.m_Param = this;

  file_availability_.version = 1;
  file_availability_.IsDataAvail = &IsDataAvail;
  file_availability_.engine = this;

  download_hints_.version = 1;
  download_hints_.AddSegment = &AddSegment;
  download_hints_.engine = this;

  // Initialize FPDF_FORMFILLINFO member variables.  Deriving from this struct
  // allows the static callbacks to be able to cast the FPDF_FORMFILLINFO in
  // callbacks to ourself instead of maintaining a map of them to
  // PDFiumEngine.
  FPDF_FORMFILLINFO::version = 1;
  FPDF_FORMFILLINFO::m_pJsPlatform = this;
  FPDF_FORMFILLINFO::Release = nullptr;
  FPDF_FORMFILLINFO::FFI_Invalidate = Form_Invalidate;
  FPDF_FORMFILLINFO::FFI_OutputSelectedRect = Form_OutputSelectedRect;
  FPDF_FORMFILLINFO::FFI_SetCursor = Form_SetCursor;
  FPDF_FORMFILLINFO::FFI_SetTimer = Form_SetTimer;
  FPDF_FORMFILLINFO::FFI_KillTimer = Form_KillTimer;
  FPDF_FORMFILLINFO::FFI_GetLocalTime = Form_GetLocalTime;
  FPDF_FORMFILLINFO::FFI_OnChange = Form_OnChange;
  FPDF_FORMFILLINFO::FFI_GetPage = Form_GetPage;
  FPDF_FORMFILLINFO::FFI_GetCurrentPage = Form_GetCurrentPage;
  FPDF_FORMFILLINFO::FFI_GetRotation = Form_GetRotation;
  FPDF_FORMFILLINFO::FFI_ExecuteNamedAction = Form_ExecuteNamedAction;
  FPDF_FORMFILLINFO::FFI_SetTextFieldFocus = Form_SetTextFieldFocus;
  FPDF_FORMFILLINFO::FFI_DoURIAction = Form_DoURIAction;
  FPDF_FORMFILLINFO::FFI_DoGoToAction = Form_DoGoToAction;
#if defined(PDF_ENABLE_XFA)
  FPDF_FORMFILLINFO::version = 2;
  FPDF_FORMFILLINFO::FFI_EmailTo = Form_EmailTo;
  FPDF_FORMFILLINFO::FFI_DisplayCaret = Form_DisplayCaret;
  FPDF_FORMFILLINFO::FFI_SetCurrentPage = Form_SetCurrentPage;
  FPDF_FORMFILLINFO::FFI_GetCurrentPageIndex = Form_GetCurrentPageIndex;
  FPDF_FORMFILLINFO::FFI_GetPageViewRect = Form_GetPageViewRect;
  FPDF_FORMFILLINFO::FFI_GetPlatform = Form_GetPlatform;
  FPDF_FORMFILLINFO::FFI_PageEvent = nullptr;
  FPDF_FORMFILLINFO::FFI_PopupMenu = Form_PopupMenu;
  FPDF_FORMFILLINFO::FFI_PostRequestURL = Form_PostRequestURL;
  FPDF_FORMFILLINFO::FFI_PutRequestURL = Form_PutRequestURL;
  FPDF_FORMFILLINFO::FFI_UploadTo = Form_UploadTo;
  FPDF_FORMFILLINFO::FFI_DownloadFromURL = Form_DownloadFromURL;
  FPDF_FORMFILLINFO::FFI_OpenFile = Form_OpenFile;
  FPDF_FORMFILLINFO::FFI_GotoURL = Form_GotoURL;
  FPDF_FORMFILLINFO::FFI_GetLanguage = Form_GetLanguage;
#endif  // defined(PDF_ENABLE_XFA)
  IPDF_JSPLATFORM::version = 3;
  IPDF_JSPLATFORM::app_alert = Form_Alert;
  IPDF_JSPLATFORM::app_beep = Form_Beep;
  IPDF_JSPLATFORM::app_response = Form_Response;
  IPDF_JSPLATFORM::Doc_getFilePath = Form_GetFilePath;
  IPDF_JSPLATFORM::Doc_mail = Form_Mail;
  IPDF_JSPLATFORM::Doc_print = Form_Print;
  IPDF_JSPLATFORM::Doc_submitForm = Form_SubmitForm;
  IPDF_JSPLATFORM::Doc_gotoPage = Form_GotoPage;
  IPDF_JSPLATFORM::Field_browse = nullptr;

  IFSDK_PAUSE::version = 1;
  IFSDK_PAUSE::user = nullptr;
  IFSDK_PAUSE::NeedToPauseNow = Pause_NeedToPauseNow;

#if defined(OS_LINUX)
  // PreviewModeClient does not know its pp::Instance.
  pp::Instance* instance = client_->GetPluginInstance();
  if (instance)
    g_last_instance_id = instance->pp_instance();
#endif
}

PDFiumEngine::~PDFiumEngine() {
  for (auto& page : pages_)
    page->Unload();

  if (doc_) {
    FORM_DoDocumentAAction(form_, FPDFDOC_AACTION_WC);
    FPDFDOC_ExitFormFillEnvironment(form_);
    FPDF_CloseDocument(doc_);
  }
  FPDFAvail_Destroy(fpdf_availability_);
}

#if defined(PDF_ENABLE_XFA)

void PDFiumEngine::Form_EmailTo(FPDF_FORMFILLINFO* param,
                                FPDF_FILEHANDLER* file_handler,
                                FPDF_WIDESTRING to,
                                FPDF_WIDESTRING subject,
                                FPDF_WIDESTRING cc,
                                FPDF_WIDESTRING bcc,
                                FPDF_WIDESTRING message) {
  std::string to_str = WideStringToString(to);
  std::string subject_str = WideStringToString(subject);
  std::string cc_str = WideStringToString(cc);
  std::string bcc_str = WideStringToString(bcc);
  std::string message_str = WideStringToString(message);

  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  engine->client_->Email(to_str, cc_str, bcc_str, subject_str, message_str);
}

void PDFiumEngine::Form_DisplayCaret(FPDF_FORMFILLINFO* param,
                                     FPDF_PAGE page,
                                     FPDF_BOOL visible,
                                     double left,
                                     double top,
                                     double right,
                                     double bottom) {}

void PDFiumEngine::Form_SetCurrentPage(FPDF_FORMFILLINFO* param,
                                       FPDF_DOCUMENT document,
                                       int page) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  pp::Rect page_view_rect = engine->GetPageContentsRect(page);
  engine->ScrolledToYPosition(page_view_rect.height());
  pp::Point pos(1, page_view_rect.height());
  engine->SetScrollPosition(pos);
}

int PDFiumEngine::Form_GetCurrentPageIndex(FPDF_FORMFILLINFO* param,
                                           FPDF_DOCUMENT document) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  return engine->GetMostVisiblePage();
}

void PDFiumEngine::Form_GetPageViewRect(FPDF_FORMFILLINFO* param,
                                        FPDF_PAGE page,
                                        double* left,
                                        double* top,
                                        double* right,
                                        double* bottom) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  int page_index = engine->GetVisiblePageIndex(page);
  if (!engine->PageIndexInBounds(page_index)) {
    *left = 0;
    *right = 0;
    *top = 0;
    *bottom = 0;
    return;
  }

  pp::Rect page_view_rect = engine->GetPageContentsRect(page_index);

  float toolbar_height_in_screen_coords =
      engine->GetToolbarHeightInScreenCoords();

  float page_width = FPDF_GetPageWidth(page);
  float page_height = FPDF_GetPageHeight(page);

  // To convert from a screen scale to a page scale, we multiply by
  // (page_height / page_view_rect.height()) and
  // (page_width / page_view_rect.width()),
  // The base point of the page in screen coords is (page_view_rect.x(),
  // page_view_rect.y()).
  // Therefore, to convert an x position from screen to page
  // coords, we use (page_width * (x - base_x) / page_view_rect.width()).
  // For y positions, (page_height * (y - base_y) / page_view_rect.height()).

  // The top-most y position that can be relied to be visible on the screen is
  // the bottom of the toolbar, which is y = toolbar_height_in_screen_coords.
  float screen_top_in_page_coords =
      page_height * (toolbar_height_in_screen_coords - page_view_rect.y()) /
      page_view_rect.height();
  // The bottom-most y position that is visible on the screen is the bottom of
  // the plugin area, which is y = engine->plugin_size_.height().
  float screen_bottom_in_page_coords =
      page_height * (engine->plugin_size_.height() - page_view_rect.y()) /
      page_view_rect.height();
  // The left-most x position that is visible on the screen is the left of the
  // plugin area, which is x = 0.
  float screen_left_in_page_coords =
      page_width * (0 - page_view_rect.x()) / page_view_rect.width();
  // The right-most x position that is visible on the screen is the right of the
  // plugin area, which is x = engine->plugin_size_.width().
  float screen_right_in_page_coords =
      page_width * (engine->plugin_size_.width() - page_view_rect.x()) /
      page_view_rect.width();

  // Return the edge of the screen or of the page, since we're restricted to
  // both.
  *left = std::max(screen_left_in_page_coords, 0.0f);
  *right = std::min(screen_right_in_page_coords, page_width);
  *top = std::max(screen_top_in_page_coords, 0.0f);
  *bottom = std::min(screen_bottom_in_page_coords, page_height);
}

int PDFiumEngine::Form_GetPlatform(FPDF_FORMFILLINFO* param,
                                   void* platform,
                                   int length) {
  int platform_flag = -1;

#if defined(WIN32)
  platform_flag = 0;
#elif defined(__linux__)
  platform_flag = 1;
#else
  platform_flag = 2;
#endif

  std::string javascript =
      "alert(\"Platform:" + base::NumberToString(platform_flag) + "\")";

  return platform_flag;
}

FPDF_BOOL PDFiumEngine::Form_PopupMenu(FPDF_FORMFILLINFO* param,
                                       FPDF_PAGE page,
                                       FPDF_WIDGET widget,
                                       int menu_flag,
                                       float x,
                                       float y) {
  return false;
}

FPDF_BOOL PDFiumEngine::Form_PostRequestURL(FPDF_FORMFILLINFO* param,
                                            FPDF_WIDESTRING url,
                                            FPDF_WIDESTRING data,
                                            FPDF_WIDESTRING content_type,
                                            FPDF_WIDESTRING encode,
                                            FPDF_WIDESTRING header,
                                            FPDF_BSTR* response) {
  std::string url_str = WideStringToString(url);
  std::string data_str = WideStringToString(data);
  std::string content_type_str = WideStringToString(content_type);
  std::string encode_str = WideStringToString(encode);
  std::string header_str = WideStringToString(header);

  std::string javascript = "alert(\"Post:" + url_str + "," + data_str + "," +
                           content_type_str + "," + encode_str + "," +
                           header_str + "\")";
  return true;
}

FPDF_BOOL PDFiumEngine::Form_PutRequestURL(FPDF_FORMFILLINFO* param,
                                           FPDF_WIDESTRING url,
                                           FPDF_WIDESTRING data,
                                           FPDF_WIDESTRING encode) {
  std::string url_str = WideStringToString(url);
  std::string data_str = WideStringToString(data);
  std::string encode_str = WideStringToString(encode);

  std::string javascript =
      "alert(\"Put:" + url_str + "," + data_str + "," + encode_str + "\")";

  return true;
}

void PDFiumEngine::Form_UploadTo(FPDF_FORMFILLINFO* param,
                                 FPDF_FILEHANDLER* file_handle,
                                 int file_flag,
                                 FPDF_WIDESTRING to) {
  std::string to_str = WideStringToString(to);
  // TODO: needs the full implementation of form uploading
}

FPDF_LPFILEHANDLER PDFiumEngine::Form_DownloadFromURL(FPDF_FORMFILLINFO* param,
                                                      FPDF_WIDESTRING url) {
  // NOTE: Think hard about the security implications before allowing
  // a PDF file to perform this action.
  return nullptr;
}

FPDF_FILEHANDLER* PDFiumEngine::Form_OpenFile(FPDF_FORMFILLINFO* param,
                                              int file_flag,
                                              FPDF_WIDESTRING url,
                                              const char* mode) {
  // NOTE: Think hard about the security implications before allowing
  // a PDF file to perform this action.
  return nullptr;
}

void PDFiumEngine::Form_GotoURL(FPDF_FORMFILLINFO* param,
                                FPDF_DOCUMENT document,
                                FPDF_WIDESTRING url) {
  std::string url_str = WideStringToString(url);
  // TODO: needs to implement GOTO URL action
}

int PDFiumEngine::Form_GetLanguage(FPDF_FORMFILLINFO* param,
                                   void* language,
                                   int length) {
  return 0;
}

#endif  // defined(PDF_ENABLE_XFA)

int PDFiumEngine::GetBlock(void* param,
                           unsigned long position,
                           unsigned char* buffer,
                           unsigned long size) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  return engine->doc_loader_->GetBlock(position, size, buffer);
}

FPDF_BOOL PDFiumEngine::IsDataAvail(FX_FILEAVAIL* param,
                                    size_t offset,
                                    size_t size) {
  auto* file_avail = static_cast<FileAvail*>(param);
  return file_avail->engine->doc_loader_->IsDataAvailable(offset, size);
}

void PDFiumEngine::AddSegment(FX_DOWNLOADHINTS* param,
                              size_t offset,
                              size_t size) {
  auto* download_hints = static_cast<DownloadHints*>(param);
  return download_hints->engine->doc_loader_->RequestData(offset, size);
}

bool PDFiumEngine::New(const char* url, const char* headers) {
  url_ = url;
  if (headers)
    headers_ = headers;
  else
    headers_.clear();
  return true;
}

void PDFiumEngine::PageOffsetUpdated(const pp::Point& page_offset) {
  page_offset_ = page_offset;
}

void PDFiumEngine::PluginSizeUpdated(const pp::Size& size) {
  CancelPaints();

  plugin_size_ = size;
  CalculateVisiblePages();
}

void PDFiumEngine::ScrolledToXPosition(int position) {
  CancelPaints();

  int old_x = position_.x();
  position_.set_x(position);
  CalculateVisiblePages();
  client_->DidScroll(pp::Point(old_x - position, 0));
  OnSelectionPositionChanged();
}

void PDFiumEngine::ScrolledToYPosition(int position) {
  CancelPaints();

  int old_y = position_.y();
  position_.set_y(position);
  CalculateVisiblePages();
  client_->DidScroll(pp::Point(0, old_y - position));
  OnSelectionPositionChanged();
}

void PDFiumEngine::PrePaint() {
  for (auto& paint : progressive_paints_)
    paint.painted_ = false;
}

void PDFiumEngine::Paint(const pp::Rect& rect,
                         pp::ImageData* image_data,
                         std::vector<pp::Rect>* ready,
                         std::vector<pp::Rect>* pending) {
  DCHECK(image_data);
  DCHECK(ready);
  DCHECK(pending);

  pp::Rect leftover = rect;
  for (size_t i = 0; i < visible_pages_.size(); ++i) {
    int index = visible_pages_[i];
    // Convert the current page's rectangle to screen rectangle.  We do this
    // instead of the reverse (converting the dirty rectangle from screen to
    // page coordinates) because then we'd have to convert back to screen
    // coordinates, and the rounding errors sometime leave pixels dirty or even
    // move the text up or down a pixel when zoomed.
    pp::Rect page_rect_in_screen = GetPageScreenRect(index);
    pp::Rect dirty_in_screen = page_rect_in_screen.Intersect(leftover);
    if (dirty_in_screen.IsEmpty())
      continue;

    // Compute the leftover dirty region. The first page may have blank space
    // above it, in which case we also need to subtract that space from the
    // dirty region.
    if (i == 0) {
      pp::Rect blank_space_in_screen = dirty_in_screen;
      blank_space_in_screen.set_y(0);
      blank_space_in_screen.set_height(dirty_in_screen.y());
      leftover = leftover.Subtract(blank_space_in_screen);
    }
    leftover = leftover.Subtract(dirty_in_screen);

    if (pages_[index]->available()) {
      int progressive = GetProgressiveIndex(index);
      if (progressive != -1) {
        DCHECK_GE(progressive, 0);
        DCHECK_LT(static_cast<size_t>(progressive), progressive_paints_.size());
        if (progressive_paints_[progressive].rect != dirty_in_screen) {
          // The PDFium code can only handle one progressive paint at a time, so
          // queue this up. Previously we used to merge the rects when this
          // happened, but it made scrolling up on complex PDFs very slow since
          // there would be a damaged rect at the top (from scroll) and at the
          // bottom (from toolbar).
          pending->push_back(dirty_in_screen);
          continue;
        }
      }

      if (progressive == -1) {
        progressive = StartPaint(index, dirty_in_screen);
        progressive_paint_timeout_ = kMaxInitialProgressivePaintTime;
      } else {
        progressive_paint_timeout_ = kMaxProgressivePaintTime;
      }

      progressive_paints_[progressive].painted_ = true;
      if (ContinuePaint(progressive, image_data)) {
        FinishPaint(progressive, image_data);
        ready->push_back(dirty_in_screen);
      } else {
        pending->push_back(dirty_in_screen);
      }
    } else {
      PaintUnavailablePage(index, dirty_in_screen, image_data);
      ready->push_back(dirty_in_screen);
    }
  }
}

void PDFiumEngine::PostPaint() {
  for (size_t i = 0; i < progressive_paints_.size(); ++i) {
    if (progressive_paints_[i].painted_)
      continue;

    // This rectangle must have been merged with another one, that's why we
    // weren't asked to paint it. Remove it or otherwise we'll never finish
    // painting.
    FPDF_RenderPage_Close(pages_[progressive_paints_[i].page_index]->GetPage());
    FPDFBitmap_Destroy(progressive_paints_[i].bitmap);
    progressive_paints_.erase(progressive_paints_.begin() + i);
    --i;
  }
}

bool PDFiumEngine::HandleDocumentLoad(const pp::URLLoader& loader) {
  password_tries_remaining_ = kMaxPasswordTries;
  process_when_pending_request_complete_ = true;
  auto loader_wrapper =
      std::make_unique<URLLoaderWrapperImpl>(GetPluginInstance(), loader);
  loader_wrapper->SetResponseHeaders(headers_);

  doc_loader_ = std::make_unique<DocumentLoader>(this);
  if (doc_loader_->Init(std::move(loader_wrapper), url_)) {
    // request initial data.
    doc_loader_->RequestData(0, 1);
    return true;
  }
  return false;
}

pp::Instance* PDFiumEngine::GetPluginInstance() {
  return client_->GetPluginInstance();
}

std::unique_ptr<URLLoaderWrapper> PDFiumEngine::CreateURLLoader() {
  return std::make_unique<URLLoaderWrapperImpl>(GetPluginInstance(),
                                                client_->CreateURLLoader());
}

void PDFiumEngine::AppendPage(PDFEngine* engine, int index) {
  // Unload and delete the blank page before appending.
  pages_[index]->Unload();
  pages_[index]->set_calculated_links(false);
  pp::Size curr_page_size = GetPageSize(index);
  FPDFPage_Delete(doc_, index);
  FPDF_ImportPages(doc_, static_cast<PDFiumEngine*>(engine)->doc(), "1", index);
  pp::Size new_page_size = GetPageSize(index);
  if (curr_page_size != new_page_size)
    LoadPageInfo(true);
  client_->Invalidate(GetPageScreenRect(index));
}

#if defined(PDF_ENABLE_XFA)
void PDFiumEngine::SetScrollPosition(const pp::Point& position) {
  position_ = position;
}
#endif

std::string PDFiumEngine::GetMetadata(const std::string& key) {
  return GetDocumentMetadata(doc(), key);
}

void PDFiumEngine::OnPendingRequestComplete() {
  if (!process_when_pending_request_complete_)
    return;
  if (!fpdf_availability_) {
    file_access_.m_FileLen = doc_loader_->GetDocumentSize();
    fpdf_availability_ = FPDFAvail_Create(&file_availability_, &file_access_);
    DCHECK(fpdf_availability_);
    // Currently engine does not deal efficiently with some non-linearized
    // files.
    // See http://code.google.com/p/chromium/issues/detail?id=59400
    // To improve user experience we download entire file for non-linearized
    // PDF.
    if (FPDFAvail_IsLinearized(fpdf_availability_) != PDF_LINEARIZED) {
      // Wait complete document.
      process_when_pending_request_complete_ = false;
      FPDFAvail_Destroy(fpdf_availability_);
      fpdf_availability_ = nullptr;
      return;
    }
  }

  if (!doc_) {
    LoadDocument();
    return;
  }

  if (pages_.empty()) {
    LoadBody();
    return;
  }

  // LoadDocument() will result in |pending_pages_| being reset so there's no
  // need to run the code below in that case.
  bool update_pages = false;
  std::vector<int> still_pending;
  for (int pending_page : pending_pages_) {
    if (CheckPageAvailable(pending_page, &still_pending)) {
      update_pages = true;
      if (IsPageVisible(pending_page)) {
        client_->NotifyPageBecameVisible(
            pages_[pending_page]->GetPageFeatures());
        client_->Invalidate(GetPageScreenRect(pending_page));
      }
    }
  }
  pending_pages_.swap(still_pending);
  if (update_pages)
    LoadPageInfo(true);
}

void PDFiumEngine::OnNewDataReceived() {
  client_->DocumentLoadProgress(doc_loader_->bytes_received(),
                                doc_loader_->GetDocumentSize());
}

void PDFiumEngine::OnDocumentComplete() {
  if (doc_)
    return FinishLoadingDocument();

  file_access_.m_FileLen = doc_loader_->GetDocumentSize();
  if (!fpdf_availability_) {
    fpdf_availability_ = FPDFAvail_Create(&file_availability_, &file_access_);
    DCHECK(fpdf_availability_);
  }
  LoadDocument();
}

void PDFiumEngine::OnDocumentCanceled() {
  if (visible_pages_.empty())
    client_->DocumentLoadFailed();
  else
    OnDocumentComplete();
}

void PDFiumEngine::CancelBrowserDownload() {
  client_->CancelBrowserDownload();
}

void PDFiumEngine::FinishLoadingDocument() {
  DCHECK(doc_);
  DCHECK(doc_loader_->IsDocumentComplete());

  LoadBody();

  bool need_update = false;
  for (size_t i = 0; i < pages_.size(); ++i) {
    if (pages_[i]->available())
      continue;

    pages_[i]->set_available(true);
    // We still need to call IsPageAvail() even if the whole document is
    // already downloaded.
    FPDFAvail_IsPageAvail(fpdf_availability_, i, &download_hints_);
    need_update = true;
    if (IsPageVisible(i))
      client_->Invalidate(GetPageScreenRect(i));
  }
  if (need_update)
    LoadPageInfo(true);

  if (called_do_document_action_)
    return;
  called_do_document_action_ = true;

  // These can only be called now, as the JS might end up needing a page.
  FORM_DoDocumentJSAction(form_);
  FORM_DoDocumentOpenAction(form_);
  if (most_visible_page_ != -1) {
    FPDF_PAGE new_page = pages_[most_visible_page_]->GetPage();
    FORM_DoPageAAction(new_page, form_, FPDFPAGE_AACTION_OPEN);
  }

  if (doc_) {
    DocumentFeatures document_features;
    document_features.page_count = pages_.size();
    document_features.has_attachments = (FPDFDoc_GetAttachmentCount(doc_) > 0);
    document_features.is_linearized =
        (FPDFAvail_IsLinearized(fpdf_availability_) == PDF_LINEARIZED);
    document_features.is_tagged = FPDFCatalog_IsTagged(doc_);
    document_features.form_type = static_cast<FormType>(FPDF_GetFormType(doc_));
    client_->DocumentLoadComplete(document_features,
                                  doc_loader_->bytes_received());
  }
}

void PDFiumEngine::UnsupportedFeature(int type) {
  std::string feature;
  switch (type) {
#if !defined(PDF_ENABLE_XFA)
    case FPDF_UNSP_DOC_XFAFORM:
      feature = "XFA";
      break;
#endif
    case FPDF_UNSP_DOC_PORTABLECOLLECTION:
      feature = "Portfolios_Packages";
      break;
    case FPDF_UNSP_DOC_ATTACHMENT:
    case FPDF_UNSP_ANNOT_ATTACHMENT:
      feature = "Attachment";
      break;
    case FPDF_UNSP_DOC_SECURITY:
      feature = "Rights_Management";
      break;
    case FPDF_UNSP_DOC_SHAREDREVIEW:
      feature = "Shared_Review";
      break;
    case FPDF_UNSP_DOC_SHAREDFORM_ACROBAT:
    case FPDF_UNSP_DOC_SHAREDFORM_FILESYSTEM:
    case FPDF_UNSP_DOC_SHAREDFORM_EMAIL:
      feature = "Shared_Form";
      break;
    case FPDF_UNSP_ANNOT_3DANNOT:
      feature = "3D";
      break;
    case FPDF_UNSP_ANNOT_MOVIE:
      feature = "Movie";
      break;
    case FPDF_UNSP_ANNOT_SOUND:
      feature = "Sound";
      break;
    case FPDF_UNSP_ANNOT_SCREEN_MEDIA:
    case FPDF_UNSP_ANNOT_SCREEN_RICHMEDIA:
      feature = "Screen";
      break;
    case FPDF_UNSP_ANNOT_SIG:
      feature = "Digital_Signature";
      break;
  }
  client_->DocumentHasUnsupportedFeature(feature);
}

void PDFiumEngine::FontSubstituted() {
  client_->FontSubstituted();
}

void PDFiumEngine::ContinueFind(int32_t result) {
  StartFind(current_find_text_, result != 0);
}

bool PDFiumEngine::HandleEvent(const pp::InputEvent& event) {
  DCHECK(!defer_page_unload_);
  defer_page_unload_ = true;
  bool rv = false;
  switch (event.GetType()) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      rv = OnMouseDown(pp::MouseInputEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_MOUSEUP:
      rv = OnMouseUp(pp::MouseInputEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      rv = OnMouseMove(pp::MouseInputEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
      OnMouseEnter(pp::MouseInputEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      rv = OnKeyDown(pp::KeyboardInputEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_KEYUP:
      rv = OnKeyUp(pp::KeyboardInputEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_CHAR:
      rv = OnChar(pp::KeyboardInputEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_TOUCHSTART: {
      KillTouchTimer(next_touch_timer_id_);

      pp::TouchInputEvent touch_event(event);
      if (touch_event.GetTouchCount(PP_TOUCHLIST_TYPE_TARGETTOUCHES) == 1)
        ScheduleTouchTimer(touch_event);
      break;
    }
    case PP_INPUTEVENT_TYPE_TOUCHEND:
      KillTouchTimer(next_touch_timer_id_);
      break;
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
      // TODO(dsinclair): This should allow a little bit of movement (up to the
      // touch radii) to account for finger jiggle.
      KillTouchTimer(next_touch_timer_id_);
      break;
    default:
      break;
  }

  DCHECK(defer_page_unload_);
  defer_page_unload_ = false;

  // Store the pages to unload away because the act of unloading pages can cause
  // there to be more pages to unload. We leave those extra pages to be unloaded
  // on the next go around.
  std::vector<int> pages_to_unload;
  std::swap(pages_to_unload, deferred_page_unloads_);
  for (int page_index : pages_to_unload)
    pages_[page_index]->Unload();

  return rv;
}

uint32_t PDFiumEngine::QuerySupportedPrintOutputFormats() {
  if (HasPermission(PERMISSION_PRINT_HIGH_QUALITY))
    return PP_PRINTOUTPUTFORMAT_PDF | PP_PRINTOUTPUTFORMAT_RASTER;
  if (HasPermission(PERMISSION_PRINT_LOW_QUALITY))
    return PP_PRINTOUTPUTFORMAT_RASTER;
  return 0;
}

void PDFiumEngine::PrintBegin() {
  FORM_DoDocumentAAction(form_, FPDFDOC_AACTION_WP);
}

pp::Resource PDFiumEngine::PrintPages(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings) {
  ScopedSubstFont scoped_subst_font(this);
  if ((print_settings.format & PP_PRINTOUTPUTFORMAT_PDF) &&
      HasPermission(PERMISSION_PRINT_HIGH_QUALITY)) {
    return PrintPagesAsPDF(page_ranges, page_range_count, print_settings);
  }
  if (HasPermission(PERMISSION_PRINT_LOW_QUALITY))
    return PrintPagesAsRasterPDF(page_ranges, page_range_count, print_settings);
  return pp::Resource();
}

FPDF_DOCUMENT PDFiumEngine::CreateSinglePageRasterPdf(
    double source_page_width,
    double source_page_height,
    const PP_PrintSettings_Dev& print_settings,
    PDFiumPage* page_to_print) {
  FPDF_DOCUMENT temp_doc = FPDF_CreateNewDocument();
  if (!temp_doc)
    return temp_doc;

  const pp::Size& bitmap_size(page_to_print->rect().size());

  pp::ImageData image =
      pp::ImageData(client_->GetPluginInstance(),
                    PP_IMAGEDATAFORMAT_BGRA_PREMUL, bitmap_size, false);

  FPDF_BITMAP bitmap =
      FPDFBitmap_CreateEx(bitmap_size.width(), bitmap_size.height(),
                          FPDFBitmap_BGRx, image.data(), image.stride());

  // Clear the bitmap
  FPDFBitmap_FillRect(bitmap, 0, 0, bitmap_size.width(), bitmap_size.height(),
                      0xFFFFFFFF);

  pp::Rect page_rect = page_to_print->rect();
  FPDF_RenderPageBitmap(bitmap, page_to_print->GetPrintPage(), page_rect.x(),
                        page_rect.y(), page_rect.width(), page_rect.height(),
                        print_settings.orientation,
                        FPDF_PRINTING | FPDF_NO_CATCH);

  // Draw the forms.
  FPDF_FFLDraw(form_, bitmap, page_to_print->GetPrintPage(), page_rect.x(),
               page_rect.y(), page_rect.width(), page_rect.height(),
               print_settings.orientation,
               FPDF_ANNOT | FPDF_PRINTING | FPDF_NO_CATCH);

  unsigned char* bitmap_data =
      static_cast<unsigned char*>(FPDFBitmap_GetBuffer(bitmap));
  double ratio_x = ConvertUnitDouble(bitmap_size.width(), print_settings.dpi,
                                     kPointsPerInch);
  double ratio_y = ConvertUnitDouble(bitmap_size.height(), print_settings.dpi,
                                     kPointsPerInch);

  // Add the bitmap to an image object and add the image object to the output
  // page.
  FPDF_PAGEOBJECT temp_img = FPDFPageObj_NewImageObj(temp_doc);

  bool encoded = false;
  std::vector<uint8_t> compressed_bitmap_data;
  if (!(print_settings.format & PP_PRINTOUTPUTFORMAT_PDF)) {
    // Use quality = 40 as this does not significantly degrade the printed
    // document relative to a normal bitmap and provides better compression than
    // a higher quality setting.
    const int kQuality = 40;
    SkImageInfo info = SkImageInfo::Make(
        FPDFBitmap_GetWidth(bitmap), FPDFBitmap_GetHeight(bitmap),
        kBGRA_8888_SkColorType, kOpaque_SkAlphaType);
    SkPixmap src(info, bitmap_data, FPDFBitmap_GetStride(bitmap));
    encoded = gfx::JPEGCodec::Encode(src, kQuality, &compressed_bitmap_data);
  }

  {
    std::unique_ptr<void, FPDFPageDeleter> temp_page_holder(
        FPDFPage_New(temp_doc, 0, source_page_width, source_page_height));
    FPDF_PAGE temp_page = temp_page_holder.get();
    if (encoded) {
      FPDF_FILEACCESS file_access = {};
      file_access.m_FileLen =
          static_cast<unsigned long>(compressed_bitmap_data.size());
      file_access.m_GetBlock = &GetBlockForJpeg;
      file_access.m_Param = &compressed_bitmap_data;

      FPDFImageObj_LoadJpegFileInline(&temp_page, 1, temp_img, &file_access);
    } else {
      FPDFImageObj_SetBitmap(&temp_page, 1, temp_img, bitmap);
    }

    FPDFImageObj_SetMatrix(temp_img, ratio_x, 0, 0, ratio_y, 0, 0);
    FPDFPage_InsertObject(temp_page, temp_img);
    FPDFPage_GenerateContent(temp_page);
  }

  page_to_print->ClosePrintPage();
  FPDFBitmap_Destroy(bitmap);

  return temp_doc;
}

pp::Buffer_Dev PDFiumEngine::PrintPagesAsRasterPDF(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings) {
  if (!page_range_count)
    return pp::Buffer_Dev();

  // If document is not downloaded yet, disable printing.
  if (doc_ && !doc_loader_->IsDocumentComplete())
    return pp::Buffer_Dev();

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  if (!output_doc)
    return pp::Buffer_Dev();

  KillFormFocus();

  std::vector<PDFiumPage> pages_to_print;
  // width and height of source PDF pages.
  std::vector<std::pair<double, double>> source_page_sizes;
  // Collect pages to print and sizes of source pages.
  std::vector<uint32_t> page_numbers =
      GetPageNumbersFromPrintPageNumberRange(page_ranges, page_range_count);
  for (uint32_t page_number : page_numbers) {
    FPDF_PAGE pdf_page = FPDF_LoadPage(doc_, page_number);
    double source_page_width = FPDF_GetPageWidth(pdf_page);
    double source_page_height = FPDF_GetPageHeight(pdf_page);
    source_page_sizes.push_back(
        std::make_pair(source_page_width, source_page_height));
    // For computing size in pixels, use a square dpi since the source PDF page
    // has square DPI.
    int width_in_pixels =
        ConvertUnit(source_page_width, kPointsPerInch, print_settings.dpi);
    int height_in_pixels =
        ConvertUnit(source_page_height, kPointsPerInch, print_settings.dpi);

    pp::Rect rect(width_in_pixels, height_in_pixels);
    pages_to_print.push_back(PDFiumPage(this, page_number, rect, true));
    FPDF_ClosePage(pdf_page);
  }

#if defined(OS_LINUX)
  g_last_instance_id = client_->GetPluginInstance()->pp_instance();
#endif

  size_t i = 0;
  for (; i < pages_to_print.size(); ++i) {
    double source_page_width = source_page_sizes[i].first;
    double source_page_height = source_page_sizes[i].second;

    // Use temp_doc to compress image by saving PDF to buffer.
    FPDF_DOCUMENT temp_doc =
        CreateSinglePageRasterPdf(source_page_width, source_page_height,
                                  print_settings, &pages_to_print[i]);

    if (!temp_doc)
      break;

    pp::Buffer_Dev buffer = GetFlattenedPrintData(temp_doc);
    FPDF_CloseDocument(temp_doc);

    PDFiumMemBufferFileRead file_read(buffer.data(), buffer.size());
    temp_doc = FPDF_LoadCustomDocument(&file_read, nullptr);

    FPDF_BOOL imported = FPDF_ImportPages(output_doc, temp_doc, "1", i);
    FPDF_CloseDocument(temp_doc);
    if (!imported)
      break;
  }

  pp::Buffer_Dev buffer;
  if (i == pages_to_print.size()) {
    FPDF_CopyViewerPreferences(output_doc, doc_);
    FitContentsToPrintableAreaIfRequired(output_doc, print_settings);
    // Now flatten all the output pages.
    buffer = GetFlattenedPrintData(output_doc);
  }
  FPDF_CloseDocument(output_doc);
  return buffer;
}

pp::Buffer_Dev PDFiumEngine::GetFlattenedPrintData(FPDF_DOCUMENT doc) {
  pp::Buffer_Dev buffer;
  ScopedSubstFont scoped_subst_font(this);
  int page_count = FPDF_GetPageCount(doc);
  for (int i = 0; i < page_count; ++i) {
    FPDF_PAGE page = FPDF_LoadPage(doc, i);
    DCHECK(page);
    int flatten_ret = FPDFPage_Flatten(page, FLAT_PRINT);
    FPDF_ClosePage(page);
    if (flatten_ret == FLATTEN_FAIL)
      return buffer;
  }

  PDFiumMemBufferFileWrite output_file_write;
  if (FPDF_SaveAsCopy(doc, &output_file_write, 0)) {
    size_t size = output_file_write.size();
    buffer = pp::Buffer_Dev(client_->GetPluginInstance(), size);
    if (!buffer.is_null())
      memcpy(buffer.data(), output_file_write.buffer().c_str(), size);
  }
  return buffer;
}

pp::Buffer_Dev PDFiumEngine::PrintPagesAsPDF(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings) {
  if (!page_range_count)
    return pp::Buffer_Dev();

  DCHECK(doc_);
  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  if (!output_doc)
    return pp::Buffer_Dev();

  KillFormFocus();

  std::string page_number_str;
  for (uint32_t index = 0; index < page_range_count; ++index) {
    if (!page_number_str.empty())
      page_number_str.append(",");
    const PP_PrintPageNumberRange_Dev& range = page_ranges[index];
    page_number_str.append(base::UintToString(range.first_page_number + 1));
    if (range.first_page_number != range.last_page_number) {
      page_number_str.append("-");
      page_number_str.append(base::UintToString(range.last_page_number + 1));
    }
  }

  std::vector<uint32_t> page_numbers =
      GetPageNumbersFromPrintPageNumberRange(page_ranges, page_range_count);
  for (uint32_t page_number : page_numbers) {
    pages_[page_number]->GetPage();
    if (!IsPageVisible(page_number))
      pages_[page_number]->Unload();
  }

  FPDF_CopyViewerPreferences(output_doc, doc_);
  if (!FPDF_ImportPages(output_doc, doc_, page_number_str.c_str(), 0)) {
    FPDF_CloseDocument(output_doc);
    return pp::Buffer_Dev();
  }

  FitContentsToPrintableAreaIfRequired(output_doc, print_settings);

  // Now flatten all the output pages.
  pp::Buffer_Dev buffer = GetFlattenedPrintData(output_doc);
  FPDF_CloseDocument(output_doc);
  return buffer;
}

void PDFiumEngine::FitContentsToPrintableAreaIfRequired(
    FPDF_DOCUMENT doc,
    const PP_PrintSettings_Dev& print_settings) {
  // Check to see if we need to fit pdf contents to printer paper size.
  if (print_settings.print_scaling_option !=
      PP_PRINTSCALINGOPTION_SOURCE_SIZE) {
    int num_pages = FPDF_GetPageCount(doc);
    // In-place transformation is more efficient than creating a new
    // transformed document from the source document. Therefore, transform
    // every page to fit the contents in the selected printer paper.
    for (int i = 0; i < num_pages; ++i) {
      FPDF_PAGE page = FPDF_LoadPage(doc, i);
      TransformPDFPageForPrinting(page, print_settings);
      FPDF_ClosePage(page);
    }
  }
}

void PDFiumEngine::KillFormFocus() {
  FORM_ForceToKillFocus(form_);
  SetInFormTextArea(false);
}

void PDFiumEngine::SetFormSelectedText(FPDF_FORMHANDLE form_handle,
                                       FPDF_PAGE page) {
  unsigned long form_sel_text_len =
      FORM_GetSelectedText(form_handle, page, nullptr, 0);

  // If form selected text is empty and there was no previous form text
  // selection, exit early because nothing has changed. When |form_sel_text_len|
  // is 2, that represents a wide string with just a NUL-terminator.
  if (form_sel_text_len <= 2 && selected_form_text_.empty())
    return;

  base::string16 selected_form_text16;
  PDFiumAPIStringBufferSizeInBytesAdapter<base::string16> string_adapter(
      &selected_form_text16, form_sel_text_len, false);
  string_adapter.Close(FORM_GetSelectedText(
      form_handle, page, string_adapter.GetData(), form_sel_text_len));

  // Update previous and current selections, then compare them to check if
  // selection has changed. If so, set plugin text selection.
  std::string selected_form_text = selected_form_text_;
  selected_form_text_ = base::UTF16ToUTF8(selected_form_text16);
  if (selected_form_text != selected_form_text_) {
    DCHECK(in_form_text_area_);
    pp::PDF::SetSelectedText(GetPluginInstance(), selected_form_text_.c_str());
  }
}

void PDFiumEngine::PrintEnd() {
  FORM_DoDocumentAAction(form_, FPDFDOC_AACTION_DP);
}

PDFiumPage::Area PDFiumEngine::GetCharIndex(const pp::Point& point,
                                            int* page_index,
                                            int* char_index,
                                            int* form_type,
                                            PDFiumPage::LinkTarget* target) {
  int page = -1;
  pp::Point point_in_page(
      static_cast<int>((point.x() + position_.x()) / current_zoom_),
      static_cast<int>((point.y() + position_.y()) / current_zoom_));
  for (int visible_page : visible_pages_) {
    if (pages_[visible_page]->rect().Contains(point_in_page)) {
      page = visible_page;
      break;
    }
  }
  if (page == -1)
    return PDFiumPage::NONSELECTABLE_AREA;

  // If the page hasn't finished rendering, calling into the page sometimes
  // leads to hangs.
  for (const auto& paint : progressive_paints_) {
    if (paint.page_index == page)
      return PDFiumPage::NONSELECTABLE_AREA;
  }

  *page_index = page;
  PDFiumPage::Area result = pages_[page]->GetCharIndex(
      point_in_page, current_rotation_, char_index, form_type, target);
  return (client_->IsPrintPreview() && result == PDFiumPage::WEBLINK_AREA)
             ? PDFiumPage::NONSELECTABLE_AREA
             : result;
}

bool PDFiumEngine::OnMouseDown(const pp::MouseInputEvent& event) {
  pp::MouseInputEvent normalized_event =
      NormalizeMouseEvent(client_->GetPluginInstance(), event);
  if (normalized_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_LEFT)
    return OnLeftMouseDown(normalized_event);
  if (normalized_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_MIDDLE)
    return OnMiddleMouseDown(normalized_event);
  if (normalized_event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_RIGHT)
    return OnRightMouseDown(normalized_event);
  return false;
}

void PDFiumEngine::OnSingleClick(int page_index, int char_index) {
  SetSelecting(true);
  selection_.push_back(PDFiumRange(pages_[page_index].get(), char_index, 0));
}

void PDFiumEngine::OnMultipleClick(int click_count,
                                   int page_index,
                                   int char_index) {
  DCHECK_GE(click_count, 2);
  bool is_double_click = click_count == 2;

  // It would be more efficient if the SDK could support finding a space, but
  // now it doesn't.
  int start_index = char_index;
  do {
    base::char16 cur = pages_[page_index]->GetCharAtIndex(start_index);
    if (FindMultipleClickBoundary(is_double_click, cur))
      break;
  } while (--start_index >= 0);
  if (start_index)
    start_index++;

  int end_index = char_index;
  int total = pages_[page_index]->GetCharCount();
  while (end_index++ <= total) {
    base::char16 cur = pages_[page_index]->GetCharAtIndex(end_index);
    if (FindMultipleClickBoundary(is_double_click, cur))
      break;
  }

  selection_.push_back(PDFiumRange(pages_[page_index].get(), start_index,
                                   end_index - start_index));
}

bool PDFiumEngine::OnLeftMouseDown(const pp::MouseInputEvent& event) {
  SetMouseLeftButtonDown(true);

  auto selection_invalidator =
      std::make_unique<SelectionChangeInvalidator>(this);
  selection_.clear();

  int page_index = -1;
  int char_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  pp::Point point = event.GetPosition();
  PDFiumPage::Area area =
      GetCharIndex(point, &page_index, &char_index, &form_type, &target);
  DCHECK_GE(form_type, FPDF_FORMFIELD_UNKNOWN);
  mouse_down_state_.Set(area, target);

  // Decide whether to open link or not based on user action in mouse up and
  // mouse move events.
  if (IsLinkArea(area))
    return true;

  if (page_index != -1) {
    last_page_mouse_down_ = page_index;
    double page_x;
    double page_y;
    DeviceToPage(page_index, point.x(), point.y(), &page_x, &page_y);

    bool is_form_text_area = IsFormTextArea(area, form_type);
    FPDF_PAGE page = pages_[page_index]->GetPage();
    bool is_editable_form_text_area =
        is_form_text_area &&
        IsPointInEditableFormTextArea(page, page_x, page_y, form_type);

    FORM_OnLButtonDown(form_, page, 0, page_x, page_y);
    if (form_type != FPDF_FORMFIELD_UNKNOWN) {
      // Destroy SelectionChangeInvalidator object before SetInFormTextArea()
      // changes plugin's focus to be in form text area. This way, regular text
      // selection can be cleared when a user clicks into a form text area
      // because the pp::PDF::SetSelectedText() call in
      // ~SelectionChangeInvalidator() still goes to the Mimehandler
      // (not the Renderer).
      selection_invalidator.reset();

      SetInFormTextArea(is_form_text_area);
      editable_form_text_area_ = is_editable_form_text_area;
      return true;  // Return now before we get into the selection code.
    }
  }
  SetInFormTextArea(false);

  if (area != PDFiumPage::TEXT_AREA)
    return true;  // Return true so WebKit doesn't do its own highlighting.

  if (event.GetClickCount() == 1)
    OnSingleClick(page_index, char_index);
  else if (event.GetClickCount() == 2 || event.GetClickCount() == 3)
    OnMultipleClick(event.GetClickCount(), page_index, char_index);

  return true;
}

bool PDFiumEngine::OnMiddleMouseDown(const pp::MouseInputEvent& event) {
  SetMouseLeftButtonDown(false);
  mouse_middle_button_down_ = true;
  mouse_middle_button_last_position_ = event.GetPosition();

  SelectionChangeInvalidator selection_invalidator(this);
  selection_.clear();

  int unused_page_index = -1;
  int unused_char_index = -1;
  int unused_form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  PDFiumPage::Area area =
      GetCharIndex(event.GetPosition(), &unused_page_index, &unused_char_index,
                   &unused_form_type, &target);
  mouse_down_state_.Set(area, target);

  // Decide whether to open link or not based on user action in mouse up and
  // mouse move events.
  if (IsLinkArea(area))
    return true;

  if (kViewerImplementedPanning) {
    // Switch to hand cursor when panning.
    client_->UpdateCursor(PP_CURSORTYPE_HAND);
  }

  // Prevent middle mouse button from selecting texts.
  return false;
}

bool PDFiumEngine::OnRightMouseDown(const pp::MouseInputEvent& event) {
  DCHECK_EQ(PP_INPUTEVENT_MOUSEBUTTON_RIGHT, event.GetButton());

  pp::Point point = event.GetPosition();
  int page_index = -1;
  int char_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  PDFiumPage::Area area =
      GetCharIndex(point, &page_index, &char_index, &form_type, &target);
  DCHECK_GE(form_type, FPDF_FORMFIELD_UNKNOWN);

  bool is_form_text_area = IsFormTextArea(area, form_type);
  bool is_editable_form_text_area = false;

  double page_x = -1;
  double page_y = -1;
  FPDF_PAGE page = nullptr;
  if (is_form_text_area) {
    DCHECK_NE(page_index, -1);

    DeviceToPage(page_index, point.x(), point.y(), &page_x, &page_y);
    page = pages_[page_index]->GetPage();
    is_editable_form_text_area =
        IsPointInEditableFormTextArea(page, page_x, page_y, form_type);
  }

  // Handle the case when focus starts inside a form text area.
  if (in_form_text_area_) {
    if (is_form_text_area) {
      FORM_OnFocus(form_, page, 0, page_x, page_y);
    } else {
      // Transition out of a form text area.
      FORM_ForceToKillFocus(form_);
      SetInFormTextArea(false);
    }
    return true;
  }

  // Handle the case when focus starts outside a form text area and transitions
  // into a form text area.
  if (is_form_text_area) {
    {
      SelectionChangeInvalidator selection_invalidator(this);
      selection_.clear();
    }

    SetInFormTextArea(true);
    editable_form_text_area_ = is_editable_form_text_area;
    FORM_OnFocus(form_, page, 0, page_x, page_y);
    return true;
  }

  // Handle the case when focus starts outside a form text area and stays
  // outside.
  if (selection_.empty())
    return false;

  std::vector<pp::Rect> selection_rect_vector;
  GetAllScreenRectsUnion(&selection_, GetVisibleRect().point(),
                         &selection_rect_vector);
  for (const auto& rect : selection_rect_vector) {
    if (rect.Contains(point.x(), point.y()))
      return false;
  }
  SelectionChangeInvalidator selection_invalidator(this);
  selection_.clear();
  return true;
}

bool PDFiumEngine::OnMouseUp(const pp::MouseInputEvent& event) {
  if (event.GetButton() != PP_INPUTEVENT_MOUSEBUTTON_LEFT &&
      event.GetButton() != PP_INPUTEVENT_MOUSEBUTTON_MIDDLE) {
    return false;
  }

  if (event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_LEFT)
    SetMouseLeftButtonDown(false);
  else if (event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_MIDDLE)
    mouse_middle_button_down_ = false;

  int page_index = -1;
  int char_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  pp::Point point = event.GetPosition();
  PDFiumPage::Area area =
      GetCharIndex(point, &page_index, &char_index, &form_type, &target);

  // Open link on mouse up for same link for which mouse down happened earlier.
  if (mouse_down_state_.Matches(area, target)) {
    if (area == PDFiumPage::WEBLINK_AREA) {
      uint32_t modifiers = event.GetModifiers();
      bool middle_button =
          !!(modifiers & PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN);
      bool alt_key = !!(modifiers & PP_INPUTEVENT_MODIFIER_ALTKEY);
      bool ctrl_key = !!(modifiers & PP_INPUTEVENT_MODIFIER_CONTROLKEY);
      bool meta_key = !!(modifiers & PP_INPUTEVENT_MODIFIER_METAKEY);
      bool shift_key = !!(modifiers & PP_INPUTEVENT_MODIFIER_SHIFTKEY);

      WindowOpenDisposition disposition = ui::DispositionFromClick(
          middle_button, alt_key, ctrl_key, meta_key, shift_key);

      client_->NavigateTo(target.url, disposition);
      SetInFormTextArea(false);
      return true;
    }
    if (area == PDFiumPage::DOCLINK_AREA) {
      if (!PageIndexInBounds(target.page))
        return true;

      pp::Rect page_rect(GetPageScreenRect(target.page));
      int y = position_.y() + page_rect.y();
      if (target.y_in_pixels)
        y += target.y_in_pixels.value() * current_zoom_;
      client_->ScrollToY(y, /*compensate_for_toolbar=*/true);
      SetInFormTextArea(false);
      return true;
    }
  }

  if (event.GetButton() == PP_INPUTEVENT_MOUSEBUTTON_MIDDLE) {
    if (kViewerImplementedPanning) {
      // Update the cursor when panning stops.
      client_->UpdateCursor(DetermineCursorType(area, form_type));
    }

    // Prevent middle mouse button from selecting texts.
    return false;
  }

  if (page_index != -1) {
    double page_x;
    double page_y;
    DeviceToPage(page_index, point.x(), point.y(), &page_x, &page_y);
    FORM_OnLButtonUp(form_, pages_[page_index]->GetPage(), 0, page_x, page_y);
  }

  if (!selecting_)
    return false;

  SetSelecting(false);
  return true;
}

bool PDFiumEngine::OnMouseMove(const pp::MouseInputEvent& event) {
  int page_index = -1;
  int char_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  pp::Point point = event.GetPosition();
  PDFiumPage::Area area =
      GetCharIndex(point, &page_index, &char_index, &form_type, &target);

  // Clear |mouse_down_state_| if mouse moves away from where the mouse down
  // happened.
  if (!mouse_down_state_.Matches(area, target))
    mouse_down_state_.Reset();

  if (!selecting_) {
    client_->UpdateCursor(DetermineCursorType(area, form_type));

    if (page_index != -1) {
      double page_x;
      double page_y;
      DeviceToPage(page_index, point.x(), point.y(), &page_x, &page_y);
      FORM_OnMouseMove(form_, pages_[page_index]->GetPage(), 0, page_x, page_y);
    }

    std::string url = GetLinkAtPosition(event.GetPosition());
    if (url != link_under_cursor_) {
      link_under_cursor_ = url;
      pp::PDF::SetLinkUnderCursor(GetPluginInstance(), url.c_str());
    }

    // If in form text area while left mouse button is held down, check if form
    // text selection needs to be updated.
    if (mouse_left_button_down_ && area == PDFiumPage::FORM_TEXT_AREA &&
        last_page_mouse_down_ != -1) {
      SetFormSelectedText(form_, pages_[last_page_mouse_down_]->GetPage());
    }

    if (kViewerImplementedPanning && mouse_middle_button_down_) {
      // Subtract (origin - destination) so delta is already the delta for
      // moving the page, rather than the delta the mouse moved.
      // GetMovement() does not work here, as small mouse movements are
      // considered zero.
      pp::Point page_position_delta =
          mouse_middle_button_last_position_ - event.GetPosition();
      if (page_position_delta.x() != 0 || page_position_delta.y() != 0) {
        client_->ScrollBy(page_position_delta);
        mouse_middle_button_last_position_ = event.GetPosition();
      }
    }

    // No need to swallow the event, since this might interfere with the
    // scrollbars if the user is dragging them.
    return false;
  }

  // We're selecting but right now we're not over text, so don't change the
  // current selection.
  if (area != PDFiumPage::TEXT_AREA && !IsLinkArea(area))
    return false;

  SelectionChangeInvalidator selection_invalidator(this);
  return ExtendSelection(page_index, char_index);
}

PP_CursorType_Dev PDFiumEngine::DetermineCursorType(PDFiumPage::Area area,
                                                    int form_type) const {
  if (kViewerImplementedPanning && mouse_middle_button_down_) {
    return PP_CURSORTYPE_HAND;
  }

  switch (area) {
    case PDFiumPage::TEXT_AREA:
      return PP_CURSORTYPE_IBEAM;
    case PDFiumPage::WEBLINK_AREA:
    case PDFiumPage::DOCLINK_AREA:
      return PP_CURSORTYPE_HAND;
    case PDFiumPage::NONSELECTABLE_AREA:
    case PDFiumPage::FORM_TEXT_AREA:
    default:
      switch (form_type) {
        case FPDF_FORMFIELD_PUSHBUTTON:
        case FPDF_FORMFIELD_CHECKBOX:
        case FPDF_FORMFIELD_RADIOBUTTON:
        case FPDF_FORMFIELD_COMBOBOX:
        case FPDF_FORMFIELD_LISTBOX:
          return PP_CURSORTYPE_HAND;
        case FPDF_FORMFIELD_TEXTFIELD:
          return PP_CURSORTYPE_IBEAM;
#if defined(PDF_ENABLE_XFA)
        case FPDF_FORMFIELD_XFA_CHECKBOX:
        case FPDF_FORMFIELD_XFA_COMBOBOX:
        case FPDF_FORMFIELD_XFA_IMAGEFIELD:
        case FPDF_FORMFIELD_XFA_LISTBOX:
        case FPDF_FORMFIELD_XFA_PUSHBUTTON:
        case FPDF_FORMFIELD_XFA_SIGNATURE:
          return PP_CURSORTYPE_HAND;
        case FPDF_FORMFIELD_XFA_TEXTFIELD:
          return PP_CURSORTYPE_IBEAM;
#endif
        default:
          return PP_CURSORTYPE_POINTER;
      }
  }
}

void PDFiumEngine::OnMouseEnter(const pp::MouseInputEvent& event) {
  if (event.GetModifiers() & PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN) {
    if (!mouse_middle_button_down_) {
      mouse_middle_button_down_ = true;
      mouse_middle_button_last_position_ = event.GetPosition();
    }
  } else {
    if (mouse_middle_button_down_) {
      mouse_middle_button_down_ = false;
    }
  }
}

bool PDFiumEngine::ExtendSelection(int page_index, int char_index) {
  // Check if the user has decreased their selection area and we need to remove
  // pages from |selection_|.
  for (size_t i = 0; i < selection_.size(); ++i) {
    if (selection_[i].page_index() == page_index) {
      // There should be no other pages after this.
      selection_.erase(selection_.begin() + i + 1, selection_.end());
      break;
    }
  }
  if (selection_.empty())
    return false;

  PDFiumRange& last_selection = selection_.back();
  const int last_page_index = last_selection.page_index();
  const int last_char_index = last_selection.char_index();
  if (last_page_index == page_index) {
    // Selecting within a page.
    int count = char_index - last_char_index;
    if (count >= 0) {
      // Selecting forward.
      ++count;
    } else {
      --count;
    }
    last_selection.SetCharCount(count);
  } else if (last_page_index < page_index) {
    // Selecting into the next page.

    // First make sure that there are no gaps in selection, i.e. if mousedown on
    // page one but we only get mousemove over page three, we want page two.
    for (int i = last_page_index + 1; i < page_index; ++i) {
      selection_.push_back(
          PDFiumRange(pages_[i].get(), 0, pages_[i]->GetCharCount()));
    }

    int count = pages_[last_page_index]->GetCharCount();
    last_selection.SetCharCount(count - last_char_index);
    selection_.push_back(PDFiumRange(pages_[page_index].get(), 0, char_index));
  } else {
    // Selecting into the previous page.
    // The selection's char_index is 0-based, so the character count is one
    // more than the index. The character count needs to be negative to
    // indicate a backwards selection.
    last_selection.SetCharCount(-last_char_index - 1);

    // First make sure that there are no gaps in selection, i.e. if mousedown on
    // page three but we only get mousemove over page one, we want page two.
    for (int i = last_page_index - 1; i > page_index; --i) {
      selection_.push_back(
          PDFiumRange(pages_[i].get(), 0, pages_[i]->GetCharCount()));
    }

    int count = pages_[page_index]->GetCharCount();
    selection_.push_back(
        PDFiumRange(pages_[page_index].get(), count, count - char_index));
  }

  return true;
}

bool PDFiumEngine::OnKeyDown(const pp::KeyboardInputEvent& event) {
  if (last_page_mouse_down_ == -1)
    return false;

  bool rv = !!FORM_OnKeyDown(form_, pages_[last_page_mouse_down_]->GetPage(),
                             event.GetKeyCode(), event.GetModifiers());

  if (event.GetKeyCode() == ui::VKEY_BACK ||
      event.GetKeyCode() == ui::VKEY_ESCAPE) {
    // Blink does not send char events for backspace or escape keys, see
    // WebKeyboardEvent::IsCharacterKey() and b/961192 for more information.
    // So just fake one since PDFium uses it.
    std::string str;
    str.push_back(event.GetKeyCode());
    pp::KeyboardInputEvent synthesized(pp::KeyboardInputEvent(
        client_->GetPluginInstance(), PP_INPUTEVENT_TYPE_CHAR,
        event.GetTimeStamp(), event.GetModifiers(), event.GetKeyCode(), str));
    OnChar(synthesized);
  }

  return rv;
}

bool PDFiumEngine::OnKeyUp(const pp::KeyboardInputEvent& event) {
  if (last_page_mouse_down_ == -1)
    return false;

  // Check if form text selection needs to be updated.
  FPDF_PAGE page = pages_[last_page_mouse_down_]->GetPage();
  if (in_form_text_area_)
    SetFormSelectedText(form_, page);

  return !!FORM_OnKeyUp(form_, page, event.GetKeyCode(), event.GetModifiers());
}

bool PDFiumEngine::OnChar(const pp::KeyboardInputEvent& event) {
  if (last_page_mouse_down_ == -1)
    return false;

  base::string16 str = base::UTF8ToUTF16(event.GetCharacterText().AsString());
  return !!FORM_OnChar(form_, pages_[last_page_mouse_down_]->GetPage(), str[0],
                       event.GetModifiers());
}

void PDFiumEngine::StartFind(const std::string& text, bool case_sensitive) {
  // If the caller asks StartFind() to search for no text, then this is an
  // error on the part of the caller. The PPAPI Find_Private interface
  // guarantees it is not empty, so this should never happen.
  DCHECK(!text.empty());

  // If StartFind() gets called before we have any page information (i.e.
  // before the first call to LoadDocument has happened). Handle this case.
  if (pages_.empty())
    return;

  bool first_search = (current_find_text_ != text);
  int character_to_start_searching_from = 0;
  if (first_search) {
    std::vector<PDFiumRange> old_selection = selection_;
    StopFind();
    current_find_text_ = text;

    if (old_selection.empty()) {
      // Start searching from the beginning of the document.
      next_page_to_search_ = 0;
      last_page_to_search_ = pages_.size() - 1;
      last_character_index_to_search_ = -1;
    } else {
      // There's a current selection, so start from it.
      next_page_to_search_ = old_selection[0].page_index();
      last_character_index_to_search_ = old_selection[0].char_index();
      character_to_start_searching_from = old_selection[0].char_index();
      last_page_to_search_ = next_page_to_search_;
    }
  }

  int current_page = next_page_to_search_;

  if (pages_[current_page]->available()) {
    base::string16 str = base::UTF8ToUTF16(text);
    // Don't use PDFium to search for now, since it doesn't support unicode
    // text. Leave the code for now to avoid bit-rot, in case it's fixed later.
    if (0) {
      SearchUsingPDFium(str, case_sensitive, first_search,
                        character_to_start_searching_from, current_page);
    } else {
      SearchUsingICU(str, case_sensitive, first_search,
                     character_to_start_searching_from, current_page);
    }

    if (!IsPageVisible(current_page))
      pages_[current_page]->Unload();
  }

  if (next_page_to_search_ != last_page_to_search_ ||
      (first_search && last_character_index_to_search_ != -1)) {
    ++next_page_to_search_;
  }

  if (next_page_to_search_ == static_cast<int>(pages_.size()))
    next_page_to_search_ = 0;
  // If there's only one page in the document and we start searching midway,
  // then we'll want to search the page one more time.
  bool end_of_search =
      next_page_to_search_ == last_page_to_search_ &&
      // Only one page but didn't start midway.
      ((pages_.size() == 1 && last_character_index_to_search_ == -1) ||
       // Started midway, but only 1 page and we already looped around.
       (pages_.size() == 1 && !first_search) ||
       // Started midway, and we've just looped around.
       (pages_.size() > 1 && current_page == next_page_to_search_));

  if (end_of_search) {
    // Send the final notification.
    client_->NotifyNumberOfFindResultsChanged(find_results_.size(), true);
  } else {
    pp::CompletionCallback callback =
        find_factory_.NewCallback(&PDFiumEngine::ContinueFind);
    pp::Module::Get()->core()->CallOnMainThread(0, callback,
                                                case_sensitive ? 1 : 0);
  }
}

void PDFiumEngine::SearchUsingPDFium(const base::string16& term,
                                     bool case_sensitive,
                                     bool first_search,
                                     int character_to_start_searching_from,
                                     int current_page) {
  // Find all the matches in the current page.
  unsigned long flags = case_sensitive ? FPDF_MATCHCASE : 0;
  FPDF_SCHHANDLE find =
      FPDFText_FindStart(pages_[current_page]->GetTextPage(),
                         reinterpret_cast<const unsigned short*>(term.c_str()),
                         flags, character_to_start_searching_from);

  // Note: since we search one page at a time, we don't find matches across
  // page boundaries.  We could do this manually ourself, but it seems low
  // priority since Reader itself doesn't do it.
  while (FPDFText_FindNext(find)) {
    PDFiumRange result(pages_[current_page].get(),
                       FPDFText_GetSchResultIndex(find),
                       FPDFText_GetSchCount(find));

    if (!first_search && last_character_index_to_search_ != -1 &&
        result.page_index() == last_page_to_search_ &&
        result.char_index() >= last_character_index_to_search_) {
      break;
    }

    AddFindResult(result);
  }

  FPDFText_FindClose(find);
}

void PDFiumEngine::SearchUsingICU(const base::string16& term,
                                  bool case_sensitive,
                                  bool first_search,
                                  int character_to_start_searching_from,
                                  int current_page) {
  DCHECK(!term.empty());

  const int original_text_length = pages_[current_page]->GetCharCount();
  int text_length = original_text_length;
  if (character_to_start_searching_from) {
    text_length -= character_to_start_searching_from;
  } else if (!first_search && last_character_index_to_search_ != -1 &&
             current_page == last_page_to_search_) {
    text_length = last_character_index_to_search_;
  }
  if (text_length <= 0)
    return;

  base::string16 page_text;
  PDFiumAPIStringBufferAdapter<base::string16> api_string_adapter(
      &page_text, text_length, false);
  unsigned short* data =
      reinterpret_cast<unsigned short*>(api_string_adapter.GetData());
  int written =
      FPDFText_GetText(pages_[current_page]->GetTextPage(),
                       character_to_start_searching_from, text_length, data);
  api_string_adapter.Close(written);

  std::vector<PDFEngine::Client::SearchStringResult> results =
      client_->SearchString(page_text.c_str(), term.c_str(), case_sensitive);
  for (const auto& result : results) {
    // Need to map the indexes from the page text, which may have generated
    // characters like space etc, to character indices from the page.
    int text_to_start_searching_from = FPDFText_GetTextIndexFromCharIndex(
        pages_[current_page]->GetTextPage(), character_to_start_searching_from);
    int temp_start = result.start_index + text_to_start_searching_from;
    int start = FPDFText_GetCharIndexFromTextIndex(
        pages_[current_page]->GetTextPage(), temp_start);
    int end = FPDFText_GetCharIndexFromTextIndex(
        pages_[current_page]->GetTextPage(), temp_start + result.length);

    // If |term| occurs at the end of a page, then |end| will be -1 due to the
    // index being out of bounds. Compensate for this case so the range
    // character count calculation below works out.
    if (temp_start + result.length == original_text_length) {
      DCHECK_EQ(-1, end);
      end = original_text_length;
    }
    DCHECK_LT(start, end);
    DCHECK_EQ(term.size(), static_cast<size_t>(end - start));
    AddFindResult(PDFiumRange(pages_[current_page].get(), start, end - start));
  }
}

void PDFiumEngine::AddFindResult(const PDFiumRange& result) {
  // Figure out where to insert the new location, since we could have
  // started searching midway and now we wrapped.
  size_t result_index;
  int page_index = result.page_index();
  int char_index = result.char_index();
  for (result_index = 0; result_index < find_results_.size(); ++result_index) {
    if (find_results_[result_index].page_index() > page_index ||
        (find_results_[result_index].page_index() == page_index &&
         find_results_[result_index].char_index() > char_index)) {
      break;
    }
  }
  find_results_.insert(find_results_.begin() + result_index, result);
  UpdateTickMarks();
  client_->NotifyNumberOfFindResultsChanged(find_results_.size(), false);
}

bool PDFiumEngine::SelectFindResult(bool forward) {
  if (find_results_.empty()) {
    NOTREACHED();
    return false;
  }

  SelectionChangeInvalidator selection_invalidator(this);

  // Move back/forward through the search locations we previously found.
  size_t new_index;
  const size_t last_index = find_results_.size() - 1;

  if (resume_find_index_) {
    new_index = resume_find_index_.value();
    resume_find_index_.reset();
  } else if (current_find_index_) {
    size_t current_index = current_find_index_.value();
    if ((forward && current_index >= last_index) ||
        (!forward && current_index == 0)) {
      current_find_index_.reset();
      client_->NotifySelectedFindResultChanged(-1);
      client_->NotifyNumberOfFindResultsChanged(find_results_.size(), true);
      return true;
    }
    int increment = forward ? 1 : -1;
    new_index = current_index + increment;
  } else {
    new_index = forward ? 0 : last_index;
  }
  current_find_index_ = new_index;

  // Update the selection before telling the client to scroll, since it could
  // paint then.
  selection_.clear();
  selection_.push_back(find_results_[current_find_index_.value()]);

  // If the result is not in view, scroll to it.
  pp::Rect bounding_rect;
  pp::Rect visible_rect = GetVisibleRect();
  // Use zoom of 1.0 since |visible_rect| is without zoom.
  const std::vector<pp::Rect>& rects =
      find_results_[current_find_index_.value()].GetScreenRects(
          pp::Point(), 1.0, current_rotation_);
  for (const auto& rect : rects)
    bounding_rect = bounding_rect.Union(rect);
  if (!visible_rect.Contains(bounding_rect)) {
    pp::Point center = bounding_rect.CenterPoint();
    // Make the page centered.
    int new_y = CalculateCenterForZoom(center.y(), visible_rect.height(),
                                       current_zoom_);
    client_->ScrollToY(new_y, /*compensate_for_toolbar=*/false);

    // Only move horizontally if it's not visible.
    if (center.x() < visible_rect.x() || center.x() > visible_rect.right()) {
      int new_x = CalculateCenterForZoom(center.x(), visible_rect.width(),
                                         current_zoom_);
      client_->ScrollToX(new_x);
    }
  }

  client_->NotifySelectedFindResultChanged(current_find_index_.value());
  client_->NotifyNumberOfFindResultsChanged(find_results_.size(), true);
  return true;
}

void PDFiumEngine::StopFind() {
  SelectionChangeInvalidator selection_invalidator(this);
  selection_.clear();
  selecting_ = false;

  find_results_.clear();
  next_page_to_search_ = -1;
  last_page_to_search_ = -1;
  last_character_index_to_search_ = -1;
  current_find_index_.reset();
  current_find_text_.clear();

  UpdateTickMarks();
  find_factory_.CancelAll();
}

void PDFiumEngine::GetAllScreenRectsUnion(std::vector<PDFiumRange>* rect_range,
                                          const pp::Point& offset_point,
                                          std::vector<pp::Rect>* rect_vector) {
  for (auto& range : *rect_range) {
    pp::Rect result_rect;
    const std::vector<pp::Rect>& rects =
        range.GetScreenRects(offset_point, current_zoom_, current_rotation_);
    for (const auto& rect : rects)
      result_rect = result_rect.Union(rect);
    rect_vector->push_back(result_rect);
  }
}

void PDFiumEngine::UpdateTickMarks() {
  std::vector<pp::Rect> tickmarks;
  GetAllScreenRectsUnion(&find_results_, pp::Point(0, 0), &tickmarks);
  client_->UpdateTickMarks(tickmarks);
}

void PDFiumEngine::ZoomUpdated(double new_zoom_level) {
  CancelPaints();

  current_zoom_ = new_zoom_level;

  CalculateVisiblePages();
  UpdateTickMarks();
}

void PDFiumEngine::RotateClockwise() {
  current_rotation_ = (current_rotation_ + 1) % 4;
  RotateInternal();
}

void PDFiumEngine::RotateCounterclockwise() {
  current_rotation_ = (current_rotation_ - 1) % 4;
  RotateInternal();
}

void PDFiumEngine::InvalidateAllPages() {
  CancelPaints();
  StopFind();
  LoadPageInfo(true);
  client_->Invalidate(pp::Rect(plugin_size_));
}

std::string PDFiumEngine::GetSelectedText() {
  if (!HasPermission(PDFEngine::PERMISSION_COPY))
    return std::string();

  base::string16 result;
  for (size_t i = 0; i < selection_.size(); ++i) {
    static constexpr base::char16 kNewLineChar = L'\n';
    base::string16 current_selection_text = selection_[i].GetText();
    if (i != 0) {
      if (selection_[i - 1].page_index() > selection_[i].page_index())
        std::swap(current_selection_text, result);
      result.push_back(kNewLineChar);
    }
    result.append(current_selection_text);
  }

  FormatStringWithHyphens(&result);
  FormatStringForOS(&result);
  return base::UTF16ToUTF8(result);
}

bool PDFiumEngine::CanEditText() {
  return editable_form_text_area_;
}

void PDFiumEngine::ReplaceSelection(const std::string& text) {
  DCHECK(CanEditText());
  if (last_page_mouse_down_ != -1) {
    base::string16 text_wide = base::UTF8ToUTF16(text);
    FPDF_WIDESTRING text_pdf_wide =
        reinterpret_cast<FPDF_WIDESTRING>(text_wide.c_str());

    FORM_ReplaceSelection(form_, pages_[last_page_mouse_down_]->GetPage(),
                          text_pdf_wide);
  }
}

std::string PDFiumEngine::GetLinkAtPosition(const pp::Point& point) {
  std::string url;
  int temp;
  int page_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  PDFiumPage::Area area =
      GetCharIndex(point, &page_index, &temp, &form_type, &target);
  if (area == PDFiumPage::WEBLINK_AREA)
    url = target.url;
  return url;
}

bool PDFiumEngine::HasPermission(DocumentPermission permission) const {
  // PDF 1.7 spec, section 3.5.2 says: "If the revision number is 2 or greater,
  // the operations to which user access can be controlled are as follows: ..."
  //
  // Thus for revision numbers less than 2, permissions are ignored and this
  // always returns true.
  if (permissions_handler_revision_ < 2)
    return true;

  // Handle high quality printing permission separately for security handler
  // revision 3+. See table 3.20 in the PDF 1.7 spec.
  if (permission == PERMISSION_PRINT_HIGH_QUALITY &&
      permissions_handler_revision_ >= 3) {
    return (permissions_ & kPDFPermissionPrintLowQualityMask) != 0 &&
           (permissions_ & kPDFPermissionPrintHighQualityMask) != 0;
  }

  switch (permission) {
    case PERMISSION_COPY:
      return (permissions_ & kPDFPermissionCopyMask) != 0;
    case PERMISSION_COPY_ACCESSIBLE:
      return (permissions_ & kPDFPermissionCopyAccessibleMask) != 0;
    case PERMISSION_PRINT_LOW_QUALITY:
    case PERMISSION_PRINT_HIGH_QUALITY:
      // With security handler revision 2 rules, check the same bit for high
      // and low quality. See table 3.20 in the PDF 1.7 spec.
      return (permissions_ & kPDFPermissionPrintLowQualityMask) != 0;
    default:
      return true;
  }
}

void PDFiumEngine::SelectAll() {
  if (in_form_text_area_)
    return;

  SelectionChangeInvalidator selection_invalidator(this);

  selection_.clear();
  for (const auto& page : pages_) {
    if (page->available())
      selection_.push_back(PDFiumRange(page.get(), 0, page->GetCharCount()));
  }
}

int PDFiumEngine::GetNumberOfPages() {
  return pages_.size();
}

pp::VarArray PDFiumEngine::GetBookmarks() {
  pp::VarDictionary dict = TraverseBookmarks(nullptr, 0);
  // The root bookmark contains no useful information.
  return pp::VarArray(dict.Get(pp::Var("children")));
}

pp::VarDictionary PDFiumEngine::TraverseBookmarks(FPDF_BOOKMARK bookmark,
                                                  unsigned int depth) {
  pp::VarDictionary dict;
  base::string16 title;
  unsigned long buffer_size = FPDFBookmark_GetTitle(bookmark, nullptr, 0);
  if (buffer_size > 0) {
    PDFiumAPIStringBufferSizeInBytesAdapter<base::string16> api_string_adapter(
        &title, buffer_size, true);
    api_string_adapter.Close(FPDFBookmark_GetTitle(
        bookmark, api_string_adapter.GetData(), buffer_size));
  }
  dict.Set(pp::Var("title"), pp::Var(base::UTF16ToUTF8(title)));

  FPDF_DEST dest = FPDFBookmark_GetDest(doc_, bookmark);
  // Some bookmarks don't have a page to select.
  if (dest) {
    int page_index = FPDFDest_GetDestPageIndex(doc_, dest);
    if (PageIndexInBounds(page_index)) {
      dict.Set(pp::Var("page"), pp::Var(page_index));

      base::Optional<gfx::PointF> xy =
          pages_[page_index]->GetPageXYTarget(dest);
      if (xy)
        dict.Set(pp::Var("y"), pp::Var(static_cast<int>(xy.value().y())));
    }
  } else {
    // Extract URI for bookmarks linking to an external page.
    FPDF_ACTION action = FPDFBookmark_GetAction(bookmark);
    buffer_size = FPDFAction_GetURIPath(doc_, action, nullptr, 0);
    if (buffer_size > 0) {
      std::string uri;
      PDFiumAPIStringBufferAdapter<std::string> api_string_adapter(
          &uri, buffer_size, true);
      api_string_adapter.Close(FPDFAction_GetURIPath(
          doc_, action, api_string_adapter.GetData(), buffer_size));
      dict.Set(pp::Var("uri"), pp::Var(uri));
    }
  }

  pp::VarArray children;

  // Don't trust PDFium to handle circular bookmarks.
  const unsigned int kMaxDepth = 128;
  if (depth < kMaxDepth) {
    int child_index = 0;
    std::set<FPDF_BOOKMARK> seen_bookmarks;
    for (FPDF_BOOKMARK child_bookmark =
             FPDFBookmark_GetFirstChild(doc_, bookmark);
         child_bookmark;
         child_bookmark = FPDFBookmark_GetNextSibling(doc_, child_bookmark)) {
      if (base::ContainsKey(seen_bookmarks, child_bookmark))
        break;

      seen_bookmarks.insert(child_bookmark);
      children.Set(child_index, TraverseBookmarks(child_bookmark, depth + 1));
      child_index++;
    }
  }
  dict.Set(pp::Var("children"), children);
  return dict;
}

base::Optional<PDFEngine::NamedDestination> PDFiumEngine::GetNamedDestination(
    const std::string& destination) {
  // Look for the destination.
  FPDF_DEST dest = FPDF_GetNamedDestByName(doc_, destination.c_str());
  if (!dest) {
    // Look for a bookmark with the same name.
    base::string16 destination_wide = base::UTF8ToUTF16(destination);
    FPDF_WIDESTRING destination_pdf_wide =
        reinterpret_cast<FPDF_WIDESTRING>(destination_wide.c_str());
    FPDF_BOOKMARK bookmark = FPDFBookmark_Find(doc_, destination_pdf_wide);
    if (bookmark)
      dest = FPDFBookmark_GetDest(doc_, bookmark);
  }

  if (!dest)
    return {};

  int page = FPDFDest_GetDestPageIndex(doc_, dest);
  if (page < 0)
    return {};

  PDFEngine::NamedDestination result;
  result.page = page;
  unsigned long view_int =
      FPDFDest_GetView(dest, &result.num_params, result.params);
  result.view = ConvertViewIntToViewString(view_int);
  return result;
}

gfx::PointF PDFiumEngine::TransformPagePoint(int page_index,
                                             const gfx::PointF& page_xy) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->TransformPageToScreenXY(page_xy);
}

int PDFiumEngine::GetMostVisiblePage() {
  if (in_flight_visible_page_)
    return *in_flight_visible_page_;

  // We can call GetMostVisiblePage through a callback from PDFium. We have
  // to defer the page deletion otherwise we could potentially delete the page
  // that originated the calling JS request and destroy the objects that are
  // currently being used.
  base::AutoReset<bool> defer_page_unload_guard(&defer_page_unload_, true);
  CalculateVisiblePages();
  return most_visible_page_;
}

pp::Rect PDFiumEngine::GetPageRect(int index) {
  pp::Rect rc(pages_[index]->rect());
  rc.Inset(-kPageShadowLeft, -kPageShadowTop, -kPageShadowRight,
           -kPageShadowBottom);
  return rc;
}

pp::Rect PDFiumEngine::GetPageBoundsRect(int index) {
  return pages_[index]->rect();
}

pp::Rect PDFiumEngine::GetPageContentsRect(int index) {
  return GetScreenRect(pages_[index]->rect());
}

int PDFiumEngine::GetVerticalScrollbarYPosition() {
  return position_.y();
}

void PDFiumEngine::SetGrayscale(bool grayscale) {
  render_grayscale_ = grayscale;
}

void PDFiumEngine::OnCallback(int id) {
  auto it = formfill_timers_.find(id);
  if (it == formfill_timers_.end())
    return;

  it->second.timer_callback(id);
  it = formfill_timers_.find(id);  // The callback might delete the timer.
  if (it != formfill_timers_.end())
    client_->ScheduleCallback(id, it->second.timer_period);
}

void PDFiumEngine::OnTouchTimerCallback(int id) {
  if (!touch_timers_.count(id))
    return;

  HandleLongPress(touch_timers_[id]);
  KillTouchTimer(id);
}

void PDFiumEngine::HandleLongPress(const pp::TouchInputEvent& event) {
  pp::FloatPoint fp =
      event.GetTouchByIndex(PP_TOUCHLIST_TYPE_TARGETTOUCHES, 0).position();
  pp::Point point;
  point.set_x(fp.x());
  point.set_y(fp.y());

  // Send a fake mouse down to trigger the multi-click selection code.
  pp::MouseInputEvent mouse_event(
      client_->GetPluginInstance(), PP_INPUTEVENT_TYPE_MOUSEDOWN,
      event.GetTimeStamp(), event.GetModifiers(),
      PP_INPUTEVENT_MOUSEBUTTON_LEFT, point, 2, point);

  OnMouseDown(mouse_event);
}

int PDFiumEngine::GetCharCount(int page_index) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetCharCount();
}

pp::FloatRect PDFiumEngine::GetCharBounds(int page_index, int char_index) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetCharBounds(char_index);
}

uint32_t PDFiumEngine::GetCharUnicode(int page_index, int char_index) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetCharUnicode(char_index);
}

void PDFiumEngine::GetTextRunInfo(int page_index,
                                  int start_char_index,
                                  uint32_t* out_len,
                                  double* out_font_size,
                                  pp::FloatRect* out_bounds) {
  DCHECK(PageIndexInBounds(page_index));
  return pages_[page_index]->GetTextRunInfo(start_char_index, out_len,
                                            out_font_size, out_bounds);
}

bool PDFiumEngine::GetPrintScaling() {
  return !!FPDF_VIEWERREF_GetPrintScaling(doc_);
}

int PDFiumEngine::GetCopiesToPrint() {
  return FPDF_VIEWERREF_GetNumCopies(doc_);
}

int PDFiumEngine::GetDuplexType() {
  return static_cast<int>(FPDF_VIEWERREF_GetDuplex(doc_));
}

bool PDFiumEngine::GetPageSizeAndUniformity(pp::Size* size) {
  if (pages_.empty())
    return false;

  pp::Size page_size = GetPageSize(0);
  for (size_t i = 1; i < pages_.size(); ++i) {
    if (page_size != GetPageSize(i))
      return false;
  }

  // Convert |page_size| back to points.
  size->set_width(
      ConvertUnit(page_size.width(), kPixelsPerInch, kPointsPerInch));
  size->set_height(
      ConvertUnit(page_size.height(), kPixelsPerInch, kPointsPerInch));
  return true;
}

void PDFiumEngine::AppendBlankPages(int num_pages) {
  DCHECK_NE(num_pages, 0);

  if (!doc_)
    return;

  selection_.clear();
  pending_pages_.clear();

  // Delete all pages except the first one.
  while (pages_.size() > 1) {
    pages_.pop_back();
    FPDFPage_Delete(doc_, pages_.size());
  }

  // Calculate document size and all page sizes.
  std::vector<pp::Rect> page_rects;
  pp::Size page_size = GetPageSize(0);
  page_size.Enlarge(kPageShadowLeft + kPageShadowRight,
                    kPageShadowTop + kPageShadowBottom);
  pp::Size old_document_size = document_size_;
  document_size_ = pp::Size(page_size.width(), 0);
  for (int i = 0; i < num_pages; ++i) {
    if (i != 0) {
      // Add space for horizontal separator.
      document_size_.Enlarge(0, kPageSeparatorThickness);
    }

    pp::Rect rect(pp::Point(0, document_size_.height()), page_size);
    page_rects.push_back(rect);

    document_size_.Enlarge(0, page_size.height());
  }

  // Create blank pages.
  for (int i = 1; i < num_pages; ++i) {
    pp::Rect page_rect(page_rects[i]);
    page_rect.Inset(kPageShadowLeft, kPageShadowTop, kPageShadowRight,
                    kPageShadowBottom);
    double width_in_points =
        ConvertUnitDouble(page_rect.width(), kPixelsPerInch, kPointsPerInch);
    double height_in_points =
        ConvertUnitDouble(page_rect.height(), kPixelsPerInch, kPointsPerInch);
    // Add a new page to the document, but delete the FPDF_PAGE object.
    FPDF_PAGE temp_page =
        FPDFPage_New(doc_, i, width_in_points, height_in_points);
    FPDF_ClosePage(temp_page);
    pages_.push_back(std::make_unique<PDFiumPage>(this, i, page_rect, true));
  }

  CalculateVisiblePages();
  if (document_size_ != old_document_size)
    client_->DocumentSizeUpdated(document_size_);
}

void PDFiumEngine::LoadDocument() {
  // Check if the document is ready for loading. If it isn't just bail for now,
  // we will call LoadDocument() again later.
  if (!doc_ && !doc_loader_->IsDocumentComplete() &&
      !FPDFAvail_IsDocAvail(fpdf_availability_, &download_hints_)) {
    return;
  }

  // If we're in the middle of getting a password, just return. We will retry
  // loading the document after we get the password anyway.
  if (getting_password_)
    return;

  ScopedUnsupportedFeature scoped_unsupported_feature(this);
  ScopedSubstFont scoped_subst_font(this);
  bool needs_password = false;
  if (TryLoadingDoc(std::string(), &needs_password)) {
    ContinueLoadingDocument(std::string());
    return;
  }
  if (needs_password)
    GetPasswordAndLoad();
  else
    client_->DocumentLoadFailed();
}

bool PDFiumEngine::TryLoadingDoc(const std::string& password,
                                 bool* needs_password) {
  *needs_password = false;
  if (doc_) {
    // This is probably not necessary, because it should have already been
    // called below in the |doc_| initialization path. However, the previous
    // call may have failed, so call it again for good measure.
    FPDFAvail_IsDocAvail(fpdf_availability_, &download_hints_);
    return true;
  }

  const char* password_cstr = nullptr;
  if (!password.empty()) {
    password_cstr = password.c_str();
    password_tries_remaining_--;
  }
  if (doc_loader_->IsDocumentComplete() &&
      !FPDFAvail_IsLinearized(fpdf_availability_)) {
    doc_ = FPDF_LoadCustomDocument(&file_access_, password_cstr);
  } else {
    doc_ = FPDFAvail_GetDocument(fpdf_availability_, password_cstr);
  }
  if (!doc_) {
    if (FPDF_GetLastError() == FPDF_ERR_PASSWORD)
      *needs_password = true;
    return false;
  }

  // Always call FPDFAvail_IsDocAvail() so PDFium initializes internal data
  // structures.
  FPDFAvail_IsDocAvail(fpdf_availability_, &download_hints_);
  return true;
}

void PDFiumEngine::GetPasswordAndLoad() {
  getting_password_ = true;
  DCHECK(!doc_);
  DCHECK_EQ(static_cast<unsigned long>(FPDF_ERR_PASSWORD), FPDF_GetLastError());
  client_->GetDocumentPassword(password_factory_.NewCallbackWithOutput(
      &PDFiumEngine::OnGetPasswordComplete));
}

void PDFiumEngine::OnGetPasswordComplete(int32_t result,
                                         const pp::Var& password) {
  getting_password_ = false;

  std::string password_text;
  if (result == PP_OK && password.is_string())
    password_text = password.AsString();
  ContinueLoadingDocument(password_text);
}

void PDFiumEngine::ContinueLoadingDocument(const std::string& password) {
  ScopedUnsupportedFeature scoped_unsupported_feature(this);
  ScopedSubstFont scoped_subst_font(this);

  bool needs_password = false;
  bool loaded = TryLoadingDoc(password, &needs_password);
  bool password_incorrect = !loaded && needs_password && !password.empty();
  if (password_incorrect && password_tries_remaining_ > 0) {
    GetPasswordAndLoad();
    return;
  }

  if (!doc_) {
    client_->DocumentLoadFailed();
    return;
  }

  if (FPDFDoc_GetPageMode(doc_) == PAGEMODE_USEOUTLINES)
    client_->DocumentHasUnsupportedFeature("Bookmarks");

  permissions_ = FPDF_GetDocPermissions(doc_);
  permissions_handler_revision_ = FPDF_GetSecurityHandlerRevision(doc_);

  LoadBody();

  if (doc_loader_->IsDocumentComplete())
    FinishLoadingDocument();
}

void PDFiumEngine::LoadPageInfo(bool reload) {
  if (!doc_loader_)
    return;
  if (pages_.empty() && reload)
    return;
  pending_pages_.clear();
  pp::Size old_document_size = document_size_;
  document_size_ = pp::Size();
  std::vector<pp::Rect> page_rects;
  int page_count = FPDF_GetPageCount(doc_);
  bool doc_complete = doc_loader_->IsDocumentComplete();
  bool is_linear = FPDFAvail_IsLinearized(fpdf_availability_) == PDF_LINEARIZED;
  for (int i = 0; i < page_count; ++i) {
    if (i != 0) {
      // Add space for horizontal separator.
      document_size_.Enlarge(0, kPageSeparatorThickness);
    }

    // Get page availability. If |reload| == true, then the document has been
    // constructed already. Get page availability flag from already existing
    // PDFiumPage class.
    // If |reload| == false, then the document may not be fully loaded yet.
    bool page_available;
    if (reload) {
      page_available = pages_[i]->available();
    } else if (is_linear) {
      int linear_page_avail =
          FPDFAvail_IsPageAvail(fpdf_availability_, i, &download_hints_);
      page_available = linear_page_avail == PDF_DATA_AVAIL;
    } else {
      page_available = doc_complete;
    }

    pp::Size size = page_available ? GetPageSize(i) : default_page_size_;
    size.Enlarge(kPageShadowLeft + kPageShadowRight,
                 kPageShadowTop + kPageShadowBottom);
    pp::Rect rect(pp::Point(0, document_size_.height()), size);
    page_rects.push_back(rect);

    if (size.width() > document_size_.width())
      document_size_.set_width(size.width());

    document_size_.Enlarge(0, size.height());
  }

  for (int i = 0; i < page_count; ++i) {
    // Center pages relative to the entire document.
    page_rects[i].set_x((document_size_.width() - page_rects[i].width()) / 2);
    pp::Rect page_rect(page_rects[i]);
    page_rect.Inset(kPageShadowLeft, kPageShadowTop, kPageShadowRight,
                    kPageShadowBottom);
    if (reload) {
      pages_[i]->set_rect(page_rect);
    } else {
      // The page is marked as not being available even if |doc_complete| is
      // true because FPDFAvail_IsPageAvail() still has to be called for this
      // page, which will be done in FinishLoadingDocument().
      pages_.push_back(std::make_unique<PDFiumPage>(this, i, page_rect, false));
    }
  }

  CalculateVisiblePages();
  if (document_size_ != old_document_size)
    client_->DocumentSizeUpdated(document_size_);
}

void PDFiumEngine::LoadBody() {
  DCHECK(doc_);
  DCHECK(fpdf_availability_);
  if (doc_loader_->IsDocumentComplete()) {
    LoadForm();
  } else if (FPDFAvail_IsLinearized(fpdf_availability_) == PDF_LINEARIZED &&
             FPDF_GetPageCount(doc_) == 1) {
    // If we have only one page we should load form first, bacause it is may be
    // XFA document. And after loading form the page count and its contents may
    // be changed.
    LoadForm();
    if (form_status_ == PDF_FORM_NOTAVAIL)
      return;
  }
  LoadPages();
}

void PDFiumEngine::LoadPages() {
  if (pages_.empty()) {
    if (!doc_loader_->IsDocumentComplete()) {
      // Check if the first page is available.  In a linearized PDF, that is not
      // always page 0.  Doing this gives us the default page size, since when
      // the document is available, the first page is available as well.
      CheckPageAvailable(FPDFAvail_GetFirstPageNum(doc_), &pending_pages_);
    }
    LoadPageInfo(false);
  }
}

void PDFiumEngine::LoadForm() {
  if (form_)
    return;
  DCHECK(doc_);
  form_status_ = FPDFAvail_IsFormAvail(fpdf_availability_, &download_hints_);
  if (form_status_ != PDF_FORM_NOTAVAIL || doc_loader_->IsDocumentComplete()) {
    form_ = FPDFDOC_InitFormFillEnvironment(
        doc_, static_cast<FPDF_FORMFILLINFO*>(this));
#if defined(PDF_ENABLE_XFA)
    FPDF_LoadXFA(doc_);
#endif

    FPDF_SetFormFieldHighlightColor(form_, FPDF_FORMFIELD_UNKNOWN,
                                    kFormHighlightColor);
    FPDF_SetFormFieldHighlightAlpha(form_, kFormHighlightAlpha);
  }
}  // namespace chrome_pdf

void PDFiumEngine::CalculateVisiblePages() {
  if (!doc_loader_)
    return;
  // Clear pending requests queue, since it may contain requests to the pages
  // that are already invisible (after scrolling for example).
  pending_pages_.clear();
  doc_loader_->ClearPendingRequests();

  std::vector<int> formerly_visible_pages;
  std::swap(visible_pages_, formerly_visible_pages);

  pp::Rect visible_rect(plugin_size_);
  for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
    // Check an entire PageScreenRect, since we might need to repaint side
    // borders and shadows even if the page itself is not visible.
    // For example, when user use pdf with different page sizes and zoomed in
    // outside page area.
    if (visible_rect.Intersects(GetPageScreenRect(i))) {
      visible_pages_.push_back(i);
      if (CheckPageAvailable(i, &pending_pages_)) {
        auto it = std::find(formerly_visible_pages.begin(),
                            formerly_visible_pages.end(), i);
        if (it == formerly_visible_pages.end())
          client_->NotifyPageBecameVisible(pages_[i]->GetPageFeatures());
      }
    } else {
      // Need to unload pages when we're not using them, since some PDFs use a
      // lot of memory.  See http://crbug.com/48791
      if (defer_page_unload_) {
        deferred_page_unloads_.push_back(i);
      } else {
        pages_[i]->Unload();
      }

      // If the last mouse down was on a page that's no longer visible, reset
      // that variable so that we don't send keyboard events to it (the focus
      // will be lost when the page is first closed anyways).
      if (static_cast<int>(i) == last_page_mouse_down_)
        last_page_mouse_down_ = -1;
    }
  }

  // Any pending highlighting of form fields will be invalid since these are in
  // screen coordinates.
  form_highlights_.clear();

  int most_visible_page = visible_pages_.empty() ? -1 : visible_pages_.front();
  // Check if the next page is more visible than the first one.
  if (most_visible_page != -1 && !pages_.empty() &&
      most_visible_page < static_cast<int>(pages_.size()) - 1) {
    pp::Rect rc_first =
        visible_rect.Intersect(GetPageScreenRect(most_visible_page));
    pp::Rect rc_next =
        visible_rect.Intersect(GetPageScreenRect(most_visible_page + 1));
    if (rc_next.height() > rc_first.height())
      most_visible_page++;
  }

  SetCurrentPage(most_visible_page);
}

bool PDFiumEngine::IsPageVisible(int index) const {
  return base::ContainsValue(visible_pages_, index);
}

void PDFiumEngine::ScrollToPage(int page) {
  if (!PageIndexInBounds(page))
    return;

  in_flight_visible_page_ = page;
  client_->ScrollToPage(page);
}

bool PDFiumEngine::CheckPageAvailable(int index, std::vector<int>* pending) {
  if (!doc_)
    return false;

  const int num_pages = static_cast<int>(pages_.size());
  if (index < num_pages && pages_[index]->available())
    return true;

  if (!FPDFAvail_IsPageAvail(fpdf_availability_, index, &download_hints_)) {
    if (!base::ContainsValue(*pending, index))
      pending->push_back(index);
    return false;
  }

  if (index < num_pages)
    pages_[index]->set_available(true);
  if (default_page_size_.IsEmpty())
    default_page_size_ = GetPageSize(index);
  return true;
}

pp::Size PDFiumEngine::GetPageSize(int index) {
  pp::Size size;
  double width_in_points = 0;
  double height_in_points = 0;
  int rv =
      FPDF_GetPageSizeByIndex(doc_, index, &width_in_points, &height_in_points);

  if (rv) {
    int width_in_pixels = static_cast<int>(
        ConvertUnitDouble(width_in_points, kPointsPerInch, kPixelsPerInch));
    int height_in_pixels = static_cast<int>(
        ConvertUnitDouble(height_in_points, kPointsPerInch, kPixelsPerInch));
    if (current_rotation_ % 2 == 1)
      std::swap(width_in_pixels, height_in_pixels);
    size = pp::Size(width_in_pixels, height_in_pixels);
  }
  return size;
}

int PDFiumEngine::StartPaint(int page_index, const pp::Rect& dirty) {
  // For the first time we hit paint, do nothing and just record the paint for
  // the next callback.  This keeps the UI responsive in case the user is doing
  // a lot of scrolling.
  ProgressivePaint progressive;
  progressive.rect = dirty;
  progressive.page_index = page_index;
  progressive.bitmap = nullptr;
  progressive.painted_ = false;
  progressive_paints_.push_back(progressive);
  return progressive_paints_.size() - 1;
}

bool PDFiumEngine::ContinuePaint(int progressive_index,
                                 pp::ImageData* image_data) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());
  DCHECK(image_data);

#if defined(OS_LINUX)
  g_last_instance_id = client_->GetPluginInstance()->pp_instance();
#endif

  int rv;
  FPDF_BITMAP bitmap = progressive_paints_[progressive_index].bitmap;
  int page_index = progressive_paints_[progressive_index].page_index;
  DCHECK(PageIndexInBounds(page_index));
  FPDF_PAGE page = pages_[page_index]->GetPage();

  last_progressive_start_time_ = base::Time::Now();
  if (bitmap) {
    rv = FPDF_RenderPage_Continue(page, static_cast<IFSDK_PAUSE*>(this));
  } else {
    pp::Rect dirty = progressive_paints_[progressive_index].rect;
    bitmap = CreateBitmap(dirty, image_data);
    int start_x, start_y, size_x, size_y;
    GetPDFiumRect(page_index, dirty, &start_x, &start_y, &size_x, &size_y);
    FPDFBitmap_FillRect(bitmap, start_x, start_y, size_x, size_y, 0xFFFFFFFF);
    rv = FPDF_RenderPageBitmap_Start(
        bitmap, page, start_x, start_y, size_x, size_y, current_rotation_,
        GetRenderingFlags(), static_cast<IFSDK_PAUSE*>(this));
    progressive_paints_[progressive_index].bitmap = bitmap;
  }
  return rv != FPDF_RENDER_TOBECOUNTINUED;
}

void PDFiumEngine::FinishPaint(int progressive_index,
                               pp::ImageData* image_data) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());
  DCHECK(image_data);

  int page_index = progressive_paints_[progressive_index].page_index;
  const pp::Rect& dirty_in_screen = progressive_paints_[progressive_index].rect;
  FPDF_BITMAP bitmap = progressive_paints_[progressive_index].bitmap;
  int start_x, start_y, size_x, size_y;
  GetPDFiumRect(page_index, dirty_in_screen, &start_x, &start_y, &size_x,
                &size_y);

  // Draw the forms.
  FPDF_FFLDraw(form_, bitmap, pages_[page_index]->GetPage(), start_x, start_y,
               size_x, size_y, current_rotation_, GetRenderingFlags());

  FillPageSides(progressive_index);

  // Paint the page shadows.
  PaintPageShadow(progressive_index, image_data);

  DrawSelections(progressive_index, image_data);

  FPDF_RenderPage_Close(pages_[page_index]->GetPage());
  FPDFBitmap_Destroy(bitmap);
  progressive_paints_.erase(progressive_paints_.begin() + progressive_index);

  client_->DocumentPaintOccurred();
}

void PDFiumEngine::CancelPaints() {
  for (const auto& paint : progressive_paints_) {
    FPDF_RenderPage_Close(pages_[paint.page_index]->GetPage());
    FPDFBitmap_Destroy(paint.bitmap);
  }
  progressive_paints_.clear();
}

void PDFiumEngine::FillPageSides(int progressive_index) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());

  int page_index = progressive_paints_[progressive_index].page_index;
  const pp::Rect& dirty_in_screen = progressive_paints_[progressive_index].rect;
  FPDF_BITMAP bitmap = progressive_paints_[progressive_index].bitmap;

  pp::Rect page_rect = pages_[page_index]->rect();
  if (page_rect.x() > 0) {
    pp::Rect left(0, page_rect.y() - kPageShadowTop,
                  page_rect.x() - kPageShadowLeft,
                  page_rect.height() + kPageShadowTop + kPageShadowBottom +
                      kPageSeparatorThickness);
    left = GetScreenRect(left).Intersect(dirty_in_screen);

    FPDFBitmap_FillRect(bitmap, left.x() - dirty_in_screen.x(),
                        left.y() - dirty_in_screen.y(), left.width(),
                        left.height(), client_->GetBackgroundColor());
  }

  if (page_rect.right() < document_size_.width()) {
    pp::Rect right(
        page_rect.right() + kPageShadowRight, page_rect.y() - kPageShadowTop,
        document_size_.width() - page_rect.right() - kPageShadowRight,
        page_rect.height() + kPageShadowTop + kPageShadowBottom +
            kPageSeparatorThickness);
    right = GetScreenRect(right).Intersect(dirty_in_screen);

    FPDFBitmap_FillRect(bitmap, right.x() - dirty_in_screen.x(),
                        right.y() - dirty_in_screen.y(), right.width(),
                        right.height(), client_->GetBackgroundColor());
  }

  // Paint separator.
  pp::Rect bottom(page_rect.x() - kPageShadowLeft,
                  page_rect.bottom() + kPageShadowBottom,
                  page_rect.width() + kPageShadowLeft + kPageShadowRight,
                  kPageSeparatorThickness);
  bottom = GetScreenRect(bottom).Intersect(dirty_in_screen);

  FPDFBitmap_FillRect(bitmap, bottom.x() - dirty_in_screen.x(),
                      bottom.y() - dirty_in_screen.y(), bottom.width(),
                      bottom.height(), client_->GetBackgroundColor());
}

void PDFiumEngine::PaintPageShadow(int progressive_index,
                                   pp::ImageData* image_data) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());
  DCHECK(image_data);

  int page_index = progressive_paints_[progressive_index].page_index;
  const pp::Rect& dirty_in_screen = progressive_paints_[progressive_index].rect;
  pp::Rect page_rect = pages_[page_index]->rect();
  pp::Rect shadow_rect(page_rect);
  shadow_rect.Inset(-kPageShadowLeft, -kPageShadowTop, -kPageShadowRight,
                    -kPageShadowBottom);

  // Due to the rounding errors of the GetScreenRect it is possible to get
  // different size shadows on the left and right sides even they are defined
  // the same. To fix this issue let's calculate shadow rect and then shrink
  // it by the size of the shadows.
  shadow_rect = GetScreenRect(shadow_rect);
  page_rect = shadow_rect;

  page_rect.Inset(static_cast<int>(ceil(kPageShadowLeft * current_zoom_)),
                  static_cast<int>(ceil(kPageShadowTop * current_zoom_)),
                  static_cast<int>(ceil(kPageShadowRight * current_zoom_)),
                  static_cast<int>(ceil(kPageShadowBottom * current_zoom_)));

  DrawPageShadow(page_rect, shadow_rect, dirty_in_screen, image_data);
}

void PDFiumEngine::DrawSelections(int progressive_index,
                                  pp::ImageData* image_data) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());
  DCHECK(image_data);

  int page_index = progressive_paints_[progressive_index].page_index;
  const pp::Rect& dirty_in_screen = progressive_paints_[progressive_index].rect;

  void* region = nullptr;
  int stride;
  GetRegion(dirty_in_screen.point(), image_data, &region, &stride);

  std::vector<pp::Rect> highlighted_rects;
  pp::Rect visible_rect = GetVisibleRect();
  for (auto& range : selection_) {
    if (range.page_index() != page_index)
      continue;

    const std::vector<pp::Rect>& rects = range.GetScreenRects(
        visible_rect.point(), current_zoom_, current_rotation_);
    for (const auto& rect : rects) {
      pp::Rect visible_selection = rect.Intersect(dirty_in_screen);
      if (visible_selection.IsEmpty())
        continue;

      visible_selection.Offset(-dirty_in_screen.point().x(),
                               -dirty_in_screen.point().y());
      Highlight(region, stride, visible_selection, &highlighted_rects);
    }
  }

  for (const auto& highlight : form_highlights_) {
    pp::Rect visible_selection = highlight.Intersect(dirty_in_screen);
    if (visible_selection.IsEmpty())
      continue;

    visible_selection.Offset(-dirty_in_screen.point().x(),
                             -dirty_in_screen.point().y());
    Highlight(region, stride, visible_selection, &highlighted_rects);
  }
  form_highlights_.clear();
}

void PDFiumEngine::PaintUnavailablePage(int page_index,
                                        const pp::Rect& dirty,
                                        pp::ImageData* image_data) {
  int start_x, start_y, size_x, size_y;
  GetPDFiumRect(page_index, dirty, &start_x, &start_y, &size_x, &size_y);
  FPDF_BITMAP bitmap = CreateBitmap(dirty, image_data);
  FPDFBitmap_FillRect(bitmap, start_x, start_y, size_x, size_y,
                      kPendingPageColor);

  pp::Rect loading_text_in_screen(
      pages_[page_index]->rect().width() / 2,
      pages_[page_index]->rect().y() + kLoadingTextVerticalOffset, 0, 0);
  loading_text_in_screen = GetScreenRect(loading_text_in_screen);
  FPDFBitmap_Destroy(bitmap);
}

int PDFiumEngine::GetProgressiveIndex(int page_index) const {
  for (size_t i = 0; i < progressive_paints_.size(); ++i) {
    if (progressive_paints_[i].page_index == page_index)
      return i;
  }
  return -1;
}

FPDF_BITMAP PDFiumEngine::CreateBitmap(const pp::Rect& rect,
                                       pp::ImageData* image_data) const {
  void* region;
  int stride;
  GetRegion(rect.point(), image_data, &region, &stride);
  if (!region)
    return nullptr;
  return FPDFBitmap_CreateEx(rect.width(), rect.height(), FPDFBitmap_BGRx,
                             region, stride);
}

void PDFiumEngine::GetPDFiumRect(int page_index,
                                 const pp::Rect& rect,
                                 int* start_x,
                                 int* start_y,
                                 int* size_x,
                                 int* size_y) const {
  pp::Rect page_rect = GetScreenRect(pages_[page_index]->rect());
  page_rect.Offset(-rect.x(), -rect.y());

  *start_x = page_rect.x();
  *start_y = page_rect.y();
  *size_x = page_rect.width();
  *size_y = page_rect.height();
}

int PDFiumEngine::GetRenderingFlags() const {
  int flags = FPDF_LCD_TEXT | FPDF_NO_CATCH;
  if (render_grayscale_)
    flags |= FPDF_GRAYSCALE;
  if (client_->IsPrintPreview())
    flags |= FPDF_PRINTING;
  if (render_annots_)
    flags |= FPDF_ANNOT;
  return flags;
}

pp::Rect PDFiumEngine::GetVisibleRect() const {
  pp::Rect rv;
  rv.set_x(static_cast<int>(position_.x() / current_zoom_));
  rv.set_y(static_cast<int>(position_.y() / current_zoom_));
  rv.set_width(static_cast<int>(ceil(plugin_size_.width() / current_zoom_)));
  rv.set_height(static_cast<int>(ceil(plugin_size_.height() / current_zoom_)));
  return rv;
}

pp::Rect PDFiumEngine::GetPageScreenRect(int page_index) const {
  // Since we use this rect for creating the PDFium bitmap, also include other
  // areas around the page that we might need to update such as the page
  // separator and the sides if the page is narrower than the document.
  return GetScreenRect(
      pp::Rect(0, pages_[page_index]->rect().y() - kPageShadowTop,
               document_size_.width(),
               pages_[page_index]->rect().height() + kPageShadowTop +
                   kPageShadowBottom + kPageSeparatorThickness));
}

pp::Rect PDFiumEngine::GetScreenRect(const pp::Rect& rect) const {
  pp::Rect rv;
  int right =
      static_cast<int>(ceil(rect.right() * current_zoom_ - position_.x()));
  int bottom =
      static_cast<int>(ceil(rect.bottom() * current_zoom_ - position_.y()));

  rv.set_x(static_cast<int>(rect.x() * current_zoom_ - position_.x()));
  rv.set_y(static_cast<int>(rect.y() * current_zoom_ - position_.y()));
  rv.set_width(right - rv.x());
  rv.set_height(bottom - rv.y());
  return rv;
}

void PDFiumEngine::Highlight(void* buffer,
                             int stride,
                             const pp::Rect& rect,
                             std::vector<pp::Rect>* highlighted_rects) {
  if (!buffer)
    return;

  pp::Rect new_rect = rect;
  for (const auto& highlighted : *highlighted_rects)
    new_rect = new_rect.Subtract(highlighted);
  if (new_rect.IsEmpty())
    return;

  std::vector<size_t> overlapping_rect_indices;
  for (size_t i = 0; i < highlighted_rects->size(); ++i) {
    if (new_rect.Intersects((*highlighted_rects)[i]))
      overlapping_rect_indices.push_back(i);
  }

  highlighted_rects->push_back(new_rect);
  int l = new_rect.x();
  int t = new_rect.y();
  int w = new_rect.width();
  int h = new_rect.height();

  for (int y = t; y < t + h; ++y) {
    for (int x = l; x < l + w; ++x) {
      bool overlaps = false;
      for (size_t i : overlapping_rect_indices) {
        const auto& highlighted = (*highlighted_rects)[i];
        if (highlighted.Contains(x, y)) {
          overlaps = true;
          break;
        }
      }
      if (overlaps)
        continue;

      uint8_t* pixel = static_cast<uint8_t*>(buffer) + y * stride + x * 4;
      pixel[0] = static_cast<uint8_t>(pixel[0] * (kHighlightColorB / 255.0));
      pixel[1] = static_cast<uint8_t>(pixel[1] * (kHighlightColorG / 255.0));
      pixel[2] = static_cast<uint8_t>(pixel[2] * (kHighlightColorR / 255.0));
    }
  }
}

PDFiumEngine::SelectionChangeInvalidator::SelectionChangeInvalidator(
    PDFiumEngine* engine)
    : engine_(engine),
      previous_origin_(engine_->GetVisibleRect().point()),
      old_selections_(GetVisibleSelections()) {}

PDFiumEngine::SelectionChangeInvalidator::~SelectionChangeInvalidator() {
  // Offset the old selections if the document scrolled since we recorded them.
  pp::Point offset = previous_origin_ - engine_->GetVisibleRect().point();
  for (auto& old_selection : old_selections_)
    old_selection.Offset(offset);

  std::vector<pp::Rect> new_selections = GetVisibleSelections();
  for (auto& new_selection : new_selections) {
    for (auto& old_selection : old_selections_) {
      if (!old_selection.IsEmpty() && new_selection == old_selection) {
        // Rectangle was selected before and after, so no need to invalidate it.
        // Mark the rectangles by setting them to empty.
        new_selection = old_selection = pp::Rect();
        break;
      }
    }
  }

  bool selection_changed = false;
  for (const auto& old_selection : old_selections_) {
    if (!old_selection.IsEmpty()) {
      Invalidate(old_selection);
      selection_changed = true;
    }
  }
  for (const auto& new_selection : new_selections) {
    if (!new_selection.IsEmpty()) {
      Invalidate(new_selection);
      selection_changed = true;
    }
  }

  if (selection_changed) {
    engine_->OnSelectionTextChanged();
    engine_->OnSelectionPositionChanged();
  }
}

std::vector<pp::Rect>
PDFiumEngine::SelectionChangeInvalidator::GetVisibleSelections() const {
  std::vector<pp::Rect> rects;
  pp::Point visible_point = engine_->GetVisibleRect().point();
  for (auto& range : engine_->selection_) {
    // Exclude selections on pages that's not currently visible.
    if (!engine_->IsPageVisible(range.page_index()))
      continue;

    const std::vector<pp::Rect>& selection_rects = range.GetScreenRects(
        visible_point, engine_->current_zoom_, engine_->current_rotation_);
    rects.insert(rects.end(), selection_rects.begin(), selection_rects.end());
  }
  return rects;
}

void PDFiumEngine::SelectionChangeInvalidator::Invalidate(
    const pp::Rect& selection) {
  pp::Rect expanded_selection = selection;
  expanded_selection.Inset(-1, -1);
  engine_->client_->Invalidate(expanded_selection);
}

PDFiumEngine::MouseDownState::MouseDownState(
    const PDFiumPage::Area& area,
    const PDFiumPage::LinkTarget& target)
    : area_(area), target_(target) {}

PDFiumEngine::MouseDownState::~MouseDownState() = default;

void PDFiumEngine::MouseDownState::Set(const PDFiumPage::Area& area,
                                       const PDFiumPage::LinkTarget& target) {
  area_ = area;
  target_ = target;
}

void PDFiumEngine::MouseDownState::Reset() {
  area_ = PDFiumPage::NONSELECTABLE_AREA;
  target_ = PDFiumPage::LinkTarget();
}

bool PDFiumEngine::MouseDownState::Matches(
    const PDFiumPage::Area& area,
    const PDFiumPage::LinkTarget& target) const {
  if (area_ != area)
    return false;

  if (area == PDFiumPage::WEBLINK_AREA)
    return target_.url == target.url;

  if (area == PDFiumPage::DOCLINK_AREA)
    return target_.page == target.page;

  return true;
}

void PDFiumEngine::DeviceToPage(int page_index,
                                float device_x,
                                float device_y,
                                double* page_x,
                                double* page_y) {
  *page_x = *page_y = 0;
  int temp_x = static_cast<int>((device_x + position_.x()) / current_zoom_ -
                                pages_[page_index]->rect().x());
  int temp_y = static_cast<int>((device_y + position_.y()) / current_zoom_ -
                                pages_[page_index]->rect().y());
  FPDF_DeviceToPage(pages_[page_index]->GetPage(), 0, 0,
                    pages_[page_index]->rect().width(),
                    pages_[page_index]->rect().height(), current_rotation_,
                    temp_x, temp_y, page_x, page_y);
}

int PDFiumEngine::GetVisiblePageIndex(FPDF_PAGE page) {
  // Copy visible_pages_ since it can change as a result of loading the page in
  // GetPage(). See https://crbug.com/822091.
  std::vector<int> visible_pages_copy(visible_pages_);
  for (int page_index : visible_pages_copy) {
    if (pages_[page_index]->GetPage() == page)
      return page_index;
  }
  return -1;
}

void PDFiumEngine::SetCurrentPage(int index) {
  in_flight_visible_page_.reset();

  if (index == most_visible_page_ || !form_)
    return;

  if (most_visible_page_ != -1 && called_do_document_action_) {
    FPDF_PAGE old_page = pages_[most_visible_page_]->GetPage();
    FORM_DoPageAAction(old_page, form_, FPDFPAGE_AACTION_CLOSE);
  }
  most_visible_page_ = index;
#if defined(OS_LINUX)
  g_last_instance_id = client_->GetPluginInstance()->pp_instance();
#endif
  if (most_visible_page_ != -1 && called_do_document_action_) {
    FPDF_PAGE new_page = pages_[most_visible_page_]->GetPage();
    FORM_DoPageAAction(new_page, form_, FPDFPAGE_AACTION_OPEN);
  }
}

void PDFiumEngine::TransformPDFPageForPrinting(
    FPDF_PAGE page,
    const PP_PrintSettings_Dev& print_settings) {
  // Get the source page width and height in points.
  const double src_page_width = FPDF_GetPageWidth(page);
  const double src_page_height = FPDF_GetPageHeight(page);

  const int src_page_rotation = FPDFPage_GetRotation(page);
  const bool fit_to_page = print_settings.print_scaling_option ==
                           PP_PRINTSCALINGOPTION_FIT_TO_PRINTABLE_AREA;

  pp::Size page_size(print_settings.paper_size);
  pp::Rect content_rect(print_settings.printable_area);
  const bool rotated = (src_page_rotation % 2 == 1);
  SetPageSizeAndContentRect(rotated, src_page_width > src_page_height,
                            &page_size, &content_rect);

  // Compute the screen page width and height in points.
  const int actual_page_width =
      rotated ? page_size.height() : page_size.width();
  const int actual_page_height =
      rotated ? page_size.width() : page_size.height();

  const gfx::Rect gfx_content_rect(content_rect.x(), content_rect.y(),
                                   content_rect.width(), content_rect.height());
  const double scale_factor =
      fit_to_page ? CalculateScaleFactor(gfx_content_rect, src_page_width,
                                         src_page_height, rotated)
                  : 1.0;

  // Calculate positions for the clip box.
  PdfRectangle media_box;
  PdfRectangle crop_box;
  bool has_media_box =
      !!FPDFPage_GetMediaBox(page, &media_box.left, &media_box.bottom,
                             &media_box.right, &media_box.top);
  bool has_crop_box = !!FPDFPage_GetCropBox(
      page, &crop_box.left, &crop_box.bottom, &crop_box.right, &crop_box.top);
  CalculateMediaBoxAndCropBox(rotated, has_media_box, has_crop_box, &media_box,
                              &crop_box);
  PdfRectangle source_clip_box = CalculateClipBoxBoundary(media_box, crop_box);
  ScalePdfRectangle(scale_factor, &source_clip_box);

  // Calculate the translation offset values.
  double offset_x = 0;
  double offset_y = 0;
  if (fit_to_page) {
    CalculateScaledClipBoxOffset(gfx_content_rect, source_clip_box, &offset_x,
                                 &offset_y);
  } else {
    CalculateNonScaledClipBoxOffset(gfx_content_rect, src_page_rotation,
                                    actual_page_width, actual_page_height,
                                    source_clip_box, &offset_x, &offset_y);
  }

  // Reset the media box and crop box. When the page has crop box and media box,
  // the plugin will display the crop box contents and not the entire media box.
  // If the pages have different crop box values, the plugin will display a
  // document of multiple page sizes. To give better user experience, we
  // decided to have same crop box and media box values. Hence, the user will
  // see a list of uniform pages.
  FPDFPage_SetMediaBox(page, 0, 0, page_size.width(), page_size.height());
  FPDFPage_SetCropBox(page, 0, 0, page_size.width(), page_size.height());

  // Transformation is not required, return. Do this check only after updating
  // the media box and crop box. For more detailed information, please refer to
  // the comment block right before FPDF_SetMediaBox and FPDF_GetMediaBox calls.
  if (scale_factor == 1.0 && offset_x == 0 && offset_y == 0)
    return;

  // All the positions have been calculated, now manipulate the PDF.
  FS_MATRIX matrix = {static_cast<float>(scale_factor),
                      0,
                      0,
                      static_cast<float>(scale_factor),
                      static_cast<float>(offset_x),
                      static_cast<float>(offset_y)};
  FS_RECTF cliprect = {static_cast<float>(source_clip_box.left + offset_x),
                       static_cast<float>(source_clip_box.top + offset_y),
                       static_cast<float>(source_clip_box.right + offset_x),
                       static_cast<float>(source_clip_box.bottom + offset_y)};
  FPDFPage_TransFormWithClip(page, &matrix, &cliprect);
  FPDFPage_TransformAnnots(page, scale_factor, 0, 0, scale_factor, offset_x,
                           offset_y);
}

void PDFiumEngine::DrawPageShadow(const pp::Rect& page_rc,
                                  const pp::Rect& shadow_rc,
                                  const pp::Rect& clip_rc,
                                  pp::ImageData* image_data) {
  pp::Rect page_rect(page_rc);
  page_rect.Offset(page_offset_);

  pp::Rect shadow_rect(shadow_rc);
  shadow_rect.Offset(page_offset_);

  pp::Rect clip_rect(clip_rc);
  clip_rect.Offset(page_offset_);

  // Page drop shadow parameters.
  const double factor = 0.5;
  uint32_t depth =
      std::max(std::max(page_rect.x() - shadow_rect.x(),
                        page_rect.y() - shadow_rect.y()),
               std::max(shadow_rect.right() - page_rect.right(),
                        shadow_rect.bottom() - page_rect.bottom()));
  depth = static_cast<uint32_t>(depth * 1.5) + 1;

  // We need to check depth only to verify our copy of shadow matrix is correct.
  if (!page_shadow_.get() || page_shadow_->depth() != depth) {
    page_shadow_ = std::make_unique<ShadowMatrix>(
        depth, factor, client_->GetBackgroundColor());
  }

  DCHECK(!image_data->is_null());
  DrawShadow(image_data, shadow_rect, page_rect, clip_rect, *page_shadow_);
}

void PDFiumEngine::GetRegion(const pp::Point& location,
                             pp::ImageData* image_data,
                             void** region,
                             int* stride) const {
  if (image_data->is_null()) {
    DCHECK(plugin_size_.IsEmpty());
    *stride = 0;
    *region = nullptr;
    return;
  }
  char* buffer = static_cast<char*>(image_data->data());
  *stride = image_data->stride();

  pp::Point offset_location = location + page_offset_;
  // TODO: update this when we support BIDI and scrollbars can be on the left.
  if (!buffer ||
      !pp::Rect(page_offset_, plugin_size_).Contains(offset_location)) {
    *region = nullptr;
    return;
  }

  buffer += location.y() * (*stride);
  buffer += (location.x() + page_offset_.x()) * 4;
  *region = buffer;
}

void PDFiumEngine::OnSelectionTextChanged() {
  DCHECK(!in_form_text_area_);
  pp::PDF::SetSelectedText(GetPluginInstance(), GetSelectedText().c_str());
}

void PDFiumEngine::OnSelectionPositionChanged() {
  // We need to determine the top-left and bottom-right points of the selection
  // in order to report those to the embedder. This code assumes that the
  // selection list is out of order.
  pp::Rect left(std::numeric_limits<int32_t>::max(),
                std::numeric_limits<int32_t>::max(), 0, 0);
  pp::Rect right;
  for (auto& sel : selection_) {
    const std::vector<pp::Rect>& screen_rects = sel.GetScreenRects(
        GetVisibleRect().point(), current_zoom_, current_rotation_);
    for (const auto& rect : screen_rects) {
      if (IsAboveOrDirectlyLeftOf(rect, left))
        left = rect;
      if (IsAboveOrDirectlyLeftOf(right, rect))
        right = rect;
    }
  }
  right.set_x(right.x() + right.width());
  if (left.IsEmpty()) {
    left.set_x(0);
    left.set_y(0);
  }
  client_->SelectionChanged(left, right);
}

void PDFiumEngine::RotateInternal() {
  // Store the current find index so that we can resume finding at that
  // particular index after we have recomputed the find results.
  std::string current_find_text = current_find_text_;
  resume_find_index_ = current_find_index_;

  // Save the current page.
  int most_visible_page = most_visible_page_;

  InvalidateAllPages();

  // Restore find results.
  if (!current_find_text.empty()) {
    // Clear the UI.
    client_->NotifyNumberOfFindResultsChanged(0, false);
    StartFind(current_find_text, false);
  }

  // Restore current page. After a rotation, the page heights have changed but
  // the scroll position has not. Re-adjust.
  // TODO(thestig): It would be better to also restore the position on the page.
  client_->ScrollToPage(most_visible_page);
}

void PDFiumEngine::SetSelecting(bool selecting) {
  bool was_selecting = selecting_;
  selecting_ = selecting;
  if (selecting_ != was_selecting)
    client_->IsSelectingChanged(selecting);
}

void PDFiumEngine::SetEditMode(bool edit_mode) {
  if (!kIsEditModeTracked || edit_mode_ == edit_mode)
    return;

  edit_mode_ = edit_mode;
  client_->IsEditModeChanged(edit_mode_);
}

void PDFiumEngine::SetInFormTextArea(bool in_form_text_area) {
  // If focus was previously in form text area, clear form text selection.
  // Clearing needs to be done before changing focus to ensure the correct
  // observer is notified of the change in selection. When |in_form_text_area_|
  // is true, this is the Renderer. After it flips, the MimeHandler is notified.
  if (in_form_text_area_)
    pp::PDF::SetSelectedText(GetPluginInstance(), "");

  client_->FormTextFieldFocusChange(in_form_text_area);
  in_form_text_area_ = in_form_text_area;

  // Clear |editable_form_text_area_| when focus no longer in form text area.
  if (!in_form_text_area_)
    editable_form_text_area_ = false;
}

void PDFiumEngine::SetMouseLeftButtonDown(bool is_mouse_left_button_down) {
  mouse_left_button_down_ = is_mouse_left_button_down;
}

bool PDFiumEngine::IsPointInEditableFormTextArea(FPDF_PAGE page,
                                                 double page_x,
                                                 double page_y,
                                                 int form_type) {
#if defined(PDF_ENABLE_XFA)
  if (IS_XFA_FORMFIELD(form_type))
    return form_type == FPDF_FORMFIELD_XFA_TEXTFIELD ||
           form_type == FPDF_FORMFIELD_XFA_COMBOBOX;
#endif  // defined(PDF_ENABLE_XFA)

  FPDF_ANNOTATION annot =
      FPDFAnnot_GetFormFieldAtPoint(form_, page, page_x, page_y);
  if (!annot)
    return false;

  int flags = FPDFAnnot_GetFormFieldFlags(page, annot);
  bool is_editable_form_text_area =
      CheckIfEditableFormTextArea(flags, form_type);
  FPDFPage_CloseAnnot(annot);
  return is_editable_form_text_area;
}

void PDFiumEngine::ScheduleTouchTimer(const pp::TouchInputEvent& evt) {
  touch_timers_[++next_touch_timer_id_] = evt;
  client_->ScheduleTouchTimerCallback(next_touch_timer_id_,
                                      kTouchLongPressTimeout);
}

void PDFiumEngine::KillTouchTimer(int timer_id) {
  touch_timers_.erase(timer_id);
}

bool PDFiumEngine::PageIndexInBounds(int index) const {
  return index >= 0 && index < static_cast<int>(pages_.size());
}

float PDFiumEngine::GetToolbarHeightInScreenCoords() {
  return client_->GetToolbarHeightInScreenCoords();
}

void PDFiumEngine::Form_Invalidate(FPDF_FORMFILLINFO* param,
                                   FPDF_PAGE page,
                                   double left,
                                   double top,
                                   double right,
                                   double bottom) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  int page_index = engine->GetVisiblePageIndex(page);
  if (page_index == -1) {
    // This can sometime happen when the page is closed because it went off
    // screen, and PDFium invalidates the control as it's being deleted.
    return;
  }

  pp::Rect rect = engine->pages_[page_index]->PageToScreen(
      engine->GetVisibleRect().point(), engine->current_zoom_, left, top, right,
      bottom, engine->current_rotation_);
  engine->client_->Invalidate(rect);
}

void PDFiumEngine::Form_OutputSelectedRect(FPDF_FORMFILLINFO* param,
                                           FPDF_PAGE page,
                                           double left,
                                           double top,
                                           double right,
                                           double bottom) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  int page_index = engine->GetVisiblePageIndex(page);
  if (page_index == -1) {
    NOTREACHED();
    return;
  }
  pp::Rect rect = engine->pages_[page_index]->PageToScreen(
      engine->GetVisibleRect().point(), engine->current_zoom_, left, top, right,
      bottom, engine->current_rotation_);
  if (rect.IsEmpty())
    return;

  engine->form_highlights_.push_back(rect);
}

void PDFiumEngine::Form_SetCursor(FPDF_FORMFILLINFO* param, int cursor_type) {
  // We don't need this since it's not enough to change the cursor in all
  // scenarios.  Instead, we check which form field we're under in OnMouseMove.
}

int PDFiumEngine::Form_SetTimer(FPDF_FORMFILLINFO* param,
                                int elapse,
                                TimerCallback timer_func) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  base::TimeDelta elapse_time = base::TimeDelta::FromMilliseconds(elapse);
  engine->formfill_timers_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(++engine->next_formfill_timer_id_),
      std::forward_as_tuple(elapse_time, timer_func));
  engine->client_->ScheduleCallback(engine->next_formfill_timer_id_,
                                    elapse_time);
  return engine->next_formfill_timer_id_;
}

void PDFiumEngine::Form_KillTimer(FPDF_FORMFILLINFO* param, int timer_id) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  engine->formfill_timers_.erase(timer_id);
}

FPDF_SYSTEMTIME PDFiumEngine::Form_GetLocalTime(FPDF_FORMFILLINFO* param) {
  base::Time time = base::Time::Now();
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);

  FPDF_SYSTEMTIME rv;
  rv.wYear = exploded.year;
  rv.wMonth = exploded.month;
  rv.wDayOfWeek = exploded.day_of_week;
  rv.wDay = exploded.day_of_month;
  rv.wHour = exploded.hour;
  rv.wMinute = exploded.minute;
  rv.wSecond = exploded.second;
  rv.wMilliseconds = exploded.millisecond;
  return rv;
}

void PDFiumEngine::Form_OnChange(FPDF_FORMFILLINFO* param) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  engine->SetEditMode(true);
}

FPDF_PAGE PDFiumEngine::Form_GetPage(FPDF_FORMFILLINFO* param,
                                     FPDF_DOCUMENT document,
                                     int page_index) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  if (!engine->PageIndexInBounds(page_index))
    return nullptr;
  return engine->pages_[page_index]->GetPage();
}

FPDF_PAGE PDFiumEngine::Form_GetCurrentPage(FPDF_FORMFILLINFO* param,
                                            FPDF_DOCUMENT document) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  int index = engine->last_page_mouse_down_;
  if (index == -1) {
    index = engine->GetMostVisiblePage();
    if (index == -1) {
      NOTREACHED();
      return nullptr;
    }
  }

  return engine->pages_[index]->GetPage();
}

int PDFiumEngine::Form_GetRotation(FPDF_FORMFILLINFO* param, FPDF_PAGE page) {
  return 0;
}

void PDFiumEngine::Form_ExecuteNamedAction(FPDF_FORMFILLINFO* param,
                                           FPDF_BYTESTRING named_action) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  std::string action(named_action);
  if (action == "Print") {
    engine->client_->Print();
    return;
  }

  int index = engine->last_page_mouse_down_;
  /* Don't try to calculate the most visible page if we don't have a left click
     before this event (this code originally copied Form_GetCurrentPage which of
     course needs to do that and which doesn't have recursion). This can end up
     causing infinite recursion. See http://crbug.com/240413 for more
     information. Either way, it's not necessary for the spec'd list of named
     actions.
  if (index == -1)
    index = engine->GetMostVisiblePage();
  */
  if (index == -1)
    return;

  // This is the only list of named actions per the spec (see 12.6.4.11). Adobe
  // Reader supports more, like FitWidth, but since they're not part of the spec
  // and we haven't got bugs about them, no need to now.
  if (action == "NextPage") {
    engine->ScrollToPage(index + 1);
  } else if (action == "PrevPage") {
    engine->ScrollToPage(index - 1);
  } else if (action == "FirstPage") {
    engine->ScrollToPage(0);
  } else if (action == "LastPage") {
    engine->ScrollToPage(engine->pages_.size() - 1);
  }
}

void PDFiumEngine::Form_SetTextFieldFocus(FPDF_FORMFILLINFO* param,
                                          FPDF_WIDESTRING value,
                                          FPDF_DWORD valueLen,
                                          FPDF_BOOL is_focus) {
  // Do nothing for now.
  // TODO(gene): use this signal to trigger OSK.
}

void PDFiumEngine::Form_DoURIAction(FPDF_FORMFILLINFO* param,
                                    FPDF_BYTESTRING uri) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  engine->client_->NavigateTo(std::string(uri),
                              WindowOpenDisposition::CURRENT_TAB);
}

void PDFiumEngine::Form_DoGoToAction(FPDF_FORMFILLINFO* param,
                                     int page_index,
                                     int zoom_mode,
                                     float* position_array,
                                     int size_of_array) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  engine->ScrollToPage(page_index);
}

int PDFiumEngine::Form_Alert(IPDF_JSPLATFORM* param,
                             FPDF_WIDESTRING message,
                             FPDF_WIDESTRING title,
                             int type,
                             int icon) {
  // See fpdfformfill.h for these values.
  enum AlertType {
    ALERT_TYPE_OK = 0,
    ALERT_TYPE_OK_CANCEL,
    ALERT_TYPE_YES_ON,
    ALERT_TYPE_YES_NO_CANCEL
  };

  enum AlertResult {
    ALERT_RESULT_OK = 1,
    ALERT_RESULT_CANCEL,
    ALERT_RESULT_NO,
    ALERT_RESULT_YES
  };

  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  std::string message_str = WideStringToString(message);
  if (type == ALERT_TYPE_OK) {
    engine->client_->Alert(message_str);
    return ALERT_RESULT_OK;
  }

  bool rv = engine->client_->Confirm(message_str);
  if (type == ALERT_TYPE_OK_CANCEL)
    return rv ? ALERT_RESULT_OK : ALERT_RESULT_CANCEL;
  return rv ? ALERT_RESULT_YES : ALERT_RESULT_NO;
}

void PDFiumEngine::Form_Beep(IPDF_JSPLATFORM* param, int type) {
  // Beeps are annoying, and not possible using javascript, so ignore for now.
}

int PDFiumEngine::Form_Response(IPDF_JSPLATFORM* param,
                                FPDF_WIDESTRING question,
                                FPDF_WIDESTRING title,
                                FPDF_WIDESTRING default_response,
                                FPDF_WIDESTRING label,
                                FPDF_BOOL password,
                                void* response,
                                int length) {
  std::string question_str = WideStringToString(question);
  std::string default_str = WideStringToString(default_response);

  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  std::string rv = engine->client_->Prompt(question_str, default_str);
  base::string16 rv_16 = base::UTF8ToUTF16(rv);
  int rv_bytes = rv_16.size() * sizeof(base::char16);
  if (response) {
    int bytes_to_copy = rv_bytes < length ? rv_bytes : length;
    memcpy(response, rv_16.c_str(), bytes_to_copy);
  }
  return rv_bytes;
}

int PDFiumEngine::Form_GetFilePath(IPDF_JSPLATFORM* param,
                                   void* file_path,
                                   int length) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  std::string rv = engine->client_->GetURL();
  if (file_path && rv.size() <= static_cast<size_t>(length))
    memcpy(file_path, rv.c_str(), rv.size());
  return rv.size();
}

void PDFiumEngine::Form_Mail(IPDF_JSPLATFORM* param,
                             void* mail_data,
                             int length,
                             FPDF_BOOL ui,
                             FPDF_WIDESTRING to,
                             FPDF_WIDESTRING subject,
                             FPDF_WIDESTRING cc,
                             FPDF_WIDESTRING bcc,
                             FPDF_WIDESTRING message) {
  // Note: |mail_data| and |length| are ignored. We don't handle attachments;
  // there is no way with mailto.
  std::string to_str = WideStringToString(to);
  std::string cc_str = WideStringToString(cc);
  std::string bcc_str = WideStringToString(bcc);
  std::string subject_str = WideStringToString(subject);
  std::string message_str = WideStringToString(message);

  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  engine->client_->Email(to_str, cc_str, bcc_str, subject_str, message_str);
}

void PDFiumEngine::Form_Print(IPDF_JSPLATFORM* param,
                              FPDF_BOOL ui,
                              int start,
                              int end,
                              FPDF_BOOL silent,
                              FPDF_BOOL shrink_to_fit,
                              FPDF_BOOL print_as_image,
                              FPDF_BOOL reverse,
                              FPDF_BOOL annotations) {
  // No way to pass the extra information to the print dialog using JavaScript.
  // Just opening it is fine for now.
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  engine->client_->Print();
}

void PDFiumEngine::Form_SubmitForm(IPDF_JSPLATFORM* param,
                                   void* form_data,
                                   int length,
                                   FPDF_WIDESTRING url) {
  std::string url_str = WideStringToString(url);
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  engine->client_->SubmitForm(url_str, form_data, length);
}

void PDFiumEngine::Form_GotoPage(IPDF_JSPLATFORM* param, int page_number) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  engine->ScrollToPage(page_number);
}

FPDF_BOOL PDFiumEngine::Pause_NeedToPauseNow(IFSDK_PAUSE* param) {
  PDFiumEngine* engine = static_cast<PDFiumEngine*>(param);
  return base::Time::Now() - engine->last_progressive_start_time_ >
         engine->progressive_paint_timeout_;
}

void PDFiumEngine::SetCaretPosition(const pp::Point& position) {
  // TODO(dsinclair): Handle caret position ...
}

void PDFiumEngine::MoveRangeSelectionExtent(const pp::Point& extent) {
  int page_index = -1;
  int char_index = -1;
  int form_type = FPDF_FORMFIELD_UNKNOWN;
  PDFiumPage::LinkTarget target;
  GetCharIndex(extent, &page_index, &char_index, &form_type, &target);
  if (page_index < 0 || char_index < 0)
    return;

  SelectionChangeInvalidator selection_invalidator(this);
  if (range_selection_direction_ == RangeSelectionDirection::Right) {
    ExtendSelection(page_index, char_index);
    return;
  }

  // For a left selection we clear the current selection and set a new starting
  // point based on the new left position. We then extend that selection out to
  // the previously provided base location.
  selection_.clear();
  selection_.push_back(PDFiumRange(pages_[page_index].get(), char_index, 0));

  // This should always succeeed because the range selection base should have
  // already been selected.
  GetCharIndex(range_selection_base_, &page_index, &char_index, &form_type,
               &target);
  ExtendSelection(page_index, char_index);
}

void PDFiumEngine::SetSelectionBounds(const pp::Point& base,
                                      const pp::Point& extent) {
  range_selection_base_ = base;
  range_selection_direction_ = IsAboveOrDirectlyLeftOf(base, extent)
                                   ? RangeSelectionDirection::Left
                                   : RangeSelectionDirection::Right;
}

void PDFiumEngine::GetSelection(uint32_t* selection_start_page_index,
                                uint32_t* selection_start_char_index,
                                uint32_t* selection_end_page_index,
                                uint32_t* selection_end_char_index) {
  size_t len = selection_.size();
  if (len == 0) {
    *selection_start_page_index = 0;
    *selection_start_char_index = 0;
    *selection_end_page_index = 0;
    *selection_end_char_index = 0;
    return;
  }

  *selection_start_page_index = selection_[0].page_index();
  *selection_start_char_index = selection_[0].char_index();
  *selection_end_page_index = selection_[len - 1].page_index();

  // If the selection is all within one page, the end index is the
  // start index plus the char count. But if the selection spans
  // multiple pages, the selection starts at the beginning of the
  // last page in |selection_| and goes to the char count.
  if (len == 1) {
    *selection_end_char_index =
        selection_[0].char_index() + selection_[0].char_count();
  } else {
    *selection_end_char_index = selection_[len - 1].char_count();
  }
}

PDFiumEngine::FormFillTimerData::FormFillTimerData(base::TimeDelta period,
                                                   TimerCallback callback)
    : timer_period(period), timer_callback(callback) {}

ScopedUnsupportedFeature::ScopedUnsupportedFeature(PDFiumEngine* engine)
    : old_engine_(g_engine_for_unsupported) {
  g_engine_for_unsupported = engine;
}

ScopedUnsupportedFeature::~ScopedUnsupportedFeature() {
  g_engine_for_unsupported = old_engine_;
}

ScopedSubstFont::ScopedSubstFont(PDFiumEngine* engine)
    : old_engine_(g_engine_for_fontmapper) {
  g_engine_for_fontmapper = engine;
}

ScopedSubstFont::~ScopedSubstFont() {
  g_engine_for_fontmapper = old_engine_;
}

namespace {

base::LazyInstance<PDFiumEngineExports>::Leaky g_pdf_engine_exports =
    LAZY_INSTANCE_INITIALIZER;

int CalculatePosition(FPDF_PAGE page,
                      const PDFiumEngineExports::RenderingSettings& settings,
                      pp::Rect* dest) {
  // settings.bounds is in terms of the max DPI. Convert page sizes to match.
  int dpi = std::max(settings.dpi_x, settings.dpi_y);
  int page_width = static_cast<int>(
      ConvertUnitDouble(FPDF_GetPageWidth(page), kPointsPerInch, dpi));
  int page_height = static_cast<int>(
      ConvertUnitDouble(FPDF_GetPageHeight(page), kPointsPerInch, dpi));

  // Start by assuming that we will draw exactly to the bounds rect
  // specified.
  *dest = settings.bounds;

  int rotate = 0;  // normal orientation.

  // Auto-rotate landscape pages to print correctly.
  if (settings.autorotate &&
      (dest->width() > dest->height()) != (page_width > page_height)) {
    rotate = 3;  // 90 degrees counter-clockwise.
    std::swap(page_width, page_height);
  }

  // See if we need to scale the output
  bool scale_to_bounds = false;
  if (settings.fit_to_bounds &&
      ((page_width > dest->width()) || (page_height > dest->height()))) {
    scale_to_bounds = true;
  } else if (settings.stretch_to_bounds &&
             ((page_width < dest->width()) || (page_height < dest->height()))) {
    scale_to_bounds = true;
  }

  if (scale_to_bounds) {
    // If we need to maintain aspect ratio, calculate the actual width and
    // height.
    if (settings.keep_aspect_ratio) {
      double scale_factor_x = page_width;
      scale_factor_x /= dest->width();
      double scale_factor_y = page_height;
      scale_factor_y /= dest->height();
      if (scale_factor_x > scale_factor_y) {
        dest->set_height(page_height / scale_factor_x);
      } else {
        dest->set_width(page_width / scale_factor_y);
      }
    }
  } else {
    // We are not scaling to bounds. Draw in the actual page size. If the
    // actual page size is larger than the bounds, the output will be
    // clipped.
    dest->set_width(page_width);
    dest->set_height(page_height);
  }

  // Scale the bounds to device units if DPI is rectangular.
  if (settings.dpi_x != settings.dpi_y) {
    dest->set_width(dest->width() * settings.dpi_x / dpi);
    dest->set_height(dest->height() * settings.dpi_y / dpi);
  }

  if (settings.center_in_bounds) {
    pp::Point offset(
        (settings.bounds.width() * settings.dpi_x / dpi - dest->width()) / 2,
        (settings.bounds.height() * settings.dpi_y / dpi - dest->height()) / 2);
    dest->Offset(offset);
  }
  return rotate;
}

}  // namespace

PDFEngineExports::RenderingSettings::RenderingSettings(int dpi_x,
                                                       int dpi_y,
                                                       const pp::Rect& bounds,
                                                       bool fit_to_bounds,
                                                       bool stretch_to_bounds,
                                                       bool keep_aspect_ratio,
                                                       bool center_in_bounds,
                                                       bool autorotate,
                                                       bool use_color)
    : dpi_x(dpi_x),
      dpi_y(dpi_y),
      bounds(bounds),
      fit_to_bounds(fit_to_bounds),
      stretch_to_bounds(stretch_to_bounds),
      keep_aspect_ratio(keep_aspect_ratio),
      center_in_bounds(center_in_bounds),
      autorotate(autorotate),
      use_color(use_color) {}

PDFEngineExports::RenderingSettings::RenderingSettings(
    const RenderingSettings& that) = default;

PDFEngineExports* PDFEngineExports::Get() {
  return g_pdf_engine_exports.Pointer();
}

#if defined(OS_WIN)
bool PDFiumEngineExports::RenderPDFPageToDC(const void* pdf_buffer,
                                            int buffer_size,
                                            int page_number,
                                            const RenderingSettings& settings,
                                            HDC dc) {
  std::unique_ptr<void, FPDFDocumentDeleter> doc(
      FPDF_LoadMemDocument(pdf_buffer, buffer_size, nullptr));
  if (!doc)
    return false;
  FPDF_PAGE page = FPDF_LoadPage(doc.get(), page_number);
  if (!page)
    return false;

  RenderingSettings new_settings = settings;
  // calculate the page size
  if (new_settings.dpi_x == -1)
    new_settings.dpi_x = GetDeviceCaps(dc, LOGPIXELSX);
  if (new_settings.dpi_y == -1)
    new_settings.dpi_y = GetDeviceCaps(dc, LOGPIXELSY);

  pp::Rect dest;
  int rotate = CalculatePosition(page, new_settings, &dest);

  int save_state = SaveDC(dc);
  // The caller wanted all drawing to happen within the bounds specified.
  // Based on scale calculations, our destination rect might be larger
  // than the bounds. Set the clip rect to the bounds.
  IntersectClipRect(dc, settings.bounds.x(), settings.bounds.y(),
                    settings.bounds.x() + settings.bounds.width(),
                    settings.bounds.y() + settings.bounds.height());

  int flags = FPDF_ANNOT | FPDF_PRINTING | FPDF_NO_CATCH;
  if (!settings.use_color)
    flags |= FPDF_GRAYSCALE;

  // A "temporary" hack. Some PDFs seems to render very slowly if
  // FPDF_RenderPage() is directly used on a printer DC. I suspect it is
  // because of the code to talk Postscript directly to the printer if
  // the printer supports this. Need to discuss this with PDFium. For now,
  // render to a bitmap and then blit the bitmap to the DC if we have been
  // supplied a printer DC.
  int device_type = GetDeviceCaps(dc, TECHNOLOGY);
  if (device_type == DT_RASPRINTER || device_type == DT_PLOTTER) {
    FPDF_BITMAP bitmap =
        FPDFBitmap_Create(dest.width(), dest.height(), FPDFBitmap_BGRx);
    // Clear the bitmap
    FPDFBitmap_FillRect(bitmap, 0, 0, dest.width(), dest.height(), 0xFFFFFFFF);
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, dest.width(), dest.height(),
                          rotate, flags);
    int stride = FPDFBitmap_GetStride(bitmap);
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dest.width();
    bmi.bmiHeader.biHeight = -dest.height();  // top-down image
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = stride * dest.height();
    StretchDIBits(dc, dest.x(), dest.y(), dest.width(), dest.height(), 0, 0,
                  dest.width(), dest.height(), FPDFBitmap_GetBuffer(bitmap),
                  &bmi, DIB_RGB_COLORS, SRCCOPY);
    FPDFBitmap_Destroy(bitmap);
  } else {
    FPDF_RenderPage(dc, page, dest.x(), dest.y(), dest.width(), dest.height(),
                    rotate, flags);
  }
  RestoreDC(dc, save_state);
  FPDF_ClosePage(page);
  return true;
}

void PDFiumEngineExports::SetPDFEnsureTypefaceCharactersAccessible(
    PDFEnsureTypefaceCharactersAccessible func) {
  FPDF_SetTypefaceAccessibleFunc(
      reinterpret_cast<PDFiumEnsureTypefaceCharactersAccessible>(func));
}

void PDFiumEngineExports::SetPDFUseGDIPrinting(bool enable) {
  FPDF_SetPrintTextWithGDI(enable);
}

void PDFiumEngineExports::SetPDFUsePrintMode(int mode) {
  FPDF_SetPrintMode(mode);
}
#endif  // defined(OS_WIN)

bool PDFiumEngineExports::RenderPDFPageToBitmap(
    const void* pdf_buffer,
    int pdf_buffer_size,
    int page_number,
    const RenderingSettings& settings,
    void* bitmap_buffer) {
  std::unique_ptr<void, FPDFDocumentDeleter> doc(
      FPDF_LoadMemDocument(pdf_buffer, pdf_buffer_size, nullptr));
  if (!doc)
    return false;
  FPDF_PAGE page = FPDF_LoadPage(doc.get(), page_number);
  if (!page)
    return false;

  pp::Rect dest;
  int rotate = CalculatePosition(page, settings, &dest);

  FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(
      settings.bounds.width(), settings.bounds.height(), FPDFBitmap_BGRA,
      bitmap_buffer, settings.bounds.width() * 4);
  // Clear the bitmap
  FPDFBitmap_FillRect(bitmap, 0, 0, settings.bounds.width(),
                      settings.bounds.height(), 0xFFFFFFFF);
  // Shift top-left corner of bounds to (0, 0) if it's not there.
  dest.set_point(dest.point() - settings.bounds.point());

  int flags = FPDF_ANNOT | FPDF_PRINTING | FPDF_NO_CATCH;
  if (!settings.use_color)
    flags |= FPDF_GRAYSCALE;

  FPDF_RenderPageBitmap(bitmap, page, dest.x(), dest.y(), dest.width(),
                        dest.height(), rotate, flags);
  FPDFBitmap_Destroy(bitmap);
  FPDF_ClosePage(page);
  return true;
}

bool PDFiumEngineExports::GetPDFDocInfo(const void* pdf_buffer,
                                        int buffer_size,
                                        int* page_count,
                                        double* max_page_width) {
  std::unique_ptr<void, FPDFDocumentDeleter> doc(
      FPDF_LoadMemDocument(pdf_buffer, buffer_size, nullptr));
  if (!doc)
    return false;

  if (!page_count && !max_page_width)
    return true;

  int page_count_local = FPDF_GetPageCount(doc.get());
  if (page_count)
    *page_count = page_count_local;

  if (max_page_width) {
    *max_page_width = 0;
    for (int page_number = 0; page_number < page_count_local; page_number++) {
      double page_width = 0;
      double page_height = 0;
      FPDF_GetPageSizeByIndex(doc.get(), page_number, &page_width,
                              &page_height);
      if (page_width > *max_page_width) {
        *max_page_width = page_width;
      }
    }
  }
  return true;
}

bool PDFiumEngineExports::GetPDFPageSizeByIndex(const void* pdf_buffer,
                                                int pdf_buffer_size,
                                                int page_number,
                                                double* width,
                                                double* height) {
  std::unique_ptr<void, FPDFDocumentDeleter> doc(
      FPDF_LoadMemDocument(pdf_buffer, pdf_buffer_size, nullptr));
  if (!doc)
    return false;
  return FPDF_GetPageSizeByIndex(doc.get(), page_number, width, height) != 0;
}

}  // namespace chrome_pdf
