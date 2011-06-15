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
  ResourceCreationImpl(PluginInstance* instance);
  virtual ~ResourceCreationImpl();

  // FunctionGroupBase implementation.
  virtual ::ppapi::thunk::ResourceCreationAPI* AsResourceCreationAPI();

  // ResourceCreationAPI implementation.
  virtual PP_Resource CreateAudio(PP_Instance instance,
                                  PP_Resource config_id,
                                  PPB_Audio_Callback audio_callback,
                                  void* user_data) OVERRIDE;
  virtual PP_Resource CreateAudioTrusted(PP_Instance instace) OVERRIDE;
  virtual PP_Resource CreateAudioConfig(PP_Instance instance,
                                        PP_AudioSampleRate sample_rate,
                                        uint32_t sample_frame_count) OVERRIDE;
  virtual PP_Resource CreateBroker(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateBuffer(PP_Instance instance,
                                   uint32_t size) OVERRIDE;
  virtual PP_Resource CreateDirectoryReader(PP_Resource directory_ref) OVERRIDE;
  virtual PP_Resource CreateFileChooser(
      PP_Instance instance,
      const PP_FileChooserOptions_Dev* options) OVERRIDE;
  virtual PP_Resource CreateFileIO(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateFileRef(PP_Resource file_system,
                                    const char* path) OVERRIDE;
  virtual PP_Resource CreateFileSystem(PP_Instance instance,
                                       PP_FileSystemType_Dev type) OVERRIDE;
  virtual PP_Resource CreateFlashMenu(PP_Instance instance,
                                      const PP_Flash_Menu* menu_data) OVERRIDE;
  virtual PP_Resource CreateFlashNetConnector(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateFontObject(
      PP_Instance instance,
      const PP_FontDescription_Dev* description) OVERRIDE;
  virtual PP_Resource CreateGraphics2D(PP_Instance pp_instance,
                                       const PP_Size& size,
                                       PP_Bool is_always_opaque) OVERRIDE;
  virtual PP_Resource CreateImageData(PP_Instance instance,
                                      PP_ImageDataFormat format,
                                      const PP_Size& size,
                                      PP_Bool init_to_zero) OVERRIDE;
  virtual PP_Resource CreateSurface3D(PP_Instance instance,
                                      PP_Config3D_Dev config,
                                      const int32_t* attrib_list) OVERRIDE;
  virtual PP_Resource CreateURLLoader(PP_Instance instance) OVERRIDE;
  virtual PP_Resource CreateURLRequestInfo(PP_Instance instance) OVERRIDE;

 private:
  PluginInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCreationImpl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_RESOURCE_CREATION_IMPL_H_
