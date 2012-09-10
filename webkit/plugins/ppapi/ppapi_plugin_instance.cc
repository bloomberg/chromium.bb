// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_offset_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor_client.h"
#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/c/dev/ppb_zoom_dev.h"
#include "ppapi/c/dev/ppp_find_dev.h"
#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/c/dev/ppp_text_input_dev.h"
#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/c/ppp_mouse_lock.h"
#include "ppapi/c/private/pp_content_decryptor.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/ppb_gamepad_shared.h"
#include "ppapi/shared_impl/ppb_input_event_shared.h"
#include "ppapi/shared_impl/ppb_url_util_shared.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/shared_impl/ppp_instance_combined.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "printing/units.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGamepads.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPrintParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPrintScalingOption.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScopedUserGesture.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/range/range.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "webkit/plugins/plugin_constants.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/event_conversion.h"
#include "webkit/plugins/ppapi/fullscreen_container.h"
#include "webkit/plugins/ppapi/gfx_conversion.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/message_channel.h"
#include "webkit/plugins/ppapi/npapi_glue.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/plugin_object.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"
#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"
#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/ppp_pdf.h"
#include "webkit/plugins/sad_plugin.h"

#if defined(OS_MACOSX)
#include "printing/metafile_impl.h"
#if !defined(USE_SKIA)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#endif  // !defined(USE_SKIA)
#endif  // defined(OS_MACOSX)

#if defined(USE_SKIA)
#include "printing/metafile.h"
#include "printing/metafile_skia_wrapper.h"
#include "skia/ext/platform_device.h"
#endif

#if defined(OS_WIN)
#include "base/metrics/histogram.h"
#include "base/win/windows_version.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/gdi_util.h"
#endif

using base::StringPrintf;
using ppapi::InputEventData;
using ppapi::PpapiGlobals;
using ppapi::PPB_InputEvent_Shared;
using ppapi::PPB_View_Shared;
using ppapi::PPP_Instance_Combined;
using ppapi::ScopedPPResource;
using ppapi::StringVar;
using ppapi::TrackedCallback;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_Graphics2D_API;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::PPB_ImageData_API;
using ppapi::Var;
using ppapi::ArrayBufferVar;
using ppapi::ViewData;
using WebKit::WebBindings;
using WebKit::WebCanvas;
using WebKit::WebCursorInfo;
using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebInputEvent;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPrintParams;
using WebKit::WebPrintScalingOption;
using WebKit::WebScopedUserGesture;
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

// Check PP_TextInput_Type and ui::TextInputType are kept in sync.
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_NONE) == \
    int(PP_TEXTINPUT_TYPE_NONE), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_TEXT) == \
    int(PP_TEXTINPUT_TYPE_TEXT), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_PASSWORD) == \
    int(PP_TEXTINPUT_TYPE_PASSWORD), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_SEARCH) == \
    int(PP_TEXTINPUT_TYPE_SEARCH), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_EMAIL) == \
    int(PP_TEXTINPUT_TYPE_EMAIL), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_NUMBER) == \
    int(PP_TEXTINPUT_TYPE_NUMBER), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_TELEPHONE) == \
    int(PP_TEXTINPUT_TYPE_TELEPHONE), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_URL) == \
    int(PP_TEXTINPUT_TYPE_URL), mismatching_enums);

// The default text input type is to regard the plugin always accept text input.
// This is for allowing users to use input methods even on completely-IME-
// unaware plugins (e.g., PPAPI Flash or PDF plugin for M16).
// Plugins need to explicitly opt out the text input mode if they know
// that they don't accept texts.
const ui::TextInputType kPluginDefaultTextInputType = ui::TEXT_INPUT_TYPE_TEXT;

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, np_name) \
    COMPILE_ASSERT(static_cast<int>(WebCursorInfo::webkit_name) \
                       == static_cast<int>(np_name), \
                   mismatching_enums)

#define COMPILE_ASSERT_PRINT_SCALING_MATCHING_ENUM(webkit_name, pp_name) \
    COMPILE_ASSERT(static_cast<int>(webkit_name) \
                       == static_cast<int>(pp_name), \
                   mismatching_enums)

// <embed>/<object> attributes.
const char kWidth[] = "width";
const char kHeight[] = "height";
const char kBorder[] = "border";  // According to w3c, deprecated.
const char kStyle[] = "style";

COMPILE_ASSERT_MATCHING_ENUM(TypePointer, PP_MOUSECURSOR_TYPE_POINTER);
COMPILE_ASSERT_MATCHING_ENUM(TypeCross, PP_MOUSECURSOR_TYPE_CROSS);
COMPILE_ASSERT_MATCHING_ENUM(TypeHand, PP_MOUSECURSOR_TYPE_HAND);
COMPILE_ASSERT_MATCHING_ENUM(TypeIBeam, PP_MOUSECURSOR_TYPE_IBEAM);
COMPILE_ASSERT_MATCHING_ENUM(TypeWait, PP_MOUSECURSOR_TYPE_WAIT);
COMPILE_ASSERT_MATCHING_ENUM(TypeHelp, PP_MOUSECURSOR_TYPE_HELP);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastResize, PP_MOUSECURSOR_TYPE_EASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthResize, PP_MOUSECURSOR_TYPE_NORTHRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastResize,
                             PP_MOUSECURSOR_TYPE_NORTHEASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestResize,
                             PP_MOUSECURSOR_TYPE_NORTHWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthResize, PP_MOUSECURSOR_TYPE_SOUTHRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthEastResize,
                             PP_MOUSECURSOR_TYPE_SOUTHEASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthWestResize,
                             PP_MOUSECURSOR_TYPE_SOUTHWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeWestResize, PP_MOUSECURSOR_TYPE_WESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthSouthResize,
                             PP_MOUSECURSOR_TYPE_NORTHSOUTHRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastWestResize,
                             PP_MOUSECURSOR_TYPE_EASTWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastSouthWestResize,
                             PP_MOUSECURSOR_TYPE_NORTHEASTSOUTHWESTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestSouthEastResize,
                             PP_MOUSECURSOR_TYPE_NORTHWESTSOUTHEASTRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeColumnResize,
                             PP_MOUSECURSOR_TYPE_COLUMNRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeRowResize, PP_MOUSECURSOR_TYPE_ROWRESIZE);
COMPILE_ASSERT_MATCHING_ENUM(TypeMiddlePanning,
                             PP_MOUSECURSOR_TYPE_MIDDLEPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeEastPanning, PP_MOUSECURSOR_TYPE_EASTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthPanning,
                             PP_MOUSECURSOR_TYPE_NORTHPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthEastPanning,
                             PP_MOUSECURSOR_TYPE_NORTHEASTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeNorthWestPanning,
                             PP_MOUSECURSOR_TYPE_NORTHWESTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthPanning,
                             PP_MOUSECURSOR_TYPE_SOUTHPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthEastPanning,
                             PP_MOUSECURSOR_TYPE_SOUTHEASTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeSouthWestPanning,
                             PP_MOUSECURSOR_TYPE_SOUTHWESTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeWestPanning, PP_MOUSECURSOR_TYPE_WESTPANNING);
COMPILE_ASSERT_MATCHING_ENUM(TypeMove, PP_MOUSECURSOR_TYPE_MOVE);
COMPILE_ASSERT_MATCHING_ENUM(TypeVerticalText,
                             PP_MOUSECURSOR_TYPE_VERTICALTEXT);
