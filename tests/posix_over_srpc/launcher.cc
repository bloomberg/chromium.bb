/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/tests/posix_over_srpc/launcher.h"

#include <fcntl.h>

#include <iostream>

#include "native_client/tests/posix_over_srpc/linux_lib/posix_over_srpc_linux.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"

/*
 * kUpChannelDestDesc is handle in nacl module which corresponds to imc socket
 * which is base of srpc channel. All the up calls from nacl module are made
 * via this channel. kUpChannelDestDesc will be passed to
 * SelLdrLauncher::ExportImcSock.
 */
int kUpChannelDestDesc = 6;

bool PosixOverSrpcLauncher::ValueByHandle(int child_id, int handle,
                                          HandledValue* pval) {
  ValueByHandleMap* pmap = value_by_handle_maps_[child_id];
  ValueByHandleMap::iterator i = pmap->find(handle);
  if (i == pmap->end()) {
    std::cerr << "Wrong handle: " << handle << "\n";
    return false;
  } else {
    *pval = i->second;
    return true;
  }
}

int PosixOverSrpcLauncher::NewHandle(int child_id, HandledValue val) {
  ValueByHandleMap* pmap = value_by_handle_maps_[child_id];
  ++last_used_handles_[child_id];
  while (pmap->find(last_used_handles_[child_id]) != pmap->end()) {
    ++last_used_handles_[child_id];
  }
  (*pmap)[last_used_handles_[child_id]] = val;
  return last_used_handles_[child_id];
}

void PosixOverSrpcLauncher::CollectDesc(int child_id, void* desc,
                                        enum CollectTag tag) {
  desc_collectors_[child_id]->push(CollectPair(desc, tag));
}

nacl::DescWrapperFactory* PosixOverSrpcLauncher::DescFactory() {
  return &factory_;
}

void PosixOverSrpcLauncher::CloseUnusedDescs(int child_id) {
  CollectPair cur;
  NaClDescIoDesc* io_desc;
  for (; !desc_collectors_[child_id]->empty();) {
    cur = desc_collectors_[child_id]->front();
    desc_collectors_[child_id]->pop();
    if (DESC_WRAPPER == cur.second) {
      if (reinterpret_cast<nacl::DescWrapper*>(cur.first)->Close()) {
        std::cerr << "DescWrapper Close failed\n";
      }
    } else if (IO_DESC == cur.second) {
      io_desc = reinterpret_cast<NaClDescIoDesc*>(cur.first);
      if (reinterpret_cast<struct NaClDescVtbl const *>(io_desc->base.base.vtbl)
          ->Close(reinterpret_cast<NaClDesc*>(io_desc))) {
        std::cerr << "NaClDescIoDesc Close failed\n";
      }
    }
  }
}

PosixOverSrpcLauncher::PosixOverSrpcLauncher() {
  const char* kSelLdrArgs[] = {"-P", "5", "-X", "5"};
  sel_ldr_args_ = std::vector<nacl::string>(kSelLdrArgs, kSelLdrArgs + 4);
  nchildren_ = 0;
  pthread_mutex_init(&data_mu_, NULL);
  pthread_cond_init(&all_children_died_cv_, NULL);
  NaClNrdAllModulesInit();
}

PosixOverSrpcLauncher::~PosixOverSrpcLauncher() {
  pthread_mutex_lock(&data_mu_);
  while (nchildren_) {
    pthread_cond_wait(&all_children_died_cv_, &data_mu_);
  }
  pthread_mutex_unlock(&data_mu_);
  pthread_mutex_destroy(&data_mu_);
  pthread_cond_destroy(&all_children_died_cv_);
}

void PosixOverSrpcLauncher::IncreaseNumberOfChildren() {
  pthread_mutex_lock(&data_mu_);
  ++nchildren_;
  pthread_mutex_unlock(&data_mu_);
}

void PosixOverSrpcLauncher::DecreaseNumberOfChildren() {
  pthread_mutex_lock(&data_mu_);
  --nchildren_;
  if (0 == nchildren_) {
    pthread_cond_signal(&all_children_died_cv_);
  }
  pthread_mutex_unlock(&data_mu_);
}

