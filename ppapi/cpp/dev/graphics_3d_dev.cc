// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/graphics_3d_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Graphics3D_Dev>() {
  return PPB_GRAPHICS_3D_DEV_INTERFACE;
}

}  // namespace

Graphics3D_Dev::Graphics3D_Dev() {
}

Graphics3D_Dev::Graphics3D_Dev(const Instance& instance,
                               PP_Config3D_Dev config,
                               const Graphics3D_Dev& share_context,
                               const int32_t* attrib_list) {
  if (has_interface<PPB_Graphics3D_Dev>()) {
    PassRefFromConstructor(get_interface<PPB_Graphics3D_Dev>()->Create(
        instance.pp_instance(),
        config,
        share_context.pp_resource(),
        attrib_list));
  }
}

Graphics3D_Dev::~Graphics3D_Dev() {
}

// static
int32_t Graphics3D_Dev::GetConfigs(int32_t *configs,
                                   int32_t config_size,
                                   int32_t *num_config) {
  if (!has_interface<PPB_Graphics3D_Dev>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_Dev>()->GetConfigs(
      configs, config_size, num_config);
}

// static
int32_t Graphics3D_Dev::GetConfigAttribs(PP_Config3D_Dev config,
                                         int32_t* attrib_list) {
  if (!has_interface<PPB_Graphics3D_Dev>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_Dev>()->GetConfigAttribs(
      config, attrib_list);
}

// static
Var Graphics3D_Dev::GetString(int32_t name) {
  if (!has_interface<PPB_Graphics3D_Dev>())
    return Var();

  return Var(Var::PassRef(),
             get_interface<PPB_Graphics3D_Dev>()->GetString(name));
}

int32_t Graphics3D_Dev::GetAttribs(int32_t* attrib_list) const {
  if (!has_interface<PPB_Graphics3D_Dev>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_Dev>()->GetAttribs(
      pp_resource(),
      attrib_list);
}

int32_t Graphics3D_Dev::SetAttribs(int32_t* attrib_list) {
  if (!has_interface<PPB_Graphics3D_Dev>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_Dev>()->SetAttribs(
      pp_resource(),
      attrib_list);
}

int32_t Graphics3D_Dev::ResizeBuffers(int32_t width, int32_t height) {
  if (!has_interface<PPB_Graphics3D_Dev>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_Dev>()->ResizeBuffers(
      pp_resource(), width, height);
}

int32_t Graphics3D_Dev::SwapBuffers(const CompletionCallback& cc) {
  if (!has_interface<PPB_Graphics3D_Dev>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_Dev>()->SwapBuffers(
      pp_resource(),
      cc.pp_completion_callback());
}

}  // namespace pp

