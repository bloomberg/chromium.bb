/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "posix_over_srpc_linux.h"

PosixOverSrpcLauncher* GetLauncher(NaClSrpcChannel* channel) {
  return
      static_cast<ChildContext*>(channel->server_instance_data)->psrpc_launcher;
}

int GetChildId(NaClSrpcChannel* channel) {
  return static_cast<ChildContext*>(channel->server_instance_data)->child_id;
}

int NewPtrHandle(NaClSrpcChannel* channel, void* ptr) {
  return GetLauncher(channel)->NewPtrHandle(GetChildId(channel), ptr);
}

void CollectDesc(NaClSrpcChannel* channel, void* desc, enum CollectTag tag) {
  GetLauncher(channel)->CollectDesc(GetChildId(channel), desc, tag);
}

void CloseUnusedDescs(NaClSrpcChannel* channel) {
  GetLauncher(channel)->CloseUnusedDescs(GetChildId(channel));
}

nacl::DescWrapperFactory* DescFactory(NaClSrpcChannel* channel) {
  return GetLauncher(channel)->DescFactory();
}
