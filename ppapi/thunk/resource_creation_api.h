// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_RESOURCE_CREATION_API_H_
#define PPAPI_THUNK_RESOURCE_CREATION_API_H_

#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/dev/ppb_video_layer_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/dev/pp_video_dev.h"
#include "ppapi/c/dev/ppb_transport_dev.h"
#include "ppapi/c/dev/ppb_websocket_dev.h"
#include "ppapi/shared_impl/api_id.h"

struct PP_Flash_Menu;
struct PP_FontDescription_Dev;
struct PP_Size;

namespace ppapi {

struct PPB_URLRequestInfo_Data;

namespace thunk {

// A functional API for creating resource types. Separating out the creation
// functions here allows us to implement most resources as a pure "resource
// API", meaning all calls are routed on a per-resource-object basis. The
// creation functions are not per-object (since there's no object during
// creation) so need functional routing based on the instance ID.
class ResourceCreationAPI {
 public:
  virtual ~ResourceCreationAPI() {}

  virtual PP_Resource CreateAudio(PP_Instance instance,
                                  PP_Resource config_id,
                                  PPB_Audio_Callback audio_callback,
                                  void* user_data) = 0;
  virtual PP_Resource CreateAudioTrusted(PP_Instance instance) = 0;
  virtual PP_Resource CreateAudioConfig(PP_Instance instance,
                                        PP_AudioSampleRate sample_rate,
                                        uint32_t sample_frame_count) = 0;
  virtual PP_Resource CreateAudioInput(
      PP_Instance instance,
      PP_Resource config_id,
      PPB_AudioInput_Callback audio_input_callback,
      void* user_data) = 0;
  virtual PP_Resource CreateAudioInputTrusted(PP_Instance instance) = 0;
  virtual PP_Resource CreateBroker(PP_Instance instance) = 0;
  virtual PP_Resource CreateBuffer(PP_Instance instance, uint32_t size) = 0;
  virtual PP_Resource CreateDirectoryReader(PP_Resource directory_ref) = 0;
  virtual PP_Resource CreateFileChooser(
      PP_Instance instance,
      PP_FileChooserMode_Dev mode,
      const char* accept_mime_types) = 0;
  virtual PP_Resource CreateFileIO(PP_Instance instance) = 0;
  virtual PP_Resource CreateFileRef(PP_Resource file_system,
                                    const char* path) = 0;
  virtual PP_Resource CreateFileSystem(PP_Instance instance,
                                       PP_FileSystemType type) = 0;
  virtual PP_Resource CreateFlashMenu(PP_Instance instance,
                                      const PP_Flash_Menu* menu_data) = 0;
  virtual PP_Resource CreateFlashNetConnector(PP_Instance instance) = 0;
  // Note: can't be called CreateFont due to Windows #defines.
  virtual PP_Resource CreateFontObject(
      PP_Instance instance,
      const PP_FontDescription_Dev* description) = 0;
  virtual PP_Resource CreateGraphics2D(PP_Instance instance,
                                       const PP_Size& size,
                                       PP_Bool is_always_opaque) = 0;
  virtual PP_Resource CreateGraphics3D(PP_Instance instance,
                                       PP_Resource share_context,
                                       const int32_t* attrib_list) = 0;
  virtual PP_Resource CreateGraphics3DRaw(PP_Instance instance,
                                          PP_Resource share_context,
                                          const int32_t* attrib_list) = 0;
  virtual PP_Resource CreateImageData(PP_Instance instance,
                                      PP_ImageDataFormat format,
                                      const PP_Size& size,
                                      PP_Bool init_to_zero) = 0;
  virtual PP_Resource CreateKeyboardInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      uint32_t key_code,
      struct PP_Var character_text) = 0;
  virtual PP_Resource CreateMouseInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      PP_InputEvent_MouseButton mouse_button,
      const PP_Point* mouse_position,
      int32_t click_count,
      const PP_Point* mouse_movement) = 0;
  virtual PP_Resource CreateScrollbar(PP_Instance instance,
                                      PP_Bool vertical) = 0;
  virtual PP_Resource CreateTCPSocketPrivate(PP_Instance instace) = 0;
  virtual PP_Resource CreateTransport(PP_Instance instance,
                                      const char* name,
                                      PP_TransportType type) = 0;
  virtual PP_Resource CreateUDPSocketPrivate(PP_Instance instace) = 0;
  virtual PP_Resource CreateURLLoader(PP_Instance instance) = 0;
  virtual PP_Resource CreateURLRequestInfo(
      PP_Instance instance,
      const PPB_URLRequestInfo_Data& data) = 0;
  virtual PP_Resource CreateVideoCapture(PP_Instance instance) = 0;
  virtual PP_Resource CreateVideoDecoder(
      PP_Instance instance,
      PP_Resource context3d_id,
      PP_VideoDecoder_Profile profile) = 0;
  virtual PP_Resource CreateVideoLayer(PP_Instance instance,
                                       PP_VideoLayerMode_Dev mode) = 0;
  virtual PP_Resource CreateWebSocket(PP_Instance instance) = 0;
  virtual PP_Resource CreateWheelInputEvent(
      PP_Instance instance,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      const PP_FloatPoint* wheel_delta,
      const PP_FloatPoint* wheel_ticks,
      PP_Bool scroll_by_page) = 0;

  static const ApiID kApiID = API_ID_RESOURCE_CREATION;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_RESOURCE_CREATION_API_H_