COMPILE_ASSERT_MATCHING_ENUM(TypeCell, PP_MOUSECURSOR_TYPE_CELL);
COMPILE_ASSERT_MATCHING_ENUM(TypeContextMenu, PP_MOUSECURSOR_TYPE_CONTEXTMENU);
COMPILE_ASSERT_MATCHING_ENUM(TypeAlias, PP_MOUSECURSOR_TYPE_ALIAS);
COMPILE_ASSERT_MATCHING_ENUM(TypeProgress, PP_MOUSECURSOR_TYPE_PROGRESS);
COMPILE_ASSERT_MATCHING_ENUM(TypeNoDrop, PP_MOUSECURSOR_TYPE_NODROP);
COMPILE_ASSERT_MATCHING_ENUM(TypeCopy, PP_MOUSECURSOR_TYPE_COPY);
COMPILE_ASSERT_MATCHING_ENUM(TypeNone, PP_MOUSECURSOR_TYPE_NONE);
COMPILE_ASSERT_MATCHING_ENUM(TypeNotAllowed, PP_MOUSECURSOR_TYPE_NOTALLOWED);
COMPILE_ASSERT_MATCHING_ENUM(TypeZoomIn, PP_MOUSECURSOR_TYPE_ZOOMIN);
COMPILE_ASSERT_MATCHING_ENUM(TypeZoomOut, PP_MOUSECURSOR_TYPE_ZOOMOUT);
COMPILE_ASSERT_MATCHING_ENUM(TypeGrab, PP_MOUSECURSOR_TYPE_GRAB);
COMPILE_ASSERT_MATCHING_ENUM(TypeGrabbing, PP_MOUSECURSOR_TYPE_GRABBING);
// Do not assert WebCursorInfo::TypeCustom == PP_CURSORTYPE_CUSTOM;
// PP_CURSORTYPE_CUSTOM is pinned to allow new cursor types.

COMPILE_ASSERT_PRINT_SCALING_MATCHING_ENUM(WebKit::WebPrintScalingOptionNone,
                                           PP_PRINTSCALINGOPTION_NONE);
COMPILE_ASSERT_PRINT_SCALING_MATCHING_ENUM(
    WebKit::WebPrintScalingOptionFitToPrintableArea,
    PP_PRINTSCALINGOPTION_FIT_TO_PRINTABLE_AREA);
COMPILE_ASSERT_PRINT_SCALING_MATCHING_ENUM(
    WebKit::WebPrintScalingOptionSourceSize, PP_PRINTSCALINGOPTION_SOURCE_SIZE);

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

// Convert the given vector to an array of C-strings. The strings in the
// returned vector are only guaranteed valid so long as the vector of strings
// is not modified.
scoped_array<const char*> StringVectorToArgArray(
    const std::vector<std::string>& vector) {
  scoped_array<const char*> array(new const char*[vector.size()]);
  for (size_t i = 0; i < vector.size(); ++i)
    array[i] = vector[i].c_str();
  return array.Pass();
}

// Creates a PP_Resource containing a PPB_Buffer_Impl, copies |data| into the
// buffer resource, and returns it. Returns a an invalid PP_Resource with an ID
// of 0 on failure. Upon success, the returned Buffer resource has a reference
// count of 1.
PP_Resource MakeBufferResource(PP_Instance instance,
                               const uint8* data, int size) {
  if (!data || !size)
    return 0;

  ScopedPPResource resource(PPB_Buffer_Impl::Create(instance, size));
  if (!resource.get())
    return 0;

  EnterResourceNoLock<PPB_Buffer_API> enter(resource, true);
  if (enter.failed())
    return 0;

  BufferAutoMapper mapper(enter.object());
  if (!mapper.data() || mapper.size() < static_cast<size_t>(size))
    return 0;

  memcpy(mapper.data(), data, size);
  return resource.get();
}

// Copies the content of |str| into |array|.
// Returns true if copy succeeded. Returns false if copy failed, e.g. if the
// |array_size| is smaller than the |str| length.
template <uint32_t array_size>
bool CopyStringToArray(const std::string& str, uint8 (&array)[array_size]) {
  if (array_size < str.size())
    return false;

  memcpy(array, str.data(), str.size());
  return true;
}

// Fills the |block_info| with information from |decrypt_config|, |timestamp|
// and |request_id|.
// Returns true if |block_info| is successfully filled. Returns false otherwise.
bool MakeEncryptedBlockInfo(
    const media::DecryptConfig& decrypt_config,
    int64_t timestamp,
    uint32_t request_id,
    PP_EncryptedBlockInfo* block_info) {
  DCHECK(block_info);

  // TODO(xhwang): Fix initialization of PP_EncryptedBlockInfo here and
  // anywhere else.
  memset(block_info, 0, sizeof(*block_info));

  block_info->tracking_info.request_id = request_id;
  block_info->tracking_info.timestamp = timestamp;
  block_info->data_offset = decrypt_config.data_offset();

  if (!CopyStringToArray(decrypt_config.key_id(), block_info->key_id) ||
      !CopyStringToArray(decrypt_config.iv(), block_info->iv) ||
      !CopyStringToArray(decrypt_config.checksum(), block_info->checksum))
    return false;

  block_info->key_id_size = decrypt_config.key_id().size();
  block_info->iv_size = decrypt_config.iv().size();
  block_info->checksum_size = decrypt_config.checksum().size();

  if (decrypt_config.subsamples().size() > arraysize(block_info->subsamples))
    return false;

  block_info->num_subsamples = decrypt_config.subsamples().size();
  for (uint32_t i = 0; i < block_info->num_subsamples; ++i) {
    block_info->subsamples[i].clear_bytes =
        decrypt_config.subsamples()[i].clear_bytes;
    block_info->subsamples[i].cipher_bytes =
        decrypt_config.subsamples()[i].cypher_bytes;
  }

  return true;
}

}  // namespace

// static
PluginInstance* PluginInstance::Create(PluginDelegate* delegate,
                                       PluginModule* module) {
  base::Callback<const void*(const char*)> get_plugin_interface_func =
      base::Bind(&PluginModule::GetPluginInterface, module);
  PPP_Instance_Combined* ppp_instance_combined =
      PPP_Instance_Combined::Create(get_plugin_interface_func);
  if (!ppp_instance_combined)
    return NULL;
  return new PluginInstance(delegate, module, ppp_instance_combined);
}

PluginInstance::GamepadImpl::GamepadImpl(PluginDelegate* delegate)
    : delegate_(delegate) {
}

