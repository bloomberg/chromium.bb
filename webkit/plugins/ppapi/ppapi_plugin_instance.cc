// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/utf_offset_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppb_zoom_dev.h"
#include "ppapi/c/dev/ppp_find_dev.h"
#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/shared_impl/input_event_impl.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/url_util_impl.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "printing/units.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/event_conversion.h"
#include "webkit/plugins/ppapi/fullscreen_container.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/message_channel.h"
#include "webkit/plugins/ppapi/npapi_glue.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/plugin_object.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"
#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"
#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/ppp_pdf.h"
#include "webkit/plugins/ppapi/string.h"
#include "webkit/plugins/sad_plugin.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "printing/metafile_impl.h"
#endif

#if defined(USE_SKIA)
#include "printing/metafile.h"
#include "printing/metafile_skia_wrapper.h"
#endif

#if defined(OS_WIN)
#include "skia/ext/vector_platform_device_emf_win.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/gdi_util.h"
#endif

#if defined(OS_MACOSX) && defined(USE_SKIA)
#include "skia/ext/skia_utils_mac.h"
#endif

using base::StringPrintf;
using ppapi::InputEventData;
using ppapi::InputEventImpl;
using ppapi::PpapiGlobals;
using ppapi::StringVar;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_Graphics2D_API;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::PPB_ImageData_API;
using ppapi::thunk::PPB_Instance_FunctionAPI;
using ppapi::Var;
using WebKit::WebBindings;
using WebKit::WebCanvas;
using WebKit::WebConsoleMessage;
using WebKit::WebCursorInfo;
using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebElement;
using WebKit::WebInputEvent;
using WebKit::WebPluginContainer;
using WebKit::WebString;
using WebKit::WebURLRequest;
using WebKit::WebView;

