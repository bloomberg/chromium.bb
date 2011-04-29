// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_video_layer_impl.h"

#include "base/logging.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_video_layer_software.h"

namespace webkit {
namespace ppapi {

namespace {

PP_Resource Create(PP_Instance instance_id, PP_VideoLayerMode_Dev mode) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  if (mode != PP_VIDEOLAYERMODE_SOFTWARE)
    return 0;

  scoped_refptr<PPB_VideoLayer_Impl> layer(
      new PPB_VideoLayer_Software(instance));
  return layer->GetReference();
}

PP_Bool IsVideoLayer(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_VideoLayer_Impl>(resource));
}
void SetPixelFormat(PP_Resource resource,
                    PP_VideoLayerPixelFormat_Dev pixel_format) {
 scoped_refptr<PPB_VideoLayer_Impl> layer(
     Resource::GetAs<PPB_VideoLayer_Impl>(resource));
 layer->SetPixelFormat(pixel_format);
}

void SetNativeSize(PP_Resource resource, const struct PP_Size* size) {
 scoped_refptr<PPB_VideoLayer_Impl> layer(
     Resource::GetAs<PPB_VideoLayer_Impl>(resource));
 layer->SetNativeSize(size);
}

void SetClipRect(PP_Resource resource, const struct PP_Rect* clip_rect) {
 scoped_refptr<PPB_VideoLayer_Impl> layer(
     Resource::GetAs<PPB_VideoLayer_Impl>(resource));
 layer->SetClipRect(clip_rect);
}

PP_Bool IsReady(PP_Resource resource) {
 scoped_refptr<PPB_VideoLayer_Impl> layer(
     Resource::GetAs<PPB_VideoLayer_Impl>(resource));
 return layer->IsReady();
}

PP_Bool UpdateContent(PP_Resource resource, uint32_t no_of_planes,
                      const void** planes) {
 scoped_refptr<PPB_VideoLayer_Impl> layer(
     Resource::GetAs<PPB_VideoLayer_Impl>(resource));
 return layer->UpdateContent(no_of_planes, planes);
}

const PPB_VideoLayer_Dev ppb_videolayer = {
  &Create,
  &IsVideoLayer,
  &SetPixelFormat,
  &SetNativeSize,
  &SetClipRect,
  &IsReady,
  &UpdateContent,
};

}  // namespace

PPB_VideoLayer_Impl::PPB_VideoLayer_Impl(PluginInstance* instance)
    : Resource(instance) {
}

PPB_VideoLayer_Impl::~PPB_VideoLayer_Impl() {
}

PPB_VideoLayer_Impl* PPB_VideoLayer_Impl::AsPPB_VideoLayer_Impl() {
  return this;
}

// static
const PPB_VideoLayer_Dev* PPB_VideoLayer_Impl::GetInterface() {
  return &ppb_videolayer;
}

}  // namespace ppapi
}  // namespace webkit