void PluginInstance::GamepadImpl::Sample(PP_GamepadsSampleData* data) {
  WebKit::WebGamepads webkit_data;
  delegate_->SampleGamepads(&webkit_data);
  ConvertWebKitGamepadData(
      *reinterpret_cast<const ::ppapi::WebKitGamepads*>(&webkit_data), data);
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
      sent_initial_did_change_view_(false),
      view_change_weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      has_webkit_focus_(false),
      has_content_area_focus_(false),
      find_identifier_(-1),
      plugin_decryption_interface_(NULL),
      plugin_find_interface_(NULL),
      plugin_input_event_interface_(NULL),
      plugin_messaging_interface_(NULL),
      plugin_mouse_lock_interface_(NULL),
      plugin_pdf_interface_(NULL),
      plugin_private_interface_(NULL),
      plugin_selection_interface_(NULL),
      plugin_textinput_interface_(NULL),
      plugin_zoom_interface_(NULL),
      checked_for_plugin_input_event_interface_(false),
      checked_for_plugin_messaging_interface_(false),
      gamepad_impl_(delegate),
      plugin_print_interface_(NULL),
      plugin_graphics_3d_interface_(NULL),
      always_on_top_(false),
      fullscreen_container_(NULL),
      flash_fullscreen_(false),
      desired_fullscreen_state_(false),
      message_channel_(NULL),
      sad_plugin_(NULL),
      input_event_mask_(0),
      filtered_input_event_mask_(0),
      text_input_type_(kPluginDefaultTextInputType),
      text_input_caret_(0, 0, 0, 0),
      text_input_caret_bounds_(0, 0, 0, 0),
      text_input_caret_set_(false),
      selection_caret_(0),
      selection_anchor_(0),
      pending_user_gesture_(0.0),
      flash_impl_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      decryptor_client_(NULL),
      next_decryption_request_id_(1) {
  pp_instance_ = HostGlobals::Get()->AddInstance(this);

  memset(&current_print_settings_, 0, sizeof(current_print_settings_));
  DCHECK(delegate);
  module_->InstanceCreated(this);
  delegate_->InstanceCreated(this);
  message_channel_.reset(new MessageChannel(this));

  view_data_.is_page_visible = delegate->IsPageVisible();

  resource_creation_ = delegate_->CreateResourceCreationAPI(this);
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

  if (lock_mouse_callback_)
    TrackedCallback::ClearAndAbort(&lock_mouse_callback_);

  delegate_->InstanceDeleted(this);
  module_->InstanceDeleted(this);
  // If we switched from the NaCl plugin module, notify it too.
  if (original_module_.get())
    original_module_->InstanceDeleted(this);

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
  // Force the MessageChannel to release its "passthrough object" which should
  // release our last reference to the "InstanceObject" and will probably
  // destroy it. We want to do this prior to calling DidDestroy in case the
  // destructor of the instance object tries to use the instance.
  message_channel_->SetPassthroughObject(NULL);
  // If this is a NaCl plugin instance, shut down the NaCl plugin by calling
  // its DidDestroy. Don't call DidDestroy on the untrusted plugin instance,
  // since there is little that it can do at this point.
  if (original_instance_interface_.get())
    original_instance_interface_->DidDestroy(pp_instance());
  else
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
    if (!container_ ||
        view_data_.rect.size.width == 0 || view_data_.rect.size.height == 0)
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
  // Unbind current 2D or 3D graphics context.
  BindGraphics(pp_instance(), 0);
  InvalidateRect(gfx::Rect());

  delegate()->PluginCrashed(this);
}

static void SetGPUHistogram(const ::ppapi::Preferences& prefs,
                            const std::vector<std::string>& arg_names,
                            const std::vector<std::string>& arg_values) {
  // Calculate a histogram to let us determine how likely people are to try to
  // run Stage3D content on machines that have it blacklisted.
#if defined(OS_WIN)
  bool needs_gpu = false;
  bool is_xp = base::win::GetVersion() <= base::win::VERSION_XP;

  for (size_t i = 0; i < arg_names.size(); i++) {
    if (arg_names[i] == "wmode") {
      // In theory content other than Flash could have a "wmode" argument,
      // but that's pretty unlikely.
      if (arg_values[i] == "direct" || arg_values[i] == "gpu")
        needs_gpu = true;
      break;
    }
  }
  // 0 : No 3D content and GPU is blacklisted
  // 1 : No 3D content and GPU is not blacklisted
  // 2 : 3D content but GPU is blacklisted
  // 3 : 3D content and GPU is not blacklisted
  // 4 : No 3D content and GPU is blacklisted on XP
  // 5 : No 3D content and GPU is not blacklisted on XP
  // 6 : 3D content but GPU is blacklisted on XP
  // 7 : 3D content and GPU is not blacklisted on XP
  UMA_HISTOGRAM_ENUMERATION("Flash.UsesGPU",
      is_xp * 4 + needs_gpu * 2 + prefs.is_webgl_supported, 8);
#endif
}

bool PluginInstance::Initialize(WebPluginContainer* container,
                                const std::vector<std::string>& arg_names,
                                const std::vector<std::string>& arg_values,
                                const GURL& plugin_url,
                                bool full_frame) {
  container_ = container;
  plugin_url_ = plugin_url;
  full_frame_ = full_frame;

  container_->setIsAcceptingTouchEvents(IsAcceptingTouchEvents());

  SetGPUHistogram(delegate_->GetPreferences(), arg_names, arg_values);

  argn_ = arg_names;
  argv_ = arg_values;
  scoped_array<const char*> argn_array(StringVectorToArgArray(argn_));
  scoped_array<const char*> argv_array(StringVectorToArgArray(argv_));
  return PP_ToBool(instance_interface_->DidCreate(pp_instance(),
                                                  argn_.size(),
                                                  argn_array.get(),
                                                  argv_array.get()));
}

bool PluginInstance::HandleDocumentLoad(PPB_URLLoader_Impl* loader) {
  if (!document_loader_)
    document_loader_ = loader;
  DCHECK(loader == document_loader_.get());

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
    handled = true;  // Unfiltered events are assumed to be handled.
  scoped_refptr<PPB_InputEvent_Shared> event_resource(
      new PPB_InputEvent_Shared(::ppapi::OBJECT_IS_IMPL, pp_instance(), event));
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

void PluginInstance::GetSurroundingText(string16* text,
                                        ui::Range* range) const {
  std::vector<size_t> offsets;
  offsets.push_back(selection_anchor_);
  offsets.push_back(selection_caret_);
  *text = UTF8ToUTF16AndAdjustOffsets(surrounding_text_, &offsets);
  range->set_start(offsets[0] == string16::npos ? text->size() : offsets[0]);
  range->set_end(offsets[1] == string16::npos ? text->size() : offsets[1]);
}

bool PluginInstance::IsPluginAcceptingCompositionEvents() const {
  return (filtered_input_event_mask_ & PP_INPUTEVENT_CLASS_IME) ||
      (input_event_mask_ & PP_INPUTEVENT_CLASS_IME);
}

gfx::Rect PluginInstance::GetCaretBounds() const {
  if (!text_input_caret_set_) {
    // If it is never set by the plugin, use the bottom left corner.
    return gfx::Rect(view_data_.rect.point.x,
                     view_data_.rect.point.y + view_data_.rect.size.height,
                     0, 0);
  }

  // TODO(kinaba) Take CSS transformation into accont.
  // TODO(kinaba) Take bounding_box into account. On some platforms, an
  // "exclude rectangle" where candidate window must avoid the region can be
  // passed to IME. Currently, we pass only the caret rectangle because
  // it is the only information supported uniformly in Chromium.
  gfx::Rect caret(text_input_caret_);
  caret.Offset(view_data_.rect.point.x, view_data_.rect.point.y);
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

      // Allow the user gesture to be pending after the plugin handles the
      // event. This allows out-of-process plugins to respond to the user
      // gesture after processing has finished here.
      WebFrame* frame = container_->element().document().frame();
      if (frame->isProcessingUserGesture()) {
        pending_user_gesture_ =
            ::ppapi::EventTimeToPPTimeTicks(event.timeStampSeconds);
      }

      // Each input event may generate more than one PP_InputEvent.
      for (size_t i = 0; i < events.size(); i++) {
        if (filtered_input_event_mask_ & event_class)
          events[i].is_filtered = true;
        else
          rv = true;  // Unfiltered events are assumed to be handled.
        scoped_refptr<PPB_InputEvent_Shared> event_resource(
            new PPB_InputEvent_Shared(::ppapi::OBJECT_IS_IMPL,
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
                                 const gfx::Rect& clip,
                                 const std::vector<gfx::Rect>& cut_outs_rects) {
  // WebKit can give weird (x,y) positions for empty clip rects (since the
  // position technically doesn't matter). But we want to make these
  // consistent since this is given to the plugin, so force everything to 0
  // in the "everything is clipped" case.
  gfx::Rect new_clip;
  if (!clip.IsEmpty())
    new_clip = clip;

  cut_outs_rects_ = cut_outs_rects;

  ViewData previous_view = view_data_;

  view_data_.rect = PP_FromGfxRect(position);
  view_data_.clip_rect = PP_FromGfxRect(clip);
  view_data_.device_scale = container_->deviceScaleFactor();
  view_data_.css_scale = container_->pageZoomFactor() *
                         container_->pageScaleFactor();

  if (desired_fullscreen_state_ || view_data_.is_fullscreen) {
    WebElement element = container_->element();
    WebDocument document = element.document();
    bool is_fullscreen_element = (element == document.fullScreenElement());
    if (!view_data_.is_fullscreen && desired_fullscreen_state_ &&
        delegate()->IsInFullscreenMode() && is_fullscreen_element) {
      // Entered fullscreen. Only possible via SetFullscreen().
      view_data_.is_fullscreen = true;
    } else if (view_data_.is_fullscreen && !is_fullscreen_element) {
      // Exited fullscreen. Possible via SetFullscreen() or F11/link,
      // so desired_fullscreen_state might be out-of-date.
      desired_fullscreen_state_ = false;
      view_data_.is_fullscreen = false;

      // This operation will cause the plugin to re-layout which will send more
      // DidChangeView updates. Schedule an asynchronous update and suppress
      // notifications until that completes to avoid sending intermediate sizes
      // to the plugins.
      ScheduleAsyncDidChangeView(previous_view);

      // Reset the size attributes that we hacked to fill in the screen and
      // retrigger ViewChanged. Make sure we don't forward duplicates of
      // this view to the plugin.
      ResetSizeAttributesAfterFullscreen();
      return;
    }
  }

  UpdateFlashFullscreenState(fullscreen_container_ != NULL);

  SendDidChangeView(previous_view);
}

void PluginInstance::SetWebKitFocus(bool has_focus) {
  if (has_webkit_focus_ == has_focus)
    return;

  bool old_plugin_focus = PluginHasFocus();
  has_webkit_focus_ = has_focus;
  if (PluginHasFocus() != old_plugin_focus)
    SendFocusChangeNotification();
}

void PluginInstance::SetContentAreaFocus(bool has_focus) {
  if (has_content_area_focus_ == has_focus)
    return;

  bool old_plugin_focus = PluginHasFocus();
  has_content_area_focus_ = has_focus;
  if (PluginHasFocus() != old_plugin_focus)
    SendFocusChangeNotification();
}

void PluginInstance::PageVisibilityChanged(bool is_visible) {
  if (is_visible == view_data_.is_page_visible)
    return;  // Nothing to do.
  ViewData old_data = view_data_;
  view_data_.is_page_visible = is_visible;
  SendDidChangeView(old_data);
}

void PluginInstance::ViewWillInitiatePaint() {
  if (GetBoundGraphics2D())
    GetBoundGraphics2D()->ViewWillInitiatePaint();
  else if (GetBoundGraphics3D())
    GetBoundGraphics3D()->ViewWillInitiatePaint();
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
    gfx::Rect* clip,
    float* scale_factor) {
  if (!always_on_top_)
    return false;
  if (!GetBoundGraphics2D() || !GetBoundGraphics2D()->is_always_opaque())
    return false;

  // We specifically want to compare against the area covered by the backing
  // store when seeing if we cover the given paint bounds, since the backing
  // store could be smaller than the declared plugin area.
  PPB_ImageData_Impl* image_data = GetBoundGraphics2D()->image_data();
  // ImageDatas created by NaCl don't have a PlatformImage, so can't be
  // optimized this way.
  if (!image_data->PlatformImage())
    return false;

  gfx::Point plugin_origin = PP_ToGfxPoint(view_data_.rect.point);
  // Convert |paint_bounds| to be relative to the left-top corner of the plugin.
  gfx::Rect relative_paint_bounds(paint_bounds);
  relative_paint_bounds.Offset(-plugin_origin.x(), -plugin_origin.y());

  gfx::Rect plugin_backing_store_rect(
      0, 0, image_data->width(), image_data->height());

  gfx::Rect clip_page = PP_ToGfxRect(view_data_.clip_rect);
  gfx::Rect plugin_paint_rect = plugin_backing_store_rect.Intersect(clip_page);
  if (!plugin_paint_rect.Contains(relative_paint_bounds))
    return false;

  // Don't do optimized painting if the area to paint intersects with the
  // cut-out rects, otherwise we will paint over them.
  for (std::vector<gfx::Rect>::const_iterator iter = cut_outs_rects_.begin();
       iter != cut_outs_rects_.end(); ++iter) {
    if (relative_paint_bounds.Intersects(*iter))
      return false;
  }

  *dib = image_data->PlatformImage()->GetTransportDIB();
  plugin_backing_store_rect.Offset(plugin_origin);
  *location = plugin_backing_store_rect;
  clip_page.Offset(plugin_origin);
  *clip = clip_page;
  *scale_factor = GetBoundGraphics2D()->GetScale();
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

void PluginInstance::RequestSurroundingText(
    size_t desired_number_of_characters) {
  // Keep a reference on the stack. See NOTE above.
  scoped_refptr<PluginInstance> ref(this);
  if (!LoadTextInputInterface())
    return;
  plugin_textinput_interface_->RequestSurroundingText(
      pp_instance(), desired_number_of_characters);
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

bool PluginInstance::LoadContentDecryptorInterface() {
  if (!plugin_decryption_interface_) {
    plugin_decryption_interface_ =
        static_cast<const PPP_ContentDecryptor_Private*>(
          module_->GetPluginInterface(
              PPP_CONTENTDECRYPTOR_PRIVATE_INTERFACE));
  }
  return !!plugin_decryption_interface_;
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
            PPP_MESSAGING_INTERFACE));
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
        static_cast<const PPP_Pdf_1*>(module_->GetPluginInterface(
            PPP_PDF_INTERFACE_1));
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

bool PluginInstance::LoadTextInputInterface() {
  if (!plugin_textinput_interface_) {
    plugin_textinput_interface_ =
        static_cast<const PPP_TextInput_Dev*>(module_->GetPluginInterface(
            PPP_TEXTINPUT_DEV_INTERFACE));
  }

  return !!plugin_textinput_interface_;
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
  return flash_fullscreen_ || (has_webkit_focus_ && has_content_area_focus_);
}

void PluginInstance::SendFocusChangeNotification() {
  bool has_focus = PluginHasFocus();
  delegate()->PluginFocusChanged(this, has_focus);
  instance_interface_->DidChangeFocus(pp_instance(), PP_FromBool(has_focus));
}

bool PluginInstance::IsAcceptingTouchEvents() const {
  return (filtered_input_event_mask_ & PP_INPUTEVENT_CLASS_TOUCH) ||
      (input_event_mask_ & PP_INPUTEVENT_CLASS_TOUCH);
}

void PluginInstance::ScheduleAsyncDidChangeView(
    const ::ppapi::ViewData& previous_view) {
  if (view_change_weak_ptr_factory_.HasWeakPtrs())
    return;  // Already scheduled.
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&PluginInstance::SendAsyncDidChangeView,
                            view_change_weak_ptr_factory_.GetWeakPtr(),
                            previous_view));
}

void PluginInstance::SendAsyncDidChangeView(const ViewData& previous_view) {
  // The bound callback that owns the weak pointer is still valid until after
  // this function returns. SendDidChangeView checks HasWeakPtrs, so we need to
  // invalidate them here.
  // NOTE: If we ever want to have more than one pending callback, it should
  // use a different factory, or we should have a different strategy here.
  view_change_weak_ptr_factory_.InvalidateWeakPtrs();
  SendDidChangeView(previous_view);
}

void PluginInstance::SendDidChangeView(const ViewData& previous_view) {
  // Don't send DidChangeView to crashed plugins.
  if (module()->is_crashed())
    return;

  if (view_change_weak_ptr_factory_.HasWeakPtrs() ||
      (sent_initial_did_change_view_ && previous_view.Equals(view_data_)))
    return;  // Nothing to update.

  const PP_Size& size = view_data_.rect.size;
  // Avoid sending a notification with a huge rectangle.
  if (size.width < 0  || size.width > kMaxPluginSideLength ||
      size.height < 0 || size.height > kMaxPluginSideLength ||
      // We know this won't overflow due to above checks.
      static_cast<uint32>(size.width) * static_cast<uint32>(size.height) >
          kMaxPluginSize) {
    return;
  }

  sent_initial_did_change_view_ = true;
  ScopedPPResource resource(
      ScopedPPResource::PassRef(),
      (new PPB_View_Shared(::ppapi::OBJECT_IS_IMPL,
                           pp_instance(), view_data_))->GetReference());

  instance_interface_->DidChangeView(pp_instance(), resource,
                                     &view_data_.rect,
                                     &view_data_.clip_rect);
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

int PluginInstance::PrintBegin(const WebPrintParams& print_params) {
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
  print_settings.printable_area = PP_FromGfxRect(print_params.printableArea);
  print_settings.content_area = PP_FromGfxRect(print_params.printContentArea);
  print_settings.paper_size = PP_FromGfxSize(print_params.paperSize);
  print_settings.dpi = print_params.printerDPI;
  print_settings.orientation = PP_PRINTORIENTATION_NORMAL;
  print_settings.grayscale = PP_FALSE;
  print_settings.print_scaling_option = static_cast<PP_PrintScalingOption_Dev>(
      print_params.printScalingOption);
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
#if defined(ENABLE_PRINTING)
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
#else  // defined(ENABLED_PRINTING)
  return false;
#endif
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

bool PluginInstance::CanRotateView() {
  if (!LoadPdfInterface())
    return false;

  return true;
}

void PluginInstance::RotateView(WebPlugin::RotationType type) {
  if (!LoadPdfInterface())
    return;
  PP_PrivatePageTransformType transform_type =
      type == WebPlugin::RotationType90Clockwise ?
      PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CW :
      PP_PRIVATEPAGETRANSFORMTYPE_ROTATE_90_CCW;
  plugin_pdf_interface_->Transform(pp_instance(), transform_type);
  // NOTE: plugin instance may have been deleted.
}

void PluginInstance::set_decrypt_client(
    media::DecryptorClient* decryptor_client) {
  DCHECK(decryptor_client);
  decryptor_client_ = decryptor_client;
}

bool PluginInstance::GenerateKeyRequest(const std::string& key_system,
                                        const std::string& init_data) {
  if (!LoadContentDecryptorInterface())
    return false;
  if (key_system.empty())
    return false;

  PP_Var init_data_array =
      PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
          init_data.size(), init_data.data());

  return PP_ToBool(plugin_decryption_interface_->GenerateKeyRequest(
      pp_instance(),
      StringVar::StringToPPVar(key_system),
      init_data_array));
}

bool PluginInstance::AddKey(const std::string& session_id,
                            const std::string& key,
                            const std::string& init_data) {
  if (!LoadContentDecryptorInterface())
    return false;
  PP_Var key_array =
      PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(key.size(),
                                                                 key.data());
  PP_Var init_data_array =
      PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
          init_data.size(),
          init_data.data());

  return PP_ToBool(plugin_decryption_interface_->AddKey(
      pp_instance(),
      StringVar::StringToPPVar(session_id),
      key_array,
      init_data_array));
}

bool PluginInstance::CancelKeyRequest(const std::string& session_id) {
  if (!LoadContentDecryptorInterface())
    return false;

  return PP_ToBool(plugin_decryption_interface_->CancelKeyRequest(
      pp_instance(),
      StringVar::StringToPPVar(session_id)));
}

bool PluginInstance::Decrypt(
    const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
    const media::Decryptor::DecryptCB& decrypt_cb) {
  if (!LoadContentDecryptorInterface())
    return false;

  ScopedPPResource encrypted_resource(
      ScopedPPResource::PassRef(),
      MakeBufferResource(pp_instance(),
                         encrypted_buffer->GetData(),
                         encrypted_buffer->GetDataSize()));
  if (!encrypted_resource.get())
    return false;

  uint32_t request_id = next_decryption_request_id_++;

  PP_EncryptedBlockInfo block_info;
  DCHECK(encrypted_buffer->GetDecryptConfig());
  if (!MakeEncryptedBlockInfo(*encrypted_buffer->GetDecryptConfig(),
                              encrypted_buffer->GetTimestamp().InMicroseconds(),
                              request_id,
                              &block_info)) {
    return false;
  }

  DCHECK(!ContainsKey(pending_decryption_cbs_, request_id));
  pending_decryption_cbs_.insert(std::make_pair(request_id, decrypt_cb));

  return PP_ToBool(plugin_decryption_interface_->Decrypt(pp_instance(),
                                                         encrypted_resource,
                                                         &block_info));
}

bool PluginInstance::DecryptAndDecode(
    const scoped_refptr<media::DecoderBuffer>& encrypted_buffer,
    const media::Decryptor::DecryptCB& decrypt_cb) {
  if (!LoadContentDecryptorInterface())
    return false;

  ScopedPPResource encrypted_resource(MakeBufferResource(
      pp_instance(),
      encrypted_buffer->GetData(),
      encrypted_buffer->GetDataSize()));
  if (!encrypted_resource.get())
    return false;

  PP_EncryptedBlockInfo block_info;

  // TODO(tomfinegan): Store callback and ID in a map, and pass ID to decryptor.
  return PP_ToBool(
      plugin_decryption_interface_->DecryptAndDecode(pp_instance(),
                                                     encrypted_resource,
                                                     &block_info));
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
  if (view_data_.is_fullscreen != desired_fullscreen_state_)
    return false;

  if (fullscreen && !IsProcessingUserGesture())
    return false;

  VLOG(1) << "Setting fullscreen to " << (fullscreen ? "on" : "off");
  desired_fullscreen_state_ = fullscreen;

  if (fullscreen) {
    // Create the user gesture in case we're processing one that's pending.
    WebScopedUserGesture user_gesture;
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
    UpdateFlashFullscreenState(false);
    if (!delay_report) {
      ReportGeometry();
    } else {
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&PluginInstance::ReportGeometry, this));
    }
  }
}

void PluginInstance::UpdateFlashFullscreenState(bool flash_fullscreen) {
  bool is_mouselock_pending = TrackedCallback::IsPending(lock_mouse_callback_);

  if (flash_fullscreen == flash_fullscreen_) {
    // Manually clear callback when fullscreen fails with mouselock pending.
    if (!flash_fullscreen && is_mouselock_pending)
      TrackedCallback::ClearAndRun(&lock_mouse_callback_, PP_ERROR_FAILED);
    return;
  }

  bool old_plugin_focus = PluginHasFocus();
  flash_fullscreen_ = flash_fullscreen;
  if (is_mouselock_pending && !delegate()->IsMouseLocked(this)) {
    if (!delegate()->LockMouse(this))
      TrackedCallback::ClearAndRun(&lock_mouse_callback_, PP_ERROR_FAILED);
  }

  if (PluginHasFocus() != old_plugin_focus)
    SendFocusChangeNotification();
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

bool PluginInstance::IsRectTopmost(const gfx::Rect& rect) {
  if (flash_fullscreen_)
    return true;

#if 0
  WebView* web_view = container()->element().document().frame()->view();
  if (!web_view) {
    NOTREACHED();
    return false;
  }
#else
//FIXME
  return container_->isRectTopmost(rect);
#endif
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
#if defined(ENABLE_PRINTING)
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
    gfx::ScopedCGContextSaveGState save_gstate(cgContext)
    CGContextTranslateCTM(cgContext, 0,
                          current_print_settings_.printable_area.size.height);
    CGContextScaleCTM(cgContext, 1.0, -1.0);
    CGRect page_rect;
    page_rect.origin.x = current_print_settings_.printable_area.point.x;
    page_rect.origin.y = current_print_settings_.printable_area.point.y;
    page_rect.size.width = current_print_settings_.printable_area.size.width;
    page_rect.size.height = current_print_settings_.printable_area.size.height;

    ret = metafile.RenderPage(1, cgContext, page_rect, true, false, true, true);
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
    // We need to scale down DC to fit an entire page into DC available area.
    // First, we'll try to use default scaling based on the 72dpi that is
    // used in webkit for printing.
    // If default scaling is not enough to fit the entire PDF without
    // Current metafile is based on screen DC and have current screen size.
    // Writing outside of those boundaries will result in the cut-off output.
    // On metafiles (this is the case here), scaling down will still record
    // original coordinates and we'll be able to print in full resolution.
    // Before playback we'll need to counter the scaling up that will happen
    // in the browser (printed_document_win.cc).
    double dynamic_scale = gfx::CalculatePageScale(dc, size_in_pixels.width(),
                                                   size_in_pixels.height());
    double page_scale = static_cast<double>(printing::kPointsPerInch) /
        static_cast<double>(current_print_settings_.dpi);

    if (dynamic_scale < page_scale) {
      page_scale = dynamic_scale;
      printing::MetafileSkiaWrapper::SetCustomScaleOnCanvas(*canvas,
                                                            page_scale);
    }

    gfx::ScaleDC(dc, page_scale);

    ret = render_proc(static_cast<unsigned char*>(mapper.data()), mapper.size(),
                      0, dc, current_print_settings_.dpi,
                      current_print_settings_.dpi, 0, 0, size_in_pixels.width(),
                      size_in_pixels.height(), true, false, true, true, true);
    skia::EndPlatformPaint(canvas);
  }
#endif  // defined(OS_WIN)

  return ret;
#else  // defined(ENABLE_PRINTING)
  return false;
#endif
}

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

void PluginInstance::setBackingTextureId(unsigned int id, bool is_opaque) {
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

  if (container_) {
    container_->setBackingTextureId(id);
    container_->setOpaque(is_opaque);
  }
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

bool PluginInstance::IsProcessingUserGesture() {
  PP_TimeTicks now =
      ::ppapi::TimeTicksToPPTimeTicks(base::TimeTicks::Now());
  // Give a lot of slack so tests won't be flaky. Well behaved plugins will
  // close the user gesture.
  const PP_TimeTicks kUserGestureDurationInSeconds = 10.0;
  return (now - pending_user_gesture_ < kUserGestureDurationInSeconds);
}

void PluginInstance::OnLockMouseACK(bool succeeded) {
  if (TrackedCallback::IsPending(lock_mouse_callback_)) {
    TrackedCallback::ClearAndRun(&lock_mouse_callback_,
                                 succeeded ? PP_OK : PP_ERROR_FAILED);
  }
}

void PluginInstance::OnMouseLockLost() {
  if (LoadMouseLockInterface())
    plugin_mouse_lock_interface_->MouseLockLost(pp_instance());
}

void PluginInstance::HandleMouseLockedInputEvent(
    const WebKit::WebMouseEvent& event) {
  // |cursor_info| is ignored since it is hidden when the mouse is locked.
  WebKit::WebCursorInfo cursor_info;
  HandleInputEvent(event, &cursor_info);
}

void PluginInstance::SimulateInputEvent(const InputEventData& input_event) {
  WebView* web_view = container()->element().document().frame()->view();
  if (!web_view) {
    NOTREACHED();
    return;
  }

  bool handled = SimulateIMEEvent(input_event);
  if (handled)
    return;

  std::vector<linked_ptr<WebInputEvent> > events =
      CreateSimulatedWebInputEvents(
          input_event,
          view_data_.rect.point.x + view_data_.rect.size.width / 2,
          view_data_.rect.point.y + view_data_.rect.size.height / 2);
  for (std::vector<linked_ptr<WebInputEvent> >::iterator it = events.begin();
      it != events.end(); ++it) {
    web_view->handleInputEvent(*it->get());
  }
}

bool PluginInstance::SimulateIMEEvent(const InputEventData& input_event) {
  switch (input_event.event_type) {
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
      SimulateImeSetCompositionEvent(input_event);
      break;
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
      DCHECK(input_event.character_text.empty());
      SimulateImeSetCompositionEvent(input_event);
      break;
    case PP_INPUTEVENT_TYPE_IME_TEXT:
      delegate()->SimulateImeConfirmComposition(
          UTF8ToUTF16(input_event.character_text));
      break;
    default:
      return false;
  }
  return true;
}

void PluginInstance::SimulateImeSetCompositionEvent(
    const InputEventData& input_event) {
  std::vector<size_t> offsets;
  offsets.push_back(input_event.composition_selection_start);
  offsets.push_back(input_event.composition_selection_end);
  offsets.insert(offsets.end(),
                 input_event.composition_segment_offsets.begin(),
                 input_event.composition_segment_offsets.end());

  string16 utf16_text =
      UTF8ToUTF16AndAdjustOffsets(input_event.character_text, &offsets);

  std::vector<WebKit::WebCompositionUnderline> underlines;
  for (size_t i = 2; i + 1 < offsets.size(); ++i) {
    WebKit::WebCompositionUnderline underline;
    underline.startOffset = offsets[i];
    underline.endOffset = offsets[i + 1];
    if (input_event.composition_target_segment == static_cast<int32_t>(i - 2))
      underline.thick = true;
    underlines.push_back(underline);
  }

  delegate()->SimulateImeSetComposition(
      utf16_text, underlines, offsets[0], offsets[1]);
}

void PluginInstance::ClosePendingUserGesture(PP_Instance instance,
                                             PP_TimeTicks timestamp) {
  // Do nothing so that the pending user gesture will stay open for
  // kUserGestureDurationInSeconds.
  // TODO(yzshen): remove the code for closing pending user gesture.
}

PP_Bool PluginInstance::BindGraphics(PP_Instance instance,
                                     PP_Resource device) {
  // The Graphics3D instance can't be destroyed until we call
  // setBackingTextureId.
  scoped_refptr< ::ppapi::Resource> old_graphics = bound_graphics_;
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
    setBackingTextureId(0, false);
    InvalidateRect(gfx::Rect());
    return PP_TRUE;
  }

  // Refuse to bind if in transition to fullscreen with PPB_FlashFullscreen or
  // to/from fullscreen with PPB_Fullscreen.
  if ((fullscreen_container_ && !flash_fullscreen_) ||
      desired_fullscreen_state_ != view_data_.is_fullscreen)
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
    setBackingTextureId(0, graphics_2d->is_always_opaque());
    // BindToInstance will have invalidated the plugin if necessary.
  } else if (graphics_3d) {
    // Make sure graphics can only be bound to the instance it is
    // associated with.
    if (graphics_3d->pp_instance() != pp_instance())
      return PP_FALSE;
    if (!graphics_3d->BindToInstance(true))
      return PP_FALSE;

    bound_graphics_ = graphics_3d;
    setBackingTextureId(graphics_3d->GetBackingTextureId(),
                        graphics_3d->IsOpaque());
  } else {
    // The device is not a valid resource type.
    return PP_FALSE;
  }

  return PP_TRUE;
}

PP_Bool PluginInstance::IsFullFrame(PP_Instance instance) {
  return PP_FromBool(full_frame());
}

const ViewData* PluginInstance::GetViewData(PP_Instance instance) {
  return &view_data_;
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
  TryCatch try_catch(exception);
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
  bool ok = false;
  if (IsProcessingUserGesture()) {
    WebKit::WebScopedUserGesture user_gesture;
    ok = WebBindings::evaluate(NULL, frame->windowObject(), &np_script,
                               &result);
  } else {
    ok = WebBindings::evaluate(NULL, frame->windowObject(), &np_script,
                               &result);
  }
  if (!ok) {
    // TryCatch doesn't catch the exceptions properly. Since this is only for
    // a trusted API, just set to a general exception message.
    try_catch.SetException("Exception caught");
    WebBindings::releaseVariantValue(&result);
    return PP_MakeUndefined();
  }

  PP_Var ret = NPVariantToPPVar(this, &result);
  WebBindings::releaseVariantValue(&result);
  return ret;
}

uint32_t PluginInstance::GetAudioHardwareOutputSampleRate(
    PP_Instance instance) {
  return delegate()->GetAudioHardwareOutputSampleRate();
}

uint32_t PluginInstance::GetAudioHardwareOutputBufferSize(
    PP_Instance instance) {
  return delegate()->GetAudioHardwareOutputBufferSize();
}

PP_Var PluginInstance::GetDefaultCharSet(PP_Instance instance) {
  std::string encoding = delegate()->GetDefaultEncoding();
  return StringVar::StringToPPVar(encoding);
}

PP_Var PluginInstance::GetFontFamilies(PP_Instance instance) {
  // No in-process implementation.
  return PP_MakeUndefined();
}

void PluginInstance::NeedKey(PP_Instance instance,
                             PP_Var key_system_var,
                             PP_Var session_id_var,
                             PP_Var init_data_var) {
  // TODO(tomfinegan): send the data to media stack.
}

void PluginInstance::KeyAdded(PP_Instance instance,
                              PP_Var key_system_var,
                              PP_Var session_id_var) {
  StringVar* key_system_string = StringVar::FromPPVar(key_system_var);
  StringVar* session_id_string = StringVar::FromPPVar(session_id_var);
  if (!key_system_string || !session_id_string) {
    decryptor_client_->KeyError("", "", media::Decryptor::kUnknownError, 0);
    return;
  }

  DCHECK(decryptor_client_);
  decryptor_client_->KeyAdded(key_system_string->value(),
                              session_id_string->value());
}

void PluginInstance::KeyMessage(PP_Instance instance,
                                PP_Var key_system_var,
                                PP_Var session_id_var,
                                PP_Resource message_resource,
                                PP_Var default_url_var) {
  StringVar* key_system_string = StringVar::FromPPVar(key_system_var);
  StringVar* session_id_string = StringVar::FromPPVar(session_id_var);
  StringVar* default_url_string = StringVar::FromPPVar(default_url_var);

  if (!key_system_string || !session_id_string || !default_url_string) {
    decryptor_client_->KeyError("", "", media::Decryptor::kUnknownError, 0);
    return;
  }

  EnterResourceNoLock<PPB_Buffer_API> enter(message_resource, true);
  if (!enter.succeeded()) {
    decryptor_client_->KeyError(key_system_string->value(),
                                session_id_string->value(),
                                media::Decryptor::kUnknownError,
                                0);
    return;
  }

  BufferAutoMapper mapper(enter.object());
  scoped_array<uint8> message_array(new uint8[mapper.size()]);
  if (mapper.data() && mapper.size())
    memcpy(message_array.get(), mapper.data(), mapper.size());

  DCHECK(decryptor_client_);
  decryptor_client_->KeyMessage(key_system_string->value(),
                                session_id_string->value(),
                                message_array.Pass(),
                                mapper.size(),
                                default_url_string->value());
}

void PluginInstance::KeyError(PP_Instance instance,
                              PP_Var key_system_var,
                              PP_Var session_id_var,
                              int32_t media_error,
                              int32_t system_code) {
  StringVar* key_system_string = StringVar::FromPPVar(key_system_var);
  StringVar* session_id_string = StringVar::FromPPVar(session_id_var);
  if (!key_system_string || !session_id_string) {
    decryptor_client_->KeyError("", "", media::Decryptor::kUnknownError, 0);
    return;
  }

  DCHECK(decryptor_client_);
  decryptor_client_->KeyError(
      key_system_string->value(),
      session_id_string->value(),
      static_cast<media::Decryptor::KeyError>(media_error),
      system_code);
}

void PluginInstance::DeliverBlock(PP_Instance instance,
                                  PP_Resource decrypted_block,
                                  const PP_DecryptedBlockInfo* block_info) {
  DCHECK(block_info);
  DecryptionCBMap::iterator found = pending_decryption_cbs_.find(
      block_info->tracking_info.request_id);
  if (found == pending_decryption_cbs_.end())
    return;
  media::Decryptor::DecryptCB decrypt_cb = found->second;
  pending_decryption_cbs_.erase(found);

  if (block_info->result == PP_DECRYPTRESULT_DECRYPT_NOKEY) {
    decrypt_cb.Run(media::Decryptor::kNoKey, NULL);
    return;
  }
  if (block_info->result != PP_DECRYPTRESULT_SUCCESS) {
    decrypt_cb.Run(media::Decryptor::kError, NULL);
    return;
  }

  EnterResourceNoLock<PPB_Buffer_API> enter(decrypted_block, true);
  if (!enter.succeeded()) {
    decrypt_cb.Run(media::Decryptor::kError, NULL);
    return;
  }
  BufferAutoMapper mapper(enter.object());
  if (!mapper.data() || !mapper.size()) {
    decrypt_cb.Run(media::Decryptor::kError, NULL);
    return;
  }

  // TODO(tomfinegan): Find a way to take ownership of the shared memory
  // managed by the PPB_Buffer_Dev, and avoid the extra copy.
  scoped_refptr<media::DecoderBuffer> decrypted_buffer(
      media::DecoderBuffer::CopyFrom(
          reinterpret_cast<const uint8*>(mapper.data()), mapper.size()));
  decrypted_buffer->SetTimestamp(base::TimeDelta::FromMicroseconds(
      block_info->tracking_info.timestamp));
  decrypt_cb.Run(media::Decryptor::kSuccess, decrypted_buffer);
}

void PluginInstance::DeliverFrame(PP_Instance instance,
                                  PP_Resource decrypted_frame,
                                  const PP_DecryptedBlockInfo* block_info) {
  // TODO(tomfinegan): To be implemented after completion of v0.1 of the
  // EME/CDM work.
}

void PluginInstance::DeliverSamples(PP_Instance instance,
                                    PP_Resource decrypted_samples,
                                    const PP_DecryptedBlockInfo* block_info) {
  // TODO(tomfinegan): To be implemented after completion of v0.1 of the
  // EME/CDM work.
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

PP_Bool PluginInstance::SetFullscreen(PP_Instance instance,
                                      PP_Bool fullscreen) {
  return PP_FromBool(SetFullscreen(PP_ToBool(fullscreen)));
}

PP_Bool PluginInstance::GetScreenSize(PP_Instance instance, PP_Size* size) {
  gfx::Size screen_size = delegate()->GetScreenSize();
  *size = PP_MakeSize(screen_size.width(), screen_size.height());
  return PP_TRUE;
}

::ppapi::thunk::PPB_Flash_API* PluginInstance::GetFlashAPI() {
  return &flash_impl_;
}

::ppapi::thunk::PPB_Gamepad_API* PluginInstance::GetGamepadAPI(
    PP_Instance /* instance */) {
  return &gamepad_impl_;
}

int32_t PluginInstance::RequestInputEvents(PP_Instance instance,
                                           uint32_t event_classes) {
  input_event_mask_ |= event_classes;
  filtered_input_event_mask_ &= ~(event_classes);
  if (event_classes & PP_INPUTEVENT_CLASS_TOUCH)
    container_->setIsAcceptingTouchEvents(IsAcceptingTouchEvents());
  return ValidateRequestInputEvents(false, event_classes);
}

int32_t PluginInstance::RequestFilteringInputEvents(PP_Instance instance,
                                                    uint32_t event_classes) {
  filtered_input_event_mask_ |= event_classes;
  input_event_mask_ &= ~(event_classes);
  if (event_classes & PP_INPUTEVENT_CLASS_TOUCH)
    container_->setIsAcceptingTouchEvents(IsAcceptingTouchEvents());
  return ValidateRequestInputEvents(true, event_classes);
}

void PluginInstance::ClearInputEventRequest(PP_Instance instance,
                                            uint32_t event_classes) {
  input_event_mask_ &= ~(event_classes);
  filtered_input_event_mask_ &= ~(event_classes);
  if (event_classes & PP_INPUTEVENT_CLASS_TOUCH)
    container_->setIsAcceptingTouchEvents(IsAcceptingTouchEvents());
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

PP_Bool PluginInstance::SetCursor(PP_Instance instance,
                                  PP_MouseCursor_Type type,
                                  PP_Resource image,
                                  const PP_Point* hot_spot) {
  if (!ValidateSetCursorParams(type, image, hot_spot))
    return PP_FALSE;

  if (type != PP_MOUSECURSOR_TYPE_CUSTOM) {
    DoSetCursor(new WebCursorInfo(static_cast<WebCursorInfo::Type>(type)));
    return PP_TRUE;
  }

  EnterResourceNoLock<PPB_ImageData_API> enter(image, true);
  if (enter.failed())
    return PP_FALSE;
  PPB_ImageData_Impl* image_data =
      static_cast<PPB_ImageData_Impl*>(enter.object());

  ImageDataAutoMapper auto_mapper(image_data);
  if (!auto_mapper.is_valid())
    return PP_FALSE;

  scoped_ptr<WebCursorInfo> custom_cursor(
      new WebCursorInfo(WebCursorInfo::TypeCustom));
  custom_cursor->hotSpot.x = hot_spot->x;
  custom_cursor->hotSpot.y = hot_spot->y;

  const SkBitmap* bitmap = image_data->GetMappedBitmap();
  // Make a deep copy, so that the cursor remains valid even after the original
  // image data gets freed.
  if (!bitmap->copyTo(&custom_cursor->customImage.getSkBitmap(),
                      bitmap->config())) {
    return PP_FALSE;
  }

  DoSetCursor(custom_cursor.release());
  return PP_TRUE;
}

int32_t PluginInstance::LockMouse(PP_Instance instance,
                                  scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(lock_mouse_callback_))
    return PP_ERROR_INPROGRESS;

  if (delegate()->IsMouseLocked(this))
    return PP_OK;

  if (!CanAccessMainFrame())
    return PP_ERROR_NOACCESS;

  // Attempt mouselock only if Flash isn't waiting on fullscreen, otherwise
  // we wait and call LockMouse() in UpdateFlashFullscreenState().
  if (!FlashIsFullscreenOrPending() || flash_fullscreen()) {
    if (!delegate()->LockMouse(this))
      return PP_ERROR_FAILED;
  }

  // Either mouselock succeeded or a Flash fullscreen is pending.
  lock_mouse_callback_ = callback;
  return PP_OK_COMPLETIONPENDING;
}

void PluginInstance::UnlockMouse(PP_Instance instance) {
  delegate()->UnlockMouse(this);
}

void PluginInstance::SetTextInputType(PP_Instance instance,
                                      PP_TextInput_Type type) {
  int itype = type;
  if (itype < 0 || itype > ui::TEXT_INPUT_TYPE_URL)
    itype = ui::TEXT_INPUT_TYPE_NONE;
  text_input_type_ = static_cast<ui::TextInputType>(itype);
  delegate()->PluginTextInputTypeChanged(this);
}

void PluginInstance::UpdateCaretPosition(PP_Instance instance,
                                         const PP_Rect& caret,
                                         const PP_Rect& bounding_box) {
  text_input_caret_ = PP_ToGfxRect(caret);
  text_input_caret_bounds_ = PP_ToGfxRect(bounding_box);
  text_input_caret_set_ = true;
  delegate()->PluginCaretPositionChanged(this);
}

void PluginInstance::CancelCompositionText(PP_Instance instance) {
  delegate()->PluginRequestedCancelComposition(this);
}

void PluginInstance::SelectionChanged(PP_Instance instance) {
  // TODO(kinaba): currently the browser always calls RequestSurroundingText.
  // It can be optimized so that it won't call it back until the information
  // is really needed.

  // Avoid calling in nested context or else this will reenter the plugin. This
  // uses a weak pointer rather than exploiting the fact that this class is
  // refcounted because we don't actually want this operation to affect the
  // lifetime of the instance.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&PluginInstance::RequestSurroundingText,
                 AsWeakPtr(),
                 static_cast<size_t>(kExtraCharsForTextInput)));
}