namespace webkit {
namespace ppapi {

#if defined(OS_WIN)
// Exported by pdf.dll
typedef bool (*RenderPDFPageToDCProc)(
    const unsigned char* pdf_buffer, int buffer_size, int page_number, HDC dc,
    int dpi_x, int dpi_y, int bounds_origin_x, int bounds_origin_y,
    int bounds_width, int bounds_height, bool fit_to_bounds,
    bool stretch_to_bounds, bool keep_aspect_ratio, bool center_in_bounds,
    bool autorotate);

void DrawEmptyRectangle(HDC dc) {
  // TODO(sanjeevr): This is a temporary hack. If we output a JPEG
  // to the EMF, the EnumEnhMetaFile call fails in the browser
  // process. The failure also happens if we output nothing here.
  // We need to investigate the reason for this failure and fix it.
  // In the meantime this temporary hack of drawing an empty
  // rectangle in the DC gets us by.
  Rectangle(dc, 0, 0, 0, 0);
}
#endif  // defined(OS_WIN)

namespace {

#if !defined(TOUCH_UI)
// The default text input type is to regard the plugin always accept text input.
// This is for allowing users to use input methods even on completely-IME-
// unaware plugins (e.g., PPAPI Flash or PDF plugin for M16).
// Plugins need to explicitly opt out the text input mode if they know
// that they don't accept texts.
const ui::TextInputType kPluginDefaultTextInputType = ui::TEXT_INPUT_TYPE_TEXT;
#else
// On the other hand, for touch ui, accepting text input implies to pop up
// virtual keyboard always. It makes IME-unaware plugins almost unusable,
// and hence is disabled by default (codereview.chromium.org/7800044).
const ui::TextInputType kPluginDefaultTextInputType = ui::TEXT_INPUT_TYPE_NONE;
#endif

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, np_name) \
    COMPILE_ASSERT(static_cast<int>(WebCursorInfo::webkit_name) \
                       == static_cast<int>(np_name), \
                   mismatching_enums)

// <embed>/<object> attributes.
static const char kWidth[] = "width";
static const char kHeight[] = "height";
static const char kBorder[] = "border";  // According to w3c, deprecated.
static const char kStyle[] = "style";

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
COMPILE_ASSERT_MATCHING_ENUM(TypeGrab, PP_CURSORTYPE_GRAB);
COMPILE_ASSERT_MATCHING_ENUM(TypeGrabbing, PP_CURSORTYPE_GRABBING);
// Do not assert WebCursorInfo::TypeCustom == PP_CURSORTYPE_CUSTOM;
// PP_CURSORTYPE_CUSTOM is pinned to allow new cursor types.

void RectToPPRect(const gfx::Rect& input, PP_Rect* output) {
  *output = PP_MakeRectFromXYWH(input.x(), input.y(),
                                input.width(), input.height());
}

// Sets |*security_origin| to be the WebKit security origin associated with the
// document containing the given plugin instance. On success, returns true. If
// the instance is invalid, returns false and |*security_origin| will be
// unchanged.
bool SecurityOriginForInstance(PP_Instance instance_id,
                               WebKit::WebSecurityOrigin* security_origin) {
  PluginInstance* instance = HostGlobals::Get()->GetInstance(instance_id);
  if (!instance)
    return false;

  WebElement plugin_element = instance->container()->element();
  *security_origin = plugin_element.document().securityOrigin();
  return true;
}

}  // namespace

// static
PluginInstance* PluginInstance::Create1_0(PluginDelegate* delegate,
                                          PluginModule* module,
                                          const void* ppp_instance_if_1_0) {
  const PPP_Instance_1_0* instance =
      static_cast<const PPP_Instance_1_0*>(ppp_instance_if_1_0);
  return new PluginInstance(
      delegate,
      module,
      new ::ppapi::PPP_Instance_Combined(*instance));
}

PluginInstance::PluginInstance(
    PluginDelegate* delegate,
    PluginModule* module,
    ::ppapi::PPP_Instance_Combined* instance_interface)
    : delegate_(delegate),
      module_(module),
      instance_interface_(instance_interface),
      pp_instance_(0),
      container_(NULL),
      full_frame_(false),
      sent_did_change_view_(false),
      has_webkit_focus_(false),
      has_content_area_focus_(false),
      find_identifier_(-1),
      plugin_find_interface_(NULL),
      plugin_messaging_interface_(NULL),
      plugin_mouse_lock_interface_(NULL),
      plugin_input_event_interface_(NULL),
      plugin_private_interface_(NULL),
      plugin_pdf_interface_(NULL),
      plugin_selection_interface_(NULL),
      plugin_zoom_interface_(NULL),
      checked_for_plugin_input_event_interface_(false),
      checked_for_plugin_messaging_interface_(false),
      plugin_print_interface_(NULL),
      plugin_graphics_3d_interface_(NULL),
      always_on_top_(false),
      fullscreen_container_(NULL),
      flash_fullscreen_(false),
      desired_fullscreen_state_(false),
      fullscreen_(false),
      message_channel_(NULL),
      sad_plugin_(NULL),
      input_event_mask_(0),
      filtered_input_event_mask_(0),
      text_input_type_(kPluginDefaultTextInputType),
      text_input_caret_(0, 0, 0, 0),
      text_input_caret_bounds_(0, 0, 0, 0),
      text_input_caret_set_(false),
      lock_mouse_callback_(PP_BlockUntilComplete()) {
  pp_instance_ = HostGlobals::Get()->AddInstance(this);

  memset(&current_print_settings_, 0, sizeof(current_print_settings_));
  DCHECK(delegate);
  module_->InstanceCreated(this);
  delegate_->InstanceCreated(this);
  message_channel_.reset(new MessageChannel(this));
}

PluginInstance::~PluginInstance() {
  DCHECK(!fullscreen_container_);

  // Free all the plugin objects. This will automatically clear the back-
  // pointer from the NPObject so WebKit can't call into the plugin any more.
  //
  // Swap out the set so we can delete from it (the objects will try to
  // unregister themselves inside the delete call).
  PluginObjectSet plugin_object_copy;
  live_plugin_objects_.swap(plugin_object_copy);
  for (PluginObjectSet::iterator i = plugin_object_copy.begin();
       i != plugin_object_copy.end(); ++i)
    delete *i;

  if (lock_mouse_callback_.func)
    PP_RunAndClearCompletionCallback(&lock_mouse_callback_, PP_ERROR_ABORTED);

  delegate_->InstanceDeleted(this);
  module_->InstanceDeleted(this);

  HostGlobals::Get()->InstanceDeleted(pp_instance_);
}

// NOTE: Any of these methods that calls into the plugin needs to take into
// account that the plugin may use Var to remove the <embed> from the DOM, which
// will make the WebPluginImpl drop its reference, usually the last one. If a
// method needs to access a member of the instance after the call has returned,
// then it needs to keep its own reference on the stack.

void PluginInstance::Delete() {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  instance_interface_->DidDestroy(pp_instance());

  if (fullscreen_container_) {
    fullscreen_container_->Destroy();
    fullscreen_container_ = NULL;
  }
  container_ = NULL;
}

void PluginInstance::Paint(WebCanvas* canvas,
                           const gfx::Rect& plugin_rect,
                           const gfx::Rect& paint_rect) {
  TRACE_EVENT0("ppapi", "PluginInstance::Paint");
  if (module()->is_crashed()) {
    // Crashed plugin painting.
    if (!sad_plugin_)  // Lazily initialize bitmap.
      sad_plugin_ = delegate_->GetSadPluginBitmap();
    if (sad_plugin_)
      webkit::PaintSadPlugin(canvas, plugin_rect, *sad_plugin_);
    return;
  }

  PPB_Graphics2D_Impl* bound_graphics_2d = GetBoundGraphics2D();
  if (bound_graphics_2d)
    bound_graphics_2d->Paint(canvas, plugin_rect, paint_rect);
}

void PluginInstance::InvalidateRect(const gfx::Rect& rect) {
  if (fullscreen_container_) {
    if (rect.IsEmpty())
      fullscreen_container_->Invalidate();
    else
      fullscreen_container_->InvalidateRect(rect);
  } else {
    if (!container_ || position_.IsEmpty())
      return;  // Nothing to do.
    if (rect.IsEmpty())
      container_->invalidate();
    else
      container_->invalidateRect(rect);
  }
}

void PluginInstance::ScrollRect(int dx, int dy, const gfx::Rect& rect) {
  if (fullscreen_container_) {
    fullscreen_container_->ScrollRect(dx, dy, rect);
  } else {
    if (full_frame_ && !IsViewAccelerated()) {
      container_->scrollRect(dx, dy, rect);
    } else {
      // Can't do optimized scrolling since there could be other elements on top
      // of us or the view renders via the accelerated compositor which is
      // incompatible with the move and backfill scrolling model.
      InvalidateRect(rect);
    }
  }
}

unsigned PluginInstance::GetBackingTextureId() {
  if (GetBoundGraphics3D())
    return GetBoundGraphics3D()->GetBackingTextureId();

  return 0;
}

void PluginInstance::CommitBackingTexture() {
  if (fullscreen_container_)
    fullscreen_container_->Invalidate();
  else if (container_)
    container_->commitBackingTexture();
}

void PluginInstance::InstanceCrashed() {
  // Force free all resources and vars.
  HostGlobals::Get()->InstanceCrashed(pp_instance());

  // Free any associated graphics.
  SetFullscreen(false);
  FlashSetFullscreen(false, false);
  bound_graphics_ = NULL;
  InvalidateRect(gfx::Rect());

  delegate()->PluginCrashed(this);
}

bool PluginInstance::SetCursor(PP_CursorType_Dev type,
                               PP_Resource custom_image,
                               const PP_Point* hot_spot) {
  if (type != PP_CURSORTYPE_CUSTOM) {
    DoSetCursor(new WebCursorInfo(static_cast<WebCursorInfo::Type>(type)));
    return true;
  }

  if (!hot_spot)
    return false;

  EnterResourceNoLock<PPB_ImageData_API> enter(custom_image, true);
  if (enter.failed())
    return false;
  PPB_ImageData_Impl* image_data =
      static_cast<PPB_ImageData_Impl*>(enter.object());

  if (image_data->format() != PPB_ImageData_Impl::GetNativeImageDataFormat()) {
    // TODO(yzshen): Handle the case that the image format is different from the
    // native format.
    NOTIMPLEMENTED();
    return false;
  }

  ImageDataAutoMapper auto_mapper(image_data);
  if (!auto_mapper.is_valid())
    return false;

  scoped_ptr<WebCursorInfo> custom_cursor(
      new WebCursorInfo(WebCursorInfo::TypeCustom));
  custom_cursor->hotSpot.x = hot_spot->x;
  custom_cursor->hotSpot.y = hot_spot->y;

#if WEBKIT_USING_SKIA
  const SkBitmap* bitmap = image_data->GetMappedBitmap();
  // Make a deep copy, so that the cursor remains valid even after the original
  // image data gets freed.
  if (!bitmap->copyTo(&custom_cursor->customImage.getSkBitmap(),
                      bitmap->config())) {
    return false;
  }
#elif WEBKIT_USING_CG
  // TODO(yzshen): Implement it.
  NOTIMPLEMENTED();
  return false;
#endif

  DoSetCursor(custom_cursor.release());
  return true;
}

bool PluginInstance::Initialize(WebPluginContainer* container,
                                const std::vector<std::string>& arg_names,
                                const std::vector<std::string>& arg_values,
                                const GURL& plugin_url,
                                bool full_frame) {
  container_ = container;
  plugin_url_ = plugin_url;
  full_frame_ = full_frame;

  size_t argc = 0;
  scoped_array<const char*> argn(new const char*[arg_names.size()]);
  scoped_array<const char*> argv(new const char*[arg_names.size()]);
  for (size_t i = 0; i < arg_names.size(); ++i) {
    argn[argc] = arg_names[i].c_str();
    argv[argc] = arg_values[i].c_str();
    argc++;
  }

  return PP_ToBool(instance_interface_->DidCreate(pp_instance(),
                                                  argc,
                                                  argn.get(),
                                                  argv.get()));
}

bool PluginInstance::HandleDocumentLoad(PPB_URLLoader_Impl* loader) {
  return PP_ToBool(instance_interface_->HandleDocumentLoad(
      pp_instance(), loader->pp_resource()));
}

bool PluginInstance::SendCompositionEventToPlugin(PP_InputEvent_Type type,
                                                  const string16& text) {
  std::vector<WebKit::WebCompositionUnderline> empty;
  return SendCompositionEventWithUnderlineInformationToPlugin(
      type, text, empty, static_cast<int>(text.size()),
      static_cast<int>(text.size()));
}

bool PluginInstance::SendCompositionEventWithUnderlineInformationToPlugin(
    PP_InputEvent_Type type,
    const string16& text,
    const std::vector<WebKit::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);

  if (!LoadInputEventInterface())
    return false;

  PP_InputEvent_Class event_class = PP_INPUTEVENT_CLASS_IME;
  if (!(filtered_input_event_mask_ & event_class) &&
      !(input_event_mask_ & event_class))
    return false;

  ::ppapi::InputEventData event;
  event.event_type = type;
  event.event_time_stamp = ::ppapi::TimeTicksToPPTimeTicks(
      base::TimeTicks::Now());

  // Convert UTF16 text to UTF8 with offset conversion.
  std::vector<size_t> utf16_offsets;
  utf16_offsets.push_back(selection_start);
  utf16_offsets.push_back(selection_end);
  for (size_t i = 0; i < underlines.size(); ++i) {
    utf16_offsets.push_back(underlines[i].startOffset);
    utf16_offsets.push_back(underlines[i].endOffset);
  }
  std::vector<size_t> utf8_offsets(utf16_offsets);
  event.character_text = UTF16ToUTF8AndAdjustOffsets(text, &utf8_offsets);

  // Set the converted selection range.
  event.composition_selection_start = (utf8_offsets[0] == std::string::npos ?
      event.character_text.size() : utf8_offsets[0]);
  event.composition_selection_end = (utf8_offsets[1] == std::string::npos ?
      event.character_text.size() : utf8_offsets[1]);

  // Set the converted segmentation points.
  // Be sure to add 0 and size(), and remove duplication or errors.
  std::set<size_t> offset_set(utf8_offsets.begin()+2, utf8_offsets.end());
  offset_set.insert(0);
  offset_set.insert(event.character_text.size());
  offset_set.erase(std::string::npos);
  event.composition_segment_offsets.assign(offset_set.begin(),
                                           offset_set.end());

