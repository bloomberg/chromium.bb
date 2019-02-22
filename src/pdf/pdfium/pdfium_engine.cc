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
#include <utility>

#include "base/auto_reset.h"
#include "base/i18n/encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gin/array_buffer.h"
#include "gin/public/gin_embedders.h"
#include "gin/public/isolate_holder.h"
#include "pdf/document_loader_impl.h"
#include "pdf/draw_utils.h"
#include "pdf/pdf_transform.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_document.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_read.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "pdf/pdfium/pdfium_unsupported_features.h"
#include "pdf/url_loader_wrapper_impl.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/private/pdf.h"
#include "ppapi/cpp/trusted/browser_font_trusted.h"
#include "ppapi/cpp/var_dictionary.h"
#include "printing/units.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_annot.h"
#include "third_party/pdfium/public/fpdf_attachment.h"
#include "third_party/pdfium/public/fpdf_catalog.h"
#include "third_party/pdfium/public/fpdf_ext.h"
#include "third_party/pdfium/public/fpdf_ppo.h"
#include "third_party/pdfium/public/fpdf_searchex.h"
#include "third_party/pdfium/public/fpdf_sysfontinfo.h"
#include "ui/events/keycodes/keyboard_codes.h"
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

constexpr int32_t kPageShadowTop = 3;
constexpr int32_t kPageShadowBottom = 7;
constexpr int32_t kPageShadowLeft = 5;
constexpr int32_t kPageShadowRight = 5;

constexpr int32_t kPageSeparatorThickness = 4;
constexpr int32_t kHighlightColorR = 153;
constexpr int32_t kHighlightColorG = 193;
constexpr int32_t kHighlightColorB = 218;

constexpr uint32_t kPendingPageColor = 0xFFEEEEEE;

constexpr uint32_t kFormHighlightColor = 0xFFE4DD;
constexpr int32_t kFormHighlightAlpha = 100;

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
constexpr uint32_t kPDFPermissionPrintLowQualityMask = 1 << 2;
constexpr uint32_t kPDFPermissionPrintHighQualityMask = 1 << 11;
constexpr uint32_t kPDFPermissionCopyMask = 1 << 4;
constexpr uint32_t kPDFPermissionCopyAccessibleMask = 1 << 9;

constexpr int32_t kLoadingTextVerticalOffset = 50;

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

PDFiumEngine* g_engine_for_fontmapper = nullptr;

#if defined(OS_LINUX)

PP_Instance g_last_instance_id;

// TODO(npm): Move font stuff to another file to reduce the size of this one
PP_BrowserFont_Trusted_Weight WeightToBrowserFontTrustedWeight(int weight) {
  static_assert(PP_BROWSERFONT_TRUSTED_WEIGHT_100 == 0,
                "PP_BrowserFont_Trusted_Weight min");
  static_assert(PP_BROWSERFONT_TRUSTED_WEIGHT_900 == 8,
                "PP_BrowserFont_Trusted_Weight max");
  constexpr int kMinimumWeight = 100;
  constexpr int kMaximumWeight = 900;
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
  for (i = 0; i < base::size(kPdfFontSubstitutions); ++i) {
    if (strcmp(face, kPdfFontSubstitutions[i].pdf_name) == 0) {
      description.set_face(kPdfFontSubstitutions[i].face);
      if (kPdfFontSubstitutions[i].bold)
        description.set_weight(PP_BROWSERFONT_TRUSTED_WEIGHT_BOLD);
      if (kPdfFontSubstitutions[i].italic)
        description.set_italic(true);
      break;
    }
  }

  if (i == base::size(kPdfFontSubstitutions)) {
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

PDFiumEngine::CreateDocumentLoaderFunction
    g_create_document_loader_for_testing = nullptr;

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
  constexpr base::char16 kPdfiumHyphenEOL = 0xfffe;

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
    static constexpr base::char16 kCr[] = {L'\r', L'\0'};
    const HyphenPosition& position = hyphen_positions.back();
    if (position.next_whitespace_position != 0) {
      (*text)[position.next_whitespace_position] = L'\n';
      text->insert(position.next_whitespace_position, kCr);
    }
    text->erase(position.position, 1);
    hyphen_positions.pop_back();
  }

  // Adobe Reader also get rid of trailing spaces right before a CRLF.
  static constexpr base::char16 kSpaceCrCn[] = {L' ', L'\r', L'\n', L'\0'};
  static constexpr base::char16 kCrCn[] = {L'\r', L'\n', L'\0'};
  base::ReplaceSubstringsAfterOffset(text, 0, kSpaceCrCn, kCrCn);
}

// Replace CR/LF with just LF on POSIX.
void FormatStringForOS(base::string16* text) {
#if defined(OS_POSIX)
  static constexpr base::char16 kCr[] = {L'\r', L'\0'};
  static constexpr base::char16 kBlank[] = {L'\0'};
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
  g_isolate_holder = new gin::IsolateHolder(
      base::ThreadTaskRunnerHandle::Get(), gin::IsolateHolder::kSingleThread,
      gin::IsolateHolder::IsolateType::kUtility);
  g_isolate_holder->isolate()->Enter();
}

void TearDownV8() {
  g_isolate_holder->isolate()->Exit();
  delete g_isolate_holder;
  g_isolate_holder = nullptr;
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

  InitializeUnsupportedFeaturesHandler();

  return true;
}

void ShutdownSDK() {
  FPDF_DestroyLibrary();
#if !defined(OS_LINUX)
  delete g_font_info;
#endif
  TearDownV8();
}

