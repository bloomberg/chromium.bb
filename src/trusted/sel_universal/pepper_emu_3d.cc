/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// initialize post-message sub-system for use with sel_universal
// for now we just print the post-message content on the screen for debugging


#include <string.h>
#include <string>
#include <iostream>

#include "base/file_descriptor_posix.h"
#include "base/shared_memory.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"

#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/dev/ppb_context_3d_trusted_dev.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/sel_universal/pepper_emu.h"
#include "native_client/src/trusted/sel_universal/pepper_emu_helper.h"
#include "native_client/src/trusted/sel_universal/primitives.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/srpc_helper.h"

using nacl::DescWrapperFactory;
using nacl::DescWrapper;

namespace {

IMultimedia* GlobalMultiMediaInterface = 0;

const int kFirstContext3dTrustedHandle = 3000;
const int kFirstSurface3dHandle = 3100;

struct DataContext3dTrusted {
  gpu::CommandBufferService* command_buffer;
  // Not yet used
  gpu::gles2::GLES2CmdHelper* helper;
};

struct DataSurface3D {
  // nothing here yet
};


Resource<DataContext3dTrusted> GlobalContext3dTrustedResources(
  kFirstContext3dTrustedHandle, "3dtrusted");

Resource<DataSurface3D> GlobalSurface3DResources(
  kFirstSurface3dHandle, "surface3d");


static int DumpBuffers(gpu::CommandBufferService* command_buffer, int start) {
  gpu::Buffer ring_buffer = command_buffer->GetRingBuffer();
  printf("Dump command buffer\n");
  gpu::CommandBufferEntry* buffer =
    reinterpret_cast<gpu::CommandBufferEntry *>(ring_buffer.ptr);
  int num_entries = ring_buffer.size / 4;
  int i = start;
  while (i < num_entries) {
    gpu::CommandHeader header = buffer[i].value_header;
    gpu::gles2::CommandId id =
      static_cast<gpu::gles2::CommandId>(header.command);
    printf("[%d] %d %d %s\n",
           i,
           header.command,
           header.size,
           gpu::gles2::GetCommandName(id));
    if (header.size == 0) {
      return i;
    }
    i = (i + header.size) % num_entries;
  }
}

// PPB_Context3D_BindSurfaces:iii:i
void PPB_Context3DTrusted_CreateRaw(SRPC_PARAMS) {
  const int instance = ins[0]->u.ival;
  const int config = ins[1]->u.ival;
  const int context = ins[2]->u.ival;

  NaClLog(1, "PPB_Context3DTrusted_CreateRaw(%d, %d, %d)\n",
          instance, config, context);

  outs[0]->u.ival = GlobalContext3dTrustedResources.Alloc();
  if (outs[0]->u.ival < 0) {
    NaClLog(LOG_FATAL, "PPB_Context3DTrusted_CreateRaw: failed\n");
  }
  NaClLog(1, "PPB_Context3DTrusted_CreateRaw -> %d\n", outs[0]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_Context3DTrusted_Initialize:ii:i
void PPB_Context3DTrusted_Initialize(SRPC_PARAMS) {
  const int handle = ins[0]->u.ival;
  const int size = ins[1]->u.ival;

  NaClLog(1, "PPB_Context3DTrusted_Initialize(%d, %d)\n", handle, size);
  DataContext3dTrusted* context3d =
   GlobalContext3dTrustedResources.GetDataForHandle(handle);
  base::SharedMemory* shmem = new base::SharedMemory();
  shmem->CreateAnonymous(size);
  context3d->command_buffer = new gpu::CommandBufferService();
  context3d->command_buffer->Initialize(shmem, size);


  context3d->helper = new gpu::gles2::GLES2CmdHelper(
    context3d->command_buffer);
  context3d->helper->Initialize(size);

  outs[0]->u.ival = 1;
  if (outs[0]->u.ival < 0) {
    NaClLog(LOG_FATAL, "PPB_Context3DTrusted_Initialize: failed\n");
  }

  NaClLog(1, "PPB_Context3DTrusted_Initialize -> %d\n", outs[0]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_Context3DTrusted_GetRingBuffer:i:hi
void PPB_Context3DTrusted_GetRingBuffer(SRPC_PARAMS) {
  const int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_Context3DTrusted_GetRingBuffer(%d)\n", handle);
  DataContext3dTrusted* context3d =
    GlobalContext3dTrustedResources.GetDataForHandle(handle);

  nacl::DescWrapperFactory factory;
  gpu::Buffer buf = context3d->command_buffer->GetRingBuffer();
  // NOTE: not portable!
  nacl::DescWrapper* dw =
    factory.ImportShmHandle(buf.shared_memory->handle().fd, buf.size);
  outs[0]->u.hval = dw->desc();
  outs[1]->u.ival = buf.size;
  NaClLog(1, "PPB_Context3DTrusted_GetRingBuffer -> %d\n",
          outs[1]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


static void SetContext3DTrustedState(
  const gpu::CommandBufferService::State& raw_state, NaClSrpcArg* arg) {
  PP_Context3DTrustedState pp_state;
  pp_state.num_entries = raw_state.num_entries;
  pp_state.get_offset = raw_state.get_offset;
  pp_state.put_offset = raw_state.put_offset;
  pp_state.token = raw_state.token;
  // hack
  pp_state.error = PPB_Context3DTrustedError((int)raw_state.error);
  pp_state.generation = raw_state.generation;
  const size_t size = sizeof(pp_state);
  char* data = static_cast<char*>(malloc(size));
  memcpy(data, &pp_state, size);
  arg->u.count = size;
  arg->arrays.carr = data;

  NaClLog(1, "num_entries: %d\n", pp_state.num_entries);
  NaClLog(1, "get_offset: %d\n", pp_state.get_offset);
  NaClLog(1, "put_offset: %d\n", pp_state.put_offset);
  NaClLog(1, "token: %d\n", pp_state.token);
  NaClLog(1, "error: %d\n", pp_state.error);
  NaClLog(1, "generation: %d\n", pp_state.generation);
}


// PPB_Context3DTrusted_GetState:i:C
void PPB_Context3DTrusted_GetState(SRPC_PARAMS) {
  const int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_Context3DTrusted_GetState(%d)\n", handle);
  DataContext3dTrusted* context3d =
    GlobalContext3dTrustedResources.GetDataForHandle(handle);
  gpu::CommandBufferService::State raw_state =
    context3d->command_buffer->CommandBufferService::GetState();
  SetContext3DTrustedState(raw_state, outs[0]);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_Context3DTrusted_CreateTransferBuffer:iii:i
void PPB_Context3DTrusted_CreateTransferBuffer(SRPC_PARAMS) {
  const int handle = ins[0]->u.ival;
  const int size = ins[1]->u.ival;
  const int id = ins[2]->u.ival;
  NaClLog(1, "PPB_Context3DTrusted_CreateTransferBuffer(%d, %d, %d)\n",
          handle, size, id);
  DataContext3dTrusted* context3d =
    GlobalContext3dTrustedResources.GetDataForHandle(handle);
  int new_id = context3d->command_buffer->CreateTransferBuffer(size, id);
  NaClLog(1, "PPB_Context3DTrusted_CreateTransferBuffer -> %d\n", new_id);
  outs[0]->u.ival = new_id;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_Context3DTrusted_GetTransferBuffer:ii:hi
void PPB_Context3DTrusted_GetTransferBuffer(SRPC_PARAMS) {
  const int handle = ins[0]->u.ival;
  const int id = ins[1]->u.ival;
  NaClLog(1, "PPB_Context3DTrusted_GetTransferBuffer(%d, %d)\n",
          handle, id);

  DataContext3dTrusted* context3d =
    GlobalContext3dTrustedResources.GetDataForHandle(handle);
  gpu::Buffer buf = context3d->command_buffer->GetTransferBuffer(id);

  nacl::DescWrapperFactory factory;
  // NOTE: not portable!
  nacl::DescWrapper* dw =
    factory.ImportShmHandle(buf.shared_memory->handle().fd, buf.size);
  outs[0]->u.hval = dw->desc();
  outs[1]->u.ival = buf.size;
  NaClLog(1, "PPB_Context3DTrusted_GetTransferBuffer -> %d\n",
          outs[1]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_Context3DTrusted_FlushSync:ii:C
void PPB_Context3DTrusted_FlushSync(SRPC_PARAMS) {
  // this currenly just dumps stuff out in textual form
  // buffer get position
  static int last_entry = 0;
  const int handle = ins[0]->u.ival;
  const int offset = ins[1]->u.ival;
  NaClLog(1, "PPB_Context3DTrusted_FlushSync(%d, %d)\n",
          handle, offset);
  DataContext3dTrusted* context3d =
    GlobalContext3dTrustedResources.GetDataForHandle(handle);
  gpu::CommandBufferService::State raw_state =
    context3d->command_buffer->CommandBufferService::FlushSync(offset, -1);
  SetContext3DTrustedState(raw_state, outs[0]);
  last_entry = DumpBuffers(context3d->command_buffer, last_entry);
  context3d->command_buffer->SetGetOffset(last_entry);

  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_Surface3D_Create:iiI:i
void PPB_Surface3D_Create(SRPC_PARAMS) {
  const int handle = ins[0]->u.ival;
  const int config = ins[1]->u.ival;
  NaClLog(1, "PPB_Surface3D_Createc(%d, %d)\n",
          handle, config);

  outs[0]->u.ival = GlobalSurface3DResources.Alloc();
  if (outs[0]->u.ival < 0) {
    NaClLog(LOG_FATAL, "PPB_Surface3D_Create: failed\n");
  }
  NaClLog(1, "PPB_Surface3D_Create -> %d\n", outs[0]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_Context3D_BindSurfaces:iii:i
void PPB_Context3D_BindSurfaces(SRPC_PARAMS) {
  const int handle = ins[0]->u.ival;
  const int draw = ins[1]->u.ival;
  const int read = ins[1]->u.ival;
  NaClLog(1, "PPB_Context3D_BindSurfaces(%d, %d, %d)\n",
          handle, draw, read);

  outs[0]->u.ival = 1;
  NaClLog(1, "PPB_Context3D_BindSurfaces -> %d\n", outs[0]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

}  // end namespace

#define TUPLE(a, b) #a #b, a
void PepperEmuInit3D(NaClCommandLoop* ncl, IMultimedia* im) {
  GlobalMultiMediaInterface = im;
  NaClLog(LOG_INFO, "PepperEmuInit3D\n");

  ncl->AddUpcallRpc(TUPLE(PPB_Context3DTrusted_CreateRaw, :iiiI:i));
  ncl->AddUpcallRpc(TUPLE(PPB_Context3DTrusted_Initialize, :ii:i));
  ncl->AddUpcallRpc(TUPLE(PPB_Context3DTrusted_GetRingBuffer, :i:hi));
  ncl->AddUpcallRpc(TUPLE(PPB_Context3DTrusted_GetState, :i:C));
  ncl->AddUpcallRpc(TUPLE(PPB_Context3DTrusted_CreateTransferBuffer, :iii:i));
  ncl->AddUpcallRpc(TUPLE(PPB_Context3DTrusted_GetTransferBuffer, :ii:hi));
  ncl->AddUpcallRpc(TUPLE(PPB_Context3DTrusted_FlushSync, :ii:C));

  ncl->AddUpcallRpc(TUPLE(PPB_Context3D_BindSurfaces, :iii:i));

  ncl->AddUpcallRpc(TUPLE(PPB_Surface3D_Create, :iiI:i));

#if 0
  // NYI
  ncl->AddUpcallRpc(TUPLE(PPB_Context3DTrusted_Flush, :ii:));
  ncl->AddUpcallRpc(TUPLE(PPB_Context3DTrusted_DestroyTransferBuffer, :ii:));
  ncl->AddUpcallRpc(TUPLE(PPB_Surface3D_SwapBuffers, :ii:i));
#endif
}