  // Set the composition target.
  for (size_t i = 0; i < underlines.size(); ++i) {
    if (underlines[i].thick) {
      std::vector<uint32_t>::iterator it =
          std::find(event.composition_segment_offsets.begin(),
                    event.composition_segment_offsets.end(),
                    utf8_offsets[2*i+2]);
      if (it != event.composition_segment_offsets.end()) {
        event.composition_target_segment =
            it - event.composition_segment_offsets.begin();
        break;
      }
    }
  }

  // Send the event.
  bool handled = false;
  if (filtered_input_event_mask_ & event_class)
    event.is_filtered = true;
  else
    handled = true; // Unfiltered events are assumed to be handled.
  scoped_refptr<InputEventImpl> event_resource(
      new InputEventImpl(InputEventImpl::InitAsImpl(),
                         pp_instance(), event));
  handled |= PP_ToBool(plugin_input_event_interface_->HandleInputEvent(
      pp_instance(), event_resource->pp_resource()));
  return handled;
}

bool PluginInstance::HandleCompositionStart(const string16& text) {
  return SendCompositionEventToPlugin(PP_INPUTEVENT_TYPE_IME_COMPOSITION_START,
                                      text);
}

bool PluginInstance::HandleCompositionUpdate(
    const string16& text,
    const std::vector<WebKit::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  return SendCompositionEventWithUnderlineInformationToPlugin(
      PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE,
      text, underlines, selection_start, selection_end);
}

bool PluginInstance::HandleCompositionEnd(const string16& text) {
  return SendCompositionEventToPlugin(PP_INPUTEVENT_TYPE_IME_COMPOSITION_END,
                                      text);
}

bool PluginInstance::HandleTextInput(const string16& text) {
  return SendCompositionEventToPlugin(PP_INPUTEVENT_TYPE_IME_TEXT,
                                      text);
}

void PluginInstance::UpdateCaretPosition(const gfx::Rect& caret,
                                         const gfx::Rect& bounding_box) {
  text_input_caret_ = caret;
  text_input_caret_bounds_ = bounding_box;
  text_input_caret_set_ = true;
  delegate()->PluginCaretPositionChanged(this);
}

void PluginInstance::SetTextInputType(ui::TextInputType type) {
  text_input_type_ = type;
  delegate()->PluginTextInputTypeChanged(this);
}

bool PluginInstance::IsPluginAcceptingCompositionEvents() const {
  return (filtered_input_event_mask_ & PP_INPUTEVENT_CLASS_IME) ||
      (input_event_mask_ & PP_INPUTEVENT_CLASS_IME);
}

gfx::Rect PluginInstance::GetCaretBounds() const {
  if (!text_input_caret_set_) {
    // If it is never set by the plugin, use the bottom left corner.
    return gfx::Rect(position().x(), position().y()+position().height(), 0, 0);
  }

  // TODO(kinaba) Take CSS transformation into accont.
  // TODO(kinaba) Take bounding_box into account. On some platforms, an
  // "exclude rectangle" where candidate window must avoid the region can be
  // passed to IME. Currently, we pass only the caret rectangle because
  // it is the only information supported uniformly in Chromium.
  gfx::Rect caret(text_input_caret_);
  caret.Offset(position().origin());
  return caret;
}

bool PluginInstance::HandleInputEvent(const WebKit::WebInputEvent& event,
                                      WebCursorInfo* cursor_info) {
  TRACE_EVENT0("ppapi", "PluginInstance::HandleInputEvent");

  if (WebInputEvent::isMouseEventType(event.type))
    delegate()->DidReceiveMouseEvent(this);

  // Don't dispatch input events to crashed plugins.
  if (module()->is_crashed())
    return false;

  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);

  bool rv = false;
  if (LoadInputEventInterface()) {
    PP_InputEvent_Class event_class = ClassifyInputEvent(event.type);
    if (!event_class)
      return false;

    if ((filtered_input_event_mask_ & event_class) ||
        (input_event_mask_ & event_class)) {
      // Actually send the event.
      std::vector< ::ppapi::InputEventData > events;
      CreateInputEventData(event, &events);

      // Each input event may generate more than one PP_InputEvent.
      for (size_t i = 0; i < events.size(); i++) {
        if (filtered_input_event_mask_ & event_class)
          events[i].is_filtered = true;
        else
          rv = true;  // Unfiltered events are assumed to be handled.
        scoped_refptr<InputEventImpl> event_resource(
            new InputEventImpl(InputEventImpl::InitAsImpl(),
                               pp_instance(), events[i]));

        rv |= PP_ToBool(plugin_input_event_interface_->HandleInputEvent(
            pp_instance(), event_resource->pp_resource()));
      }
    }
  }

  if (cursor_.get())
    *cursor_info = *cursor_;
  return rv;
}

void PluginInstance::HandleMessage(PP_Var message) {
  TRACE_EVENT0("ppapi", "PluginInstance::HandleMessage");
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  if (!LoadMessagingInterface())
    return;
  plugin_messaging_interface_->HandleMessage(pp_instance(), message);
}

PP_Var PluginInstance::GetInstanceObject() {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);

  // If the plugin supports the private instance interface, try to retrieve its
  // instance object.
  if (LoadPrivateInterface())
    return plugin_private_interface_->GetInstanceObject(pp_instance());
  return PP_MakeUndefined();
}

void PluginInstance::ViewChanged(const gfx::Rect& position,
                                 const gfx::Rect& clip) {
  // WebKit can give weird (x,y) positions for empty clip rects (since the
  // position technically doesn't matter). But we want to make these
  // consistent since this is given to the plugin, so force everything to 0
  // in the "everything is clipped" case.
  gfx::Rect new_clip;
  if (!clip.IsEmpty())
    new_clip = clip;

  // Don't notify the plugin if we've already sent these same params before.
  if (sent_did_change_view_ && position == position_ && new_clip == clip_)
    return;

  if (desired_fullscreen_state_ || fullscreen_) {
    WebElement element = container_->element();
    WebDocument document = element.document();
    bool is_fullscreen_element = (element == document.fullScreenElement());
    if (!fullscreen_ && desired_fullscreen_state_ &&
        delegate()->IsInFullscreenMode() && is_fullscreen_element) {
      // Entered fullscreen. Only possible via SetFullscreen().
      fullscreen_ = true;
    } else if (fullscreen_ && !is_fullscreen_element) {
      // Exited fullscreen. Possible via SetFullscreen() or F11/link,
      // so desired_fullscreen_state might be out-of-date.
      desired_fullscreen_state_ = false;
      fullscreen_ = false;
      // Reset the size attributes that we hacked to fill in the screen and
      // retrigger ViewChanged. Make sure we don't forward duplicates of
      // this view to the plugin.
      ResetSizeAttributesAfterFullscreen();
      SetSentDidChangeView(position, new_clip);
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&PluginInstance::ReportGeometry, this));
      return;
    }
  }

  SetSentDidChangeView(position, new_clip);
  flash_fullscreen_ = (fullscreen_container_ != NULL);

  PP_Rect pp_position, pp_clip;
  RectToPPRect(position_, &pp_position);
  RectToPPRect(clip_, &pp_clip);
  instance_interface_->DidChangeView(pp_instance(), &pp_position, &pp_clip);
}

void PluginInstance::SetWebKitFocus(bool has_focus) {
  if (has_webkit_focus_ == has_focus)
    return;

  bool old_plugin_focus = PluginHasFocus();
  has_webkit_focus_ = has_focus;
  if (PluginHasFocus() != old_plugin_focus) {
    delegate()->PluginFocusChanged(this, PluginHasFocus());
    instance_interface_->DidChangeFocus(pp_instance(),
                                        PP_FromBool(PluginHasFocus()));
  }
}

void PluginInstance::SetContentAreaFocus(bool has_focus) {
  if (has_content_area_focus_ == has_focus)
    return;

  bool old_plugin_focus = PluginHasFocus();
  has_content_area_focus_ = has_focus;
  if (PluginHasFocus() != old_plugin_focus) {
    instance_interface_->DidChangeFocus(pp_instance(),
                                        PP_FromBool(PluginHasFocus()));
  }
}

