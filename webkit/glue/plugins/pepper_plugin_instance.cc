// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_plugin_instance.h"

#include "base/logging.h"
#include "base/histogram.h"
#if defined(OS_MACOSX)
#include "base/mac_util.h"
#include "base/scoped_cftyperef.h"
#endif
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "gfx/rect.h"
#if defined(OS_WIN)
#include "gfx/codec/jpeg_codec.h"
#include "gfx/gdi_util.h"
#endif
#include "gfx/skia_util.h"
#include "printing/native_metafile.h"
#include "printing/units.h"
#include "skia/ext/vector_platform_device.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/ppapi/c/dev/ppb_find_dev.h"
#include "third_party/ppapi/c/dev/ppp_find_dev.h"
#include "third_party/ppapi/c/dev/ppp_zoom_dev.h"
#include "third_party/ppapi/c/pp_event.h"
#include "third_party/ppapi/c/pp_instance.h"
#include "third_party/ppapi/c/pp_rect.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "third_party/ppapi/c/pp_var.h"
#include "third_party/ppapi/c/ppb_core.h"
#include "third_party/ppapi/c/ppb_instance.h"
#include "third_party/ppapi/c/ppp_instance.h"
#include "third_party/WebKit/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "webkit/glue/plugins/pepper_buffer.h"
#include "webkit/glue/plugins/pepper_graphics_2d.h"
#include "webkit/glue/plugins/pepper_event_conversion.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_string.h"
#include "webkit/glue/plugins/pepper_url_loader.h"
#include "webkit/glue/plugins/pepper_var.h"

using WebKit::WebBindings;
using WebKit::WebCanvas;
using WebKit::WebCursorInfo;
using WebKit::WebFrame;
using WebKit::WebInputEvent;
using WebKit::WebPluginContainer;

