// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_video_layer_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_VideoLayer_API> EnterVideoLayer;

PP_Resource Create(PP_Instance instance, PP_VideoLayerMode_Dev mode) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateVideoLayer(instance, mode);
}

PP_Bool IsVideoLayer(PP_Resource resource) {
  EnterVideoLayer enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

void SetPixelFormat(PP_Resource resource,
                    PP_VideoLayerPixelFormat_Dev pixel_format) {
  EnterVideoLayer enter(resource, true);
  if (enter.succeeded())
    enter.object()->SetPixelFormat(pixel_format);
}

void SetNativeSize(PP_Resource resource, const struct PP_Size* size) {
  EnterVideoLayer enter(resource, true);
  if (enter.succeeded())
    enter.object()->SetNativeSize(size);
}

void SetClipRect(PP_Resource resource, const struct PP_Rect* clip_rect) {
  EnterVideoLayer enter(resource, true);
  if (enter.succeeded())
    enter.object()->SetClipRect(clip_rect);
}

PP_Bool IsReady(PP_Resource resource) {
  EnterVideoLayer enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->IsReady();
}

PP_Bool UpdateContent(PP_Resource resource,
                      uint32_t no_of_planes,
                      const void** planes) {
  EnterVideoLayer enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->UpdateContent(no_of_planes, planes);
}

const PPB_VideoLayer_Dev g_ppb_videolayer_thunk = {
  &Create,
  &IsVideoLayer,
  &SetPixelFormat,
  &SetNativeSize,
  &SetClipRect,
  &IsReady,
  &UpdateContent
};

}  // namespace

const PPB_VideoLayer_Dev* GetPPB_VideoLayer_Dev_Thunk() {
  return &g_ppb_videolayer_thunk;
}

}  // namespace thunk
}  // namespace ppapi