void PluginInstance::UpdateSurroundingText(PP_Instance instance,
                                           const char* text,
                                           uint32_t caret,
                                           uint32_t anchor) {
  surrounding_text_ = text;
  selection_caret_ = caret;
  selection_anchor_ = anchor;
  delegate()->PluginSelectionChanged(this);
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
  return ::ppapi::PPB_URLUtil_Shared::GenerateURLReturn(
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
  return ::ppapi::PPB_URLUtil_Shared::GenerateURLReturn(document.url(),
                                                        components);
}

PP_Var PluginInstance::GetPluginInstanceURL(
    PP_Instance instance,
    PP_URLComponents_Dev* components) {
  return ::ppapi::PPB_URLUtil_Shared::GenerateURLReturn(plugin_url_,
                                                        components);
}

bool PluginInstance::ResetAsProxied(scoped_refptr<PluginModule> module) {
  // Save the original module and switch over to the new one now that this
  // plugin is using the IPC-based proxy.
  original_module_ = module_;
  module_ = module;

  // For NaCl instances, remember the NaCl plugin instance interface, so we
  // can shut it down by calling its DidDestroy in our Delete() method.
  original_instance_interface_.reset(instance_interface_.release());

  base::Callback<const void*(const char*)> get_plugin_interface_func =
      base::Bind(&PluginModule::GetPluginInterface, module_.get());
  PPP_Instance_Combined* ppp_instance_combined =
      PPP_Instance_Combined::Create(get_plugin_interface_func);
  if (!ppp_instance_combined) {
    // The proxy must support at least one usable PPP_Instance interface.
    NOTREACHED();
    return false;
  }
  instance_interface_.reset(ppp_instance_combined);
  // Clear all PPP interfaces we may have cached.
  plugin_find_interface_ = NULL;
  plugin_input_event_interface_ = NULL;
  plugin_messaging_interface_ = NULL;
  plugin_mouse_lock_interface_ = NULL;
  plugin_pdf_interface_ = NULL;
  plugin_private_interface_ = NULL;
  plugin_selection_interface_ = NULL;
  plugin_textinput_interface_ = NULL;
  plugin_zoom_interface_ = NULL;

  // Re-send the DidCreate event via the proxy.
  scoped_array<const char*> argn_array(StringVectorToArgArray(argn_));
  scoped_array<const char*> argv_array(StringVectorToArgArray(argv_));
  if (!instance_interface_->DidCreate(pp_instance(), argn_.size(),
                                      argn_array.get(), argv_array.get()))
    return false;

  // Use a ViewData that looks like the initial DidChangeView event for the
  // "previous" view.
  ::ppapi::ViewData empty_view;
  empty_view.is_page_visible = delegate_->IsPageVisible();
  // Clear sent_initial_did_change_view_ and cancel any pending DidChangeView
  // event. This way, SendDidChangeView will send the "current" view
  // immediately (before other events like HandleDocumentLoad).
  sent_initial_did_change_view_ = false;
  view_change_weak_ptr_factory_.InvalidateWeakPtrs();
  SendDidChangeView(empty_view);

  // If we received HandleDocumentLoad, re-send it now via the proxy.
  if (document_loader_)
    HandleDocumentLoad(document_loader_.get());
  return true;
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
