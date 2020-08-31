// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/renderer/print_render_frame_helper.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/grit/components_resources.h"
#include "components/printing/common/print_messages.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/escape.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile_skia.h"
#include "printing/printing_features.h"
#include "printing/units.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_double_size.h"
#include "third_party/blink/public/platform/web_failing_url_loader_factory.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_owner_properties.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_document.h"
#include "third_party/blink/public/web/web_print_page_description.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_print_preset_options.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/resource/resource_bundle.h"

using content::WebPreferences;

namespace printing {

namespace {

#ifndef STATIC_ASSERT_ENUM
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)
#endif

enum PrintPreviewHelperEvents {
  PREVIEW_EVENT_REQUESTED,        // Received a request for a preview document.
  PREVIEW_EVENT_CACHE_HIT,        // Unused.
  PREVIEW_EVENT_CREATE_DOCUMENT,  // Started creating a preview document.
  PREVIEW_EVENT_NEW_SETTINGS,     // Unused.
  PREVIEW_EVENT_INITIATED,        // Initiated print preview.
  PREVIEW_EVENT_MAX,
};

constexpr double kMinDpi = 1.0;

// Also set in third_party/WebKit/Source/core/page/PrintContext.h
constexpr float kPrintingMinimumShrinkFactor = 1.33333333f;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
bool g_is_preview_enabled = true;
#else
bool g_is_preview_enabled = false;
#endif

const char kPageLoadScriptFormat[] =
    "document.open(); document.write(%s); document.close();";

const char kPageSetupScriptFormat[] = "setupHeaderFooterTemplate(%s);";

void ExecuteScript(blink::WebLocalFrame* frame,
                   const char* script_format,
                   const base::Value& parameters) {
  std::string json;
  base::JSONWriter::Write(parameters, &json);
  std::string script = base::StringPrintf(script_format, json.c_str());
  frame->ExecuteScript(blink::WebString::FromUTF8(script));
}

int GetDPI(const PrintMsg_Print_Params& print_params) {
#if defined(OS_MACOSX)
  // On Mac, the printable area is in points, don't do any scaling based on DPI.
  return kPointsPerInch;
#else
  // Render using the higher of the two resolutions in both dimensions to
  // prevent bad quality print jobs on rectantular DPI printers.
  return static_cast<int>(
      std::max(print_params.dpi.width(), print_params.dpi.height()));
#endif  // defined(OS_MACOSX)
}

bool PrintMsg_Print_Params_IsValid(const PrintMsg_Print_Params& params) {
  return !params.content_size.IsEmpty() && !params.page_size.IsEmpty() &&
         !params.printable_area.IsEmpty() && params.document_cookie &&
         params.dpi.width() > kMinDpi && params.dpi.height() > kMinDpi &&
         params.margin_top >= 0 && params.margin_left >= 0;
}

// Helper function to check for fit to page
bool IsWebPrintScalingOptionFitToPage(const PrintMsg_Print_Params& params) {
  return params.print_scaling_option ==
         blink::kWebPrintScalingOptionFitToPrintableArea;
}

PrintMsg_Print_Params GetCssPrintParams(
    blink::WebLocalFrame* frame,
    int page_index,
    const PrintMsg_Print_Params& page_params) {
  PrintMsg_Print_Params page_css_params = page_params;
  int dpi = GetDPI(page_params);

  blink::WebPrintPageDescription description;
  description.size = blink::WebDoubleSize(
      ConvertUnitDouble(page_params.page_size.width(), dpi, kPixelsPerInch),
      ConvertUnitDouble(page_params.page_size.height(), dpi, kPixelsPerInch));
  description.margin_top =
      ConvertUnit(page_params.margin_top, dpi, kPixelsPerInch);
  description.margin_right = ConvertUnit(page_params.page_size.width() -
                                             page_params.content_size.width() -
                                             page_params.margin_left,
                                         dpi, kPixelsPerInch);
  description.margin_bottom = ConvertUnit(
      page_params.page_size.height() - page_params.content_size.height() -
          page_params.margin_top,
      dpi, kPixelsPerInch);
  description.margin_left =
      ConvertUnit(page_params.margin_left, dpi, kPixelsPerInch);

  if (frame)
    frame->GetPageDescription(page_index, &description);

  double new_content_width = description.size.Width() -
                             description.margin_left - description.margin_right;
  double new_content_height = description.size.Height() -
                              description.margin_top -
                              description.margin_bottom;

  // Invalid page size and/or margins. We just use the default setting.
  if (new_content_width < 1 || new_content_height < 1) {
    CHECK(frame);
    page_css_params = GetCssPrintParams(nullptr, page_index, page_params);
    return page_css_params;
  }

  page_css_params.page_size =
      gfx::Size(ConvertUnit(description.size.Width(), kPixelsPerInch, dpi),
                ConvertUnit(description.size.Height(), kPixelsPerInch, dpi));
  page_css_params.content_size =
      gfx::Size(ConvertUnit(new_content_width, kPixelsPerInch, dpi),
                ConvertUnit(new_content_height, kPixelsPerInch, dpi));

  page_css_params.margin_top =
      ConvertUnit(description.margin_top, kPixelsPerInch, dpi);
  page_css_params.margin_left =
      ConvertUnit(description.margin_left, kPixelsPerInch, dpi);
  return page_css_params;
}

double FitPrintParamsToPage(const PrintMsg_Print_Params& page_params,
                            PrintMsg_Print_Params* params_to_fit) {
  double content_width =
      static_cast<double>(params_to_fit->content_size.width());
  double content_height =
      static_cast<double>(params_to_fit->content_size.height());
  int default_page_size_height = page_params.page_size.height();
  int default_page_size_width = page_params.page_size.width();
  int css_page_size_height = params_to_fit->page_size.height();
  int css_page_size_width = params_to_fit->page_size.width();

  double scale_factor = 1.0f;
  if (page_params.page_size == params_to_fit->page_size)
    return scale_factor;

  if (default_page_size_width < css_page_size_width ||
      default_page_size_height < css_page_size_height) {
    double ratio_width =
        static_cast<double>(default_page_size_width) / css_page_size_width;
    double ratio_height =
        static_cast<double>(default_page_size_height) / css_page_size_height;
    scale_factor = ratio_width < ratio_height ? ratio_width : ratio_height;
    content_width *= scale_factor;
    content_height *= scale_factor;
  }
  params_to_fit->margin_top = static_cast<int>(
      (default_page_size_height - css_page_size_height * scale_factor) / 2 +
      (params_to_fit->margin_top * scale_factor));
  params_to_fit->margin_left = static_cast<int>(
      (default_page_size_width - css_page_size_width * scale_factor) / 2 +
      (params_to_fit->margin_left * scale_factor));
  params_to_fit->content_size = gfx::Size(static_cast<int>(content_width),
                                          static_cast<int>(content_height));
  params_to_fit->page_size = page_params.page_size;
  return scale_factor;
}

void CalculatePageLayoutFromPrintParams(
    const PrintMsg_Print_Params& params,
    double scale_factor,
    PageSizeMargins* page_layout_in_points) {
  bool fit_to_page = IsWebPrintScalingOptionFitToPage(params);
  int dpi = GetDPI(params);
  int content_width = params.content_size.width();
  int content_height = params.content_size.height();
  // Scale the content to its normal size for purpose of computing page layout.
  // Otherwise we will get negative margins.
  bool scale = fit_to_page || params.print_to_pdf;
  if (scale && scale_factor >= PrintRenderFrameHelper::kEpsilon) {
    content_width =
        static_cast<int>(static_cast<double>(content_width) * scale_factor);
    content_height =
        static_cast<int>(static_cast<double>(content_height) * scale_factor);
  }

  int margin_bottom =
      params.page_size.height() - content_height - params.margin_top;
  int margin_right =
      params.page_size.width() - content_width - params.margin_left;

  page_layout_in_points->content_width =
      ConvertUnit(content_width, dpi, kPointsPerInch);
  page_layout_in_points->content_height =
      ConvertUnit(content_height, dpi, kPointsPerInch);
  page_layout_in_points->margin_top =
      ConvertUnit(params.margin_top, dpi, kPointsPerInch);
  page_layout_in_points->margin_right =
      ConvertUnit(margin_right, dpi, kPointsPerInch);
  page_layout_in_points->margin_bottom =
      ConvertUnit(margin_bottom, dpi, kPointsPerInch);
  page_layout_in_points->margin_left =
      ConvertUnit(params.margin_left, dpi, kPointsPerInch);
}

void EnsureOrientationMatches(const PrintMsg_Print_Params& css_params,
                              PrintMsg_Print_Params* page_params) {
  if ((page_params->page_size.width() > page_params->page_size.height()) ==
      (css_params.page_size.width() > css_params.page_size.height())) {
    return;
  }

  // Swap the |width| and |height| values.
  page_params->page_size.SetSize(page_params->page_size.height(),
                                 page_params->page_size.width());
  page_params->content_size.SetSize(page_params->content_size.height(),
                                    page_params->content_size.width());
  page_params->printable_area.set_size(
      gfx::Size(page_params->printable_area.height(),
                page_params->printable_area.width()));
}

void ComputeWebKitPrintParamsInDesiredDpi(
    const PrintMsg_Print_Params& print_params,
    bool source_is_pdf,
    blink::WebPrintParams* webkit_print_params) {
  int dpi = GetDPI(print_params);
  webkit_print_params->printer_dpi = dpi;
  if (source_is_pdf) {
    // The |scale_factor| in print_params comes from the |scale_factor| in
    // PrintSettings, which converts an integer percentage between 10 and 200
    // to a float in PrintSettingsFromJobSettings. As a result, it can be
    // converted back safely for the integer |scale_factor| in WebPrintParams.
    webkit_print_params->scale_factor =
        static_cast<int>(print_params.scale_factor * 100);

#if defined(OS_MACOSX)
    // For Mac, GetDPI() returns a value that avoids DPI-based scaling. This is
    // correct except when rastering PDFs, which uses |printer_dpi|, and the
    // value for |printer_dpi| is too low. Adjust that here.
    // See https://crbug.com/943462
    webkit_print_params->printer_dpi = kDefaultPdfDpi;
#endif
  }
  webkit_print_params->rasterize_pdf = print_params.rasterize_pdf;
  webkit_print_params->print_scaling_option = print_params.print_scaling_option;

  webkit_print_params->print_content_area.width =
      ConvertUnit(print_params.content_size.width(), dpi, kPointsPerInch);
  webkit_print_params->print_content_area.height =
      ConvertUnit(print_params.content_size.height(), dpi, kPointsPerInch);

  webkit_print_params->printable_area.x =
      ConvertUnit(print_params.printable_area.x(), dpi, kPointsPerInch);
  webkit_print_params->printable_area.y =
      ConvertUnit(print_params.printable_area.y(), dpi, kPointsPerInch);
  webkit_print_params->printable_area.width =
      ConvertUnit(print_params.printable_area.width(), dpi, kPointsPerInch);
  webkit_print_params->printable_area.height =
      ConvertUnit(print_params.printable_area.height(), dpi, kPointsPerInch);

  webkit_print_params->paper_size.width =
      ConvertUnit(print_params.page_size.width(), dpi, kPointsPerInch);
  webkit_print_params->paper_size.height =
      ConvertUnit(print_params.page_size.height(), dpi, kPointsPerInch);

  // The following settings is for N-up mode.
  webkit_print_params->pages_per_sheet = print_params.pages_per_sheet;
}

blink::WebPlugin* GetPlugin(const blink::WebLocalFrame* frame) {
  return frame->GetDocument().IsPluginDocument()
             ? frame->GetDocument().To<blink::WebPluginDocument>().Plugin()
             : nullptr;
}

bool IsPrintingNodeOrPdfFrame(const blink::WebLocalFrame* frame,
                              const blink::WebNode& node) {
  if (!node.IsNull())
    return true;
  blink::WebPlugin* plugin = GetPlugin(frame);
  return plugin && plugin->SupportsPaginatedPrint();
}

bool IsPrintingPdf(blink::WebLocalFrame* frame, const blink::WebNode& node) {
  blink::WebPlugin* plugin;
  if (node.IsNull()) {
    plugin = GetPlugin(frame);
  } else {
    blink::WebPluginContainer* plugin_container = node.PluginContainer();
    plugin = plugin_container ? plugin_container->Plugin() : nullptr;
  }
  return plugin && plugin->IsPdfPlugin();
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
bool IsPrintToPdfRequested(const base::DictionaryValue& job_settings) {
  PrinterType type = static_cast<PrinterType>(
      job_settings.FindIntKey(kSettingPrinterType).value());
  return type == PrinterType::kPdf;
}

bool PrintingFrameHasPageSizeStyle(blink::WebLocalFrame* frame,
                                   int total_page_count) {
  if (!frame)
    return false;
  bool frame_has_custom_page_size_style = false;
  for (int i = 0; i < total_page_count; ++i) {
    if (frame->GetPageSizeType(i) != blink::PageSizeType::kAuto) {
      // TODO(crbug.com/1016235): We should propagate the page size type all the
      // way to the UI. See the crbug issue for details.
      frame_has_custom_page_size_style = true;
      break;
    }
  }
  return frame_has_custom_page_size_style;
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#if BUILDFLAG(ENABLE_PRINTING)
// Disable scaling when either:
// - The PDF specifies disabling scaling.
// - All the pages in the PDF are the same size,
// - |ignore_page_size| is false and the uniform size is the same as the paper
//   size.
bool PDFShouldDisableScalingBasedOnPreset(
    const blink::WebPrintPresetOptions& options,
    const PrintMsg_Print_Params& params,
    bool ignore_page_size) {
  if (options.is_scaling_disabled)
    return true;

  if (!options.is_page_size_uniform)
    return false;

  int dpi = GetDPI(params);
  if (!dpi) {
    // Likely |params| is invalid, in which case the return result does not
    // matter. Check for this so ConvertUnit() does not divide by zero.
    return true;
  }

  if (ignore_page_size)
    return false;

  blink::WebSize page_size(
      ConvertUnit(params.page_size.width(), dpi, kPointsPerInch),
      ConvertUnit(params.page_size.height(), dpi, kPointsPerInch));
  return options.uniform_page_size == page_size;
}

bool PDFShouldDisableScaling(blink::WebLocalFrame* frame,
                             const blink::WebNode& node,
                             const PrintMsg_Print_Params& params,
                             bool ignore_page_size) {
  const bool kDefaultPDFShouldDisableScalingSetting = true;
  blink::WebPrintPresetOptions preset_options;
  if (!frame->GetPrintPresetOptionsForPlugin(node, &preset_options))
    return kDefaultPDFShouldDisableScalingSetting;
  return PDFShouldDisableScalingBasedOnPreset(preset_options, params,
                                              ignore_page_size);
}
#endif

MarginType GetMarginsForPdf(blink::WebLocalFrame* frame,
                            const blink::WebNode& node,
                            const PrintMsg_Print_Params& params) {
  return PDFShouldDisableScaling(frame, node, params, false)
             ? NO_MARGINS
             : PRINTABLE_AREA_MARGINS;
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
gfx::Size GetPdfPageSize(const gfx::Size& page_size, int dpi) {
  return gfx::Size(ConvertUnit(page_size.width(), dpi, kPointsPerInch),
                   ConvertUnit(page_size.height(), dpi, kPointsPerInch));
}

ScalingType ScalingTypeFromJobSettings(
    const base::DictionaryValue& job_settings) {
  int scaling_type = job_settings.FindIntKey(kSettingScalingType).value();
  return static_cast<ScalingType>(scaling_type);
}

// Returns the print scaling option to retain/scale/crop the source page size
// to fit the printable area of the paper.
//
// We retain the source page size when the current destination printer is
// SAVE_AS_PDF.
//
// We crop the source page size to fit the printable area or we print only the
// left top page contents when
// (1) Source is PDF and the user has requested not to fit to printable area
// via |job_settings|.
// (2) Source is PDF. This is the first preview request and print scaling
// option is disabled for initiator renderer plugin.
//
// In all other cases, we scale the source page to fit the printable area.
blink::WebPrintScalingOption GetPrintScalingOption(
    blink::WebLocalFrame* frame,
    const blink::WebNode& node,
    bool source_is_html,
    const base::DictionaryValue& job_settings,
    const PrintMsg_Print_Params& params) {
  if (params.print_to_pdf)
    return blink::kWebPrintScalingOptionSourceSize;

  if (!source_is_html) {
    ScalingType scaling_type = ScalingTypeFromJobSettings(job_settings);
    // The following conditions are ordered for an optimization that avoids
    // calling PDFShouldDisableScaling(), which has to make a call using PPAPI.
    if (scaling_type == DEFAULT || scaling_type == CUSTOM)
      return blink::kWebPrintScalingOptionNone;
    if (params.is_first_request &&
        PDFShouldDisableScaling(frame, node, params, true)) {
      return blink::kWebPrintScalingOptionNone;
    }
    if (scaling_type == FIT_TO_PAPER)
      return blink::kWebPrintScalingOptionFitToPaper;
  }
  return blink::kWebPrintScalingOptionFitToPrintableArea;
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

// Helper function to scale and round an integer value with a double valued
// scaling.
int ScaleAndRound(int value, double scaling) {
  return static_cast<int>(static_cast<double>(value) / scaling);
}

// Helper function to scale the width and height of a gfx::Size by scaling.
gfx::Size ScaleAndRoundSize(gfx::Size original, double scaling) {
  return gfx::Size(ScaleAndRound(original.width(), scaling),
                   ScaleAndRound(original.height(), scaling));
}

PrintMsg_Print_Params CalculatePrintParamsForCss(
    blink::WebLocalFrame* frame,
    int page_index,
    const PrintMsg_Print_Params& page_params,
    bool ignore_css_margins,
    bool fit_to_page,
    double* scale_factor) {
  PrintMsg_Print_Params css_params =
      GetCssPrintParams(frame, page_index, page_params);

  PrintMsg_Print_Params params = page_params;
  EnsureOrientationMatches(css_params, &params);

  params.content_size = ScaleAndRoundSize(params.content_size, *scale_factor);
  if (ignore_css_margins && fit_to_page)
    return params;

  PrintMsg_Print_Params result_params = css_params;
  // If not printing a pdf or fitting to page, scale the page size.
  bool scale = !params.print_to_pdf;
  double page_scaling = scale ? *scale_factor : 1.0f;
  if (!fit_to_page) {
    result_params.page_size =
        ScaleAndRoundSize(result_params.page_size, page_scaling);
  }
  if (ignore_css_margins) {
    // Since not fitting to page, scale the page size and margins.
    params.margin_left = ScaleAndRound(params.margin_left, page_scaling);
    params.margin_top = ScaleAndRound(params.margin_top, page_scaling);
    params.page_size = ScaleAndRoundSize(params.page_size, page_scaling);

    result_params.margin_top = params.margin_top;
    result_params.margin_left = params.margin_left;

    DCHECK(!fit_to_page);
    // Since we are ignoring the margins, the css page size is no longer
    // valid for content.
    int default_margin_right = params.page_size.width() -
                               params.content_size.width() - params.margin_left;
    int default_margin_bottom = params.page_size.height() -
                                params.content_size.height() -
                                params.margin_top;
    result_params.content_size =
        gfx::Size(result_params.page_size.width() - result_params.margin_left -
                      default_margin_right,
                  result_params.page_size.height() - result_params.margin_top -
                      default_margin_bottom);
  } else {
    // Using the CSS parameters. Scale CSS content size.
    result_params.content_size =
        ScaleAndRoundSize(result_params.content_size, *scale_factor);
    if (fit_to_page) {
      double factor = FitPrintParamsToPage(params, &result_params);
      if (scale_factor)
        *scale_factor *= factor;
    } else {
      // Already scaled the page, need to also scale the CSS margins since they
      // are begin applied
      result_params.margin_left =
          ScaleAndRound(result_params.margin_left, page_scaling);
      result_params.margin_top =
          ScaleAndRound(result_params.margin_top, page_scaling);
    }
  }

  return result_params;
}

bool CopyMetafileDataToReadOnlySharedMem(
    const MetafileSkia& metafile,
    PrintHostMsg_DidPrintContent_Params* params) {
  uint32_t buf_size = metafile.GetDataSize();
  if (buf_size == 0)
    return false;

  TRACE_EVENT1("print", "CopyMetafileDataToReadOnlySharedMem", "size",
               buf_size);

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(buf_size);
  if (!region_mapping.IsValid())
    return false;

  if (!metafile.GetData(region_mapping.mapping.memory(), buf_size))
    return false;

  params->metafile_data_region = std::move(region_mapping.region);
  params->subframe_content_info = metafile.GetSubframeContentInfo();
  return true;
}

}  // namespace

FrameReference::FrameReference(blink::WebLocalFrame* frame) {
  Reset(frame);
}

FrameReference::FrameReference() {
  Reset(nullptr);
}

FrameReference::~FrameReference() {}

void FrameReference::Reset(blink::WebLocalFrame* frame) {
  if (frame) {
    view_ = frame->View();
    // Make sure this isn't called too early in the |frame| lifecycle... i.e.
    // calling this in WebLocalFrameClient::BindToFrame() doesn't work.
    // TODO(dcheng): It's a bit awkward that lifetime details like this leak out
    // of Blink. Fixing https://crbug.com/727166 should allow this to be
    // addressed.
    DCHECK(view_);
    frame_ = frame;
  } else {
    view_ = nullptr;
    frame_ = nullptr;
  }
}

blink::WebLocalFrame* FrameReference::GetFrame() {
  if (!view_ || !frame_)
    return nullptr;
  for (blink::WebFrame* frame = view_->MainFrame(); frame;
       frame = frame->TraverseNext()) {
    if (frame == frame_)
      return frame_;
  }
  return nullptr;
}

blink::WebView* FrameReference::view() {
  return view_;
}

// static
double PrintRenderFrameHelper::GetScaleFactor(double input_scale_factor,
                                              bool is_pdf) {
  if (input_scale_factor >= PrintRenderFrameHelper::kEpsilon && !is_pdf)
    return input_scale_factor;
  return 1.0f;
}

// static - Not anonymous so that platform implementations can use it.
void PrintRenderFrameHelper::PrintHeaderAndFooter(
    cc::PaintCanvas* canvas,
    int page_number,
    int total_pages,
    const blink::WebLocalFrame& source_frame,
    float webkit_scale_factor,
    const PageSizeMargins& page_layout,
    const PrintMsg_Print_Params& params) {
  cc::PaintCanvasAutoRestore auto_restore(canvas, true);
  canvas->scale(1 / webkit_scale_factor, 1 / webkit_scale_factor);

  blink::WebSize page_size(page_layout.margin_left + page_layout.margin_right +
                               page_layout.content_width,
                           page_layout.margin_top + page_layout.margin_bottom +
                               page_layout.content_height);

  blink::WebView* web_view = blink::WebView::Create(
      /*client=*/nullptr,
      /*is_hidden=*/false, /*compositing_enabled=*/false, /*opener=*/nullptr,
      mojo::NullAssociatedReceiver());
  web_view->GetSettings()->SetJavaScriptEnabled(true);

  class HeaderAndFooterClient final : public blink::WebLocalFrameClient {
   public:
    // WebLocalFrameClient:
    void BindToFrame(blink::WebNavigationControl* frame) override {
      frame_ = frame;
    }
    void FrameDetached(DetachType detach_type) override {
      frame_->FrameWidget()->Close();
      frame_->Close();
      frame_ = nullptr;
    }

   private:
    blink::WebNavigationControl* frame_ = nullptr;
  };

  HeaderAndFooterClient frame_client;
  blink::WebLocalFrame* frame = blink::WebLocalFrame::CreateMainFrame(
      web_view, &frame_client, nullptr, base::UnguessableToken::Create(),
      nullptr);

  blink::WebWidgetClient web_widget_client;
  blink::WebFrameWidget::CreateForMainFrame(
      &web_widget_client, frame,
      blink::CrossVariantMojoAssociatedRemote<
          blink::mojom::FrameWidgetHostInterfaceBase>(),
      blink::CrossVariantMojoAssociatedReceiver<
          blink::mojom::FrameWidgetInterfaceBase>(),
      blink::CrossVariantMojoAssociatedRemote<
          blink::mojom::WidgetHostInterfaceBase>(),
      blink::CrossVariantMojoAssociatedReceiver<
          blink::mojom::WidgetInterfaceBase>());
  web_view->DidAttachLocalMainFrame();

  base::Value html(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_PRINT_HEADER_FOOTER_TEMPLATE_PAGE));
  // Load page with script to avoid async operations.
  ExecuteScript(frame, kPageLoadScriptFormat, html);

  auto options = std::make_unique<base::DictionaryValue>();
  options->SetDoubleKey(kSettingHeaderFooterDate, base::Time::Now().ToJsTime());
  options->SetDoubleKey("width", page_size.width);
  options->SetDoubleKey("height", page_size.height);
  options->SetDoubleKey("topMargin", page_layout.margin_top);
  options->SetDoubleKey("bottomMargin", page_layout.margin_bottom);
  options->SetDoubleKey("leftMargin", page_layout.margin_left);
  options->SetDoubleKey("rightMargin", page_layout.margin_right);
  options->SetIntKey("pageNumber", page_number);
  options->SetIntKey("totalPages", total_pages);
  options->SetStringKey("url", params.url);
  base::string16 title = source_frame.GetDocument().Title().Utf16();
  options->SetStringKey("title", title.empty() ? params.title : title);
  options->SetStringKey("headerTemplate", params.header_template);
  options->SetStringKey("footerTemplate", params.footer_template);
  options->SetBoolKey("isRtl", base::i18n::IsRTL());

  ExecuteScript(frame, kPageSetupScriptFormat, *options);

  blink::WebPrintParams webkit_params(page_size);
  webkit_params.printer_dpi = GetDPI(params);

  frame->PrintBegin(webkit_params);
  frame->PrintPage(0, canvas);
  frame->PrintEnd();

  web_view->Close();
}

// static - Not anonymous so that platform implementations can use it.
float PrintRenderFrameHelper::RenderPageContent(blink::WebLocalFrame* frame,
                                                int page_number,
                                                const gfx::Rect& canvas_area,
                                                const gfx::Rect& content_area,
                                                double scale_factor,
                                                cc::PaintCanvas* canvas) {
  TRACE_EVENT1("print", "PrintRenderFrameHelper::RenderPageContent",
               "page_number", page_number);

  cc::PaintCanvasAutoRestore auto_restore(canvas, true);
  canvas->translate((content_area.x() - canvas_area.x()) / scale_factor,
                    (content_area.y() - canvas_area.y()) / scale_factor);
  return frame->PrintPage(page_number, canvas);
}

// Class that calls the Begin and End print functions on the frame and changes
// the size of the view temporarily to support full page printing..
class PrepareFrameAndViewForPrint : public blink::WebViewClient,
                                    public blink::WebWidgetClient,
                                    public blink::WebLocalFrameClient {
 public:
  PrepareFrameAndViewForPrint(const PrintMsg_Print_Params& params,
                              blink::WebLocalFrame* frame,
                              const blink::WebNode& node,
                              bool ignore_css_margins);
  ~PrepareFrameAndViewForPrint() override;

  // Optional. Replaces |frame_| with selection if needed. Will call |on_ready|
  // when completed.
  void CopySelectionIfNeeded(const WebPreferences& preferences,
                             base::OnceClosure on_ready);

  // Prepares frame for printing.
  void StartPrinting();

  blink::WebLocalFrame* frame() { return frame_.GetFrame(); }

  const blink::WebNode& node() const { return node_to_print_; }

  int GetExpectedPageCount() const { return expected_pages_count_; }

  void FinishPrinting();

  bool IsLoadingSelection() {
    // It's not selection if not |owns_web_view_|.
    return owns_web_view_ && frame() && frame()->IsLoading();
  }

 private:
  // blink::WebViewClient:
  void DidStopLoading() override;
  blink::WebScreenInfo GetScreenInfo() override;

  // blink::WebLocalFrameClient:
  void BindToFrame(blink::WebNavigationControl* frame) override;
  blink::WebLocalFrame* CreateChildFrame(
      blink::WebLocalFrame* parent,
      blink::mojom::TreeScopeType scope,
      const blink::WebString& name,
      const blink::WebString& fallback_name,
      const blink::FramePolicy& frame_policy,
      const blink::WebFrameOwnerProperties& frame_owner_properties,
      blink::mojom::FrameOwnerElementType owner_type) override;
  void FrameDetached(DetachType detach_type) override;
  std::unique_ptr<blink::WebURLLoaderFactory> CreateURLLoaderFactory() override;

  void CallOnReady();
  void ResizeForPrinting();
  void RestoreSize();
  void CopySelection(const WebPreferences& preferences);

  FrameReference frame_;
  FrameReference original_frame_;
  blink::WebNavigationControl* navigation_control_ = nullptr;
  blink::WebNode node_to_print_;
  bool owns_web_view_ = false;
  blink::WebPrintParams web_print_params_;
  gfx::Size prev_view_size_;
  gfx::Size prev_scroll_offset_;
  int expected_pages_count_ = 0;
  base::OnceClosure on_ready_;
  const bool should_print_backgrounds_;
  const bool should_print_selection_only_;
  bool is_printing_started_ = false;

  base::WeakPtrFactory<PrepareFrameAndViewForPrint> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrepareFrameAndViewForPrint);
};

PrepareFrameAndViewForPrint::PrepareFrameAndViewForPrint(
    const PrintMsg_Print_Params& params,
    blink::WebLocalFrame* frame,
    const blink::WebNode& node,
    bool ignore_css_margins)
    : frame_(frame),
      original_frame_(frame),
      node_to_print_(node),
      should_print_backgrounds_(params.should_print_backgrounds),
      should_print_selection_only_(params.selection_only) {
  TRACE_EVENT0("print", "PrepareFrameAndViewForPrint");

  PrintMsg_Print_Params print_params = params;
  bool source_is_pdf = IsPrintingNodeOrPdfFrame(frame, node_to_print_);
  if (!should_print_selection_only_) {
    bool fit_to_page =
        ignore_css_margins && IsWebPrintScalingOptionFitToPage(print_params);
    ComputeWebKitPrintParamsInDesiredDpi(params, source_is_pdf,
                                         &web_print_params_);
    frame->PrintBegin(web_print_params_, node_to_print_);
    double scale_factor = PrintRenderFrameHelper::GetScaleFactor(
        print_params.scale_factor, source_is_pdf);
    print_params = CalculatePrintParamsForCss(
        frame, 0, print_params, ignore_css_margins, fit_to_page, &scale_factor);
    frame->PrintEnd();
  }
  ComputeWebKitPrintParamsInDesiredDpi(print_params, source_is_pdf,
                                       &web_print_params_);
}

PrepareFrameAndViewForPrint::~PrepareFrameAndViewForPrint() {
  FinishPrinting();
}

void PrepareFrameAndViewForPrint::ResizeForPrinting() {
  TRACE_EVENT0("print", "PrepareFrameAndViewForPrint::ResizeForPrinting");

  // Layout page according to printer page size. Since WebKit shrinks the
  // size of the page automatically (from 133.3% to 200%) we trick it to
  // think the page is 133.3% larger so the size of the page is correct for
  // minimum (default) scaling.
  // The scaling factor 1.25 was originally chosen as a magic number that
  // was 'about right'. However per https://crbug.com/273306 1.333 seems to be
  // the number that produces output with the correct physical size for elements
  // that are specified in cm, mm, pt etc.
  // This is important for sites that try to fill the page.
  gfx::Size print_layout_size(web_print_params_.print_content_area.width,
                              web_print_params_.print_content_area.height);
  print_layout_size.set_height(
      ScaleAndRound(print_layout_size.height(), kPrintingMinimumShrinkFactor));

  if (!frame())
    return;

  // Plugins do not need to be resized. Resizing the PDF plugin causes a
  // flicker in the top left corner behind the preview. See crbug.com/739973.
  if (IsPrintingNodeOrPdfFrame(frame(), node_to_print_))
    return;

  // Backup size and offset if it's a local frame.
  blink::WebView* web_view = frame_.view();
  if (blink::WebFrame* web_frame = web_view->MainFrame()) {
    // TODO(lukasza, weili): Support restoring scroll offset of a remote main
    // frame - https://crbug.com/734815.
    if (web_frame->IsWebLocalFrame())
      prev_scroll_offset_ = web_frame->ToWebLocalFrame()->GetScrollOffset();
  }

  prev_view_size_ = frame()->LocalRoot()->FrameWidget()->Size();
  frame()->LocalRoot()->FrameWidget()->Resize(print_layout_size);
}

void PrepareFrameAndViewForPrint::StartPrinting() {
  ResizeForPrinting();
  blink::WebView* web_view = frame_.view();
  web_view->GetSettings()->SetShouldPrintBackgrounds(should_print_backgrounds_);
  expected_pages_count_ =
      frame()->PrintBegin(web_print_params_, node_to_print_);
  is_printing_started_ = true;
}

void PrepareFrameAndViewForPrint::CopySelectionIfNeeded(
    const WebPreferences& preferences,
    base::OnceClosure on_ready) {
  on_ready_ = std::move(on_ready);
  if (should_print_selection_only_) {
    CopySelection(preferences);
  } else {
    // Call immediately, async call crashes scripting printing.
    CallOnReady();
  }
}

void PrepareFrameAndViewForPrint::CopySelection(
    const WebPreferences& preferences) {
  ResizeForPrinting();
  frame()->PrintBegin(web_print_params_, node_to_print_);
  std::string html = frame()->SelectionAsMarkup().Utf8();
  frame()->PrintEnd();
  RestoreSize();

  // Create a new WebView with the same settings as the current display one.
  // Except that we disable javascript (don't want any active content running
  // on the page).
  WebPreferences prefs = preferences;
  prefs.javascript_enabled = false;

  blink::WebView* web_view = blink::WebView::Create(
      /*client=*/this,
      /*is_hidden=*/false,
      /*compositing_enabled=*/false,
      /*opener=*/nullptr, mojo::NullAssociatedReceiver());
  content::RenderView::ApplyWebPreferences(prefs, web_view);
  blink::WebLocalFrame* main_frame = blink::WebLocalFrame::CreateMainFrame(
      web_view, this, nullptr, base::UnguessableToken::Create(), nullptr);
  frame_.Reset(main_frame);
  blink::WebFrameWidget::CreateForMainFrame(
      this, main_frame,
      blink::CrossVariantMojoAssociatedRemote<
          blink::mojom::FrameWidgetHostInterfaceBase>(),
      blink::CrossVariantMojoAssociatedReceiver<
          blink::mojom::FrameWidgetInterfaceBase>(),
      blink::CrossVariantMojoAssociatedRemote<
          blink::mojom::WidgetHostInterfaceBase>(),
      blink::CrossVariantMojoAssociatedReceiver<
          blink::mojom::WidgetInterfaceBase>());
  web_view->DidAttachLocalMainFrame();
  node_to_print_.Reset();

  owns_web_view_ = true;

  // When loading is done this will call didStopLoading() and that will do the
  // actual printing.
  navigation_control_->CommitNavigation(
      blink::WebNavigationParams::CreateWithHTMLString(
          html, GURL(url::kAboutBlankURL)),
      nullptr /* extra_data */);
}

blink::WebScreenInfo PrepareFrameAndViewForPrint::GetScreenInfo() {
  return blink::WebScreenInfo();
}

void PrepareFrameAndViewForPrint::DidStopLoading() {
  DCHECK(!on_ready_.is_null());
  // Don't call callback here, because it can delete |this| and WebView that is
  // called didStopLoading.
  frame()
      ->GetTaskRunner(blink::TaskType::kInternalDefault)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&PrepareFrameAndViewForPrint::CallOnReady,
                                weak_ptr_factory_.GetWeakPtr()));
}