std::unique_ptr<PDFEngine> PDFEngine::Create(PDFEngine::Client* client,
                                             bool enable_javascript) {
  return std::make_unique<PDFiumEngine>(client, enable_javascript);
}

PDFiumEngine::PDFiumEngine(PDFEngine::Client* client, bool enable_javascript)
    : client_(client),
      form_filler_(this, enable_javascript),
      mouse_down_state_(PDFiumPage::NONSELECTABLE_AREA,
                        PDFiumPage::LinkTarget()),
      print_(this) {
  find_factory_.Initialize(this);
  password_factory_.Initialize(this);

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

  if (doc())
    FORM_DoDocumentAAction(form(), FPDFDOC_AACTION_WC);
}

// static
void PDFiumEngine::SetCreateDocumentLoaderFunctionForTesting(
    CreateDocumentLoaderFunction function) {
  g_create_document_loader_for_testing = function;
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
    paint.set_painted(false);
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
        if (progressive_paints_[progressive].rect() != dirty_in_screen) {
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

      progressive_paints_[progressive].set_painted(true);
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
    if (progressive_paints_[i].painted())
      continue;

    // This rectangle must have been merged with another one, that's why we
    // weren't asked to paint it. Remove it or otherwise we'll never finish
    // painting.
    FPDF_RenderPage_Close(
        pages_[progressive_paints_[i].page_index()]->GetPage());
    progressive_paints_.erase(progressive_paints_.begin() + i);
    --i;
  }
}

bool PDFiumEngine::HandleDocumentLoad(const pp::URLLoader& loader) {
  password_tries_remaining_ = kMaxPasswordTries;
  process_when_pending_request_complete_ = true;

  if (g_create_document_loader_for_testing) {
    doc_loader_ = g_create_document_loader_for_testing(this);
  } else {
    auto loader_wrapper =
        std::make_unique<URLLoaderWrapperImpl>(GetPluginInstance(), loader);
    loader_wrapper->SetResponseHeaders(headers_);

    doc_loader_ = std::make_unique<DocumentLoaderImpl>(this);
    if (!doc_loader_->Init(std::move(loader_wrapper), url_))
      return false;
  }
  document_ = std::make_unique<PDFiumDocument>(doc_loader_.get());

  // request initial data.
  doc_loader_->RequestData(0, 1);
  return true;
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
  FPDFPage_Delete(doc(), index);
  FPDF_ImportPages(doc(), static_cast<PDFiumEngine*>(engine)->doc(), "1",
                   index);
  pp::Size new_page_size = GetPageSize(index);
  if (curr_page_size != new_page_size)
    LoadPageInfo(true);
  client_->Invalidate(GetPageScreenRect(index));
}

std::string PDFiumEngine::GetMetadata(const std::string& key) {
  return GetDocumentMetadata(doc(), key);
}

std::vector<uint8_t> PDFiumEngine::GetSaveData() {
  PDFiumMemBufferFileWrite output_file_write;
  if (!FPDF_SaveAsCopy(doc(), &output_file_write, 0))
    return std::vector<uint8_t>();
  return output_file_write.TakeBuffer();
}

