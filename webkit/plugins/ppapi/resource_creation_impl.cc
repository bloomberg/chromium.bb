// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/resource_creation_impl.h"

#include "ppapi/c/pp_size.h"
#include "ppapi/shared_impl/audio_config_impl.h"
#include "ppapi/shared_impl/input_event_impl.h"
#include "ppapi/shared_impl/var.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_audio_impl.h"
#include "webkit/plugins/ppapi/ppb_audio_input_impl.h"
#include "webkit/plugins/ppapi/ppb_broker_impl.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"
#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"
#include "webkit/plugins/ppapi/ppb_file_chooser_impl.h"
#include "webkit/plugins/ppapi/ppb_file_io_impl.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_menu_impl.h"
#include "webkit/plugins/ppapi/ppb_flash_net_connector_impl.h"
#include "webkit/plugins/ppapi/ppb_font_impl.h"
#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"
#include "webkit/plugins/ppapi/ppb_graphics_3d_impl.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/ppb_scrollbar_impl.h"
#include "webkit/plugins/ppapi/ppb_transport_impl.h"
#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/ppb_video_capture_impl.h"
#include "webkit/plugins/ppapi/ppb_video_decoder_impl.h"
#include "webkit/plugins/ppapi/ppb_video_layer_impl.h"
#include "webkit/plugins/ppapi/ppb_websocket_impl.h"

using ppapi::InputEventData;
using ppapi::InputEventImpl;
using ppapi::StringVar;

