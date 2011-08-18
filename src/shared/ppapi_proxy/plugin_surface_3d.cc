// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_surface_3d.h"

#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_context_3d.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/third_party/ppapi/c/pp_completion_callback.h"
#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

namespace {

PP_Resource Create(PP_Instance instance,
                   PP_Config3D_Dev config,
                   const int32_t* attrib_list) {
  DebugPrintf("PPB_Surface3D::Create: instance=%"NACL_PRIu32"\n", instance);
  nacl_abi_size_t attrib_list_size = 0;
  PP_Resource resource;
  if (attrib_list) {
    attrib_list_size = 1;
    while (PP_GRAPHICS3DATTRIB_NONE != attrib_list[attrib_list_size - 1]) {
      attrib_list_size += 2;
    }
  }
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Surface3D_Create(
          GetMainSrpcChannel(),
          instance,
          config,
          attrib_list_size,
          const_cast<int32_t*>(attrib_list),
          &resource);
  if (retval == NACL_SRPC_RESULT_OK) {
    scoped_refptr<PluginSurface3D> surface_3d =
        PluginResource::AdoptAs<PluginSurface3D>(resource);
    if (surface_3d.get())
      return resource;
  }

  return kInvalidResourceId;
}

PP_Bool IsSurface3D(PP_Resource resource) {
  DebugPrintf("PPB_Surface3D::IsSurface3D: resource=%"NACL_PRIu32"\n",
              resource);
  return PluginResource::GetAs<PluginSurface3D>(resource).get()
      ? PP_TRUE : PP_FALSE;

}

int32_t SetAttrib(PP_Resource surface,
                  int32_t attribute,
                  int32_t value) {
  UNREFERENCED_PARAMETER(surface);
  UNREFERENCED_PARAMETER(attribute);
  UNREFERENCED_PARAMETER(value);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_FAILED;
}

int32_t GetAttrib(PP_Resource surface,
                  int32_t attribute,
                  int32_t* value) {
  UNREFERENCED_PARAMETER(surface);
  UNREFERENCED_PARAMETER(attribute);
  UNREFERENCED_PARAMETER(value);
  NACL_UNIMPLEMENTED();
  return PP_ERROR_FAILED;
}

int32_t SwapBuffs(PP_Resource surface_id,
                  struct PP_CompletionCallback callback) {
  DebugPrintf("PPB_Surface3D::SwapBuffers: surface=%"NACL_PRIu32"\n",
              surface_id);

  scoped_refptr<PluginSurface3D> surface =
      PluginResource::GetAs<PluginSurface3D>(surface_id).get();
  if (!surface.get())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);

  return MayForceCallback(callback, surface->SwapBuffers(surface_id, callback));
}

}  // namespace

PluginSurface3D::PluginSurface3D() : bound_context_(0) { }

PluginSurface3D::~PluginSurface3D() { }

bool PluginSurface3D::InitFromBrowserResource(PP_Resource res) {
  // TODO(neb): GetBoundContext()->bound_context_
  return true;
}

int32_t PluginSurface3D::SwapBuffers(PP_Resource surface_id,
                                     struct PP_CompletionCallback callback) {
  scoped_refptr<PluginContext3D> context =
      PluginResource::GetAs<PluginContext3D>(get_bound_context());

  if (!context.get())
    return PP_ERROR_BADARGUMENT;

  int32_t callback_id = CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)  // Just like Chrome, for now disallow blocking calls.
    return PP_ERROR_BADARGUMENT;

  int32_t pp_error;
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Surface3D_SwapBuffers(
          GetMainSrpcChannel(),
          surface_id,
          callback_id,
          &pp_error);
  if (retval != NACL_SRPC_RESULT_OK)
    return PP_ERROR_FAILED;

  if ((PP_OK_COMPLETIONPENDING != pp_error) && (PP_OK != pp_error))
    return pp_error;

  context->SwapBuffers();
  return PP_OK_COMPLETIONPENDING;
}

const PPB_Surface3D_Dev* PluginSurface3D::GetInterface() {
  static const PPB_Surface3D_Dev intf = {
    &Create,
    &IsSurface3D,
    &SetAttrib,
    &GetAttrib,
    &SwapBuffs
  };
  return &intf;
}

}  // namespace ppapi_proxy