void PrepareFrameAndViewForPrint::BindToFrame(
    blink::WebNavigationControl* navigation_control) {
  navigation_control_ = navigation_control;
}

blink::WebLocalFrame* PrepareFrameAndViewForPrint::CreateChildFrame(
    blink::WebLocalFrame* parent,
    blink::mojom::TreeScopeType scope,
    const blink::WebString& name,
    const blink::WebString& fallback_name,
    const blink::FramePolicy& frame_policy,
    const blink::WebFrameOwnerProperties& frame_owner_properties,
    blink::mojom::FrameOwnerElementType frame_owner_type) {
  // This is called when printing a selection and when this selection contains
  // an iframe. This is not supported yet. An empty rectangle will be displayed
  // instead.
  // Please see: https://crbug.com/732780.
  return nullptr;
}

void PrepareFrameAndViewForPrint::FrameDetached(DetachType detach_type) {
  blink::WebLocalFrame* frame = frame_.GetFrame();
  DCHECK(frame);
  frame->FrameWidget()->Close();
  frame->Close();
  navigation_control_ = nullptr;
  frame_.Reset(nullptr);
}

std::unique_ptr<blink::WebURLLoaderFactory>
PrepareFrameAndViewForPrint::CreateURLLoaderFactory() {
  if (base::FeatureList::IsEnabled(
          features::kUseFrameAssociatedLoaderFactory)) {
    blink::WebLocalFrame* frame = original_frame_.GetFrame();
    UMA_HISTOGRAM_BOOLEAN("Printing.FrameIsActiveOnCreateLoaderFactory",
                          !!frame);
    if (!frame) {
      return std::make_unique<blink::WebFailingURLLoaderFactory>();
    }
    return frame->Client()->CreateURLLoaderFactory();
  }
  return blink::Platform::Current()->CreateDefaultURLLoaderFactory();
}

