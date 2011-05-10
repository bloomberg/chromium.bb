// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/resource_creation_impl.h"

#include "ppapi/c/pp_size.h"
#include "ppapi/shared_impl/font_impl.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_font_impl.h"
#include "webkit/plugins/ppapi/ppb_graphics_2d_impl.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"

namespace webkit {
namespace ppapi {

ResourceCreationImpl::ResourceCreationImpl() {
}

ResourceCreationImpl::~ResourceCreationImpl() {
}

::ppapi::thunk::ResourceCreationAPI*
ResourceCreationImpl::AsResourceCreation() {
  return this;
}

PP_Resource ResourceCreationImpl::CreateFontObject(
    PP_Instance pp_instance,
    const PP_FontDescription_Dev* description) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(pp_instance);
  if (!instance)
    return 0;

  if (!pp::shared_impl::FontImpl::IsPPFontDescriptionValid(*description))
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

}  // namespace ppapi
}  // namespace webkit