void PluginInstance::ViewInitiatedPaint() {
  if (GetBoundGraphics2D())
    GetBoundGraphics2D()->ViewInitiatedPaint();
  else if (GetBoundGraphics3D())
    GetBoundGraphics3D()->ViewInitiatedPaint();
}

void PluginInstance::ViewFlushedPaint() {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  if (GetBoundGraphics2D())
    GetBoundGraphics2D()->ViewFlushedPaint();
  else if (GetBoundGraphics3D())
    GetBoundGraphics3D()->ViewFlushedPaint();
}

bool PluginInstance::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip) {
  if (!always_on_top_)
    return false;
  if (!GetBoundGraphics2D() || !GetBoundGraphics2D()->is_always_opaque())
    return false;

  // We specifically want to compare against the area covered by the backing
  // store when seeing if we cover the given paint bounds, since the backing
  // store could be smaller than the declared plugin area.
  PPB_ImageData_Impl* image_data = GetBoundGraphics2D()->image_data();
  gfx::Rect plugin_backing_store_rect(position_.origin(),
                                      gfx::Size(image_data->width(),
                                                image_data->height()));
  gfx::Rect clip_page(clip_);
  clip_page.Offset(position_.origin());
  gfx::Rect plugin_paint_rect = plugin_backing_store_rect.Intersect(clip_page);
  if (!plugin_paint_rect.Contains(paint_bounds))
    return false;

  *dib = image_data->platform_image()->GetTransportDIB();
  *location = plugin_backing_store_rect;
  *clip = clip_page;
  return true;
}

string16 PluginInstance::GetSelectedText(bool html) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  if (!LoadSelectionInterface())
    return string16();

  PP_Var rv = plugin_selection_interface_->GetSelectedText(pp_instance(),
                                                           PP_FromBool(html));
  StringVar* string = StringVar::FromPPVar(rv);
  string16 selection;
  if (string)
    selection = UTF8ToUTF16(string->value());
  // Release the ref the plugin transfered to us.
  HostGlobals::Get()->GetVarTracker()->ReleaseVar(rv);
  return selection;
}

string16 PluginInstance::GetLinkAtPosition(const gfx::Point& point) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  if (!LoadPdfInterface())
    return string16();

  PP_Point p;
  p.x = point.x();
  p.y = point.y();
  PP_Var rv = plugin_pdf_interface_->GetLinkAtPosition(pp_instance(), p);
  StringVar* string = StringVar::FromPPVar(rv);
  string16 link;
  if (string)
    link = UTF8ToUTF16(string->value());
  // Release the ref the plugin transfered to us.
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(rv);
  return link;
}

void PluginInstance::Zoom(double factor, bool text_only) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  if (!LoadZoomInterface())
    return;
  plugin_zoom_interface_->Zoom(pp_instance(), factor, PP_FromBool(text_only));
}

bool PluginInstance::StartFind(const string16& search_text,
                               bool case_sensitive,
                               int identifier) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  if (!LoadFindInterface())
    return false;
  find_identifier_ = identifier;
  return PP_ToBool(
      plugin_find_interface_->StartFind(
          pp_instance(),
          UTF16ToUTF8(search_text.c_str()).c_str(),
          PP_FromBool(case_sensitive)));
}

void PluginInstance::SelectFindResult(bool forward) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  if (LoadFindInterface())
    plugin_find_interface_->SelectFindResult(pp_instance(),
                                             PP_FromBool(forward));
}

void PluginInstance::StopFind() {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  if (!LoadFindInterface())
    return;
  find_identifier_ = -1;
  plugin_find_interface_->StopFind(pp_instance());
}

bool PluginInstance::LoadFindInterface() {
  if (!plugin_find_interface_) {
    plugin_find_interface_ =
        static_cast<const PPP_Find_Dev*>(module_->GetPluginInterface(
            PPP_FIND_DEV_INTERFACE));
  }

  return !!plugin_find_interface_;
}

bool PluginInstance::LoadInputEventInterface() {
  if (!checked_for_plugin_input_event_interface_) {
    checked_for_plugin_input_event_interface_ = true;
    plugin_input_event_interface_ =
        static_cast<const PPP_InputEvent*>(module_->GetPluginInterface(
            PPP_INPUT_EVENT_INTERFACE));
  }
  return !!plugin_input_event_interface_;
}

bool PluginInstance::LoadMessagingInterface() {
  if (!checked_for_plugin_messaging_interface_) {
    checked_for_plugin_messaging_interface_ = true;
    plugin_messaging_interface_ =
        static_cast<const PPP_Messaging*>(module_->GetPluginInterface(
            PPP_MESSAGING_INTERFACE_1_0));
  }
  return !!plugin_messaging_interface_;
}

bool PluginInstance::LoadMouseLockInterface() {
  if (!plugin_mouse_lock_interface_) {
    plugin_mouse_lock_interface_ =
        static_cast<const PPP_MouseLock*>(module_->GetPluginInterface(
            PPP_MOUSELOCK_INTERFACE));
  }

  return !!plugin_mouse_lock_interface_;
}

bool PluginInstance::LoadPdfInterface() {
  if (!plugin_pdf_interface_) {
    plugin_pdf_interface_ =
        static_cast<const PPP_Pdf*>(module_->GetPluginInterface(
            PPP_PDF_INTERFACE));
  }

  return !!plugin_pdf_interface_;
}

bool PluginInstance::LoadPrintInterface() {
  if (!plugin_print_interface_) {
    plugin_print_interface_ = static_cast<const PPP_Printing_Dev*>(
        module_->GetPluginInterface(PPP_PRINTING_DEV_INTERFACE));
  }
  return !!plugin_print_interface_;
}

bool PluginInstance::LoadPrivateInterface() {
  if (!plugin_private_interface_) {
    plugin_private_interface_ = static_cast<const PPP_Instance_Private*>(
        module_->GetPluginInterface(PPP_INSTANCE_PRIVATE_INTERFACE));
  }

  return !!plugin_private_interface_;
}

bool PluginInstance::LoadSelectionInterface() {
  if (!plugin_selection_interface_) {
    plugin_selection_interface_ =
        static_cast<const PPP_Selection_Dev*>(module_->GetPluginInterface(
            PPP_SELECTION_DEV_INTERFACE));
  }
  return !!plugin_selection_interface_;
}

bool PluginInstance::LoadZoomInterface() {
  if (!plugin_zoom_interface_) {
    plugin_zoom_interface_ =
        static_cast<const PPP_Zoom_Dev*>(module_->GetPluginInterface(
            PPP_ZOOM_DEV_INTERFACE));
  }

  return !!plugin_zoom_interface_;
}

bool PluginInstance::PluginHasFocus() const {
  return has_webkit_focus_ && has_content_area_focus_;
}

void PluginInstance::ReportGeometry() {
  // If this call was delayed, we may have transitioned back to fullscreen in
  // the mean time, so only report the geometry if we are actually in normal
  // mode.
  if (container_ && !fullscreen_container_ && !flash_fullscreen_)
    container_->reportGeometry();
}

bool PluginInstance::GetPreferredPrintOutputFormat(
    PP_PrintOutputFormat_Dev* format) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  if (!LoadPrintInterface())
    return false;
  uint32_t supported_formats =
      plugin_print_interface_->QuerySupportedFormats(pp_instance());
  if (supported_formats & PP_PRINTOUTPUTFORMAT_PDF) {
    *format = PP_PRINTOUTPUTFORMAT_PDF;
    return true;
  } else if (supported_formats & PP_PRINTOUTPUTFORMAT_RASTER) {
    *format = PP_PRINTOUTPUTFORMAT_RASTER;
    return true;
  }
  return false;
}

bool PluginInstance::SupportsPrintInterface() {
  PP_PrintOutputFormat_Dev format;
  return GetPreferredPrintOutputFormat(&format);
}

bool PluginInstance::IsPrintScalingDisabled() {
  DCHECK(plugin_print_interface_);
  if (!plugin_print_interface_)
    return false;
  return plugin_print_interface_->IsScalingDisabled(pp_instance()) == PP_TRUE;
}