namespace pepper {

#if defined(OS_WIN)
// Exported by pdf.dll
typedef bool (*RenderPDFPageToDCProc)(
    const unsigned char* pdf_buffer, int buffer_size, int page_number, HDC dc,
    int dpi_x, int dpi_y, int bounds_origin_x, int bounds_origin_y,
    int bounds_width, int bounds_height, bool fit_to_bounds,
    bool stretch_to_bounds, bool keep_aspect_ratio, bool center_in_bounds);
#endif  // defined(OS_WIN)

namespace {

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, np_name) \
    COMPILE_ASSERT(int(WebCursorInfo::webkit_name) == int(np_name), \
                   mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(TypePointer, PP_CURSORTYPE_POINTER);
COMPILE_ASSERT_MATCHING_ENUM(TypeCross, PP_CURSORTYPE_CROSS);
COMPILE_ASSERT_MATCHING_ENUM(TypeHand, PP_CURSORTYPE_HAND);
COMPILE_ASSERT_MATCHING_ENUM(TypeIBeam, PP_CURSORTYPE_IBEAM);
COMPILE_ASSERT_MATCHING_ENUM(TypeWait, PP_CURSORTYPE_WAIT);
COMPILE_ASSERT_MATCHING_ENUM(TypeHelp, PP_CURSORTYPE_HELP);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastResize, PP_CURSORTYPE_EASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthResize, PP_CURSORTYPE_NORTHRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastResize,
                             PP_CURSORTYPE_NORTHEASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestResize,
                             PP_CURSORTYPE_NORTHWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthResize, PP_CURSORTYPE_SOUTHRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthEastResize,
                             PP_CURSORTYPE_SOUTHEASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthWestResize,
                             PP_CURSORTYPE_SOUTHWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeWestResize, PP_CURSORTYPE_WESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthSouthResize,
                             PP_CURSORTYPE_NORTHSOUTHRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastWestResize, PP_CURSORTYPE_EASTWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastSouthWestResize,
                             PP_CURSORTYPE_NORTHEASTSOUTHWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestSouthEastResize,
                             PP_CURSORTYPE_NORTHWESTSOUTHEASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeColumnResize, PP_CURSORTYPE_COLUMNRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeRowResize, PP_CURSORTYPE_ROWRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeMiddlePanning, PP_CURSORTYPE_MIDDLEPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastPanning, PP_CURSORTYPE_EASTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthPanning, PP_CURSORTYPE_NORTHPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastPanning,
                             PP_CURSORTYPE_NORTHEASTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestPanning,
                             PP_CURSORTYPE_NORTHWESTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthPanning, PP_CURSORTYPE_SOUTHPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthEastPanning,
                             PP_CURSORTYPE_SOUTHEASTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthWestPanning,
                             PP_CURSORTYPE_SOUTHWESTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeWestPanning, PP_CURSORTYPE_WESTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeMove, PP_CURSORTYPE_MOVE);
COMPILE_ASSERT_MATCHING_ENUM(TypeVerticalText, PP_CURSORTYPE_VERTICALTEXT);
COMPILE_ASSERT_MATCHING_ENUM(TypeCell, PP_CURSORTYPE_CELL);
COMPILE_ASSERT_MATCHING_ENUM(TypeContextMenu, PP_CURSORTYPE_CONTEXTMENU);
COMPILE_ASSERT_MATCHING_ENUM(TypeAlias, PP_CURSORTYPE_ALIAS);
COMPILE_ASSERT_MATCHING_ENUM(TypeProgress, PP_CURSORTYPE_PROGRESS);
COMPILE_ASSERT_MATCHING_ENUM(TypeNoDrop, PP_CURSORTYPE_NODROP);
COMPILE_ASSERT_MATCHING_ENUM(TypeCopy, PP_CURSORTYPE_COPY);
COMPILE_ASSERT_MATCHING_ENUM(TypeNone, PP_CURSORTYPE_NONE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNotAllowed, PP_CURSORTYPE_NOTALLOWED);
COMPILE_ASSERT_MATCHING_ENUM(TypeZoomIn, PP_CURSORTYPE_ZOOMIN);
COMPILE_ASSERT_MATCHING_ENUM(TypeZoomOut, PP_CURSORTYPE_ZOOMOUT);
COMPILE_ASSERT_MATCHING_ENUM(TypeCustom, PP_CURSORTYPE_CUSTOM);

void RectToPPRect(const gfx::Rect& input, PP_Rect* output) {
  *output = PP_MakeRectFromXYWH(input.x(), input.y(),
                                input.width(), input.height());
}

PP_Var GetWindowObject(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return PP_MakeVoid();
  return instance->GetWindowObject();
}

PP_Var GetOwnerElementObject(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return PP_MakeVoid();
  return instance->GetOwnerElementObject();
}

bool BindGraphics(PP_Instance instance_id, PP_Resource device_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return false;
  return instance->BindGraphics(device_id);
}

bool IsFullFrame(PP_Instance instance_id) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return false;
  return instance->full_frame();
}

PP_Var ExecuteScript(PP_Instance instance_id,
                     PP_Var script,
                     PP_Var* exception) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return PP_MakeVoid();
  return instance->ExecuteScript(script, exception);
}

const PPB_Instance ppb_instance = {
  &GetWindowObject,
  &GetOwnerElementObject,
  &BindGraphics,
  &IsFullFrame,
  &ExecuteScript,
};

void NumberOfFindResultsChanged(PP_Instance instance_id,
                                int32_t total,
                                bool final_result) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return;

  DCHECK_NE(instance->find_identifier(), -1);
  instance->delegate()->DidChangeNumberOfFindResults(
      instance->find_identifier(), total, final_result);
}

void SelectedFindResultChanged(PP_Instance instance_id,
                               int32_t index) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return;

  DCHECK_NE(instance->find_identifier(), -1);
  instance->delegate()->DidChangeSelectedFindResult(
      instance->find_identifier(), index);
}

const PPB_Find_Dev ppb_find = {
  &NumberOfFindResultsChanged,
  &SelectedFindResultChanged,
};

}  // namespace

PluginInstance::PluginInstance(PluginDelegate* delegate,
                               PluginModule* module,
                               const PPP_Instance* instance_interface)
    : delegate_(delegate),
      module_(module),
      instance_interface_(instance_interface),
      container_(NULL),
      full_frame_(false),
      find_identifier_(-1),
      plugin_find_interface_(NULL),
      plugin_zoom_interface_(NULL),
#if defined (OS_LINUX)
      num_pages_(0),
      pdf_output_done_(false),