void PDFiumEngine::OnPendingRequestComplete() {
  if (!process_when_pending_request_complete_)
    return;

  if (!fpdf_availability()) {
    document_->file_access().m_FileLen = doc_loader_->GetDocumentSize();
    document_->CreateFPDFAvailability();
    DCHECK(fpdf_availability());
    // Currently engine does not deal efficiently with some non-linearized
    // files.
    // See http://code.google.com/p/chromium/issues/detail?id=59400
    // To improve user experience we download entire file for non-linearized
    // PDF.
    if (FPDFAvail_IsLinearized(fpdf_availability()) != PDF_LINEARIZED) {
      // Wait complete document.
      process_when_pending_request_complete_ = false;
      document_->ResetFPDFAvailability();
      return;
    }
  }

  if (!doc()) {
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
  client_->DocumentLoadProgress(doc_loader_->BytesReceived(),
                                doc_loader_->GetDocumentSize());
}

void PDFiumEngine::OnDocumentComplete() {
  if (doc())
    return FinishLoadingDocument();

  document_->file_access().m_FileLen = doc_loader_->GetDocumentSize();
  if (!fpdf_availability()) {
    document_->CreateFPDFAvailability();
    DCHECK(fpdf_availability());
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
  DCHECK(doc());
  DCHECK(doc_loader_->IsDocumentComplete());

  LoadBody();

  FX_DOWNLOADHINTS& download_hints = document_->download_hints();
  bool need_update = false;
  for (size_t i = 0; i < pages_.size(); ++i) {
    if (pages_[i]->available())
      continue;

    pages_[i]->set_available(true);
    // We still need to call IsPageAvail() even if the whole document is
    // already downloaded.
    FPDFAvail_IsPageAvail(fpdf_availability(), i, &download_hints);
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
  FORM_DoDocumentJSAction(form());
  FORM_DoDocumentOpenAction(form());
  if (most_visible_page_ != -1) {
    FPDF_PAGE new_page = pages_[most_visible_page_]->GetPage();
    FORM_DoPageAAction(new_page, form(), FPDFPAGE_AACTION_OPEN);
  }

  if (doc()) {
    DocumentFeatures document_features;
    document_features.page_count = pages_.size();
    document_features.has_attachments = (FPDFDoc_GetAttachmentCount(doc()) > 0);
    document_features.is_linearized =
        (FPDFAvail_IsLinearized(fpdf_availability()) == PDF_LINEARIZED);
    document_features.is_tagged = FPDFCatalog_IsTagged(doc());
    document_features.form_type =
        static_cast<FormType>(FPDF_GetFormType(doc()));
    client_->DocumentLoadComplete(document_features,
                                  doc_loader_->BytesReceived());
  }
}

void PDFiumEngine::UnsupportedFeature(const std::string& feature) {
  client_->DocumentHasUnsupportedFeature(feature);
}

void PDFiumEngine::FontSubstituted() {
  client_->FontSubstituted();
}

FPDF_AVAIL PDFiumEngine::fpdf_availability() const {
  return document_ ? document_->fpdf_availability() : nullptr;
}

FPDF_DOCUMENT PDFiumEngine::doc() const {
  return document_ ? document_->doc() : nullptr;
}

FPDF_FORMHANDLE PDFiumEngine::form() const {
  return document_ ? document_->form() : nullptr;
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
      KillTouchTimer();

      pp::TouchInputEvent touch_event(event);
      if (touch_event.GetTouchCount(PP_TOUCHLIST_TYPE_TARGETTOUCHES) == 1)
        ScheduleTouchTimer(touch_event);
      break;
    }
    case PP_INPUTEVENT_TYPE_TOUCHEND:
      KillTouchTimer();
      break;
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
      // TODO(dsinclair): This should allow a little bit of movement (up to the
      // touch radii) to account for finger jiggle.
      KillTouchTimer();
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
  FORM_DoDocumentAAction(form(), FPDFDOC_AACTION_WP);
}

pp::Resource PDFiumEngine::PrintPages(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings) {
  ScopedSubstFont scoped_subst_font(this);
  if (!page_range_count)
    return pp::Resource();

  if ((print_settings.format & PP_PRINTOUTPUTFORMAT_PDF) &&
      HasPermission(PERMISSION_PRINT_HIGH_QUALITY)) {
    return PrintPagesAsPdf(page_ranges, page_range_count, print_settings,
                           pdf_print_settings);
  }
  if (HasPermission(PERMISSION_PRINT_LOW_QUALITY)) {
    return PrintPagesAsRasterPdf(page_ranges, page_range_count, print_settings,
                                 pdf_print_settings);
  }
  return pp::Resource();
}

pp::Buffer_Dev PDFiumEngine::PrintPagesAsRasterPdf(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings) {
  DCHECK(page_range_count);

  // If document is not downloaded yet, disable printing.
  if (doc() && !doc_loader_->IsDocumentComplete())
    return pp::Buffer_Dev();

  KillFormFocus();

#if defined(OS_LINUX)
  g_last_instance_id = client_->GetPluginInstance()->pp_instance();
#endif

  return ConvertPdfToBufferDev(
      print_.PrintPagesAsPdf(page_ranges, page_range_count, print_settings,
                             pdf_print_settings, /*raster=*/true));
}

pp::Buffer_Dev PDFiumEngine::PrintPagesAsPdf(
    const PP_PrintPageNumberRange_Dev* page_ranges,
    uint32_t page_range_count,
    const PP_PrintSettings_Dev& print_settings,
    const PP_PdfPrintSettings_Dev& pdf_print_settings) {
  DCHECK(page_range_count);
  DCHECK(doc());

  KillFormFocus();

  std::vector<uint32_t> page_numbers =
      PDFiumPrint::GetPageNumbersFromPrintPageNumberRange(page_ranges,
                                                          page_range_count);
  for (uint32_t page_number : page_numbers) {
    pages_[page_number]->GetPage();
    if (!IsPageVisible(page_number))
      pages_[page_number]->Unload();
  }

  return ConvertPdfToBufferDev(
      print_.PrintPagesAsPdf(page_ranges, page_range_count, print_settings,
                             pdf_print_settings, /*raster=*/false));
}

pp::Buffer_Dev PDFiumEngine::ConvertPdfToBufferDev(
    const std::vector<uint8_t>& pdf_data) {
  pp::Buffer_Dev buffer;
  if (!pdf_data.empty()) {
    buffer = pp::Buffer_Dev(GetPluginInstance(), pdf_data.size());
    if (!buffer.is_null())
      memcpy(buffer.data(), pdf_data.data(), pdf_data.size());
  }
  return buffer;
}

void PDFiumEngine::KillFormFocus() {
  FORM_ForceToKillFocus(form());
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
  FORM_DoDocumentAAction(form(), FPDFDOC_AACTION_DP);
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
    if (paint.page_index() == page)
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
    DeviceToPage(page_index, point, &page_x, &page_y);

    bool is_form_text_area = IsFormTextArea(area, form_type);
    FPDF_PAGE page = pages_[page_index]->GetPage();
    bool is_editable_form_text_area =
        is_form_text_area &&
        IsPointInEditableFormTextArea(page, page_x, page_y, form_type);

    FORM_OnLButtonDown(form(), page, event.GetModifiers(), page_x, page_y);
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

    DeviceToPage(page_index, point, &page_x, &page_y);
    page = pages_[page_index]->GetPage();
    is_editable_form_text_area =
        IsPointInEditableFormTextArea(page, page_x, page_y, form_type);
  }

  // Handle the case when focus starts inside a form text area.
  if (in_form_text_area_) {
    if (is_form_text_area) {
      FORM_OnFocus(form(), page, 0, page_x, page_y);
    } else {
      // Transition out of a form text area.
      FORM_ForceToKillFocus(form());
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
    FORM_OnFocus(form(), page, 0, page_x, page_y);
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
    uint32_t modifiers = event.GetModifiers();
    bool middle_button =
        !!(modifiers & PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN);
    bool alt_key = !!(modifiers & PP_INPUTEVENT_MODIFIER_ALTKEY);
    bool ctrl_key = !!(modifiers & PP_INPUTEVENT_MODIFIER_CONTROLKEY);
    bool meta_key = !!(modifiers & PP_INPUTEVENT_MODIFIER_METAKEY);
    bool shift_key = !!(modifiers & PP_INPUTEVENT_MODIFIER_SHIFTKEY);

    WindowOpenDisposition disposition = ui::DispositionFromClick(
        middle_button, alt_key, ctrl_key, meta_key, shift_key);

    if (area == PDFiumPage::WEBLINK_AREA) {
      client_->NavigateTo(target.url, disposition);
      SetInFormTextArea(false);
      return true;
    }
    if (area == PDFiumPage::DOCLINK_AREA) {
      if (!PageIndexInBounds(target.page))
        return true;

      if (disposition == WindowOpenDisposition::CURRENT_TAB) {
        pp::Rect page_rect(GetPageScreenRect(target.page));
        int y = position_.y() + page_rect.y();
        if (target.y_in_pixels)
          y += target.y_in_pixels.value() * current_zoom_;

        client_->ScrollToY(y, /*compensate_for_toolbar=*/true);
      } else {
        std::string parameters =
            base::StringPrintf("#page=%d", target.page + 1);
        if (target.y_in_pixels) {
          parameters += base::StringPrintf(
              "&zoom=100,0,%d", static_cast<int>(target.y_in_pixels.value()));
        }

        client_->NavigateTo(parameters, disposition);
      }
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
    DeviceToPage(page_index, point, &page_x, &page_y);
    FORM_OnLButtonUp(form(), pages_[page_index]->GetPage(),
                     event.GetModifiers(), page_x, page_y);
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
      DeviceToPage(page_index, point, &page_x, &page_y);
      FORM_OnMouseMove(form(), pages_[page_index]->GetPage(), 0, page_x,
                       page_y);
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
      SetFormSelectedText(form(), pages_[last_page_mouse_down_]->GetPage());
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

  bool rv = !!FORM_OnKeyDown(form(), pages_[last_page_mouse_down_]->GetPage(),
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
    SetFormSelectedText(form(), page);

  return !!FORM_OnKeyUp(form(), page, event.GetKeyCode(), event.GetModifiers());
}

bool PDFiumEngine::OnChar(const pp::KeyboardInputEvent& event) {
  if (last_page_mouse_down_ == -1)
    return false;

  base::string16 str = base::UTF8ToUTF16(event.GetCharacterText().AsString());
  return !!FORM_OnChar(form(), pages_[last_page_mouse_down_]->GetPage(), str[0],
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
    search_in_progress_ = true;
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
    search_in_progress_ = false;

    // Send the final notification.
    client_->NotifyNumberOfFindResultsChanged(find_results_.size(), true);
    return;
  }

  // In unit tests, PPAPI is not initialized, so just call ContinueFind()
  // directly.
  if (g_create_document_loader_for_testing) {
    ContinueFind(case_sensitive ? 1 : 0);
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

  base::string16 adjusted_page_text;
  adjusted_page_text.reserve(page_text.size());
  // Values in |removed_indices| are in the adjusted text index space and
  // indicate a character was removed from the page text before the given
  // index. If multiple characters are removed in a row then there will be
  // multiple entries with the same value.
  std::vector<size_t> removed_indices;
  // When walking through the page text collapse any whitespace regions,
  // including \r and \n, down to a single ' ' character. This code does
  // not use base::CollapseWhitespace(), because that function does not
  // return where the collapsing occurs, but uses the same underlying list of
  // whitespace characters. Calculating where the collapsed regions are after
  // the fact is as complex as collapsing them manually.
  for (size_t i = 0; i < page_text.size(); i++) {
    base::char16 c = page_text[i];
    // Collapse whitespace regions by inserting a ' ' into the
    // adjusted text and recording any removed whitespace indices as preceding
    // it.
    if (base::IsUnicodeWhitespace(c)) {
      size_t whitespace_region_begin = i;
      while (i < page_text.size() && base::IsUnicodeWhitespace(page_text[i]))
        ++i;

      size_t count = i - whitespace_region_begin - 1;
      removed_indices.insert(removed_indices.end(), count,
                             adjusted_page_text.size());
      adjusted_page_text.push_back(' ');
      if (i >= page_text.size())
        break;
      c = page_text[i];
    }

    if (IsIgnorableCharacter(c))
      removed_indices.push_back(adjusted_page_text.size());
    else
      adjusted_page_text.push_back(c);
  }

  std::vector<PDFEngine::Client::SearchStringResult> results =
      client_->SearchString(adjusted_page_text.c_str(), term.c_str(),
                            case_sensitive);
  for (const auto& result : results) {
    // Need to convert from adjusted page text start to page text start, by
    // incrementing for all the characters adjusted before it in the string.
    auto removed_indices_begin = std::upper_bound(
        removed_indices.begin(), removed_indices.end(), result.start_index);
    size_t page_text_result_start_index =
        result.start_index +
        std::distance(removed_indices.begin(), removed_indices_begin);

    // Need to convert the adjusted page length into a page text length, since
    // the matching range may have adjusted characters within it. This
    // conversion only cares about skipped characters in the result interval.
    auto removed_indices_end =
        std::upper_bound(removed_indices_begin, removed_indices.end(),
                         result.start_index + result.length);
    int term_removed_count =
        std::distance(removed_indices_begin, removed_indices_end);
    int page_text_result_length = result.length + term_removed_count;

    // Need to map the indexes from the page text, which may have generated
    // characters like space etc, to character indices from the page.
    int text_to_start_searching_from = FPDFText_GetTextIndexFromCharIndex(
        pages_[current_page]->GetTextPage(), character_to_start_searching_from);
    int temp_start =
        page_text_result_start_index + text_to_start_searching_from;
    int start = FPDFText_GetCharIndexFromTextIndex(
        pages_[current_page]->GetTextPage(), temp_start);
    int end = FPDFText_GetCharIndexFromTextIndex(
        pages_[current_page]->GetTextPage(),
        temp_start + page_text_result_length);

    // If |term| occurs at the end of a page, then |end| will be -1 due to the
    // index being out of bounds. Compensate for this case so the range
    // character count calculation below works out.
    if (temp_start + page_text_result_length == original_text_length) {
      DCHECK_EQ(-1, end);
      end = original_text_length;
    }
    DCHECK_LT(start, end);
    DCHECK_EQ(term.size() + term_removed_count,
              static_cast<size_t>(end - start));
    AddFindResult(PDFiumRange(pages_[current_page].get(), start, end - start));
  }
}

void PDFiumEngine::AddFindResult(const PDFiumRange& result) {
  bool first_result = find_results_.empty();
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
  if (first_result) {
    DCHECK(!resume_find_index_);
    DCHECK(!current_find_index_);
    SelectFindResult(/*forward=*/true);
  }
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
  if (!search_in_progress_)
    client_->NotifyNumberOfFindResultsChanged(find_results_.size(), true);
  return true;
}

void PDFiumEngine::StopFind() {
  SelectionChangeInvalidator selection_invalidator(this);
  selection_.clear();
  selecting_ = false;

  find_results_.clear();
  search_in_progress_ = false;
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

bool PDFiumEngine::HasEditableText() {
  DCHECK(CanEditText());
  if (last_page_mouse_down_ == -1)
    return false;
  FPDF_PAGE page = pages_[last_page_mouse_down_]->GetPage();
  // If the return value is 2, that corresponds to "\0\0".
  return FORM_GetFocusedText(form(), page, nullptr, 0) > 2;
}

void PDFiumEngine::ReplaceSelection(const std::string& text) {
  DCHECK(CanEditText());
  if (last_page_mouse_down_ != -1) {
    base::string16 text_wide = base::UTF8ToUTF16(text);
    FPDF_WIDESTRING text_pdf_wide =
        reinterpret_cast<FPDF_WIDESTRING>(text_wide.c_str());

    FORM_ReplaceSelection(form(), pages_[last_page_mouse_down_]->GetPage(),
                          text_pdf_wide);
  }
}

bool PDFiumEngine::CanUndo() {
  if (last_page_mouse_down_ == -1)
    return false;
  return !!FORM_CanUndo(form(), pages_[last_page_mouse_down_]->GetPage());
}

bool PDFiumEngine::CanRedo() {
  if (last_page_mouse_down_ == -1)
    return false;
  return !!FORM_CanRedo(form(), pages_[last_page_mouse_down_]->GetPage());
}

void PDFiumEngine::Undo() {
  if (last_page_mouse_down_ != -1)
    FORM_Undo(form(), pages_[last_page_mouse_down_]->GetPage());
}

void PDFiumEngine::Redo() {
  if (last_page_mouse_down_ != -1)
    FORM_Redo(form(), pages_[last_page_mouse_down_]->GetPage());
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

  FPDF_DEST dest = FPDFBookmark_GetDest(doc(), bookmark);
  // Some bookmarks don't have a page to select.
  if (dest) {
    int page_index = FPDFDest_GetDestPageIndex(doc(), dest);
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
    buffer_size = FPDFAction_GetURIPath(doc(), action, nullptr, 0);
    if (buffer_size > 0) {
      std::string uri;
      PDFiumAPIStringBufferAdapter<std::string> api_string_adapter(
          &uri, buffer_size, true);
      api_string_adapter.Close(FPDFAction_GetURIPath(
          doc(), action, api_string_adapter.GetData(), buffer_size));
      dict.Set(pp::Var("uri"), pp::Var(uri));
    }
  }

  pp::VarArray children;

  // Don't trust PDFium to handle circular bookmarks.
  constexpr unsigned int kMaxDepth = 128;
  if (depth < kMaxDepth) {
    int child_index = 0;
    std::set<FPDF_BOOKMARK> seen_bookmarks;
    for (FPDF_BOOKMARK child_bookmark =
             FPDFBookmark_GetFirstChild(doc(), bookmark);
         child_bookmark;
         child_bookmark = FPDFBookmark_GetNextSibling(doc(), child_bookmark)) {
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
  FPDF_DEST dest = FPDF_GetNamedDestByName(doc(), destination.c_str());
  if (!dest) {
    // Look for a bookmark with the same name.
    base::string16 destination_wide = base::UTF8ToUTF16(destination);
    FPDF_WIDESTRING destination_pdf_wide =
        reinterpret_cast<FPDF_WIDESTRING>(destination_wide.c_str());
    FPDF_BOOKMARK bookmark = FPDFBookmark_Find(doc(), destination_pdf_wide);
    if (bookmark)
      dest = FPDFBookmark_GetDest(doc(), bookmark);
  }

  if (!dest)
    return {};

  int page = FPDFDest_GetDestPageIndex(doc(), dest);
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
  return !!FPDF_VIEWERREF_GetPrintScaling(doc());
}

int PDFiumEngine::GetCopiesToPrint() {
  return FPDF_VIEWERREF_GetNumCopies(doc());
}

int PDFiumEngine::GetDuplexType() {
  return static_cast<int>(FPDF_VIEWERREF_GetDuplex(doc()));
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

  if (!doc())
    return;

  selection_.clear();
  pending_pages_.clear();

  // Delete all pages except the first one.
  while (pages_.size() > 1) {
    pages_.pop_back();
    FPDFPage_Delete(doc(), pages_.size());
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
    {
      // Add a new page to the document, but delete the FPDF_PAGE object.
      ScopedFPDFPage temp_page(
          FPDFPage_New(doc(), i, width_in_points, height_in_points));
    }
    pages_.push_back(std::make_unique<PDFiumPage>(this, i, page_rect, true));
  }

  CalculateVisiblePages();
  if (document_size_ != old_document_size)
    client_->DocumentSizeUpdated(document_size_);
}

void PDFiumEngine::LoadDocument() {
  // Check if the document is ready for loading. If it isn't just bail for now,
  // we will call LoadDocument() again later.
  if (!doc() && !doc_loader_->IsDocumentComplete()) {
    FX_DOWNLOADHINTS& download_hints = document_->download_hints();
    if (!FPDFAvail_IsDocAvail(fpdf_availability(), &download_hints))
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
  if (doc()) {
    // This is probably not necessary, because it should have already been
    // called below in the |doc_| initialization path. However, the previous
    // call may have failed, so call it again for good measure.
    FX_DOWNLOADHINTS& download_hints = document_->download_hints();
    FPDFAvail_IsDocAvail(fpdf_availability(), &download_hints);
    return true;
  }

  const char* password_cstr = nullptr;
  if (!password.empty()) {
    password_cstr = password.c_str();
    password_tries_remaining_--;
  }
  document_->LoadDocument(password_cstr);
  if (!doc()) {
    if (FPDF_GetLastError() == FPDF_ERR_PASSWORD)
      *needs_password = true;
    return false;
  }

  // Always call FPDFAvail_IsDocAvail() so PDFium initializes internal data
  // structures.
  FX_DOWNLOADHINTS& download_hints = document_->download_hints();
  FPDFAvail_IsDocAvail(fpdf_availability(), &download_hints);
  return true;
}

void PDFiumEngine::GetPasswordAndLoad() {
  getting_password_ = true;
  DCHECK(!doc());
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

  if (!doc()) {
    client_->DocumentLoadFailed();
    return;
  }

  if (FPDFDoc_GetPageMode(doc()) == PAGEMODE_USEOUTLINES)
    client_->DocumentHasUnsupportedFeature("Bookmarks");

  permissions_ = FPDF_GetDocPermissions(doc());
  permissions_handler_revision_ = FPDF_GetSecurityHandlerRevision(doc());

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
  size_t new_page_count = FPDF_GetPageCount(doc());

  bool doc_complete = doc_loader_->IsDocumentComplete();
  bool is_linear =
      FPDFAvail_IsLinearized(fpdf_availability()) == PDF_LINEARIZED;
  for (size_t i = 0; i < new_page_count; ++i) {
    if (i != 0) {
      // Add space for horizontal separator.
      document_size_.Enlarge(0, kPageSeparatorThickness);
    }

    // Get page availability. If |reload| == true and the page is not new,
    // then the page has been constructed already. Get page availability flag
    // from already existing PDFiumPage object.
    // If |reload| == false or the page is new, then the page may not be fully
    // loaded yet.
    bool page_available;
    if (reload && i < pages_.size()) {
      page_available = pages_[i]->available();
    } else if (is_linear) {
      FX_DOWNLOADHINTS& download_hints = document_->download_hints();
      int linear_page_avail =
          FPDFAvail_IsPageAvail(fpdf_availability(), i, &download_hints);
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

  for (size_t i = 0; i < new_page_count; ++i) {
    // Center pages relative to the entire document.
    page_rects[i].set_x((document_size_.width() - page_rects[i].width()) / 2);
    pp::Rect page_rect(page_rects[i]);
    page_rect.Inset(kPageShadowLeft, kPageShadowTop, kPageShadowRight,
                    kPageShadowBottom);
    if (!reload) {
      // The page is marked as not being available even if |doc_complete| is
      // true because FPDFAvail_IsPageAvail() still has to be called for this
      // page, which will be done in FinishLoadingDocument().
      pages_.push_back(std::make_unique<PDFiumPage>(this, i, page_rect, false));
    } else if (i < pages_.size()) {
      pages_[i]->set_rect(page_rect);
    } else {
      bool available = FPDFAvail_IsPageAvail(fpdf_availability(), i, nullptr);
      pages_.push_back(
          std::make_unique<PDFiumPage>(this, i, page_rect, available));
    }
  }

  // Remove pages that do not exist anymore.
  if (pages_.size() > new_page_count) {
    for (size_t i = new_page_count; i < pages_.size(); ++i)
      pages_[i]->Unload();

    pages_.resize(new_page_count);
  }

  CalculateVisiblePages();
  if (document_size_ != old_document_size)
    client_->DocumentSizeUpdated(document_size_);
}

void PDFiumEngine::LoadBody() {
  DCHECK(doc());
  DCHECK(fpdf_availability());
  if (doc_loader_->IsDocumentComplete()) {
    LoadForm();
  } else if (FPDFAvail_IsLinearized(fpdf_availability()) == PDF_LINEARIZED &&
             FPDF_GetPageCount(doc()) == 1) {
    // If we have only one page we should load form first, bacause it is may be
    // XFA document. And after loading form the page count and its contents may
    // be changed.
    LoadForm();
    if (document_->form_status() == PDF_FORM_NOTAVAIL)
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
      CheckPageAvailable(FPDFAvail_GetFirstPageNum(doc()), &pending_pages_);
    }
    LoadPageInfo(false);
  }
}

void PDFiumEngine::LoadForm() {
  if (form())
    return;

  DCHECK(doc());
  document_->SetFormStatus();
  if (document_->form_status() != PDF_FORM_NOTAVAIL ||
      doc_loader_->IsDocumentComplete()) {
    document_->InitializeForm(&form_filler_);
#if defined(PDF_ENABLE_XFA)
    FPDF_LoadXFA(doc());
#endif

    FPDF_SetFormFieldHighlightColor(form(), FPDF_FORMFIELD_UNKNOWN,
                                    kFormHighlightColor);
    FPDF_SetFormFieldHighlightAlpha(form(), kFormHighlightAlpha);
  }
}

void PDFiumEngine::CalculateVisiblePages() {
  // Early return if the PDF isn't being loaded or if we don't have the document
  // info yet. The latter is important because otherwise as the PDF is being
  // initialized by the renderer there could be races that call this method
  // before we get the initial network responses. The document loader depends on
  // the list of pending requests to be valid for progressive loading to
  // function.
  if (!doc_loader_ || pages_.empty())
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
  if (!doc())
    return false;

  const int num_pages = static_cast<int>(pages_.size());
  if (index < num_pages && pages_[index]->available())
    return true;

  FX_DOWNLOADHINTS& download_hints = document_->download_hints();
  if (!FPDFAvail_IsPageAvail(fpdf_availability(), index, &download_hints)) {
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
  int rv = FPDF_GetPageSizeByIndex(doc(), index, &width_in_points,
                                   &height_in_points);

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
  progressive_paints_.emplace_back(page_index, dirty);
  return progressive_paints_.size() - 1;
}

bool PDFiumEngine::ContinuePaint(int progressive_index,
                                 pp::ImageData* image_data) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());
  DCHECK(image_data);

  last_progressive_start_time_ = base::Time::Now();
#if defined(OS_LINUX)
  g_last_instance_id = client_->GetPluginInstance()->pp_instance();
#endif

  int page_index = progressive_paints_[progressive_index].page_index();
  DCHECK(PageIndexInBounds(page_index));

  int rv;
  FPDF_PAGE page = pages_[page_index]->GetPage();
  if (progressive_paints_[progressive_index].bitmap()) {
    rv = FPDF_RenderPage_Continue(page, this);
  } else {
    int start_x;
    int start_y;
    int size_x;
    int size_y;
    pp::Rect dirty = progressive_paints_[progressive_index].rect();
    GetPDFiumRect(page_index, dirty, &start_x, &start_y, &size_x, &size_y);

    ScopedFPDFBitmap new_bitmap = CreateBitmap(dirty, image_data);
    FPDFBitmap_FillRect(new_bitmap.get(), start_x, start_y, size_x, size_y,
                        0xFFFFFFFF);
    rv = FPDF_RenderPageBitmap_Start(new_bitmap.get(), page, start_x, start_y,
                                     size_x, size_y, current_rotation_,
                                     GetRenderingFlags(), this);
    progressive_paints_[progressive_index].SetBitmapAndImageData(
        std::move(new_bitmap), *image_data);
  }
  return rv != FPDF_RENDER_TOBECONTINUED;
}

void PDFiumEngine::FinishPaint(int progressive_index,
                               pp::ImageData* image_data) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());
  DCHECK(image_data);

  int page_index = progressive_paints_[progressive_index].page_index();
  const pp::Rect& dirty_in_screen =
      progressive_paints_[progressive_index].rect();

  int start_x;
  int start_y;
  int size_x;
  int size_y;
  FPDF_BITMAP bitmap = progressive_paints_[progressive_index].bitmap();
  GetPDFiumRect(page_index, dirty_in_screen, &start_x, &start_y, &size_x,
                &size_y);

  // Draw the forms.
  FPDF_FFLDraw(form(), bitmap, pages_[page_index]->GetPage(), start_x, start_y,
               size_x, size_y, current_rotation_, GetRenderingFlags());

  FillPageSides(progressive_index);

  // Paint the page shadows.
  PaintPageShadow(progressive_index, image_data);

  DrawSelections(progressive_index, image_data);

  FPDF_RenderPage_Close(pages_[page_index]->GetPage());
  progressive_paints_.erase(progressive_paints_.begin() + progressive_index);

  client_->DocumentPaintOccurred();
}

void PDFiumEngine::CancelPaints() {
  for (const auto& paint : progressive_paints_)
    FPDF_RenderPage_Close(pages_[paint.page_index()]->GetPage());

  progressive_paints_.clear();
}

void PDFiumEngine::FillPageSides(int progressive_index) {
  DCHECK_GE(progressive_index, 0);
  DCHECK_LT(static_cast<size_t>(progressive_index), progressive_paints_.size());

  int page_index = progressive_paints_[progressive_index].page_index();
  const pp::Rect& dirty_in_screen =
      progressive_paints_[progressive_index].rect();
  FPDF_BITMAP bitmap = progressive_paints_[progressive_index].bitmap();

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

  int page_index = progressive_paints_[progressive_index].page_index();
  const pp::Rect& dirty_in_screen =
      progressive_paints_[progressive_index].rect();
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

  int page_index = progressive_paints_[progressive_index].page_index();
  const pp::Rect& dirty_in_screen =
      progressive_paints_[progressive_index].rect();

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
  int start_x;
  int start_y;
  int size_x;
  int size_y;
  GetPDFiumRect(page_index, dirty, &start_x, &start_y, &size_x, &size_y);
  ScopedFPDFBitmap bitmap(CreateBitmap(dirty, image_data));
  FPDFBitmap_FillRect(bitmap.get(), start_x, start_y, size_x, size_y,
                      kPendingPageColor);

  pp::Rect loading_text_in_screen(
      pages_[page_index]->rect().width() / 2,
      pages_[page_index]->rect().y() + kLoadingTextVerticalOffset, 0, 0);
  loading_text_in_screen = GetScreenRect(loading_text_in_screen);
}

int PDFiumEngine::GetProgressiveIndex(int page_index) const {
  for (size_t i = 0; i < progressive_paints_.size(); ++i) {
    if (progressive_paints_[i].page_index() == page_index)
      return i;
  }
  return -1;
}

ScopedFPDFBitmap PDFiumEngine::CreateBitmap(const pp::Rect& rect,
                                            pp::ImageData* image_data) const {
  void* region;
  int stride;
  GetRegion(rect.point(), image_data, &region, &stride);
  if (!region)
    return nullptr;
  return ScopedFPDFBitmap(FPDFBitmap_CreateEx(rect.width(), rect.height(),
                                              FPDFBitmap_BGRx, region, stride));
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
                                const pp::Point& device_point,
                                double* page_x,
                                double* page_y) {
  *page_x = *page_y = 0;
  float device_x = device_point.x();
  float device_y = device_point.y();
  int temp_x = static_cast<int>((device_x + position_.x()) / current_zoom_ -
                                pages_[page_index]->rect().x());
  int temp_y = static_cast<int>((device_y + position_.y()) / current_zoom_ -
                                pages_[page_index]->rect().y());
  FPDF_BOOL ret = FPDF_DeviceToPage(
      pages_[page_index]->GetPage(), 0, 0, pages_[page_index]->rect().width(),
      pages_[page_index]->rect().height(), current_rotation_, temp_x, temp_y,
      page_x, page_y);
  DCHECK(ret);
}

int PDFiumEngine::GetVisiblePageIndex(FPDF_PAGE page) {
  // Copy |visible_pages_| since it can change as a result of loading the page
  // in GetPage(). See https://crbug.com/822091.
  std::vector<int> visible_pages_copy(visible_pages_);
  for (int page_index : visible_pages_copy) {
    if (pages_[page_index]->GetPage() == page)
      return page_index;
  }
  return -1;
}

void PDFiumEngine::SetCurrentPage(int index) {
  in_flight_visible_page_.reset();

  if (index == most_visible_page_ || !form())
    return;

  if (most_visible_page_ != -1 && called_do_document_action_) {
    FPDF_PAGE old_page = pages_[most_visible_page_]->GetPage();
    FORM_DoPageAAction(old_page, form(), FPDFPAGE_AACTION_CLOSE);
  }
  most_visible_page_ = index;
#if defined(OS_LINUX)
  g_last_instance_id = client_->GetPluginInstance()->pp_instance();
#endif
  if (most_visible_page_ != -1 && called_do_document_action_) {
    FPDF_PAGE new_page = pages_[most_visible_page_]->GetPage();
    FORM_DoPageAAction(new_page, form(), FPDFPAGE_AACTION_OPEN);
  }
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
  constexpr double factor = 0.5;
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
  if (edit_mode_ == edit_mode)
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

  ScopedFPDFAnnotation annot(
      FPDFAnnot_GetFormFieldAtPoint(form(), page, page_x, page_y));
  if (!annot)
    return false;

  int flags = FPDFAnnot_GetFormFieldFlags(page, annot.get());
  return CheckIfEditableFormTextArea(flags, form_type);
}

void PDFiumEngine::ScheduleTouchTimer(const pp::TouchInputEvent& evt) {
  touch_timer_.Start(FROM_HERE, kTouchLongPressTimeout,
                     base::BindRepeating(&PDFiumEngine::HandleLongPress,
                                         base::Unretained(this), evt));
}

void PDFiumEngine::KillTouchTimer() {
  touch_timer_.Stop();
}

bool PDFiumEngine::PageIndexInBounds(int index) const {
  return index >= 0 && index < static_cast<int>(pages_.size());
}

float PDFiumEngine::GetToolbarHeightInScreenCoords() {
  return client_->GetToolbarHeightInScreenCoords();
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

#if defined(PDF_ENABLE_XFA)
void PDFiumEngine::UpdatePageCount() {
  InvalidateAllPages();
}
#endif  // defined(PDF_ENABLE_XFA)

PDFiumEngine::ProgressivePaint::ProgressivePaint(int index,
                                                 const pp::Rect& rect)
    : page_index_(index), rect_(rect) {}

PDFiumEngine::ProgressivePaint::ProgressivePaint(ProgressivePaint&& that) =
    default;

PDFiumEngine::ProgressivePaint::~ProgressivePaint() = default;

PDFiumEngine::ProgressivePaint& PDFiumEngine::ProgressivePaint::operator=(
    ProgressivePaint&& that) = default;

void PDFiumEngine::ProgressivePaint::SetBitmapAndImageData(
    ScopedFPDFBitmap bitmap,
    pp::ImageData image_data) {
  bitmap_ = std::move(bitmap);
  image_data_ = std::move(image_data);
}

ScopedSubstFont::ScopedSubstFont(PDFiumEngine* engine)
    : old_engine_(g_engine_for_fontmapper) {
  g_engine_for_fontmapper = engine;
}

ScopedSubstFont::~ScopedSubstFont() {
  g_engine_for_fontmapper = old_engine_;
}

}  // namespace chrome_pdf
