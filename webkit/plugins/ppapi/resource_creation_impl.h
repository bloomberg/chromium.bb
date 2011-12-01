// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_RESOURCE_CREATION_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_RESOURCE_CREATION_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class ResourceCreationImpl : public ::ppapi::FunctionGroupBase,
                             public ::ppapi::thunk::ResourceCreationAPI {
 public:
  explicit ResourceCreationImpl(PluginInstance* instance);
  virtual ~ResourceCreationImpl();

  // FunctionGroupBase implementation.
  virtual ::ppapi::thunk::ResourceCreationAPI* AsResourceCreationAPI();

  // ResourceCreationAPI implementation.
  virtual PP_Resource CreateAudio(PP_Instance instance,
                                  PP_Resource config_id,
                                  PPB_Audio_Callback audio_callback,
                                  void* user_data) OVERRIDE;
  virtual PP_Resource CreateAudioTrusted(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateAudioConfig(PP_Instance instance,
                                        PP_AudioSampleRate sample_rate,
                                        uint32_t sample_frame_count) OVERRIDE;
  virtual PP_Resource CreateAudioInput(
      PP_Instance instance,
      PP_Resource config_id,
      PPB_AudioInput_Callback audio_input_callback,
      void* user_data) OVERRIDE;
  virtual PP_Resource CreateAudioInputTrusted(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateBroker(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateBuffer(PP_Instance instance,
                                   uint32_t size) OVERRIDE;
  virtual PP_Resource CreateDirectoryReader(PP_Resource directory_ref) OVERRIDE;
  virtual PP_Resource CreateFileChooser(
      PP_Instance instance,
      PP_FileChooserMode_Dev mode,
      const char* accept_mime_types) OVERRIDE;
  virtual PP_Resource CreateFileIO(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateFileRef(PP_Resource file_system,
                                    const char* path) OVERRIDE;
  virtual PP_Resource CreateFileSystem(PP_Instance instance,
                                       PP_FileSystemType type) OVERRIDE;
  virtual PP_Resource CreateFlashMenu(PP_Instance instance,
                                      const PP_Flash_Menu* menu_data) OVERRIDE;
  virtual PP_Resource CreateFlashNetConnector(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateFontObject(
      PP_Instance instance,
      const PP_FontDescription_Dev* description) OVERRIDE;
  virtual PP_Resource CreateGraphics2D(PP_Instance pp_instance,
                                       const PP_Size& size,
                                       PP_Bool is_always_opaque) OVERRIDE;
  virtual PP_Resource CreateGraphics3D(PP_Instance instance,
                                       PP_Resource share_context,
                                       const int32_t* attrib_list) OVERRIDE;
  virtual PP_Resource CreateGraphics3DRaw(PP_Instance instance,
                                          PP_Resource share_context,
                                          const int32_t* attrib_list) OVERRIDE;
  virtual PP_Resource CreateImageData(PP_Instance instance,
                                      PP_ImageDataFormat format,
                                      const PP_Size& size,
                                      PP_Bool init_to_zero) OVERRIDE;
  virtual PP_Resource CreateKeyboardInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      uint32_t key_code,
      PP_Var character_text) OVERRIDE;
  virtual PP_Resource CreateMouseInputEvent(
      PP_Instance instance,
      PP_InputEvent_Type type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      PP_InputEvent_MouseButton mouse_button,
      const PP_Point* mouse_position,
      int32_t click_count,
      const PP_Point* mouse_movement) OVERRIDE;
  virtual PP_Resource CreateScrollbar(PP_Instance instance,
                                      PP_Bool vertical) OVERRIDE;
  virtual PP_Resource CreateTCPSocketPrivate(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateTransport(PP_Instance instance,
                                      const char* name,
                                      PP_TransportType type) OVERRIDE;
  virtual PP_Resource CreateUDPSocketPrivate(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateURLLoader(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateURLRequestInfo(
      PP_Instance instance,
      const ::ppapi::PPB_URLRequestInfo_Data& data) OVERRIDE;
  virtual PP_Resource CreateVideoCapture(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateVideoDecoder(
      PP_Instance instance,
      PP_Resource graphics3d_id,
      PP_VideoDecoder_Profile profile) OVERRIDE;
  virtual PP_Resource CreateVideoLayer(PP_Instance instance,
                                       PP_VideoLayerMode_Dev mode) OVERRIDE;
  virtual PP_Resource CreateWebSocket(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateWheelInputEvent(
      PP_Instance instance,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      const PP_FloatPoint* wheel_delta,
      const PP_FloatPoint* wheel_ticks,
      PP_Bool scroll_by_page) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceCreationImpl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_RESOURCE_CREATION_IMPL_H_
