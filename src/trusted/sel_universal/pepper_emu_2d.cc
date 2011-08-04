/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file defines hooks for all pepper related srpc calls
// involving images and 2d graphicss.
// This is for experimentation and testing. We are not concerned
// about descriptor and memory leaks

#include <string.h>
#include <fstream>
#include <queue>
#include <string>

#if (NACL_LINUX)
// for shmem
#include <sys/ipc.h>
#include <sys/shm.h>
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
#endif


#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "native_client/src/third_party/ppapi/c/pp_input_event.h"
#include "native_client/src/third_party/ppapi/c/pp_size.h"
#include "native_client/src/third_party/ppapi/c/ppb_image_data.h"

#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

#include "native_client/src/trusted/sel_universal/primitives.h"
#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/trusted/sel_universal/pepper_emu.h"
#include "native_client/src/trusted/sel_universal/pepper_emu_helper.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"

#include "native_client/src/trusted/sel_universal/srpc_helper.h"

using nacl::DescWrapperFactory;
using nacl::DescWrapper;

// ======================================================================
namespace {

const int kBytesPerPixel = 4;

IMultimedia* GlobalMultiMediaInterface = 0;

struct DataImageData {
  int size;
  int width;
  int height;
  nacl::DescWrapper* desc_video_shmem;
  void* addr_video;
};

struct DataGraphics2D {
  int image_data;
  int width;
  int height;
};

// NOTE: even though this code suggests it can handle multiple
//       ImageData and Graphics2D objects it really cannot at this time.
Resource<DataGraphics2D> GlobalGraphics2dResources(100, "graphics2d");
Resource<DataImageData> GlobalImageDataResources(100, "image_data");



// From the ImageData API
// PP_Resource Create(PP_Instance instance,
//                    PP_ImageDataFormat format,
//                    const struct PP_Size* size,
//                    PP_Bool init_to_zero);
// PPB_ImageData_Create:iiCi:i
//
// TODO(robertm) this function can currently be called only once
//               and the dimension must match the global values
//               and the format is fixed.
static void PPB_ImageData_Create(SRPC_PARAMS) {
  const int instance = ins[0]->u.ival;
  const int format = ins[1]->u.ival;
  CHECK(ins[2]->u.count == sizeof(PP_Size));
  PP_Size* img_size = (PP_Size*) ins[2]->arrays.carr;
  NaClLog(1, "PPB_ImageData_Create(%d, %d, %d, %d)\n",
          instance, format, img_size->width, img_size->height);

  CHECK(format == PP_IMAGEDATAFORMAT_BGRA_PREMUL);
  CHECK(GlobalMultiMediaInterface->VideoWidth() == img_size->width);
  CHECK(GlobalMultiMediaInterface->VideoHeight() == img_size->height);

  const int handle = GlobalImageDataResources.Alloc();
  DataImageData* data = GlobalImageDataResources.GetDataForHandle(handle);

  data->width = img_size->width;
  data->height = img_size->height;
  data->size = kBytesPerPixel * img_size->width * img_size->height;

  nacl::DescWrapperFactory factory;
  data->desc_video_shmem = factory.MakeShm(data->size);
  size_t dummy_size;

  if (data->desc_video_shmem->Map(&data->addr_video, &dummy_size)) {
    NaClLog(LOG_FATAL, "cannot map video shmem\n");
  }

  if (ins[3]->u.ival) {
    memset(data->addr_video, 0, data->size);
  }

  outs[0]->u.ival = handle;
  NaClLog(1, "PPB_ImageData_Create() -> %d\n", handle);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the ImageData API
// PP_Bool Describe(PP_Resource image_data,
//                  struct PP_ImageDataDesc* desc);
// PPB_ImageData_Describe:i:Chii
static void PPB_ImageData_Describe(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_ImageData_Describe(%d)\n", handle);
  DataImageData* data = GlobalImageDataResources.GetDataForHandle(handle);

  PP_ImageDataDesc d;
  d.format = PP_IMAGEDATAFORMAT_BGRA_PREMUL;
  d.size.width =  data->width;
  d.size.height = data->height;
  // we handle only rgba data -> each pixel is 4 bytes.
  d.stride = data->width * kBytesPerPixel;
  outs[0]->u.count = sizeof(d);
  outs[0]->arrays.carr = reinterpret_cast<char*>(calloc(1, sizeof(d)));
  memcpy(outs[0]->arrays.carr, &d, sizeof(d));

  outs[1]->u.hval = data->desc_video_shmem->desc();
  outs[2]->u.ival = data->size;
  outs[3]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the Graphics2D API
// PP_Resource Create(PP_Instance instance,
//                    const struct PP_Size* size,
//                    PP_Bool is_always_opaque);
// PPB_Graphics2D_Create:iCi:i
//
// TODO(robertm) This function can currently be called only once
//               The size must be the same as the one provided via
//                HandlerSDLInitialize()
static void PPB_Graphics2D_Create(SRPC_PARAMS) {
  int instance = ins[0]->u.ival;
  NaClLog(1, "PPB_Graphics2D_Create(%d)\n", instance);
  const int handle = GlobalGraphics2dResources.Alloc();
  PP_Size* img_size = reinterpret_cast<PP_Size*>(ins[1]->arrays.carr);
  // for enforce fixed dimensions
  CHECK(GlobalMultiMediaInterface->VideoWidth() == img_size->width);
  CHECK(GlobalMultiMediaInterface->VideoHeight() == img_size->height);

  DataGraphics2D* data = GlobalGraphics2dResources.GetDataForHandle(handle);
  data->width = img_size->width;
  data->height = img_size->height;

  // TODO(robertm):  is_always_opaque is currently ignored
  outs[0]->u.ival = handle;
  NaClLog(1, "PPB_Graphics2D_Create() -> %d\n", handle);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PP_Bool BindGraphics(PP_Instance instance, PP_Resource device);
// PPB_Instance_BindGraphics:ii:i
static void PPB_Instance_BindGraphics(SRPC_PARAMS) {
  int instance = ins[0]->u.ival;
  int handle = ins[1]->u.ival;
  NaClLog(1, "PPB_Instance_BindGraphics(%d, %d)\n", instance, handle);
  // TODO(robertm):
  // Add checking code here for type of handle, e.g. 2d or 3d graphics
  outs[0]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the Graphics2D API
// void ReplaceContents(PP_Resource graphics_2d,
//                      PP_Resource image_data);
// PPB_Graphics2D_ReplaceContents:ii:
//
// NOTE: this is completely ignored and we postpone all action to "Flush"
static void PPB_Graphics2D_ReplaceContents(SRPC_PARAMS) {
  int handle_graphics2d = ins[0]->u.ival;
  int handle_image_data = ins[1]->u.ival;
  UNREFERENCED_PARAMETER(outs);
  NaClLog(1, "PPB_Graphics2D_ReplaceContents(%d, %d)\n",
          handle_graphics2d, handle_image_data);
  DataGraphics2D* data_graphics2d =
    GlobalGraphics2dResources.GetDataForHandle(handle_graphics2d);
  data_graphics2d->image_data = handle_image_data;

  // For now assume this will be immediately followed by a Flush
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the Graphics2D API
// void PaintImageData(PP_Resource graphics_2d,
//                     PP_Resource image_data,
//                     const struct PP_Point* top_left,
//                     const struct PP_Rect* src_rect);
// PPB_Graphics2D_PaintImageData:iiCC:
//
// NOTE: this is completely ignored and we postpone all action to "Flush"
//       Furhermore we assume that entire image is painted
static void PPB_Graphics2D_PaintImageData(SRPC_PARAMS) {
  int handle_graphics2d = ins[0]->u.ival;
  int handle_image_data = ins[1]->u.ival;
  UNREFERENCED_PARAMETER(outs);
  NaClLog(1, "PPB_Graphics2D_PaintImageData(%d, %d)\n",
          handle_graphics2d, handle_image_data);

  DataGraphics2D* data_graphics2d =
    GlobalGraphics2dResources.GetDataForHandle(handle_graphics2d);
  data_graphics2d->image_data = handle_image_data;
  // For now assume this will be immediately followed by a Flush
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the Graphics2D API
// int32_t Flush(PP_Resource graphics_2d,
//               struct PP_CompletionCallback callback);
// PPB_Graphics2D_Flush:ii:i
static void PPB_Graphics2D_Flush(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  int callback_id = ins[1]->u.ival;
  NaClLog(1, "PPB_Graphics2D_Flush(%d, %d)\n", handle, callback_id);
  outs[0]->u.ival = -1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);

  DataGraphics2D* data_graphics2d =
    GlobalGraphics2dResources.GetDataForHandle(handle);
  DataImageData *image_data =
    GlobalImageDataResources.GetDataForHandle(data_graphics2d->image_data);
  GlobalMultiMediaInterface->VideoUpdate(image_data->addr_video);
  UserEvent* event =
    MakeUserEvent(EVENT_TYPE_FLUSH_CALLBACK, callback_id, 0, 0, 0);
  GlobalMultiMediaInterface->PushUserEvent(event);
}

}  //  namespace

#define TUPLE(a, b) #a #b, a
void PepperEmuInit2D(NaClCommandLoop* ncl, IMultimedia* im) {
  GlobalMultiMediaInterface = im;
  NaClLog(LOG_INFO, "PepperEmuInit2d\n");

  ncl->AddUpcallRpc(TUPLE(PPB_Graphics2D_Create, :iCi:i));
  ncl->AddUpcallRpc(TUPLE(PPB_Graphics2D_ReplaceContents, :ii:));
  ncl->AddUpcallRpc(TUPLE(PPB_Graphics2D_PaintImageData, :iiCC:));
  ncl->AddUpcallRpc(TUPLE(PPB_Graphics2D_Flush, :ii:i));

  ncl->AddUpcallRpc(TUPLE(PPB_ImageData_Describe, :i:Chii));
  ncl->AddUpcallRpc(TUPLE(PPB_ImageData_Create, :iiCi:i));

  ncl->AddUpcallRpc(TUPLE(PPB_Instance_BindGraphics, :ii:i));
}