namespace webkit {
namespace ppapi {

ResourceCreationImpl::ResourceCreationImpl(PluginInstance* instance) {
}

ResourceCreationImpl::~ResourceCreationImpl() {
}

::ppapi::thunk::ResourceCreationAPI*
ResourceCreationImpl::AsResourceCreationAPI() {
  return this;
}

PP_Resource ResourceCreationImpl::CreateAudio(
    PP_Instance instance,
    PP_Resource config_id,
    PPB_Audio_Callback audio_callback,
    void* user_data) {
  return PPB_Audio_Impl::Create(instance, config_id, audio_callback,
                                user_data);
}

PP_Resource ResourceCreationImpl::CreateAudioConfig(
    PP_Instance instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  return ::ppapi::AudioConfigImpl::CreateAsImpl(instance, sample_rate,
                                                sample_frame_count);
}

PP_Resource ResourceCreationImpl::CreateAudioTrusted(
    PP_Instance instance) {
  return (new PPB_Audio_Impl(instance))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateAudioInput(
    PP_Instance instance,
    PP_Resource config_id,
    PPB_AudioInput_Callback audio_input_callback,
    void* user_data) {
  return PPB_AudioInput_Impl::Create(instance, config_id,
      audio_input_callback, user_data);
}

PP_Resource ResourceCreationImpl::CreateAudioInputTrusted(
    PP_Instance instance) {
  return (new PPB_AudioInput_Impl(instance))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateBroker(PP_Instance instance) {
  return (new PPB_Broker_Impl(instance))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateBuffer(PP_Instance instance,
                                               uint32_t size) {
  return PPB_Buffer_Impl::Create(instance, size);
}

PP_Resource ResourceCreationImpl::CreateDirectoryReader(
    PP_Resource directory_ref) {
  return PPB_DirectoryReader_Impl::Create(directory_ref);
}

PP_Resource ResourceCreationImpl::CreateFileChooser(
    PP_Instance instance,
    PP_FileChooserMode_Dev mode,
    const char* accept_mime_types) {
  return PPB_FileChooser_Impl::Create(instance, mode, accept_mime_types);
}

PP_Resource ResourceCreationImpl::CreateFileIO(PP_Instance instance) {
  return (new PPB_FileIO_Impl(instance))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateFileRef(PP_Resource file_system,
                                                const char* path) {
  PPB_FileRef_Impl* res = PPB_FileRef_Impl::CreateInternal(file_system, path);
  return res ? res->GetReference() : 0;
}

PP_Resource ResourceCreationImpl::CreateFileSystem(
    PP_Instance instance,
    PP_FileSystemType type) {
  return PPB_FileSystem_Impl::Create(instance, type);
}

PP_Resource ResourceCreationImpl::CreateFlashMenu(
    PP_Instance instance,
    const PP_Flash_Menu* menu_data) {
  return PPB_Flash_Menu_Impl::Create(instance, menu_data);
}

PP_Resource ResourceCreationImpl::CreateFlashNetConnector(
    PP_Instance instance) {
  return (new PPB_Flash_NetConnector_Impl(instance))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateFontObject(
    PP_Instance instance,
    const PP_FontDescription_Dev* description) {
  return PPB_Font_Impl::Create(instance, *description);
}

PP_Resource ResourceCreationImpl::CreateGraphics2D(
    PP_Instance instance,
    const PP_Size& size,
    PP_Bool is_always_opaque) {
  return PPB_Graphics2D_Impl::Create(instance, size, is_always_opaque);
}

PP_Resource ResourceCreationImpl::CreateGraphics3D(
    PP_Instance instance,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  return PPB_Graphics3D_Impl::Create(instance, share_context, attrib_list);
}

PP_Resource ResourceCreationImpl::CreateGraphics3DRaw(
    PP_Instance instance,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  return PPB_Graphics3D_Impl::CreateRaw(instance, share_context, attrib_list);
}

PP_Resource ResourceCreationImpl::CreateImageData(PP_Instance instance,
                                                  PP_ImageDataFormat format,
                                                  const PP_Size& size,
                                                  PP_Bool init_to_zero) {
  return PPB_ImageData_Impl::Create(instance, format, size, init_to_zero);
}

PP_Resource ResourceCreationImpl::CreateKeyboardInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    uint32_t key_code,
    struct PP_Var character_text) {
  if (type != PP_INPUTEVENT_TYPE_RAWKEYDOWN &&
      type != PP_INPUTEVENT_TYPE_KEYDOWN &&
      type != PP_INPUTEVENT_TYPE_KEYUP &&
      type != PP_INPUTEVENT_TYPE_CHAR)
    return 0;

  InputEventData data;
  data.event_type = type;
  data.event_time_stamp = time_stamp;
  data.event_modifiers = modifiers;
  data.key_code = key_code;
  if (character_text.type == PP_VARTYPE_STRING) {
    StringVar* string_var = StringVar::FromPPVar(character_text);
    if (!string_var)
      return 0;
    data.character_text = string_var->value();
  }

  return (new InputEventImpl(InputEventImpl::InitAsImpl(),
                             instance, data))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateMouseInputEvent(
    PP_Instance instance,
    PP_InputEvent_Type type,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    PP_InputEvent_MouseButton mouse_button,
    const PP_Point* mouse_position,
    int32_t click_count,
    const PP_Point* mouse_movement) {
  if (type != PP_INPUTEVENT_TYPE_MOUSEDOWN &&
      type != PP_INPUTEVENT_TYPE_MOUSEUP &&
      type != PP_INPUTEVENT_TYPE_MOUSEMOVE &&
      type != PP_INPUTEVENT_TYPE_MOUSEENTER &&
      type != PP_INPUTEVENT_TYPE_MOUSELEAVE)
    return 0;

  InputEventData data;
  data.event_type = type;
  data.event_time_stamp = time_stamp;
  data.event_modifiers = modifiers;
  data.mouse_button = mouse_button;
  data.mouse_position = *mouse_position;
  data.mouse_click_count = click_count;
  data.mouse_movement = *mouse_movement;

  return (new InputEventImpl(InputEventImpl::InitAsImpl(),
                             instance, data))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateScrollbar(PP_Instance instance,
                                                  PP_Bool vertical) {
  return PPB_Scrollbar_Impl::Create(instance, PP_ToBool(vertical));
}

PP_Resource ResourceCreationImpl::CreateTCPSocketPrivate(PP_Instance instance) {
  // Creating TCP socket resource at the renderer side is not supported.
  return 0;
}

PP_Resource ResourceCreationImpl::CreateTransport(PP_Instance instance,
                                                  const char* name,
                                                  PP_TransportType type) {
#if defined(ENABLE_P2P_APIS)
  return PPB_Transport_Impl::Create(instance, name, type);
#endif
}

PP_Resource ResourceCreationImpl::CreateUDPSocketPrivate(PP_Instance instance) {
  // Creating UDP socket resource at the renderer side is not supported.
  return 0;
}

PP_Resource ResourceCreationImpl::CreateURLLoader(PP_Instance instance) {
  return (new PPB_URLLoader_Impl(instance, false))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateURLRequestInfo(
    PP_Instance instance,
    const ::ppapi::PPB_URLRequestInfo_Data& data) {
  return (new PPB_URLRequestInfo_Impl(instance, data))->GetReference();
}

PP_Resource ResourceCreationImpl::CreateVideoCapture(PP_Instance instance) {
  scoped_refptr<PPB_VideoCapture_Impl> video_capture =
      new PPB_VideoCapture_Impl(instance);
  if (!video_capture->Init())
    return 0;
  return video_capture->GetReference();
}

PP_Resource ResourceCreationImpl::CreateVideoDecoder(
    PP_Instance instance,
    PP_Resource graphics3d_id,
    PP_VideoDecoder_Profile profile) {
  return PPB_VideoDecoder_Impl::Create(instance, graphics3d_id, profile);
}

PP_Resource ResourceCreationImpl::CreateVideoLayer(PP_Instance instance,
                                                   PP_VideoLayerMode_Dev mode) {
  return PPB_VideoLayer_Impl::Create(instance, mode);
}

PP_Resource ResourceCreationImpl::CreateWebSocket(PP_Instance instance) {
  return PPB_WebSocket_Impl::Create(instance);
}

PP_Resource ResourceCreationImpl::CreateWheelInputEvent(
    PP_Instance instance,
    PP_TimeTicks time_stamp,
    uint32_t modifiers,
    const PP_FloatPoint* wheel_delta,
    const PP_FloatPoint* wheel_ticks,
    PP_Bool scroll_by_page) {
  InputEventData data;
  data.event_type = PP_INPUTEVENT_TYPE_WHEEL;
  data.event_time_stamp = time_stamp;
  data.event_modifiers = modifiers;
  data.wheel_delta = *wheel_delta;
  data.wheel_ticks = *wheel_ticks;
  data.wheel_scroll_by_page = PP_ToBool(scroll_by_page);

  return (new InputEventImpl(InputEventImpl::InitAsImpl(),
                             instance, data))->GetReference();
}

}  // namespace ppapi
}  // namespace webkit
