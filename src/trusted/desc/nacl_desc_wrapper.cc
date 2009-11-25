// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <new>
#include "base/shared_memory.h"
#include "chrome/common/transport_dib.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#ifdef NACL_LINUX
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
#endif  // NACL_LINUX
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"

namespace nacl {
bool DescWrapperFactory::Init() {
  // Create a bound socket for use by the transfer effector.
  if (0 != NaClCommonDescMakeBoundSock(sockets_)) {
    return false;
  }
  // Set up the transfer effector.
  if (!NaClNrdXferEffectorCtor(&eff_, sockets_[0])) {
    Cleanup();
    return false;
  }
  effp_ = reinterpret_cast<struct NaClDescEffector*>(&eff_);
  return true;
}

void DescWrapperFactory::Cleanup() {
  NaClDescUnref(sockets_[0]);
  sockets_[0] = NULL;
  NaClDescUnref(sockets_[1]);
  sockets_[1] = NULL;
}

DescWrapperFactory::~DescWrapperFactory() {
  if (NULL != effp_) {
    effp_->vtbl->Dtor(effp_);
  }
  Cleanup();
}

int DescWrapperFactory::MakeBoundSock(DescWrapper* pair[2]) {
  struct NaClDesc* descs[2];

  pair[0] = NULL;
  pair[1] = NULL;
  int ret = NaClCommonDescMakeBoundSock(descs);
  if (0 != ret) {
    return ret;
  }
  pair[0] = new(std::nothrow) DescWrapper(this, descs[0]);
  pair[1] = new(std::nothrow) DescWrapper(this, descs[1]);

  return 0;
}

DescWrapper* DescWrapperFactory::MakeShm(size_t size) {
  size_t rounded_size = NaClRoundAllocPage(size);
  NaClHandle handle = CreateMemoryObject(rounded_size);
  if (kInvalidHandle == handle) {
    Close(handle);
    return NULL;
  }
  return ImportShmHandle(handle, size);
}

DescWrapper* DescWrapperFactory::ImportShmHandle(NaClHandle handle,
                                                 size_t size) {
  struct NaClDescImcShm* imc_desc = NULL;
  struct NaClDesc* desc = NULL;
  DescWrapper* wrapper = NULL;

  imc_desc = new(std::nothrow) NaClDescImcShm;
  if (NULL == imc_desc) {
    goto cleanup;
  }
  if (!NaClDescImcShmCtor(imc_desc, handle, size)) {
    free(imc_desc);
    desc = NULL;
    goto cleanup;
  }
  desc = reinterpret_cast<struct NaClDesc*>(imc_desc);
  wrapper = new(std::nothrow) DescWrapper(this, desc);
  if (NULL == wrapper) {
    goto cleanup;
  }
  return wrapper;

cleanup:
  NaClDescUnref(reinterpret_cast<struct NaClDesc*>(desc));
  return NULL;
}

#if NACL_LINUX
DescWrapper* DescWrapperFactory::ImportSysvShm(int key, size_t size) {
  struct NaClDescSysvShm* sysv_desc = NULL;
  struct NaClDesc* desc = NULL;
  DescWrapper* wrapper = NULL;

  sysv_desc = new(std::nothrow) NaClDescSysvShm;
  if (NULL == sysv_desc) {
    goto cleanup;
  }
  if (!NaClDescSysvShmImportCtor(sysv_desc,
                                key,
                                static_cast<nacl_off64_t>(size))) {
    free(sysv_desc);
    desc = NULL;
    goto cleanup;
  }
  desc = reinterpret_cast<struct NaClDesc*>(sysv_desc);
  wrapper = new(std::nothrow) DescWrapper(this, desc);
  if (NULL == wrapper) {
    goto cleanup;
  }
  return wrapper;

cleanup:
  NaClDescUnref(reinterpret_cast<struct NaClDesc*>(desc));
  return NULL;
}
#endif  // NACL_LINUX

DescWrapper* DescWrapperFactory::ImportSharedMemory(base::SharedMemory* shm) {
#if  NACL_LINUX || NACL_OSX
  return ImportShmHandle(shm->handle().fd, shm->max_size());
#elif NACL_WINDOWS
  return ImportShmHandle(shm->handle(), shm->max_size());
#else
# error "What platform?"
#endif  // NACL_LINUX || NACL_OSX
}

DescWrapper* DescWrapperFactory::ImportTransportDIB(TransportDIB* dib) {
#if  NACL_LINUX
  // TransportDIBs use SysV (X) shared memory on Linux.
  return ImportSysvShm(dib->handle(), dib->size());
#elif NACL_OSX
  // TransportDIBs use mmap shared memory on OSX.
  return ImportShmHandle(dib->handle().fd, dib->size());
#elif NACL_WINDOWS
  // TransportDIBs use MapViewOfFile shared memory on Windows.
  return ImportShmHandle(dib->handle(), dib->size());
#else
# error "What platform?"
#endif  // NACL_LINUX
}

DescWrapper* DescWrapperFactory::MakeGeneric(struct NaClDesc* desc) {
  return new(std::nothrow) DescWrapper(this, desc);
}

DescWrapper::~DescWrapper() {
  NaClDescUnref(desc_);
  desc_ = NULL;
}

void* DescWrapper::Map(void* start_addr,
                       size_t len,
                       int prot,
                       int flags,
                       nacl_off64_t offset) {
  return reinterpret_cast<void*>(desc_->vtbl->Map(desc_,
                                                  factory_->effp_,
                                                  start_addr,
                                                  len,
                                                  prot,
                                                  flags,
                                                  offset));
}

int DescWrapper::Unmap(void* start_addr, size_t len) {
  return desc_->vtbl->Unmap(desc_, factory_->effp_, start_addr, len);
}

ssize_t DescWrapper::Read(void* buf, size_t len) {
  return desc_->vtbl->Read(desc_, factory_->effp_, buf, len);
}

ssize_t DescWrapper::Write(void* buf, size_t len) {
  return desc_->vtbl->Write(desc_, factory_->effp_, buf, len);
}

nacl_off64_t DescWrapper::Seek(nacl_off64_t offset, int whence) {
  return desc_->vtbl->Seek(desc_, factory_->effp_, offset, whence);
}

int DescWrapper::Ioctl(int request, void* arg) {
  return desc_->vtbl->Ioctl(desc_, factory_->effp_, request, arg);
}

int DescWrapper::Fstat(struct nacl_abi_stat* statbuf) {
  return desc_->vtbl->Fstat(desc_, factory_->effp_, statbuf);
}

int DescWrapper::Close() {
  return desc_->vtbl->Close(desc_, factory_->effp_);
}

ssize_t DescWrapper::Getdents(void* dirp, size_t count) {
  return desc_->vtbl->Getdents(desc_, factory_->effp_, dirp, count);
}

int DescWrapper::Lock() {
  return desc_->vtbl->Lock(desc_, factory_->effp_);
}

int DescWrapper::TryLock() {
  return desc_->vtbl->TryLock(desc_, factory_->effp_);
}

int DescWrapper::Unlock() {
  return desc_->vtbl->Unlock(desc_, factory_->effp_);
}

int DescWrapper::Wait(DescWrapper* mutex) {
  return desc_->vtbl->Wait(desc_, factory_->effp_, mutex->desc_);
}

int DescWrapper::TimedWaitAbs(DescWrapper* mutex,
                              struct nacl_abi_timespec* ts) {
  return desc_->vtbl->TimedWaitAbs(desc_, factory_->effp_, mutex->desc_, ts);
}

int DescWrapper::Signal() {
  return desc_->vtbl->Signal(desc_, factory_->effp_);
}

int DescWrapper::Broadcast() {
  return desc_->vtbl->Broadcast(desc_, factory_->effp_);
}

int DescWrapper::SendMsg(MsgHeader* dgram, int flags) {
  struct NaClImcTypedMsgHdr header;
  int ret = -NACL_ABI_EINVAL;
  size_t i;

  header.ndescv = NULL;
  header.iov = new(std::nothrow) NaClImcMsgIoVec[dgram->iov_length];
  if (NULL == header.iov) {
    ret = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  for (i = 0; i < dgram->iov_length; ++i) {
    header.iov[i].base = dgram->iov[i].base;
    header.iov[i].length = dgram->iov[i].length;
  }
  header.ndescv = new(std::nothrow) NaClDesc*[dgram->ndescv_length];
  if (NULL == header.iov) {
    ret = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  for (i = 0; i < dgram->ndescv_length; ++i) {
    header.ndescv[i] = dgram->ndescv[i]->desc_;
  }
  ret = NaClImcSendTypedMessage(desc_, factory_->effp_, &header, flags);

cleanup:
  delete header.ndescv;
  delete header.iov;
  return ret;
}

int DescWrapper::RecvMsg(MsgHeader* dgram, int flags) {
  struct NaClImcTypedMsgHdr header;
  int ret = -NACL_ABI_EINVAL;
  size_t i;

  header.iov = new(std::nothrow) NaClImcMsgIoVec[dgram->iov_length];
  if (NULL == header.iov) {
    ret = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  for (i = 0; i < dgram->iov_length; ++i) {
    header.iov[i].base = dgram->iov[i].base;
    header.iov[i].length = dgram->iov[i].length;
  }
  header.ndescv = new(std::nothrow) NaClDesc*[dgram->ndescv_length];
  if (NULL == header.iov) {
    ret = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  ret = NaClImcRecvTypedMessage(desc_, factory_->effp_, &header, flags);
  for (i = 0; i < dgram->ndescv_length; ++i) {
    dgram->ndescv[i] =
        new(std::nothrow) DescWrapper(factory_, header.ndescv[i]);
  }

cleanup:
  return ret;
}

DescWrapper* DescWrapper::Connect() {
  int rv = desc_->vtbl->ConnectAddr(desc_, factory_->effp_);
  if (0 != rv) {
    // Connect failed.
    return NULL;
  }
  struct NaClDesc* connected_desc =
      NaClNrdXferEffectorTakeDesc(&factory_->eff_);
  if (NULL == connected_desc) {
    // Take from effector failed.
    return NULL;
  }
  return new(std::nothrow) DescWrapper(factory_, connected_desc);
}

DescWrapper* DescWrapper::Accept() {
  int rv = desc_->vtbl->AcceptConn(desc_, factory_->effp_);
  if (0 != rv) {
    // Accept failed.
    return NULL;
  }
  struct NaClDesc* connected_desc =
      NaClNrdXferEffectorTakeDesc(&factory_->eff_);
  if (NULL == connected_desc) {
    // Take from effector failed.
    return NULL;
  }
  return new(std::nothrow) DescWrapper(factory_, connected_desc);
}

int DescWrapper::Post() {
  return desc_->vtbl->Post(desc_, factory_->effp_);
}

int DescWrapper::SemWait() {
  return desc_->vtbl->SemWait(desc_, factory_->effp_);
}

int DescWrapper::GetValue() {
  return desc_->vtbl->GetValue(desc_, factory_->effp_);
}

}  // namespace nacl