void PrepareFrameAndViewForPrint::CallOnReady() {
  if (on_ready_)
    std::move(on_ready_).Run();  // Can delete |this|.
}

void PrepareFrameAndViewForPrint::RestoreSize() {
  if (!frame())
    return;

  // Do not restore plugins, since they are not resized.
  if (IsPrintingNodeOrPdfFrame(frame(), node_to_print_))
    return;

  frame()->LocalRoot()->FrameWidget()->Resize(prev_view_size_);
  blink::WebView* web_view = frame_.GetFrame()->View();
  if (blink::WebFrame* web_frame = web_view->MainFrame()) {
    // TODO(lukasza, weili): Support restoring scroll offset of a remote main
    // frame - https://crbug.com/734815.
    if (web_frame->IsWebLocalFrame())
      web_frame->ToWebLocalFrame()->SetScrollOffset(prev_scroll_offset_);
  }
}

void PrepareFrameAndViewForPrint::FinishPrinting() {
  TRACE_EVENT0("print", "PrepareFrameAndViewForPrint::FinishPrinting");

  blink::WebLocalFrame* frame = frame_.GetFrame();
  if (frame) {
    blink::WebView* web_view = frame->View();
    if (is_printing_started_) {
      is_printing_started_ = false;
      if (!owns_web_view_) {
        web_view->GetSettings()->SetShouldPrintBackgrounds(false);
        RestoreSize();
      }
      frame->PrintEnd();
    }
    if (owns_web_view_) {
      DCHECK(!frame->IsLoading());
      owns_web_view_ = false;
      web_view->Close();
    }
  }
  navigation_control_ = nullptr;
  frame_.Reset(nullptr);
  on_ready_.Reset();
}