void* UpcallLoopThread(void* void_arg) {
  ChildContext* context = static_cast<ChildContext*>(void_arg);
  NaClSrpcHandlerDesc handlers[] = {
    {"accept:i:hii", nonnacl_accept},
    {"bind:iiCi:ii", nonnacl_bind},
    {"close:i:", nonnacl_close},
    {"closedir:i:ii", nonnacl_closedir},
    {"getcwd::Cii", nonnacl_getcwd},
    {"getpagesize::i", nonnacl_getpagesize},
    {"listen:ii:ii", nonnacl_listen},
    {"open:sii:hi", nonnacl_open},
    {"opendir:s:ii", nonnacl_opendir},
    {"pathconf:si:ii", nonnacl_pathconf},
    {"pipe::iihh", nonnacl_pipe},
    {"readdir:i:iiisii", nonnacl_readdir},
    {"setsockopt:iiiC:ii", nonnacl_setsockopt},
    {"socket:iii:hii", nonnacl_socket},
    {"times::iiii", nonnacl_times},
    {NULL, NULL}
  };
  nacl::DescWrapperFactory* factory = context->psrpc_launcher->DescFactory();
  NaClSrpcServerLoop(
      factory->MakeImcSock(context->imc_handle)->desc(),
      handlers,
      context);
  delete context;
  return NULL;
}

void* InvokeInitUpcallChannel(void* void_context) {
  ChildContext* context = static_cast<ChildContext*>(void_context);
  pthread_t loop_thread;
  NaClSrpcError srpc_code;
  NaClSrpcChannel command;
  NaClSrpcChannel untrusted;

  if (false == context->sel_ldr_launcher->OpenSrpcChannels(&command,
                                                           &untrusted)) {
    std::cerr << "OpenSrpcChannels failed\n";
    goto ret;
  }
  if (0 != pthread_create(&loop_thread, NULL, UpcallLoopThread, context)) {
    std::cerr << "Unable to create UpcallLoopThread\n";
    goto ret;
  }
  srpc_code = NaClSrpcInvokeBySignature(&untrusted,
                                        "init_upcall_channel:i:",
                                        kUpChannelDestDesc);
  if (NACL_SRPC_RESULT_OK != srpc_code) {
    std::cerr << "init_upcall_channel:h: invocation error code: "
        << NaClSrpcErrorString(srpc_code) << "\n";
  }

 ret:
  context->psrpc_launcher->DecreaseNumberOfChildren();
  delete context->sel_ldr_launcher;
  delete context;
  return NULL;
}

bool PosixOverSrpcLauncher::SpawnNaClModule(
    const nacl::string& nacl_module,
    const std::vector<nacl::string>& argv) {
  // TODO(mikhailt): Make this thread safe.
  pthread_t invoke_thread;
  ChildContext* context;
  nacl::SelLdrLauncher* launcher = new nacl::SelLdrLauncher;
  int upcall_imc_handle = launcher->ExportImcFD(kUpChannelDestDesc);

  if (false == launcher->Start(nacl_module, 5, sel_ldr_args_, argv)) {
    std::cerr << "Unable to start sel_ldr from " << sel_ldr_dir_ << "\n";
    delete launcher;
    return false;
  }
  context = new ChildContext;
  context->psrpc_launcher = this;
  context->sel_ldr_launcher = launcher;
  context->child_id = nchildren_;
  context->imc_handle = upcall_imc_handle;
  IncreaseNumberOfChildren();
  last_used_handles_.push_back(0);
  value_by_handle_maps_.push_back(new ValueByHandleMap);
  launchers_.push_back(launcher);
  desc_collectors_.push_back(new DescCollector);

  if (0 != pthread_create(&invoke_thread, NULL, InvokeInitUpcallChannel,
                          context)) {
    std::cerr << "Unable to create thread for InvokeInitUpcallChannel\n";
    launcher->KillChild();
    delete launcher;
    delete context;
    DecreaseNumberOfChildren();
    last_used_handles_.pop_back();
    value_by_handle_maps_.pop_back();
    launchers_.pop_back();
    desc_collectors_.pop_back();
    return false;
  }
  return true;
}
