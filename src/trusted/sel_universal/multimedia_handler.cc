/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// This file exports a single function used to setup the
// multimedia sub-system for use with sel_universal
// It was inpspired by src/trusted/plugin/srpc/multimedia_socket.cc
// On the untrusted side it interface with: src/untrusted/av/nacl_av.c
//
// NOTE: this is experimentation and testing. We are not concerned
//       about descriptor and memory leaks

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/sel_universal/multimedia.h"
#include "native_client/src/trusted/service_runtime/include/sys/audio_video.h"
#include "native_client/src/trusted/service_runtime/internal_errno.h"

#include "native_client/src/untrusted/av/nacl_av_priv.h"

using nacl::DescWrapperFactory;
using nacl::DescWrapper;

// Configs

const int kMaxArgs = 5;

// Two ugly globals
IMultimedia* g_mm;
// NOTE: this shared memory struct is defined in src/untrusted/av/nacl_av_priv.h
NaClVideoShare* g_video_share;

static bool EventQueueIsFull() {
  const int next_write_pos = (g_video_share->u.h.event_write_index + 1) %
                             NACL_EVENT_RING_BUFFER_SIZE;
  return next_write_pos == g_video_share->u.h.event_read_index;
}

// TODO(robertm): there maybe some subtle theoretical race conditions here
//                consumer is src/untrusted/av/nacl_av.c
static void EventQueuePut(const NaClMultimediaEvent& event) {
  const int write_pos = g_video_share->u.h.event_write_index %
                       NACL_EVENT_RING_BUFFER_SIZE;
  // NOTE: structure assignment
  g_video_share->u.h.event_queue[write_pos] = event;
  g_video_share->u.h.event_write_index = (write_pos + 1) %
                                         NACL_EVENT_RING_BUFFER_SIZE;
}

static void EventQueueRefill(int max_events) {
  NaClMultimediaEvent event;
  for (int i = 0; i < max_events; ++i) {
    if (EventQueueIsFull()) {
      NaClLog(1, "event queue has filled up\n");
      return;
    }

    g_mm->EventPoll(&event);
    if (event.type == NACL_EVENT_NOT_USED) {
      return;
    }

    EventQueuePut(event);
  }
}

static void handleUpcall(NaClSrpcRpc* rpc,
                         NaClSrpcArg** ins,
                         NaClSrpcArg** outs,
                         NaClSrpcClosure* done) {
  UNREFERENCED_PARAMETER(ins);
  UNREFERENCED_PARAMETER(outs);
  g_mm->VideoUpdate(g_video_share->video_pixels);
  // NOTE: this is major hack: we piggyback on the video update
  //       to fill the event queue
  EventQueueRefill(NACL_EVENT_RING_BUFFER_SIZE);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


static void WINAPI ServiceHandlerThread(void* desc_void) {
  DescWrapper* desc = reinterpret_cast<DescWrapper*>(desc_void);
  NaClSrpcHandlerDesc handlers[] = {
    { "upcall::", handleUpcall },
    { NULL, NULL }
  };

  NaClLog(1, "upcall service handler start\n");
  NaClSrpcServerLoop(desc->desc(), handlers, NULL);
  NaClLog(1, "upcall server handlers stop\n");
  NaClThreadExit();
}


static void BuildArgVec(NaClSrpcArg* argv[], NaClSrpcArg arg[], int count) {
  int i;
  for (i = 0; i < count; ++i) {
    argv[i] = &arg[i];
  }
  argv[count] = NULL;
}


static void SendVideoMemAndUpChannel(NaClSrpcChannel* channel,
                                     DescWrapper* video_desc,
                                     DescWrapper* back_channel) {
  uint32_t rpc_num;
  NaClSrpcArg in[kMaxArgs];
  NaClSrpcArg* inv[kMaxArgs + 1];
  NaClSrpcArg out[kMaxArgs];
  NaClSrpcArg* outv[kMaxArgs + 1];

  rpc_num = NaClSrpcServiceMethodIndex(channel->client,
                                       "nacl_multimedia_bridge:hh:");

  NaClLog(1, "making rpc to nacl_multimedia_bridge %d\n", rpc_num);

  in[0].tag = NACL_SRPC_ARG_TYPE_HANDLE;
  in[0].u.hval = video_desc->desc();
  in[1].tag = NACL_SRPC_ARG_TYPE_HANDLE;
  in[1].u.hval = back_channel->desc();
  BuildArgVec(inv, in, 2);
  BuildArgVec(outv, out, 0);

  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeV(channel, rpc_num, inv, outv)) {
    NaClLog(LOG_FATAL, "rpc to nacl_multimedia_bridge failed\n");
  }
}


// This function intializes the SDL multimedia subsystem
// * it allocates a shared memory datastructure including video_memory and
// * sets up a service for copy this video_memory into the framebuffer
// * finally it calls the client rpc "nacl_multimedia_bridge" to communicate
//   the shared memory area and the new service to the client
void IntializeMultimediaHandler(NaClSrpcChannel* channel,
                                int width,
                                int height,
                                const char* title) {
  DescWrapperFactory* factory = new DescWrapperFactory();
  NaClLog(1, "IntializeMultimediaHandler\n");
  g_mm = MakeMultimediaSDL(width, height, title);

  // NOTE: these are really NaClDescXferableDataDesc. Code was mimicked after
  //       the exisiting plugin code
  NaClLog(1, "create socket pair so that client can make multimedia upcalls\n");

  DescWrapper* descs[2] = { NULL, NULL };
  if (0 != factory->MakeSocketPair(descs)) {
    NaClLog(LOG_FATAL, "cannot create socket pair\n");
  }

  NaClLog(1,
          "creating video buffer shared region video size 0x%x\n",
          g_mm->VideoBufferSize());

  const size_t required_size = g_mm->VideoBufferSize() + sizeof(NaClVideoShare);
  DescWrapper* shm = factory->MakeShm(required_size);
  NaClLog(1, "mapping shared video memory locally\n");
  size_t mapped_size;
  shm->Map(reinterpret_cast<void**>(&g_video_share), &mapped_size);

  NaClLog(1, "intializing the shared area at %p\n",
          static_cast<void*>(g_video_share));
  g_video_share->u.h.revision = 0x101;
  g_video_share->u.h.video_width = width;
  g_video_share->u.h.video_height = height;
  g_video_share->u.h.video_size = g_mm->VideoBufferSize();

  NaClLog(1, "spawning multimedia service thread\n");

  NaClThread thread;
  if (!NaClThreadCtor(
        &thread,
        ServiceHandlerThread,
        descs[0],
        128 << 10)) {
    NaClLog(LOG_FATAL, "cannot create service handler thread\n");
  }

  SendVideoMemAndUpChannel(channel, shm, descs[1]);
}