#endif  // defined (OS_LINUX)
      plugin_print_interface_(NULL),
      plugin_graphics_3d_interface_(NULL) {
  memset(&current_print_settings_, 0, sizeof(current_print_settings_));
  DCHECK(delegate);
  module_->InstanceCreated(this);
  delegate_->InstanceCreated(this);
}

PluginInstance::~PluginInstance() {
  delegate_->InstanceDeleted(this);
  module_->InstanceDeleted(this);
}

// static
const PPB_Instance* PluginInstance::GetInterface() {
  return &ppb_instance;
}

// static
PluginInstance* PluginInstance::FromPPInstance(PP_Instance instance) {
  return reinterpret_cast<PluginInstance*>(instance);
}

// static
const PPB_Find_Dev* PluginInstance::GetFindInterface() {
  return &ppb_find;
}

PP_Instance PluginInstance::GetPPInstance() {
  return reinterpret_cast<intptr_t>(this);
}

void PluginInstance::Paint(WebCanvas* canvas,
                           const gfx::Rect& plugin_rect,
                           const gfx::Rect& paint_rect) {
  if (bound_graphics_2d_)
    bound_graphics_2d_->Paint(canvas, plugin_rect, paint_rect);
}

void PluginInstance::InvalidateRect(const gfx::Rect& rect) {
  if (!container_ || position_.IsEmpty())
    return;  // Nothing to do.
  if (rect.IsEmpty())
    container_->invalidate();
  else
    container_->invalidateRect(rect);
}

PP_Var PluginInstance::GetWindowObject() {
  if (!container_)
    return PP_MakeVoid();

  WebFrame* frame = container_->element().document().frame();
  if (!frame)
    return PP_MakeVoid();

  return NPObjectToPPVar(frame->windowObject());
}

PP_Var PluginInstance::GetOwnerElementObject() {
  if (!container_)
    return PP_MakeVoid();

  return NPObjectToPPVar(container_->scriptableObjectForElement());
}

bool PluginInstance::BindGraphics(PP_Resource device_id) {
  if (!device_id) {
    // Special-case clearing the current device.
    if (bound_graphics_2d_) {
      bound_graphics_2d_->BindToInstance(NULL);
      bound_graphics_2d_ = NULL;
      InvalidateRect(gfx::Rect());
    }
    return true;
  }

  scoped_refptr<Graphics2D> device_2d = Resource::GetAs<Graphics2D>(device_id);

  if (device_2d) {
    if (!device_2d->BindToInstance(this))
      return false;  // Can't bind to more than one instance.

    // See http://crbug.com/49403: this can be further optimized by keeping the
    // old device around and painting from it.
    if (bound_graphics_2d_.get()) {
      // Start the new image with the content of the old image until the plugin
      // repaints.
      const SkBitmap* old_backing_bitmap =
          bound_graphics_2d_->image_data()->GetMappedBitmap();
      SkRect old_size = SkRect::MakeWH(
          SkScalar(static_cast<float>(old_backing_bitmap->width())),
          SkScalar(static_cast<float>(old_backing_bitmap->height())));

      SkCanvas canvas(*device_2d->image_data()->GetMappedBitmap());
      canvas.drawBitmap(*old_backing_bitmap, 0, 0);

      // Fill in any extra space with white.
      canvas.clipRect(old_size, SkRegion::kDifference_Op);
      canvas.drawARGB(255, 255, 255, 255);
    }

    bound_graphics_2d_ = device_2d;
    // BindToInstance will have invalidated the plugin if necessary.
  }

  return true;
}

bool PluginInstance::SetCursor(PP_CursorType_Dev type) {
  cursor_.reset(new WebCursorInfo(static_cast<WebCursorInfo::Type>(type)));
  return true;
}

