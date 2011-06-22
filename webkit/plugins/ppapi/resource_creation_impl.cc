// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/resource_creation_impl.h"

#include "ppapi/c/pp_size.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_audio_impl.h"
#include "webkit/plugins/ppapi/ppb_broker_impl.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"
#include "webkit/plugins/ppapi/ppb_context_3d_impl.h"
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
#include "webkit/plugins/ppapi/ppb_surface_3d_impl.h"
#include "webkit/plugins/ppapi/ppb_transport_impl.h"
#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/ppb_video_decoder_impl.h"
#include "webkit/plugins/ppapi/ppb_video_layer_impl.h"

namespace webkit {
namespace ppapi {

namespace {

// We use two methods for creating resources. When the resource initialization
// is simple and can't fail, just do
//   return ReturnResource(new PPB_Foo_Impl(instance_, ...));
// This will set up everything necessary.
//
// If the resource is more complex, generally the best thing is to write a
// static "Create" function on the resource class that returns a PP_Resource
// or 0 on failure. That helps keep the resource-specific stuff localized and
// this class very simple.
PP_Resource ReturnResource(Resource* resource) {
  // We actually have to keep a ref here since the argument will not be ref'ed
  // at all if it was just passed in with new (the expected usage). The
  // returned PP_Resource created by GetReference will hold onto a ref on
  // behalf of the plugin which will outlive this function. So the end result
  // will be a Resource with one ref.
  scoped_refptr<Resource> ref(resource);
  return resource->GetReference();
}

}  // namespace


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
  return PPB_Audio_Impl::Create(instance_, config_id, audio_callback,
                                user_data);
}

PP_Resource ResourceCreationImpl::CreateAudioConfig(
    PP_Instance instance_id,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  return PPB_AudioConfig_Impl::Create(instance_, sample_rate,
                                       sample_frame_count);
}

PP_Resource ResourceCreationImpl::CreateAudioTrusted(
    PP_Instance instance_id) {
  return ReturnResource(new PPB_Audio_Impl(instance_));
}

PP_Resource ResourceCreationImpl::CreateBroker(PP_Instance instance) {
  return ReturnResource(new PPB_Broker_Impl(instance_));
}

PP_Resource ResourceCreationImpl::CreateBuffer(PP_Instance instance,
                                               uint32_t size) {
  return PPB_Buffer_Impl::Create(instance_, size);
}

PP_Resource ResourceCreationImpl::CreateContext3D(
    PP_Instance instance,
    PP_Config3D_Dev config,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  return PPB_Context3D_Impl::Create(instance, config, share_context,
                                    attrib_list);
}

PP_Resource ResourceCreationImpl::CreateContext3DRaw(
    PP_Instance instance,
    PP_Config3D_Dev config,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  return PPB_Context3D_Impl::CreateRaw(instance, config, share_context,
                                       attrib_list);
}

PP_Resource ResourceCreationImpl::CreateDirectoryReader(
    PP_Resource directory_ref) {
  return PPB_DirectoryReader_Impl::Create(directory_ref);
}

PP_Resource ResourceCreationImpl::CreateFileChooser(
    PP_Instance instance,
    const PP_FileChooserOptions_Dev* options) {
  return PPB_FileChooser_Impl::Create(instance_, options);
}

PP_Resource ResourceCreationImpl::CreateFileIO(PP_Instance instance) {
  return ReturnResource(new PPB_FileIO_Impl(instance_));
}

PP_Resource ResourceCreationImpl::CreateFileRef(PP_Resource file_system,
                                                const char* path) {
  return PPB_FileRef_Impl::Create(file_system, path);
}

PP_Resource ResourceCreationImpl::CreateFileSystem(
    PP_Instance instance,
    PP_FileSystemType_Dev type) {
  return PPB_FileSystem_Impl::Create(instance_, type);
}

PP_Resource ResourceCreationImpl::CreateFlashMenu(
    PP_Instance instance,
    const PP_Flash_Menu* menu_data) {
  return PPB_Flash_Menu_Impl::Create(instance_, menu_data);
}

PP_Resource ResourceCreationImpl::CreateFlashNetConnector(
    PP_Instance instance) {
  return ReturnResource(new PPB_Flash_NetConnector_Impl(instance_));
}

PP_Resource ResourceCreationImpl::CreateFontObject(
    PP_Instance pp_instance,
    const PP_FontDescription_Dev* description) {
  return PPB_Font_Impl::Create(instance_, *description);
}

PP_Resource ResourceCreationImpl::CreateGraphics2D(
    PP_Instance pp_instance,
    const PP_Size& size,
    PP_Bool is_always_opaque) {
  return PPB_Graphics2D_Impl::Create(instance_, size, is_always_opaque);
}

PP_Resource ResourceCreationImpl::CreateGraphics3D(
    PP_Instance instance,
    PP_Config3D_Dev config,
    PP_Resource share_context,
    const int32_t* attrib_list) {
  return PPB_Graphics3D_Impl::Create(instance_, config, share_context,
                                     attrib_list);
}

PP_Resource ResourceCreationImpl::CreateImageData(PP_Instance pp_instance,
                                                  PP_ImageDataFormat format,
                                                  const PP_Size& size,
                                                  PP_Bool init_to_zero) {
  return PPB_ImageData_Impl::Create(instance_, format, size, init_to_zero);
}

PP_Resource ResourceCreationImpl::CreateSurface3D(
    PP_Instance instance,
    PP_Config3D_Dev config,
    const int32_t* attrib_list) {
  NOTIMPLEMENTED();
  return 0;
}

PP_Resource ResourceCreationImpl::CreateTransport(PP_Instance instance,
                                                  const char* name,
                                                  const char* proto) {
  return PPB_Transport_Impl::Create(instance_, name, proto);
}

PP_Resource ResourceCreationImpl::CreateURLLoader(PP_Instance instance) {
  return ReturnResource(new PPB_URLLoader_Impl(instance_, false));
}

PP_Resource ResourceCreationImpl::CreateURLRequestInfo(PP_Instance instance) {
  return ReturnResource(new PPB_URLRequestInfo_Impl(instance_));
}

PP_Resource ResourceCreationImpl::CreateVideoDecoder(PP_Instance instance) {
  return ReturnResource(new PPB_VideoDecoder_Impl(instance_));
}

PP_Resource ResourceCreationImpl::CreateVideoLayer(PP_Instance instance,
                                                   PP_VideoLayerMode_Dev mode) {
  return PPB_VideoLayer_Impl::Create(instance_, mode);
}

}  // namespace ppapi
}  // namespace webkit