bool PrintRenderFrameHelper::Delegate::IsScriptedPrintEnabled() {
  return true;
}

bool PrintRenderFrameHelper::Delegate::ShouldGenerateTaggedPDF() {
  return false;
}

PrintRenderFrameHelper::PrintRenderFrameHelper(
    content::RenderFrame* render_frame,
    std::unique_ptr<Delegate> delegate)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<PrintRenderFrameHelper>(render_frame),
      delegate_(std::move(delegate)) {
  if (!delegate_->IsPrintPreviewEnabled())
    DisablePreview();

  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&PrintRenderFrameHelper::BindPrintRenderFrameReceiver,
                          weak_ptr_factory_.GetWeakPtr()));
}

PrintRenderFrameHelper::~PrintRenderFrameHelper() {}

// static
void PrintRenderFrameHelper::DisablePreview() {
  g_is_preview_enabled = false;
}

bool PrintRenderFrameHelper::IsScriptInitiatedPrintAllowed(
    blink::WebLocalFrame* frame,
    bool user_initiated) {
  if (!is_printing_enabled_ || !delegate_->IsScriptedPrintEnabled())
    return false;

  // If preview is enabled, then the print dialog is tab modal, and the user
  // can always close the tab on a mis-behaving page (the system print dialog
  // is app modal). If the print was initiated through user action, don't
  // throttle. Or, if the command line flag to skip throttling has been set.
  return user_initiated || g_is_preview_enabled ||
         scripting_throttler_.IsAllowed(frame);
}

void PrintRenderFrameHelper::DidStartNavigation(
    const GURL& url,
    base::Optional<blink::WebNavigationType> navigation_type) {
  is_loading_ = true;
}

void PrintRenderFrameHelper::DidFailProvisionalLoad() {
  DidFinishLoad();
}

void PrintRenderFrameHelper::DidFinishLoad() {
  is_loading_ = false;
  if (!on_stop_loading_closure_.is_null())
    std::move(on_stop_loading_closure_).Run();
}

void PrintRenderFrameHelper::ScriptedPrint(bool user_initiated) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  if (!IsScriptInitiatedPrintAllowed(web_frame, user_initiated))
    return;

  if (delegate_->OverridePrint(web_frame))
    return;

  // Detached documents can't be printed.
  if (!web_frame->GetDocument().GetFrame())
    return;

  if (g_is_preview_enabled) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    print_preview_context_.InitWithFrame(web_frame);
    RequestPrintPreview(PRINT_PREVIEW_SCRIPTED);
#endif
  } else {
    auto weak_this = weak_ptr_factory_.GetWeakPtr();
    web_frame->DispatchBeforePrintEvent();
    if (!weak_this)
      return;
    Print(web_frame, blink::WebNode(), PrintRequestType::kScripted);
    if (weak_this)
      web_frame->DispatchAfterPrintEvent();
  }
  // WARNING: |this| may be gone at this point. Do not do any more work here and
  // just return.
}

void PrintRenderFrameHelper::OnDestruct() {
  if (ipc_nesting_level_ > 0) {
    render_frame_gone_ = true;
    return;
  }
  delete this;
}

void PrintRenderFrameHelper::BindPrintRenderFrameReceiver(
    mojo::PendingAssociatedReceiver<mojom::PrintRenderFrame> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PrintRenderFrameHelper::PrintRequestedPages() {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > 1)
    return;

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  frame->DispatchBeforePrintEvent();
  // Don't print if the RenderFrame is gone.
  if (render_frame_gone_)
    return;

  // If we are printing a PDF extension frame, find the plugin node and print
  // that instead.
  auto plugin = delegate_->GetPdfElement(frame);
  Print(frame, plugin, PrintRequestType::kRegular);
  if (!render_frame_gone_)
    frame->DispatchAfterPrintEvent();
  // WARNING: |this| may be gone at this point. Do not do any more work here and
  // just return.
}

void PrintRenderFrameHelper::PrintForSystemDialog() {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > 1)
    return;
  blink::WebLocalFrame* frame = print_preview_context_.source_frame();
  if (!frame) {
    NOTREACHED();
    return;
  }
  Print(frame, print_preview_context_.source_node(),
        PrintRequestType::kRegular);
  if (!render_frame_gone_)
    frame->DispatchAfterPrintEvent();
  // WARNING: |this| may be gone at this point. Do not do any more work here and
  // just return.
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintRenderFrameHelper::SetPrintPreviewUI(
    mojo::PendingAssociatedRemote<mojom::PrintPreviewUI> preview) {
  preview_ui_.Bind(std::move(preview));
  preview_ui_.set_disconnect_handler(base::BindOnce(
      &PrintRenderFrameHelper::OnPreviewDisconnect, base::Unretained(this)));
}

void PrintRenderFrameHelper::InitiatePrintPreview(
    mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
    bool has_selection) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > 1)
    return;

  if (print_renderer) {
    print_renderer_.Bind(std::move(print_renderer));
    print_preview_context_.SetIsForArc(true);
  }

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  // If we are printing a PDF extension frame, find the plugin node and print
  // that instead.
  auto plugin = delegate_->GetPdfElement(frame);
  if (!plugin.IsNull()) {
    PrintNode(plugin);
    return;
  }
  print_preview_context_.InitWithFrame(frame);
  RequestPrintPreview(has_selection
                          ? PRINT_PREVIEW_USER_INITIATED_SELECTION
                          : PRINT_PREVIEW_USER_INITIATED_ENTIRE_FRAME);
}

void PrintRenderFrameHelper::PrintPreview(base::Value settings) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > 1)
    return;

  print_preview_context_.OnPrintPreview();

  base::UmaHistogramEnumeration(print_preview_context_.IsForArc()
                                    ? "Arc.PrintPreview.PreviewEvent"
                                    : "PrintPreview.PreviewEvent",
                                PREVIEW_EVENT_REQUESTED, PREVIEW_EVENT_MAX);

  if (!print_preview_context_.source_frame()) {
    DidFinishPrinting(FAIL_PREVIEW);
    return;
  }

  if (!UpdatePrintSettings(print_preview_context_.source_frame(),
                           print_preview_context_.source_node(),
                           base::Value::AsDictionaryValue(settings))) {
    if (print_preview_context_.last_error() != PREVIEW_ERROR_BAD_SETTING) {
      DidFinishPrinting(INVALID_SETTINGS);
    } else {
      DidFinishPrinting(FAIL_PREVIEW);
    }
    return;
  }

  // Save the job settings if a PrintRenderer will be used to create the preview
  // document.
  if (print_renderer_)
    print_renderer_job_settings_ = std::move(settings);

  // Set the options from document if we are previewing a pdf and send a
  // message to browser.
  if (print_pages_params_->params.is_first_request &&
      !print_preview_context_.IsModifiable()) {
    mojom::OptionsFromDocumentParamsPtr options = SetOptionsFromPdfDocument();
    if (options) {
      preview_ui_->SetOptionsFromDocument(
          std::move(options), print_pages_params_->params.preview_request_id);
    }
  }

  is_print_ready_metafile_sent_ = false;

  // PDF printer device supports alpha blending.
  print_pages_params_->params.supports_alpha_blend = true;

  PrepareFrameForPreviewDocument();
}

void PrintRenderFrameHelper::OnPrintPreviewDialogClosed() {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  print_preview_context_.source_frame()->DispatchAfterPrintEvent();
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintRenderFrameHelper::PrintFrameContent(
    mojom::PrintFrameContentParamsPtr params) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > 1)
    return;

  // If the last request is not finished yet, do not proceed.
  if (prep_frame_view_) {
    DLOG(ERROR) << "Previous request is still ongoing";
    return;
  }

  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  frame->DispatchBeforePrintEvent();
  if (!weak_this)
    return;

  MetafileSkia metafile(SkiaDocumentType::MSKP, params->document_cookie);
  gfx::Size area_size = params->printable_area.size();
  // Since GetVectorCanvasForNewPage() starts a new recording, it will return
  // a valid canvas.
  cc::PaintCanvas* canvas =
      metafile.GetVectorCanvasForNewPage(area_size, gfx::Rect(area_size), 1.0f);
  DCHECK(canvas);

  canvas->SetPrintingMetafile(&metafile);

  // This subframe doesn't need to fit to the page size, thus we are not using
  // printing layout for it. It just prints with the specified size.
  blink::WebPrintParams web_print_params(area_size,
                                         /*use_printing_layout=*/false);

  // Printing embedded pdf plugin has been broken since pdf plugin viewer was
  // moved out-of-process
  // (https://bugs.chromium.org/p/chromium/issues/detail?id=464269). So don't
  // try to handle pdf plugin element until that bug is fixed.
  {
    TRACE_EVENT0("print", "PrintRenderFrameHelper::PrintFrameContent");
    if (frame->PrintBegin(web_print_params,
                          /*constrain_to_node=*/blink::WebElement())) {
      frame->PrintPage(0, canvas);
    }
    frame->PrintEnd();
  }

  // Done printing. Close the canvas to retrieve the compiled metafile.
  bool ret = metafile.FinishPage();
  DCHECK(ret);

  metafile.FinishFrameContent();

  // Send the printed result back.
  PrintHostMsg_DidPrintContent_Params printed_frame_params;
  if (!CopyMetafileDataToReadOnlySharedMem(metafile, &printed_frame_params)) {
    DLOG(ERROR) << "CopyMetafileDataToSharedMem failed";
    return;
  }

  Send(new PrintHostMsg_DidPrintFrameContent(
      routing_id(), params->document_cookie, printed_frame_params));

  if (!render_frame_gone_)
    frame->DispatchAfterPrintEvent();
}

void PrintRenderFrameHelper::PrintingDone(bool success) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  if (ipc_nesting_level_ > 1)
    return;
  notify_browser_of_print_failure_ = false;
  DidFinishPrinting(success ? OK : FAIL_PRINT);
}

void PrintRenderFrameHelper::SetPrintingEnabled(bool enabled) {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  is_printing_enabled_ = enabled;
}

void PrintRenderFrameHelper::PrintNodeUnderContextMenu() {
  ScopedIPC scoped_ipc(weak_ptr_factory_.GetWeakPtr());
  PrintNode(render_frame()->GetWebFrame()->ContextMenuNode());
}

void PrintRenderFrameHelper::GetPageSizeAndContentAreaFromPageLayout(
    const PageSizeMargins& page_layout_in_points,
    gfx::Size* page_size,
    gfx::Rect* content_area) {
  *page_size = gfx::Size(
      page_layout_in_points.content_width + page_layout_in_points.margin_right +
          page_layout_in_points.margin_left,
      page_layout_in_points.content_height + page_layout_in_points.margin_top +
          page_layout_in_points.margin_bottom);
  *content_area = gfx::Rect(page_layout_in_points.margin_left,
                            page_layout_in_points.margin_top,
                            page_layout_in_points.content_width,
                            page_layout_in_points.content_height);
}