PP_Var PluginInstance::ExecuteScript(PP_Var script, PP_Var* exception) {
  TryCatch try_catch(exception);
  if (try_catch.HasException())
    return PP_MakeVoid();

  // Convert the script into an inconvenient NPString object.
  String* script_string = GetString(script);
  if (!script_string) {
    try_catch.SetException("Script param to ExecuteScript must be a string.");
    return PP_MakeVoid();
  }
  NPString np_script;
  np_script.UTF8Characters = script_string->value().c_str();
  np_script.UTF8Length = script_string->value().length();

  // Get the current frame to pass to the evaluate function.
  WebFrame* frame = container_->element().document().frame();
  if (!frame) {
    try_catch.SetException("No frame to execute script in.");
    return PP_MakeVoid();
  }

  NPVariant result;
  bool ok = WebBindings::evaluate(NULL, frame->windowObject(), &np_script,
                                  &result);
  if (!ok) {
    // TODO(brettw) bug 54011: The TryCatch isn't working properly and
    // doesn't actually catch this exception.
    try_catch.SetException("Exception caught");
    WebBindings::releaseVariantValue(&result);
    return PP_MakeVoid();
  }

  PP_Var ret = NPVariantToPPVar(&result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

void PluginInstance::Delete() {
  instance_interface_->Delete(GetPPInstance());

  container_ = NULL;
}

bool PluginInstance::Initialize(WebPluginContainer* container,
                                const std::vector<std::string>& arg_names,
                                const std::vector<std::string>& arg_values,
                                bool full_frame) {
  container_ = container;
  full_frame_ = full_frame;

  if (!instance_interface_->New(GetPPInstance()))
    return false;

  size_t argc = 0;
  scoped_array<const char*> argn(new const char*[arg_names.size()]);
  scoped_array<const char*> argv(new const char*[arg_names.size()]);
  for (size_t i = 0; i < arg_names.size(); ++i) {
    argn[argc] = arg_names[i].c_str();
    argv[argc] = arg_values[i].c_str();
    argc++;
  }

  return instance_interface_->Initialize(GetPPInstance(),
                                         argc, argn.get(), argv.get());
}

bool PluginInstance::HandleDocumentLoad(URLLoader* loader) {
  Resource::ScopedResourceId resource(loader);
  return instance_interface_->HandleDocumentLoad(GetPPInstance(), resource.id);
}

bool PluginInstance::HandleInputEvent(const WebKit::WebInputEvent& event,
                                      WebCursorInfo* cursor_info) {
  scoped_ptr<PP_Event> pp_event(CreatePP_Event(event));
  if (!pp_event.get())
    return false;

  bool rv = instance_interface_->HandleEvent(GetPPInstance(), pp_event.get());
  if (cursor_.get())
    *cursor_info = *cursor_;
  return rv;
}

PP_Var PluginInstance::GetInstanceObject() {
  return instance_interface_->GetInstanceObject(GetPPInstance());
}

void PluginInstance::ViewChanged(const gfx::Rect& position,
                                 const gfx::Rect& clip) {
  position_ = position;
  if (clip.IsEmpty()) {
    // WebKit can give weird (x,y) positions for empty clip rects (since the
    // position technically doesn't matter). But we want to make these
    // consistent since this is given to the plugin, so force everything to 0
    // in the "everything is clipped" case.
    clip_ = gfx::Rect();
  } else {
    clip_ = clip;
  }

  PP_Rect pp_position, pp_clip;
  RectToPPRect(position_, &pp_position);
  RectToPPRect(clip_, &pp_clip);
  instance_interface_->ViewChanged(GetPPInstance(), &pp_position, &pp_clip);
}

void PluginInstance::ViewInitiatedPaint() {
  if (bound_graphics_2d_)
    bound_graphics_2d_->ViewInitiatedPaint();
}

void PluginInstance::ViewFlushedPaint() {
  if (bound_graphics_2d_)
    bound_graphics_2d_->ViewFlushedPaint();
}

string16 PluginInstance::GetSelectedText(bool html) {
  PP_Var rv = instance_interface_->GetSelectedText(GetPPInstance(), html);
  String* string = GetString(rv);
  if (!string)
    return string16();
  return UTF8ToUTF16(string->value());
}

void PluginInstance::Zoom(float factor, bool text_only) {
  if (!LoadZoomInterface())
    return;
  plugin_zoom_interface_->Zoom(GetPPInstance(), factor, text_only);
}

bool PluginInstance::StartFind(const string16& search_text,
                               bool case_sensitive,
                               int identifier) {
  if (!LoadFindInterface())
    return false;
  find_identifier_ = identifier;
  return plugin_find_interface_->StartFind(
      GetPPInstance(),
      UTF16ToUTF8(search_text.c_str()).c_str(),
      case_sensitive);
}

void PluginInstance::SelectFindResult(bool forward) {
  if (LoadFindInterface())
    plugin_find_interface_->SelectFindResult(GetPPInstance(), forward);
}

void PluginInstance::StopFind() {
  if (!LoadFindInterface())
    return;
  find_identifier_ = -1;
  plugin_find_interface_->StopFind(GetPPInstance());
}

bool PluginInstance::LoadFindInterface() {
  if (!plugin_find_interface_) {
    plugin_find_interface_ =
        reinterpret_cast<const PPP_Find_Dev*>(module_->GetPluginInterface(
            PPP_FIND_DEV_INTERFACE));
  }

  return !!plugin_find_interface_;
}

bool PluginInstance::LoadZoomInterface() {
  if (!plugin_zoom_interface_) {
    plugin_zoom_interface_ =
        reinterpret_cast<const PPP_Zoom_Dev*>(module_->GetPluginInterface(
            PPP_ZOOM_DEV_INTERFACE));
  }

  return !!plugin_zoom_interface_;
}

bool PluginInstance::GetPreferredPrintOutputFormat(
    PP_PrintOutputFormat_Dev* format) {
  if (!plugin_print_interface_) {
    plugin_print_interface_ =
        reinterpret_cast<const PPP_Printing_Dev*>(module_->GetPluginInterface(
            PPP_PRINTING_DEV_INTERFACE));
  }
  if (!plugin_print_interface_)
    return false;
  uint32_t format_count = 0;
  PP_PrintOutputFormat_Dev* supported_formats =
      plugin_print_interface_->QuerySupportedFormats(GetPPInstance(),
                                                     &format_count);
  if (!supported_formats)
    return false;

  bool found_supported_format = false;
  for (uint32_t index = 0; index < format_count; index++) {
    if (supported_formats[index] == PP_PRINTOUTPUTFORMAT_PDF) {
      // If we found PDF, we are done.
      found_supported_format = true;
      *format = PP_PRINTOUTPUTFORMAT_PDF;
      break;
    } else if (supported_formats[index] == PP_PRINTOUTPUTFORMAT_RASTER) {
      // We found raster. Keep looking.
      found_supported_format = true;
      *format = PP_PRINTOUTPUTFORMAT_RASTER;
    }
  }
  PluginModule::GetCore()->MemFree(supported_formats);
  return found_supported_format;
}

bool PluginInstance::SupportsPrintInterface() {
  PP_PrintOutputFormat_Dev format;
  return GetPreferredPrintOutputFormat(&format);
}

int PluginInstance::PrintBegin(const gfx::Rect& printable_area,
                               int printer_dpi) {
  PP_PrintOutputFormat_Dev format;
  if (!GetPreferredPrintOutputFormat(&format)) {
    // PrintBegin should not have been called since SupportsPrintInterface
    // would have returned false;
    NOTREACHED();
    return 0;
  }

  PP_PrintSettings_Dev print_settings;
  RectToPPRect(printable_area, &print_settings.printable_area);
  print_settings.dpi = printer_dpi;
  print_settings.orientation = PP_PRINTORIENTATION_NORMAL;
  print_settings.grayscale = false;
  print_settings.format = format;
  int num_pages = plugin_print_interface_->Begin(GetPPInstance(),
                                                 &print_settings);
  if (!num_pages)
    return 0;
  current_print_settings_ = print_settings;
#if defined (OS_LINUX)
  num_pages_ = num_pages;
  pdf_output_done_ = false;
#endif  // (OS_LINUX)
  return num_pages;
}

bool PluginInstance::PrintPage(int page_number, WebKit::WebCanvas* canvas) {
  DCHECK(plugin_print_interface_);
  PP_PrintPageNumberRange_Dev page_range;
#if defined(OS_LINUX)
  if (current_print_settings_.format == PP_PRINTOUTPUTFORMAT_PDF) {
    // On Linux we will try and output all pages as PDF in the first call to
    // PrintPage. This is a temporary hack.
    // TODO(sanjeevr): Remove this hack and fix this by changing the print
    // interfaces for WebFrame and WebPlugin.
    if (page_number != 0)
      return pdf_output_done_;
    page_range.first_page_number = 0;
    page_range.last_page_number = num_pages_ - 1;
  }
#else  // defined(OS_LINUX)
  page_range.first_page_number = page_range.last_page_number = page_number;
#endif  // defined(OS_LINUX)

  PP_Resource print_output =
      plugin_print_interface_->PrintPages(GetPPInstance(), &page_range, 1);

  if (!print_output)
    return false;

  bool ret = false;

  if (current_print_settings_.format == PP_PRINTOUTPUTFORMAT_PDF)
    ret = PrintPDFOutput(print_output, canvas);
  else if (current_print_settings_.format == PP_PRINTOUTPUTFORMAT_RASTER)
    ret = PrintRasterOutput(print_output, canvas);

  // Now we need to release the print output resource.
  PluginModule::GetCore()->ReleaseResource(print_output);

  return ret;
}

void PluginInstance::PrintEnd() {
  DCHECK(plugin_print_interface_);
  if (plugin_print_interface_)
    plugin_print_interface_->End(GetPPInstance());
  memset(&current_print_settings_, 0, sizeof(current_print_settings_));
#if defined(OS_MACOSX)
  last_printed_page_ = NULL;
#elif defined(OS_LINUX)
  num_pages_ = 0;
  pdf_output_done_ = false;
#endif  // defined(OS_LINUX)
}

void PluginInstance::Graphics3DContextLost() {
  if (!plugin_graphics_3d_interface_) {
    plugin_graphics_3d_interface_ =
        reinterpret_cast<const PPP_Graphics3D_Dev*>(module_->GetPluginInterface(
            PPP_GRAPHICS_3D_DEV_INTERFACE));
  }
  if (plugin_graphics_3d_interface_)
    plugin_graphics_3d_interface_->Graphics3DContextLost(GetPPInstance());
}

bool PluginInstance::PrintPDFOutput(PP_Resource print_output,
                                    WebKit::WebCanvas* canvas) {
  scoped_refptr<Buffer> buffer(Resource::GetAs<Buffer>(print_output));
  if (!buffer.get() || !buffer->is_mapped() || !buffer->size()) {
    NOTREACHED();
    return false;
  }
#if defined(OS_WIN)
  // For Windows, we need the PDF DLL to render the output PDF to a DC.
  HMODULE pdf_module = GetModuleHandle(L"pdf.dll");
  if (!pdf_module)
    return false;
  RenderPDFPageToDCProc render_proc =
      reinterpret_cast<RenderPDFPageToDCProc>(
          GetProcAddress(pdf_module, "RenderPDFPageToDC"));
  if (!render_proc)
    return false;
#endif  // defined(OS_WIN)

  bool ret = false;
#if defined(OS_LINUX)
  // On Linux we need to get the backing PdfPsMetafile and write the bits
  // directly.
  cairo_t* context = canvas->beginPlatformPaint();
  printing::NativeMetafile* metafile =
      printing::NativeMetafile::FromCairoContext(context);
  DCHECK(metafile);
  if (metafile) {
    ret = metafile->SetRawData(buffer->mapped_buffer(), buffer->size());
    if (ret)
      pdf_output_done_ = true;
  }
  canvas->endPlatformPaint();
#elif defined(OS_MACOSX)
  printing::NativeMetafile metafile;
  // Create a PDF metafile and render from there into the passed in context.
  if (metafile.Init(buffer->mapped_buffer(), buffer->size())) {
    // Flip the transform.
    CGContextSaveGState(canvas);
    CGContextTranslateCTM(canvas, 0,
                          current_print_settings_.printable_area.size.height);
    CGContextScaleCTM(canvas, 1.0, -1.0);
    CGRect page_rect;
    page_rect.origin.x = current_print_settings_.printable_area.point.x;
    page_rect.origin.y = current_print_settings_.printable_area.point.y;
    page_rect.size.width = current_print_settings_.printable_area.size.width;
    page_rect.size.height = current_print_settings_.printable_area.size.height;

    ret = metafile.RenderPage(1, canvas, page_rect, true, false, true, true);
    CGContextRestoreGState(canvas);
  }
#elif defined(OS_WIN)
  // On Windows, we now need to render the PDF to the DC that backs the
  // supplied canvas.
  skia::VectorPlatformDevice& device =
      static_cast<skia::VectorPlatformDevice&>(
          canvas->getTopPlatformDevice());
  HDC dc = device.getBitmapDC();
  gfx::Size size_in_pixels;
  size_in_pixels.set_width(
      printing::ConvertUnit(current_print_settings_.printable_area.size.width,
                            static_cast<int>(printing::kPointsPerInch),
                            current_print_settings_.dpi));
  size_in_pixels.set_height(
      printing::ConvertUnit(current_print_settings_.printable_area.size.height,
                            static_cast<int>(printing::kPointsPerInch),
                            current_print_settings_.dpi));
  // We need to render using the actual printer DPI (rendering to a smaller
  // set of pixels leads to a blurry output). However, we need to counter the
  // scaling up that will happen in the browser.
  XFORM xform = {0};
  xform.eM11 = xform.eM22 = static_cast<float>(printing::kPointsPerInch) /
      static_cast<float>(current_print_settings_.dpi);
  ModifyWorldTransform(dc, &xform, MWT_LEFTMULTIPLY);

  ret = render_proc(buffer->mapped_buffer(), buffer->size(), 0, dc,
                    current_print_settings_.dpi, current_print_settings_.dpi,
                    0, 0, size_in_pixels.width(),
                    size_in_pixels.height(), true, false, true, true);
#endif  // defined(OS_WIN)

  return ret;
}

bool PluginInstance::PrintRasterOutput(PP_Resource print_output,
                                       WebKit::WebCanvas* canvas) {
  scoped_refptr<ImageData> image(Resource::GetAs<ImageData>(print_output));
  if (!image.get() || !image->is_mapped())
    return false;

  const SkBitmap* bitmap = image->GetMappedBitmap();
  if (!bitmap)
    return false;

  // Draw the printed image into the supplied canvas.
  SkIRect src_rect;
  src_rect.set(0, 0, bitmap->width(), bitmap->height());
  SkRect dest_rect;
  dest_rect.set(
      SkIntToScalar(current_print_settings_.printable_area.point.x),
      SkIntToScalar(current_print_settings_.printable_area.point.y),
      SkIntToScalar(current_print_settings_.printable_area.point.x +
                    current_print_settings_.printable_area.size.width),
      SkIntToScalar(current_print_settings_.printable_area.point.y +
                    current_print_settings_.printable_area.size.height));
  bool draw_to_canvas = true;
  gfx::Rect dest_rect_gfx;
  dest_rect_gfx.set_x(current_print_settings_.printable_area.point.x);
  dest_rect_gfx.set_y(current_print_settings_.printable_area.point.y);
  dest_rect_gfx.set_width(current_print_settings_.printable_area.size.width);
  dest_rect_gfx.set_height(current_print_settings_.printable_area.size.height);

#if defined(OS_WIN)
  // Since this is a raster output, the size of the bitmap can be
  // huge (especially at high printer DPIs). On Windows, this can
  // result in a HUGE EMF (on Mac and Linux the output goes to PDF
  // which appears to Flate compress the bitmap). So, if this bitmap
  // is larger than 20 MB, we save the bitmap as a JPEG into the EMF
  // DC. Note: We chose JPEG over PNG because JPEG compression seems
  // way faster (about 4 times faster).
  static const int kCompressionThreshold = 20 * 1024 * 1024;
  if (bitmap->getSize() > kCompressionThreshold) {
    DrawJPEGToPlatformDC(*bitmap, dest_rect_gfx, canvas);
    draw_to_canvas = false;
  }
#endif  // defined(OS_WIN)
#if defined(OS_MACOSX)
  draw_to_canvas = false;
  DrawSkBitmapToCanvas(*bitmap, canvas, dest_rect_gfx,
                       current_print_settings_.printable_area.size.height);
  // See comments in the header file.
  last_printed_page_ = image;
#else  // defined(OS_MACOSX)
  if (draw_to_canvas)
    canvas->drawBitmapRect(*bitmap, &src_rect, dest_rect);
#endif  // defined(OS_MACOSX)
  return true;
}

#if defined(OS_WIN)
bool PluginInstance::DrawJPEGToPlatformDC(
    const SkBitmap& bitmap,
    const gfx::Rect& printable_area,
    WebKit::WebCanvas* canvas) {
  skia::VectorPlatformDevice& device =
      static_cast<skia::VectorPlatformDevice&>(
          canvas->getTopPlatformDevice());
  HDC dc = device.getBitmapDC();
  // TODO(sanjeevr): This is a temporary hack. If we output a JPEG
  // to the EMF, the EnumEnhMetaFile call fails in the browser
  // process. The failure also happens if we output nothing here.
  // We need to investigate the reason for this failure and fix it.
  // In the meantime this temporary hack of drawing an empty
  // rectangle in the DC gets us by.
  Rectangle(dc, 0, 0, 0, 0);

  // Ideally we should add JPEG compression to the VectorPlatformDevice class
  // However, Skia currently has no JPEG compression code and we cannot
  // depend on gfx/jpeg_codec.h in Skia. So we do the compression here.
  SkAutoLockPixels lock(bitmap);
  DCHECK(bitmap.getConfig() == SkBitmap::kARGB_8888_Config);
  const uint32_t* pixels =
      static_cast<const uint32_t*>(bitmap.getPixels());
  std::vector<unsigned char> compressed_image;
  base::TimeTicks start_time = base::TimeTicks::Now();
  bool encoded = gfx::JPEGCodec::Encode(
      reinterpret_cast<const unsigned char*>(pixels),
      gfx::JPEGCodec::FORMAT_BGRA, bitmap.width(), bitmap.height(),
      static_cast<int>(bitmap.rowBytes()), 100, &compressed_image);
  UMA_HISTOGRAM_TIMES("PepperPluginPrint.RasterBitmapCompressTime",
                      base::TimeTicks::Now() - start_time);
  if (!encoded) {
    NOTREACHED();
    return false;
  }
  BITMAPINFOHEADER bmi = {0};
  gfx::CreateBitmapHeader(bitmap.width(), bitmap.height(), &bmi);
  bmi.biCompression = BI_JPEG;
  bmi.biSizeImage = compressed_image.size();
  bmi.biHeight = -bmi.biHeight;
  StretchDIBits(dc, printable_area.x(), printable_area.y(),
                printable_area.width(), printable_area.height(),
                0, 0, bitmap.width(), bitmap.height(),
                &compressed_image.front(),
                reinterpret_cast<const BITMAPINFO*>(&bmi),
                DIB_RGB_COLORS, SRCCOPY);
  return true;
}
#endif  // OS_WIN

#if defined(OS_MACOSX)
void PluginInstance::DrawSkBitmapToCanvas(
    const SkBitmap& bitmap, WebKit::WebCanvas* canvas,
    const gfx::Rect& dest_rect,
    int canvas_height) {
  SkAutoLockPixels lock(bitmap);
  DCHECK(bitmap.getConfig() == SkBitmap::kARGB_8888_Config);
  scoped_cftyperef<CGDataProviderRef> data_provider(
      CGDataProviderCreateWithData(
          NULL, bitmap.getAddr32(0, 0),
          bitmap.rowBytes() * bitmap.height(), NULL));
  scoped_cftyperef<CGImageRef> image(
      CGImageCreate(
          bitmap.width(), bitmap.height(),
          8, 32, bitmap.rowBytes(),
          mac_util::GetSystemColorSpace(),
          kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
          data_provider, NULL, false, kCGRenderingIntentDefault));

  // Flip the transform
  CGContextSaveGState(canvas);
  CGContextTranslateCTM(canvas, 0, canvas_height);
  CGContextScaleCTM(canvas, 1.0, -1.0);

  CGRect bounds;
  bounds.origin.x = dest_rect.x();
  bounds.origin.y = canvas_height - dest_rect.y() - dest_rect.height();
  bounds.size.width = dest_rect.width();
  bounds.size.height = dest_rect.height();

  CGContextDrawImage(canvas, bounds, image);
  CGContextRestoreGState(canvas);
}
#endif  // defined(OS_MACOSX)


}  // namespace pepper