int PluginInstance::PrintBegin(const gfx::Rect& printable_area,
                               int printer_dpi) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  PP_PrintOutputFormat_Dev format;
  if (!GetPreferredPrintOutputFormat(&format)) {
    // PrintBegin should not have been called since SupportsPrintInterface
    // would have returned false;
    NOTREACHED();
    return 0;
  }

  int num_pages = 0;
  PP_PrintSettings_Dev print_settings;
  RectToPPRect(printable_area, &print_settings.printable_area);
  print_settings.dpi = printer_dpi;
  print_settings.orientation = PP_PRINTORIENTATION_NORMAL;
  print_settings.grayscale = PP_FALSE;
  print_settings.format = format;
  num_pages = plugin_print_interface_->Begin(pp_instance(),
                                             &print_settings);
  if (!num_pages)
    return 0;
  current_print_settings_ = print_settings;
#if defined(USE_SKIA)
  canvas_ = NULL;
  ranges_.clear();
#endif  // USE_SKIA
  return num_pages;
}

bool PluginInstance::PrintPage(int page_number, WebKit::WebCanvas* canvas) {
  DCHECK(plugin_print_interface_);
  PP_PrintPageNumberRange_Dev page_range;
  page_range.first_page_number = page_range.last_page_number = page_number;
#if defined(USE_SKIA)
  // The canvas only has a metafile on it for print preview.
  bool save_for_later =
      (printing::MetafileSkiaWrapper::GetMetafileFromCanvas(*canvas) != NULL);
#if defined(OS_MACOSX) || defined(OS_WIN)
  save_for_later = save_for_later && skia::IsPreviewMetafile(*canvas);
#endif
  if (save_for_later) {
    ranges_.push_back(page_range);
    canvas_ = canvas;
    return true;
  } else
#endif  // USE_SKIA
  {
    return PrintPageHelper(&page_range, 1, canvas);
  }
}

bool PluginInstance::PrintPageHelper(PP_PrintPageNumberRange_Dev* page_ranges,
                                     int num_ranges,
                                     WebKit::WebCanvas* canvas) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  DCHECK(plugin_print_interface_);
  if (!plugin_print_interface_)
    return false;
  PP_Resource print_output = plugin_print_interface_->PrintPages(
      pp_instance(), page_ranges, num_ranges);
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
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
#if defined(USE_SKIA)
  if (!ranges_.empty())
    PrintPageHelper(&(ranges_.front()), ranges_.size(), canvas_.get());
  canvas_ = NULL;
  ranges_.clear();
#endif  // USE_SKIA

  DCHECK(plugin_print_interface_);
  if (plugin_print_interface_)
    plugin_print_interface_->End(pp_instance());

  memset(&current_print_settings_, 0, sizeof(current_print_settings_));
#if defined(OS_MACOSX)
  last_printed_page_ = NULL;
#endif  // defined(OS_MACOSX)
}

bool PluginInstance::FlashIsFullscreenOrPending() {
  return fullscreen_container_ != NULL;
}

bool PluginInstance::IsFullscreenOrPending() {
  return desired_fullscreen_state_;
}

bool PluginInstance::SetFullscreen(bool fullscreen) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);

  // Check whether we are trying to switch to the state we're already going
  // to (i.e. if we're already switching to fullscreen but the fullscreen
  // container isn't ready yet, don't do anything more).
  if (fullscreen == IsFullscreenOrPending())
    return false;

  // Check whether we are trying to switch while the state is in transition.
  // The 2nd request gets dropped while messing up the internal state, so
  // disallow this.
  if (fullscreen_ != desired_fullscreen_state_)
    return false;

  // The browser will allow us to go into fullscreen mode only when processing
  // a user gesture. This is guaranteed to work with in-process plugins and
  // out-of-process syncronous proxies, but might be an issue with Flash when
  // it switches over from PPB_FlashFullscreen.
  // TODO(polina, bbudge): make this work with asynchronous proxies.
  WebFrame* frame = container_->element().document().frame();
  if (fullscreen && !frame->isProcessingUserGesture())
    return false;

  VLOG(1) << "Setting fullscreen to " << (fullscreen ? "on" : "off");
  desired_fullscreen_state_ = fullscreen;

  if (fullscreen) {
    // WebKit does not resize the plugin to fill the screen in fullscreen mode,
    // so we will tweak plugin's attributes to support the expected behavior.
    KeepSizeAttributesBeforeFullscreen();
    SetSizeAttributesForFullscreen();
    container_->element().requestFullScreen();
  } else {
    container_->element().document().cancelFullScreen();
  }
  return true;
}

void PluginInstance::FlashSetFullscreen(bool fullscreen, bool delay_report) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);

  // We check whether we are trying to switch to the state we're already going
  // to (i.e. if we're already switching to fullscreen but the fullscreen
  // container isn't ready yet, don't do anything more).
  if (fullscreen == FlashIsFullscreenOrPending())
    return;

  // Unbind current 2D or 3D graphics context.
  BindGraphics(pp_instance(), 0);
  VLOG(1) << "Setting fullscreen to " << (fullscreen ? "on" : "off");
  if (fullscreen) {
    DCHECK(!fullscreen_container_);
    fullscreen_container_ = delegate_->CreateFullscreenContainer(this);
  } else {
    DCHECK(fullscreen_container_);
    fullscreen_container_->Destroy();
    fullscreen_container_ = NULL;
    flash_fullscreen_ = false;
    if (!delay_report) {
      ReportGeometry();
    } else {
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&PluginInstance::ReportGeometry, this));
    }
  }
}

int32_t PluginInstance::Navigate(PPB_URLRequestInfo_Impl* request,
                                 const char* target,
                                 bool from_user_action) {
  if (!container_)
    return PP_ERROR_FAILED;

  WebDocument document = container_->element().document();
  WebFrame* frame = document.frame();
  if (!frame)
    return PP_ERROR_FAILED;

  WebURLRequest web_request;
  if (!request->ToWebURLRequest(frame, &web_request))
    return PP_ERROR_FAILED;
  web_request.setFirstPartyForCookies(document.firstPartyForCookies());
  web_request.setHasUserGesture(from_user_action);

  GURL gurl(web_request.url());
  if (gurl.SchemeIs("javascript")) {
    // In imitation of the NPAPI implementation, only |target_frame == frame| is
    // allowed for security reasons.
    WebFrame* target_frame =
        frame->view()->findFrameByName(WebString::fromUTF8(target), frame);
    if (target_frame != frame)
      return PP_ERROR_NOACCESS;

    // TODO(viettrungluu): NPAPI sends the result back to the plugin -- do we
    // need that?
    WebString result = container_->executeScriptURL(gurl, from_user_action);
    return result.isNull() ? PP_ERROR_FAILED : PP_OK;
  }

  // Only GETs and POSTs are supported.
  if (web_request.httpMethod() != "GET" &&
      web_request.httpMethod() != "POST")
    return PP_ERROR_BADARGUMENT;

  WebString target_str = WebString::fromUTF8(target);
  container_->loadFrameRequest(web_request, target_str, false, NULL);
  return PP_OK;
}

bool PluginInstance::IsViewAccelerated() {
  if (!container_)
    return false;

  WebDocument document = container_->element().document();
  WebFrame* frame = document.frame();
  if (!frame)
    return false;
  WebView* view = frame->view();
  if (!view)
    return false;

  return view->isAcceleratedCompositingActive();
}

PluginDelegate::PlatformContext3D* PluginInstance::CreateContext3D() {
  if (fullscreen_container_)
    return fullscreen_container_->CreateContext3D();
  else
    return delegate_->CreateContext3D();
}