void PrintRenderFrameHelper::UpdateFrameMarginsCssInfo(
    const base::DictionaryValue& settings) {
  base::Optional<int> margins_type = settings.FindIntKey(kSettingMarginsType);
  ignore_css_margins_ =
      margins_type.value_or(DEFAULT_MARGINS) != DEFAULT_MARGINS;
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintRenderFrameHelper::PrepareFrameForPreviewDocument() {
  reset_prep_frame_view_ = false;

  if (!print_pages_params_) {
    print_preview_context_.set_error(PREVIEW_ERROR_ZERO_PAGES);
    DidFinishPrinting(FAIL_PREVIEW);
    return;
  }

  if (CheckForCancel()) {
    // No need to set an error, since |notify_browser_of_print_failure_| is
    // false.
    DidFinishPrinting(FAIL_PREVIEW);
    return;
  }

  // Don't reset loading frame or WebKit will fail assert. Just retry when
  // current selection is loaded.
  if (prep_frame_view_ && prep_frame_view_->IsLoadingSelection()) {
    reset_prep_frame_view_ = true;
    return;
  }

  const PrintMsg_Print_Params& print_params = print_pages_params_->params;
  prep_frame_view_ = std::make_unique<PrepareFrameAndViewForPrint>(
      print_params, print_preview_context_.source_frame(),
      print_preview_context_.source_node(), ignore_css_margins_);
  prep_frame_view_->CopySelectionIfNeeded(
      render_frame()->GetWebkitPreferences(),
      base::BindOnce(&PrintRenderFrameHelper::OnFramePreparedForPreviewDocument,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintRenderFrameHelper::OnFramePreparedForPreviewDocument() {
  if (reset_prep_frame_view_) {
    PrepareFrameForPreviewDocument();
    return;
  }
  CreatePreviewDocumentResult result = CreatePreviewDocument();
  if (result != CREATE_IN_PROGRESS)
    DidFinishPrinting(result == CREATE_SUCCESS ? OK : FAIL_PREVIEW);
}

PrintRenderFrameHelper::CreatePreviewDocumentResult
PrintRenderFrameHelper::CreatePreviewDocument() {
  if (!print_pages_params_ || CheckForCancel())
    return CREATE_FAIL;

  base::UmaHistogramEnumeration(
      print_preview_context_.IsForArc() ? "Arc.PrintPreview.PreviewEvent"
                                        : "PrintPreview.PreviewEvent",
      PREVIEW_EVENT_CREATE_DOCUMENT, PREVIEW_EVENT_MAX);

  const PrintMsg_Print_Params& print_params = print_pages_params_->params;
  const std::vector<int>& pages = print_pages_params_->pages;

  bool require_document_metafile =
      print_renderer_ ||
      print_params.printed_doc_type != SkiaDocumentType::MSKP;
  if (!print_preview_context_.CreatePreviewDocument(
          std::move(prep_frame_view_), pages, print_params.printed_doc_type,
          print_params.document_cookie, require_document_metafile)) {
    return CREATE_FAIL;
  }

  PageSizeMargins default_page_layout;
  double scale_factor = GetScaleFactor(print_params.scale_factor,
                                       !print_preview_context_.IsModifiable());

  ComputePageLayoutInPointsForCss(print_preview_context_.prepared_frame(), 0,
                                  print_params, ignore_css_margins_,
                                  &scale_factor, &default_page_layout);
  bool has_page_size_style =
      PrintingFrameHasPageSizeStyle(print_preview_context_.prepared_frame(),
                                    print_preview_context_.total_page_count());
  int dpi = GetDPI(print_params);

  gfx::Rect printable_area_in_points(
      ConvertUnit(print_params.printable_area.x(), dpi, kPointsPerInch),
      ConvertUnit(print_params.printable_area.y(), dpi, kPointsPerInch),
      ConvertUnit(print_params.printable_area.width(), dpi, kPointsPerInch),
      ConvertUnit(print_params.printable_area.height(), dpi, kPointsPerInch));

  PrintHostMsg_PreviewIds ids(print_params.preview_request_id,
                              print_params.preview_ui_id);

  // Margins: Send default page layout to browser process.
  Send(new PrintHostMsg_DidGetDefaultPageLayout(
      routing_id(), default_page_layout, printable_area_in_points,
      has_page_size_style, ids));

  PrintHostMsg_DidStartPreview_Params params;
  params.page_count = print_preview_context_.total_page_count();
  params.pages_to_render = print_preview_context_.pages_to_render();
  params.pages_per_sheet = print_params.pages_per_sheet;
  params.page_size = GetPdfPageSize(print_params.page_size, dpi);
  params.fit_to_page_scaling =
      GetFitToPageScaleFactor(printable_area_in_points);
  Send(new PrintHostMsg_DidStartPreview(routing_id(), params, ids));
  if (CheckForCancel())
    return CREATE_FAIL;

  // If a PrintRenderer has been provided, use it to create the preview
  // document.
  if (print_renderer_) {
    base::TimeTicks begin_time = base::TimeTicks::Now();
    print_renderer_->CreatePreviewDocument(
        print_renderer_job_settings_.Clone(),
        base::BindOnce(&PrintRenderFrameHelper::OnPreviewDocumentCreated,
                       weak_ptr_factory_.GetWeakPtr(),
                       print_params.document_cookie, begin_time));
    return CREATE_IN_PROGRESS;
  }

  if (print_pages_params_->params.printed_doc_type == SkiaDocumentType::MSKP) {
    // Want modifiable content of MSKP type to be collected into a document
    // during individual page preview generation (to avoid separate document
    // version for composition), notify to prepare to do this collection.
    Send(new PrintHostMsg_DidPrepareDocumentForPreview(
        routing_id(), print_pages_params_->params.document_cookie, ids));
  }

  while (!print_preview_context_.IsFinalPageRendered()) {
    int page_number = print_preview_context_.GetNextPageNumber();
    DCHECK_GE(page_number, 0);
    if (!RenderPreviewPage(page_number))
      return CREATE_FAIL;

    if (CheckForCancel())
      return CREATE_FAIL;

    // We must call PrepareFrameAndViewForPrint::FinishPrinting() (by way of
    // print_preview_context_.AllPagesRendered()) before calling
    // FinalizePrintReadyDocument() when printing a PDF because the plugin
    // code does not generate output until we call FinishPrinting().  We do not
    // generate draft pages for PDFs, so IsFinalPageRendered() and
    // IsLastPageOfPrintReadyMetafile() will be true in the same iteration of
    // the loop.
    if (print_preview_context_.IsFinalPageRendered())
      print_preview_context_.AllPagesRendered();

    if (print_preview_context_.IsLastPageOfPrintReadyMetafile()) {
      DCHECK(print_preview_context_.IsModifiable() ||
             print_preview_context_.IsFinalPageRendered());
      if (!FinalizePrintReadyDocument())
        return CREATE_FAIL;
    }
  }
  print_preview_context_.Finished();
  return CREATE_SUCCESS;
}

bool PrintRenderFrameHelper::RenderPreviewPage(int page_number) {
  TRACE_EVENT1("print", "PrintRenderFrameHelper::RenderPreviewPage",
               "page_number", page_number);

  const PrintMsg_Print_Params& print_params = print_pages_params_->params;
  MetafileSkia* render_metafile = print_preview_context_.metafile();
  std::unique_ptr<MetafileSkia> page_render_metafile;
  if (!render_metafile) {
    // No document metafile means using the print compositor, which will
    // provide the document metafile by combining the individual pages.
    page_render_metafile = std::make_unique<MetafileSkia>(
        print_params.printed_doc_type, print_params.document_cookie);
    CHECK(page_render_metafile->Init());
    render_metafile = page_render_metafile.get();
  }
  base::TimeTicks begin_time = base::TimeTicks::Now();
  double scale_factor = GetScaleFactor(print_params.scale_factor,
                                       !print_preview_context_.IsModifiable());
  PrintPageInternal(print_params, page_number,
                    print_preview_context_.total_page_count(), scale_factor,
                    print_preview_context_.prepared_frame(), render_metafile,
                    nullptr, nullptr);
  print_preview_context_.RenderedPreviewPage(base::TimeTicks::Now() -
                                             begin_time);

  // For non-modifiable content, there is no need to call PreviewPageRendered()
  // since it generally renders very fast. Just render and send the finished
  // document to the browser.
  if (!print_preview_context_.IsModifiable())
    return true;

  // Let the browser know this page has been rendered. Send
  // |page_render_metafile|, which contains the rendering for just this one
  // page. Then the browser can update the user visible print preview one page
  // at a time, instead of waiting for the entire document to be rendered.
  page_render_metafile =
      render_metafile->GetMetafileForCurrentPage(print_params.printed_doc_type);
  return PreviewPageRendered(page_number, std::move(page_render_metafile));
}

bool PrintRenderFrameHelper::FinalizePrintReadyDocument() {
  TRACE_EVENT0("print", "PrintRenderFrameHelper::FinalizePrintReadyDocument");

  DCHECK(!is_print_ready_metafile_sent_);
  print_preview_context_.FinalizePrintReadyDocument();

  PrintHostMsg_DidPreviewDocument_Params preview_params;

  // Modifiable content of MSKP type is collected into a document during
  // individual page preview generation, so only need to share a separate
  // document version for composition when it isn't MSKP or is from a
  // separate print renderer (e.g., not print compositor).
  MetafileSkia* metafile = print_preview_context_.metafile();
  if (metafile) {
    if (!CopyMetafileDataToReadOnlySharedMem(*metafile,
                                             &preview_params.content)) {
      LOG(ERROR) << "CopyMetafileDataToReadOnlySharedMem failed";
      print_preview_context_.set_error(PREVIEW_ERROR_METAFILE_COPY_FAILED);
      return false;
    }
  }

  preview_params.document_cookie = print_pages_params_->params.document_cookie;
  preview_params.expected_pages_count =
      print_preview_context_.pages_rendered_count();

  PrintHostMsg_PreviewIds ids(print_pages_params_->params.preview_request_id,
                              print_pages_params_->params.preview_ui_id);

  is_print_ready_metafile_sent_ = true;

  Send(new PrintHostMsg_MetafileReadyForPrinting(routing_id(), preview_params,
                                                 ids));
  return true;
}

void PrintRenderFrameHelper::OnPreviewDocumentCreated(
    int document_cookie,
    base::TimeTicks begin_time,
    base::ReadOnlySharedMemoryRegion preview_document_region) {
  // Since the PrintRenderer renders preview documents asynchronously, multiple
  // preview document requests may be sent before a preview document is
  // returned. If the received preview document's cookie does not match the
  // latest document cookie, ignore it and wait for the final preview document.
  if (document_cookie != print_pages_params_->params.document_cookie) {
    return;
  }

  bool success =
      ProcessPreviewDocument(begin_time, std::move(preview_document_region));
  DidFinishPrinting(success ? OK : FAIL_PREVIEW);
}

bool PrintRenderFrameHelper::ProcessPreviewDocument(
    base::TimeTicks begin_time,
    base::ReadOnlySharedMemoryRegion preview_document_region) {
  // Record the render time for the entire document.
  print_preview_context_.RenderedPreviewDocument(base::TimeTicks::Now() -
                                                 begin_time);

  base::ReadOnlySharedMemoryMapping preview_document_mapping =
      preview_document_region.Map();
  if (!preview_document_mapping.IsValid())
    return false;

  CHECK(print_preview_context_.metafile()->InitFromData(
      preview_document_mapping.GetMemoryAsSpan<const uint8_t>()));

  if (CheckForCancel())
    return false;

  print_preview_context_.AllPagesRendered();
  if (!FinalizePrintReadyDocument())
    return false;

  print_preview_context_.Finished();
  return true;
}

int PrintRenderFrameHelper::GetFitToPageScaleFactor(
    const gfx::Rect& printable_area_in_points) {
  if (print_preview_context_.IsModifiable())
    return 100;

  blink::WebLocalFrame* frame = print_preview_context_.source_frame();
  const blink::WebNode& node = print_preview_context_.source_node();
  blink::WebPrintPresetOptions preset_options;
  if (!frame->GetPrintPresetOptionsForPlugin(node, &preset_options))
    return 100;

  if (!preset_options.is_page_size_uniform)
    return 0;

  // Ensure we do not divide by 0 later.
  const auto& uniform_page_size = preset_options.uniform_page_size;
  if (uniform_page_size.width == 0 || uniform_page_size.height == 0)
    return 0;

  // Figure out if the sizes have the same orientation
  bool is_printable_area_landscape =
      printable_area_in_points.width() > printable_area_in_points.height();
  bool is_preset_landscape = uniform_page_size.width > uniform_page_size.height;
  bool rotate = is_printable_area_landscape != is_preset_landscape;
  // Match orientation for computing scaling
  double printable_width = rotate ? printable_area_in_points.height()
                                  : printable_area_in_points.width();
  double printable_height = rotate ? printable_area_in_points.width()
                                   : printable_area_in_points.height();

  double scale_width =
      printable_width / static_cast<double>(uniform_page_size.width);
  double scale_height =
      printable_height / static_cast<double>(uniform_page_size.height);
  return static_cast<int>(100.0f * std::min(scale_width, scale_height));
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

bool PrintRenderFrameHelper::IsPrintingEnabled() const {
  return is_printing_enabled_;
}

void PrintRenderFrameHelper::PrintNode(const blink::WebNode& node) {
  if (node.IsNull() || !node.GetDocument().GetFrame()) {
    // This can occur when the context menu refers to an invalid WebNode.
    // See http://crbug.com/100890#c17 for a repro case.
    return;
  }

  if (print_node_in_progress_) {
    // This can happen as a result of processing sync messages when printing
    // from ppapi plugins. It's a rare case, so its OK to just fail here.
    // See http://crbug.com/159165.
    return;
  }

  print_node_in_progress_ = true;

  if (g_is_preview_enabled) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    print_preview_context_.InitWithNode(node);
    RequestPrintPreview(PRINT_PREVIEW_USER_INITIATED_CONTEXT_NODE);
#endif
  } else {
    // Make a copy of the node, in case RenderView::OnContextMenuClosed() resets
    // its |context_menu_node_|.
    blink::WebNode duplicate_node(node);

    auto self = weak_ptr_factory_.GetWeakPtr();
    Print(duplicate_node.GetDocument().GetFrame(), duplicate_node,
          PrintRequestType::kRegular);
    // Check if |this| is still valid.
    if (!self)
      return;
  }

  print_node_in_progress_ = false;
}

void PrintRenderFrameHelper::Print(blink::WebLocalFrame* frame,
                                   const blink::WebNode& node,
                                   PrintRequestType print_request_type) {
  // If still not finished with earlier print request simply ignore.
  if (prep_frame_view_)
    return;

  FrameReference frame_ref(frame);

  int expected_page_count = 0;
  if (!CalculateNumberOfPages(frame, node, &expected_page_count)) {
    DidFinishPrinting(FAIL_PRINT_INIT);
    return;  // Failed to init print page settings.
  }

  // Some full screen plugins can say they don't want to print.
  if (!expected_page_count) {
    DidFinishPrinting(FAIL_PRINT);
    return;
  }

  // Ask the browser to show UI to retrieve the final print settings.
  {
    // PrintHostMsg_ScriptedPrint in GetPrintSettingsFromUser() will reset
    // |print_scaling_option|, so save the value here and restore it afterwards.
    blink::WebPrintScalingOption scaling_option =
        print_pages_params_->params.print_scaling_option;

    PrintMsg_PrintPages_Params print_settings;
    auto self = weak_ptr_factory_.GetWeakPtr();
    GetPrintSettingsFromUser(frame_ref.GetFrame(), node, expected_page_count,
                             print_request_type, &print_settings);
    // Check if |this| is still valid.
    if (!self)
      return;

    print_settings.params.print_scaling_option =
        print_settings.params.prefer_css_page_size
            ? blink::kWebPrintScalingOptionSourceSize
            : scaling_option;
    SetPrintPagesParams(print_settings);
    if (print_settings.params.dpi.IsEmpty() ||
        !print_settings.params.document_cookie) {
      DidFinishPrinting(OK);  // Release resources and fail silently on failure.
      return;
    }
  }

  // Render Pages for printing.
  if (!RenderPagesForPrint(frame_ref.GetFrame(), node)) {
    LOG(ERROR) << "RenderPagesForPrint failed";
    DidFinishPrinting(FAIL_PRINT);
  }
  scripting_throttler_.Reset();
}

void PrintRenderFrameHelper::DidFinishPrinting(PrintingResult result) {
  int cookie =
      print_pages_params_ ? print_pages_params_->params.document_cookie : 0;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  PrintHostMsg_PreviewIds ids;
  if (print_pages_params_) {
    ids.ui_id = print_pages_params_->params.preview_ui_id;
    ids.request_id = print_pages_params_->params.preview_request_id;
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
  switch (result) {
    case OK:
      break;

    case FAIL_PRINT_INIT:
      DCHECK(!notify_browser_of_print_failure_);
      break;

    case FAIL_PRINT:
      if (notify_browser_of_print_failure_ && print_pages_params_) {
        Send(new PrintHostMsg_PrintingFailed(routing_id(), cookie));
      }
      break;

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    case FAIL_PREVIEW:
      if (!is_print_ready_metafile_sent_) {
        if (notify_browser_of_print_failure_) {
          LOG(ERROR) << "CreatePreviewDocument failed";
          preview_ui_->PrintPreviewFailed(cookie, ids.request_id);
        } else {
          Send(new PrintHostMsg_PrintPreviewCancelled(routing_id(), cookie,
                                                      ids));
        }
      }
      print_preview_context_.Failed(notify_browser_of_print_failure_);
      break;
    case INVALID_SETTINGS:
      Send(new PrintHostMsg_PrintPreviewInvalidPrinterSettings(routing_id(),
                                                               cookie, ids));
      print_preview_context_.Failed(false);
      break;
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
  }
  prep_frame_view_.reset();
  print_pages_params_.reset();
  notify_browser_of_print_failure_ = true;
  snapshotter_.reset();
}

void PrintRenderFrameHelper::OnFramePreparedForPrintPages() {
  PrintPages();
  FinishFramePrinting();
}

void PrintRenderFrameHelper::PrintPages() {
  if (!prep_frame_view_)  // Printing is already canceled or failed.
    return;

  prep_frame_view_->StartPrinting();

  int page_count = prep_frame_view_->GetExpectedPageCount();
  if (!page_count) {
    LOG(ERROR) << "Can't print 0 pages.";
    return DidFinishPrinting(FAIL_PRINT);
  }

  const PrintMsg_PrintPages_Params& params = *print_pages_params_;
  const PrintMsg_Print_Params& print_params = params.params;

  // TODO(vitalybuka): should be page_count or valid pages from params.pages.
  // See http://crbug.com/161576
  Send(new PrintHostMsg_DidGetPrintedPagesCount(
      routing_id(), print_params.document_cookie, page_count));

  if (print_params.preview_ui_id < 0) {
    // Printing for system dialog.
    int printed_count = params.pages.empty() ? page_count : params.pages.size();
    base::UmaHistogramCounts1M("PrintPreview.PageCount.SystemDialog",
                               printed_count);
  }

  bool is_pdf = IsPrintingNodeOrPdfFrame(prep_frame_view_->frame(),
                                         prep_frame_view_->node());
  if (!PrintPagesNative(prep_frame_view_->frame(), page_count, is_pdf)) {
    LOG(ERROR) << "Printing failed.";
    return DidFinishPrinting(FAIL_PRINT);
  }
}

bool PrintRenderFrameHelper::PrintPagesNative(blink::WebLocalFrame* frame,
                                              int page_count,
                                              bool is_pdf) {
  const PrintMsg_PrintPages_Params& params = *print_pages_params_;
  const PrintMsg_Print_Params& print_params = params.params;

  std::vector<int> printed_pages = GetPrintedPages(params, page_count);
  if (printed_pages.empty())
    return false;

  MetafileSkia metafile(print_params.printed_doc_type,
                        print_params.document_cookie);
  CHECK(metafile.Init());

  // If tagged PDF exporting is enabled, we also need to capture an
  // accessibility tree and store it in the metafile. AXTreeSnapshotter
  // should stay alive through the end of this function, because text
  // drawing commands are only annotated with a DOMNodeId if accessibility
  // is enabled.
  std::unique_ptr<content::AXTreeSnapshotter> snapshotter;
  if (delegate_->ShouldGenerateTaggedPDF()) {
    snapshotter = render_frame()->CreateAXTreeSnapshotter();
    snapshotter->Snapshot(ui::kAXModeComplete, 0,
                          &metafile.accessibility_tree());
  }

  PrintHostMsg_DidPrintDocument_Params page_params;
  gfx::Size* page_size_in_dpi;
  gfx::Rect* content_area_in_dpi;
#if defined(OS_MACOSX) || defined(OS_WIN)
  page_size_in_dpi = &page_params.page_size;
  content_area_in_dpi = &page_params.content_area;
#else
  page_size_in_dpi = nullptr;
  content_area_in_dpi = nullptr;
#endif
  PrintPageInternal(print_params, printed_pages[0], page_count,
                    GetScaleFactor(print_params.scale_factor, is_pdf), frame,
                    &metafile, page_size_in_dpi, content_area_in_dpi);
  for (size_t i = 1; i < printed_pages.size(); ++i) {
    PrintPageInternal(print_params, printed_pages[i], page_count,
                      GetScaleFactor(print_params.scale_factor, is_pdf), frame,
                      &metafile, nullptr, nullptr);
  }

  // blink::printEnd() for PDF should be called before metafile is closed.
  FinishFramePrinting();

  metafile.FinishDocument();

  if (!CopyMetafileDataToReadOnlySharedMem(metafile, &page_params.content)) {
    return false;
  }

  page_params.document_cookie = print_params.document_cookie;
#if defined(OS_WIN)
  page_params.physical_offsets = printer_printable_area_.origin();
#endif
  bool completed = false;
  Send(
      new PrintHostMsg_DidPrintDocument(routing_id(), page_params, &completed));
  return completed;
}

void PrintRenderFrameHelper::FinishFramePrinting() {
  prep_frame_view_.reset();
}

// static - Not anonymous so that platform implementations can use it.
void PrintRenderFrameHelper::ComputePageLayoutInPointsForCss(
    blink::WebLocalFrame* frame,
    int page_index,
    const PrintMsg_Print_Params& page_params,
    bool ignore_css_margins,
    double* scale_factor,
    PageSizeMargins* page_layout_in_points) {
  double input_scale_factor = *scale_factor;
  PrintMsg_Print_Params params = CalculatePrintParamsForCss(
      frame, page_index, page_params, ignore_css_margins,
      IsWebPrintScalingOptionFitToPage(page_params), scale_factor);
  CalculatePageLayoutFromPrintParams(params, input_scale_factor,
                                     page_layout_in_points);
}

// static - Not anonymous so that platform implementations can use it.
std::vector<int> PrintRenderFrameHelper::GetPrintedPages(
    const PrintMsg_PrintPages_Params& params,
    int page_count) {
  std::vector<int> printed_pages;
  if (params.pages.empty()) {
    for (int i = 0; i < page_count; ++i) {
      printed_pages.push_back(i);
    }
  } else {
    for (int page : params.pages) {
      if (page >= 0 && page < page_count) {
        printed_pages.push_back(page);
      }
    }
  }
  return printed_pages;
}

void PrintRenderFrameHelper::IPCReceived() {
  // The class is not designed to handle recursive messages. This is not
  // expected during regular flow. However, during rendering of content for
  // printing, lower level code may run a nested run loop. E.g. PDF may have a
  // script to show message box (http://crbug.com/502562). In that moment, the
  // browser may receive updated printer capabilities and decide to restart
  // print preview generation. When this happens, message handling functions may
  // choose to ignore messages or safely crash the process.
  ++ipc_nesting_level_;
}

void PrintRenderFrameHelper::IPCProcessed() {
  --ipc_nesting_level_;
  if (ipc_nesting_level_ == 0 && render_frame_gone_)
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

bool PrintRenderFrameHelper::InitPrintSettings(bool fit_to_paper_size) {
  PrintMsg_PrintPages_Params settings;
  Send(new PrintHostMsg_GetDefaultPrintSettings(routing_id(),
                                                &settings.params));
  // Check if the printer returned any settings, if the settings is empty, we
  // can safely assume there are no printer drivers configured. So we safely
  // terminate.
  bool result = true;
  if (!PrintMsg_Print_Params_IsValid(settings.params))
    result = false;

  // Reset to default values.
  ignore_css_margins_ = false;
  settings.pages.clear();

  settings.params.print_scaling_option =
      fit_to_paper_size ? blink::kWebPrintScalingOptionFitToPrintableArea
                        : blink::kWebPrintScalingOptionSourceSize;

  SetPrintPagesParams(settings);
  return result;
}

bool PrintRenderFrameHelper::CalculateNumberOfPages(blink::WebLocalFrame* frame,
                                                    const blink::WebNode& node,
                                                    int* number_of_pages) {
  DCHECK(frame);
  bool fit_to_paper_size = !IsPrintingNodeOrPdfFrame(frame, node);
  if (!InitPrintSettings(fit_to_paper_size)) {
    notify_browser_of_print_failure_ = false;
    Send(new PrintHostMsg_ShowInvalidPrinterSettingsError(routing_id()));
    return false;
  }

  const PrintMsg_Print_Params& params = print_pages_params_->params;
  PrepareFrameAndViewForPrint prepare(params, frame, node, ignore_css_margins_);
  prepare.StartPrinting();

  *number_of_pages = prepare.GetExpectedPageCount();
  return true;
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
mojom::OptionsFromDocumentParamsPtr
PrintRenderFrameHelper::SetOptionsFromPdfDocument() {
  blink::WebLocalFrame* source_frame = print_preview_context_.source_frame();
  const blink::WebNode& source_node = print_preview_context_.source_node();

  blink::WebPrintPresetOptions preset_options;
  if (!source_frame->GetPrintPresetOptionsForPlugin(source_node,
                                                    &preset_options)) {
    return nullptr;
  }

  return mojom::OptionsFromDocumentParams::New(
      PDFShouldDisableScalingBasedOnPreset(preset_options,
                                           print_pages_params_->params, false),
      preset_options.copies, preset_options.duplex_mode);
}

bool PrintRenderFrameHelper::UpdatePrintSettings(
    blink::WebLocalFrame* frame,
    const blink::WebNode& node,
    const base::DictionaryValue& passed_job_settings) {
  if (passed_job_settings.empty()) {
    // TODO(thestig): Remove this block in the future, when we are certain this
    // is not reachable.
    NOTREACHED();
    print_preview_context_.set_error(PREVIEW_ERROR_BAD_SETTING);
    return false;
  }

  base::DictionaryValue modified_job_settings;
  const base::DictionaryValue* job_settings;
  bool source_is_html = !IsPrintingNodeOrPdfFrame(frame, node);
  if (source_is_html) {
    job_settings = &passed_job_settings;
  } else {
    modified_job_settings.MergeDictionary(&passed_job_settings);
    modified_job_settings.SetBoolKey(kSettingHeaderFooterEnabled, false);
    modified_job_settings.SetIntKey(kSettingMarginsType, NO_MARGINS);
    job_settings = &modified_job_settings;
  }

  // Send the cookie so that UpdatePrintSettings can reuse PrinterQuery when
  // possible.
  int cookie =
      print_pages_params_ ? print_pages_params_->params.document_cookie : 0;
  PrintMsg_PrintPages_Params settings;
  bool canceled = false;
  Send(new PrintHostMsg_UpdatePrintSettings(routing_id(), cookie, *job_settings,
                                            &settings, &canceled));
  if (canceled) {
    notify_browser_of_print_failure_ = false;
    return false;
  }

  // TODO(dhoss): Replace deprecated base::DictionaryValue::Get<Type>() calls
  if (!job_settings->GetInteger(kPreviewUIID, &settings.params.preview_ui_id)) {
    NOTREACHED();
    print_preview_context_.set_error(PREVIEW_ERROR_BAD_SETTING);
    return false;
  }

  // Validate expected print preview settings.
  if (!job_settings->GetInteger(kPreviewRequestID,
                                &settings.params.preview_request_id) ||
      !job_settings->GetBoolean(kIsFirstRequest,
                                &settings.params.is_first_request)) {
    NOTREACHED();
    print_preview_context_.set_error(PREVIEW_ERROR_BAD_SETTING);
    return false;
  }

  settings.params.print_to_pdf = IsPrintToPdfRequested(*job_settings);
  UpdateFrameMarginsCssInfo(*job_settings);
  settings.params.print_scaling_option = GetPrintScalingOption(
      frame, node, source_is_html, *job_settings, settings.params);

  SetPrintPagesParams(settings);

  if (PrintMsg_Print_Params_IsValid(settings.params))
    return true;

  print_preview_context_.set_error(PREVIEW_ERROR_INVALID_PRINTER_SETTINGS);
  return false;
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintRenderFrameHelper::GetPrintSettingsFromUser(
    blink::WebLocalFrame* frame,
    const blink::WebNode& node,
    int expected_pages_count,
    PrintRequestType print_request_type,
    PrintMsg_PrintPages_Params* print_settings) {
  bool is_scripted = print_request_type == PrintRequestType::kScripted;
  DCHECK(is_scripted || print_request_type == PrintRequestType::kRegular);

  PrintHostMsg_ScriptedPrint_Params params;
  params.cookie = print_pages_params_->params.document_cookie;
  params.has_selection = frame->HasSelection();
  params.expected_pages_count = expected_pages_count;
  MarginType margin_type = DEFAULT_MARGINS;
  if (IsPrintingNodeOrPdfFrame(frame, node))
    margin_type = GetMarginsForPdf(frame, node, print_pages_params_->params);
  params.margin_type = margin_type;
  params.is_scripted = is_scripted;
  params.is_modifiable = !IsPrintingNodeOrPdfFrame(frame, node);

  Send(new PrintHostMsg_DidShowPrintDialog(routing_id()));

  print_pages_params_.reset();

  auto msg = std::make_unique<PrintHostMsg_ScriptedPrint>(routing_id(), params,
                                                          print_settings);
  msg->EnableMessagePumping();
  Send(msg.release());
  // WARNING: |this| may be gone at this point. Do not do any more work here
  // and just return.
}

bool PrintRenderFrameHelper::RenderPagesForPrint(blink::WebLocalFrame* frame,
                                                 const blink::WebNode& node) {
  if (!frame || prep_frame_view_)
    return false;

  const PrintMsg_PrintPages_Params& params = *print_pages_params_;
  const PrintMsg_Print_Params& print_params = params.params;
  prep_frame_view_ = std::make_unique<PrepareFrameAndViewForPrint>(
      print_params, frame, node, ignore_css_margins_);
  DCHECK(!print_pages_params_->params.selection_only ||
         print_pages_params_->pages.empty());
  prep_frame_view_->CopySelectionIfNeeded(
      render_frame()->GetWebkitPreferences(),
      base::BindOnce(&PrintRenderFrameHelper::OnFramePreparedForPrintPages,
                     weak_ptr_factory_.GetWeakPtr()));
  return true;
}

#if !defined(OS_MACOSX)
void PrintRenderFrameHelper::PrintPageInternal(
    const PrintMsg_Print_Params& params,
    int page_number,
    int page_count,
    double scale_factor,
    blink::WebLocalFrame* frame,
    MetafileSkia* metafile,
    gfx::Size* page_size_in_dpi,
    gfx::Rect* content_area_in_dpi) {
  double css_scale_factor = scale_factor;

  // Save the original page size here to avoid rounding errors incurred by
  // converting to pixels and back and by scaling the page for reflow and
  // scaling back. Windows uses |page_size_in_dpi| for the actual page size
  // so requires an accurate value.
  gfx::Size original_page_size = params.page_size;
  PageSizeMargins page_layout_in_points;
  ComputePageLayoutInPointsForCss(frame, page_number, params,
                                  ignore_css_margins_, &css_scale_factor,
                                  &page_layout_in_points);

  gfx::Size page_size;
  gfx::Rect content_area;
  GetPageSizeAndContentAreaFromPageLayout(page_layout_in_points, &page_size,
                                          &content_area);

  // Calculate the actual page size and content area in dpi.
  if (page_size_in_dpi)
    *page_size_in_dpi = original_page_size;

  if (content_area_in_dpi) {
    // Output PDF matches paper size and should be printer edge to edge.
    *content_area_in_dpi =
        gfx::Rect(0, 0, page_size_in_dpi->width(), page_size_in_dpi->height());
  }

  gfx::Rect canvas_area =
      params.display_header_footer ? gfx::Rect(page_size) : content_area;

  // TODO(thestig): Figure out why Linux is different.
#if defined(OS_WIN)
  float webkit_page_shrink_factor = frame->GetPrintPageShrink(page_number);
  float final_scale_factor = css_scale_factor * webkit_page_shrink_factor;
#else
  float final_scale_factor = css_scale_factor;
#endif

  cc::PaintCanvas* canvas = metafile->GetVectorCanvasForNewPage(
      page_size, canvas_area, final_scale_factor);
  if (!canvas)
    return;

  canvas->SetPrintingMetafile(metafile);

  if (params.display_header_footer) {
#if defined(OS_WIN)
    const float fudge_factor = 1;
#else
    // TODO(thestig): Figure out why Linux needs this. It is almost certainly
    // |kPrintingMinimumShrinkFactor| from Blink.
    const float fudge_factor = kPrintingMinimumShrinkFactor;
#endif
    // |page_number| is 0-based, so 1 is added.
    PrintHeaderAndFooter(canvas, page_number + 1, page_count, *frame,
                         final_scale_factor / fudge_factor,
                         page_layout_in_points, params);
  }

  float webkit_scale_factor =
      RenderPageContent(frame, page_number, canvas_area, content_area,
                        final_scale_factor, canvas);
  DCHECK_GT(webkit_scale_factor, 0.0f);

  // Done printing. Close the canvas to retrieve the compiled metafile.
  bool ret = metafile->FinishPage();
  DCHECK(ret);
}
#endif  // !defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintRenderFrameHelper::ShowScriptedPrintPreview() {
  if (is_scripted_preview_delayed_) {
    is_scripted_preview_delayed_ = false;
    Send(new PrintHostMsg_ShowScriptedPrintPreview(
        routing_id(), print_preview_context_.IsModifiable()));
  }
}

void PrintRenderFrameHelper::RequestPrintPreview(PrintPreviewRequestType type) {
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  print_preview_context_.source_frame()->DispatchBeforePrintEvent();
  if (!weak_this)
    return;
  const bool is_from_arc = print_preview_context_.IsForArc();
  const bool is_modifiable = print_preview_context_.IsModifiable();
  const bool is_pdf = print_preview_context_.IsPdf();
  const bool has_selection = print_preview_context_.HasSelection();

  // If tagged PDF exporting is enabled, we also need to capture an
  // accessibility tree. AXTreeSnapshotter should stay alive through the end of
  // the scope of printing, because text drawing commands are only annotated
  // with a DOMNodeId if accessibility is enabled.
  if (delegate_->ShouldGenerateTaggedPDF())
    snapshotter_ = render_frame()->CreateAXTreeSnapshotter();

  PrintHostMsg_RequestPrintPreview_Params params;
  params.is_from_arc = is_from_arc;
  params.is_modifiable = is_modifiable;
  params.is_pdf = is_pdf;
  params.has_selection = has_selection;
  switch (type) {
    case PRINT_PREVIEW_SCRIPTED: {
      // Shows scripted print preview in two stages.
      // 1. PrintHostMsg_SetupScriptedPrintPreview blocks this call and JS by
      //    pumping messages here.
      // 2. PrintHostMsg_ShowScriptedPrintPreview shows preview once the
      //    document has been loaded.
      is_scripted_preview_delayed_ = true;
      if (is_loading_ && GetPlugin(print_preview_context_.source_frame())) {
        // Wait for DidStopLoading. Plugins may not know the correct
        // |is_modifiable| value until they are fully loaded, which occurs when
        // DidStopLoading() is called. Defer showing the preview until then.
        on_stop_loading_closure_ =
            base::BindOnce(&PrintRenderFrameHelper::ShowScriptedPrintPreview,
                           weak_ptr_factory_.GetWeakPtr());
      } else {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&PrintRenderFrameHelper::ShowScriptedPrintPreview,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      auto msg = std::make_unique<PrintHostMsg_SetupScriptedPrintPreview>(
          routing_id());
      msg->EnableMessagePumping();
      auto self = weak_ptr_factory_.GetWeakPtr();
      Send(msg.release());
      // Check if |this| is still valid.
      if (self)
        is_scripted_preview_delayed_ = false;
      return;
    }
    case PRINT_PREVIEW_USER_INITIATED_ENTIRE_FRAME: {
      // Wait for DidStopLoading. Continuing with this function while
      // |is_loading_| is true will cause print preview to hang when try to
      // print a PDF document.
      if (is_loading_ && GetPlugin(print_preview_context_.source_frame())) {
        on_stop_loading_closure_ =
            base::BindOnce(&PrintRenderFrameHelper::RequestPrintPreview,
                           weak_ptr_factory_.GetWeakPtr(), type);
        return;
      }

      break;
    }
    case PRINT_PREVIEW_USER_INITIATED_SELECTION: {
      DCHECK(has_selection);
      DCHECK(!GetPlugin(print_preview_context_.source_frame()));
      params.selection_only = has_selection;
      break;
    }
    case PRINT_PREVIEW_USER_INITIATED_CONTEXT_NODE: {
      if (is_loading_ && GetPlugin(print_preview_context_.source_frame())) {
        on_stop_loading_closure_ =
            base::BindOnce(&PrintRenderFrameHelper::RequestPrintPreview,
                           weak_ptr_factory_.GetWeakPtr(), type);
        return;
      }

      params.webnode_only = true;
      break;
    }
    default: {
      NOTREACHED();
      return;
    }
  }

  base::UmaHistogramEnumeration(print_preview_context_.IsForArc()
                                    ? "Arc.PrintPreview.PreviewEvent"
                                    : "PrintPreview.PreviewEvent",
                                PREVIEW_EVENT_INITIATED, PREVIEW_EVENT_MAX);
  Send(new PrintHostMsg_RequestPrintPreview(routing_id(), params));
}

bool PrintRenderFrameHelper::CheckForCancel() {
  const PrintMsg_Print_Params& print_params = print_pages_params_->params;
  bool cancel = false;
  Send(new PrintHostMsg_CheckForCancel(
      routing_id(),
      PrintHostMsg_PreviewIds(print_params.preview_request_id,
                              print_params.preview_ui_id),
      &cancel));
  if (cancel)
    notify_browser_of_print_failure_ = false;
  return cancel;
}

bool PrintRenderFrameHelper::PreviewPageRendered(
    int page_number,
    std::unique_ptr<MetafileSkia> metafile) {
  DCHECK_GE(page_number, FIRST_PAGE_INDEX);
  DCHECK(metafile);
  DCHECK(print_preview_context_.IsModifiable());

  TRACE_EVENT1("print", "PrintRenderFrameHelper::PreviewPageRendered",
               "page_number", page_number);

#if BUILDFLAG(ENABLE_TAGGED_PDF)
  // For tagged PDF exporting, send a snapshot of the accessibility tree
  // along with page 0. The accessibility tree contains the content for
  // all of the pages of the main frame.
  //
  // TODO(dmazzoni) Support multi-frame tagged PDFs.
  // http://crbug.com/1039817
  if (snapshotter_ && page_number == 0) {
    ui::AXTreeUpdate accessibility_tree;
    snapshotter_->Snapshot(ui::kAXModeComplete, 0, &accessibility_tree);
    Send(new PrintHostMsg_AccessibilityTree(
        routing_id(), print_pages_params_->params.document_cookie,
        accessibility_tree));
  }
#endif

  PrintHostMsg_DidPreviewPage_Params preview_page_params;
  if (!CopyMetafileDataToReadOnlySharedMem(*metafile,
                                           &preview_page_params.content)) {
    LOG(ERROR) << "CopyMetafileDataToReadOnlySharedMem failed";
    print_preview_context_.set_error(PREVIEW_ERROR_METAFILE_COPY_FAILED);
    return false;
  }

  preview_page_params.page_number = page_number;
  preview_page_params.document_cookie =
      print_pages_params_->params.document_cookie;

  PrintHostMsg_PreviewIds ids(print_pages_params_->params.preview_request_id,
                              print_pages_params_->params.preview_ui_id);

  Send(new PrintHostMsg_DidPreviewPage(routing_id(), preview_page_params, ids));
  return true;
}

void PrintRenderFrameHelper::OnPreviewDisconnect() {
  preview_ui_.reset();
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

PrintRenderFrameHelper::PrintPreviewContext::PrintPreviewContext() = default;

PrintRenderFrameHelper::PrintPreviewContext::~PrintPreviewContext() = default;

void PrintRenderFrameHelper::PrintPreviewContext::InitWithFrame(
    blink::WebLocalFrame* web_frame) {
  DCHECK(web_frame);
  DCHECK(!IsRendering());
  state_ = INITIALIZED;
  source_frame_.Reset(web_frame);
  source_node_.Reset();
  CalculateIsModifiable();
  CalculateIsPdf();
}

void PrintRenderFrameHelper::PrintPreviewContext::InitWithNode(
    const blink::WebNode& web_node) {
  DCHECK(!web_node.IsNull());
  DCHECK(web_node.GetDocument().GetFrame());
  DCHECK(!IsRendering());
  state_ = INITIALIZED;
  source_frame_.Reset(web_node.GetDocument().GetFrame());
  source_node_ = web_node;
  CalculateIsModifiable();
  CalculateIsPdf();
}

void PrintRenderFrameHelper::PrintPreviewContext::OnPrintPreview() {
  DCHECK_EQ(INITIALIZED, state_);
  ClearContext();
}

bool PrintRenderFrameHelper::PrintPreviewContext::CreatePreviewDocument(
    std::unique_ptr<PrepareFrameAndViewForPrint> prepared_frame,
    const std::vector<int>& pages,
    SkiaDocumentType doc_type,
    int document_cookie,
    bool require_document_metafile) {
  DCHECK_EQ(INITIALIZED, state_);
  state_ = RENDERING;

  // Need to make sure old object gets destroyed first.
  prep_frame_view_ = std::move(prepared_frame);
  prep_frame_view_->StartPrinting();

  total_page_count_ = prep_frame_view_->GetExpectedPageCount();
  if (total_page_count_ == 0) {
    LOG(ERROR) << "CreatePreviewDocument got 0 page count";
    set_error(PREVIEW_ERROR_ZERO_PAGES);
    return false;
  }

  if (require_document_metafile) {
    metafile_ = std::make_unique<MetafileSkia>(doc_type, document_cookie);
    CHECK(metafile_->Init());
  }

  current_page_index_ = 0;
  pages_to_render_ = pages;
  // Sort and make unique.
  std::sort(pages_to_render_.begin(), pages_to_render_.end());
  pages_to_render_.resize(
      std::unique(pages_to_render_.begin(), pages_to_render_.end()) -
      pages_to_render_.begin());
  // Remove invalid pages.
  pages_to_render_.resize(std::lower_bound(pages_to_render_.begin(),
                                           pages_to_render_.end(),
                                           total_page_count_) -
                          pages_to_render_.begin());

  if (pages_to_render_.empty()) {
    // Render all pages.
    pages_to_render_.reserve(total_page_count_);
    for (int i = 0; i < total_page_count_; ++i)
      pages_to_render_.push_back(i);
  }
  print_ready_metafile_page_count_ = pages_to_render_.size();

  document_render_time_ = base::TimeDelta();
  begin_time_ = base::TimeTicks::Now();

  return true;
}

void PrintRenderFrameHelper::PrintPreviewContext::RenderedPreviewPage(
    const base::TimeDelta& page_time) {
  DCHECK_EQ(RENDERING, state_);
  document_render_time_ += page_time;
  base::UmaHistogramTimes("PrintPreview.RenderPDFPageTime", page_time);
}

void PrintRenderFrameHelper::PrintPreviewContext::RenderedPreviewDocument(
    const base::TimeDelta document_time) {
  DCHECK_EQ(RENDERING, state_);
  document_render_time_ += document_time;
}

void PrintRenderFrameHelper::PrintPreviewContext::AllPagesRendered() {
  DCHECK_EQ(RENDERING, state_);
  state_ = DONE;
  prep_frame_view_->FinishPrinting();
}

void PrintRenderFrameHelper::PrintPreviewContext::FinalizePrintReadyDocument() {
  DCHECK(IsRendering());

  base::TimeTicks begin_time = base::TimeTicks::Now();

  if (metafile_)
    metafile_->FinishDocument();

  if (print_ready_metafile_page_count_ <= 0) {
    NOTREACHED();
    return;
  }

  base::TimeDelta total_time =
      (base::TimeTicks::Now() - begin_time) + document_render_time_;
  base::TimeDelta avg_time_per_page = total_time / pages_to_render_.size();

  base::UmaHistogramMediumTimes(is_for_arc_ ? "Arc.PrintPreview.RenderToPDFTime"
                                            : "PrintPreview.RenderToPDFTime",
                                document_render_time_);
  base::UmaHistogramMediumTimes(
      is_for_arc_ ? "Arc.PrintPreview.RenderAndGeneratePDFTime"
                  : "PrintPreview.RenderAndGeneratePDFTime",
      total_time);
  base::UmaHistogramMediumTimes(
      is_for_arc_ ? "Arc.PrintPreview.RenderAndGeneratePDFTimeAvgPerPage"
                  : "PrintPreview.RenderAndGeneratePDFTimeAvgPerPage",
      avg_time_per_page);
}

void PrintRenderFrameHelper::PrintPreviewContext::Finished() {
  DCHECK_EQ(DONE, state_);
  state_ = INITIALIZED;
  ClearContext();
}

void PrintRenderFrameHelper::PrintPreviewContext::Failed(bool report_error) {
  DCHECK(state_ != UNINITIALIZED);
  state_ = INITIALIZED;
  if (report_error) {
    DCHECK_NE(PREVIEW_ERROR_NONE, error_);
    base::UmaHistogramEnumeration(is_for_arc_ ? "Arc.PrintPreview.RendererError"
                                              : "PrintPreview.RendererError",
                                  error_, PREVIEW_ERROR_LAST_ENUM);
  }
  ClearContext();
}

int PrintRenderFrameHelper::PrintPreviewContext::GetNextPageNumber() {
  DCHECK_EQ(RENDERING, state_);
  if (IsFinalPageRendered())
    return -1;
  return pages_to_render_[current_page_index_++];
}

bool PrintRenderFrameHelper::PrintPreviewContext::IsRendering() const {
  return state_ == RENDERING || state_ == DONE;
}

bool PrintRenderFrameHelper::PrintPreviewContext::IsForArc() const {
  DCHECK_NE(state_, UNINITIALIZED);
  return is_for_arc_;
}

bool PrintRenderFrameHelper::PrintPreviewContext::IsModifiable() const {
  DCHECK(state_ != UNINITIALIZED);
  return is_modifiable_;
}

bool PrintRenderFrameHelper::PrintPreviewContext::IsPdf() const {
  DCHECK(state_ != UNINITIALIZED);
  return is_pdf_;
}

bool PrintRenderFrameHelper::PrintPreviewContext::HasSelection() {
  return IsModifiable() && source_frame()->HasSelection();
}

bool PrintRenderFrameHelper::PrintPreviewContext::
    IsLastPageOfPrintReadyMetafile() const {
  DCHECK(IsRendering());
  return current_page_index_ == print_ready_metafile_page_count_;
}

bool PrintRenderFrameHelper::PrintPreviewContext::IsFinalPageRendered() const {
  DCHECK(IsRendering());
  return static_cast<size_t>(current_page_index_) == pages_to_render_.size();
}

void PrintRenderFrameHelper::PrintPreviewContext::SetIsForArc(bool is_for_arc) {
  is_for_arc_ = is_for_arc;
}

void PrintRenderFrameHelper::PrintPreviewContext::set_error(
    enum PrintPreviewErrorBuckets error) {
  error_ = error;
}

blink::WebLocalFrame*
PrintRenderFrameHelper::PrintPreviewContext::source_frame() {
  DCHECK(state_ != UNINITIALIZED);
  return source_frame_.GetFrame();
}

const blink::WebNode& PrintRenderFrameHelper::PrintPreviewContext::source_node()
    const {
  DCHECK(state_ != UNINITIALIZED);
  return source_node_;
}

blink::WebLocalFrame*
PrintRenderFrameHelper::PrintPreviewContext::prepared_frame() {
  DCHECK(state_ != UNINITIALIZED);
  return prep_frame_view_->frame();
}

const blink::WebNode&
PrintRenderFrameHelper::PrintPreviewContext::prepared_node() const {
  DCHECK(state_ != UNINITIALIZED);
  return prep_frame_view_->node();
}

int PrintRenderFrameHelper::PrintPreviewContext::total_page_count() const {
  DCHECK(state_ != UNINITIALIZED);
  return total_page_count_;
}

const std::vector<int>&
PrintRenderFrameHelper::PrintPreviewContext::pages_to_render() const {
  DCHECK_EQ(RENDERING, state_);
  return pages_to_render_;
}

int PrintRenderFrameHelper::PrintPreviewContext::pages_rendered_count() const {
  DCHECK_EQ(DONE, state_);
  return pages_to_render_.size();
}

MetafileSkia* PrintRenderFrameHelper::PrintPreviewContext::metafile() {
  DCHECK(IsRendering());
  return metafile_.get();
}

int PrintRenderFrameHelper::PrintPreviewContext::last_error() const {
  return error_;
}

void PrintRenderFrameHelper::PrintPreviewContext::ClearContext() {
  prep_frame_view_.reset();
  metafile_.reset();
  pages_to_render_.clear();
  error_ = PREVIEW_ERROR_NONE;
}

void PrintRenderFrameHelper::PrintPreviewContext::CalculateIsModifiable() {
  is_modifiable_ = !IsPrintingNodeOrPdfFrame(source_frame(), source_node_);
}

void PrintRenderFrameHelper::PrintPreviewContext::CalculateIsPdf() {
  is_pdf_ = IsPrintingPdf(source_frame(), source_node_);
}

void PrintRenderFrameHelper::SetPrintPagesParams(
    const PrintMsg_PrintPages_Params& settings) {
  print_pages_params_ = std::make_unique<PrintMsg_PrintPages_Params>(settings);
  Send(new PrintHostMsg_DidGetDocumentCookie(routing_id(),
                                             settings.params.document_cookie));
}

PrintRenderFrameHelper::ScopedIPC::ScopedIPC(
    base::WeakPtr<PrintRenderFrameHelper> weak_this)
    : weak_this_(std::move(weak_this)) {
  DCHECK(weak_this_);
  weak_this_->IPCReceived();
}

PrintRenderFrameHelper::ScopedIPC::~ScopedIPC() {
  if (weak_this_)
    weak_this_->IPCProcessed();
}

PrintRenderFrameHelper::ScriptingThrottler::ScriptingThrottler() = default;

bool PrintRenderFrameHelper::ScriptingThrottler::IsAllowed(
    blink::WebLocalFrame* frame) {
  const int kMinSecondsToIgnoreJavascriptInitiatedPrint = 2;
  const int kMaxSecondsToIgnoreJavascriptInitiatedPrint = 32;
  bool too_frequent = false;

  // Check if there is script repeatedly trying to print and ignore it if too
  // frequent.  The first 3 times, we use a constant wait time, but if this
  // gets excessive, we switch to exponential wait time. So for a page that
  // calls print() in a loop the user will need to cancel the print dialog
  // after: [2, 2, 2, 4, 8, 16, 32, 32, ...] seconds.
  // This gives the user time to navigate from the page.
  if (count_ > 0) {
    base::TimeDelta diff = base::Time::Now() - last_print_;
    int min_wait_seconds = kMinSecondsToIgnoreJavascriptInitiatedPrint;
    if (count_ > 3) {
      min_wait_seconds =
          std::min(kMinSecondsToIgnoreJavascriptInitiatedPrint << (count_ - 3),
                   kMaxSecondsToIgnoreJavascriptInitiatedPrint);
    }
    if (diff.InSeconds() < min_wait_seconds) {
      too_frequent = true;
    }
  }

  if (!too_frequent) {
    ++count_;
    last_print_ = base::Time::Now();
    return true;
  }

  blink::WebString message(
      blink::WebString::FromASCII("Ignoring too frequent calls to print()."));
  frame->AddMessageToConsole(blink::WebConsoleMessage(
      blink::mojom::ConsoleMessageLevel::kWarning, message));
  return false;
}

void PrintRenderFrameHelper::ScriptingThrottler::Reset() {
  // Reset counter on successful print.
  count_ = 0;
}

}  // namespace printing
