// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/graphics_3d_dev.h"

#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Graphics3D_Dev>() {
  return PPB_GRAPHICS_3D_DEV_INTERFACE;
}

template <> const char* interface_name<PPB_OpenGLES2_Dev>() {
  return PPB_OPENGLES2_DEV_INTERFACE;
}

}  // namespace

// static
bool Graphics3D_Dev::GetConfigs(int32_t *configs, int32_t config_size,
                                int32_t *num_config) {
  if (has_interface<PPB_Graphics3D_Dev>()) {
    return PPBoolToBool(get_interface<PPB_Graphics3D_Dev>()->GetConfigs(
        configs, config_size, num_config));
  }
  return false;
}

// static
bool Graphics3D_Dev::ChooseConfig(const int32_t *attrib_list, int32_t *configs,
                                  int32_t config_size, int32_t *num_config) {
  if (has_interface<PPB_Graphics3D_Dev>()) {
    return PPBoolToBool(get_interface<PPB_Graphics3D_Dev>()->ChooseConfig(
        attrib_list, configs, config_size, num_config));
  }
  return false;
}

// static
bool Graphics3D_Dev::GetConfigAttrib(int32_t config, int32_t attribute,
                                     int32_t *value) {
  if (has_interface<PPB_Graphics3D_Dev>()) {
    return PPBoolToBool(get_interface<PPB_Graphics3D_Dev>()->GetConfigAttrib(
        config, attribute, value));
  }
  return false;
}

// static
const char* Graphics3D_Dev::QueryString(int32_t name) {
  if (has_interface<PPB_Graphics3D_Dev>())
    return get_interface<PPB_Graphics3D_Dev>()->QueryString(name);
  return NULL;
}

// static
void* Graphics3D_Dev::GetProcAddress(const char* name) {
  if (has_interface<PPB_Graphics3D_Dev>())
    return get_interface<PPB_Graphics3D_Dev>()->GetProcAddress(name);
  return NULL;
}

Graphics3D_Dev Graphics3D_Dev::FromResource(PP_Resource resource_id) {
  if (has_interface<PPB_Graphics3D_Dev>() &&
      get_interface<PPB_Graphics3D_Dev>()->IsGraphics3D(resource_id))
    return Graphics3D_Dev(resource_id);
  return Graphics3D_Dev();
}

uint32_t Graphics3D_Dev::GetError() {
  if (has_interface<PPB_Graphics3D_Dev>())
    return get_interface<PPB_Graphics3D_Dev>()->GetError();
  return PP_GRAPHICS_3D_ERROR_NOT_INITIALIZED;
}

const PPB_OpenGLES2_Dev* Graphics3D_Dev::GetImplementation() {
  return get_interface<PPB_OpenGLES2_Dev>();
}

Graphics3D_Dev::Graphics3D_Dev(const Instance& instance,
                               int32_t config,
                               int32_t share_context,
                               const int32_t* attrib_list) {
  if (has_interface<PPB_Graphics3D_Dev>() &&
      has_interface<PPB_OpenGLES2_Dev>()) {
    PassRefFromConstructor(get_interface<PPB_Graphics3D_Dev>()->CreateContext(
        instance.pp_instance(), config, share_context, attrib_list));
  }
}

bool Graphics3D_Dev::SwapBuffers() const {
  return has_interface<PPB_Graphics3D_Dev>() &&
      get_interface<PPB_Graphics3D_Dev>()->SwapBuffers(pp_resource());
}

}  // namespace pp
