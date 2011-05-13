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

class ResourceCreationImpl : public ::ppapi::FunctionGroupBase,
                             public ::ppapi::thunk::ResourceCreationAPI {
 public:
  ResourceCreationImpl();
  virtual ~ResourceCreationImpl();

  // FunctionGroupBase implementation.
  virtual ::ppapi::thunk::ResourceCreationAPI* AsResourceCreation();

  // ResourceCreationAPI implementation.
  virtual PP_Resource CreateAudio(PP_Instance instance,
                                  PP_Resource config_id,
                                  PPB_Audio_Callback audio_callback,
                                  void* user_data) OVERRIDE;
  virtual PP_Resource CreateAudioTrusted(PP_Instance instace) OVERRIDE;
  virtual PP_Resource CreateAudioConfig(PP_Instance instance,
                                        PP_AudioSampleRate sample_rate,
                                        uint32_t sample_frame_count) OVERRIDE;
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

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceCreationImpl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_RESOURCE_CREATION_IMPL_H_
