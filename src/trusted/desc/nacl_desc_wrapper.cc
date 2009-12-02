// Copyright (c) 2009 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <new>
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "chrome/common/transport_dib.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_sync_socket.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#ifdef NACL_LINUX
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
#endif  // NACL_LINUX
#include "native_client/src/trusted/desc/nrd_xfer.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"

/*
 * TODO(bsy): remove when we put SIZE_T_MAX in a common header file.
 */
#if !defined(SIZE_T_MAX)
# define SIZE_T_MAX ((size_t) -1)
#endif

namespace nacl {

bool DescWrapperCommon::Init() {
  // Create a bound socket for use by the transfer effector.
  if (0 != NaClCommonDescMakeBoundSock(sockets_)) {
    return false;
  }
  // Set up the transfer effector.
  if (!NaClNrdXferEffectorCtor(&eff_, sockets_[0])) {
    Cleanup();
    return false;
  }
  is_initialized_ = true;
  return true;
}

void DescWrapperCommon::Cleanup() {
  NaClDescSafeUnref(sockets_[0]);
  sockets_[0] = NULL;
  NaClDescSafeUnref(sockets_[1]);
  sockets_[1] = NULL;
}

DescWrapperCommon::~DescWrapperCommon() {
  if (is_initialized_) {
    effp()->vtbl->Dtor(effp());
  }
  Cleanup();
}

DescWrapperFactory::DescWrapperFactory() {
  common_data_ = new(std::nothrow) DescWrapperCommon();
  if (common_data_ && !common_data_->Init()) {
    delete common_data_;
    common_data_ = reinterpret_cast<DescWrapperCommon*>(NULL);
  }
  if (reinterpret_cast<DescWrapperCommon*>(NULL) != common_data_) {
    common_data_->AddRef();
  }
}

DescWrapperFactory::~DescWrapperFactory() {
  if (reinterpret_cast<DescWrapperCommon*>(NULL) != common_data_) {
    common_data_->RemoveRef();
  }
}

int DescWrapperFactory::MakeBoundSock(DescWrapper* pair[2]) {
  // Return an error if the factory wasn't properly initialized.
  if (!common_data_->is_initialized()) {
    return -1;
  }
  struct NaClDesc* descs[2] = { NULL, NULL };
  DescWrapper* tmp_pair[2] = { NULL, NULL };

  int ret = NaClCommonDescMakeBoundSock(descs);
  if (0 != ret) {
    return ret;
  }
  tmp_pair[0] = new(std::nothrow) DescWrapper(common_data_, descs[0]);
  if (NULL == tmp_pair[0]) {
    goto cleanup;
  }
  // DescWrapper took ownership of descs[0], so to allow cleanup we NULL it out.
  descs[0] = NULL;
  tmp_pair[1] = new(std::nothrow) DescWrapper(common_data_, descs[1]);
  if (NULL == tmp_pair[1]) {
    goto cleanup;
  }
  // DescWrapper took ownership of descs[1], so to allow cleanup we NULL it out.
  descs[1] = NULL;
  pair[0] = tmp_pair[0];
  pair[1] = tmp_pair[1];
  return 0;

cleanup:
  NaClDescSafeUnref(descs[0]);
  NaClDescSafeUnref(descs[1]);
  delete tmp_pair[0];
  delete tmp_pair[1];
  return -1;
}

DescWrapper* DescWrapperFactory::MakeShm(size_t size) {
  // Return an error if the factory wasn't properly initialized.
  if (!common_data_->is_initialized()) {
    return NULL;
  }
  size_t rounded_size = NaClRoundAllocPage(size);
  NaClHandle handle = CreateMemoryObject(rounded_size);
  if (kInvalidHandle == handle) {
    return NULL;
  }
  return ImportShmHandle(handle, size);
}

DescWrapper* DescWrapperFactory::ImportShmHandle(NaClHandle handle,
                                                 size_t size) {
  struct NaClDescImcShm* imc_desc = NULL;
  DescWrapper* wrapper = NULL;

  imc_desc = new(std::nothrow) NaClDescImcShm;
  if (NULL == imc_desc) {
    goto cleanup;
  }
  if (!NaClDescImcShmCtor(imc_desc, handle, size)) {
    delete imc_desc;
    imc_desc = NULL;
    goto cleanup;
  }
  wrapper = new(std::nothrow)
      DescWrapper(common_data_, reinterpret_cast<struct NaClDesc*>(imc_desc));
  if (NULL == wrapper) {
    goto cleanup;
  }
  // DescWrapper takes ownership of imc_desc, so we NULL it out.
  imc_desc = NULL;
  return wrapper;

cleanup:
  NaClDescSafeUnref(reinterpret_cast<struct NaClDesc*>(imc_desc));
  return NULL;
}

#if NACL_LINUX
DescWrapper* DescWrapperFactory::ImportSysvShm(int key, size_t size) {
  struct NaClDescSysvShm* sysv_desc = NULL;
  DescWrapper* wrapper = NULL;

  if (static_cast<size_t>(INT_MAX) <= size) {
    // Avoid overflow when casting to nacl_off64_t by preventing negative size.
    goto cleanup;
  }
  sysv_desc = new(std::nothrow) NaClDescSysvShm;
  if (NULL == sysv_desc) {
    goto cleanup;
  }
  if (!NaClDescSysvShmImportCtor(sysv_desc,
                                 key,
                                 static_cast<nacl_off64_t>(size))) {
    delete sysv_desc;
    sysv_desc = NULL;
    goto cleanup;
  }
  wrapper = new(std::nothrow)
      DescWrapper(common_data_, reinterpret_cast<struct NaClDesc*>(sysv_desc));
  if (NULL == wrapper) {
    goto cleanup;
  }
  return wrapper;

cleanup:
  NaClDescSafeUnref(reinterpret_cast<struct NaClDesc*>(sysv_desc));
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

DescWrapper* DescWrapperFactory::ImportSyncSocket(base::SyncSocket* sock) {
  struct NaClDescSyncSocket* ss_desc = NULL;
  DescWrapper* wrapper = NULL;

  ss_desc = new(std::nothrow) NaClDescSyncSocket;
  if (NULL == ss_desc) {
    goto cleanup;
  }
  if (!NaClDescSyncSocketCtor(ss_desc, sock->handle())) {
    delete ss_desc;
    ss_desc = NULL;
    goto cleanup;
  }
  wrapper = new(std::nothrow)
      DescWrapper(common_data_, reinterpret_cast<struct NaClDesc*>(ss_desc));
  if (NULL == wrapper) {
    goto cleanup;
  }
  // DescWrapper takes ownership of ss_desc, so we NULL it out.
  ss_desc = NULL;
  return wrapper;

cleanup:
  NaClDescSafeUnref(reinterpret_cast<struct NaClDesc*>(ss_desc));
  return NULL;
}

DescWrapper* DescWrapperFactory::MakeGeneric(struct NaClDesc* desc) {
  // Return an error if the factory wasn't properly initialized.
  if (!common_data_->is_initialized()) {
    return NULL;
  }
  return new(std::nothrow) DescWrapper(common_data_, desc);
}

DescWrapper::DescWrapper(DescWrapperCommon* common_data,
                         struct NaClDesc* desc)
      : common_data_(common_data), desc_(desc) {
  if (reinterpret_cast<DescWrapperCommon*>(NULL) != common_data_) {
    common_data_->AddRef();
  }
}

DescWrapper::~DescWrapper() {
  if (reinterpret_cast<DescWrapperCommon*>(NULL) != common_data_) {
    common_data_->RemoveRef();
  }
  NaClDescSafeUnref(desc_);
  desc_ = NULL;
}

void* DescWrapper::Map(void* start_addr,
                       size_t len,
                       int prot,
                       int flags,
                       nacl_off64_t offset) {
  return reinterpret_cast<void*>(desc_->vtbl->Map(desc_,
                                                  common_data_->effp(),
                                                  start_addr,
                                                  len,
                                                  prot,
                                                  flags,
                                                  offset));
}

int DescWrapper::Unmap(void* start_addr, size_t len) {
  return desc_->vtbl->Unmap(desc_, common_data_->effp(), start_addr, len);
}

ssize_t DescWrapper::Read(void* buf, size_t len) {
  return desc_->vtbl->Read(desc_, common_data_->effp(), buf, len);
}

ssize_t DescWrapper::Write(const void* buf, size_t len) {
  return desc_->vtbl->Write(desc_, common_data_->effp(), buf, len);
}

nacl_off64_t DescWrapper::Seek(nacl_off64_t offset, int whence) {
  return desc_->vtbl->Seek(desc_, common_data_->effp(), offset, whence);
}

int DescWrapper::Ioctl(int request, void* arg) {
  return desc_->vtbl->Ioctl(desc_, common_data_->effp(), request, arg);
}

int DescWrapper::Fstat(struct nacl_abi_stat* statbuf) {
  return desc_->vtbl->Fstat(desc_, common_data_->effp(), statbuf);
}

int DescWrapper::Close() {
  return desc_->vtbl->Close(desc_, common_data_->effp());
}

ssize_t DescWrapper::Getdents(void* dirp, size_t count) {
  return desc_->vtbl->Getdents(desc_, common_data_->effp(), dirp, count);
}

int DescWrapper::Lock() {
  return desc_->vtbl->Lock(desc_, common_data_->effp());
}

int DescWrapper::TryLock() {
  return desc_->vtbl->TryLock(desc_, common_data_->effp());
}

int DescWrapper::Unlock() {
  return desc_->vtbl->Unlock(desc_, common_data_->effp());
}

int DescWrapper::Wait(DescWrapper* mutex) {
  return desc_->vtbl->Wait(desc_, common_data_->effp(), mutex->desc_);
}

int DescWrapper::TimedWaitAbs(DescWrapper* mutex,
                              struct nacl_abi_timespec* ts) {
  return desc_->vtbl->TimedWaitAbs(desc_,
                                   common_data_->effp(),
                                   mutex->desc_,
                                   ts);
}

int DescWrapper::Signal() {
  return desc_->vtbl->Signal(desc_, common_data_->effp());
}

int DescWrapper::Broadcast() {
  return desc_->vtbl->Broadcast(desc_, common_data_->effp());
}

int DescWrapper::SendMsg(const MsgHeader* dgram, int flags) {
  struct NaClImcTypedMsgHdr header;
  int ret = -NACL_ABI_ENOMEM;
  size_t diov_length = dgram->iov_length;
  size_t ddescv_length = dgram->ndescv_length;
  size_t i;

  // Initialize to allow simple cleanups.
  header.ndescv = NULL;
  // Allocate and copy IOV.
  if (SIZE_T_MAX / sizeof(NaClImcMsgIoVec) <= diov_length) {
    goto cleanup;
  }
  header.iov = new(std::nothrow) NaClImcMsgIoVec[dgram->iov_length];
  if (NULL == header.iov) {
    goto cleanup;
  }
  for (i = 0; i < dgram->iov_length; ++i) {
    header.iov[i].base = dgram->iov[i].base;
    header.iov[i].length = dgram->iov[i].length;
  }
  // Allocate and copy the descriptor vector, removing DescWrappers.
  if (kHandleCountMax < dgram->ndescv_length) {
    goto cleanup;
  }
  if (SIZE_T_MAX / sizeof(NaClDesc*) <= ddescv_length) {
    goto cleanup;
  }
  header.ndescv = new(std::nothrow) NaClDesc*[dgram->ndescv_length];
  if (NULL == header.iov) {
    goto cleanup;
  }
  for (i = 0; i < dgram->ndescv_length; ++i) {
    header.ndescv[i] = dgram->ndescv[i]->desc_;
  }
  // Send the message.
  ret = NaClImcSendTypedMessage(desc_, common_data_->effp(), &header, flags);

cleanup:
  delete header.ndescv;
  delete header.iov;
  return ret;
}

int DescWrapper::RecvMsg(MsgHeader* dgram, int flags) {
  struct NaClImcTypedMsgHdr header;
  int ret = -NACL_ABI_ENOMEM;
  size_t diov_length = dgram->iov_length;
  size_t ddescv_length = dgram->ndescv_length;
  size_t i;

  // Initialize to allow simple cleanups.
  header.ndescv = NULL;
  for (i = 0; i < dgram->iov_length; ++i) {
    dgram->ndescv[i] = reinterpret_cast<DescWrapper*>(NULL);
  }

  // Allocate and copy the IOV.
  if (SIZE_T_MAX / sizeof(NaClImcMsgIoVec) <= diov_length) {
    goto cleanup;
  }
  header.iov = new(std::nothrow) NaClImcMsgIoVec[dgram->iov_length];
  if (NULL == header.iov) {
    goto cleanup;
  }
  for (i = 0; i < dgram->iov_length; ++i) {
    header.iov[i].base = dgram->iov[i].base;
    header.iov[i].length = dgram->iov[i].length;
  }
  // Allocate and copy the descriptor vector.
  if (kHandleCountMax < dgram->ndescv_length) {
    goto cleanup;
  }
  if (SIZE_T_MAX / sizeof(NaClDesc*) <= ddescv_length) {
    goto cleanup;
  }
  header.ndescv = new(std::nothrow) NaClDesc*[dgram->ndescv_length];
  if (NULL == header.ndescv) {
    goto cleanup;
  }
  // Receive the message.
  ret = NaClImcRecvTypedMessage(desc_, common_data_->effp(), &header, flags);
  // Copy the descriptors, creating new DescWrappers around them.
  for (i = 0; i < dgram->ndescv_length; ++i) {
    dgram->ndescv[i] =
        new(std::nothrow) DescWrapper(common_data_, header.ndescv[i]);
    if (reinterpret_cast<DescWrapper*>(NULL) == dgram->ndescv[i]) {
      goto cleanup;
    }
  }
  return ret;

cleanup:
  for (i = 0; i < dgram->ndescv_length; ++i) {
    delete dgram->ndescv[i];
  }
  delete header.ndescv;
  delete header.iov;
  return ret;
}

DescWrapper* DescWrapper::Connect() {
  int rv = desc_->vtbl->ConnectAddr(desc_, common_data_->effp());
  if (0 != rv) {
    // Connect failed.
    return NULL;
  }
  struct NaClNrdXferEffector* nrd_effector =
      reinterpret_cast<struct NaClNrdXferEffector*>(common_data_->effp());
  struct NaClDesc* connected_desc = NaClNrdXferEffectorTakeDesc(nrd_effector);
  if (NULL == connected_desc) {
    // Take from effector failed.
    return NULL;
  }
  return new(std::nothrow) DescWrapper(common_data_, connected_desc);
}

DescWrapper* DescWrapper::Accept() {
  int rv = desc_->vtbl->AcceptConn(desc_, common_data_->effp());
  if (0 != rv) {
    // Accept failed.
    return NULL;
  }

  struct NaClNrdXferEffector* nrd_effector =
      reinterpret_cast<struct NaClNrdXferEffector*>(common_data_->effp());
  struct NaClDesc* connected_desc = NaClNrdXferEffectorTakeDesc(nrd_effector);
  if (NULL == connected_desc) {
    // Take from effector failed.
    return NULL;
  }
  return new(std::nothrow) DescWrapper(common_data_, connected_desc);
}

int DescWrapper::Post() {
  return desc_->vtbl->Post(desc_, common_data_->effp());
}

int DescWrapper::SemWait() {
  return desc_->vtbl->SemWait(desc_, common_data_->effp());
}

int DescWrapper::GetValue() {
  return desc_->vtbl->GetValue(desc_, common_data_->effp());
}

}  // namespace nacl