bool PluginInstance::PrintPDFOutput(PP_Resource print_output,
                                    WebKit::WebCanvas* canvas) {
  ::ppapi::thunk::EnterResourceNoLock<PPB_Buffer_API> enter(print_output, true);
  if (enter.failed())
    return false;

  BufferAutoMapper mapper(enter.object());
  if (!mapper.data() || !mapper.size()) {
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
#if defined(OS_LINUX) || (defined(OS_MACOSX) && defined(USE_SKIA))
  // On Linux we just set the final bits in the native metafile
  // (NativeMetafile and PreviewMetafile must have compatible formats,
  // i.e. both PDF for this to work).
  printing::Metafile* metafile =
      printing::MetafileSkiaWrapper::GetMetafileFromCanvas(*canvas);
  DCHECK(metafile != NULL);
  if (metafile)
    ret = metafile->InitFromData(mapper.data(), mapper.size());
#elif defined(OS_MACOSX)
  printing::NativeMetafile metafile;
  // Create a PDF metafile and render from there into the passed in context.
  if (metafile.InitFromData(mapper.data(), mapper.size())) {
    // Flip the transform.
    CGContextRef cgContext = canvas;
    CGContextSaveGState(cgContext);
    CGContextTranslateCTM(cgContext, 0,
                          current_print_settings_.printable_area.size.height);
    CGContextScaleCTM(cgContext, 1.0, -1.0);
    CGRect page_rect;
    page_rect.origin.x = current_print_settings_.printable_area.point.x;
    page_rect.origin.y = current_print_settings_.printable_area.point.y;
    page_rect.size.width = current_print_settings_.printable_area.size.width;
    page_rect.size.height = current_print_settings_.printable_area.size.height;

    ret = metafile.RenderPage(1, cgContext, page_rect, true, false, true, true);
    CGContextRestoreGState(cgContext);
  }
#elif defined(OS_WIN)
  printing::Metafile* metafile =
    printing::MetafileSkiaWrapper::GetMetafileFromCanvas(*canvas);
  if (metafile) {
    // We only have a metafile when doing print preview, so we just want to
    // pass the PDF off to preview.
    ret = metafile->InitFromData(mapper.data(), mapper.size());
  } else {
    // On Windows, we now need to render the PDF to the DC that backs the
    // supplied canvas.
    HDC dc = skia::BeginPlatformPaint(canvas);
    DrawEmptyRectangle(dc);
    gfx::Size size_in_pixels;
    size_in_pixels.set_width(printing::ConvertUnit(
        current_print_settings_.printable_area.size.width,
        static_cast<int>(printing::kPointsPerInch),
        current_print_settings_.dpi));
    size_in_pixels.set_height(printing::ConvertUnit(
        current_print_settings_.printable_area.size.height,
        static_cast<int>(printing::kPointsPerInch),
        current_print_settings_.dpi));
    // We need to render using the actual printer DPI (rendering to a smaller
    // set of pixels leads to a blurry output). However, we need to counter the
    // scaling up that will happen in the browser.
    XFORM xform = {0};
    xform.eM11 = xform.eM22 = static_cast<float>(printing::kPointsPerInch) /
        static_cast<float>(current_print_settings_.dpi);
    ModifyWorldTransform(dc, &xform, MWT_LEFTMULTIPLY);

    ret = render_proc(static_cast<unsigned char*>(mapper.data()), mapper.size(),
                      0, dc, current_print_settings_.dpi,
                      current_print_settings_.dpi, 0, 0, size_in_pixels.width(),
                      size_in_pixels.height(), true, false, true, true, true);
    skia::EndPlatformPaint(canvas);
  }
#endif  // defined(OS_WIN)

  return ret;
}

bool PluginInstance::PrintRasterOutput(PP_Resource print_output,
                                       WebKit::WebCanvas* canvas) {
  EnterResourceNoLock<PPB_ImageData_API> enter(print_output, true);
  if (enter.failed())
    return false;
  PPB_ImageData_Impl* image =
      static_cast<PPB_ImageData_Impl*>(enter.object());

  // TODO(brettw) this should not require the image to be mapped. It should
  // instead map on demand. The DCHECK here is to remind you if you see the
  // assert fire, fix the bug rather than mapping the data.
  DCHECK(image->is_mapped());
  if (!image->is_mapped())
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
#if defined(OS_MACOSX) && !defined(USE_SKIA)
  draw_to_canvas = false;
  DrawSkBitmapToCanvas(*bitmap, canvas, dest_rect_gfx,
                       current_print_settings_.printable_area.size.height);
  // See comments in the header file.
  last_printed_page_ = image;
#else  // defined(OS_MACOSX) && !defined(USE_SKIA)
  if (draw_to_canvas)
    canvas->drawBitmapRect(*bitmap, &src_rect, dest_rect);
#endif  // defined(OS_MACOSX) && !defined(USE_SKIA)
  return true;
}

#if defined(OS_WIN)
bool PluginInstance::DrawJPEGToPlatformDC(
    const SkBitmap& bitmap,
    const gfx::Rect& printable_area,
    WebKit::WebCanvas* canvas) {
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

  skia::ScopedPlatformPaint scoped_platform_paint(canvas);
  HDC dc = scoped_platform_paint.GetPlatformSurface();
  DrawEmptyRectangle(dc);
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

#if defined(OS_MACOSX) && !defined(USE_SKIA)
void PluginInstance::DrawSkBitmapToCanvas(
    const SkBitmap& bitmap, WebKit::WebCanvas* canvas,
    const gfx::Rect& dest_rect,
    int canvas_height) {
  SkAutoLockPixels lock(bitmap);
  DCHECK(bitmap.getConfig() == SkBitmap::kARGB_8888_Config);
  base::mac::ScopedCFTypeRef<CGDataProviderRef> data_provider(
      CGDataProviderCreateWithData(
          NULL, bitmap.getAddr32(0, 0),
          bitmap.rowBytes() * bitmap.height(), NULL));
  base::mac::ScopedCFTypeRef<CGImageRef> image(
      CGImageCreate(
          bitmap.width(), bitmap.height(),
          8, 32, bitmap.rowBytes(),
          base::mac::GetSystemColorSpace(),
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
#endif  // defined(OS_MACOSX) && !defined(USE_SKIA)

PPB_Graphics2D_Impl* PluginInstance::GetBoundGraphics2D() const {
  if (bound_graphics_.get() == NULL)
    return NULL;

  if (bound_graphics_->AsPPB_Graphics2D_API())
    return static_cast<PPB_Graphics2D_Impl*>(bound_graphics_.get());
  return NULL;
}

PPB_Graphics3D_Impl* PluginInstance::GetBoundGraphics3D() const {
  if (bound_graphics_.get() == NULL)
    return NULL;

  if (bound_graphics_->AsPPB_Graphics3D_API())
    return static_cast<PPB_Graphics3D_Impl*>(bound_graphics_.get());
  return NULL;
}

void PluginInstance::setBackingTextureId(unsigned int id) {
  // If we have a fullscreen_container_ (under PPB_FlashFullscreen)
  // or desired_fullscreen_state is true (under PPB_Fullscreen),
  // then the plugin is fullscreen or transitioning to fullscreen
  // and the parent context is not the one for the browser page,
  // but for the fullscreen window, and so the parent texture ID
  // doesn't correspond to anything in the page's context.
  //
  // TODO(alokp): It would be better at some point to have the equivalent
  // in the FullscreenContainer so that we don't need to poll
  if (fullscreen_container_ || desired_fullscreen_state_)
    return;

  if (container_)
    container_->setBackingTextureId(id);
}

void PluginInstance::AddPluginObject(PluginObject* plugin_object) {
  DCHECK(live_plugin_objects_.find(plugin_object) ==
         live_plugin_objects_.end());
  live_plugin_objects_.insert(plugin_object);
}

void PluginInstance::RemovePluginObject(PluginObject* plugin_object) {
  // Don't actually verify that the object is in the set since during module
  // deletion we'll be in the process of freeing them.
  live_plugin_objects_.erase(plugin_object);
}

bool PluginInstance::IsFullPagePlugin() const {
  WebFrame* frame = container()->element().document().frame();
  return frame->view()->mainFrame()->document().isPluginDocument();
}

void PluginInstance::OnLockMouseACK(int32_t result) {
  if (!lock_mouse_callback_.func) {
    NOTREACHED();
    return;
  }

  PP_RunAndClearCompletionCallback(&lock_mouse_callback_, result);
}

void PluginInstance::OnMouseLockLost() {
  if (LoadMouseLockInterface())
    plugin_mouse_lock_interface_->MouseLockLost(pp_instance());
}

void PluginInstance::SimulateInputEvent(const InputEventData& input_event) {
  WebView* web_view = container()->element().document().frame()->view();
  if (!web_view) {
    NOTREACHED();
    return;
  }

  std::vector<linked_ptr<WebInputEvent> > events =
      CreateSimulatedWebInputEvents(
          input_event,
          position().x() + position().width() / 2,
          position().y() + position().height() / 2);
  for (std::vector<linked_ptr<WebInputEvent> >::iterator it = events.begin();
      it != events.end(); ++it) {
    web_view->handleInputEvent(*it->get());
  }
}

PPB_Instance_FunctionAPI* PluginInstance::AsPPB_Instance_FunctionAPI() {
  return this;
}

PP_Bool PluginInstance::BindGraphics(PP_Instance instance,
                                     PP_Resource device) {
  if (bound_graphics_.get()) {
    if (GetBoundGraphics2D()) {
      GetBoundGraphics2D()->BindToInstance(NULL);
    } else if (GetBoundGraphics3D()) {
      GetBoundGraphics3D()->BindToInstance(false);
    }
    bound_graphics_ = NULL;
  }

  // Special-case clearing the current device.
  if (!device) {
    setBackingTextureId(0);
    InvalidateRect(gfx::Rect());
    return PP_TRUE;
  }

  // Refuse to bind if in transition to fullscreen with PPB_FlashFullscreen or
  // to/from fullscreen with PPB_Fullscreen.
  if ((fullscreen_container_ && !flash_fullscreen_) ||
      desired_fullscreen_state_ != fullscreen_)
    return PP_FALSE;

  EnterResourceNoLock<PPB_Graphics2D_API> enter_2d(device, false);
  PPB_Graphics2D_Impl* graphics_2d = enter_2d.succeeded() ?
      static_cast<PPB_Graphics2D_Impl*>(enter_2d.object()) : NULL;
  EnterResourceNoLock<PPB_Graphics3D_API> enter_3d(device, false);
  PPB_Graphics3D_Impl* graphics_3d = enter_3d.succeeded() ?
      static_cast<PPB_Graphics3D_Impl*>(enter_3d.object()) : NULL;

  if (graphics_2d) {
    if (graphics_2d->pp_instance() != pp_instance())
      return PP_FALSE;  // Can't bind other instance's contexts.
    if (!graphics_2d->BindToInstance(this))
      return PP_FALSE;  // Can't bind to more than one instance.

    bound_graphics_ = graphics_2d;
    setBackingTextureId(0);
    // BindToInstance will have invalidated the plugin if necessary.
  } else if (graphics_3d) {
    // Make sure graphics can only be bound to the instance it is
    // associated with.
    if (graphics_3d->pp_instance() != pp_instance())
      return PP_FALSE;
    if (!graphics_3d->BindToInstance(true))
      return PP_FALSE;

    bound_graphics_ = graphics_3d;
    setBackingTextureId(graphics_3d->GetBackingTextureId());
  } else {
    // The device is not a valid resource type.
    return PP_FALSE;
  }

  return PP_TRUE;
}

PP_Bool PluginInstance::IsFullFrame(PP_Instance instance) {
  return PP_FromBool(full_frame());
}

PP_Var PluginInstance::GetWindowObject(PP_Instance instance) {
  if (!container_)
    return PP_MakeUndefined();

  WebFrame* frame = container_->element().document().frame();
  if (!frame)
    return PP_MakeUndefined();

  return NPObjectToPPVar(this, frame->windowObject());
}

PP_Var PluginInstance::GetOwnerElementObject(PP_Instance instance) {
  if (!container_)
    return PP_MakeUndefined();
  return NPObjectToPPVar(this, container_->scriptableObjectForElement());
}

PP_Var PluginInstance::ExecuteScript(PP_Instance instance,
                                     PP_Var script,
                                     PP_Var* exception) {
  // Executing the script may remove the plugin from the DOM, so we need to keep
  // a reference to ourselves so that we can still process the result after the
  // WebBindings::evaluate() below.
  scoped_refptr<PluginInstance> ref(this);
  TryCatch try_catch(module()->pp_module(), exception);
  if (try_catch.has_exception())
    return PP_MakeUndefined();

  // Convert the script into an inconvenient NPString object.
  StringVar* script_string = StringVar::FromPPVar(script);
  if (!script_string) {
    try_catch.SetException("Script param to ExecuteScript must be a string.");
    return PP_MakeUndefined();
  }
  NPString np_script;
  np_script.UTF8Characters = script_string->value().c_str();
  np_script.UTF8Length = script_string->value().length();

  // Get the current frame to pass to the evaluate function.
  WebFrame* frame = container_->element().document().frame();
  if (!frame) {
    try_catch.SetException("No frame to execute script in.");
    return PP_MakeUndefined();
  }

  NPVariant result;
  bool ok = WebBindings::evaluate(NULL, frame->windowObject(), &np_script,
                                  &result);
  if (!ok) {
    // TODO(brettw) bug 54011: The TryCatch isn't working properly and
    // doesn't actually catch this exception.
    try_catch.SetException("Exception caught");
    WebBindings::releaseVariantValue(&result);
    return PP_MakeUndefined();
  }

  PP_Var ret = NPVariantToPPVar(this, &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

PP_Var PluginInstance::GetDefaultCharSet(PP_Instance instance) {
  std::string encoding = delegate()->GetDefaultEncoding();
  return StringVar::StringToPPVar(module()->pp_module(), encoding);
}

void PluginInstance::Log(PP_Instance instance,
                         int log_level,
                         PP_Var value) {
  // TODO(brettw) get the plugin name and use it as the source.
  LogWithSource(instance, log_level, PP_MakeUndefined(), value);
}

void PluginInstance::LogWithSource(PP_Instance instance,
                                   int log_level,
                                   PP_Var source,
                                   PP_Var value) {
  // Convert the log level, defaulting to error.
  WebConsoleMessage::Level web_level;
  switch (log_level) {
    case PP_LOGLEVEL_TIP:
      web_level = WebConsoleMessage::LevelTip;
      break;
    case PP_LOGLEVEL_LOG:
      web_level = WebConsoleMessage::LevelLog;
      break;
    case PP_LOGLEVEL_WARNING:
      web_level = WebConsoleMessage::LevelWarning;
      break;
    case PP_LOGLEVEL_ERROR:
    default:
      web_level = WebConsoleMessage::LevelError;
      break;
  }

  // Format is the "<source>: <value>". The source defaults to the module name
  // if the source isn't a string or is empty.
  std::string message;
  if (source.type == PP_VARTYPE_STRING)
    message = Var::PPVarToLogString(source);
  if (message.empty())
    message = module()->name();
  message.append(": ");
  message.append(Var::PPVarToLogString(value));

  container()->element().document().frame()->addMessageToConsole(
      WebConsoleMessage(web_level, WebString(UTF8ToUTF16(message))));
}

void PluginInstance::NumberOfFindResultsChanged(PP_Instance instance,
                                                int32_t total,
                                                PP_Bool final_result) {
  DCHECK_NE(find_identifier_, -1);
  delegate_->NumberOfFindResultsChanged(find_identifier_, total,
                                        PP_ToBool(final_result));
}

void PluginInstance::SelectedFindResultChanged(PP_Instance instance,
                                               int32_t index) {
  DCHECK_NE(find_identifier_, -1);
  delegate_->SelectedFindResultChanged(find_identifier_, index);
}

PP_Bool PluginInstance::IsFullscreen(PP_Instance instance) {
  return PP_FromBool(fullscreen_);
}

PP_Bool PluginInstance::FlashIsFullscreen(PP_Instance instance) {
  return PP_FromBool(flash_fullscreen_);
}

PP_Bool PluginInstance::SetFullscreen(PP_Instance instance,
                                      PP_Bool fullscreen) {
  return PP_FromBool(SetFullscreen(PP_ToBool(fullscreen)));
}

PP_Bool PluginInstance::FlashSetFullscreen(PP_Instance instance,
                                           PP_Bool fullscreen) {
  FlashSetFullscreen(PP_ToBool(fullscreen), true);
  return PP_TRUE;
}

PP_Bool PluginInstance::GetScreenSize(PP_Instance instance, PP_Size* size) {
  gfx::Size screen_size = delegate()->GetScreenSize();
  *size = PP_MakeSize(screen_size.width(), screen_size.height());
  return PP_TRUE;
}

PP_Bool PluginInstance::FlashGetScreenSize(PP_Instance instance,
                                           PP_Size* size) {
  return GetScreenSize(instance, size);
}

int32_t PluginInstance::RequestInputEvents(PP_Instance instance,
                                           uint32_t event_classes) {
  input_event_mask_ |= event_classes;
  filtered_input_event_mask_ &= ~(event_classes);
  return ValidateRequestInputEvents(false, event_classes);
}

int32_t PluginInstance::RequestFilteringInputEvents(PP_Instance instance,
                                                    uint32_t event_classes) {
  filtered_input_event_mask_ |= event_classes;
  input_event_mask_ &= ~(event_classes);
  return ValidateRequestInputEvents(true, event_classes);
}

void PluginInstance::ClearInputEventRequest(PP_Instance instance,
                                            uint32_t event_classes) {
  input_event_mask_ &= ~(event_classes);
  filtered_input_event_mask_ &= ~(event_classes);
}

void PluginInstance::ZoomChanged(PP_Instance instance, double factor) {
  // We only want to tell the page to change its zoom if the whole page is the
  // plugin.  If we're in an iframe, then don't do anything.
  if (!IsFullPagePlugin())
    return;
  container()->zoomLevelChanged(WebView::zoomFactorToZoomLevel(factor));
}

void PluginInstance::ZoomLimitsChanged(PP_Instance instance,
                                       double minimum_factor,
                                       double maximium_factor) {
  if (minimum_factor > maximium_factor) {
    NOTREACHED();
    return;
  }
  delegate()->ZoomLimitsChanged(minimum_factor, maximium_factor);
}

void PluginInstance::PostMessage(PP_Instance instance, PP_Var message) {
  message_channel_->PostMessageToJavaScript(message);
}

int32_t PluginInstance::LockMouse(PP_Instance instance,
                                  PP_CompletionCallback callback) {
  if (!callback.func) {
    // Don't support synchronous call.
    return PP_ERROR_BLOCKS_MAIN_THREAD;
  }
  if (lock_mouse_callback_.func)
    return PP_ERROR_INPROGRESS;
  if (!CanAccessMainFrame())
    return PP_ERROR_NOACCESS;

  lock_mouse_callback_ = callback;
  // We will be notified on completion via OnLockMouseACK(), either
  // synchronously or asynchronously.
  delegate()->LockMouse(this);
  return PP_OK_COMPLETIONPENDING;
}

void PluginInstance::UnlockMouse(PP_Instance instance) {
  delegate()->UnlockMouse(this);
}

PP_Var PluginInstance::ResolveRelativeToDocument(
    PP_Instance instance,
    PP_Var relative,
    PP_URLComponents_Dev* components) {
  StringVar* relative_string = StringVar::FromPPVar(relative);
  if (!relative_string)
    return PP_MakeNull();

  WebElement plugin_element = container()->element();
  GURL document_url = plugin_element.document().baseURL();
  return ::ppapi::URLUtilImpl::GenerateURLReturn(
      module()->pp_module(),
      document_url.Resolve(relative_string->value()),
      components);
}

PP_Bool PluginInstance::DocumentCanRequest(PP_Instance instance, PP_Var url) {
  StringVar* url_string = StringVar::FromPPVar(url);
  if (!url_string)
    return PP_FALSE;

  WebKit::WebSecurityOrigin security_origin;
  if (!SecurityOriginForInstance(instance, &security_origin))
    return PP_FALSE;

  GURL gurl(url_string->value());
  if (!gurl.is_valid())
    return PP_FALSE;

  return BoolToPPBool(security_origin.canRequest(gurl));
}

PP_Bool PluginInstance::DocumentCanAccessDocument(PP_Instance instance,
                                                  PP_Instance target) {
  WebKit::WebSecurityOrigin our_origin;
  if (!SecurityOriginForInstance(instance, &our_origin))
    return PP_FALSE;

  WebKit::WebSecurityOrigin target_origin;
  if (!SecurityOriginForInstance(instance, &target_origin))
    return PP_FALSE;

  return BoolToPPBool(our_origin.canAccess(target_origin));
}

PP_Var PluginInstance::GetDocumentURL(PP_Instance instance,
                                      PP_URLComponents_Dev* components) {
  WebKit::WebDocument document = container()->element().document();
  return ::ppapi::URLUtilImpl::GenerateURLReturn(module()->pp_module(),
                                                 document.url(), components);
}

PP_Var PluginInstance::GetPluginInstanceURL(
    PP_Instance instance,
    PP_URLComponents_Dev* components) {
  return ::ppapi::URLUtilImpl::GenerateURLReturn(module()->pp_module(),
                                                 plugin_url_, components);
}

void PluginInstance::DoSetCursor(WebCursorInfo* cursor) {
  cursor_.reset(cursor);
  if (fullscreen_container_) {
    fullscreen_container_->DidChangeCursor(*cursor);
  } else {
    delegate()->DidChangeCursor(this, *cursor);
  }
}

bool PluginInstance::CanAccessMainFrame() const {
  if (!container_)
    return false;
  WebKit::WebDocument containing_document = container_->element().document();

  if (!containing_document.frame() ||
      !containing_document.frame()->view() ||
      !containing_document.frame()->view()->mainFrame()) {
    return false;
  }
  WebKit::WebDocument main_document =
      containing_document.frame()->view()->mainFrame()->document();

  return containing_document.securityOrigin().canAccess(
      main_document.securityOrigin());
}

void PluginInstance::SetSentDidChangeView(const gfx::Rect& position,
                                          const gfx::Rect& clip) {
  sent_did_change_view_ = true;
  position_ = position;
  clip_ = clip;
}

void PluginInstance::KeepSizeAttributesBeforeFullscreen() {
  WebElement element = container_->element();
  width_before_fullscreen_ = element.getAttribute(WebString::fromUTF8(kWidth));
  height_before_fullscreen_ =
      element.getAttribute(WebString::fromUTF8(kHeight));
  border_before_fullscreen_ =
      element.getAttribute(WebString::fromUTF8(kBorder));
  style_before_fullscreen_ = element.getAttribute(WebString::fromUTF8(kStyle));
}

void PluginInstance::SetSizeAttributesForFullscreen() {
  screen_size_for_fullscreen_ = delegate()->GetScreenSize();
  std::string width = StringPrintf("%d", screen_size_for_fullscreen_.width());
  std::string height = StringPrintf("%d", screen_size_for_fullscreen_.height());

  WebElement element = container_->element();
  element.setAttribute(WebString::fromUTF8(kWidth), WebString::fromUTF8(width));
  element.setAttribute(WebString::fromUTF8(kHeight),
                       WebString::fromUTF8(height));
  element.setAttribute(WebString::fromUTF8(kBorder), WebString::fromUTF8("0"));

  // There should be no style settings that matter in fullscreen mode,
  // so just replace them instead of appending.
  // NOTE: "position: fixed" and "display: block" reset the plugin and
  // using %% settings might not work without them (e.g. if the plugin is a
  // child of a container element).
  std::string style;
  style += StringPrintf("width: %s !important; ", width.c_str());
  style += StringPrintf("height: %s !important; ", height.c_str());
  style += "margin: 0 !important; padding: 0 !important; border: 0 !important";
  container_->element().setAttribute(kStyle, WebString::fromUTF8(style));
}

void PluginInstance::ResetSizeAttributesAfterFullscreen() {
  screen_size_for_fullscreen_ = gfx::Size();
  WebElement element = container_->element();
  element.setAttribute(WebString::fromUTF8(kWidth), width_before_fullscreen_);
  element.setAttribute(WebString::fromUTF8(kHeight), height_before_fullscreen_);
  element.setAttribute(WebString::fromUTF8(kBorder), border_before_fullscreen_);
  element.setAttribute(WebString::fromUTF8(kStyle), style_before_fullscreen_);
}

}  // namespace ppapi
}  // namespace webkit
