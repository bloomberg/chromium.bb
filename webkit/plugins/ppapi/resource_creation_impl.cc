// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/resource_creation_impl.h"

#include "ppapi/c/pp_size.h"
#include "ppapi/shared_impl/font_impl.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_audio_impl.h"
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
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/ppb_surface_3d_impl.h"
#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/ppb_video_decoder_impl.h"
#include "webkit/plugins/ppapi/ppb_video_layer_impl.h"

namespace webkit {
namespace ppapi {

ResourceCreationImpl::ResourceCreationImpl(PluginInstance* instance)
    : instance_(instance) {
}

ResourceCreationImpl::~ResourceCreationImpl() {
}

::ppapi::thunk::ResourceCreationAPI*
ResourceCreationImpl::AsResourceCreationAPI() {
  return this;
}

PP_Resource ResourceCreationImpl::CreateAudio(
    PP_Instance instance_id,
    PP_Resource config_id,
    PPB_Audio_Callback audio_callback,
    void* user_data) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;
  scoped_refptr<PPB_Audio_Impl> audio(new PPB_Audio_Impl(instance));
  if (!audio->Init(config_id, audio_callback, user_data))
    return 0;
  return audio->GetReference();
}

PP_Resource ResourceCreationImpl::CreateAudioConfig(
    PP_Instance instance_id,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;
  scoped_refptr<PPB_AudioConfig_Impl> config(
      new PPB_AudioConfig_Impl(instance));
  if (!config->Init(sample_rate, sample_frame_count))
    return 0;
  return config->GetReference();
}

PP_Resource ResourceCreationImpl::CreateAudioTrusted(
    PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;
  scoped_refptr<PPB_Audio_Impl> audio(new PPB_Audio_Impl(instance));
  return audio->GetReference();
}

PP_Resource ResourceCreationImpl::CreateBroker(PP_Instance instance) {
  return PPB_Broker_Impl::Create(instance);
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
    const PP_FileChooserOptions_Dev* options) {
  return PPB_FileChooser_Impl::Create(instance, options);
}

PP_Resource ResourceCreationImpl::CreateFileIO(PP_Instance instance) {
  return PPB_FileIO_Impl::Create(instance);
}

PP_Resource ResourceCreationImpl::CreateFileRef(PP_Resource file_system,
                                                const char* path) {
  return PPB_FileRef_Impl::Create(file_system, path);
}

PP_Resource ResourceCreationImpl::CreateFileSystem(
    PP_Instance instance,
    PP_FileSystemType_Dev type) {
  return PPB_FileSystem_Impl::Create(instance, type);
}

PP_Resource ResourceCreationImpl::CreateFlashMenu(
    PP_Instance instance,
    const PP_Flash_Menu* menu_data) {
  return PPB_Flash_Menu_Impl::Create(instance, menu_data);
}

PP_Resource ResourceCreationImpl::CreateFlashNetConnector(
    PP_Instance instance) {
  return PPB_Flash_NetConnector_Impl::Create(instance);
}

PP_Resource ResourceCreationImpl::CreateFontObject(
    PP_Instance pp_instance,
    const PP_FontDescription_Dev* description) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return 0;

  if (!::ppapi::FontImpl::IsPPFontDescriptionValid(*description))
    return 0;

  scoped_refptr<PPB_Font_Impl> font(new PPB_Font_Impl(instance, *description));
  return font->GetReference();
}

PP_Resource ResourceCreationImpl::CreateGraphics2D(
    PP_Instance pp_instance,
    const PP_Size& size,
    PP_Bool is_always_opaque) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Graphics2D_Impl> graphics_2d(
      new PPB_Graphics2D_Impl(instance));
  if (!graphics_2d->Init(size.width, size.height,
                         PPBoolToBool(is_always_opaque))) {
    return 0;
  }
  return graphics_2d->GetReference();
}

PP_Resource ResourceCreationImpl::CreateImageData(PP_Instance pp_instance,
                                                  PP_ImageDataFormat format,
                                                  const PP_Size& size,
                                                  PP_Bool init_to_zero) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return 0;

  scoped_refptr<PPB_ImageData_Impl> data(new PPB_ImageData_Impl(instance));
  if (!data->Init(format, size.width, size.height, !!init_to_zero))
    return 0;
  return data->GetReference();
}

PP_Resource ResourceCreationImpl::CreateSurface3D(
    PP_Instance instance,
    PP_Config3D_Dev config,
    const int32_t* attrib_list) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateURLLoader(PP_Instance instance) {
  return PPB_URLLoader_Impl::Create(instance);
}

PP_Resource ResourceCreationImpl::CreateURLRequestInfo(PP_Instance instance) {
  return PPB_URLRequestInfo_Impl::Create(instance);
}

PP_Resource ResourceCreationImpl::CreateVideoDecoder(PP_Instance instance) {
  return PPB_VideoDecoder_Impl::Create(instance);
}

PP_Resource ResourceCreationImpl::CreateVideoLayer(PP_Instance instance,
                                                   PP_VideoLayerMode_Dev mode) {
  return PPB_VideoLayer_Impl::Create(instance, mode);
}

}  // namespace ppapi
}  // namespace webkit
